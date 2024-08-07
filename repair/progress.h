// SPDX-License-Identifier: GPL-2.0

#ifndef	_XFS_REPAIR_PROGRESS_RPT_H_
#define	_XFS_REPAIR_PROGRESS_RPT_H_

#define PROG_RPT_DEFAULT	(15*60)	 /* default 15 minute report interval */
#define	PHASE_START		0
#define	PHASE_END		1


#define	PROG_FMT_ZERO_LOG	0	/* Phase 2 */
#define	PROG_FMT_SCAN_AG 	1

#define	PROG_FMT_AGI_UNLINKED 	2	/* Phase 3 */
#define	PROG_FMT_UNCERTAIN      3
#define	PROG_FMT_PROCESS_INO	4
#define	PROG_FMT_NEW_INODES	5

#define	PROG_FMT_DUP_EXTENT	6	/* Phase 4 */
#define	PROG_FMT_INIT_RTEXT	7
#define	PROG_FMT_RESET_RTBM	8
#define	PROG_FMT_DUP_BLOCKS	9

#define	PROG_FMT_REBUILD_AG	10	/* Phase 5 */

#define	PROG_FMT_TRAVERSAL	11	/* Phase 6 */
#define	PROG_FMT_TRAVERSSUB	12
#define	PROG_FMT_DISCONINODE	13

#define	PROGRESS_FMT_CORR_LINK	14	/* Phase 7 */
#define	PROGRESS_FMT_VRFY_LINK 	15

#define	DURATION_BUF_SIZE	512

extern void init_progress_rpt(void);
extern void stop_progress_rpt(void);
extern void summary_report(void);
extern int  set_progress_msg(int report, uint64_t total);
extern uint64_t print_final_rpt(void);
extern char *timestamp(struct xfs_mount *mp, int end, int phase, char *buf);
char *duration(time_t val, char *buf) __attribute__((nonnull(2)));
extern int do_parallel;

#define	PROG_RPT_INC(a,b) if (ag_stride && prog_rpt_done) (a) += (b)

#endif	/* _XFS_REPAIR_PROGRESS_RPT_H_ */
