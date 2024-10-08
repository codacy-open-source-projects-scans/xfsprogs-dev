// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include "libxfs.h"
#include "command.h"
#include "freesp.h"
#include "io.h"
#include "type.h"
#include "output.h"
#include "init.h"
#include "malloc.h"
#include "libfrog/histogram.h"

static void	addhistent(int h);
static void	addtohist(xfs_agnumber_t agno, xfs_agblock_t agbno,
			  xfs_extlen_t len);
static int	freesp_f(int argc, char **argv);
static void	histinit(int maxlen);
static int	init(int argc, char **argv);
static void	printhist(void);
static void	scan_ag(xfs_agnumber_t agno);
static void	scanfunc_bno(struct xfs_btree_block *block, typnm_t typ, int level,
			     xfs_agf_t *agf);
static void	scanfunc_cnt(struct xfs_btree_block *block, typnm_t typ, int level,
			     xfs_agf_t *agf);
static void	scan_freelist(xfs_agf_t *agf);
static void	scan_sbtree(xfs_agf_t *agf, xfs_agblock_t root, typnm_t typ,
			    int nlevels,
			    void (*func)(struct xfs_btree_block *block, typnm_t typ,
					 int level, xfs_agf_t *agf));
static int	usage(void);

static int		agcount;
static xfs_agnumber_t	*aglist;
static int		alignment;
static int		countflag;
static int		dumpflag;
static int		equalsize;
static struct histogram	freesp_hist;
static int		multsize;
static int		seen1;
static int		summaryflag;

static const cmdinfo_t	freesp_cmd =
	{ "freesp", NULL, freesp_f, 0, -1, 0,
	  "[-bcdfs] [-A alignment] [-a agno]... [-e binsize] [-h h1]... [-m binmult]",
	  "summarize free space for filesystem", NULL };

static int
inaglist(
	xfs_agnumber_t	agno)
{
	int		i;

	if (agcount == 0)
		return 1;
	for (i = 0; i < agcount; i++)
		if (aglist[i] == agno)
			return 1;
	return 0;
}

/*
 * Report on freespace usage in xfs filesystem.
 */
static int
freesp_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;

	if (!init(argc, argv))
		return 0;

	if (dumpflag)
		dbprintf("%8s %8s %8s\n", "agno", "agbno", "len");

	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++)  {
		if (inaglist(agno))
			scan_ag(agno);
	}
	if (hist_buckets(&freesp_hist))
		printhist();
	if (summaryflag) {
		struct histogram_strings hstr = {
			.sum		= _("total free blocks"),
			.observations	= _("total free extents"),
			.averages	= _("average free extent size"),
		};

		hist_summarize(&freesp_hist, &hstr);
	}
	if (aglist)
		xfree(aglist);
	hist_free(&freesp_hist);
	return 0;
}

void
freesp_init(void)
{
	add_command(&freesp_cmd);
}

static void
aglistadd(
	char	*a)
{
	aglist = xrealloc(aglist, (agcount + 1) * sizeof(*aglist));
	aglist[agcount] = (xfs_agnumber_t)atoi(a);
	agcount++;
}

static int
init(
	int		argc,
	char		**argv)
{
	int		c;
	int		speced = 0;

	agcount = countflag = dumpflag = equalsize = multsize = optind = 0;
	seen1 = summaryflag = 0;
	aglist = NULL;

	while ((c = getopt(argc, argv, "A:a:bcde:h:m:s")) != EOF) {
		switch (c) {
		case 'A':
			alignment = atoi(optarg);
			break;
		case 'a':
			aglistadd(optarg);
			break;
		case 'b':
			if (speced)
				return usage();
			multsize = 2;
			speced = 1;
			break;
		case 'c':
			countflag = 1;
			break;
		case 'd':
			dumpflag = 1;
			break;
		case 'e':
			if (speced)
				return usage();
			equalsize = atoi(optarg);
			speced = 1;
			break;
		case 'h':
			if (speced && hist_buckets(&freesp_hist) == 0)
				return usage();
			addhistent(atoi(optarg));
			speced = 1;
			break;
		case 'm':
			if (speced)
				return usage();
			multsize = atoi(optarg);
			speced = 1;
			break;
		case 's':
			summaryflag = 1;
			break;
		default:
			return usage();
		}
	}
	if (optind != argc)
		return usage();
	if (!speced)
		multsize = 2;
	histinit((int)mp->m_sb.sb_agblocks);
	return 1;
}

static int
usage(void)
{
	dbprintf(_("freesp arguments: [-bcds] [-a agno] [-e binsize] [-h h1]... "
		 "[-m binmult]\n"));
	return 0;
}

static void
scan_ag(
	xfs_agnumber_t	agno)
{
	xfs_agf_t	*agf;

	push_cur();
	set_cur(&typtab[TYP_AGF], XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR(mp)),
				XFS_FSS_TO_BB(mp, 1), DB_RING_IGN, NULL);
	agf = iocur_top->data;
	scan_freelist(agf);
	if (countflag)
		scan_sbtree(agf, be32_to_cpu(agf->agf_cnt_root),
			TYP_CNTBT, be32_to_cpu(agf->agf_cnt_level),
			scanfunc_cnt);
	else
		scan_sbtree(agf, be32_to_cpu(agf->agf_bno_root),
			TYP_BNOBT, be32_to_cpu(agf->agf_bno_level),
			scanfunc_bno);
	pop_cur();
}

static int
scan_agfl(
	struct xfs_mount	*mp,
	xfs_agblock_t		bno,
	void			*priv)
{
	addtohist(*(xfs_agnumber_t *)priv, bno, 1);
	return 0;
}

static void
scan_freelist(
	xfs_agf_t	*agf)
{
	xfs_agnumber_t	seqno = be32_to_cpu(agf->agf_seqno);

	if (be32_to_cpu(agf->agf_flcount) == 0)
		return;
	push_cur();
	set_cur(&typtab[TYP_AGFL], XFS_AG_DADDR(mp, seqno, XFS_AGFL_DADDR(mp)),
				XFS_FSS_TO_BB(mp, 1), DB_RING_IGN, NULL);

	/* verify agf values before proceeding */
	if (be32_to_cpu(agf->agf_flfirst) >= libxfs_agfl_size(mp) ||
	    be32_to_cpu(agf->agf_fllast) >= libxfs_agfl_size(mp)) {
		dbprintf(_("agf %d freelist blocks bad, skipping "
			  "freelist scan\n"), seqno);
		pop_cur();
		return;
	}

	libxfs_agfl_walk(mp, agf, iocur_top->bp, scan_agfl, &seqno);
	pop_cur();
}

static void
scan_sbtree(
	xfs_agf_t	*agf,
	xfs_agblock_t	root,
	typnm_t		typ,
	int		nlevels,
	void		(*func)(struct xfs_btree_block	*block,
				typnm_t			typ,
				int			level,
				xfs_agf_t		*agf))
{
	xfs_agnumber_t	seqno = be32_to_cpu(agf->agf_seqno);

	push_cur();
	set_cur(&typtab[typ], XFS_AGB_TO_DADDR(mp, seqno, root),
		blkbb, DB_RING_IGN, NULL);
	if (iocur_top->data == NULL) {
		dbprintf(_("can't read btree block %u/%u\n"), seqno, root);
		return;
	}
	(*func)(iocur_top->data, typ, nlevels - 1, agf);
	pop_cur();
}

/*ARGSUSED*/
static void
scanfunc_bno(
	struct xfs_btree_block	*block,
	typnm_t			typ,
	int			level,
	xfs_agf_t		*agf)
{
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (!(be32_to_cpu(block->bb_magic) == XFS_ABTB_MAGIC ||
	      be32_to_cpu(block->bb_magic) == XFS_ABTB_CRC_MAGIC))
		return;

	if (level == 0) {
		rp = XFS_ALLOC_REC_ADDR(mp, block, 1);
		for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
			addtohist(be32_to_cpu(agf->agf_seqno),
					be32_to_cpu(rp[i].ar_startblock),
					be32_to_cpu(rp[i].ar_blockcount));
		return;
	}
	pp = XFS_ALLOC_PTR_ADDR(mp, block, 1, mp->m_alloc_mxr[1]);
	for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
		scan_sbtree(agf, be32_to_cpu(pp[i]), typ, level, scanfunc_bno);
}

static void
scanfunc_cnt(
	struct xfs_btree_block	*block,
	typnm_t			typ,
	int			level,
	xfs_agf_t		*agf)
{
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (!(be32_to_cpu(block->bb_magic) == XFS_ABTC_MAGIC ||
	      be32_to_cpu(block->bb_magic) == XFS_ABTC_CRC_MAGIC))
		return;

	if (level == 0) {
		rp = XFS_ALLOC_REC_ADDR(mp, block, 1);
		for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
			addtohist(be32_to_cpu(agf->agf_seqno),
					be32_to_cpu(rp[i].ar_startblock),
					be32_to_cpu(rp[i].ar_blockcount));
		return;
	}
	pp = XFS_ALLOC_PTR_ADDR(mp, block, 1, mp->m_alloc_mxr[1]);
	for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
		scan_sbtree(agf, be32_to_cpu(pp[i]), typ, level, scanfunc_cnt);
}

static void
addhistent(
	int	h)
{
	hist_add_bucket(&freesp_hist, h);
}

static void
addtohist(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len)
{
	if (alignment && (XFS_AGB_TO_FSB(mp,agno,agbno) % alignment))
		return;

	if (dumpflag)
		dbprintf("%8d %8d %8d\n", agno, agbno, len);
	hist_add(&freesp_hist, len);
}

static void
histinit(
	int	maxlen)
{
	int	i;

	hist_init(&freesp_hist);
	if (equalsize) {
		for (i = 1; i < maxlen; i += equalsize)
			addhistent(i);
	} else if (multsize) {
		for (i = 1; i < maxlen; i *= multsize)
			addhistent(i);
	} else {
		if (!seen1)
			addhistent(1);
	}
	hist_prepare(&freesp_hist, maxlen);
}

static void
printhist(void)
{
	struct histogram_strings hstr = {
		.sum		= _("blocks"),
		.observations	= _("extents"),
	};

	hist_print(&freesp_hist, &hstr);
}
