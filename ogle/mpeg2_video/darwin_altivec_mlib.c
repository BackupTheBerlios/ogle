/* Ogle - A video player
 * Copyright (C) 2001, Charles M. Hannum <root@ihack.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * AltiVec support written by Charles M. Hannum <root@ihack.net>, except
 * for the core IDCT routine published by Motorola.
 *
 * Notes:
 * 1) All AltiVec loads and stores are aligned.  Conveniently, the output
 *    area for the IDCT and motion comp functions is always aligned.
 *    However, the reference area is not; therefore, we check its
 *    alignment and use lvsl/vperm as necessary to align the reference
 *    data as it's loaded.
 * 2) Unfortunately, AltiVec doesn't do 8-byte loads and stores.  This
 *    means that the fastest paths are only applicable to the Y channel.
 *    For 8-byte operations on the U and V channels, there are two cases.
 *    When the alignment of the input and output are the same, we can do
 *    16-byte loads and just do two 4-byte stores.  For the unmatched
 *    alignment case, we have to do a rotation of the loaded data first.
 * 3) The `i[0-7]' variables look silly, but they prevent GCC from
 *    generating gratuitous multiplies, and allow the loaded constants
 *    to be recycled several times in the IDCT routine.
 * 4) The use of "b" constraints is *very* important.  Using r0 in any
 *    of the AltiVec load/store instructions is equivalent to a constant
 *    0.
 */

#include <inttypes.h>

#if 0
#define	ASSERT(x)	if (!(x)) abort()
#else
#define	ASSERT(x)
#endif

void
mlib_Init(void)
{
	asm("mtspr	0x100,%0" : : "b" (-1));
}

static inline void
mlib_VideoInterpAveXY_U8_U8(uint8_t *curr_block, 
                            const uint8_t *ref_block, 
                            const int width, const int height,
                            int32_t frame_stride,   
                            int32_t field_stride) 
{
	int x, y;
	const uint8_t *ref_block_next = ref_block + field_stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (curr_block[x] + 
			     ((ref_block[x] + ref_block_next[x] +
			       ref_block[x+1] + ref_block_next[x+1] + 2) >> 2) + 1) >> 1;
		curr_block     += frame_stride;
		ref_block      += frame_stride;
		ref_block_next += frame_stride;
	}
}

static inline void
mlib_VideoInterpXY_U8_U8(uint8_t *curr_block, 
			 const uint8_t *ref_block, 
			 const int width, const int height,
			 int32_t frame_stride,   
			 int32_t field_stride) 
{
	int x, y;
	const uint8_t *ref_block_next = ref_block + field_stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (ref_block[x] + ref_block_next[x] +
			     ref_block[x+1] + ref_block_next[x+1] + 2) >> 2;
		curr_block     += frame_stride;
		ref_block      += frame_stride;
		ref_block_next += frame_stride;
	}
}

void
mlib_VideoCopyRefAve_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v3,%0,%1\n"
                    "" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
                    "" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
                    "lvx v0,%1,%2\n"
                    "lvx v1,%1,%3\n"
                    "lvx v2,%0,%2\n"
                    "vperm v0,v0,v1,v3\n"
                    "vavgub v0,v0,v2\n"
                    "stvx v0,%0,%2\n"
                    "" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n" 
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
                    "" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2 \n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v3,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v3,%1,%2\n"
			"lvsl v4,%1,%3\n"
			"lvsr v5,%0,%2\n"
			"lvsr v6,%0,%3\n"
			"vperm v3,v3,v3,v5\n"
			"vperm v4,v4,v4,v6\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v4\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v4\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v4\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v4\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v3,%1,%2\n"
			"lvsl v4,%1,%3\n"
			"lvsr v5,%0,%2\n"
			"lvsr v6,%0,%3\n"
			"vperm v3,v3,v3,v5\n"
			"vperm v4,v4,v4,v6\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v4\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v3\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"lvx v2,%0,%2\n"
			"vperm v0,v0,v1,v4\n"
			"vavgub v0,v0,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%0,%2\n"
			"lvx v1,%1,%2\n"
			"vavgub v0,v0,v1\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void
mlib_VideoCopyRef_U8_U8_16x16_multiple(uint8_t *curr_block,
				       const uint8_t *ref_block,
				       int32_t stride,
				       int32_t count)
{
	ASSERT(((int)curr_block & 15) == 0);
	ASSERT(((int)ref_block & 15) == 0);
	
	while (count--) {
		int i0 = 0;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));

		curr_block += 16, ref_block += 16;
	}
}

void
mlib_VideoCopyRef_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);
	
	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v2,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRef_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v2,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvx v0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x8_multiple(uint8_t *curr_block,
				     uint8_t *ref_block,
				     int32_t stride,
				     int32_t count)
{
	ASSERT(((int)curr_block & 7) == 0);

	while (count--) {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));

		curr_block += 8, ref_block += 8;
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v2,%1,%2\n"
			"lvsl v3,%1,%3\n"
			"lvsr v4,%0,%2\n"
			"lvsr v5,%0,%3\n"
			"vperm v2,v2,v2,v4\n"
			"vperm v3,v3,v3,v5\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v3\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v3\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v3\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v3\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v2,%1,%2\n"
			"lvsl v3,%1,%3\n"
			"lvsr v4,%0,%2\n"
			"lvsr v5,%0,%3\n"
			"vperm v2,v2,v2,v4\n"
			"vperm v3,v3,v3,v5\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v3\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v0,v0,v1,v3\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx v0,%1,%2\n"
			"stvewx v0,%0,%2\n"
			"stvewx v0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void
mlib_VideoInterpAveX_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%0,%1\n"
		"vaddubs v3,v2,v0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v4,v0,v1,v2\n"
			"vperm v5,v0,v1,v3\n"
			"lvx v6,%0,%2\n"
			"vavgub v4,v4,v5\n"
			"vavgub v4,v4,v6\n"
			"stvx v4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%0,%1\n"
		"vaddubs v3,v2,v0\n"
	"" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v4,v0,v1,v2\n"
			"vperm v5,v0,v1,v3\n"
			"lvx v6,%0,%2\n"
			"vavgub v4,v4,v5\n"
			"vavgub v4,v4,v6\n"
			"stvx v4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%1,%2\n"
		"lvsr v3,%0,%2\n"
		"lvsl v4,%1,%3\n"
		"lvsr v5,%0,%3\n"
		"vperm v2,v2,v2,v3\n"
		"vperm v4,v4,v4,v5\n"
		"vaddubs v3,v2,v0\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 4; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v2\n"
			"vperm v7,v0,v1,v3\n"
			"lvx v8,%0,%2\n"
			"vavgub v6,v6,v7\n"
			"vavgub v6,v6,v8\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v4\n"
			"vperm v7,v0,v1,v5\n"
			"lvx v8,%0,%2\n"
			"vavgub v6,v6,v7\n"
			"vavgub v6,v6,v8\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%1,%2\n"
		"lvsr v3,%0,%2\n"
		"lvsl v4,%1,%3\n"
		"lvsr v5,%0,%3\n"
		"vperm v2,v2,v2,v3\n"
		"vperm v4,v4,v4,v5\n"
		"vaddubs v3,v2,v0\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 2; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v2\n"
			"vperm v7,v0,v1,v3\n"
			"lvx v8,%0,%2\n"
			"vavgub v6,v6,v7\n"
			"vavgub v6,v6,v8\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v4\n"
			"vperm v7,v0,v1,v5\n"
			"lvx v8,%0,%2\n"
			"vavgub v6,v6,v7\n"
			"vavgub v6,v6,v8\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void
mlib_VideoInterpAveY_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"lvx v2,%1,%4\n"
				"lvx v3,%2,%4\n"
				"vperm v0,v0,v2,v4\n"
				"vperm v1,v1,v3,v4\n"
				"lvx v2,%0,%3\n"
				"vavgub v0,v0,v1\n"
				"vavgub v0,v0,v2\n"
				"stvx v0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"lvx v2,%0,%3\n"
				"vavgub v0,v0,v1\n"
				"vavgub v0,v0,v2\n"
				"stvx v0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"lvx v2,%1,%4\n"
				"lvx v3,%2,%4\n"
				"vperm v0,v0,v2,v4\n"
				"vperm v1,v1,v3,v4\n"
				"lvx v2,%0,%3\n"
				"vavgub v0,v0,v1\n"
				"vavgub v0,v0,v2\n"
				"stvx v0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"lvx v2,%0,%3\n"
				"vavgub v0,v0,v1\n"
				"vavgub v0,v0,v2\n"
				"stvx v0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v4,%1,%3\n"
			"lvsl v5,%1,%4\n"
			"lvsl v6,%2,%3\n"
			"lvsl v7,%2,%4\n"
			"lvsr v8,%0,%3\n"
			"lvsr v9,%0,%4\n"
			"vperm v4,v4,v4,v8\n"
			"vperm v5,v5,v5,v9\n"
			"vperm v6,v6,v6,v8\n"
			"vperm v7,v7,v7,v9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v4\n"
				"vperm v9,v2,v3,v6\n"
				"lvx v10,%0,%3\n"
				"vavgub v8,v8,v9\n"
				"vavgub v8,v8,v10\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v5\n"
				"vperm v9,v2,v3,v7\n"
				"lvx v10,%0,%3\n"
				"vavgub v8,v8,v9\n"
				"vavgub v8,v8,v10\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"lvx v2,%0,%3\n"
				"vavgub v0,v0,v1\n"
				"vavgub v0,v0,v2\n"
				"stvewx v0,%0,%3\n"
				"stvewx v0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v4,%1,%3\n"
			"lvsl v5,%1,%4\n"
			"lvsl v6,%2,%3\n"
			"lvsl v7,%2,%4\n"
			"lvsr v8,%0,%3\n"
			"lvsr v9,%0,%4\n"
			"vperm v4,v4,v4,v8\n"
			"vperm v5,v5,v5,v9\n"
			"vperm v6,v6,v6,v8\n"
			"vperm v7,v7,v7,v9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 2; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v4\n"
				"vperm v9,v2,v3,v6\n"
				"lvx v10,%0,%3\n"
				"vavgub v8,v8,v9\n"
				"vavgub v8,v8,v10\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v5\n"
				"vperm v9,v2,v3,v7\n"
				"lvx v10,%0,%3\n"
				"vavgub v8,v8,v9\n"
				"vavgub v8,v8,v10\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"lvx v2,%0,%3\n"
				"vavgub v0,v0,v1\n"
				"vavgub v0,v0,v2\n"
				"stvewx v0,%0,%3\n"
				"stvewx v0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void
mlib_VideoInterpAveXY_U8_U8_16x16(uint8_t *curr_block,
				  const uint8_t *ref_block,
				  int32_t frame_stride,
				  int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v4,%0,%1\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx v0,%1,%3\n"
			"lvx v1,%2,%3\n"
			"lvx v2,%1,%4\n"
			"lvx v3,%2,%4\n"
			"vperm v6,v0,v2,v4\n"
			"vperm v7,v0,v2,v5\n"
			"vperm v8,v1,v3,v4\n"
			"vperm v9,v1,v3,v5\n"
			"vavgub v6,v6,v7\n"
			"vavgub v8,v8,v9\n"
			"lvx v10,%0,%3\n"
			"vavgub v6,v6,v8\n"
			"vavgub v6,v6,v10\n"
			"stvx v6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v4,%0,%1\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx v0,%1,%3\n"
			"lvx v1,%2,%3\n"
			"lvx v2,%1,%4\n"
			"lvx v3,%2,%4\n"
			"vperm v6,v0,v2,v4\n"
			"vperm v7,v0,v2,v5\n"
			"vperm v8,v1,v3,v4\n"
			"vperm v9,v1,v3,v5\n"
			"vavgub v6,v6,v7\n"
			"vavgub v8,v8,v9\n"
			"lvx v10,%0,%3\n"
			"vavgub v6,v6,v8\n"
			"vavgub v6,v6,v10\n"
			"stvx v6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveXY_U8_U8_8x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x4(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpX_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%0,%1\n"
		"vaddubs v3,v2,v0\n"
	"" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v4,v0,v1,v2\n"
			"vperm v5,v0,v1,v3\n"
			"vavgub v4,v4,v5\n"
			"stvx v4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%0,%1\n"
		"vaddubs v3,v2,v0\n"
	"" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v4,v0,v1,v2\n"
			"vperm v5,v0,v1,v3\n"
			"vavgub v4,v4,v5\n"
			"stvx v4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%1,%2\n"
		"lvsr v3,%0,%2\n"
		"lvsl v4,%1,%3\n"
		"lvsr v5,%0,%3\n"
		"vperm v2,v2,v2,v3\n"
		"vperm v4,v4,v4,v5\n"
		"vaddubs v3,v2,v0\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 4; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v2\n"
			"vperm v7,v0,v1,v3\n"
			"vavgub v6,v6,v7\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v4\n"
			"vperm v7,v0,v1,v5\n"
			"vavgub v6,v6,v7\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v2,%1,%2\n"
		"lvsr v3,%0,%2\n"
		"lvsl v4,%1,%3\n"
		"lvsr v5,%0,%3\n"
		"vperm v2,v2,v2,v3\n"
		"vperm v4,v4,v4,v5\n"
		"vaddubs v3,v2,v0\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 2; i++) {
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v2\n"
			"vperm v7,v0,v1,v3\n"
			"vavgub v6,v6,v7\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx v0,%1,%2\n"
			"lvx v1,%1,%3\n"
			"vperm v6,v0,v1,v4\n"
			"vperm v7,v0,v1,v5\n"
			"vavgub v6,v6,v7\n"
			"stvewx v6,%0,%2\n"
			"stvewx v6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void
mlib_VideoInterpY_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v5,v0,v1,v4\n"
				"vperm v6,v2,v3,v4\n"
				"vavgub v5,v5,v6\n"
				"stvx v5,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"vavgub v0,v0,v1\n"
				"stvx v0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl v4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v5,v0,v1,v4\n"
				"vperm v6,v2,v3,v4\n"
				"vavgub v5,v5,v6\n"
				"stvx v5,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"vavgub v0,v0,v1\n"
				"stvx v0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v4,%1,%3\n"
			"lvsl v5,%1,%4\n"
			"lvsl v6,%2,%3\n"
			"lvsl v7,%2,%4\n"
			"lvsr v8,%0,%3\n"
			"lvsr v9,%0,%4\n"
			"vperm v4,v4,v4,v8\n"
			"vperm v5,v5,v5,v9\n"
			"vperm v6,v6,v6,v8\n"
			"vperm v7,v7,v7,v9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v4\n"
				"vperm v9,v2,v3,v6\n"
				"vavgub v8,v8,v9\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v5\n"
				"vperm v9,v2,v3,v7\n"
				"vavgub v8,v8,v9\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"vavgub v0,v0,v1\n"
				"stvewx v0,%0,%3\n"
				"stvewx v0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl v4,%1,%3\n"
			"lvsl v5,%1,%4\n"
			"lvsl v6,%2,%3\n"
			"lvsl v7,%2,%4\n"
			"lvsr v8,%0,%3\n"
			"lvsr v9,%0,%4\n"
			"vperm v4,v4,v4,v8\n"
			"vperm v5,v5,v5,v9\n"
			"vperm v6,v6,v6,v8\n"
			"vperm v7,v7,v7,v9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 2; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v4\n"
				"vperm v9,v2,v3,v6\n"
				"vavgub v8,v8,v9\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%1,%4\n"
				"lvx v2,%2,%3\n"
				"lvx v3,%2,%4\n"
				"vperm v8,v0,v1,v5\n"
				"vperm v9,v2,v3,v7\n"
				"vavgub v8,v8,v9\n"
				"stvewx v8,%0,%3\n"
				"stvewx v8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx v0,%1,%3\n"
				"lvx v1,%2,%3\n"
				"vavgub v0,v0,v1\n"
				"stvewx v0,%0,%3\n"
				"stvewx v0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void
mlib_VideoInterpXY_U8_U8_16x16(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v4,%0,%1\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx v0,%1,%3\n"
			"lvx v1,%2,%3\n"
			"lvx v2,%1,%4\n"
			"lvx v3,%2,%4\n"
			"vperm v6,v0,v2,v4\n"
			"vperm v7,v0,v2,v5\n"
			"vperm v8,v1,v3,v4\n"
			"vperm v9,v1,v3,v5\n"
			"vavgub v6,v6,v7\n"
			"vavgub v8,v8,v9\n"
			"vavgub v6,v6,v8\n"
			"stvx v6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpXY_U8_U8_16x8(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb v0,1\n"
		"lvsl v4,%0,%1\n"
		"vaddubs v5,v4,v0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx v0,%1,%3\n"
			"lvx v1,%2,%3\n"
			"lvx v2,%1,%4\n"
			"lvx v3,%2,%4\n"
			"vperm v6,v0,v2,v4\n"
			"vperm v7,v0,v2,v5\n"
			"vperm v8,v1,v3,v4\n"
			"vperm v9,v1,v3,v5\n"
			"vavgub v6,v6,v7\n"
			"vavgub v8,v8,v9\n"
			"vavgub v6,v6,v8\n"
			"stvx v6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpXY_U8_U8_8x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x4(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void mlib_ClearCoeffs(int16_t *coeffs)
{
	asm(""
		"vspltish v0,0\n"
		"stvx v0,%0,%1\n"
		"addi %1,%1,32\n"
		"stvx v0,%0,%2\n"
		"addi %2,%2,32\n"
		"stvx v0,%0,%1\n"
		"addi %1,%1,32\n"
		"stvx v0,%0,%2\n"
		"addi %2,%2,32\n"
		"stvx v0,%0,%1\n"
		"addi %1,%1,32\n"
		"stvx v0,%0,%2\n"
		"addi %2,%2,32\n"
		"stvx v0,%0,%1\n"
		"stvx v0,%0,%2\n"
	"" : : "b" (coeffs), "b" (0), "b" (16));
}


/***************************************************************
 *
 * Copyright:   (c) Copyright Motorola Inc. 1998
 *
 * Date:        April 17, 1998
 *
 * Function:    IDCT
 *
 * Description: Scaled Chen (III) algorithm for IDCT
 *              Arithmetic is 16-bit fixed point.
 *
 * Inputs:      input - Pointer to input data (short), which
 *                      must be between -2048 to +2047.
 *                      It is assumed that the allocated array
 *                      has been 128-bit aligned and contains
 *                      8x8 short elements.
 *
 * Outputs:     output - Pointer to output area for the transfored
 *                       data. The output values are between -255
 *                       and 255 . It is assumed that a 128-bit
 *                       aligned 8x8 array of short has been
 *                       pre-allocated.
 *
 * Return:      None
 *
 ***************************************************************/

static const int16_t SpecialConstants[8] __attribute__ ((aligned (16))) = {
	23170, 13573, 6518, 21895, -23170, -21895, 32, 0 };

static const int16_t PreScale[64] __attribute__ ((aligned (16))) = {
	16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725, 
	22725, 31521, 29692, 26722, 22725, 26722, 29692, 31521, 
	21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692, 
	19266, 26722, 25172, 22654, 19266, 22654, 25172, 26722, 
};

void mlib_VideoIDCTAdd_U8_S16(uint8_t *output, const int16_t *input, int32_t stride) 
{
	ASSERT(((int)output & 7) == 0);

	/* Load constants, input data, and prescale factors.  Do prescaling. */
	asm(""
		"vspltish        v31,0\n"
		"lvx		v24,0,%1\n"
		"vspltish	v23,4\n"
\
		"addi		r5,0,0\n"
		"vsplth		v29,v24,4\n"
		"lvx		v0,%0,r5\n"
		"addi		r6,0,16\n"
		"vsplth		v28,v24,3\n"
		"lvx		v16,%2,r5\n"
		"addi		r7,0,32\n"
		"vsplth		v27,v24,2\n"
		"lvx		v1,%0,r6\n"
		"addi		r8,0,48\n"
		"vsplth		v26,v24,1\n"
		"lvx		v17,%2,r6\n"
		"addi		r5,0,64\n"
		"vsplth		v25,v24,0\n"
		"lvx		v2,%0,r7\n"
		"addi		r6,0,80\n"
		"vslh		v0,v0,v23\n"
		"lvx		v18,%2,r7\n"
		"addi		r7,0,96\n"
		"vslh		v1,v1,v23\n"
		"lvx		v3,%0,r8\n"
		"vslh		v2,v2,v23\n"
		"lvx		v19,%2,r8\n"
		"addi		r8,0,112\n"
		"vslh		v3,v3,v23\n"
		"lvx		v4,%0,r5\n"
		"vsplth		v30,v24,5\n"
		"lvx		v5,%0,r6\n"
		"vsplth		v24,v24,6\n"
		"lvx		v6,%0,r7\n"
		"vslh		v4,v4,v23\n"
		"lvx		v7,%0,r8\n"
		"vslh		v5,v5,v23\n"
		"vmhraddshs	v0,v0,v16,v31\n"
		"vslh		v6,v6,v23\n"
		"vmhraddshs	v4,v4,v16,v31\n"
		"vslh		v7,v7,v23\n"
	"" : : "b" (input), "b" (SpecialConstants), "b" (PreScale)
	  : "cc", "r5", "r6", "r7", "r8", "memory");

	asm(""
		"vmhraddshs	v1,v1,v17,v31\n"
		"vmhraddshs	v5,v5,v19,v31\n"
		"vmhraddshs	v2,v2,v18,v31\n"
		"vmhraddshs	v6,v6,v18,v31\n"
		"vmhraddshs	v3,v3,v19,v31\n"
		"vmhraddshs	v7,v7,v17,v31\n"
\
\
		"vmhraddshs      v11,v27,v7,v1\n"
		"vmhraddshs      v19,v27,v1,v31\n"
		"vmhraddshs      v12,v26,v6,v2\n"
		"vmhraddshs      v13,v30,v3,v5\n"
		"vmhraddshs      v17,v28,v5,v3\n"
		"vsubshs		v18,v19,v7\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      v19,v26,v2,v31\n"
		"vaddshs		v15,v0,v4\n"
		"vsubshs		v10,v0,v4\n"
		"vsubshs		v14,v19,v6\n"
		"vaddshs		v16,v18,v13\n"
		"vsubshs		v13,v18,v13\n"
		"vsubshs		v18,v11,v17\n"
		"vaddshs		v11,v11,v17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		v17,v15,v12\n"
		"vsubshs		v12,v15,v12\n"
		"vaddshs		v15,v10,v14\n"
		"vsubshs		v10,v10,v14\n"
		"vsubshs		v14,v18,v13\n"
		"vaddshs		v13,v18,v13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs      v2,v25,v14,v10\n"
		"vsubshs		v4,v12,v16\n"
		"vmhraddshs      v1,v25,v13,v15\n"
		"vaddshs		v0,v17,v11\n"
		"vmhraddshs      v5,v29,v14,v10\n"
		"vmrghh  v8,v0,v4\n"
		"vaddshs		v3,v12,v16\n"
		"vmrglh  v9,v0,v4\n"
		"vmhraddshs      v6,v29,v13,v15\n"
		"vmrghh  v10,v1,v5\n"
		"vsubshs		v7,v17,v11\n"
	"");

	/* Transpose the matrix again. */
	asm(""
		"vmrglh  v11,v1,v5\n"
		"vmrghh  v12,v2,v6\n"
		"vmrglh  v13,v2,v6\n"
		"vmrghh  v14,v3,v7\n"
		"vmrglh  v15,v3,v7\n"
\
		"vmrghh  v16,v8,v12\n"
		"vmrglh  v17,v8,v12\n"
		"vmrghh  v18,v9,v13\n"
		"vmrglh  v19,v9,v13\n"
		"vmrghh  v20,v10,v14\n"
		"vmrglh  v21,v10,v14\n"
		"vmrghh  v22,v11,v15\n"
		"vmrglh  v23,v11,v15\n"
\
		"vmrglh  v1,v16,v20\n"
		"vmrglh  v7,v19,v23\n"
		"vmrglh  v3,v17,v21\n"
		"vmrghh  v2,v17,v21\n"
		"vmhraddshs      v11,v27,v7,v1\n"
		"vmrghh  v6,v19,v23\n"
		"vmhraddshs      v19,v27,v1,v31\n"
		"vmrglh  v5,v18,v22\n"
		"vmhraddshs      v12,v26,v6,v2\n"
		"vmrghh  v0,v16,v20\n"
		"vmhraddshs      v13,v30,v3,v5\n"
		"vmrghh  v4,v18,v22\n"
		"vmhraddshs      v17,v28,v5,v3\n"
		"vsubshs		v18,v19,v7\n"
	"");

	/* Add a rounding bias for the final shift.  v0 is added into every
	   vector, so the bias propagates from here. */
	asm(""
		"vaddshs	v0,v0,v24\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      v19,v26,v2,v31\n"
		"vaddshs		v15,v0,v4\n"
		"vsubshs		v10,v0,v4\n"
		"vsubshs		v14,v19,v6\n"
		"vaddshs		v16,v18,v13\n"
		"vsubshs		v13,v18,v13\n"
		"vsubshs		v18,v11,v17\n"
		"vaddshs		v11,v11,v17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		v17,v15,v12\n"
		"vsubshs		v12,v15,v12\n"
		"vaddshs		v15,v10,v14\n"
		"vsubshs		v10,v10,v14\n"
		"vsubshs		v14,v18,v13\n"
		"vaddshs		v13,v18,v13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs	v2,v25,v14,v10\n"
		"vsubshs		v4,v12,v16\n"
		"vmhraddshs	v1,v25,v13,v15\n"
		"vaddshs		v0,v17,v11\n"
		"vmhraddshs	v5,v29,v14,v10\n"
		"vaddshs		v3,v12,v16\n"
		"vspltish	v23,6\n"
		"vmhraddshs	v6,v29,v13,v15\n"
		"vsubshs		v7,v17,v11\n"
	"");

	/* Load and permutations for the reference data we're adding to. */
	asm(""
		"lvsl		v17,%0,%1\n"
		"vspltisb	v19,-1\n"
		"lvsl		v18,%0,%2\n"
		"vmrghb		v17,v19,v17\n"
		"vmrghb		v18,v19,v18\n"
	"" : : "b" (output), "b" (0), "b" (0+stride));

	/* Copy out each 8 values, adding to the existing frame. */
	asm(""
		"vsrah		v0,v0,v23\n"
		"lvx		v8,%0,%1\n"
		"vsrah		v1,v1,v23\n"
		"lvx		v9,%0,%3\n"
		"vperm		v8,v8,v31,v17\n"
		"vperm		v9,v9,v31,v18\n"
		"vaddshs		v0,v0,v8\n"
		"vaddshs		v1,v1,v9\n"
		"vmaxsh		v0,v0,v31\n"
		"vmaxsh		v1,v1,v31\n"
		"vpkuhus		v0,v0,v0\n"
		"stvewx		v0,%0,%1\n"
		"vpkuhus		v1,v1,v1\n"
		"stvewx		v0,%0,%2\n"
		"stvewx		v1,%0,%3\n"
		"stvewx		v1,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		v2,v2,v23\n"
		"lvx		v10,%0,%1\n"
		"vsrah		v3,v3,v23\n"
		"lvx		v11,%0,%3\n"
		"vperm		v10,v10,v31,v17\n"
		"vperm		v11,v11,v31,v18\n"
		"vaddshs		v2,v2,v10\n"
		"vaddshs		v3,v3,v11\n"
		"vmaxsh		v2,v2,v31\n"
		"vmaxsh		v3,v3,v31\n"
		"vpkuhus		v2,v2,v2\n"
		"stvewx		v2,%0,%1\n"
		"vpkuhus		v3,v3,v3\n"
		"stvewx		v2,%0,%2\n"
		"stvewx		v3,%0,%3\n"
		"stvewx		v3,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		v4,v4,v23\n"
		"lvx		v12,%0,%1\n"
		"vsrah		v5,v5,v23\n"
		"lvx		v13,%0,%3\n"
		"vperm		v12,v12,v31,v17\n"
		"vperm		v13,v13,v31,v18\n"
		"vaddshs		v4,v4,v12\n"
		"vaddshs		v5,v5,v13\n"
		"vmaxsh		v4,v4,v31\n"
		"vmaxsh		v5,v5,v31\n"
		"vpkuhus		v4,v4,v4\n"
		"stvewx		v4,%0,%1\n"
		"vpkuhus		v5,v5,v5\n"
		"stvewx		v4,%0,%2\n"
		"stvewx		v5,%0,%3\n"
		"stvewx		v5,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		v6,v6,v23\n"
		"lvx		v14,%0,%1\n"
		"vsrah		v7,v7,v23\n"
		"lvx		v15,%0,%3\n"
		"vperm		v14,v14,v31,v17\n"
		"vperm		v15,v15,v31,v18\n"
		"vaddshs		v6,v6,v14\n"
		"vaddshs		v7,v7,v15\n"
		"vmaxsh		v6,v6,v31\n"
		"vmaxsh		v7,v7,v31\n"
		"vpkuhus		v6,v6,v6\n"
		"stvewx		v6,%0,%1\n"
		"vpkuhus		v7,v7,v7\n"
		"stvewx		v6,%0,%2\n"
		"stvewx		v7,%0,%3\n"
		"stvewx		v7,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
}  

void mlib_VideoIDCT8x8_U8_S16(uint8_t *output, const int16_t *input, int32_t stride)
{
	ASSERT(((int)output & 7) == 0);

	/* Load constants, input data, and prescale factors.  Do prescaling. */
	asm(""
		"vspltish        v31,0\n"
		"lvx		v24,0,%1\n"
		"vspltish	v23,4\n"
\
		"addi		r5,0,0\n"
		"vsplth		v29,v24,4\n"
		"lvx		v0,%0,r5\n"
		"addi		r6,0,16\n"
		"vsplth		v28,v24,3\n"
		"lvx		v16,%2,r5\n"
		"addi		r7,0,32\n"
		"vsplth		v27,v24,2\n"
		"lvx		v1,%0,r6\n"
		"addi		r8,0,48\n"
		"vsplth		v26,v24,1\n"
		"lvx		v17,%2,r6\n"
		"addi		r5,0,64\n"
		"vsplth		v25,v24,0\n"
		"lvx		v2,%0,r7\n"
		"addi		r6,0,80\n"
		"vslh		v0,v0,v23\n"
		"lvx		v18,%2,r7\n"
		"addi		r7,0,96\n"
		"vslh		v1,v1,v23\n"
		"lvx		v3,%0,r8\n"
		"vslh		v2,v2,v23\n"
		"lvx		v19,%2,r8\n"
		"addi		r8,0,112\n"
		"vslh		v3,v3,v23\n"
		"lvx		v4,%0,r5\n"
		"vsplth		v30,v24,5\n"
		"lvx		v5,%0,r6\n"
		"vsplth		v24,v24,6\n"
		"lvx		v6,%0,r7\n"
		"vslh		v4,v4,v23\n"
		"lvx		v7,%0,r8\n"
		"vslh		v5,v5,v23\n"
		"vmhraddshs	v0,v0,v16,v31\n"
		"vslh		v6,v6,v23\n"
		"vmhraddshs	v4,v4,v16,v31\n"
		"vslh		v7,v7,v23\n"
	"" : : "b" (input), "b" (SpecialConstants), "b" (PreScale)
	  : "cc", "r5", "r6", "r7", "r8", "memory");

	asm(""
		"vmhraddshs	v1,v1,v17,v31\n"
		"vmhraddshs	v5,v5,v19,v31\n"
		"vmhraddshs	v2,v2,v18,v31\n"
		"vmhraddshs	v6,v6,v18,v31\n"
		"vmhraddshs	v3,v3,v19,v31\n"
		"vmhraddshs	v7,v7,v17,v31\n"
\
		"vmhraddshs      v11,v27,v7,v1\n"
		"vmhraddshs      v19,v27,v1,v31\n"
		"vmhraddshs      v12,v26,v6,v2\n"
		"vmhraddshs      v13,v30,v3,v5\n"
		"vmhraddshs      v17,v28,v5,v3\n"
		"vsubshs		v18,v19,v7\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      v19,v26,v2,v31\n"
		"vaddshs		v15,v0,v4\n"
		"vsubshs		v10,v0,v4\n"
		"vsubshs		v14,v19,v6\n"
		"vaddshs		v16,v18,v13\n"
		"vsubshs		v13,v18,v13\n"
		"vsubshs		v18,v11,v17\n"
		"vaddshs		v11,v11,v17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		v17,v15,v12\n"
		"vsubshs		v12,v15,v12\n"
		"vaddshs		v15,v10,v14\n"
		"vsubshs		v10,v10,v14\n"
		"vsubshs		v14,v18,v13\n"
		"vaddshs		v13,v18,v13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs      v2,v25,v14,v10\n"
		"vsubshs		v4,v12,v16\n"
		"vmhraddshs      v1,v25,v13,v15\n"
		"vaddshs		v0,v17,v11\n"
		"vmhraddshs      v5,v29,v14,v10\n"
		"vmrghh  v8,v0,v4\n"
		"vaddshs		v3,v12,v16\n"
		"vmrglh  v9,v0,v4\n"
		"vmhraddshs      v6,v29,v13,v15\n"
		"vmrghh  v10,v1,v5\n"
		"vsubshs		v7,v17,v11\n"
	"");

	/* Transpose the matrix again. */
	asm(""
		"vmrglh  v11,v1,v5\n"
		"vmrghh  v12,v2,v6\n"
		"vmrglh  v13,v2,v6\n"
		"vmrghh  v14,v3,v7\n"
		"vmrglh  v15,v3,v7\n"
\
		"vmrghh  v16,v8,v12\n"
		"vmrglh  v17,v8,v12\n"
		"vmrghh  v18,v9,v13\n"
		"vmrglh  v19,v9,v13\n"
		"vmrghh  v20,v10,v14\n"
		"vmrglh  v21,v10,v14\n"
		"vmrghh  v22,v11,v15\n"
		"vmrglh  v23,v11,v15\n"
\
		"vmrglh  v1,v16,v20\n"
		"vmrglh  v7,v19,v23\n"
		"vmrglh  v3,v17,v21\n"
		"vmrghh  v2,v17,v21\n"
		"vmhraddshs      v11,v27,v7,v1\n"
		"vmrghh  v6,v19,v23\n"
		"vmhraddshs      v19,v27,v1,v31\n"
		"vmrglh  v5,v18,v22\n"
		"vmhraddshs      v12,v26,v6,v2\n"
		"vmrghh  v0,v16,v20\n"
		"vmhraddshs      v13,v30,v3,v5\n"
		"vmrghh  v4,v18,v22\n"
		"vmhraddshs      v17,v28,v5,v3\n"
		"vsubshs		v18,v19,v7\n"
	"");

	/* Add a rounding bias for the final shift.  v0 is added into every
	   vector, so the bias propagates from here. */
	asm(""
		"vaddshs	v0,v0,v24\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      v19,v26,v2,v31\n"
		"vaddshs		v15,v0,v4\n"
		"vsubshs		v10,v0,v4\n"
		"vsubshs		v14,v19,v6\n"
		"vaddshs		v16,v18,v13\n"
		"vsubshs		v13,v18,v13\n"
		"vsubshs		v18,v11,v17\n"
		"vaddshs		v11,v11,v17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		v17,v15,v12\n"
		"vsubshs		v12,v15,v12\n"
		"vaddshs		v15,v10,v14\n"
		"vsubshs		v10,v10,v14\n"
		"vsubshs		v14,v18,v13\n"
		"vaddshs		v13,v18,v13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs	v2,v25,v14,v10\n"
		"vsubshs		v4,v12,v16\n"
		"vmhraddshs	v1,v25,v13,v15\n"
		"vaddshs		v0,v17,v11\n"
		"vmhraddshs	v5,v29,v14,v10\n"
		"vaddshs		v3,v12,v16\n"
		"vspltish	v23,6\n"
		"vmhraddshs	v6,v29,v13,v15\n"
		"vsubshs		v7,v17,v11\n"
	"");

	/* Copy out each 8 values. */
	asm(""
		"vmaxsh		v0,v0,v31\n"
		"vmaxsh		v1,v1,v31\n"
		"vsrah		v0,v0,v23\n"
		"vsrah		v1,v1,v23\n"
		"vpkuhus		v0,v0,v0\n"
		"vmaxsh		v2,v2,v31\n"
		"stvewx		v0,%0,%1\n"
		"vpkuhus		v1,v1,v1\n"
		"vmaxsh		v3,v3,v31\n"
		"stvewx		v0,%0,%2\n"
		"stvewx		v1,%0,%3\n"
		"stvewx		v1,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		v2,v2,v23\n"
		"vsrah		v3,v3,v23\n"
		"vpkuhus		v2,v2,v2\n"
		"vmaxsh		v4,v4,v31\n"
		"stvewx		v2,%0,%1\n"
		"vpkuhus		v3,v3,v3\n"
		"vmaxsh		v5,v5,v31\n"
		"stvewx		v2,%0,%2\n"
		"stvewx		v3,%0,%3\n"
		"stvewx		v3,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		v4,v4,v23\n"
		"vsrah		v5,v5,v23\n"
		"vpkuhus		v4,v4,v4\n"
		"vmaxsh		v6,v6,v31\n"
		"stvewx		v4,%0,%1\n"
		"vpkuhus		v5,v5,v5\n"
		"vmaxsh		v7,v7,v31\n"
		"stvewx		v4,%0,%2\n"
		"stvewx		v5,%0,%3\n"
		"stvewx		v5,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		v6,v6,v23\n"
		"vsrah		v7,v7,v23\n"
		"vpkuhus		v6,v6,v6\n"
		"stvewx		v6,%0,%1\n"
		"vpkuhus		v7,v7,v7\n"
		"stvewx		v6,%0,%2\n"
		"stvewx		v7,%0,%3\n"
		"stvewx		v7,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
}
