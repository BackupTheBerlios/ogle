/* Ogle - A video player
 * Copyright (C) 2000, 2001 Vilhelm Bergman
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

#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>
#include <setjmp.h>
#include <X11/Xlib.h>
#include "common.h"
#include "debug_print.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

extern char *program_name;
static char *new_file(void);

JSAMPLE * jpg_buffer;	/* Points to large array of R,G,B-order data */
int image_height;	/* Number of rows in image */
int image_width;	/* Number of columns in image */

GLOBAL(void)
     write_JPEG_file (char * filename, J_COLOR_SPACE type, int quality,
		      uint16_t x_density, uint16_t y_density)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_compress(&cinfo);

  if ((outfile = fopen(filename, "wb")) == NULL) {
    FATAL("can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  cinfo.image_width = image_width; 	/* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = type;          /* colorspace of input image */

  jpeg_set_defaults(&cinfo);

  cinfo.density_unit = 0;
  cinfo.X_density = x_density;
  cinfo.Y_density = y_density;

  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  jpeg_start_compress(&cinfo, TRUE);

  row_stride = image_width * 3;	/* JSAMPLEs per row in jpg_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = & jpg_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  jpeg_finish_compress(&cinfo);
  fclose(outfile);
  jpeg_destroy_compress(&cinfo);
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

METHODDEF(void)
     my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}



void screenshot_rgb_jpg(unsigned char *data,
			unsigned int width, unsigned int height,
			int sar_frac_n, int sar_frac_d) {
  int x,y;
  char *file = new_file(); //support for progressive screenshots

  image_height = height; /* Number of rows in image */
  image_width  = width;	/* Number of columns in image */  

  
  DNOTE("screenshot_rgb_jpg()\n");
  jpg_buffer = (JSAMPLE*)malloc(sizeof(JSAMPLE)
				* image_height * image_width * 3);
  
  if(jpg_buffer == NULL) {
    FATAL("FEL!\n");
    exit(1);
  }
  
  for(y=0; y < image_height ; y++) {
    for(x=0; x < image_width; x++) {
      jpg_buffer[y * image_width * 3 + x*3 +0 ] = 
	data [y * image_width * 4 + x*4 +3 ];

      jpg_buffer[y * image_width * 3 + x*3 +1 ] = 
	data [y * image_width * 4 + x*4 +2 ]; 
      
      jpg_buffer[y * image_width * 3 + x*3 +2 ] = 
	data [y * image_width * 4 + x*4 +1 ] ;
    }
  }
  write_JPEG_file (file, JCS_RGB, 100, sar_frac_n, sar_frac_d);
}



void screenshot_yuv_jpg(yuv_image_t *yuv_data, XImage *ximg,
			int sar_frac_n, int sar_frac_d) {
  int x,y;
  char *file = new_file(); //support for progressive screenshots
  
  uint8_t* py = yuv_data->y;
  uint8_t* pu = yuv_data->u; 
  uint8_t* pv = yuv_data->v; 
  
  //ximg->height; /* Number of rows in image */
  //ximg->width;	/* Number of columns in image */  

  image_height = yuv_data->info->picture.vertical_size;
  image_width  = yuv_data->info->picture.horizontal_size;
  
  DNOTE("screenshot_yuv_jpg()\n");
  jpg_buffer = (JSAMPLE*)malloc(sizeof(JSAMPLE)
				* image_height * image_width * 3);
  
  if(jpg_buffer == NULL) {
    FATAL("FEL!\n");
    exit(1);
  }
  
  
  for(y=0; y < image_height ; y++) {
    for(x=0; x < image_width; x++) {
      jpg_buffer[y * image_width * 3 + x*3 +0 ] = (*py++);
    }
  }

  for(y=0; y < image_height ; y+=2) {
    for(x=0; x < image_width; x+=2) {
      
      jpg_buffer[y * image_width * 3 + x*3 +1 ] = (*pu);
      jpg_buffer[y * image_width * 3 + (x+1)*3 +1 ] = (*pu);
      jpg_buffer[(y+1) * image_width * 3 + x*3 +1 ] = (*pu);
      jpg_buffer[(y+1) * image_width * 3 + (x+1)*3 +1 ] = (*pu++);
      
      jpg_buffer[y     * image_width * 3 + x*3 +2 ] = (*pv);
      jpg_buffer[y     * image_width * 3 + (x+1)*3 +2 ] = (*pv);
      jpg_buffer[(y+1) * image_width * 3 + x*3 +2 ] = (*pv);
      jpg_buffer[(y+1) * image_width * 3 + (x+1)*3 +2 ] = (*pv++);
    }
  }
  write_JPEG_file ( file, JCS_YCbCr, 100, sar_frac_n, sar_frac_d);
}

static char *new_file(void) {
    char *pre = "shot";
    static int i = 0;
    int fd;
    static char full_name[20];

    while(i < 10000) {
        i++;
        snprintf(full_name, sizeof(full_name), "%s%04i.jpg", pre, i);
        fd = open(full_name, O_EXCL | O_CREAT, 0644);
        if(fd != -1) { 
	  close(fd); 
	  return full_name; 
	}
	if(errno != EEXIST) { 
	  return "screenshot.jpg"; 
	} 
    }
    return full_name;
}
