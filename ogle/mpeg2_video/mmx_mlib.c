/*
 *  mmx_mlib.c
 *
 *  Intel MMX implementation of motion comp routines.
 *  MMX code written by David I. Lehn <dlehn@vt.edu>.
 *  lib{mmx,xmmx,sse} can be found at http://shay.ecn.purdue.edu/~swar/
 *
 *  Copyright 2000, David I. Lehn <dlehn@vt.edu>. Released under GPL.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "mmx.h"

extern void IDCT_mmx(int16_t *);

static uint64_t ones = 0x0001000100010001ULL;
static uint64_t twos = 0x0002000200020002ULL;

//#define MC_MMX_verify
inline static uint8_t
clip_to_u8 (int16_t value)
{
  return value < 0 ? 0 : (value > 255 ? 255 : value);
}

static inline void
mmx_average_2_U8(uint8_t *dst, uint8_t *src1, uint8_t *src2)
{
   //
   // *dst = clip_to_u8((*src1 + *src2 + 1)/2);
   //

   //pxor_r2r(mm0,mm0);         // load 0 into mm0

   movq_m2r(*src1,mm1);        // load 8 src1 bytes
   movq_r2r(mm1,mm2);          // copy 8 src1 bytes

   movq_m2r(*src2,mm3);        // load 8 src2 bytes
   movq_r2r(mm3,mm4);          // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm1);     // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);     // unpack high src1 bytes

   punpcklbw_r2r(mm0,mm3);     // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);     // unpack high src2 bytes

   paddw_m2r(ones, mm1);
   paddw_r2r(mm3,mm1);         // add lows to mm1
   psraw_i2r(1,mm1);           // /2

   paddw_m2r(ones, mm2);
   paddw_r2r(mm4,mm2);         // add highs to mm2
   psraw_i2r(1,mm2);           // /2

   packuswb_r2r(mm2,mm1);      // pack (w/ saturation)
   movq_r2m(mm1,*dst);         // store result in dst
}

static inline void
mmx_interp_average_2_U8(uint8_t *dst, uint8_t *src1, uint8_t *src2)
{
   //
   // *dst = clip_to_u8((*dst + (*src1 + *src2 + 1)/2 + 1)/2);
   //

   //pxor_r2r(mm0,mm0);             // load 0 into mm0

   movq_m2r(*dst,mm1);            // load 8 dst bytes
   movq_r2r(mm1,mm2);             // copy 8 dst bytes

   movq_m2r(*src1,mm3);           // load 8 src1 bytes
   movq_r2r(mm3,mm4);             // copy 8 src1 bytes

   movq_m2r(*src2,mm5);           // load 8 src2 bytes
   movq_r2r(mm5,mm6);             // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm1);        // unpack low dst bytes
   punpckhbw_r2r(mm0,mm2);        // unpack high dst bytes

   punpcklbw_r2r(mm0,mm3);        // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm4);        // unpack high src1 bytes

   punpcklbw_r2r(mm0,mm5);        // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm6);        // unpack high src2 bytes

   paddw_m2r(ones, mm3);
   paddw_r2r(mm5,mm3);            // add lows
   paddw_m2r(ones, mm4);
   paddw_r2r(mm6,mm4);            // add highs

   psraw_i2r(1,mm3);              // /2
   psraw_i2r(1,mm4);              // /2

   paddw_m2r(ones, mm1);
   paddw_r2r(mm3,mm1);            // add lows
   paddw_m2r(ones, mm2);
   paddw_r2r(mm4,mm2);            // add highs

   psraw_i2r(1,mm1);              // /2
   psraw_i2r(1,mm2);              // /2

   packuswb_r2r(mm2,mm1);         // pack (w/ saturation)
   movq_r2m(mm1,*dst);            // store result in dst
}

static inline void
mmx_average_4_U8(uint8_t *dst, uint8_t *src1, uint8_t *src2, uint8_t *src3, uint8_t *src4)
{
   //
   // *dst = clip_to_u8((*src1 + *src2 + *src3 + *src4 + 2)/4);
   //

   //pxor_r2r(mm0,mm0);                  // load 0 into mm0

   movq_m2r(*src1,mm1);                // load 8 src1 bytes
   movq_r2r(mm1,mm2);                  // copy 8 src1 bytes

   punpcklbw_r2r(mm0,mm1);             // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);             // unpack high src1 bytes

   movq_m2r(*src2,mm3);                // load 8 src2 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src2 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   // now have partials in mm1 and mm2

   movq_m2r(*src3,mm3);                // load 8 src3 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src3 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src3 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src3 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   movq_m2r(*src4,mm5);                // load 8 src4 bytes
   movq_r2r(mm5,mm6);                  // copy 8 src4 bytes

   punpcklbw_r2r(mm0,mm5);             // unpack low src4 bytes
   punpckhbw_r2r(mm0,mm6);             // unpack high src4 bytes

   paddw_m2r(twos, mm1);
   paddw_r2r(mm5,mm1);                 // add lows
   paddw_m2r(twos, mm2);
   paddw_r2r(mm6,mm2);                 // add highs

   // now have subtotal in mm1 and mm2

   psraw_i2r(2,mm1);                   // /4
   psraw_i2r(2,mm2);                   // /4

   packuswb_r2r(mm2,mm1);              // pack (w/ saturation)
   movq_r2m(mm1,*dst);                 // store result in dst
}

static inline void
mmx_interp_average_4_U8(uint8_t *dst, uint8_t *src1, uint8_t *src2, uint8_t *src3, uint8_t *src4)
{
   //
   // *dst = clip_to_u8((*dst + (*src1 + *src2 + *src3 + *src4 + 2)/4 + 1)/2);
   //

   //pxor_r2r(mm0,mm0);                  // load 0 into mm0

   movq_m2r(*src1,mm1);                // load 8 src1 bytes
   movq_r2r(mm1,mm2);                  // copy 8 src1 bytes

   punpcklbw_r2r(mm0,mm1);             // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);             // unpack high src1 bytes

   movq_m2r(*src2,mm3);                // load 8 src2 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src2 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   // now have partials in mm1 and mm2

   movq_m2r(*src3,mm3);                // load 8 src3 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src3 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src3 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src3 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   movq_m2r(*src4,mm5);                // load 8 src4 bytes
   movq_r2r(mm5,mm6);                  // copy 8 src4 bytes

   punpcklbw_r2r(mm0,mm5);             // unpack low src4 bytes
   punpckhbw_r2r(mm0,mm6);             // unpack high src4 bytes

   paddw_m2r(twos, mm1);
   paddw_r2r(mm5,mm1);                 // add lows
   paddw_m2r(twos, mm2);
   paddw_r2r(mm6,mm2);                 // add highs

   psraw_i2r(2,mm1);                   // /4
   psraw_i2r(2,mm2);                   // /4

   // now have subtotal/4 in mm1 and mm2

   movq_m2r(*dst,mm3);                 // load 8 dst bytes
   movq_r2r(mm3,mm4);                  // copy 8 dst bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low dst bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high dst bytes

   paddw_m2r(ones, mm1);
   paddw_r2r(mm3,mm1);                 // add lows
   paddw_m2r(ones, mm2);
   paddw_r2r(mm4,mm2);                 // add highs

   psraw_i2r(1,mm1);                   // /2
   psraw_i2r(1,mm2);                   // /2

   // now have end value in mm1 and mm2

   packuswb_r2r(mm2,mm1);              // pack (w/ saturation)
   movq_r2m(mm1,*dst);                 // store result in dst
}

// Add a motion compensated 8x8 block to the current block
void
mlib_VideoAddBlock_U8_S16(
      uint8_t *curr_block,
      int16_t *mc_block,
      int32_t stride) 
{
   const int n = 8;
#define MMX_mmx_VideoAddBlock_U8_S16
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoAddBlock_U8_S16)
   int x,y;
   const int jump = stride - n;

   for (y = 0; y < n; y++) {
      for (x = 0; x < n; x++)
         *curr_block++ = clip_to_u8(*curr_block + *mc_block++);
      curr_block += jump;
   }
#else
   int x,y;

   //printf("%d %d\n", (uint_32)curr_block % 64, (uint_32)mc_block % 64);

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < n/8; x++) {
         movq_m2r(*curr_block,mm1);     // load 8 curr bytes
         movq_r2r(mm1,mm2);             // copy 8 curr bytes

         punpcklbw_r2r(mm0,mm1);        // unpack low curr bytes
         punpckhbw_r2r(mm0,mm2);        // unpack high curr bytes

         movq_m2r(*mc_block,mm3);       // load 4 mc words (mc0)
         paddsw_r2r(mm3,mm1);           // mm1=curr+mc0 (w/ saturatation)

         movq_m2r(*(mc_block+4),mm4);   // load next 4 mc words (mc1)
         paddsw_r2r(mm4,mm2);           // mm2=curr+mc1 (w/ saturatation)

         packuswb_r2r(mm2,mm1);         // pack (w/ saturation)
         movq_r2m(mm1,*curr_block);     // store result in curr

         curr_block += stride;
         mc_block   += n;
      }
   }
#endif
}  

// VideoCopyRef* - Copy block from reference block to current block
// ---------------------------------------------------------------
inline void
mlib_VideoCopyRefAve_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
#define MMX_mmx_VideoCopyRefAve_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoCopyRefAve_U8_U8_MxN)
   int x,y;
   const int jump = stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*curr_block + *ref_block++ + 1)/2);
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = stride - m;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_2_U8(curr_block, curr_block, ref_block);

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void
mlib_VideoCopyRefAve_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRefAve_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRefAve_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      8, 8, 
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRefAve_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      stride);
}

#if 0
inline void
mlib_VideoCopyRef_U8_U8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t width,
      int32_t height,
      int32_t stride)
{
   int x,y;
   const int jump = stride - width;
   printf("I don't get called!");

   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++)
         *curr_block++ = *ref_block++;
      ref_block  += jump;
      curr_block += jump;
   }
}
#endif
void
print_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *block0,
      uint8_t *block1,
      int32_t stride)
{
   int x,y;
   int jump = stride - m;
   printf("block %dx%d @ %x<-%x stride=%d\n",n,n,(uint32_t)block0,(uint32_t)block1,stride);
   for (y = 0; y < n; y++) {
      printf("%2d: ",y);
      for (x = 0; x < m; x++) {
         printf("%3d<%3d ",*block0++,*block1++);
      }
      printf("\n");
      block0 += jump;
      block1 += jump;
   }
}

inline void
mlib_VideoCopyRef_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
#define MMX_mmx_VideoCopyRef_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoCopyRef_U8_U8_MxN)
   int x,y;
   const int jump = stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = *ref_block++;
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = stride - m;
#ifdef MC_MMX_verify
   uint8_t *old_curr_block = curr_block;
   uint8_t *old_ref_block = ref_block;
   printf("before\n");
   print_U8_U8_MxN(n,old_curr_block,old_ref_block,stride);
#endif

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         movq_m2r(*ref_block,mm1);    // load 8 ref bytes
         movq_r2m(mm1,*curr_block);   // store 8 bytes at curr

         curr_block += step;
         ref_block  += step;
         //printf("iter n=%d x=%d y=%d curr=%x ref=%x\n",n,x,y,(uint32_t)curr_block,(uint32_t)ref_block);
         //print_U8_U8_MxN(n,old_curr_block,old_ref_block,stride);
         //getchar();
      }
      curr_block += jump;
      ref_block  += jump;
   }
#ifdef MC_MMX_verify
   printf("after\n");
   print_U8_U8_MxN(n,old_curr_block,old_ref_block,stride);
   getchar();
#endif
#endif
}

void
mlib_VideoCopyRef_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRef_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRef_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRef_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      stride);
}

// VideoInterp*X - Half pixel interpolation in the x direction
// ------------------------------------------------------------------
inline void 
mlib_VideoInterpAveX_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpAveX_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpAveX_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*curr_block + (*ref_block++ + *(ref_block + 1) + 1)/2 + 1)/2);
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_interp_average_2_U8(curr_block, ref_block, ref_block + 1);

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void 
mlib_VideoInterpAveX_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

inline void
mlib_VideoInterpX_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpX_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpX_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*ref_block++ + *(ref_block + 1) + 1)/2);
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - n;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_2_U8(curr_block, ref_block, ref_block + 1);

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void
mlib_VideoInterpX_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpX_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpX_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpX_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

// VideoInterp*XY - half pixel interpolation in both x and y directions
// --------------------------------------------------------------------
inline void 
mlib_VideoInterpAveXY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpAveXY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpAveXY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block+field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*curr_block + (*ref_block++ + *(ref_block + 1) + *ref_block_next++ + *(ref_block_next + 1) + 2)/4 + 1)/2);
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - n;
   uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_interp_average_4_U8(curr_block, ref_block, ref_block + 1, ref_block_next, ref_block_next + 1);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void 
mlib_VideoInterpAveXY_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

inline void 
mlib_VideoInterpXY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpXY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpXY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block+field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*ref_block++ + *(ref_block + 1) + *ref_block_next++ + *(ref_block_next + 1) + 2)/4);
      curr_block += jump;
      ref_block += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_4_U8(curr_block, ref_block, ref_block + 1, ref_block_next, ref_block_next + 1);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void 
mlib_VideoInterpXY_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

// VideoInterp*Y - half pixel interpolation in the y direction
// -----------------------------------------------------------
inline void 
mlib_VideoInterpAveY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpAveY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpAveY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block + field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*curr_block + (*ref_block++ + *ref_block_next++ + 1)/2 + 1)/2);
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_interp_average_2_U8(curr_block, ref_block, ref_block_next);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void 
mlib_VideoInterpAveY_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

inline void
mlib_VideoInterpY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block + field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = clip_to_u8((*ref_block++ + *ref_block_next++ + 1)/2);
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_2_U8(curr_block, ref_block, ref_block_next);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void
mlib_VideoInterpY_U8_U8_16x16(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpY_U8_U8_16x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpY_U8_U8_8x8(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpY_U8_U8_8x4(
      uint8_t *curr_block,
      uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}




#define W1 2841 /* 2048*sqrt(2)*cos(1*pi/16) */
#define W2 2676 /* 2048*sqrt(2)*cos(2*pi/16) */
#define W3 2408 /* 2048*sqrt(2)*cos(3*pi/16) */
#define W5 1609 /* 2048*sqrt(2)*cos(5*pi/16) */
#define W6 1108 /* 2048*sqrt(2)*cos(6*pi/16) */
#define W7 565  /* 2048*sqrt(2)*cos(7*pi/16) */



/* row (horizontal) IDCT
 *
 *           7                       pi         1
 * dst[k] = sum c[l] * src[l] * cos( -- * ( k + - ) * l )
 *          l=0                      8          2
 *
 * where: c[0]    = 128
 *        c[1..7] = 128*sqrt(2)
 */

inline static void 
idct_row(int16_t *blk, int16_t *coeffs)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  x1 = coeffs[4]<<11; 
  x2 = coeffs[6]; 
  x3 = coeffs[2];
  x4 = coeffs[1]; 
  x5 = coeffs[7]; 
  x6 = coeffs[5]; 
  x7 = coeffs[3];

#if 0
  /* shortcut */
  if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7 ))
  {
    blk[0]=blk[1]=blk[2]=blk[3]=blk[4]=blk[5]=blk[6]=blk[7]=coeffs[0]<<3;
    return;
  }
#endif

  x0 = (coeffs[0]<<11) + 128; /* for proper rounding in the fourth stage */

  /* first stage */
  x8 = W7*(x4+x5);
  x4 = x8 + (W1-W7)*x4;
  x5 = x8 - (W1+W7)*x5;
  x8 = W3*(x6+x7);
  x6 = x8 - (W3-W5)*x6;
  x7 = x8 - (W3+W5)*x7;
  
  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2);
  x2 = x1 - (W2+W6)*x2;
  x3 = x1 + (W2-W6)*x3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;
  
  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;
  
  /* fourth stage */
  blk[0] = (x7+x1)>>8;
  blk[1] = (x3+x2)>>8;
  blk[2] = (x0+x4)>>8;
  blk[3] = (x8+x6)>>8;
  blk[4] = (x8-x6)>>8;
  blk[5] = (x0-x4)>>8;
  blk[6] = (x3-x2)>>8;
  blk[7] = (x7-x1)>>8;
}


/* column (vertical) IDCT
 *
 *             7                         pi         1
 * dst[8*k] = sum c[l] * src[8*l] * cos( -- * ( k + - ) * l )
 *            l=0                        8          2
 *
 * where: c[0]    = 1/1024
 *        c[1..7] = (1/1024)*sqrt(2)
 */

/* FIXME something odd is going on with inlining this 
 * procedure. Things break if it isn't inlined */
inline static void 
idct_col(int16_t *blk, int16_t *coeffs)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  x1 = (coeffs[8*4]<<8); 
  x2 = coeffs[8*6]; 
  x3 = coeffs[8*2];
  x4 = coeffs[8*1];
  x5 = coeffs[8*7]; 
  x6 = coeffs[8*5];
  x7 = coeffs[8*3];

#if 0
  if (!(x1  | x2 | x3 | x4 | x5 | x6 | x7 ))
  {
    blk[8*0] = blk[8*1] = blk[8*2] = blk[8*3] 
      = blk[8*4] = blk[8*5] = blk[8*6] = blk[8*7] 
      = (coeffs[8*0]+32)>>6;
    return;
  }
#endif

  x0 = (coeffs[8*0]<<8) + 8192;

  /* first stage */
  x8 = W7*(x4+x5) + 4;
  x4 = (x8+(W1-W7)*x4)>>3;
  x5 = (x8-(W1+W7)*x5)>>3;
  x8 = W3*(x6+x7) + 4;
  x6 = (x8-(W3-W5)*x6)>>3;
  x7 = (x8-(W3+W5)*x7)>>3;
  
  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2) + 4;
  x2 = (x1-(W2+W6)*x2)>>3;
  x3 = (x1+(W2-W6)*x3)>>3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;
  
  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;
  
  /* fourth stage */
  blk[8*0] = (x7+x1)>>14;
  blk[8*1] = (x3+x2)>>14;
  blk[8*2] = (x0+x4)>>14;
  blk[8*3] = (x8+x6)>>14;
  blk[8*4] = (x8-x6)>>14;
  blk[8*5] = (x0-x4)>>14;
  blk[8*6] = (x3-x2)>>14;
  blk[8*7] = (x7-x1)>>14;
}

inline static void 
idct_col_u8(uint8_t *blk, int16_t *coeffs, int stride)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  x1 = (coeffs[8*4]<<8); 
  x2 = coeffs[8*6]; 
  x3 = coeffs[8*2];
  x4 = coeffs[8*1];
  x5 = coeffs[8*7]; 
  x6 = coeffs[8*5];
  x7 = coeffs[8*3];

#if 0
  if (!(x1  | x2 | x3 | x4 | x5 | x6 | x7 ))
  {
    blk[stride*0] = blk[stride*1] = blk[stride*2] = blk[stride*3]
      = blk[stride*4] = blk[stride*5] = blk[stride*6] = blk[stride*7]
      = clip_to_u8(coeffs[stride*0]+32)>>6;
    return;
  }
#endif

  x0 = (coeffs[8*0]<<8) + 8192;

  /* first stage */
  x8 = W7*(x4+x5) + 4;
  x4 = (x8+(W1-W7)*x4)>>3;
  x5 = (x8-(W1+W7)*x5)>>3;
  x8 = W3*(x6+x7) + 4;
  x6 = (x8-(W3-W5)*x6)>>3;
  x7 = (x8-(W3+W5)*x7)>>3;
  
  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2) + 4;
  x2 = (x1-(W2+W6)*x2)>>3;
  x3 = (x1+(W2-W6)*x3)>>3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;
  
  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;
  
  /* fourth stage */
  blk[stride*0] = clip_to_u8((x7+x1)>>14);
  blk[stride*1] = clip_to_u8((x3+x2)>>14);
  blk[stride*2] = clip_to_u8((x0+x4)>>14);
  blk[stride*3] = clip_to_u8((x8+x6)>>14);
  blk[stride*4] = clip_to_u8((x8-x6)>>14);
  blk[stride*5] = clip_to_u8((x0-x4)>>14);
  blk[stride*6] = clip_to_u8((x3-x2)>>14);
  blk[stride*7] = clip_to_u8((x7-x1)>>14);
}



void
mlib_VideoIDCT8x8_S16_S16(int16_t *block,
			  int16_t *coeffs)
{
#ifndef HAVE_MMX
  int i;
  
  for (i=0; i<8; i++)
    idct_row(block + 8*i, coeffs + 8*i);
  for (i=0; i<8; i++)
    idct_col(block + i, block + i);
#else
  
  movq_m2r(*coeffs, mm0);
  movq_m2r(*(coeffs+4), mm1);
  psllw_i2r(4, mm0);
  movq_m2r(*(coeffs+8), mm2);
  psllw_i2r(4, mm1);
  movq_m2r(*(coeffs+12), mm3);
  psllw_i2r(4, mm2);
  movq_m2r(*(coeffs+16), mm4);
  psllw_i2r(4, mm3);
  movq_m2r(*(coeffs+20), mm5);
  psllw_i2r(4, mm4);
  movq_m2r(*(coeffs+24), mm6);
  psllw_i2r(4, mm5);
  movq_m2r(*(coeffs+28), mm7);
  psllw_i2r(4, mm6);
  psllw_i2r(4, mm7);
  movq_r2m(mm0, *block);
  movq_r2m(mm1, *(block+4));
  movq_r2m(mm2, *(block+8));
  movq_r2m(mm3, *(block+12));
  movq_r2m(mm4, *(block+16));
  movq_r2m(mm5, *(block+20));
  movq_r2m(mm6, *(block+24));
  movq_r2m(mm7, *(block+28));
  movq_m2r(*(coeffs+32), mm0);
  movq_m2r(*(coeffs+36), mm1);
  psllw_i2r(4, mm0);
  movq_m2r(*(coeffs+40), mm2);
  psllw_i2r(4, mm1);
  movq_m2r(*(coeffs+44), mm3);
  psllw_i2r(4, mm2);
  movq_m2r(*(coeffs+48), mm4);
  psllw_i2r(4, mm3);
  movq_m2r(*(coeffs+52), mm5);
  psllw_i2r(4, mm4);
  movq_m2r(*(coeffs+56), mm6);
  psllw_i2r(4, mm5);
  movq_m2r(*(coeffs+60), mm7);
  psllw_i2r(4, mm6);
  psllw_i2r(4, mm7);
  movq_r2m(mm0, *(block+32));
  movq_r2m(mm1, *(block+36));
  movq_r2m(mm2, *(block+40));
  movq_r2m(mm3, *(block+44));
  movq_r2m(mm4, *(block+48));
  movq_r2m(mm5, *(block+52));
  movq_r2m(mm6, *(block+56));
  movq_r2m(mm7, *(block+60));
	
  IDCT_mmx( block );
#endif
}


void
mlib_VideoIDCT8x8_U8_S16(uint8_t *block,
			 int16_t *coeffs,
			 int32_t stride)
{
#ifndef HAVE_MMX
  int16_t temp[64];
  int i;
  
  for (i=0; i<8; i++)
    idct_row(temp + 8*i, coeffs + 8*i);
  for (i=0; i<8; i++)
    idct_col_u8(block + i, temp + i, stride);
#else
  int x,y;
  const int n = 8;
  const int jump = stride - n;

  movq_m2r(*coeffs, mm0);
  movq_m2r(*(coeffs+4), mm1);
  psllw_i2r(4, mm0);
  movq_m2r(*(coeffs+8), mm2);
  psllw_i2r(4, mm1);
  movq_m2r(*(coeffs+12), mm3);
  psllw_i2r(4, mm2);
  movq_m2r(*(coeffs+16), mm4);
  psllw_i2r(4, mm3);
  movq_m2r(*(coeffs+20), mm5);
  psllw_i2r(4, mm4);
  movq_m2r(*(coeffs+24), mm6);
  psllw_i2r(4, mm5);
  movq_m2r(*(coeffs+28), mm7);
  psllw_i2r(4, mm6);
  psllw_i2r(4, mm7);
  movq_r2m(mm0, *coeffs);
  movq_r2m(mm1, *(coeffs+4));
  movq_r2m(mm2, *(coeffs+8));
  movq_r2m(mm3, *(coeffs+12));
  movq_r2m(mm4, *(coeffs+16));
  movq_r2m(mm5, *(coeffs+20));
  movq_r2m(mm6, *(coeffs+24));
  movq_r2m(mm7, *(coeffs+28));
  movq_m2r(*(coeffs+32), mm0);
  movq_m2r(*(coeffs+36), mm1);
  psllw_i2r(4, mm0);
  movq_m2r(*(coeffs+40), mm2);
  psllw_i2r(4, mm1);
  movq_m2r(*(coeffs+44), mm3);
  psllw_i2r(4, mm2);
  movq_m2r(*(coeffs+48), mm4);
  psllw_i2r(4, mm3);
  movq_m2r(*(coeffs+52), mm5);
  psllw_i2r(4, mm4);
  movq_m2r(*(coeffs+56), mm6);
  psllw_i2r(4, mm5);
  movq_m2r(*(coeffs+60), mm7);
  psllw_i2r(4, mm6);
  psllw_i2r(4, mm7);
  movq_r2m(mm0, *(coeffs+32));
  movq_r2m(mm1, *(coeffs+36));
  movq_r2m(mm2, *(coeffs+40));
  movq_r2m(mm3, *(coeffs+44));
  movq_r2m(mm4, *(coeffs+48));
  movq_r2m(mm5, *(coeffs+52));
  movq_r2m(mm6, *(coeffs+56));
  movq_r2m(mm7, *(coeffs+60));

  IDCT_mmx( coeffs );
  
  pxor_r2r(mm0, mm0);             // load 0 into mm0
  
  for (y = 0; y < n; y++) {
    for (x = 0; x < n/8; x++) {
      movq_m2r(*coeffs, mm1);         // load 4 mc words (mc0)
      movq_m2r(*(coeffs+4), mm2);     // load next 4 mc words (mc1)
      
      packuswb_r2r(mm2, mm1);         // pack (w/ saturation)
      movq_r2m(mm1, *block);          // store result in curr
      
      block  += stride;
      coeffs += n;
    }
  }
#endif
}  



