#include <stdio.h>
#include <stdlib.h>

#include "jpeglib.h"
#include <setjmp.h>
#include <X11/Xlib.h>

JSAMPLE * jpg_buffer;	/* Points to large array of R,G,B-order data */
int image_height;	/* Number of rows in image */
int image_width;	/* Number of columns in image */

GLOBAL(void)
     write_JPEG_file (char * filename, int quality)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_compress(&cinfo);

  if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  cinfo.image_width = image_width; 	/* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */

  jpeg_set_defaults(&cinfo);

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



void screenshot_jpg(unsigned char *data, XImage *ximg) {
  int x,y;
  image_height = ximg->height; /* Number of rows in image */
  image_width  = ximg->width;	/* Number of columns in image */  
  
  fprintf(stderr, "screenshot_jpg()\n");
  jpg_buffer = (JSAMPLE*)malloc(sizeof(JSAMPLE)
				* image_height * image_width * 3);
  
  if(jpg_buffer == NULL) {
    fprintf(stderr, "FEL!\n");
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
  write_JPEG_file ("sune", 100);
}
