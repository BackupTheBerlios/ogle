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
  unsigned int padded_width;
  
  DPRINTF(2, "dct_type: %d\n", mb.modes.dct_type);
  
  if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
    fprintf(stderr, "**** DP remove this and check if working\n");
    exit(-1);
  }
  if(mb.prediction_type == PRED_TYPE_16x8_MC) {
    fprintf(stderr, "**** 16x8 MC remove this and check if working\n");
    exit(-1);
  }

  padded_width = seq.mb_width * 16;
  
  
  if(mb.modes.macroblock_motion_forward) {
    int vcount, i;
    
    DPRINTF(2, "forward_motion_comp\n");
    DPRINTF(3, "x: %d, y: %d\n", seq.mb_column, seq.mb_row);

    
    vcount = mb.motion_vector_count;  
    for(i = 0; i < vcount; i++) {
      uint8_t *dst_y, *dst_u;//, *dst_v;
      uint8_t *pred_y, *pred_u;//, *pred_v;
      int half_flags_y;
      int half_flags_uv;
      int pred_stride, stride;
      int dd;
      
      /* Image/Field select */
      /* FIXME: Should test is 'second coded field'. */
      if(pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD &&
	 pic.header.picture_coding_type == PIC_CODING_TYPE_P &&
	 ((mb.motion_vertical_field_select[i][0] == 0 /* TOP_FIELD */ &&
	   pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) ||
	  (mb.motion_vertical_field_select[i][0] == 1 /* BOTTOM_FIELD */ &&
	   pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD))) {
	pred_y = dst_image->y;
	pred_u = dst_image->u;
	//pred_v = dst_image->v;
      } else {
	pred_y = fwd_ref_image->y;
	pred_u = fwd_ref_image->u;
	//pred_v = fwd_ref_image->v;
      }
      
      /* Motion vector Origo (block->index) */
      /* Prediction destination (block->index) */
      if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	unsigned int x = seq.mb_column;
	unsigned int y = seq.mb_row;
	pred_y += x * 16 + y * 16 * padded_width;
	pred_u += x * 8 + y * 8 * padded_width/2;
	//pred_v += x * 8 + y * 8 * padded_width/2;
	dst_y = &dst_image->y[x * 16 + y * 16 * padded_width];
	dst_u = &dst_image->u[x * 8 + y * 8 * padded_width/2];
	//dst_v = &dst_image->v[x * 8 + y * 8 * padded_width/2];
      } else {
	unsigned int x = seq.mb_column;
	unsigned int y = seq.mb_row;
	pred_y += x * 16 + y * 16 * padded_width*2;
	pred_u += x * 8 + y * 8 * padded_width;	
	//pred_v += x * 8 + y * 8 * padded_width;	
	dst_y = &dst_image->y[x * 16 + y * 16 * padded_width*2];
	dst_u = &dst_image->u[x * 8 + y * 8 * padded_width];
	//dst_v = &dst_image->v[x * 8 + y * 8 * padded_width];
      }
      
      /* Prediction destination (field) */
      if(i == 1 || 
	 pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) {
	dst_y += padded_width;
	dst_u += padded_width/2;
	//dst_v += padded_width/2;
      }
      
      /* Motion vector offset */
      if(mb.mv_format == MV_FORMAT_FRAME)
	pred_stride = padded_width;
      else
	pred_stride = padded_width * 2;
      
      pred_y += (mb.vector[i][0][0] >> 1) + 
	(mb.vector[i][0][1] >> 1) * pred_stride;
      pred_u += ((mb.vector[i][0][0]/2) >> 1) + 
	((mb.vector[i][0][1]/2) >> 1) * pred_stride/2;
      //pred_v +=  ((mb.vector[i][0][0]/2) >> 1) + 
      //((mb.vector[i][0][1]/2) >> 1) * pred_stride/2;
      
      half_flags_y = 
	((mb.vector[i][0][0] << 1) | (mb.vector[i][0][1] & 1)) & 3;
      half_flags_uv = 
	(((mb.vector[i][0][0]/2) << 1) | ((mb.vector[i][0][1]/2) & 1)) & 3;
	
#if 0 
      /* Check for vectors pointing outside the reference frames */
      if( (int_vec_y[0] < 0) || (int_vec_y[0] > (seq.mb_width - 1) * 16) ||
	  (int_vec_y[1] < 0) || (int_vec_y[1] > (seq.mb_height - 1) * 16) ||
	  (int_vec_uv[0] < 0) || (int_vec_uv[0] > (seq.mb_width - 1) * 8) ||
	  (int_vec_uv[1] < 0) || (int_vec_uv[1] > (seq.mb_height - 1) * 8) ) { 
	fprintf(stderr, "Y (%i,%i), UV (%i,%i)\n", 
		int_vec_y[0], int_vec_y[1], int_vec_uv[0], int_vec_uv[1]);
      }
#endif
      
      /* Field select */
      // ?? Do we realy need mb.mv_format here ?? 
      // This is wrong for Dual Prime... 
      if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
	if(mb.motion_vertical_field_select[i][0]) {
	  pred_y += padded_width;
	  pred_u += padded_width/2;
	  //pred_v += padded_width/2;
	}
      }
      
      if(mb.prediction_type == PRED_TYPE_FIELD_BASED)
	stride = padded_width*2;
      else
	stride = padded_width;
      
      dd = fwd_ref_image->v - fwd_ref_image->u;
    
      if(vcount == 2) {
	switch(half_flags_y) {
	case 0:
	  mlib_VideoCopyRef_U8_U8_16x8(dst_y, pred_y, stride);
	  break;
	case 1:
	  mlib_VideoInterpY_U8_U8_16x8(dst_y, pred_y, stride, stride);
	  break;
	case 2:
	  mlib_VideoInterpX_U8_U8_16x8(dst_y, pred_y, stride, stride);
	  break;
	case 3:
	  mlib_VideoInterpXY_U8_U8_16x8(dst_y, pred_y, stride, stride);
	  break;
	}
	
	switch(half_flags_uv) {
	case 0:
	  mlib_VideoCopyRef_U8_U8_8x4(dst_u, pred_u, stride/2);
	  mlib_VideoCopyRef_U8_U8_8x4(dst_u+dd, pred_u+dd, stride/2);
	  //mlib_VideoCopyRef_U8_U8_8x4(dst_v, pred_v, stride/2);
	  break;
	case 1:
	  mlib_VideoInterpY_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	  mlib_VideoInterpY_U8_U8_8x4(dst_u+dd, pred_u+dd, stride/2, stride/2);
	  //mlib_VideoInterpY_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	  break;
	case 2:
	  mlib_VideoInterpX_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	  mlib_VideoInterpX_U8_U8_8x4(dst_u+dd, pred_u+dd, stride/2, stride/2);
	  //mlib_VideoInterpX_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	  break;
	case 3:
	  mlib_VideoInterpXY_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	  mlib_VideoInterpXY_U8_U8_8x4(dst_u+dd,pred_u+dd, stride/2, stride/2);
	  //mlib_VideoInterpXY_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	  break;
	}
      } else {      
	switch(half_flags_y) {
	case 0:
	  mlib_VideoCopyRef_U8_U8_16x16(dst_y, pred_y, stride);
	  break;
	case 1:
	  mlib_VideoInterpY_U8_U8_16x16(dst_y, pred_y, stride, stride);
	  break;
	case 2:
	  mlib_VideoInterpX_U8_U8_16x16(dst_y, pred_y, stride, stride);
	  break;
	case 3:
	  mlib_VideoInterpXY_U8_U8_16x16(dst_y, pred_y, stride, stride);
	  break;
	}
	
	switch(half_flags_uv) {
	case 0:
	  mlib_VideoCopyRef_U8_U8_8x8(dst_u, pred_u, stride/2);
	  mlib_VideoCopyRef_U8_U8_8x8(dst_u+dd, pred_u+dd, stride/2);
	  //mlib_VideoCopyRef_U8_U8_8x8(dst_v, pred_v, stride/2);
	  break;
	case 1:
	  mlib_VideoInterpY_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	  mlib_VideoInterpY_U8_U8_8x8(dst_u+dd, pred_u+dd, stride/2, stride/2);
	  //mlib_VideoInterpY_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	  break;
	case 2:
	  mlib_VideoInterpX_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	  mlib_VideoInterpX_U8_U8_8x8(dst_u+dd, pred_u+dd, stride/2, stride/2);
	  //mlib_VideoInterpX_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	  break;
	case 3:
	  mlib_VideoInterpXY_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	  mlib_VideoInterpXY_U8_U8_8x8(dst_u+dd,pred_u+dd, stride/2, stride/2);
	  //mlib_VideoInterpXY_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	  break;
	}
      }
    }
  }
  
  if(mb.modes.macroblock_motion_backward) {
    int vcount, i;

    DPRINTF(2, "backward_motion_comp\n");
    DPRINTF(3, "x: %d, y: %d\n", seq.mb_column, seq.mb_row);
    
    
    vcount = mb.motion_vector_count;  
    for(i = 0; i < vcount; i++) {
      uint8_t *dst_y, *dst_u;//, *dst_v;
      uint8_t *pred_y, *pred_u;//, *pred_v;
      int half_flags_y;
      int half_flags_uv;
      int pred_stride, stride;
      int d;
      
      /* Image/Field select */
      pred_y = bwd_ref_image->y;
      pred_u = bwd_ref_image->u;
      //pred_v = bwd_ref_image->v;
      
      /* Motion vector Origo (block->index) */
      /* Prediction destination (block->index) */
      if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	unsigned int x = seq.mb_column;
	unsigned int y = seq.mb_row;
	pred_y += x * 16 + y * 16 * padded_width;
	pred_u += x * 8 + y * 8 * padded_width/2;
	//pred_v += x * 8 + y * 8 * padded_width/2;
	dst_y = &dst_image->y[x * 16 + y * 16 * padded_width];
	dst_u = &dst_image->u[x * 8 + y * 8 * padded_width/2];
	//dst_v = &dst_image->v[x * 8 + y * 8 * padded_width/2];
      } else {
	unsigned int x = seq.mb_column;
	unsigned int y = seq.mb_row;
	pred_y += x * 16 + y * 16 * padded_width*2;
	pred_u += x * 8 + y * 8 * padded_width;	
	//pred_v += x * 8 + y * 8 * padded_width;	
	dst_y = &dst_image->y[x * 16 + y * 16 * padded_width*2];
	dst_u = &dst_image->u[x * 8 + y * 8 * padded_width];
	//dst_v = &dst_image->v[x * 8 + y * 8 * padded_width];
      }
      
      /* Prediction destination (field) */
      if(i == 1 || 
	 pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) {
	dst_y += padded_width;
	dst_u += padded_width/2;
	//dst_v += padded_width/2;
      }
      
      /* Motion vector offset */
      if(mb.mv_format == MV_FORMAT_FRAME)
	pred_stride = padded_width;
      else
	pred_stride = padded_width * 2;
      
      pred_y += (mb.vector[i][1][0] >> 1) + 
	(mb.vector[i][1][1] >> 1) * pred_stride;
      pred_u += ((mb.vector[i][1][0]/2) >> 1) + 
	((mb.vector[i][1][1]/2) >> 1) * pred_stride/2;
      //pred_v +=  ((mb.vector[i][1][0]/2) >> 1) + 
      //((mb.vector[i][1][1]/2) >> 1) * pred_stride/2;
      
      half_flags_y = 
	((mb.vector[i][1][0] << 1) | (mb.vector[i][1][1] & 1)) & 3;
      half_flags_uv = 
	(((mb.vector[i][1][0]/2) << 1) | ((mb.vector[i][1][1]/2) & 1)) & 3;
	
#if 0 
      /* Check for vectors pointing outside the reference frames */
      if( (int_vec_y[0] < 0) || (int_vec_y[0] > (seq.mb_width - 1) * 16) ||
	  (int_vec_y[1] < 0) || (int_vec_y[1] > (seq.mb_height - 1) * 16) ||
	  (int_vec_uv[0] < 0) || (int_vec_uv[0] > (seq.mb_width - 1) * 8) ||
	  (int_vec_uv[1] < 0) || (int_vec_uv[1] > (seq.mb_height - 1) * 8) ) { 
	fprintf(stderr, "Y (%i,%i), UV (%i,%i)\n", 
		int_vec_y[0], int_vec_y[1], int_vec_uv[0], int_vec_uv[1]);
      }
#endif
      
      /* Field select */
      // ?? Do we realy need mb.mv_format here ?? 
      // This is wrong for Dual Prime... 
      if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
	if(mb.motion_vertical_field_select[i][1]) {
	  pred_y += padded_width;
	  pred_u += padded_width/2;
	  //pred_v += padded_width/2;
	}
      }
      
      if(mb.prediction_type == PRED_TYPE_FIELD_BASED)
	stride = padded_width*2;
      else
	stride = padded_width;
      
      d = bwd_ref_image->v - bwd_ref_image->u;

      DPRINTF(3, "x: %d, y: %d\n", x, y);
    
      if(mb.modes.macroblock_motion_forward) {
	if(vcount == 2) {
	  switch(half_flags_y) {
	  case 0:
	    mlib_VideoCopyRefAve_U8_U8_16x8(dst_y, pred_y, stride);
	    break;
	  case 1:
	    mlib_VideoInterpAveY_U8_U8_16x8(dst_y, pred_y, stride, stride);
	    break;
	  case 2:
	    mlib_VideoInterpAveX_U8_U8_16x8(dst_y, pred_y, stride, stride);
	    break;
	  case 3:
	    mlib_VideoInterpAveXY_U8_U8_16x8(dst_y, pred_y, stride, stride);
	    break;
	  }
	
	  switch(half_flags_uv) {
	  case 0:
	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_u, pred_u, stride/2);
	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_u+d, pred_u+d, stride/2);
	    //mlib_VideoCopyRefAve_U8_U8_8x4(dst_v, pred_v, stride/2);
	    break;
	  case 1:
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_u+d,pred_u+d,stride/2,stride/2);
    //mlib_VideoInterpAveY_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 2:
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_u+d,pred_u+d,stride/2,stride/2);
    //mlib_VideoInterpAveX_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 3:
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_u+d,pred_u+d,stride/2,stride/2);
    //mlib_VideoInterpAveXY_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	    break;
	  }
	} else {
	  switch(half_flags_y) {
	  case 0:
	    mlib_VideoCopyRefAve_U8_U8_16x16(dst_y, pred_y, stride);
	    break;
	  case 1:
	    mlib_VideoInterpAveY_U8_U8_16x16(dst_y, pred_y, stride, stride);
	    break;
	  case 2:
	    mlib_VideoInterpAveX_U8_U8_16x16(dst_y, pred_y, stride, stride);
	    break;
	  case 3:
	    mlib_VideoInterpAveXY_U8_U8_16x16(dst_y, pred_y, stride, stride);
	    break;
	  }
	
	  switch(half_flags_uv) {
	  case 0:
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_u, pred_u, stride/2);
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_u+d, pred_u+d, stride/2);
	    //mlib_VideoCopyRef_U8_U8_8x8(dst_v, pred_v, stride/2);
	    break;
	  case 1:
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_u+d,pred_u+d,stride/2,stride/2);
    //mlib_VideoInterpAveY_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 2:
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_u+d,pred_u+d,stride/2,stride/2);
    //mlib_VideoInterpAveX_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 3:
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_u+d,pred_u+d,stride/2,stride/2);
    //mlib_VideoInterpAveXY_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	    break;
	  }
	}
      } else {
	if(vcount == 2) {
	  switch(half_flags_y) {
	  case 0:
	    mlib_VideoCopyRef_U8_U8_16x8(dst_y, pred_y, stride);
	    break;
	  case 1:
	    mlib_VideoInterpY_U8_U8_16x8(dst_y, pred_y, stride, stride);
	    break;
	  case 2:
	    mlib_VideoInterpX_U8_U8_16x8(dst_y, pred_y, stride, stride);
	    break;
	  case 3:
	    mlib_VideoInterpXY_U8_U8_16x8(dst_y, pred_y, stride, stride);
	    break;
	  }
	
	  switch(half_flags_uv) {
	  case 0:
	    mlib_VideoCopyRef_U8_U8_8x4(dst_u, pred_u, stride/2);
	    mlib_VideoCopyRef_U8_U8_8x4(dst_u+d, pred_u+d, stride/2);
	    //mlib_VideoCopyRef_U8_U8_8x4(dst_v, pred_v, stride/2);
	    break;
	  case 1:
	    mlib_VideoInterpY_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpY_U8_U8_8x4(dst_u+d, pred_u+d, stride/2, stride/2);
	    //mlib_VideoInterpY_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 2:
	    mlib_VideoInterpX_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpX_U8_U8_8x4(dst_u+d, pred_u+d, stride/2, stride/2);
	    //mlib_VideoInterpX_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 3:
	    mlib_VideoInterpXY_U8_U8_8x4(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpXY_U8_U8_8x4(dst_u+d, pred_u+d, stride/2,stride/2);
	    //mlib_VideoInterpXY_U8_U8_8x4(dst_v, pred_v, stride/2, stride/2);
	    break;
	  }
	} else {
	  switch(half_flags_y) {
	  case 0:
	    mlib_VideoCopyRef_U8_U8_16x16(dst_y, pred_y, stride);
	    break;
	  case 1:
	    mlib_VideoInterpY_U8_U8_16x16(dst_y, pred_y, stride, stride);
	    break;
	  case 2:
	    mlib_VideoInterpX_U8_U8_16x16(dst_y, pred_y, stride, stride);
	    break;
	  case 3:
	    mlib_VideoInterpXY_U8_U8_16x16(dst_y, pred_y, stride, stride);
	    break;
	  }
	
	  switch(half_flags_uv) {
	  case 0:
	    mlib_VideoCopyRef_U8_U8_8x8(dst_u, pred_u, stride/2);
	    mlib_VideoCopyRef_U8_U8_8x8(dst_u+d, pred_u+d, stride/2);
	    //mlib_VideoCopyRef_U8_U8_8x8(dst_v, pred_v, stride/2);
	    break;
	  case 1:
	    mlib_VideoInterpY_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpY_U8_U8_8x8(dst_u+d, pred_u+d, stride/2, stride/2);
	    //mlib_VideoInterpY_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 2:
	    mlib_VideoInterpX_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpX_U8_U8_8x8(dst_u+d, pred_u+d, stride/2, stride/2);
	    //mlib_VideoInterpX_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	    break;
	  case 3:
	    mlib_VideoInterpXY_U8_U8_8x8(dst_u, pred_u, stride/2, stride/2);
	    mlib_VideoInterpXY_U8_U8_8x8(dst_u+d, pred_u+d, stride/2,stride/2);
	    //mlib_VideoInterpXY_U8_U8_8x8(dst_v, pred_v, stride/2, stride/2);
	    break;
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
      if(i == 4)
	dst = &dst_image->u[x * 8 + y * 8 * stride];
      else // i == 5
	dst = &dst_image->v[x * 8 + y * 8 * stride];
    }
    if (pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD)
      dst += stride/2;
  }
  
  mlib_VideoAddBlock_U8_S16(dst, mb.QFS, stride);
}
