// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (c) 1995, 2001-2002, 2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include "platform_defs.h"
#include "xfs.h"
#include "handle.h"
#include "jdm.h"
#include "parent.h"

/* internal fshandle - typecast to a void for external use */
#define FSHANDLE_SZ		8
typedef struct fshandle {
	char fsh_space[FSHANDLE_SZ];
} fshandle_t;

/* private file handle - for use by open_by_fshandle */
#define FILEHANDLE_SZ		24
#define FILEHANDLE_SZ_FOLLOWING	14
#define FILEHANDLE_SZ_PAD	2
typedef struct filehandle {
	fshandle_t fh_fshandle;		/* handle of fs containing this inode */
	int16_t fh_sz_following;	/* bytes in handle after this member */
	char fh_pad[FILEHANDLE_SZ_PAD];	/* padding, must be zeroed */
	uint32_t fh_gen;		/* generation count */
	xfs_ino_t fh_ino;		/* 64 bit ino */
} filehandle_t;


static void
jdm_fill_filehandle( filehandle_t *handlep,
		     fshandle_t *fshandlep,
		     struct xfs_bstat *statp)
{
	handlep->fh_fshandle = *fshandlep;
	handlep->fh_sz_following = FILEHANDLE_SZ_FOLLOWING;
	memset(handlep->fh_pad, 0, FILEHANDLE_SZ_PAD);
	handlep->fh_gen = statp->bs_gen;
	handlep->fh_ino = statp->bs_ino;
}

static void
jdm_fill_filehandle_v5(
	struct filehandle	*handlep,
	struct fshandle		*fshandlep,
	struct xfs_bulkstat	*statp)
{
	handlep->fh_fshandle = *fshandlep;
	handlep->fh_sz_following = FILEHANDLE_SZ_FOLLOWING;
	memset(handlep->fh_pad, 0, FILEHANDLE_SZ_PAD);
	handlep->fh_gen = statp->bs_gen;
	handlep->fh_ino = statp->bs_ino;
}

jdm_fshandle_t *
jdm_getfshandle( char *mntpnt )
{
	fshandle_t *fshandlep;
	size_t fshandlesz;
	char resolved[MAXPATHLEN];

	/* sanity checks */
	ASSERT( sizeof( fshandle_t ) == FSHANDLE_SZ );
	ASSERT( sizeof( filehandle_t ) == FILEHANDLE_SZ );
	ASSERT( sizeof( filehandle_t )
		-
		offsetofmember( filehandle_t, fh_pad )
		==
		FILEHANDLE_SZ_FOLLOWING );
	ASSERT( sizeofmember( filehandle_t, fh_pad ) == FILEHANDLE_SZ_PAD );
	ASSERT( FILEHANDLE_SZ_PAD == sizeof( int16_t ));

	fshandlep = NULL; /* for lint */
	fshandlesz = sizeof( *fshandlep );

	if (!realpath( mntpnt, resolved ))
		return NULL;

	if (path_to_fshandle( resolved, ( void ** )&fshandlep, &fshandlesz ))
		return NULL;

	assert( fshandlesz == sizeof( *fshandlep ));

	return ( jdm_fshandle_t * )fshandlep;
}


/* externally visible functions */

void
jdm_new_filehandle( jdm_filehandle_t **handlep,
		    size_t *hlen,
		    jdm_fshandle_t *fshandlep,
		    struct xfs_bstat *statp)
{
	/* allocate and fill filehandle */
	*hlen = sizeof(filehandle_t);
	*handlep = (filehandle_t *) malloc(*hlen);

	if (*handlep)
		jdm_fill_filehandle(*handlep, (fshandle_t *) fshandlep, statp);
}

void
jdm_new_filehandle_v5(
	jdm_filehandle_t	**handlep,
	size_t			*hlen,
	jdm_fshandle_t		*fshandlep,
	struct xfs_bulkstat	*statp)
{
	/* allocate and fill filehandle */
	*hlen = sizeof(filehandle_t);
	*handlep = (filehandle_t *) malloc(*hlen);
	if (!*handlep)
		return;

	jdm_fill_filehandle_v5(*handlep, (struct fshandle *)fshandlep, statp);
}

/* ARGSUSED */
void
jdm_delete_filehandle( jdm_filehandle_t *handlep, size_t hlen )
{
	free(handlep);
}

intgen_t
jdm_open( jdm_fshandle_t *fshp, struct xfs_bstat *statp, intgen_t oflags )
{
	fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t fd;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	fd = open_by_fshandle( ( void * )&filehandle,
			     sizeof( filehandle ),
			     oflags );
	return fd;
}

intgen_t
jdm_open_v5(
	jdm_fshandle_t		*fshp,
	struct xfs_bulkstat	*statp,
	intgen_t		oflags)
{
	struct fshandle		*fshandlep = (struct fshandle *)fshp;
	struct filehandle	filehandle;

	jdm_fill_filehandle_v5(&filehandle, fshandlep, statp);
	return open_by_fshandle(&filehandle, sizeof(filehandle), oflags);
}

intgen_t
jdm_readlink( jdm_fshandle_t *fshp,
	      struct xfs_bstat *statp,
	      char *bufp, size_t bufsz )
{
	fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t rval;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = readlink_by_handle( ( void * )&filehandle,
				   sizeof( filehandle ),
				   ( void * )bufp,
				   bufsz );
	return rval;
}

intgen_t
jdm_readlink_v5(
	jdm_fshandle_t		*fshp,
	struct xfs_bulkstat	*statp,
	char			*bufp,
	size_t			bufsz)
{
	struct fshandle		*fshandlep = (struct fshandle *)fshp;
	struct filehandle	filehandle;

	jdm_fill_filehandle_v5(&filehandle, fshandlep, statp);
	return readlink_by_handle(&filehandle, sizeof(filehandle), bufp, bufsz);
}

int
jdm_attr_multi(	jdm_fshandle_t *fshp,
		struct xfs_bstat *statp,
		char *bufp, int rtrvcnt, int flags)
{
	fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	int rval;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = attr_multi_by_handle ( ( void * )&filehandle,
				      sizeof( filehandle ),
				      (void *) bufp,
				      rtrvcnt, flags);
	return rval;
}

int
jdm_attr_multi_v5(
	jdm_fshandle_t		*fshp,
	struct xfs_bulkstat	*statp,
	char			*bufp,
	int			rtrvcnt,
	int			flags)
{
	struct fshandle		*fshandlep = (struct fshandle *)fshp;
	struct filehandle	filehandle;

	jdm_fill_filehandle_v5(&filehandle, fshandlep, statp);
	return attr_multi_by_handle(&filehandle, sizeof(filehandle), bufp,
			rtrvcnt, flags);
}

int
jdm_attr_list(	jdm_fshandle_t *fshp,
		struct xfs_bstat *statp,
		char *bufp, size_t bufsz, int flags,
		struct attrlist_cursor *cursor)
{
	fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	int rval;

	/* prevent needless EINVAL from the kernel */
	if (bufsz > XFS_XATTR_LIST_MAX)
		bufsz = XFS_XATTR_LIST_MAX;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = attr_list_by_handle (( void * )&filehandle,
			sizeof( filehandle ),
			bufp, bufsz, flags, cursor);
	return rval;
}

int
jdm_attr_list_v5(
	jdm_fshandle_t		*fshp,
	struct xfs_bulkstat	*statp,
	char			*bufp,
	size_t			bufsz,
	int			flags,
	struct attrlist_cursor	*cursor)
{
	struct fshandle		*fshandlep = (struct fshandle *)fshp;
	struct filehandle	filehandle;

	/* prevent needless EINVAL from the kernel */
	if (bufsz > XFS_XATTR_LIST_MAX)
		bufsz = XFS_XATTR_LIST_MAX;

	jdm_fill_filehandle_v5(&filehandle, fshandlep, statp);
	return attr_list_by_handle(&filehandle, sizeof(filehandle), bufp,
			bufsz, flags, cursor);
}

int
jdm_parents( jdm_fshandle_t *fshp,
		struct xfs_bstat *statp,
		parent_t *bufp, size_t bufsz,
		unsigned int *count)
{
	errno = EOPNOTSUPP;
	return -1;
}

int
jdm_parents_v5(
	jdm_fshandle_t		*fshp,
	struct xfs_bulkstat	*statp,
	struct parent		*bufp,
	size_t			bufsz,
	unsigned int		*count)
{
	errno = EOPNOTSUPP;
	return -1;
}

int
jdm_parentpaths( jdm_fshandle_t *fshp,
		struct xfs_bstat *statp,
		parent_t *bufp, size_t bufsz,
		unsigned int *count)
{
	errno = EOPNOTSUPP;
	return -1;
}

int
jdm_parentpaths_v5(
	jdm_fshandle_t		*fshp,
	struct xfs_bulkstat	*statp,
	struct parent		*bufp,
	size_t			bufsz,
	unsigned int		*count)
{
	errno = EOPNOTSUPP;
	return -1;
}
