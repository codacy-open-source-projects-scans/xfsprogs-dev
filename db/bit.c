// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include "libxfs.h"
#include "bit.h"

int
getbit_l(
	char	*ptr,
	int	bit)
{
	int	mask;
	int	shift;

	ptr += byteize(bit);
	bit = bitoffs(bit);
	shift = 7 - bit;
	mask = 1 << shift;
	return (*ptr & mask) >> shift;
}

void
setbit_l(
	char *ptr,
	int  bit,
	int  val)
{
	int	mask;
	int	shift;

	ptr += byteize(bit);
	bit = bitoffs(bit);
	shift = 7 - bit;
	mask = (1 << shift);
	if (val) {
		*ptr |= mask;
	} else {
		mask = ~mask;
		*ptr &= mask;
	}
}

int64_t
getbitval(
	void		*obj,
	int		bitoff,
	int		nbits,
	int		flags)
{
	int		bit;
	int		i;
	char		*p;
	int64_t		rval;
	int		signext;

	ASSERT(nbits<=64);

	p = (char *)obj + byteize(bitoff);
	bit = bitoffs(bitoff);
	signext = (flags & BVSIGNED) != 0;

	if (bit != 0)
		goto scrape_bits;

	switch (nbits) {
	case 64:
		return get_unaligned_be64(p);
	case 32:
		if (signext)
			return (__s32)get_unaligned_be32(p);
		return (__u32)get_unaligned_be32(p);
	case 16:
		if (signext)
			return (__s16)get_unaligned_be16(p);
		return (__u16)get_unaligned_be16(p);
	case 8:
		if (signext)
			return *(__s8 *)p;
		return *(__u8 *)p;
	}

scrape_bits:
	for (i = 0, rval = 0LL; i < nbits; i++) {
		if (getbit_l(p, bit + i)) {
			/* If the last bit is on and we care about sign
			 * bits and we don't have a full 64 bit
			 * container, turn all bits on between the
			 * sign bit and the most sig bit.
			 */

			/* handle endian swap here */
#if __BYTE_ORDER == LITTLE_ENDIAN
			if (i == 0 && signext && nbits < 64)
				rval = (~0ULL) << nbits;
			rval |= 1ULL << (nbits - i - 1);
#else
			if ((i == (nbits - 1)) && signext && nbits < 64)
				rval |= ((~0ULL) << nbits);
			rval |= 1ULL << (nbits - i - 1);
#endif
		}
	}
	return rval;
}

/*
 * The input data can be 8, 16, 32, and 64 sized numeric values
 * aligned on a byte boundry, or odd sized numbers stored on odd
 * aligned offset (for example the bmbt fields).
 *
 * The input data sent to this routine has been converted to big endian
 * and has been adjusted in the array so that the first input bit is to
 * be written in the first bit in the output.
 *
 * If the field length and the output buffer are byte aligned, then use
 * memcpy from the input to the output, but if either entries are not byte
 * aligned, then loop over the entire bit range reading the input value
 * and set/clear the matching bit in the output.
 *
 * example when ibuf is not multiple of a byte in length:
 *
 * ibuf:	| BBBBBBBB | bbbxxxxx |
 *		  \\\\\\\\--\\\\
 * obuf+bitoff:	| xBBBBBBB | Bbbbxxxx |
 *
 */
void
setbitval(
	void	*obuf,		/* start of buffer to write into */
	int	bitoff,		/* bit offset into the output buffer */
	int	nbits,		/* number of bits to write */
	void	*ibuf)		/* source bits */
{
	char	*in = ibuf;
	char	*out = obuf;
	int	bit;

	if (bitoff % NBBY || nbits % NBBY) {
		for (bit = 0; bit < nbits; bit++)
			setbit_l(out, bit + bitoff, getbit_l(in, bit));
	} else
		memcpy(out + byteize(bitoff), in, byteize(nbits));
}
