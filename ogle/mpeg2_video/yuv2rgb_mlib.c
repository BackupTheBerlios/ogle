/* 
 *  yuv2rgb_mlib.c, Software YUV to RGB coverter
 *
 *
 *  Copyright (C) 1999, H�kan Hjort <d95hjort@dtek.chalmers.se>
 *  All Rights Reserved. 
 *
 *  This file is part of mpeg2_video, a free MPEG-2 video stream decoder.
 *      
 *  mpeg2_video is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2_video is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "yuv2rgb.h"
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>


void mlib_YUV2ARGB420_32(uint8_t* image, const uint8_t* py, const uint8_t* pu,
			 const uint8_t* pv, const int h_size,
			 const int v_size, const int rgb_stride,
			 const int y_stride, const int uv_stride) 
{
  mlib_VideoColorYUV2ARGB420(image, py, pu, pv, h_size,
			     v_size, rgb_stride, y_stride, uv_stride);
}

void mlib_YUV2ABGR420_32(uint8_t* image, const uint8_t* py, const uint8_t* pu,
			 const uint8_t* pv, const int h_size,
			 const int v_size, const int rgb_stride,
			 const int y_stride, const int uv_stride)
{
  mlib_VideoColorYUV2ABGR420(image, py, pu, pv, h_size,
			     v_size, rgb_stride, y_stride, uv_stride);
}

void mlib_YUV2RGB420_24(uint8_t* image, const uint8_t* py, const uint8_t* pu,
			const uint8_t* pv, const int h_size,
			const int v_size, const int rgb_stride,
			const int y_stride, const int uv_stride)
{
  mlib_VideoColorYUV2RGB420(image, py, pu, pv, h_size,
			    v_size, rgb_stride, y_stride, uv_stride);
}



yuv2rgb_fun yuv2rgb_mlib_init(int bpp, int mode) 
{  
  fprintf(stderr, "Using mediaLib for colorspace convertion\n");
 
  if( bpp == 24 ) {
    if( mode == MODE_RGB )
      return mlib_YUV2RGB420_24;
  }
  
  if( bpp == 32 ) {
    if( mode == MODE_RGB )
      return mlib_YUV2ARGB420_32;
    else if( mode == MODE_BGR )
      return mlib_YUV2ABGR420_32;
  }
  
  return NULL; // Fallback to C.
}

