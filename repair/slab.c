// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 */
#include "libxfs.h"
#include "slab.h"

#undef SLAB_DEBUG

#ifdef SLAB_DEBUG
# define dbg_printf(f, a...)  do {printf(f, ## a); fflush(stdout); } while (0)
#else
# define dbg_printf(f, a...)
#endif

/*
 * Slab Arrays and Bags
 *
 * The slab array is a dynamically growable linear array.  Internally it
 * maintains a list of slabs of increasing size; when a slab fills up, another
 * is allocated.  Each slab is sorted individually, which means that one must
 * use an iterator to walk the entire logical array, sorted order or otherwise.
 * Array items can neither be removed nor accessed randomly, since (at the
 * moment) the only user of them (storing reverse mappings) doesn't need either
 * piece.  Pointers are not stable across sort operations.
 *
 * A bag is a collection of pointers.  The bag can be added to or removed from
 * arbitrarily, and the bag items can be iterated.  Bags are used to process
 * rmaps into refcount btree entries.
 */

/*
 * Slabs -- each slab_hdr holds an array of items; when a slab_hdr fills up, we
 * allocate a new one and add to that one.  The slab object coordinates the
 * slab_hdrs.
 */

/* Each slab holds at least 4096 items */
#define MIN_SLAB_NR		4096
/* and cannot be larger than 128M */
#define MAX_SLAB_SIZE		(128 * 1048576)
struct xfs_slab_hdr {
	uint32_t		sh_nr;
	uint32_t		sh_inuse;	/* items in use */
	struct xfs_slab_hdr	*sh_next;	/* next slab hdr */
						/* objects follow */
};

struct xfs_slab {
	uint64_t		s_nr_slabs;	/* # of slabs */
	uint64_t		s_nr_items;	/* # of items */
	struct xfs_slab_hdr	*s_first;	/* first slab header */
	struct xfs_slab_hdr	*s_last;	/* last sh_next pointer */
	size_t			s_item_sz;	/* item size */
};

/*
 * Slab cursors -- each slab_hdr_cursor tracks a slab_hdr; the slab_cursor
 * tracks the slab_hdr_cursors.  If a compare_fn is specified, the cursor
 * returns objects in increasing order (if you've previously sorted the
 * slabs with qsort_slab()).  If compare_fn == NULL, it returns slab items
 * in order.
 */
struct xfs_slab_hdr_cursor {
	struct xfs_slab_hdr	*hdr;		/* a slab header */
	uint32_t		loc;		/* where we are in the slab */
};

typedef int (*xfs_slab_compare_fn)(const void *, const void *);

struct xfs_slab_cursor {
	uint64_t			nr;		/* # of per-slab cursors */
	struct xfs_slab			*slab;		/* pointer to the slab */
	struct xfs_slab_hdr_cursor	*last_hcur;	/* last header we took from */
	xfs_slab_compare_fn		compare_fn;	/* compare items */
	struct xfs_slab_hdr_cursor	hcur[0];	/* per-slab cursors */
};

/*
 * Create a slab to hold some objects of a particular size.
 */
int
init_slab(
	struct xfs_slab	**slab,
	size_t		item_size)
{
	struct xfs_slab	*ptr;

	ptr = calloc(1, sizeof(struct xfs_slab));
	if (!ptr)
		return -ENOMEM;
	ptr->s_item_sz = item_size;
	ptr->s_last = NULL;
	*slab = ptr;

	return 0;
}

/*
 * Frees a slab.
 */
void
free_slab(
	struct xfs_slab		**slab)
{
	struct xfs_slab		*ptr;
	struct xfs_slab_hdr	*hdr;
	struct xfs_slab_hdr	*nhdr;

	ptr = *slab;
	if (!ptr)
		return;
	hdr = ptr->s_first;
	while (hdr) {
		nhdr = hdr->sh_next;
		free(hdr);
		hdr = nhdr;
	}
	free(ptr);
	*slab = NULL;
}

static void *
slab_ptr(
	struct xfs_slab		*slab,
	struct xfs_slab_hdr	*hdr,
	uint32_t		idx)
{
	char			*p;

	ASSERT(idx < hdr->sh_inuse);
	p = (char *)(hdr + 1);
	p += slab->s_item_sz * idx;
	return p;
}

/*
 * Add an item to the slab.
 */
int
slab_add(
	struct xfs_slab		*slab,
	void			*item)
{
	struct xfs_slab_hdr	*hdr;
	void			*p;

	hdr = slab->s_last;
	if (!hdr || hdr->sh_inuse == hdr->sh_nr) {
		uint32_t	n;

		n = (hdr ? hdr->sh_nr * 2 : MIN_SLAB_NR);
		if (n * slab->s_item_sz > MAX_SLAB_SIZE)
			n = MAX_SLAB_SIZE / slab->s_item_sz;
		hdr = malloc(sizeof(struct xfs_slab_hdr) + (n * slab->s_item_sz));
		if (!hdr)
			return -ENOMEM;
		hdr->sh_nr = n;
		hdr->sh_inuse = 0;
		hdr->sh_next = NULL;
		if (slab->s_last)
			slab->s_last->sh_next = hdr;
		if (!slab->s_first)
			slab->s_first = hdr;
		slab->s_last = hdr;
		slab->s_nr_slabs++;
	}
	hdr->sh_inuse++;
	p = slab_ptr(slab, hdr, hdr->sh_inuse - 1);
	memcpy(p, item, slab->s_item_sz);
	slab->s_nr_items++;

	return 0;
}

#include "threads.h"

struct qsort_slab {
	struct xfs_slab		*slab;
	struct xfs_slab_hdr	*hdr;
	int			(*compare_fn)(const void *, const void *);
};

static void
qsort_slab_helper(
	struct workqueue	*wq,
	xfs_agnumber_t		agno,
	void			*arg)
{
	struct qsort_slab	*qs = arg;

	qsort(slab_ptr(qs->slab, qs->hdr, 0), qs->hdr->sh_inuse,
			qs->slab->s_item_sz, qs->compare_fn);
	free(qs);
}

/*
 * Sort the items in the slab.  Do not run this method if there are any
 * cursors holding on to the slab.
 */
void
qsort_slab(
	struct xfs_slab		*slab,
	int (*compare_fn)(const void *, const void *))
{
	struct workqueue	wq;
	struct xfs_slab_hdr	*hdr;
	struct qsort_slab	*qs;

	/*
	 * If we don't have that many slabs, we're probably better
	 * off skipping all the thread overhead.
	 */
	if (slab->s_nr_slabs <= 4) {
		hdr = slab->s_first;
		while (hdr) {
			qsort(slab_ptr(slab, hdr, 0), hdr->sh_inuse,
					slab->s_item_sz, compare_fn);
			hdr = hdr->sh_next;
		}
		return;
	}

	create_work_queue(&wq, NULL, platform_nproc());
	hdr = slab->s_first;
	while (hdr) {
		qs = malloc(sizeof(struct qsort_slab));
		qs->slab = slab;
		qs->hdr = hdr;
		qs->compare_fn = compare_fn;
		queue_work(&wq, qsort_slab_helper, 0, qs);
		hdr = hdr->sh_next;
	}
	destroy_work_queue(&wq);
}

/*
 * init_slab_cursor() -- Create a slab cursor to iterate the slab items.
 *
 * @slab: The slab.
 * @compare_fn: If specified, use this function to return items in ascending order.
 * @cur: The new cursor.
 */
int
init_slab_cursor(
	struct xfs_slab		*slab,
	int (*compare_fn)(const void *, const void *),
	struct xfs_slab_cursor	**cur)
{
	struct xfs_slab_cursor	*c;
	struct xfs_slab_hdr_cursor	*hcur;
	struct xfs_slab_hdr	*hdr;

	c = malloc(sizeof(struct xfs_slab_cursor) +
		   (sizeof(struct xfs_slab_hdr_cursor) * slab->s_nr_slabs));
	if (!c)
		return -ENOMEM;
	c->nr = slab->s_nr_slabs;
	c->slab = slab;
	c->compare_fn = compare_fn;
	c->last_hcur = NULL;
	hcur = (struct xfs_slab_hdr_cursor *)(c + 1);
	hdr = slab->s_first;
	while (hdr) {
		hcur->hdr = hdr;
		hcur->loc = 0;
		hcur++;
		hdr = hdr->sh_next;
	}
	*cur = c;
	return 0;
}

/*
 * Free the slab cursor.
 */
void
free_slab_cursor(
	struct xfs_slab_cursor	**cur)
{
	if (!*cur)
		return;
	free(*cur);
	*cur = NULL;
}

/*
 * Return the smallest item in the slab, without advancing the iterator.
 * The slabs must be sorted prior to the creation of the cursor.
 */
void *
peek_slab_cursor(
	struct xfs_slab_cursor	*cur)
{
	struct xfs_slab_hdr_cursor	*hcur;
	void			*p = NULL;
	void			*q;
	uint64_t		i;

	cur->last_hcur = NULL;

	/* no compare function; inorder traversal */
	if (!cur->compare_fn) {
		if (!cur->last_hcur)
			cur->last_hcur = &cur->hcur[0];
		hcur = cur->last_hcur;
		while (hcur < &cur->hcur[cur->nr] &&
			hcur->loc >= hcur->hdr->sh_inuse)
			hcur++;
		if (hcur == &cur->hcur[cur->nr])
			return NULL;
		p = slab_ptr(cur->slab, hcur->hdr, hcur->loc);
		cur->last_hcur = hcur;
		return p;
	}

	/* otherwise return things in increasing order */
	for (i = 0, hcur = &cur->hcur[i]; i < cur->nr; i++, hcur++) {
		if (hcur->loc >= hcur->hdr->sh_inuse)
			continue;
		q = slab_ptr(cur->slab, hcur->hdr, hcur->loc);
		if (!p || cur->compare_fn(p, q) > 0) {
			p = q;
			cur->last_hcur = hcur;
		}
	}

	return p;
}

/*
 * After a peek operation, advance the cursor.
 */
void
advance_slab_cursor(
	struct xfs_slab_cursor	*cur)
{
	ASSERT(cur->last_hcur);
	cur->last_hcur->loc++;
}

/*
 * Retrieve the next item in the slab and advance the cursor.
 */
void *
pop_slab_cursor(
	struct xfs_slab_cursor	*cur)
{
	void			*p;

	p = peek_slab_cursor(cur);
	if (p)
		advance_slab_cursor(cur);
	return p;
}

/*
 * Return the number of items in the slab.
 */
uint64_t
slab_count(
	struct xfs_slab	*slab)
{
	return slab->s_nr_items;
}
