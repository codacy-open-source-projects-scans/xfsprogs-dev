// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef XFS_SCRUB_VFS_H_
#define XFS_SCRUB_VFS_H_

/*
 * Visit a subdirectory prior to iterating entries in that subdirectory.
 * Return 0 to continue iteration or a positive error code to stop iterating
 * and return to the caller.
 */
typedef int (*scan_fs_tree_dir_fn)(struct scrub_ctx *, const char *,
		int, void *);

/*
 * Visit each directory entry in a directory.  Return 0 to continue iteration
 * or a positive error code to stop iterating and return to the caller.
 */
typedef int (*scan_fs_tree_dirent_fn)(struct scrub_ctx *, const char *,
		int, struct dirent *, struct stat *, void *);

int scan_fs_tree(struct scrub_ctx *ctx, scan_fs_tree_dir_fn dir_fn,
		scan_fs_tree_dirent_fn dirent_fn, void *arg);

int fstrim(struct scrub_ctx *ctx, uint64_t start, uint64_t len, uint64_t minlen);

#endif /* XFS_SCRUB_VFS_H_ */
