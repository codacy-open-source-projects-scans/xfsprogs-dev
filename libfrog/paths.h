// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef __LIBFROG_PATH_H__
#define __LIBFROG_PATH_H__

#include "platform_defs.h"

/*
 * XFS Filesystem Paths
 *
 * Utility routines for iterating and searching through the list
 * of known mounted filesystems and project paths.
 */

#define FS_MOUNT_POINT	(1<<0)
#define FS_PROJECT_PATH	(1<<1)
#define FS_FOREIGN	(1<<2)

typedef struct fs_path {
	char		*fs_name;	/* Data device for filesystem 	*/
	dev_t		fs_datadev;
	char		*fs_log;	/* External log device, if any	*/
	dev_t		fs_logdev;
	char		*fs_rt;		/* Realtime device, if any	*/
	dev_t		fs_rtdev;
	char		*fs_dir;	/* Directory / mount point	*/
	uint		fs_flags;	/* FS_{MOUNT_POINT,PROJECT_PATH}*/
	uint		fs_prid;	/* Project ID for tree root	*/
} fs_path_t;

extern int fs_count;		/* number of entries in fs table */
extern int xfs_fs_count;	/* number of xfs entries in fs table */
extern fs_path_t *fs_table;	/* array of entries in fs table  */
extern fs_path_t *fs_path;	/* current entry in the fs table */
extern char *mtab_file;

extern void fs_table_initialise(int, char *[], int, char *[]);
extern void fs_table_destroy(void);

extern int fs_table_insert_project_path(char *__dir, uint __projid);


extern fs_path_t *fs_table_lookup(const char *__dir, uint __flags);
extern fs_path_t *fs_table_lookup_mount(const char *__dir);
extern fs_path_t *fs_table_lookup_blkdev(const char *bdev);

typedef struct fs_cursor {
	uint		count;		/* total count of mount entries	*/
	uint		index;		/* current position in table	*/
	uint		flags;		/* iterator flags: mounts/trees */
	fs_path_t	*table;		/* local/global table pointer	*/
	fs_path_t	local;		/* space for single-entry table	*/
} fs_cursor_t;

extern void fs_cursor_initialise(char *__dir, uint __flags, fs_cursor_t *__cp);
extern fs_path_t *fs_cursor_next_entry(fs_cursor_t *__cp);

/* Path information. */

struct path_list;
struct path_component;

struct path_component *path_component_init(const char *name, uint64_t ino);
void path_component_free(struct path_component *pc);

struct path_list *path_list_init(void);
void path_list_free(struct path_list *path);
void path_list_add_parent_component(struct path_list *path,
		struct path_component *pc);
void path_list_add_component(struct path_list *path, struct path_component *pc);
void path_list_del_component(struct path_list *path, struct path_component *pc);

ssize_t path_list_to_string(const struct path_list *path, char *buf,
		size_t buflen);

typedef int (*path_walk_fn_t)(const char *name, uint64_t ino, void *arg);

int path_walk_components(const struct path_list *path, path_walk_fn_t fn,
		void *arg);

bool path_will_loop(const struct path_list *path, uint64_t ino);

#endif	/* __LIBFROG_PATH_H__ */
