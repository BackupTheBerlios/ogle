/* Ogle - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort, Martin Norbäck
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


#ifdef HAVE_MLIB
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>
#else /* ! HAVE_MLIB */
#ifdef HAVE_MMX
#include "mmx.h"
#endif /* HAVE_MMX */
#include "c_mlib.h"
#endif /* ! HAVE_MLIB */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/types.h>

#include <glib.h>

#include "common.h"
#include "video_types.h"
#include "yuv2rgb.h"

#define SUNXIL_WARNING_DISABLE
#include <xil/xil.h>

static XilSystemState state;
static XilImage display_image, render_image, three_band_image;
static int image_width, image_height;
static int resized;
static float horizontal_scale, vertical_scale;


extern buf_ctrl_head_t *buf_ctrl_head;

static XVisualInfo vinfo;
Display *mydisplay;
Window window;
GC mygc;
int bpp, mode;

void display_init(int padded_width, int padded_height,
		  int horizontal_size, int vertical_size)
{
  int screen;
  char *hello = "I loathe XIL";
  XSizeHints hint;
  XEvent xev;

  XGCValues xgcv;
  Colormap theCmap;
  XWindowAttributes attribs;
  XSetWindowAttributes xswa;
  unsigned long xswamask;

  mydisplay = XOpenDisplay(NULL);

  if (mydisplay == NULL)
    fprintf(stderr,"Can not open display\n");

  screen = DefaultScreen(mydisplay);

  hint.x = 0;
  hint.y = 0;
  hint.width = horizontal_size;
  hint.height = vertical_size;
  hint.flags = PPosition | PSize;

  /* Make the window */
  XGetWindowAttributes(mydisplay, DefaultRootWindow(mydisplay), &attribs);
  bpp = attribs.depth;
  if (bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) {
    fprintf(stderr,"Only 15,16,24, and 32bpp supported. Trying 24bpp!\n");
    bpp = 24;
  }
  
  XMatchVisualInfo(mydisplay,screen,bpp,TrueColor,&vinfo);
  printf("visual id is  %lx\n",vinfo.visualid);

  theCmap   = XCreateColormap(mydisplay, RootWindow(mydisplay,screen), 
			      vinfo.visual, AllocNone);

  xswa.background_pixel = 0;
  xswa.border_pixel     = 1;
  xswa.colormap         = theCmap;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  window = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
			 hint.x, hint.y, hint.width, hint.height, 
			 0, bpp, CopyFromParent, vinfo.visual, 
			 xswamask, &xswa);

  resized = 0;

  XSelectInput(mydisplay, window, StructureNotifyMask);

  /* Tell other applications about this window */
  XSetStandardProperties(mydisplay, window, 
			 hello, hello, None, NULL, 0, &hint);

  /* Map window. */
  XMapWindow(mydisplay, window);
  
  // This doesn't work correctly
  /* Wait for map. */
  do {
    XNextEvent(mydisplay, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != window);
  
  XFlush(mydisplay);
  XSync(mydisplay, False);
   
  /* Create the colormaps. */   
  mygc = XCreateGC(mydisplay, window, 0L, &xgcv);

  bpp = 32;
   
  yuv2rgb_init(bpp, MODE_BGR);

  /* Setup XIL */
  state = xil_open ();
  if (state == NULL) {
    exit (1);
  }

  /* Create display and render images */
  display_image = xil_create_from_window (state, mydisplay, window);
  //xil_set_synchronize (display_image, 1);

  render_image = xil_create (state, horizontal_size, vertical_size, 
                             4, XIL_BYTE);
  /* Create a child image of render image, for use in xil_copy */
  three_band_image = xil_create_child(render_image, 0, 0, 
                                      horizontal_size, vertical_size, 1, 3);
  image_width = horizontal_size;
  image_height = vertical_size;
  
  return;
}

void display_exit () {
}

void resize () {
  Window root;
  int x, y;
  unsigned int w, h, b, d;
  
  XGetGeometry (mydisplay, window, &root, &x, &y, &w, &h, &b, &d);

  /* Calculate scale factors */
  if (w == image_width && h == image_height) {
    resized = 0;
    horizontal_scale = 1.0;
    vertical_scale = 1.0;
  } else {
    resized = 1;
    horizontal_scale = ((float) w)/image_width;
    vertical_scale = ((float) h)/image_height;
  }

  /* Stop events temporarily, while creating new display_image */
  XSelectInput(mydisplay, window, NoEventMask);

  /* Create new display_image */
  xil_destroy(display_image);
  display_image = xil_create_from_window(state, mydisplay, window);
  //xil_set_synchronize(display_image, 1);

  /* Turn on events */
  XSelectInput (mydisplay, window, StructureNotifyMask);
}


void display(yuv_image_t *image)
{
  XilMemoryStorage storage;
  XEvent event;

  xil_export(render_image);
  xil_get_memory_storage(render_image, &storage);
  yuv2rgb(storage.byte.data, image->y, image->u, image->v,
	  image->padded_width,
	  image->padded_height, 
	  image->padded_width*(bpp/8),
	  image->padded_width, image->padded_width/2 );
  xil_import(render_image, TRUE);
  if (resized) {
    xil_scale(three_band_image, display_image, "bilinear",
               horizontal_scale, vertical_scale);
  } else {
    xil_copy(three_band_image, display_image);
  }
  xil_toss(three_band_image);
  xil_toss(render_image);;
  
  /* Check if scale factors need recalculating */
  if (XCheckWindowEvent (mydisplay, window, StructureNotifyMask, &event))
    resize();
}

