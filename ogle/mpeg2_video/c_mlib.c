#include <inttypes.h>

/* NOT YET
mlib_VideoIDCT_S16_S16
mlib_VideoIDCT_U8_S16
*/

static inline uint8_t
clip_to_u8 (int16_t value)
{
  return value < 0 ? 0 : (value > 255 ? 255 : value);
}

void
mlib_VideoAddBlock_U8_S16 (uint8_t *curr_block,
			   int16_t *mc_block, 
                           int32_t stride) 
{
  int x,y;

  for (y = 0; y < 8; y++) {
    for (x = 0; x < 8; x++)
      curr_block[x] = clip_to_u8(curr_block[x] + *mc_block++);
    curr_block += stride;
  }
}  

void
mlib_VideoCopyRefAve_U8_U8 (uint8_t *curr_block,
			    uint8_t *ref_block,
                            int width, int height,
			    int32_t stride)
{
  int x,y;
  
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = clip_to_u8((curr_block[x] + ref_block[x] + 1)/2);
    ref_block += stride;
    curr_block += stride;
  }
}

void
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

void
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
        clip_to_u8((curr_block[x] + 
		    (ref_block[x] + ref_block[x+1] + 1)/2 + 1) /2);
    ref_block += frame_stride;
    curr_block += frame_stride;
  }
}

void
mlib_VideoInterpAveY_U8_U8(uint8_t *curr_block, 
                           uint8_t *ref_block,
                           int width, int height,
                           int32_t frame_stride,   
                           int32_t field_stride) 
{
  int x, y;
  uint8_t *ref_block_next = ref_block+field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = 
        clip_to_u8((curr_block[x] + 
		    (ref_block[x] + ref_block_next[x] + 1)/2 + 1)/2);
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

void
mlib_VideoInterpAveXY_U8_U8(uint8_t *curr_block, 
                            uint8_t *ref_block, 
                            int width, int height,
                            int32_t frame_stride,   
                            int32_t field_stride) 
{
  int x,y;
  uint8_t *ref_block_next = ref_block+field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = 
        clip_to_u8((curr_block[x] + 
		    (ref_block[x]      + ref_block[x+1] + 
		     ref_block_next[x] + ref_block_next[x+1] + 2)/4 + 1)/2);
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

void
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
        clip_to_u8((ref_block[x] + ref_block[x+1] + 1)/2);
    ref_block += frame_stride;
    curr_block += frame_stride;
  }
}

void
mlib_VideoInterpY_U8_U8(uint8_t *curr_block, 
			uint8_t *ref_block,
			int width, int height,
			int32_t frame_stride,   
			int32_t field_stride) 
{
  int x, y;
  uint8_t *ref_block_next = ref_block+field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = 
        clip_to_u8((ref_block[x] + ref_block_next[x] + 1)/2);
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

void
mlib_VideoInterpXY_U8_U8(uint8_t *curr_block, 
			 uint8_t *ref_block, 
			 int width, int height,
			 int32_t frame_stride,   
			 int32_t field_stride) 
{
  int x,y;
  uint8_t *ref_block_next = ref_block+field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = 
        clip_to_u8((ref_block[x]      + ref_block[x+1] + 
                    ref_block_next[x] + ref_block_next[x+1] + 2)/4);
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
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 16, 16, stride);
}

void 
mlib_VideoCopyRefAve_U8_U8_16x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 16, 8, stride);
}

void 
mlib_VideoCopyRefAve_U8_U8_8x8(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 8, 8, stride);
}

void 
mlib_VideoCopyRefAve_U8_U8_8x4(uint8_t *curr_block,
			       uint8_t *ref_block,
			       int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 8, 4, stride);
}

void
mlib_VideoCopyRef_U8_U8_16x16(uint8_t *curr_block,
			      uint8_t *ref_block,
			      int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 16, 16, stride);
}

void 
mlib_VideoCopyRef_U8_U8_16x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 16, 8, stride);
}

void 
mlib_VideoCopyRef_U8_U8_8x8(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 8, 8, stride);
}

void 
mlib_VideoCopyRef_U8_U8_8x4(uint8_t *curr_block,
			    uint8_t *ref_block,
			    int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 8, 4, stride);
}

void
mlib_VideoInterpAveX_U8_U8_16x16(uint8_t *curr_block,
                                 uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
  mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
  mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
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
  mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(uint8_t *curr_block,
				uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
  mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
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
  mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(uint8_t *curr_block,
                                 uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
  mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
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
  mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpX_U8_U8_16x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
  mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
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
  mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpY_U8_U8_16x8(uint8_t *curr_block,
			     uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
  mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
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
  mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_16x8(uint8_t *curr_block,
			      uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
  mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
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

uint32_t matrix_coefficients = 6;

int32_t Inverse_Table_6_9[8][4] =
{
  {117504, 138453, 13954, 34903}, /* no sequence_display_extension */
  {117504, 138453, 13954, 34903}, /* ITU-R Rec. 709 (1990) */
  {104597, 132201, 25675, 53279}, /* unspecified */
  {104597, 132201, 25675, 53279}, /* reserved */
  {104448, 132798, 24759, 53109}, /* FCC */
  {104597, 132201, 25675, 53279}, /* ITU-R Rec. 624-4 System B, G */
  {104597, 132201, 25675, 53279}, /* SMPTE 170M */
  {117579, 136230, 16907, 35559}  /* SMPTE 240M (1987) */
};


void 
mlib_VideoColorYUV2ABGR420(uint8_t* image, const uint8_t* py, const uint8_t* pu,
			   const uint8_t* pv, const uint32_t h_size,
			   const uint32_t v_size, const uint32_t rgb_stride,
			   const uint32_t y_stride, const uint32_t uv_stride)
{
  int32_t Y,U,V;
  int32_t g_common,b_common,r_common;
  uint32_t x,y;
  
  uint32_t *dst_line_1;
  uint32_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  volatile char prefetch;
  
  int32_t crv,cbu,cgu,cgv;
  
  /* matrix coefficients */
  crv = Inverse_Table_6_9[matrix_coefficients][0];
  cbu = Inverse_Table_6_9[matrix_coefficients][1];
  cgu = Inverse_Table_6_9[matrix_coefficients][2];
  cgv = Inverse_Table_6_9[matrix_coefficients][3];
	
  dst_line_1 = (uint32_t *)(image);
  dst_line_2 = (uint32_t *)(image + rgb_stride);
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  uint32_t pixel1,pixel2,pixel3,pixel4;

	  //Common to all four pixels
	  prefetch = pu[32];
	  U = (*pu++) - 128;
	  prefetch = pv[32];
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgu * V - 32768;
	  b_common = cbu * U + 32768;

	  prefetch = py_line_1[32];
	  //Pixel I
	  Y = 76309 * ((*py_line_1++) - 16);
	  pixel1 = 
	    clip_to_u8((Y+b_common)>>16)<<16 |
	    clip_to_u8((Y-g_common)>>16)<<8 |
	    clip_to_u8((Y+r_common)>>16);
	  *dst_line_1++ = pixel1;
		  
	  //Pixel II
	  Y = 76309 * ((*py_line_1++) - 16);
	  pixel2 = 
	    clip_to_u8((Y+b_common)>>16)<<16 |
	    clip_to_u8((Y-g_common)>>16)<<8 |
	    clip_to_u8((Y+r_common)>>16);
	  *dst_line_1++ = pixel2;

	  //Pixel III
	  prefetch = py_line_2[32];
	  Y = 76309 * ((*py_line_2++) - 16);
	  pixel3 = 
	    clip_to_u8((Y+b_common)>>16)<<16 |
	    clip_to_u8((Y-g_common)>>16)<<8 |
	    clip_to_u8((Y+r_common)>>16);
	  *dst_line_2++ = pixel3;

	  //Pixel IV
	  Y = 76309 * ((*py_line_2++) - 16);
	  pixel4 = 
	    clip_to_u8((Y+b_common)>>16)<<16 |
	    clip_to_u8((Y-g_common)>>16)<<8|
	    clip_to_u8((Y+r_common)>>16);
	  *dst_line_2++ = pixel4;
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride/4;
      dst_line_2 += rgb_stride/4;
    }
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

static void idct_row(int16_t *dst, int16_t *coeffs)
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
    coeffs[0]=coeffs[1]=coeffs[2]=coeffs[3]=coeffs[4]=coeffs[5]=coeffs[6]=coeffs[7]=coeffs[0]<<3;
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
  dst[0] = (x7+x1)>>8;
  dst[1] = (x3+x2)>>8;
  dst[2] = (x0+x4)>>8;
  dst[3] = (x8+x6)>>8;
  dst[4] = (x8-x6)>>8;
  dst[5] = (x0-x4)>>8;
  dst[6] = (x3-x2)>>8;
  dst[7] = (x7-x1)>>8;
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
static void idct_col(int16_t *dst, int16_t *coeffs)
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
    coeffs[8*0]=coeffs[8*1]=coeffs[8*2]=coeffs[8*3]=coeffs[8*4]=coeffs[8*5]=coeffs[8*6]=coeffs[8*7]=
      clip_to_u8((coeffs[8*0]+32)>>6);
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
  dst[8*0] = clip_to_u8((x7+x1)>>14);
  dst[8*1] = clip_to_u8((x3+x2)>>14);
  dst[8*2] = clip_to_u8((x0+x4)>>14);
  dst[8*3] = clip_to_u8((x8+x6)>>14);
  dst[8*4] = clip_to_u8((x8-x6)>>14);
  dst[8*5] = clip_to_u8((x0-x4)>>14);
  dst[8*6] = clip_to_u8((x3-x2)>>14);
  dst[8*7] = clip_to_u8((x7-x1)>>14);
}

 
void
mlib_VideoIDCT8x8_U8_S16(int16_t block[64], int16_t coeffs[64])
{
	int i,j;
	
        for (i=0; i < 8; i++)
          for (j=0; j < 8; j++)
            idct_row(block + 8*j, coeffs + 8*j);
          for (j=0; j < 8; j++)
            idct_col(block + j, coeffs + j);
}


