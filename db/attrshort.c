// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001,2004-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include "libxfs.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "bit.h"
#include "attrshort.h"

static int	attr_sf_entry_name_count(void *obj, int startoff);
static int	attr_sf_entry_value_count(void *obj, int startoff);
static int	attr_sf_entry_value_offset(void *obj, int startoff, int idx);
static int	attr_shortform_list_count(void *obj, int startoff);
static int	attr_shortform_list_offset(void *obj, int startoff, int idx);

static int	attr_sf_entry_pptr_count(void *obj, int startoff);

const field_t	attr_shortform_flds[] = {
	{ "hdr", FLDT_ATTR_SF_HDR, OI(0), C1, 0, TYP_NONE },
	{ "list", FLDT_ATTR_SF_ENTRY, attr_shortform_list_offset,
	  attr_shortform_list_count, FLD_ARRAY|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ NULL }
};

#define	HOFF(f)	bitize(offsetof(struct xfs_attr_sf_hdr, f))
const field_t	attr_sf_hdr_flds[] = {
	{ "totsize", FLDT_UINT16D, OI(HOFF(totsize)), C1, 0, TYP_NONE },
	{ "count", FLDT_UINT8D, OI(HOFF(count)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	EOFF(f)	bitize(offsetof(struct xfs_attr_sf_entry, f))
const field_t	attr_sf_entry_flds[] = {
	{ "namelen", FLDT_UINT8D, OI(EOFF(namelen)), C1, 0, TYP_NONE },
	{ "valuelen", FLDT_UINT8D, OI(EOFF(valuelen)), C1, 0, TYP_NONE },
	{ "flags", FLDT_UINT8X, OI(EOFF(flags)), C1, FLD_SKIPALL, TYP_NONE },
	{ "root", FLDT_UINT1,
	  OI(EOFF(flags) + bitsz(uint8_t) - XFS_ATTR_ROOT_BIT - 1), C1, 0,
	  TYP_NONE },
	{ "secure", FLDT_UINT1,
	  OI(EOFF(flags) + bitsz(uint8_t) - XFS_ATTR_SECURE_BIT - 1), C1, 0,
	  TYP_NONE },
	{ "parent", FLDT_UINT1,
	  OI(EOFF(flags) + bitsz(uint8_t) - XFS_ATTR_PARENT_BIT - 1), C1, 0,
	  TYP_NONE },
	{ "name", FLDT_CHARNS, OI(EOFF(nameval)), attr_sf_entry_name_count,
	  FLD_COUNT, TYP_NONE },
	{ "parent_dir", FLDT_PARENT_REC, attr_sf_entry_value_offset,
	  attr_sf_entry_pptr_count, FLD_COUNT | FLD_OFFSET, TYP_NONE },
	{ "value", FLDT_CHARNS, attr_sf_entry_value_offset,
	  attr_sf_entry_value_count, FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ NULL }
};

static int
attr_sf_entry_name_count(
	void				*obj,
	int				startoff)
{
	struct xfs_attr_sf_entry	*e;

	ASSERT(bitoffs(startoff) == 0);
	e = (struct xfs_attr_sf_entry *)((char *)obj + byteize(startoff));
	return e->namelen;
}

int
attr_sf_entry_size(
	void				*obj,
	int				startoff,
	int				idx)
{
	struct xfs_attr_sf_entry	*e;
	int				i;
	struct xfs_attr_sf_hdr		*hdr;

	ASSERT(bitoffs(startoff) == 0);
	hdr = (struct xfs_attr_sf_hdr *)((char *)obj + byteize(startoff));
	e = libxfs_attr_sf_firstentry(hdr);
	for (i = 0; i < idx; i++)
		e = xfs_attr_sf_nextentry(e);
	return bitize((int)xfs_attr_sf_entsize(e));
}

static int
attr_sf_entry_value_count(
	void				*obj,
	int				startoff)
{
	struct xfs_attr_sf_entry	*e;

	ASSERT(bitoffs(startoff) == 0);
	e = (struct xfs_attr_sf_entry *)((char *)obj + byteize(startoff));

	if ((e->flags & XFS_ATTR_NSP_ONDISK_MASK) == XFS_ATTR_PARENT)
		return 0;

	return e->valuelen;
}

/*ARGSUSED*/
static int
attr_sf_entry_value_offset(
	void				*obj,
	int				startoff,
	int				idx)
{
	struct xfs_attr_sf_entry	*e;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	e = (struct xfs_attr_sf_entry *)((char *)obj + byteize(startoff));
	return bitize((int)((char *)&e->nameval[e->namelen] - (char *)e));
}

static int
attr_shortform_list_count(
	void				*obj,
	int				startoff)
{
	struct xfs_attr_sf_hdr		*hdr;

	ASSERT(bitoffs(startoff) == 0);
	hdr = (struct xfs_attr_sf_hdr *)((char *)obj + byteize(startoff));
	return hdr->count;
}

static int
attr_shortform_list_offset(
	void				*obj,
	int				startoff,
	int				idx)
{
	struct xfs_attr_sf_entry	*e;
	int				i;
	struct xfs_attr_sf_hdr		*hdr;

	ASSERT(bitoffs(startoff) == 0);
	hdr = (struct xfs_attr_sf_hdr *)((char *)obj + byteize(startoff));
	e = libxfs_attr_sf_firstentry(hdr);
	for (i = 0; i < idx; i++)
		e = xfs_attr_sf_nextentry(e);
	return bitize((int)((char *)e - (char *)hdr));
}

/*ARGSUSED*/
int
attrshort_size(
	void				*obj,
	int				startoff,
	int				idx)
{
	struct xfs_attr_sf_entry	*e;
	int				i;
	struct xfs_attr_sf_hdr		*hdr;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	hdr = (struct xfs_attr_sf_hdr *)((char *)obj + byteize(startoff));
	e = libxfs_attr_sf_firstentry(hdr);
	for (i = 0; i < hdr->count; i++)
		e = xfs_attr_sf_nextentry(e);
	return bitize((int)((char *)e - (char *)hdr));
}

static int
attr_sf_entry_pptr_count(
	void				*obj,
	int				startoff)
{
	struct xfs_attr_sf_entry	*e;

	ASSERT(bitoffs(startoff) == 0);
	e = (struct xfs_attr_sf_entry *)((char *)obj + byteize(startoff));

	if ((e->flags & XFS_ATTR_NSP_ONDISK_MASK) != XFS_ATTR_PARENT)
		return 0;

	return 1;
}
