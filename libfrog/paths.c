// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2005-2006 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <paths.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "paths.h"
#include "input.h"
#include "projects.h"
#include <mntent.h>
#include "list.h"
#include <limits.h>

extern char *progname;

int fs_count;
int xfs_fs_count;
struct fs_path *fs_table;
struct fs_path *fs_path;

char *mtab_file;
#define PROC_MOUNTS	"/proc/self/mounts"

static int
fs_device_number(
	const char	*name,
	dev_t		*devnum)
{
	struct stat	sbuf;

	if (stat(name, &sbuf) < 0)
		return errno;
	/*
	 * We want to match st_rdev if the path provided is a device
	 * special file.  Otherwise we are looking for the the
	 * device id for the containing filesystem, in st_dev.
	 */
	if (S_ISBLK(sbuf.st_mode) || S_ISCHR(sbuf.st_mode))
		*devnum = sbuf.st_rdev;
	else
		*devnum = sbuf.st_dev;

	return 0;
}

/*
 * Find the FS table entry for the given path.  The "flags" argument
 * is a mask containing FS_MOUNT_POINT or FS_PROJECT_PATH (or both)
 * to indicate the type of table entry sought.
 * fs_table_lookup() finds the fs table entry for the filesystem hosting
 * the file represented in the "dir" argument. To compare against actual
 * mount point entries, use fs_table_lookup_mount() instead.
 */
struct fs_path *
fs_table_lookup(
	const char	*dir,
	uint		flags)
{
	uint		i;
	dev_t		dev = 0;

	if (fs_device_number(dir, &dev))
		return NULL;

	for (i = 0; i < fs_count; i++) {
		if (flags && !(flags & fs_table[i].fs_flags))
			continue;
		if (fs_table[i].fs_datadev == dev)
			return &fs_table[i];
	}
	return NULL;
}

static struct fs_path *
__fs_table_lookup_mount(
	const char	*dir,
	const char	*blkdev)
{
	uint		i;
	char		rpath[PATH_MAX];
	char		dpath[PATH_MAX];

	if (!dir && !blkdev)
		return NULL;

	if (dir && !realpath(dir, dpath))
		return NULL;
	if (blkdev && !realpath(blkdev, dpath))
		return NULL;

	for (i = 0; i < fs_count; i++) {
		if (fs_table[i].fs_flags != FS_MOUNT_POINT)
			continue;
		if (dir && !realpath(fs_table[i].fs_dir, rpath))
			continue;
		if (blkdev && !realpath(fs_table[i].fs_name, rpath))
			continue;
		if (strcmp(rpath, dpath) == 0)
			return &fs_table[i];
	}
	return NULL;
}

/*
 * Find the FS table entry describing an actual mount for the given path.
 * Unlike fs_table_lookup(), fs_table_lookup_mount() compares the "dir"
 * argument to actual mount point entries in the table. Accordingly, it
 * will find matches only if the "dir" argument is indeed mounted.
 */
struct fs_path *
fs_table_lookup_mount(
	const char	*dir)
{
	return __fs_table_lookup_mount(dir, NULL);
}

/*
 * Find the FS table entry describing an actual mount for the block device.
 * Unlike fs_table_lookup(), fs_table_lookup_blkdev() compares the "bdev"
 * argument to actual mount point names in the table. Accordingly, it
 * will find matches only if the "bdev" argument is indeed mounted.
 */
struct fs_path *
fs_table_lookup_blkdev(
	const char	*bdev)
{
	return __fs_table_lookup_mount(NULL, bdev);
}

static int
fs_table_insert(
	char		*dir,
	uint		prid,
	uint		flags,
	char		*fsname,
	char		*fslog,
	char		*fsrt)
{
	dev_t		datadev, logdev, rtdev;
	struct fs_path	*tmp_fs_table;
	int		error;

	datadev = logdev = rtdev = 0;
	error = fs_device_number(dir, &datadev);
	if (error)
		goto out_nodev;
	if (fslog) {
		error = fs_device_number(fslog, &logdev);
		if (error)
			goto out_nodev;
	}
	if (fsrt) {
		error = fs_device_number(fsrt, &rtdev);
		if (error)
			goto out_nodev;
	}

	if (!platform_test_xfs_path(dir))
		flags |= FS_FOREIGN;

	/*
	 * Make copies of the directory and data device path.
	 * The log device and real-time device, if non-null,
	 * are already the result of strdup() calls so we
	 * don't need to duplicate those.  In fact, this
	 * function is assumed to "consume" both of those
	 * pointers, meaning if an error occurs they will
	 * both get freed.
	 */
	error = ENOMEM;
	dir = strdup(dir);
	if (!dir)
		goto out_nodev;
	fsname = strdup(fsname);
	if (!fsname)
		goto out_noname;

	tmp_fs_table = realloc(fs_table, sizeof(fs_path_t) * (fs_count + 1));
	if (!tmp_fs_table)
		goto out_norealloc;
	fs_table = tmp_fs_table;

	/* Put foreign filesystems at the end, xfs filesystems at the front */
	if (flags & FS_FOREIGN || fs_count == 0) {
		fs_path = &fs_table[fs_count];
	} else {
		/* move foreign fs entries down, insert new one just before */
		memmove(&fs_table[xfs_fs_count + 1], &fs_table[xfs_fs_count],
			sizeof(fs_path_t)*(fs_count - xfs_fs_count));
		fs_path = &fs_table[xfs_fs_count];
	}
	fs_path->fs_dir = dir;
	fs_path->fs_prid = prid;
	fs_path->fs_flags = flags;
	fs_path->fs_name = fsname;
	fs_path->fs_log = fslog;
	fs_path->fs_rt = fsrt;
	fs_path->fs_datadev = datadev;
	fs_path->fs_logdev = logdev;
	fs_path->fs_rtdev = rtdev;
	fs_count++;
	if (!(flags & FS_FOREIGN))
		xfs_fs_count++;

	return 0;

out_norealloc:
	free(fsname);
out_noname:
	free(dir);
out_nodev:
	/* "Consume" fslog and fsrt even if there's an error */
	free(fslog);
	free(fsrt);

	return error;
}

/* Remove all the cached entries in the fs table. */
void
fs_table_destroy(void)
{
	int		i;
	struct fs_path	*fsp;

	for (i = 0, fsp = fs_table; i < fs_count; i++, fsp++) {
		free(fsp->fs_name);
		free(fsp->fs_dir);
		free(fsp->fs_log);
		free(fsp->fs_rt);
	}

	fs_count = 0;
	xfs_fs_count = 0;
	free(fs_table);
	fs_table = NULL;
}

/*
 * Table iteration (cursor-based) interfaces
 */

/*
 * Initialize an fs_table cursor.  If a directory path is supplied,
 * the cursor is set up to appear as though the table contains only
 * a single entry which represents the directory specified.
 * Otherwise it is set up to prepare for visiting all entries in the
 * global table, starting with the first.  "flags" can be either
 * FS_MOUNT_POINT or FS_PROJECT_PATH to limit what type of entries
 * will be selected by fs_cursor_next_entry().  0 can be used as a
 * wild card (selecting either type).
 */
void
fs_cursor_initialise(
	char		*dir,
	uint		flags,
	fs_cursor_t	*cur)
{
	fs_path_t	*path;

	memset(cur, 0, sizeof(*cur));
	if (dir) {
		if ((path = fs_table_lookup(dir, flags)) == NULL)
			return;
		cur->local = *path;
		cur->count = 1;
		cur->table = &cur->local;
	} else {
		cur->count = fs_count;
		cur->table = fs_table;
	}
	cur->flags = flags;
}

/*
 * Use the cursor to find the next entry in the table having the
 * type specified by the cursor's "flags" field.
 */
struct fs_path *
fs_cursor_next_entry(
	fs_cursor_t	*cur)
{
	while (cur->index < cur->count) {
		fs_path_t	*next = &cur->table[cur->index++];

		if (!cur->flags || (cur->flags & next->fs_flags))
			return next;
	}
	return NULL;
}

/*
 * Determines whether the "logdev" or "rtdev" mount options are
 * present for the given mount point.  If so, the value for each (a
 * device path) is returned in the pointers whose addresses are
 * provided.  The pointers are assigned NULL for an option not
 * present.  Note that the path buffers returned are allocated
 * dynamically and it is the caller's responsibility to free them.
 */
static int
fs_extract_mount_options(
	struct mntent	*mnt,
	char		**logp,
	char		**rtp)
{
	char		*fslog, *fsrt;

	/*
	 * Extract log device and realtime device from mount options.
	 *
	 * Note: the glibc hasmntopt implementation requires that the
	 * character in mnt_opts immediately after the search string
	 * must be a NULL ('\0'), a comma (','), or an equals ('=').
	 * Therefore we cannot search for 'logdev=' directly.
	 */
	if ((fslog = hasmntopt(mnt, "logdev")) && fslog[6] == '=')
		fslog += 7;
	if ((fsrt = hasmntopt(mnt, "rtdev")) && fsrt[5] == '=')
		fsrt += 6;

	/* Do this only after we've finished processing mount options */
	if (fslog) {
		fslog = strndup(fslog, strcspn(fslog, " ,"));
		if (!fslog)
			goto out_nomem;
	}
	if (fsrt) {
		fsrt = strndup(fsrt, strcspn(fsrt, " ,"));
		if (!fsrt) {
			free(fslog);
			goto out_nomem;
		}
	}
	*logp = fslog;
	*rtp = fsrt;

	return 0;

out_nomem:
	*logp = NULL;
	*rtp = NULL;
	fprintf(stderr, _("%s: unable to extract mount options for \"%s\"\n"),
		progname, mnt->mnt_dir);
	return ENOMEM;
}

/*
 * If *path is NULL, initialize the fs table with all xfs mount points in mtab
 * If *path is specified, search for that path in mtab
 *
 * Everything - path, devices, and mountpoints - are boiled down to realpath()
 * for comparison, but fs_table is populated with what comes from getmntent.
 */
static int
fs_table_initialise_mounts(
	char		*path)
{
	struct mntent	*mnt;
	FILE		*mtp;
	char		*fslog, *fsrt;
	int		error, found;
	char		rpath[PATH_MAX], rmnt_fsname[PATH_MAX], rmnt_dir[PATH_MAX];

	error = found = 0;
	fslog = fsrt = NULL;

	if (!mtab_file) {
		mtab_file = PROC_MOUNTS;
		if (access(mtab_file, R_OK) != 0)
			mtab_file = MOUNTED;
	}

	if ((mtp = setmntent(mtab_file, "r")) == NULL)
		return ENOENT;

	/* Use realpath to resolve symlinks, relative paths, etc */
	if (path)
		if (!realpath(path, rpath))
			return errno;

	while ((mnt = getmntent(mtp)) != NULL) {
		if (!strcmp(mnt->mnt_type, "autofs"))
			continue;
		if (!realpath(mnt->mnt_dir, rmnt_dir))
			continue;
		if (!realpath(mnt->mnt_fsname, rmnt_fsname))
			continue;

		if (path &&
		    ((strcmp(rpath, rmnt_dir) != 0) &&
		     (strcmp(rpath, rmnt_fsname) != 0)))
			continue;
		if (fs_extract_mount_options(mnt, &fslog, &fsrt))
			continue;
		(void) fs_table_insert(mnt->mnt_dir, 0, FS_MOUNT_POINT,
					mnt->mnt_fsname, fslog, fsrt);
		if (path) {
			found = 1;
			break;
		}
	}
	endmntent(mtp);

	if (path && !found)
		error = ENXIO;

	return error;
}

/*
 * Given a directory, match it up to a filesystem mount point.
 */
static struct fs_path *
fs_mount_point_from_path(
	const char	*dir)
{
	fs_cursor_t	cursor;
	fs_path_t	*fs;
	dev_t		dev = 0;

	if (fs_device_number(dir, &dev))
		return NULL;

	fs_cursor_initialise(NULL, FS_MOUNT_POINT, &cursor);
	while ((fs = fs_cursor_next_entry(&cursor))) {
		if (fs->fs_datadev == dev)
			break;
	}
	return fs;
}

static void
fs_table_insert_mount(
	char		*mount)
{
	int		error;

	error = fs_table_initialise_mounts(mount);
	if (error)
		fprintf(stderr, _("%s: cannot setup path for mount %s: %s\n"),
			progname, mount, strerror(error));
}

static int
fs_table_initialise_projects(
	char		*project,
	bool		all_mps_initialised)
{
	fs_project_path_t *path;
	fs_path_t	*fs;
	prid_t		prid = 0;
	int		error = 0, found = 0;

	if (project)
		prid = prid_from_string(project);

	setprpathent();
	while ((path = getprpathent()) != NULL) {
		if (project && prid != path->pp_prid)
			continue;
		fs = fs_mount_point_from_path(path->pp_pathname);
		if (!fs) {
			if (all_mps_initialised)
				fprintf(stderr,
	_("%s: cannot find mount point for path `%s': %s\n"), progname,
					path->pp_pathname, strerror(errno));
			continue;
		}
		(void) fs_table_insert(path->pp_pathname, path->pp_prid,
					FS_PROJECT_PATH, fs->fs_name,
					NULL, NULL);
		if (project) {
			found = 1;
			break;
		}
	}
	endprpathent();

	if (project && !found)
		error = ENOENT;

	return error;
}

static void
fs_table_insert_project(
	char		*project,
	bool		all_mps_initialised)
{
	int		error;

	error = fs_table_initialise_projects(project, all_mps_initialised);
	if (error)
		fprintf(stderr, _("%s: cannot setup path for project %s: %s\n"),
			progname, project, strerror(error));
}

/*
 * Initialize fs_table to contain the given set of mount points and
 * projects.  If mount_count is zero, mounts is ignored and the
 * table is populated with mounted filesystems.  If project_count is
 * zero, projects is ignored and the table is populated with all
 * projects defined in the projects file.
 */
void
fs_table_initialise(
	int	mount_count,
	char	*mounts[],
	int	project_count,
	char	*projects[])
{
	int	error;
	int	i;

	if (mount_count) {
		for (i = 0; i < mount_count; i++)
			fs_table_insert_mount(mounts[i]);
	} else {
		error = fs_table_initialise_mounts(NULL);
		if (error)
			goto out_error;
	}
	if (project_count) {
		for (i = 0; i < project_count; i++)
			fs_table_insert_project(projects[i], mount_count == 0);
	} else {
		error = fs_table_initialise_projects(NULL, mount_count == 0);
		if (error)
			goto out_error;
	}

	return;

out_error:
	fprintf(stderr, _("%s: cannot initialise path table: %s\n"),
		progname, strerror(error));
}

int
fs_table_insert_project_path(
	char		*dir,
	prid_t		prid)
{
	fs_path_t	*fs;
	int		error = 0;

	fs = fs_mount_point_from_path(dir);
	if (fs)
		error = fs_table_insert(dir, prid, FS_PROJECT_PATH,
					fs->fs_name, NULL, NULL);
	else
		error = ENOENT;

	return error;
}

/* Structured path components. */

struct path_list {
	struct list_head	p_head;
};

struct path_component {
	struct list_head	pc_list;
	uint64_t		pc_ino;
	char			*pc_fname;
};

/* Initialize a path component with a given name. */
struct path_component *
path_component_init(
	const char		*name,
	uint64_t		ino)
{
	struct path_component	*pc;

	pc = malloc(sizeof(struct path_component));
	if (!pc)
		return NULL;
	INIT_LIST_HEAD(&pc->pc_list);
	pc->pc_fname = strdup(name);
	if (!pc->pc_fname) {
		free(pc);
		return NULL;
	}
	pc->pc_ino = ino;
	return pc;
}

/* Free a path component. */
void
path_component_free(
	struct path_component	*pc)
{
	free(pc->pc_fname);
	free(pc);
}

/* Initialize a pathname or returns positive errno. */
struct path_list *
path_list_init(void)
{
	struct path_list	*path;

	path = malloc(sizeof(struct path_list));
	if (!path)
		return NULL;
	INIT_LIST_HEAD(&path->p_head);
	return path;
}

/* Empty out a pathname. */
void
path_list_free(
	struct path_list	*path)
{
	struct path_component	*pos;
	struct path_component	*n;

	list_for_each_entry_safe(pos, n, &path->p_head, pc_list) {
		path_list_del_component(path, pos);
		path_component_free(pos);
	}
	free(path);
}

/* Add a parent component to a pathname. */
void
path_list_add_parent_component(
	struct path_list	*path,
	struct path_component	*pc)
{
	list_add(&pc->pc_list, &path->p_head);
}

/* Add a component to a pathname. */
void
path_list_add_component(
	struct path_list	*path,
	struct path_component	*pc)
{
	list_add_tail(&pc->pc_list, &path->p_head);
}

/* Remove a component from a pathname. */
void
path_list_del_component(
	struct path_list	*path,
	struct path_component	*pc)
{
	list_del_init(&pc->pc_list);
}

/*
 * Convert a pathname into a string or returns -1 if the buffer isn't long
 * enough.
 */
ssize_t
path_list_to_string(
	const struct path_list	*path,
	char			*buf,
	size_t			buflen)
{
	struct path_component	*pos;
	char			*buf_end = buf + buflen;
	ssize_t			bytes = 0;
	int			ret;

	list_for_each_entry(pos, &path->p_head, pc_list) {
		if (buf >= buf_end)
			return -1;

		ret = snprintf(buf, buflen, "/%s", pos->pc_fname);
		if (ret < 0 || ret >= buflen)
			return -1;

		bytes += ret;
		buf += ret;
		buflen -= ret;
	}
	return bytes;
}

/* Walk each component of a path. */
int
path_walk_components(
	const struct path_list	*path,
	path_walk_fn_t		fn,
	void			*arg)
{
	struct path_component	*pos;
	int			ret;

	list_for_each_entry(pos, &path->p_head, pc_list) {
		ret = fn(pos->pc_fname, pos->pc_ino, arg);
		if (ret)
			return ret;
	}

	return 0;
}

/* Will this path contain a loop if we add this inode? */
bool
path_will_loop(
	const struct path_list	*path_list,
	uint64_t		ino)
{
	struct path_component	*pc;
	unsigned int		nr = 0;

	list_for_each_entry(pc, &path_list->p_head, pc_list) {
		if (pc->pc_ino == ino)
			return true;

		/* 256 path components should be enough for anyone. */
		if (++nr > 256)
			return true;
	}

	return false;
}
