/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort
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
#include <mlib_image.h>
#else /* ! HAVE_MLIB */
#ifdef HAVE_MMX
#include "mmx.h"
#endif /* HAVE_MMX */
#include "c_mlib.h"
#endif /* ! HAVE_MLIB */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "common.h"
#include "video_types.h"
#include "yuv2rgb.h"
#include "screenshot.h"

#define SPU
#ifdef SPU
#include "spu_mixer.h"
#endif
#ifdef HAVE_XV
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
static XvPortID xv_port;
static XvImage *xv_image;
static unsigned int xv_version;
static unsigned int xv_release;
static unsigned int xv_request_base;
static unsigned int xv_event_base;
static unsigned int xv_error_base;
static unsigned int xv_num_adaptors;
static unsigned int xv_num_formats;
static XvAdaptorInfo *xv_adaptor_info;
static XvImageFormatValues *xv_formats;
static int xv_id;
#endif /* HAVE_XV */

//ugly hack
#ifdef HAVE_XV
extern yuv_image_t *reserv_image;
#endif

static int CompletionType;

typedef struct {
  Window win;
  unsigned char *data;
  XImage *ximage;
  yuv_image_t *image;
} window_info;

static window_info window;


static XVisualInfo vinfo;
static XShmSegmentInfo shm_info;
static Display *mydisplay;
static GC mygc;
static char title[100];
static int color_depth, pixel_stride, mode;


static int screenshot = 0;
static int scaled_image_width;
static int scaled_image_height;
#ifdef HAVE_MLIB
static int scalemode = MLIB_BILINEAR;
#endif /* HAVE_MLIB */

static int dpy_sar_frac_n;
static int dpy_sar_frac_d;

static int use_xshm = 1;
static int use_xv = 0;

static int manually_resized = 0;


extern int msgqid;
extern yuv_image_t *image_bufs;

extern void display_process_exit(void);


static void draw_win_xv(window_info *dwin);
static void draw_win_x11(window_info *dwin);
static void display_change_size(yuv_image_t *img, int new_width, 
				int new_height, int resize_window);


static unsigned long req_serial;
static int (*prev_xerrhandler)(Display *dpy, XErrorEvent *ev);

static int xshm_errorhandler(Display *dpy, XErrorEvent *ev)
{
  if(ev->serial == req_serial) {
    /* this is an error to the xshmattach request 
     * we assume that xshm doesn't work,
     * eg we are using a remote display
     */
    use_xshm = 0;
    fprintf(stderr, "*vo: Disabling Xshm\n");
    return 0;
  } else {
    /* if we get another error we should handle it, 
     * so we give it to the previous errorhandler
     */
    fprintf(stderr, "*vo: unexpected error\n");
    return prev_xerrhandler(dpy, ev);
  }
}

Window display_init(yuv_image_t *picture_data,
		    data_buf_head_t *picture_data_head,
		    char *picture_buf_base)
{
  int screen;

  XSizeHints hint;
  XEvent xev;
  XGCValues xgcv;
  Colormap theCmap;
  XWindowAttributes attribs;
  XSetWindowAttributes xswa;
  unsigned long xswamask;
  struct {
    int width;
    int height;
    int horizontal_pixels;
    int vertical_pixels;
  } dpy_size;
  int sar_frac_n, sar_frac_d;
  int64_t scale_frac_n, scale_frac_d;

  mydisplay = XOpenDisplay(NULL);

  if(mydisplay == NULL)
    fprintf(stderr,"Can not open display\n");

  /* Check for availability of shared memory */
  if(!XShmQueryExtension(mydisplay)) {
    fprintf(stderr, "No shared memory available!\n");
    exit(1);
  }

  screen = DefaultScreen(mydisplay);
  
  dpy_size.width = DisplayWidthMM(mydisplay, screen);
  dpy_size.height = DisplayHeightMM(mydisplay, screen);
  
  dpy_size.horizontal_pixels = DisplayWidth(mydisplay, screen);
  dpy_size.vertical_pixels = DisplayHeight(mydisplay, screen);
  
  dpy_sar_frac_n =
    (int64_t)dpy_size.height * (int64_t)dpy_size.horizontal_pixels;
  dpy_sar_frac_d =
    (int64_t)dpy_size.width * (int64_t)dpy_size.vertical_pixels;
  
  fprintf(stderr, "*** h: %d, w: %d, hp: %d, wp: %d\n",
	  DisplayHeightMM(mydisplay, screen),
	  DisplayWidthMM(mydisplay, screen),
	  DisplayHeight(mydisplay, screen),
	  DisplayWidth(mydisplay, screen));
  fprintf(stderr, "*** display_sar: %d/%d\n", dpy_sar_frac_n, dpy_sar_frac_d);
  
  
  // TODO replace image->sar.. with image->dar
  sar_frac_n = picture_data->info->picture.sar_frac_n;
  sar_frac_d = picture_data->info->picture.sar_frac_d;

  scale_frac_n = (int64_t)dpy_sar_frac_n * (int64_t)sar_frac_d; 
  scale_frac_d = (int64_t)dpy_sar_frac_d * (int64_t)sar_frac_n;
    
  fprintf(stderr, "resize: sar: %d/%d, dpy_sar %d/%d, scale: %lld, %lld\n",
	  sar_frac_n, sar_frac_d,
	  dpy_sar_frac_n, dpy_sar_frac_d,
	  scale_frac_n, scale_frac_d); 
    
  if(scale_frac_n > scale_frac_d) {
    scaled_image_width = (picture_data->info->picture.horizontal_size *
			  scale_frac_n) / scale_frac_d;
    scaled_image_height = picture_data->info->picture.vertical_size;
  } else {
    scaled_image_width = picture_data->info->picture.horizontal_size;
    scaled_image_height = (picture_data->info->picture.vertical_size *
			   scale_frac_d) / scale_frac_n;
  }
  
  /* If we cant scale (i.e no Xv or mediaLib) reset the values. */
#ifndef HAVE_MLIB
  if(!use_xv) {
    scaled_image_width = picture_data->info->picture.horizontal_size;
    scaled_image_height = picture_data->info->picture.vertical_size;
  }
#endif
  
  fprintf(stderr, "vo: image width: %d, height: %d\n",
	  scaled_image_width,
	  scaled_image_height);

  hint.x = 0;
  hint.y = 0;
  hint.width = scaled_image_width;
  hint.height = scaled_image_height;
  hint.flags = PPosition | PSize;


  
  /* Make the window */
  XGetWindowAttributes(mydisplay, DefaultRootWindow(mydisplay), &attribs);
  color_depth = attribs.depth;
  if (color_depth != 15 && color_depth != 16 && 
      color_depth != 24 && color_depth != 32) {
    fprintf(stderr,"Only 15,16,24, and 32bpp supported. Trying 24bpp!\n");
    color_depth = 24;
  }
  
  XMatchVisualInfo(mydisplay, screen, color_depth, TrueColor, &vinfo);
  printf("visual id is  %lx\n", vinfo.visualid);

  theCmap   = XCreateColormap(mydisplay, RootWindow(mydisplay,screen), 
			      vinfo.visual, AllocNone);

  xswa.background_pixel = 0;
  xswa.border_pixel     = 1;
  xswa.colormap         = theCmap;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  window.win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
			     hint.x, hint.y, hint.width, hint.height, 
			     4, color_depth, CopyFromParent, vinfo.visual, 
			     xswamask, &xswa);
  
  //fprintf(stderr, "Window id: 0x%lx\n", window.win);
  
  XSelectInput(mydisplay, window.win, StructureNotifyMask | ExposureMask);

  /* Tell other applications about this window? */
  
  snprintf(&title[0], 99, "Ogle v%s", VERSION);

  XSetStandardProperties(mydisplay, window.win, 
			 &title[0], &title[0], None, NULL, 0, &hint);

  /* Map window. */
  XMapWindow(mydisplay, window.win);
  
  // This doesn't work correctly
  /* Wait for map. */
  do {
    XNextEvent(mydisplay, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != window.win);
  
  //   XSelectInput(mydisplay, mywindow, NoEventMask);

  XSync(mydisplay, False);
   
  /* Create the colormaps. */   
  mygc = XCreateGC(mydisplay, window.win, 0L, &xgcv);
   
#ifdef HAVE_XV
  /* This section of the code looks for the Xv extension for hardware
   * yuv->rgb and scaling. If it is not found, or any suitable adapter
   * is not found, it just falls through to the other code. Otherwise it
   * returns
   *
   * The variable xv_port tells if Xv is used */
  {
    int result;

    xv_port = 0; /* We have no port yet. */

    /* Check for the Xvideo extension */
    result = XvQueryExtension(mydisplay, &xv_version, &xv_release, 
			      &xv_request_base, &xv_event_base, 
			      &xv_error_base);
    if(result == Success) {
      fprintf(stderr, "Found Xv extension, checking for suitable adaptors\n");
      /* Check for available adaptors */
      result = XvQueryAdaptors(mydisplay, DefaultRootWindow (mydisplay), 
			       &xv_num_adaptors, &xv_adaptor_info);
      if(result == Success) {
        int i, j;

        /* Check adaptors */
        for(i = 0; i < xv_num_adaptors; i++) {
          if((xv_adaptor_info[i].type & XvInputMask) &&
	     (xv_adaptor_info[i].type & XvImageMask)) { 
            xv_port = xv_adaptor_info[i].base_id;
            fprintf(stderr, 
                    "Found adaptor \"%s\" checking for suitable formats\n",
                    xv_adaptor_info[i].name);
            /* Check image formats of adaptor */
            xv_formats = XvListImageFormats(mydisplay, xv_port, 
					    &xv_num_formats);
            for(j = 0; j < xv_num_formats; j++) {
	      if(xv_formats[j].id == 0x32315659) { /* YV12 */
	      //if(xv_formats[j].id == 0x30323449) { /* I420 */
                fprintf(stderr, "Found image format \"%s\", using it\n", 
			xv_formats[j].guid);
                xv_id = xv_formats[j].id;
                break;
              } 
            }
            if(j != xv_num_formats) { /* Found matching format */
              fprintf(stderr, "Using Xvideo port %li for hw scaling\n", 
		      xv_port);
              /* allocate XvImages */
              xv_image = XvShmCreateImage(mydisplay, xv_port, xv_id, NULL,
					  picture_data->info->picture.padded_width,
					  picture_data->info->picture.padded_height, 
					  &shm_info);
              shm_info.shmid = picture_data_head->shmid;
              shm_info.shmaddr = picture_buf_base;
	      
              shm_info.readOnly = True;

	      xv_image->data = picture_data->y;

	      /* make sure we don't have any unhandled errors */
	      XSync(mydisplay, False);
	      
	      /* set error handler so we can check if xshmattach failed */
	      prev_xerrhandler = XSetErrorHandler(xshm_errorhandler);
	      
	      /* get the serial of the xshmattach request */
	      req_serial = NextRequest(mydisplay);
	      
	      /* try to attach */
              XShmAttach(mydisplay, &shm_info);
	      
	      /* make sure xshmattach has been processed and any errors
		 have been returned to us */
	      XSync(mydisplay, False);
	      
	      /* revert to the previous xerrorhandler */
	      XSetErrorHandler(prev_xerrhandler);

	      if(use_xshm) {
		shmctl(shm_info.shmid, IPC_RMID, 0);
		CompletionType = XShmGetEventBase(mydisplay) + ShmCompletion;
	      }
	      return; /* All set up! */
            } else {
              xv_port = 0;
            }
          }
        }
      }
    }
    if(xv_port != 0)
      use_xv = 1;
  }
#endif /* HAVE_XV */

  CompletionType = XShmGetEventBase(mydisplay) + ShmCompletion;  
  /* Create shared memory image */
  if((scaled_image_width * scaled_image_height) 
     < (picture_data->info->picture.padded_width 
	* picture_data->info->picture.padded_height)) {
    window.ximage = XShmCreateImage(mydisplay, vinfo.visual, color_depth,
				    ZPixmap, NULL, &shm_info,
				    picture_data->info->picture.padded_width,
				    picture_data->info->picture.padded_height);
  } else {
    window.ximage = XShmCreateImage(mydisplay, vinfo.visual, color_depth,
				    ZPixmap, NULL, &shm_info,
				    scaled_image_width,
				    scaled_image_height);
  }
  
  if(window.ximage == NULL) {
    fprintf(stderr, "Shared memory: couldn't create Shm image\n");
    goto shmemerror;
  }
  
  /* Get a shared memory segment */
  shm_info.shmid = shmget(IPC_PRIVATE,
			  window.ximage->bytes_per_line * 
			  window.ximage->height, 
			  IPC_CREAT | 0777);
  
  if(shm_info.shmid < 0) {
    fprintf(stderr, "Shared memory: Couldn't get segment\n");
    goto shmemerror;
  }
  
  
  /* Attach shared memory segment */
  shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);

  if(shm_info.shmaddr == ((char *) -1)) {
    fprintf(stderr, "Shared memory: Couldn't attach segment\n");
    goto shmemerror;
  }
  
  window.ximage->data = shm_info.shmaddr;
  shm_info.readOnly = False;

  /* make sure we don't have any unhandled errors */
  XSync (mydisplay, False);
  
  /* set error handler so we can check if xshmattach failed */
  prev_xerrhandler = XSetErrorHandler(xshm_errorhandler);
  
  /* get the serial of the xshmattach request */
  req_serial = NextRequest(mydisplay);
  
  /* try to attach */
  XShmAttach(mydisplay, &shm_info);

  /* make sure xshmattach has been processed and any errors
     have been returned to us */
  XSync(mydisplay, False);
  
  /* revert to the previous xerrorhandler */
  XSetErrorHandler(prev_xerrhandler);
  
  window.data = window.ximage->data;
  
  pixel_stride = window.ximage->bits_per_pixel;
  // If we have blue in the lowest bit then obviously RGB 
  mode = ((window.ximage->blue_mask & 0x01) != 0) ? 1 : 2;
  /*
#ifdef WORDS_BIGENDIAN 
  if (window.ximage->byte_order != MSBFirst)
#else
  if (window.ximage->byte_order != LSBFirst) 
#endif
  {
    fprintf( stderr, "No support for non-native XImage byte order!\n" );
    exit(1);
  }
  */
  yuv2rgb_init(pixel_stride, mode);
  
  return window.win;
  
 shmemerror:
  exit(1);
}



/* TODO display_change_size needs to be fixed */
static void display_change_size(yuv_image_t *img, int new_width, 
				int new_height, int resize_window) {
  int padded_width = img->info->picture.padded_width;
  int padded_height = img->info->picture.padded_height;
  
  fprintf(stderr, "vo padded: %d, %d\n",
	  padded_width, padded_height);
  
  
  /* If we cant scale (i.e no Xv or mediaLib) exit give up now. */
#ifndef HAVE_MLIB
  if(!use_xv)
    return;
#endif
  
  // Check to not 'reallocate' if the size is the same or less than 
  // minimum size...
  if((new_width < padded_width && new_height < padded_height) ||
     (scaled_image_width == new_width && scaled_image_height == new_height)){
    scaled_image_width = new_width;
    scaled_image_height = new_height;
    return;
  }
  
  /* Save the new size so we know what to scale to. */
  scaled_image_width = new_width;
  scaled_image_height = new_height;

#if 0
  /* Stop events temporarily, while creating new display_image */
  XSelectInput(mydisplay, window.win, NoEventMask);
#endif

  if(!use_xv && use_xshm) {
    
    XSync(mydisplay,True);

    /* Destroy old display */
    XShmDetach(mydisplay, &shm_info);
    XDestroyImage(window.ximage);
    shmdt(shm_info.shmaddr);
    if(shm_info.shmaddr == ((char *) -1)) {
      fprintf(stderr, "Shared memory: Couldn't detach segment\n");
      exit(-10);
    }
    if(shmctl(shm_info.shmid, IPC_RMID, 0) == -1) {
      perror("shmctl ipc_rmid");
      exit(-10);
    }
    XSync(mydisplay, True);
    
    memset( &shm_info, 0, sizeof( XShmSegmentInfo ) );
    
    /* Create new display */
    window.ximage = XShmCreateImage(mydisplay, vinfo.visual, 
					color_depth, ZPixmap, 
					NULL, &shm_info,
					scaled_image_width,
					scaled_image_height);
    
    if(window.ximage == NULL) {
      fprintf(stderr, "Shared memory: couldn't create Shm image\n");
      exit(-10);
    }
    
    /* Get a shared memory segment */
    shm_info.shmid = shmget(IPC_PRIVATE,
			    window.ximage->bytes_per_line * 
			    window.ximage->height, 
			    IPC_CREAT | 0777);
    
    if(shm_info.shmid < 0) {
      fprintf(stderr, "Shared memory: Couldn't get segment\n");
      exit(-10);
    }
  
    /* Attach shared memory segment */
    shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
    if(shm_info.shmaddr == ((char *) -1)) {
      fprintf(stderr, "Shared memory: Couldn't attach segment\n");
      exit(-10);
    }
    
    window.ximage->data = shm_info.shmaddr;
    window.data = window.ximage->data;
    
    shm_info.readOnly = False;
    XShmAttach(mydisplay, &shm_info);
    
    XSync(mydisplay, 0);
  }

  if(resize_window == True) {
    /* Force a change of the widow size. */
    XResizeWindow(mydisplay, window.win, 
		  scaled_image_width, scaled_image_height);
  }
#if 0
  /* Turn on events */
  XSelectInput(mydisplay, window.win, StructureNotifyMask | ExposureMask);
#endif
}


void display_exit(void) 
{
  // Need to add some test to se if we can detatch/free/destroy things
  if(use_xshm) {
    XShmDetach(mydisplay, &shm_info);
    XDestroyImage(window.ximage);
    shmdt(shm_info.shmaddr);
    shmctl(shm_info.shmid, IPC_RMID, 0);
  }
  display_process_exit();
}



 

Bool true_predicate(Display *dpy, XEvent *ev, XPointer arg)
{
    return True;
}

void display(yuv_image_t *current_image)
{
  XEvent ev;
  static int sar_frac_n, sar_frac_d; 
  int64_t scale_frac_n, scale_frac_d;
  
  if(current_image->info->picture.sar_frac_n != sar_frac_n ||
     current_image->info->picture.sar_frac_d != sar_frac_d) {
    
    int new_width, new_height;

    sar_frac_n = current_image->info->picture.sar_frac_n;
    sar_frac_d = current_image->info->picture.sar_frac_d;

    // TODO replace image->sar.. with image->dar
    scale_frac_n = (int64_t)dpy_sar_frac_n * (int64_t)sar_frac_d; 
    scale_frac_d = (int64_t)dpy_sar_frac_d * (int64_t)sar_frac_n;
    
    fprintf(stderr, "resize: sar: %d/%d, dpy_sar %d/%d, scale: %lld, %lld\n",
	    sar_frac_n, sar_frac_d,
	    dpy_sar_frac_n, dpy_sar_frac_d,
	    scale_frac_n, scale_frac_d); 
    
    if(scale_frac_n > scale_frac_d) {
      new_width = (current_image->info->picture.horizontal_size *
		   scale_frac_n) / scale_frac_d;
      new_height = current_image->info->picture.vertical_size;
    } else {
      new_width = current_image->info->picture.horizontal_size;
      new_height = (current_image->info->picture.vertical_size *
		    scale_frac_d) / scale_frac_n;
    }

    fprintf(stderr, "vo: resizing to %d x %d\n", new_width, new_height);
    
    display_change_size(current_image, new_width, new_height, True);
    XSync(mydisplay, False);    
  }
  
  
  window.image = current_image;
    
  while(XCheckIfEvent(mydisplay, &ev, true_predicate, NULL) != False) {
    
    switch(ev.type) {
    case Expose:
      if(ev.xexpose.window == window.win)
	//draw_win(&window);
      break;
    case ConfigureNotify:
      while(XCheckTypedWindowEvent(mydisplay, window.win, 
				   ConfigureNotify, &ev) == True) 
	; 
      if(ev.xconfigure.window == window.win) {
	display_change_size(current_image, ev.xconfigure.width, 
			    ev.xconfigure.height, False);
      }
      break;    
    default:
      break;
    }
  }
  
  if(use_xv) { /* Xv found */
    draw_win_xv(&window);
  } else {
    draw_win_x11(&window);
  }

  return;
}







#ifdef HAVE_MLIB
static void draw_win_x11(window_info *dwin)
{
  mlib_image *mimage_s;
  mlib_image *mimage_d;
  
  /* Put the decode rgb data at the end of the data segment. */
  char *address = dwin->data;
  
  int dest_size = dwin->ximage->bytes_per_line * dwin->ximage->height;
  int sorce_size = (dwin->image->info->picture.padded_width*(pixel_stride/8) 
		    * dwin->image->info->picture.padded_height);
  
  int offs = dest_size - sorce_size - 4096;
  if(offs > 0)
    address += offs;
    
  // Because mlib_YUV* reads out of bounds we need to make sure that the end
  // of the picture isn't on the pageboundary for in the last page allocated
  // There is also something strange with the mlib_ImageZoom in NEAREST mode.
			      
  /* We must some how guarantee that the ximage isn't used by X11. 
     Rigth now it's done by the XSync call at the bottom... */
  
  yuv2rgb(address, dwin->image->y, dwin->image->u, dwin->image->v,
	  dwin->image->info->picture.padded_width, 
	  dwin->image->info->picture.padded_height, 
	  dwin->image->info->picture.padded_width*(pixel_stride/8),
	  dwin->image->info->picture.padded_width,
	  dwin->image->info->picture.padded_width/2 );

#ifdef SPU
  if(msgqid != -1) {
    mix_subpicture_rgb(address, dwin->image->info->picture.padded_width,
		       dwin->image->info->picture.padded_height);
  }
#endif

  if((scaled_image_width != dwin->image->info->picture.horizontal_size) ||
     (scaled_image_height != dwin->image->info->picture.vertical_size)) {
    /* Destination image */
    mimage_d = mlib_ImageCreateStruct(MLIB_BYTE, 4,
				      scaled_image_width, 
				      scaled_image_height,
				      dwin->ximage->bytes_per_line, 
				      dwin->data);
    /* Source image */
    mimage_s = mlib_ImageCreateStruct(MLIB_BYTE, 4, 
				      dwin->image->info->picture.horizontal_size, 
				      dwin->image->info->picture.vertical_size,
				      dwin->image->info->picture.padded_width*4, address);
    /* Extra fast 2x Zoom */
    if((scaled_image_width == 2 * dwin->image->info->picture.horizontal_size) &&
       (scaled_image_height == 2 * dwin->image->info->picture.vertical_size)) {
      mlib_ImageZoomIn2X(mimage_d, mimage_s, 
			 scalemode, MLIB_EDGE_DST_FILL_ZERO);
    } else {
      mlib_ImageZoom 
	(mimage_d, mimage_s,
	 (double)scaled_image_width/(double)dwin->image->info->picture.horizontal_size, 
	 (double)scaled_image_height/(double)dwin->image->info->picture.vertical_size,
	 scalemode, MLIB_EDGE_DST_FILL_ZERO);
    }
    mlib_ImageDelete(mimage_s);
    mlib_ImageDelete(mimage_d);
  }
  
  if(screenshot) {
    screenshot = 0;
    screenshot_jpg(dwin->data, dwin->ximage);
  }
  
  if(use_xshm) {
    XShmPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 0, 0, 0, 0, 
		 scaled_image_width, scaled_image_height, 1);
  } else {
    XPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 0, 0, 0, 0,
	      scaled_image_width, scaled_image_height);
  }
  
  //TEST
  XSync(mydisplay, False);
}

#else /* HAVE_MLIB */

static void draw_win_x11(window_info *dwin)
{ 
  yuv2rgb(dwin->data, dwin->image->y, dwin->image->u, dwin->image->v,
          dwin->image->info->picture.padded_width,
          dwin->image->info->picture.padded_height,
          dwin->image->info->picture.padded_width * (pixel_stride/8),
          dwin->image->info->picture.padded_width, 
	  dwin->image->info->picture.padded_width/2 );

#ifdef SPU
  if(msgqid != -1) {
    mix_subpicture_rgb(dwin->data, dwin->image->info->picture.padded_width,
		       dwin->image->info->picture.padded_height);
  }
#endif
            
  if(screenshot) {
    screenshot = 0;
    screenshot_jpg(dwin->data, dwin->ximage);
  }

  if(use_xshm) {
    XShmPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 0, 0, 0, 0, 
		 dwin->image->info->picture.horizontal_size, 
		 dwin->image->info->picture.vertical_size, True);
  } else {
    XPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 0, 0, 0, 0,
	      dwin->image->info->picture.horizontal_size,
	      dwin->image->info->picture.vertical_size);
  }

  //TEST
  XSync(mydisplay, False);
  // XFlush(mydisplay);
}
#endif /* HAVE_MLIB */


static Bool predicate(Display *dpy, XEvent *ev, XPointer arg)
{
  if(ev->type == CompletionType) {
    return True;
  } else {
    return False;
  }
}

static void draw_win_xv(window_info *dwin)
{
#ifdef HAVE_XV
  unsigned char *dst;
  
  dst = xv_image->data;
    
  xv_image->data = dwin->image->y;
    
#ifdef SPU
  if(msgqid != -1) {
    //ugly hack
    if(mix_subpicture_yuv(dwin->image, reserv_image)) {
      xv_image->data = reserv_image->y;
    }
  }
#endif

  XvShmPutImage(mydisplay, xv_port, dwin->win, mygc, xv_image, 
		0, 0, 
		dwin->image->info->picture.horizontal_size, 
		dwin->image->info->picture.vertical_size,
		0, 0, 
		scaled_image_width, scaled_image_height,
		True);
  
  //XFlush(mydisplay); ??
  {
    XEvent ev;
    XEvent *e;
    e = &ev;
    
    /* this is to make sure that we are free to use the image again
       It waits for an XShmCompletionEvent */
    XIfEvent(mydisplay, &ev, predicate, NULL);
  }
#endif /* HAVE_XV */
}

