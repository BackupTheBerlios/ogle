#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include "video_stream.h"
#include "video_types.h"

#ifdef HAVE_MLIB
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>
#else
#ifdef HAVE_MMX
#include "mmx.h"
#endif
#include "c_mlib.h"
#endif

#include "../include/common.h"

extern yuv_image_t *dst_image;
extern yuv_image_t *fwd_ref_image;
extern yuv_image_t *bwd_ref_image;


/* This should be cleand up. */
void motion_comp()
{
  int padded_width,x,y;
  int stride;
  int d;
  uint8_t *dst_y,*dst_u,*dst_v;
  int half_flag_y[2];
  int half_flag_uv[2];
  int int_vec_y[2];
  int int_vec_uv[2];

  uint8_t *pred_y;
  uint8_t *pred_u;
  uint8_t *pred_v;
  int field;

  DPRINTF(2, "dct_type: %d\n", mb.modes.dct_type);

  padded_width = seq.mb_width * 16;
  x = seq.mb_column;
  y = seq.mb_row;
  
  if (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE)
    stride = padded_width;
  else
    stride = padded_width*2;
  
  dst_y = &dst_image->y[x * 16 + y * stride * 16];
  dst_u = &dst_image->u[x * 8 + y * stride/2 * 8];
  dst_v = &dst_image->v[x * 8 + y * stride/2 * 8];
  if (pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) {
    dst_y += stride/2;
    dst_u += stride/4;
    dst_v += stride/4;
  }
  
  
  if(mb.modes.macroblock_motion_forward) {

    int vcount, i;
    
    DPRINTF(2, "forward_motion_comp\n");
    DPRINTF(3, "x: %d, y: %d\n", x, y);
    
    if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
      fprintf(stderr, "**** DP remove this and check if working\n");
      //exit(-1);
    }

    if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
      vcount = mb.motion_vector_count;
    } else { 
      vcount = 1;
    }


    for(i = 0; i < vcount; i++) {
      
      field = mb.motion_vertical_field_select[i][0];

      half_flag_y[0]  = (mb.vector[i][0][0] & 1);
      half_flag_y[1]  = (mb.vector[i][0][1] & 1);
      half_flag_uv[0] = ((mb.vector[i][0][0]/2) & 1);
      half_flag_uv[1] = ((mb.vector[i][0][1]/2) & 1);

      int_vec_y[0]  = (mb.vector[i][0][0] >> 1) + x * 16;
      int_vec_y[1]  = (mb.vector[i][0][1] >> 1)*vcount + y * 16;
      int_vec_uv[0] = ((mb.vector[i][0][0]/2) >> 1)  + x * 8;
      int_vec_uv[1] = ((mb.vector[i][0][1]/2) >> 1)*vcount + y * 8;
      
      /* Check for vectors pointing outside the reference frames */
      if( (int_vec_y[0] < 0) || (int_vec_y[0] > (seq.mb_width - 1) * 16) ||
	  (int_vec_y[1] < 0) || (int_vec_y[1] > (seq.mb_height - 1) * 16) ||
	  (int_vec_uv[0] < 0) || (int_vec_uv[0] > (seq.mb_width - 1) * 8) ||
	  (int_vec_uv[1] < 0) || (int_vec_uv[1] > (seq.mb_height - 1) * 8) ) { 
	fprintf(stderr, "Y (%i,%i), UV (%i,%i)\n", 
		int_vec_y[0], int_vec_y[1], int_vec_uv[0], int_vec_uv[1]);
      }
      
      pred_y = &fwd_ref_image->y[int_vec_y[0] + int_vec_y[1] * stride];
      pred_u = &fwd_ref_image->u[int_vec_uv[0] + int_vec_uv[1] * stride/2];
      pred_v = &fwd_ref_image->v[int_vec_uv[0] + int_vec_uv[1] * stride/2];
      
      DPRINTF(3, "x: %d, y: %d\n", x, y);
      
      
      if (half_flag_y[0] && half_flag_y[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpXY_U8_U8_16x8(dst_y + i*stride,
					pred_y+field*stride,
					stride*2, stride*2);
	} else {
	  mlib_VideoInterpXY_U8_U8_16x16(dst_y, pred_y,
					 stride, stride);
	}
      } else if (half_flag_y[0]) {
	if(vcount == 2) {
	  mlib_VideoInterpX_U8_U8_16x8(dst_y + i*stride,
				       pred_y+field*stride,
				       stride*2, stride*2);
	} else {
	  mlib_VideoInterpX_U8_U8_16x16(dst_y, pred_y,
					stride, stride);
	}
      } else if (half_flag_y[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpY_U8_U8_16x8(dst_y + i*stride,
				       pred_y+field*stride,
				       stride*2, stride*2);
	} else {
	  mlib_VideoInterpY_U8_U8_16x16(dst_y, pred_y,
					stride, stride);
	}
      } else {
	if(vcount == 2) {
	  mlib_VideoCopyRef_U8_U8_16x8(dst_y + i*stride,
				       pred_y+field*stride,
				       stride*2);
	} else {
	  mlib_VideoCopyRef_U8_U8_16x16(dst_y, pred_y,
					stride);
	}
      }

      if (half_flag_uv[0] && half_flag_uv[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpXY_U8_U8_8x4(dst_u + i*stride/2, 
				       pred_u+field*stride/2,
				       stride*2/2, stride*2/2);

	  mlib_VideoInterpXY_U8_U8_8x4(dst_v + i*stride/2,
				       pred_v+field*stride/2,
				       stride*2/2, stride*2/2);
	} else {
	  mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u,
					 stride/2, stride/2);
	  mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v,
					 stride/2, stride/2);
	}
      } else if (half_flag_uv[0]) {
	if(vcount == 2) {
	  mlib_VideoInterpX_U8_U8_8x4(dst_u + i*stride/2,
				      pred_u+field*stride/2,
				      stride*2/2, stride*2/2);

	  mlib_VideoInterpX_U8_U8_8x4(dst_v + i*stride/2,
				      pred_v+field*stride/2,
				      stride*2/2, stride*2/2);
	} else {
	  mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u,
					stride/2, stride/2);
	  mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v, 
					stride/2, stride/2);
	}
      } else if (half_flag_uv[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpY_U8_U8_8x4(dst_u + i*stride/2,
				      pred_u+field*stride/2,
				      stride*2/2, stride*2/2);
	  
	  mlib_VideoInterpY_U8_U8_8x4(dst_v + i*stride/2,
				      pred_v+field*stride/2,
				      stride*2/2, stride*2/2);
	} else {
	  mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u,
					stride/2, stride/2);
	  
	  mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v,
					stride/2, stride/2);
	}
      } else {
	if(vcount == 2) {
	  mlib_VideoCopyRef_U8_U8_8x4(dst_u + i*stride/2,
				      pred_u+field*stride/2,
				      stride*2/2);
	  
	  mlib_VideoCopyRef_U8_U8_8x4(dst_v + i*stride/2,
				      pred_v+field*stride/2,
				      stride*2/2);
	} else {
	  mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, stride/2);
	  mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, stride/2);
	}
      }
    }
  }
  
  if(mb.modes.macroblock_motion_backward) {
    int vcount, i;
    
    DPRINTF(2, "backward_motion_comp\n");
    DPRINTF(3, "x: %d, y: %d\n", x, y);
    
    if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
      fprintf(stderr, "**** DP remove this and check if working\n");
      //exit(-1);
    }

    if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
      vcount = mb.motion_vector_count;
    } else { 
      vcount = 1;
    }
    
    for(i = 0; i < vcount; i++) {
      
      field = mb.motion_vertical_field_select[i][1];

      half_flag_y[0]  = (mb.vector[i][1][0] & 1);
      half_flag_y[1]  = (mb.vector[i][1][1] & 1);
      half_flag_uv[0] = ((mb.vector[i][1][0]/2) & 1);
      half_flag_uv[1] = ((mb.vector[i][1][1]/2) & 1);

      int_vec_y[0]  = (mb.vector[i][1][0] >> 1) + (signed int)x * 16;
      int_vec_y[1]  = (mb.vector[i][1][1] >> 1)*vcount + (signed int)y * 16;
      int_vec_uv[0] = ((mb.vector[i][1][0]/2) >> 1) + (signed int)x * 8;
      int_vec_uv[1] = ((mb.vector[i][1][1]/2) >> 1)*vcount + (signed int)y * 8;
      
      /* Check for vectors pointing outside the reference frames */
      if( (int_vec_y[0] < 0) || (int_vec_y[0] > (seq.mb_width - 1) * 16) ||
	  (int_vec_y[1] < 0) || (int_vec_y[1] > (seq.mb_height - 1) * 16) ||
	  (int_vec_uv[0] < 0) || (int_vec_uv[0] > (seq.mb_width - 1) * 8) ||
	  (int_vec_uv[1] < 0) || (int_vec_uv[1] > (seq.mb_height - 1) * 8) ) { 
	fprintf(stderr, "Y (%i,%i), UV (%i,%i)\n", 
               int_vec_y[0], int_vec_y[1], int_vec_uv[0], int_vec_uv[1]);
      }

      pred_y = &bwd_ref_image->y[int_vec_y[0] + int_vec_y[1] * stride];
      pred_u = &bwd_ref_image->u[int_vec_uv[0] + int_vec_uv[1] * stride/2];
      pred_v = &bwd_ref_image->v[int_vec_uv[0] + int_vec_uv[1] * stride/2];
      
 
      
      if(mb.modes.macroblock_motion_forward) {
	if (half_flag_y[0] && half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveXY_U8_U8_16x8(dst_y + i*stride,
					     pred_y+field*stride,
					     stride*2, stride*2);
	  } else {
	    mlib_VideoInterpAveXY_U8_U8_16x16(dst_y,  pred_y,  
					      stride,   stride);
	  }
	} else if (half_flag_y[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveX_U8_U8_16x8(dst_y + i*stride,
					    pred_y+field*stride,
					    stride*2, stride*2);
	  } else {
	    mlib_VideoInterpAveX_U8_U8_16x16(dst_y, pred_y,
					     stride, stride);
	  }
	} else if (half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveY_U8_U8_16x8(dst_y + i*stride,
					    pred_y+field*stride,
					    stride*2, stride*2);
	  } else {
	    mlib_VideoInterpAveY_U8_U8_16x16(dst_y, pred_y,
					     stride, stride);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRefAve_U8_U8_16x8(dst_y + i*stride,
					    pred_y+field*stride,
					    stride*2);
	  } else {
	    mlib_VideoCopyRefAve_U8_U8_16x16(dst_y, pred_y, stride);
	  }
	}
	if (half_flag_uv[0] && half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_u + i*stride/2,
					    pred_u+field*stride/2,
					    stride*2/2, stride*2/2);
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_v + i*stride/2,
					    pred_v+field*stride/2,
					    stride*2/2, stride*2/2);
	  } else {
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_u, pred_u,
					    stride/2, stride/2);
	    
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_v, pred_v,
					    stride/2, stride/2);
	  }
	} else if (half_flag_uv[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_u + i*stride/2,
					   pred_u+field*stride/2,
					   stride*2/2, stride*2/2);
	    
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_v + i*stride/2,
					   pred_v+field*stride/2,
					   stride*2/2, stride*2/2);
	  } else {
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_u, pred_u,
					   stride/2, stride/2);
	
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_v, pred_v,
					   stride/2, stride/2);
	  }
	} else if (half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_u + i*stride/2,
					   pred_u+field*stride/2,
					   stride*2/2, stride*2/2);
	   
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_v + i*stride/2,
					   pred_v+field*stride/2,
					   stride*2/2, stride*2/2);
	  } else {
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_u, pred_u,
					   stride/2, stride/2);
	 
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_v, pred_v,
					   stride/2, stride/2);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_u + i*stride/2,
					   pred_u+field*stride/2,
					   stride*2/2);

	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_v + i*stride/2,
					   pred_v+field*stride/2,
					   stride*2/2);
	  } else {
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_u, pred_u, stride/2);
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_v, pred_v, stride/2);
	  }
	}
      } else {
	if (half_flag_y[0] && half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpXY_U8_U8_16x8(dst_y + i*stride,
					  pred_y+field*stride,
					  stride*2, stride*2);
	  } else {
	    mlib_VideoInterpXY_U8_U8_16x16(dst_y, pred_y,
					   stride, stride);
	  }
	} else if (half_flag_y[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpX_U8_U8_16x8(dst_y + i*stride,
					 pred_y+field*stride,
					 stride*2, stride*2);
	  } else {
	    mlib_VideoInterpX_U8_U8_16x16(dst_y, pred_y,
					  stride, stride);
	  }
	} else if (half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpY_U8_U8_16x8(dst_y + i*stride,
					 pred_y+field*stride,
					 stride*2, stride*2);
	  } else {
	    mlib_VideoInterpY_U8_U8_16x16(dst_y, pred_y,
					  stride, stride);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRef_U8_U8_16x8(dst_y + i*stride,
					 pred_y+field*stride,
					 stride*2);
	  } else {
	    mlib_VideoCopyRef_U8_U8_16x16(dst_y, pred_y, stride);
	  }
	}
	if (half_flag_uv[0] && half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpXY_U8_U8_8x4(dst_u + i*stride/2,
					 pred_u+field*stride/2,
					 stride*2/2, stride*2/2);
	    
	    mlib_VideoInterpXY_U8_U8_8x4(dst_v + i*stride/2,
					 pred_v+field*stride/2, 
					 stride*2/2, stride*2/2);
	  } else {
	    mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u,
					   stride/2, stride/2);
	    
	    mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v,
					   stride/2, stride/2);
	  }
	} else if (half_flag_uv[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpX_U8_U8_8x4(dst_u + i*stride/2,
					pred_u+field*stride/2,
					stride*2/2, stride*2/2);
	    
	    mlib_VideoInterpX_U8_U8_8x4(dst_v + i*stride/2,
					pred_v+field*stride/2, 
					stride*2/2, stride*2/2);
	  } else {
	    mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u,
					  stride/2, stride/2);
	    
	    mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v,
					  stride/2, stride/2);
	  }
	} else if (half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpY_U8_U8_8x4(dst_u + i*stride/2,
					pred_u+field*stride/2,
					stride*2/2, stride*2/2);

	    mlib_VideoInterpY_U8_U8_8x4(dst_v + i*stride/2,
					pred_v+field*stride/2, 
					stride*2/2, stride*2/2);
	  } else {
	    mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u,
					  stride/2, stride/2);

	    mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v,
					  stride/2, stride/2);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRef_U8_U8_8x4(dst_u + i*stride/2,
					pred_u+field*stride/2,
					stride*2/2);
	    
	    mlib_VideoCopyRef_U8_U8_8x4(dst_v + i*stride/2,
					pred_v+field*stride/2, 
					stride*2/2);
	  } else {
	    mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, stride/2);
	    mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, stride/2);
	  }
	}
      }
    }
  }
  
}


void motion_comp_add_coeff(unsigned int i)
{
  int padded_width,x,y;
  int stride;
  int d;
  uint8_t *dst;

  padded_width = seq.mb_width * 16; //seq.horizontal_size;

  x = seq.mb_column;
  y = seq.mb_row;
  if (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
    if (mb.modes.dct_type) {
      d = 1;
      stride = padded_width * 2;
    } else {
      d = 8;
      stride = padded_width;
    }
    if(i < 4) {
      dst = &dst_image->y[x * 16 + y * 16 * padded_width];
      dst = (i & 1) ? dst + 8 : dst;
      dst = (i >= 2) ? dst + padded_width * d : dst;
    }
    else {
      stride = padded_width / 2; // OBS
      if( i == 4 )
	dst = &dst_image->u[x * 8 + y * 8 * stride];
      else // i == 5
	dst = &dst_image->v[x * 8 + y * 8 * stride];
    }
  } else {
    if(i < 4) {
      stride = padded_width * 2;
      dst = &dst_image->y[x * 16 + y * 16 * stride];
      dst = (i & 1) ? dst + 8 : dst;
      dst = (i >= 2) ? dst + stride * 8 : dst;
    }
    else {
      stride = padded_width; // OBS
      if( i == 4 )
	dst = &dst_image->u[x * 8 + y * 8 * stride];
      else // i == 5
	dst = &dst_image->v[x * 8 + y * 8 * stride];
    }
    if (pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD)
      dst += stride/2;
  }
  
  mlib_VideoAddBlock_U8_S16(dst, mb.QFS, stride);
}
