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
 *    U and V operations have to use 4-byte operations with lvewx/stvewx.
 *    Also unfortunately, these instructions have extremely weird
 *    alignment behavior, so they're currently only used in the special
 *    case where the regions have the same modulo 16 alignment (which
 *    happens to occur a lot).
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

static inline void
mlib_VideoCopyRefAve_U8_U8 (uint8_t *curr_block,
			    uint8_t *ref_block,
                            int width, int height,
			    int32_t stride)
{
	int x, y;
 
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (curr_block[x] + ref_block[x] + 1) >> 1;
		ref_block += stride;
		curr_block += stride;
	}
}

static inline void
mlib_VideoCopyRef_U8_U8 (uint8_t *curr_block,
                         uint8_t *ref_block,
                         int32_t width,
                         int32_t height,
                         int32_t stride)
{
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] = ref_block[x];
		ref_block += stride;
		curr_block += stride;
	}
}

static inline void
mlib_VideoInterpAveX_U8_U8(uint8_t *curr_block, 
                           uint8_t *ref_block,
                           int width, int height,
                           int32_t frame_stride,   
                           int32_t field_stride) 
{
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (curr_block[x] +
			     ((ref_block[x] + ref_block[x+1] + 1) >> 1) + 1) >> 1;
		ref_block += frame_stride;
		curr_block += frame_stride;
	}
}

static inline void
mlib_VideoInterpAveY_U8_U8(uint8_t *curr_block, 
                           uint8_t *ref_block,
                           int width, int height,
                           int32_t frame_stride,   
                           int32_t field_stride) 
{
	int x, y;
	uint8_t *ref_block_next = ref_block + field_stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (curr_block[x] + 
			     ((ref_block[x] + ref_block_next[x] + 1) >> 1) + 1) >> 1;
		curr_block     += frame_stride;
		ref_block      += frame_stride;
		ref_block_next += frame_stride;
	}
}

static inline void
mlib_VideoInterpAveXY_U8_U8(uint8_t *curr_block, 
                            uint8_t *ref_block, 
                            int width, int height,
                            int32_t frame_stride,   
                            int32_t field_stride) 
{
	int x, y;
	uint8_t *ref_block_next = ref_block + field_stride;

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
mlib_VideoInterpX_U8_U8(uint8_t *curr_block, 
			uint8_t *ref_block,
			int width, int height,
			int32_t frame_stride,   
			int32_t field_stride) 
{
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (ref_block[x] + ref_block[x+1] + 1) >> 1;
		ref_block += frame_stride;
		curr_block += frame_stride;
	}
}

static inline void
mlib_VideoInterpY_U8_U8(uint8_t *curr_block, 
			uint8_t *ref_block,
			int width, int height,
			int32_t frame_stride,   
			int32_t field_stride) 
{
	int x, y;
	uint8_t *ref_block_next = ref_block + field_stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			curr_block[x] =
			    (ref_block[x] + ref_block_next[x] + 1) >> 1;
		curr_block     += frame_stride;
		ref_block      += frame_stride;
		ref_block_next += frame_stride;
	}
}

static inline void
mlib_VideoInterpXY_U8_U8(uint8_t *curr_block, 
			 uint8_t *ref_block, 
			 int width, int height,
			 int32_t frame_stride,   
			 int32_t field_stride) 
{
	int x, y;
	uint8_t *ref_block_next = ref_block + field_stride;

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
                                 uint8_t *ref_block,
                                 int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 3,%0,%1
		" : : "b" (ref_block), "b" (i0));
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_16x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 3,%0,%1
		" : : "b" (ref_block), "b" (i0));
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			lvx 2,%0,%2
			vperm 0,0,1,3
			vavgub 0,0,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%0,%2
			lvx 1,%1,%2
			vavgub 0,0,1
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_8x8(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t stride)
{
	ASSERT(((int)curr_block & 3) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0)
		mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 8, 8, stride);
	else {
		int i0 = 0, i1 = 4;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_8x4(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t stride)
{
	ASSERT(((int)curr_block & 3) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0)
		mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 8, 4, stride);
	else {
		int i0 = 0, i1 = 4;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%0,%2
			lvewx 1,%0,%3
			lvewx 2,%1,%2
			lvewx 3,%1,%3
			vavgub 0,0,2
			vavgub 1,1,3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void
mlib_VideoCopyRef_U8_U8_16x16(uint8_t *curr_block,
			      uint8_t *ref_block,
			      int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);
	
	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 2,%0,%1
		" : : "b" (ref_block), "b" (i0));
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRef_U8_U8_16x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 2,%0,%1
		" : : "b" (ref_block), "b" (i0));
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 0,0,1,2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm("
			lvx 0,%1,%2
			stvx 0,%0,%2
		" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x8(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t stride)
{
	ASSERT(((int)curr_block & 3) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0)
		mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 8, 8, stride);
	else {
		int i0 = 0, i1 = 4;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x4(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t stride)
{
	ASSERT(((int)curr_block & 3) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0)
		mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 8, 4, stride);
	else {
		int i0 = 0, i1 = 4;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm("
			lvewx 0,%1,%2
			lvewx 1,%1,%3
			stvewx 0,%0,%2
			stvewx 1,%0,%3
		" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void
mlib_VideoInterpAveX_U8_U8_16x16(uint8_t *curr_block,
                                 uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 2,%0,%1
		vaddubs 3,2,0
	" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 4,0,1,2
			vperm 5,0,1,3
			lvx 6,%0,%2
			vavgub 4,4,5
			vavgub 4,4,6
			stvx 4,%0,%2
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 2,%0,%1
		vaddubs 3,2,0
	" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 8; i++) {
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 4,0,1,2
			vperm 5,0,1,3
			lvx 6,%0,%2
			vavgub 4,4,5
			vavgub 4,4,6
			stvx 4,%0,%2
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_8x8(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x4(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpAveY_U8_U8_16x16(uint8_t *curr_block,
                                 uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 4,%0,%1
		" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 16; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%2,%3
				lvx 2,%1,%4
				lvx 3,%2,%4
				vperm 0,0,2,4
				vperm 1,1,3,4
				lvx 2,%0,%3
				vavgub 0,0,1
				vavgub 0,0,2
				stvx 0,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 16; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%2,%3
				lvx 2,%0,%3
				vavgub 0,0,1
				vavgub 0,0,2
				stvx 0,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 4,%0,%1
		" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 8; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%2,%3
				lvx 2,%1,%4
				lvx 3,%2,%4
				vperm 0,0,2,4
				vperm 1,1,3,4
				lvx 2,%0,%3
				vavgub 0,0,1
				vavgub 0,0,2
				stvx 0,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 8; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%2,%3
				lvx 2,%0,%3
				vavgub 0,0,1
				vavgub 0,0,2
				stvx 0,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_8x8(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x4(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpAveXY_U8_U8_16x16(uint8_t *curr_block,
				  uint8_t *ref_block,
				  int32_t frame_stride,
				  int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 4,%0,%1
		vaddubs 5,4,0
	" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm("
			lvx 0,%1,%3
			lvx 1,%2,%3
			lvx 2,%1,%4
			lvx 3,%2,%4
			vperm 6,0,2,4
			vperm 7,0,2,5
			vperm 8,1,3,4
			vperm 9,1,3,5
			vavgub 6,6,7
			vavgub 8,8,9
			lvx 10,%0,%3
			vavgub 6,6,8
			vavgub 6,6,10
			stvx 6,%0,%3
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(uint8_t *curr_block,
                                 uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 4,%0,%1
		vaddubs 5,4,0
	" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 8; i++) {
		asm("
			lvx 0,%1,%3
			lvx 1,%2,%3
			lvx 2,%1,%4
			lvx 3,%2,%4
			vperm 6,0,2,4
			vperm 7,0,2,5
			vperm 8,1,3,4
			vperm 9,1,3,5
			vavgub 6,6,7
			vavgub 8,8,9
			lvx 10,%0,%3
			vavgub 6,6,8
			vavgub 6,6,10
			stvx 6,%0,%3
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveXY_U8_U8_8x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x4(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpX_U8_U8_16x16(uint8_t *curr_block,
			      uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 2,%0,%1
		vaddubs 3,2,0
	" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 16; i++) {
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 4,0,1,2
			vperm 5,0,1,3
			vavgub 4,4,5
			stvx 4,%0,%2
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_16x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 2,%0,%1
		vaddubs 3,2,0
	" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 8; i++) {
		asm("
			lvx 0,%1,%2
			lvx 1,%1,%3
			vperm 4,0,1,2
			vperm 5,0,1,3
			vavgub 4,4,5
			stvx 4,%0,%2
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_8x8(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpX_U8_U8_8x4(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpY_U8_U8_16x16(uint8_t *curr_block,
			      uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 4,%0,%1
		" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 16; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%1,%4
				lvx 2,%2,%3
				lvx 3,%2,%4
				vperm 5,0,1,4
				vperm 6,2,3,4
				vavgub 5,5,6
				stvx 5,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 16; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%2,%3
				vavgub 0,0,1
				stvx 0,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_16x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm("
			lvsl 4,%0,%1
		" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 8; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%1,%4
				lvx 2,%2,%3
				lvx 3,%2,%4
				vperm 5,0,1,4
				vperm 6,2,3,4
				vavgub 5,5,6
				stvx 5,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 8; i++) {
			asm("
				lvx 0,%1,%3
				lvx 1,%2,%3
				vavgub 0,0,1
				stvx 0,%0,%3
			" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_8x8(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpY_U8_U8_8x4(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpXY_U8_U8_16x16(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 4,%0,%1
		vaddubs 5,4,0
	" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm("
			lvx 0,%1,%3
			lvx 1,%2,%3
			lvx 2,%1,%4
			lvx 3,%2,%4
			vperm 6,0,2,4
			vperm 7,0,2,5
			vperm 8,1,3,4
			vperm 9,1,3,5
			vavgub 6,6,7
			vavgub 8,8,9
			vavgub 6,6,8
			stvx 6,%0,%3
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpXY_U8_U8_16x8(uint8_t *curr_block,
			      uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm("
		vspltisb 0,1
		lvsl 4,%0,%1
		vaddubs 5,4,0
	" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 8; i++) {
		asm("
			lvx 0,%1,%3
			lvx 1,%2,%3
			lvx 2,%1,%4
			lvx 3,%2,%4
			vperm 6,0,2,4
			vperm 7,0,2,5
			vperm 8,1,3,4
			vperm 9,1,3,5
			vavgub 6,6,7
			vavgub 8,8,9
			vavgub 6,6,8
			stvx 6,%0,%3
		" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpXY_U8_U8_8x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x4(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void mlib_VideoAddBlock_U8_S16(uint8_t *output, int16_t *input, int32_t stride) 
{
	; /* We should really add this even if it's not used currently */
}

void mlib_ClearCoeffs(int16_t *coeffs)
{
	asm("
		vspltish 0,0
		stvx 0,%0,%1
		addi %1,%1,32
		stvx 0,%0,%2
		addi %2,%2,32
		stvx 0,%0,%1
		addi %1,%1,32
		stvx 0,%0,%2
		addi %2,%2,32
		stvx 0,%0,%1
		addi %1,%1,32
		stvx 0,%0,%2
		addi %2,%2,32
		stvx 0,%0,%1
		stvx 0,%0,%2
	" : : "b" (coeffs), "b" (0), "b" (16));
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
	23170, 13573, 6518, 21895, -23170, -21895, 0, 0 };

static const int16_t PreScale[64] __attribute__ ((aligned (16))) = {
	16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725, 
	22725, 31521, 29692, 26722, 22725, 26722, 29692, 31521, 
	21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692, 
	19266, 26722, 25172, 22654, 19266, 22654, 25172, 26722, 
};

void mlib_VideoIDCTAdd_U8_S16(uint8_t *output, int16_t *input, int32_t stride) 
{
	ASSERT(((int)output & 7) == 0);

	asm("
		vspltish        31,0
		vspltish	16,4

		lvx     24,0,%1
		vsplth  25,24,0
		vsplth  26,24,1
		vsplth  27,24,2
		vsplth  28,24,3
		vsplth  29,24,4
		vsplth  30,24,5

		addi	5,0,0
		addi    6,0,16
		lvx     0,%0,5
		lvx     1,%0,6
		vsl	0,0,16
		vsl	1,1,16
		addi    7,0,32
		addi    8,0,48
		lvx     2,%0,7
		lvx     3,%0,8
		vsl	2,2,16
		vsl	3,3,16
		addi    9,0,64
		addi    10,0,80
		lvx     4,%0,9
		lvx     5,%0,10
		vsl	4,4,16
		vsl	5,5,16
		addi    11,0,96
		addi    12,0,112
		lvx     6,%0,11
		lvx     7,%0,12
		vsl	6,6,16
		vsl	7,7,16

		lvx     0+8,%2,5
		lvx     1+8,%2,6
		vmhraddshs      0,0,0+8,31
		vmhraddshs      1,1,1+8,31
		lvx     2+8,%2,7
		lvx     3+8,%2,8
		vmhraddshs      2,2,2+8,31
		vmhraddshs      3,3,3+8,31
		vmhraddshs      4,4,0+8,31
		vmhraddshs      5,5,3+8,31
		vmhraddshs      6,6,2+8,31
		vmhraddshs      7,7,1+8,31

		vmrghh	0+8,0,4
		vmrglh	1+8,0,4
		vmrghh	2+8,1,5
		vmrglh	3+8,1,5
		vmrghh	4+8,2,6
		vmrglh	5+8,2,6
		vmrghh	6+8,3,7
		vmrglh	7+8,3,7

		vmrghh	0+16,0+8,4+8
		vmrglh	1+16,0+8,4+8
		vmrghh	2+16,1+8,5+8
		vmrglh	3+16,1+8,5+8
		vmrghh	4+16,2+8,6+8
		vmrglh	5+16,2+8,6+8
		vmrghh	6+16,3+8,7+8
		vmrglh	7+16,3+8,7+8

		vmrghh	0,0+16,4+16
		vmrglh	1,0+16,4+16
		vmrghh	2,1+16,5+16
		vmrglh	3,1+16,5+16
		vmrghh	4,2+16,6+16
		vmrglh	5,2+16,6+16
		vmrghh	6,3+16,7+16
		vmrglh	7,3+16,7+16

		vmhraddshs      19,27,1,31
		vsubshs		18,19,7
		vmhraddshs      11,27,7,1
		vmhraddshs      17,28,5,3
		vmhraddshs      13,30,3,5

		vaddshs		15,0,4
		vsubshs		10,0,4
		vmhraddshs      19,26,2,31
		vsubshs		14,19,6
		vmhraddshs      12,26,6,2

		vaddshs		16,18,13
		vsubshs		13,18,13
		vsubshs		18,11,17
		vaddshs		11,11,17

		vaddshs		17,15,12
		vsubshs		12,15,12
		vaddshs		15,10,14
		vsubshs		10,10,14

		vsubshs		14,18,13
		vaddshs		13,18,13

		vaddshs		0,17,11
		vmhraddshs      1,25,13,15
		vmhraddshs      2,25,14,10
		vaddshs		3,12,16
		vsubshs		4,12,16
		vmhraddshs      5,29,14,10
		vmhraddshs      6,29,13,15
		vsubshs		7,17,11

		vmrghh  0+8,0,4
		vmrglh  1+8,0,4
		vmrghh  2+8,1,5
		vmrglh  3+8,1,5
		vmrghh  4+8,2,6
		vmrglh  5+8,2,6
		vmrghh  6+8,3,7
		vmrglh  7+8,3,7

		vmrghh  0+16,0+8,4+8
		vmrglh  1+16,0+8,4+8
		vmrghh  2+16,1+8,5+8
		vmrglh  3+16,1+8,5+8
		vmrghh  4+16,2+8,6+8
		vmrglh  5+16,2+8,6+8
		vmrghh  6+16,3+8,7+8
		vmrglh  7+16,3+8,7+8

		vmrghh  0,0+16,4+16
		vmrglh  1,0+16,4+16
		vmrghh  2,1+16,5+16
		vmrglh  3,1+16,5+16
		vmrghh  4,2+16,6+16
		vmrglh  5,2+16,6+16
		vmrghh  6,3+16,7+16
		vmrglh  7,3+16,7+16

		vmhraddshs      19,27,1,31
		vsubshs		18,19,7
		vmhraddshs      11,27,7,1
		vmhraddshs      17,28,5,3
		vmhraddshs      13,30,3,5

		vaddshs		15,0,4
		vsubshs		10,0,4
		vmhraddshs      19,26,2,31
		vsubshs		14,19,6
		vmhraddshs      12,26,6,2

		vaddshs		16,18,13
		vsubshs		13,18,13
		vsubshs		18,11,17
		vaddshs		11,11,17

		vaddshs		17,15,12
		vsubshs		12,15,12
		vaddshs		15,10,14
		vsubshs		10,10,14

		vsubshs		14,18,13
		vaddshs		13,18,13

		vaddshs		0,17,11
		vmhraddshs	1,25,13,15
		vmhraddshs	2,25,14,10
		vaddshs		3,12,16
		vsubshs		4,12,16
		vmhraddshs	5,29,14,10
		vmhraddshs	6,29,13,15
		vsubshs		7,17,11
	" : : "b" (input), "b" (SpecialConstants), "b" (PreScale)
	  : "cc", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "memory");
	asm("
		vspltish	16,6
		vspltisb	19,-1
		lvsl		17,%0,%1
		lvsl		18,%0,%2
		vmrghb		17,19,17
		vmrghb		18,19,18
	" : : "b" (output), "b" (0), "b" (0+stride));
	asm("
		lvx		0+8,%0,%1
		lvx		1+8,%0,%3
		vperm		0+8,0+8,31,17
		vperm		1+8,1+8,31,18
		vsrah		0,0,16
		vsrah		1,1,16
		vaddshs		0,0,0+8
		vaddshs		1,1,1+8
		vmaxsh		0,0,31
		vmaxsh		1,1,31
		vpkuhus		0,0,0
		vpkuhus		1,1,1
		stvewx		0,%0,%1
		stvewx		0,%0,%2
		stvewx		1,%0,%3
		stvewx		1,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm("
		lvx		2+8,%0,%1
		lvx		3+8,%0,%3
		vperm		2+8,2+8,31,17
		vperm		3+8,3+8,31,18
		vsrah		2,2,16
		vsrah		3,3,16
		vaddshs		2,2,2+8
		vaddshs		3,3,3+8
		vmaxsh		2,2,31
		vmaxsh		3,3,31
		vpkuhus		2,2,2
		vpkuhus		3,3,3
		stvewx		2,%0,%1
		stvewx		2,%0,%2
		stvewx		3,%0,%3
		stvewx		3,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm("
		lvx		4+8,%0,%1
		lvx		5+8,%0,%3
		vperm		4+8,4+8,31,17
		vperm		5+8,5+8,31,18
		vsrah		4,4,16
		vsrah		5,5,16
		vaddshs		4,4,4+8
		vaddshs		5,5,5+8
		vmaxsh		4,4,31
		vmaxsh		5,5,31
		vpkuhus		4,4,4
		vpkuhus		5,5,5
		stvewx		4,%0,%1
		stvewx		4,%0,%2
		stvewx		5,%0,%3
		stvewx		5,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm("
		lvx		6+8,%0,%1
		lvx		7+8,%0,%3
		vperm		6+8,6+8,31,17
		vperm		7+8,7+8,31,18
		vsrah		6,6,16
		vsrah		7,7,16
		vaddshs		6,6,6+8
		vaddshs		7,7,7+8
		vmaxsh		6,6,31
		vmaxsh		7,7,31
		vpkuhus		6,6,6
		vpkuhus		7,7,7
		stvewx		6,%0,%1
		stvewx		6,%0,%2
		stvewx		7,%0,%3
		stvewx		7,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
}  

void mlib_VideoIDCT8x8_U8_S16(uint8_t *output, int16_t *input, int stride)
{
	ASSERT(((int)output & 7) == 0);

	asm("
		vspltish        31,0
		vspltish	16,4

		lvx     24,0,%1
		vsplth  25,24,0
		vsplth  26,24,1
		vsplth  27,24,2
		vsplth  28,24,3
		vsplth  29,24,4
		vsplth  30,24,5

		addi	5,0,0
		addi    6,0,16
		lvx     0,%0,5
		lvx     1,%0,6
		vsl	0,0,16
		vsl	1,1,16
		addi    7,0,32
		addi    8,0,48
		lvx     2,%0,7
		lvx     3,%0,8
		vsl	2,2,16
		vsl	3,3,16
		addi    9,0,64
		addi    10,0,80
		lvx     4,%0,9
		lvx     5,%0,10
		vsl	4,4,16
		vsl	5,5,16
		addi    11,0,96
		addi    12,0,112
		lvx     6,%0,11
		lvx     7,%0,12
		vsl	6,6,16
		vsl	7,7,16

		lvx     0+8,%2,5
		lvx     1+8,%2,6
		vmhraddshs      0,0,0+8,31
		vmhraddshs      1,1,1+8,31
		lvx     2+8,%2,7
		lvx     3+8,%2,8
		vmhraddshs      2,2,2+8,31
		vmhraddshs      3,3,3+8,31
		vmhraddshs      4,4,0+8,31
		vmhraddshs      5,5,3+8,31
		vmhraddshs      6,6,2+8,31
		vmhraddshs      7,7,1+8,31

		vmrghh	0+8,0,4
		vmrglh	1+8,0,4
		vmrghh	2+8,1,5
		vmrglh	3+8,1,5
		vmrghh	4+8,2,6
		vmrglh	5+8,2,6
		vmrghh	6+8,3,7
		vmrglh	7+8,3,7

		vmrghh	0+16,0+8,4+8
		vmrglh	1+16,0+8,4+8
		vmrghh	2+16,1+8,5+8
		vmrglh	3+16,1+8,5+8
		vmrghh	4+16,2+8,6+8
		vmrglh	5+16,2+8,6+8
		vmrghh	6+16,3+8,7+8
		vmrglh	7+16,3+8,7+8

		vmrghh	0,0+16,4+16
		vmrglh	1,0+16,4+16
		vmrghh	2,1+16,5+16
		vmrglh	3,1+16,5+16
		vmrghh	4,2+16,6+16
		vmrglh	5,2+16,6+16
		vmrghh	6,3+16,7+16
		vmrglh	7,3+16,7+16

		vmhraddshs      19,27,1,31
		vsubshs		18,19,7
		vmhraddshs      11,27,7,1
		vmhraddshs      17,28,5,3
		vmhraddshs      13,30,3,5

		vaddshs		15,0,4
		vsubshs		10,0,4
		vmhraddshs      19,26,2,31
		vsubshs		14,19,6
		vmhraddshs      12,26,6,2

		vaddshs		16,18,13
		vsubshs		13,18,13
		vsubshs		18,11,17
		vaddshs		11,11,17

		vaddshs		17,15,12
		vsubshs		12,15,12
		vaddshs		15,10,14
		vsubshs		10,10,14

		vsubshs		14,18,13
		vaddshs		13,18,13

		vaddshs		0,17,11
		vmhraddshs      1,25,13,15
		vmhraddshs      2,25,14,10
		vaddshs		3,12,16
		vsubshs		4,12,16
		vmhraddshs      5,29,14,10
		vmhraddshs      6,29,13,15
		vsubshs		7,17,11

		vmrghh  0+8,0,4
		vmrglh  1+8,0,4
		vmrghh  2+8,1,5
		vmrglh  3+8,1,5
		vmrghh  4+8,2,6
		vmrglh  5+8,2,6
		vmrghh  6+8,3,7
		vmrglh  7+8,3,7

		vmrghh  0+16,0+8,4+8
		vmrglh  1+16,0+8,4+8
		vmrghh  2+16,1+8,5+8
		vmrglh  3+16,1+8,5+8
		vmrghh  4+16,2+8,6+8
		vmrglh  5+16,2+8,6+8
		vmrghh  6+16,3+8,7+8
		vmrglh  7+16,3+8,7+8

		vmrghh  0,0+16,4+16
		vmrglh  1,0+16,4+16
		vmrghh  2,1+16,5+16
		vmrglh  3,1+16,5+16
		vmrghh  4,2+16,6+16
		vmrglh  5,2+16,6+16
		vmrghh  6,3+16,7+16
		vmrglh  7,3+16,7+16

		vmhraddshs      19,27,1,31
		vsubshs		18,19,7
		vmhraddshs      11,27,7,1
		vmhraddshs      17,28,5,3
		vmhraddshs      13,30,3,5

		vaddshs		15,0,4
		vsubshs		10,0,4
		vmhraddshs      19,26,2,31
		vsubshs		14,19,6
		vmhraddshs      12,26,6,2

		vaddshs		16,18,13
		vsubshs		13,18,13
		vsubshs		18,11,17
		vaddshs		11,11,17

		vaddshs		17,15,12
		vsubshs		12,15,12
		vaddshs		15,10,14
		vsubshs		10,10,14

		vsubshs		14,18,13
		vaddshs		13,18,13

		vaddshs		0,17,11
		vmhraddshs	1,25,13,15
		vmhraddshs	2,25,14,10
		vaddshs		3,12,16
		vsubshs		4,12,16
		vmhraddshs	5,29,14,10
		vmhraddshs	6,29,13,15
		vsubshs		7,17,11
	" : : "b" (input), "b" (SpecialConstants), "b" (PreScale)
	  : "cc", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "memory");
	asm("
		vspltish	16,6
	");
	asm("
		vsrah		0,0,16
		vsrah		1,1,16
		vmaxsh		0,0,31
		vmaxsh		1,1,31
		vpkuhus		0,0,0
		vpkuhus		1,1,1
		stvewx		0,%0,%1
		stvewx		0,%0,%2
		stvewx		1,%0,%3
		stvewx		1,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm("
		vsrah		2,2,16
		vsrah		3,3,16
		vmaxsh		2,2,31
		vmaxsh		3,3,31
		vpkuhus		2,2,2
		vpkuhus		3,3,3
		stvewx		2,%0,%1
		stvewx		2,%0,%2
		stvewx		3,%0,%3
		stvewx		3,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm("
		vsrah		4,4,16
		vsrah		5,5,16
		vmaxsh		4,4,31
		vmaxsh		5,5,31
		vpkuhus		4,4,4
		vpkuhus		5,5,5
		stvewx		4,%0,%1
		stvewx		4,%0,%2
		stvewx		5,%0,%3
		stvewx		5,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm("
		vsrah		6,6,16
		vsrah		7,7,16
		vmaxsh		6,6,31
		vmaxsh		7,7,31
		vpkuhus		6,6,6
		vpkuhus		7,7,7
		stvewx		6,%0,%1
		stvewx		6,%0,%2
		stvewx		7,%0,%3
		stvewx		7,%0,%4
	" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
}
