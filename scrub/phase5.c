// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#include "xfs.h"
#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#ifdef HAVE_LIBATTR
# include <attr/attributes.h>
#endif
#include <linux/fs.h>
#include "handle.h"
#include "list.h"
#include "libfrog/paths.h"
#include "libfrog/workqueue.h"
#include "libfrog/fsgeom.h"
#include "libfrog/scrub.h"
#include "xfs_scrub.h"
#include "common.h"
#include "inodes.h"
#include "progress.h"
#include "scrub.h"
#include "descr.h"
#include "unicrash.h"
#include "repair.h"

/* Phase 5: Full inode scans and check directory connectivity. */

/*
 * Warn about problematic bytes in a directory/attribute name.  That means
 * terminal control characters and escape sequences, since that could be used
 * to do something naughty to the user's computer and/or break scripts.  XFS
 * doesn't consider any byte sequence invalid, so don't flag these as errors.
 *
 * Returns 0 for success or -1 for error.  This function logs errors.
 */
static int
simple_check_name(
	struct scrub_ctx	*ctx,
	struct descr		*dsc,
	const char		*namedescr,
	const char		*name)
{
	const char		*p;
	bool			bad = false;
	char			*errname;

	/* Complain about zero length names. */
	if (*name == '\0' && should_warn_about_name(ctx)) {
		str_warn(ctx, descr_render(dsc), _("Zero length name found."));
		return 0;
	}

	/* control characters */
	for (p = name; *p; p++) {
		if ((*p >= 1 && *p <= 31) || *p == 127) {
			bad = true;
			break;
		}
	}

	if (bad && should_warn_about_name(ctx)) {
		errname = string_escape(name);
		if (!errname) {
			str_errno(ctx, descr_render(dsc));
			return -1;
		}
		str_info(ctx, descr_render(dsc),
_("Control character found in %s name \"%s\"."),
				namedescr, errname);
		free(errname);
	}

	return 0;
}

/*
 * Iterate a directory looking for filenames with problematic
 * characters.
 */
static int
check_dirent_names(
	struct scrub_ctx	*ctx,
	struct descr		*dsc,
	int			*fd,
	struct xfs_bulkstat	*bstat)
{
	struct unicrash		*uc = NULL;
	DIR			*dir;
	struct dirent		*dentry;
	int			ret;

	dir = fdopendir(*fd);
	if (!dir) {
		str_errno(ctx, descr_render(dsc));
		return errno;
	}
	*fd = -1; /* closedir will close *fd for us */

	ret = unicrash_dir_init(&uc, ctx, bstat);
	if (ret) {
		str_liberror(ctx, ret, descr_render(dsc));
		goto out_unicrash;
	}

	errno = 0;
	dentry = readdir(dir);
	while (dentry) {
		if (uc)
			ret = unicrash_check_dir_name(uc, dsc, dentry);
		else
			ret = simple_check_name(ctx, dsc, _("directory"),
					dentry->d_name);
		if (ret) {
			str_liberror(ctx, ret, descr_render(dsc));
			break;
		}
		errno = 0;
		dentry = readdir(dir);
	}
	if (errno) {
		ret = errno;
		str_liberror(ctx, ret, descr_render(dsc));
	}
	unicrash_free(uc);

out_unicrash:
	closedir(dir);
	return ret;
}

#ifdef HAVE_LIBATTR
/* Routines to scan all of an inode's xattrs for name problems. */
struct attrns_decode {
	int			flags;
	const char		*name;
};

static const struct attrns_decode attr_ns[] = {
	{0,			"user"},
	{ATTR_ROOT,		"system"},
	{ATTR_SECURE,		"secure"},
	{0, NULL},
};

/*
 * Check all the xattr names in a particular namespace of a file handle
 * for Unicode normalization problems or collisions.
 */
static int
check_xattr_ns_names(
	struct scrub_ctx		*ctx,
	struct descr			*dsc,
	struct xfs_handle		*handle,
	struct xfs_bulkstat		*bstat,
	const struct attrns_decode	*attr_ns)
{
	struct attrlist_cursor		cur;
	char				attrbuf[XFS_XATTR_LIST_MAX];
	char				keybuf[XATTR_NAME_MAX + 1];
	struct attrlist			*attrlist = (struct attrlist *)attrbuf;
	struct attrlist_ent		*ent;
	struct unicrash			*uc = NULL;
	int				i;
	int				error;

	error = unicrash_xattr_init(&uc, ctx, bstat);
	if (error) {
		str_liberror(ctx, error, descr_render(dsc));
		return error;
	}

	memset(attrbuf, 0, XFS_XATTR_LIST_MAX);
	memset(&cur, 0, sizeof(cur));
	memset(keybuf, 0, XATTR_NAME_MAX + 1);
	error = attr_list_by_handle(handle, sizeof(*handle), attrbuf,
			XFS_XATTR_LIST_MAX, attr_ns->flags, &cur);
	while (!error) {
		/* Examine the xattrs. */
		for (i = 0; i < attrlist->al_count; i++) {
			ent = ATTR_ENTRY(attrlist, i);
			snprintf(keybuf, XATTR_NAME_MAX, "%s.%s", attr_ns->name,
					ent->a_name);
			if (uc)
				error = unicrash_check_xattr_name(uc, dsc,
						keybuf);
			else
				error = simple_check_name(ctx, dsc,
						_("extended attribute"),
						keybuf);
			if (error) {
				str_liberror(ctx, error, descr_render(dsc));
				goto out;
			}
		}

		if (!attrlist->al_more)
			break;
		error = attr_list_by_handle(handle, sizeof(*handle), attrbuf,
				XFS_XATTR_LIST_MAX, attr_ns->flags, &cur);
	}
	if (error) {
		if (errno == ESTALE)
			errno = 0;
		error = errno;
		if (errno)
			str_errno(ctx, descr_render(dsc));
	}
out:
	unicrash_free(uc);
	return error;
}

/*
 * Check all the xattr names in all the xattr namespaces for problematic
 * characters.
 */
static int
check_xattr_names(
	struct scrub_ctx		*ctx,
	struct descr			*dsc,
	struct xfs_handle		*handle,
	struct xfs_bulkstat		*bstat)
{
	const struct attrns_decode	*ns;
	int				ret;

	for (ns = attr_ns; ns->name; ns++) {
		ret = check_xattr_ns_names(ctx, dsc, handle, bstat, ns);
		if (ret)
			break;
	}
	return ret;
}
#else
# define check_xattr_names(c, d, h, b)	(0)
#endif /* HAVE_LIBATTR */

static int
render_ino_from_handle(
	struct scrub_ctx	*ctx,
	char			*buf,
	size_t			buflen,
	void			*data)
{
	struct xfs_bulkstat	*bstat = data;

	return scrub_render_ino_descr(ctx, buf, buflen, bstat->bs_ino,
			bstat->bs_gen, NULL);
}

/*
 * Verify the connectivity of the directory tree.
 * We know that the kernel's open-by-handle function will try to reconnect
 * parents of an opened directory, so we'll accept that as sufficient.
 *
 * Check for potential Unicode collisions in names.
 */
static int
check_inode_names(
	struct scrub_ctx	*ctx,
	struct xfs_handle	*handle,
	struct xfs_bulkstat	*bstat,
	void			*arg)
{
	DEFINE_DESCR(dsc, ctx, render_ino_from_handle);
	bool			*aborted = arg;
	int			fd = -1;
	int			error = 0;
	int			err2;

	descr_set(&dsc, bstat);
	background_sleep();

	/* Warn about naming problems in xattrs. */
	if (bstat->bs_xflags & FS_XFLAG_HASATTR) {
		error = check_xattr_names(ctx, &dsc, handle, bstat);
		if (error)
			goto out;
	}

	/*
	 * Warn about naming problems in the directory entries.  Opening the
	 * dir by handle means the kernel will try to reconnect it to the root.
	 * If the reconnection fails due to corruption in the parents we get
	 * ESTALE, which is why we skip phase 5 if we found corruption.
	 */
	if (S_ISDIR(bstat->bs_mode)) {
		fd = scrub_open_handle(handle);
		if (fd < 0) {
			error = errno;
			if (error == ESTALE)
				return ESTALE;
			str_errno(ctx, descr_render(&dsc));
			goto out;
		}

		error = check_dirent_names(ctx, &dsc, &fd, bstat);
		if (error)
			goto out;
	}

out:
	progress_add(1);
	if (fd >= 0) {
		err2 = close(fd);
		if (err2)
			str_errno(ctx, descr_render(&dsc));
		if (!error && err2)
			error = err2;
	}

	if (error)
		*aborted = true;
	if (!error && *aborted)
		error = ECANCELED;

	return error;
}

#ifndef FS_IOC_GETFSLABEL
# define FSLABEL_MAX		256
# define FS_IOC_GETFSLABEL	_IOR(0x94, 49, char[FSLABEL_MAX])
#endif /* FS_IOC_GETFSLABEL */

static int
scrub_render_mountpoint(
	struct scrub_ctx	*ctx,
	char			*buf,
	size_t			buflen,
	void			*data)
{
	return snprintf(buf, buflen, _("%s"), ctx->mntpoint);
}

/*
 * Check the filesystem label for Unicode normalization problems or misleading
 * sequences.
 */
static int
check_fs_label(
	struct scrub_ctx		*ctx)
{
	DEFINE_DESCR(dsc, ctx, scrub_render_mountpoint);
	char				label[FSLABEL_MAX];
	struct unicrash			*uc = NULL;
	int				error;

	error = unicrash_fs_label_init(&uc, ctx);
	if (error) {
		str_liberror(ctx, error, descr_render(&dsc));
		return error;
	}

	descr_set(&dsc, NULL);

	/* Retrieve label; quietly bail if we don't support that. */
	error = ioctl(ctx->mnt.fd, FS_IOC_GETFSLABEL, &label);
	if (error) {
		if (errno != EOPNOTSUPP && errno != ENOTTY) {
			error = errno;
			perror(ctx->mntpoint);
		}
		goto out;
	}

	/* Ignore empty labels. */
	if (label[0] == 0)
		goto out;

	/* Otherwise check for weirdness. */
	if (uc)
		error = unicrash_check_fs_label(uc, &dsc, label);
	else
		error = simple_check_name(ctx, &dsc, _("filesystem label"),
				label);
	if (error)
		str_liberror(ctx, error, descr_render(&dsc));
out:
	unicrash_free(uc);
	return error;
}

typedef int (*fs_scan_item_fn)(struct scrub_ctx *, struct action_list *);

struct fs_scan_item {
	struct action_list	alist;
	bool			*abortedp;
	fs_scan_item_fn		scrub_fn;
};

/* Run one full-fs scan scrubber in this thread. */
static void
fs_scan_worker(
	struct workqueue	*wq,
	xfs_agnumber_t		nr,
	void			*arg)
{
	struct timespec		tv;
	struct fs_scan_item	*item = arg;
	struct scrub_ctx	*ctx = wq->wq_ctx;
	int			ret;

	/*
	 * Delay each successive fs scan by a second so that the threads are
	 * less likely to contend on the inobt and inode buffers.
	 */
	if (nr) {
		tv.tv_sec = nr;
		tv.tv_nsec = 0;
		nanosleep(&tv, NULL);
	}

	ret = item->scrub_fn(ctx, &item->alist);
	if (ret) {
		str_liberror(ctx, ret, _("checking fs scan metadata"));
		*item->abortedp = true;
		goto out;
	}

	ret = action_list_process(ctx, ctx->mnt.fd, &item->alist,
			ALP_COMPLAIN_IF_UNFIXED | ALP_NOPROGRESS);
	if (ret) {
		str_liberror(ctx, ret, _("repairing fs scan metadata"));
		*item->abortedp = true;
		goto out;
	}

out:
	free(item);
	return;
}

/* Queue one full-fs scan scrubber. */
static int
queue_fs_scan(
	struct workqueue	*wq,
	bool			*abortedp,
	xfs_agnumber_t		nr,
	fs_scan_item_fn		scrub_fn)
{
	struct fs_scan_item	*item;
	struct scrub_ctx	*ctx = wq->wq_ctx;
	int			ret;

	item = malloc(sizeof(struct fs_scan_item));
	if (!item) {
		ret = ENOMEM;
		str_liberror(ctx, ret, _("setting up fs scan"));
		return ret;
	}
	action_list_init(&item->alist);
	item->scrub_fn = scrub_fn;
	item->abortedp = abortedp;

	ret = -workqueue_add(wq, fs_scan_worker, nr, item);
	if (ret)
		str_liberror(ctx, ret, _("queuing fs scan work"));

	return ret;
}

/* Run multiple full-fs scan scrubbers at the same time. */
static int
run_kernel_fs_scan_scrubbers(
	struct scrub_ctx	*ctx)
{
	struct workqueue	wq_fs_scan;
	unsigned int		nr_threads = scrub_nproc_workqueue(ctx);
	xfs_agnumber_t		nr = 0;
	bool			aborted = false;
	int			ret, ret2;

	ret = -workqueue_create(&wq_fs_scan, (struct xfs_mount *)ctx,
			nr_threads);
	if (ret) {
		str_liberror(ctx, ret, _("setting up fs scan workqueue"));
		return ret;
	}

	/*
	 * The nlinks scanner is much faster than quotacheck because it only
	 * walks directories, so we start it first.
	 */
	ret = queue_fs_scan(&wq_fs_scan, &aborted, nr, scrub_nlinks);
	if (ret)
		goto wait;

	if (nr_threads > 1)
		nr++;

	ret = queue_fs_scan(&wq_fs_scan, &aborted, nr, scrub_quotacheck);
	if (ret)
		goto wait;

wait:
	ret2 = -workqueue_terminate(&wq_fs_scan);
	if (ret2) {
		str_liberror(ctx, ret2, _("joining fs scan workqueue"));
		if (!ret)
			ret = ret2;
	}
	if (aborted && !ret)
		ret = ECANCELED;

	workqueue_destroy(&wq_fs_scan);
	return ret;
}

/* Check directory connectivity. */
int
phase5_func(
	struct scrub_ctx	*ctx)
{
	bool			aborted = false;
	int			ret;

	/*
	 * Check and fix anything that requires a full filesystem scan.  We do
	 * this after we've checked all inodes and repaired anything that could
	 * get in the way of a scan.
	 */
	ret = run_kernel_fs_scan_scrubbers(ctx);
	if (ret)
		return ret;

	if (ctx->corruptions_found || ctx->unfixable_errors) {
		str_info(ctx, ctx->mntpoint,
_("Filesystem has errors, skipping connectivity checks."));
		return 0;
	}

	ret = check_fs_label(ctx);
	if (ret)
		return ret;

	ret = scrub_scan_all_inodes(ctx, check_inode_names, &aborted);
	if (ret)
		return ret;
	if (aborted)
		return ECANCELED;

	scrub_report_preen_triggers(ctx);
	return 0;
}

/* Estimate how much work we're going to do. */
int
phase5_estimate(
	struct scrub_ctx	*ctx,
	uint64_t		*items,
	unsigned int		*nr_threads,
	int			*rshift)
{
	*items = scrub_estimate_iscan_work(ctx);
	*nr_threads = scrub_nproc(ctx) * 2;
	*rshift = 0;
	return 0;
}
