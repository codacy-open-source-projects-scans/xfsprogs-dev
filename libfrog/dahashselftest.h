// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#include "libfrog/randbytes.h"

#ifndef __LIBFROG_DAHASHSELFTEST_H__
#define __LIBFROG_DAHASHSELFTEST_H__

/* 100 test cases */
static struct dahash_test {
	uint16_t	start;	/* random 12 bit offset in buf */
	uint16_t	length;	/* random 8 bit length of test */
	xfs_dahash_t	dahash;	/* expected dahash result */
	xfs_dahash_t	ascii_ci_dahash; /* expected ascii-ci dahash result */
} dahash_tests[] =
{
	{0x0567, 0x0097, 0x96951389, 0xc153aa0d},
	{0x0869, 0x0055, 0x6455ab4f, 0xd07f69bf},
	{0x0c51, 0x00be, 0x8663afde, 0xf9add90c},
	{0x044a, 0x00fc, 0x98fbe432, 0xbf2abb76},
	{0x0f29, 0x0079, 0x42371997, 0x282588b3},
	{0x08ba, 0x0052, 0x942be4f7, 0x2e023547},
	{0x01f2, 0x0013, 0x5262687e, 0x5266287e},
	{0x09e3, 0x00e2, 0x8ffb0908, 0x1da892f3},
	{0x007c, 0x0051, 0xb3158491, 0xb67f9e63},
	{0x0854, 0x001f, 0x83bb20d9, 0x22bb21db},
	{0x031b, 0x0008, 0x98970bdf, 0x9cd70adf},
	{0x0de7, 0x0027, 0xbfbf6f6c, 0xae3f296c},
	{0x0f76, 0x0005, 0x906a7105, 0x906a7105},
	{0x092e, 0x00d0, 0x86631850, 0xa3f6ac04},
	{0x0233, 0x0082, 0xdbdd914e, 0x5d8c7aac},
	{0x04c9, 0x0075, 0x5a400a9e, 0x12f60711},
	{0x0b66, 0x0099, 0xae128b45, 0x7551310d},
	{0x000d, 0x00ed, 0xe61c216a, 0xc22d3c4c},
	{0x0a31, 0x003d, 0xf69663b9, 0x51960bf8},
	{0x00a3, 0x0052, 0x643c39ae, 0xa93c73a8},
	{0x0125, 0x00d5, 0x7c310b0d, 0xf221cbb3},
	{0x0105, 0x004a, 0x06a77e74, 0xa4ef4561},
	{0x0858, 0x008e, 0x265bc739, 0xd6c36d9b},
	{0x045e, 0x0095, 0x13d6b192, 0x5f5c1d62},
	{0x0dab, 0x003c, 0xc4498704, 0x10414654},
	{0x00cd, 0x00b5, 0x802a4e2d, 0xfbd17c9d},
	{0x069b, 0x008c, 0x5df60f71, 0x91ddca5f},
	{0x0454, 0x006c, 0x5f03d8bb, 0x5c59fce0},
	{0x040e, 0x0032, 0x0ce513b5, 0xa8cd99b1},
	{0x0874, 0x00e2, 0x6a811fb3, 0xca028316},
	{0x0521, 0x00b4, 0x93296833, 0x2c4d4880},
	{0x0ddc, 0x00cf, 0xf9305338, 0x2c94210d},
	{0x0a70, 0x0023, 0x239549ea, 0x22b561aa},
	{0x083e, 0x0027, 0x2d88ba97, 0x5cd8bb9d},
	{0x0241, 0x00a7, 0xfe0b32e1, 0x17b506b8},
	{0x0dfc, 0x0096, 0x1a11e815, 0xee4141bd},
	{0x023e, 0x001e, 0xebc9a1f3, 0x5689a1f3},
	{0x067e, 0x0066, 0xb1067f81, 0xd9952571},
	{0x09ea, 0x000e, 0x46fd7247, 0x42b57245},
	{0x036b, 0x008c, 0x1a39acdf, 0x58bf1586},
	{0x078f, 0x0030, 0x964042ab, 0xb04218b9},
	{0x085c, 0x008f, 0x1829edab, 0x9ceca89c},
	{0x02ec, 0x009f, 0x6aefa72d, 0x634cc2a7},
	{0x043b, 0x00ce, 0x65642ff5, 0x6c8a584e},
	{0x0a32, 0x00b8, 0xbd82759e, 0x0f96a34f},
	{0x0d3c, 0x0087, 0xf4d66d54, 0xb71ba5f4},
	{0x09ec, 0x008a, 0x06bfa1ff, 0x576ca80f},
	{0x0902, 0x0015, 0x755025d2, 0x517225c2},
	{0x08fe, 0x000e, 0xf690ce2d, 0xf690cf3d},
	{0x00fb, 0x00dc, 0xe55f1528, 0x707d7d92},
	{0x0eaa, 0x003a, 0x0fe0a8d7, 0x87638cc5},
	{0x05fb, 0x0006, 0x86281cfb, 0x86281cf9},
	{0x0dd1, 0x00a7, 0x60ab51b4, 0xe28ef00c},
	{0x0005, 0x001b, 0xf51d969b, 0xe71dd6d3},
	{0x077c, 0x00dd, 0xc2fed268, 0xdc30c555},
	{0x0575, 0x00f5, 0x432c0b1a, 0x81dd7d16},
	{0x05be, 0x0088, 0x78baa04b, 0xd69b433e},
	{0x0c89, 0x0068, 0xeda9e428, 0xe9b4fa0a},
	{0x0f5c, 0x0068, 0xec143c76, 0x9947067a},
	{0x06a8, 0x0009, 0xd72651ce, 0xd72651ee},
	{0x060f, 0x008e, 0x765426cd, 0x2099626f},
	{0x07b1, 0x0047, 0x2cfcfa0c, 0x1a4baa07},
	{0x04f1, 0x0041, 0x55b172f9, 0x15331a79},
	{0x0e05, 0x00ac, 0x61efde93, 0x320568cc},
	{0x0bf7, 0x0097, 0x05b83eee, 0xc72fb7a3},
	{0x04e9, 0x00f3, 0x9928223a, 0xe8c77de2},
	{0x023a, 0x0005, 0xdfada9bc, 0xdfadb9be},
	{0x0acb, 0x000e, 0x2217cecd, 0x0017d6cd},
	{0x0148, 0x0060, 0xbc3f7405, 0xf5fd6615},
	{0x0764, 0x0059, 0xcbc201b1, 0xbb089bf4},
	{0x021f, 0x0059, 0x5d6b2256, 0xa16a0a59},
	{0x0f1e, 0x006c, 0xdefeeb45, 0xfc34f9d6},
	{0x071c, 0x00b9, 0xb9b59309, 0xb645eae2},
	{0x0564, 0x0063, 0xae064271, 0x954dc6d1},
	{0x0b14, 0x0044, 0xdb867d9b, 0xdf432309},
	{0x0e5a, 0x0055, 0xff06b685, 0xa65ff257},
	{0x015e, 0x00ba, 0x1115ccbc, 0x11c365f4},
	{0x0379, 0x00e6, 0x5f4e58dd, 0x2d176d31},
	{0x013b, 0x0067, 0x4897427e, 0xc40532fe},
	{0x0e64, 0x0071, 0x7af2b7a4, 0x1fb7bf43},
	{0x0a11, 0x0050, 0x92105726, 0xb1185e51},
	{0x0109, 0x0055, 0xd0d000f9, 0x60a60bfd},
	{0x00aa, 0x0022, 0x815d229d, 0x215d379c},
	{0x09ac, 0x004f, 0x02f9d985, 0x10b90b20},
	{0x0e1b, 0x00ce, 0x5cf92ab4, 0x6a477573},
	{0x08af, 0x00d8, 0x17ca72d1, 0x385af156},
	{0x0e33, 0x000a, 0xda2dba6b, 0xda2dbb69},
	{0x0ee3, 0x006a, 0xb00048e5, 0xa9a2decc},
	{0x0648, 0x001a, 0x2364b8cb, 0x3364b1cb},
	{0x0315, 0x0085, 0x0596fd0d, 0xa651740f},
	{0x0fbb, 0x003e, 0x298230ca, 0x7fc617c7},
	{0x0422, 0x006a, 0x78ada4ab, 0xc576ae2a},
	{0x04ba, 0x0073, 0xced1fbc2, 0xaac8455b},
	{0x007d, 0x0061, 0x4b7ff236, 0x347d5739},
	{0x070b, 0x00d0, 0x261cf0ae, 0xc7fb1c10},
	{0x0c1a, 0x0035, 0x8be92ee2, 0x8be9b4e1},
	{0x0af8, 0x0063, 0x824dcf03, 0x53010388},
	{0x08f8, 0x006d, 0xd289710c, 0x30418edd},
	{0x021b, 0x00ee, 0x6ac1c41d, 0x2557e9a3},
	{0x05b5, 0x00da, 0x8e52f0e2, 0x98531012},
};

/* Don't print anything to stdout. */
#define DAHASHTEST_QUIET	(1U << 0)

static int
dahash_test(
	unsigned int	flags)
{
	int		i;
	int		errors = 0;
	int		bytes = 0;
	struct xfs_name	xname = { };
	struct timeval	start, stop;
	uint64_t	usec;

	/* keep static to prevent cache warming code from
	 * getting eliminated by the compiler */
	static xfs_dahash_t	hash;

	/* pre-warm the cache */
	for (i = 0; i < ARRAY_SIZE(dahash_tests); i++) {
		bytes += 2 * dahash_tests[i].length;

		hash ^= libxfs_da_hashname(
				randbytes_test_buf + dahash_tests[i].start,
				dahash_tests[i].length);
	}

	gettimeofday(&start, NULL);
	for (i = 0; i < ARRAY_SIZE(dahash_tests); i++) {
		hash = libxfs_da_hashname(
				randbytes_test_buf + dahash_tests[i].start,
				dahash_tests[i].length);
		if (hash != dahash_tests[i].dahash)
			errors++;

		xname.name = randbytes_test_buf + dahash_tests[i].start;
		xname.len = dahash_tests[i].length;
		hash = libxfs_ascii_ci_hashname(&xname);
		if (hash != dahash_tests[i].ascii_ci_dahash)
			errors++;
	}
	gettimeofday(&stop, NULL);

	usec = stop.tv_usec - start.tv_usec +
		1000000 * (stop.tv_sec - start.tv_sec);

	if (flags & DAHASHTEST_QUIET)
		return errors;

	if (errors)
		printf("dahash: %d self tests failed\n", errors);
	else {
		printf("dahash: tests passed, %d bytes in %" PRIu64 " usec\n",
			bytes, usec);
	}

	return errors;
}

#endif /* __LIBFROG_DAHASHSELFTEST_H__ */
