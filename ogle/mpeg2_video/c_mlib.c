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

uint32_t matrix_coefficients = 1;

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
mlib_VideoColorYUV2ABGR420(uint8_t* image, const uint8_t* py, 
			   const uint8_t* pu, const uint8_t* pv, 
			   const uint32_t h_size, const uint32_t v_size, 
			   const uint32_t rgb_stride, const uint32_t y_stride,
			   const uint32_t uv_stride)
{
  int32_t Y,U,V;
  int32_t g_common,b_common,r_common;
  uint32_t x,y;
  
  uint32_t *dst_line_1;
  uint32_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
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
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgu * V - 32768;
	  b_common = cbu * U + 32768;

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
	    clip_to_u8((Y-g_common)>>16)<<8 |
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





