/* SKROMPF - A video player
 * Copyright (C) 2000 Bj�rn Englund, H�kan Hjort
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



int CompletionType;

typedef struct {
  Window win;
  unsigned char *data;
  XImage *ximage;
  yuv_image_t *image;
  int grid;
  int color_grid;
  macroblock_t *mbs;
} debug_win;

static debug_win windows[1];


static XVisualInfo vinfo;
static XShmSegmentInfo shm_info;
static Display *mydisplay;
//static Window window_stat;
static GC mygc;
//static GC statgc;
static char title[100];
static int color_depth, pixel_stride, mode;


extern unsigned int debug;
static int show_window[1] = {1};
static int run = 1;
static int screenshot = 0;
static int scaled_image_width;
static int scaled_image_height;
#ifdef HAVE_MLIB
static int scalemode = MLIB_BILINEAR;
#endif /* HAVE_MLIB */
static int scalemode_change = 0;
static double sar;
static double xscale_factor;

static int use_xshm = 1;

extern int msgqid;
extern yuv_image_t *image_bufs;

extern void display_process_exit(void);

static void draw_win(debug_win *dwin);
void display_change_size(int new_width, int new_height);


unsigned long req_serial;

int (*prev_xerrhandler)(Display *dpy, XErrorEvent *ev);

int xshm_errorhandler(Display *dpy, XErrorEvent *ev)
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

void display_init(int padded_width, int padded_height,
		  int horizontal_size, int vertical_size,
		  picture_data_elem_t *pinfo,
		  yuv_image_t *picture_data,
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

  mydisplay = XOpenDisplay(NULL);

  if (mydisplay == NULL)
    fprintf(stderr,"Can not open display\n");



  /* Check for availability of shared memory */
  if (!XShmQueryExtension(mydisplay)) {
    fprintf(stderr, "No shared memory available!\n");
    exit(1);
  }


  screen = DefaultScreen(mydisplay);

  sar = ((double)DisplayHeightMM(mydisplay, screen)*(double)DisplayWidth(mydisplay, screen))/((double)DisplayWidthMM(mydisplay, screen)*(double)DisplayHeight(mydisplay, screen));
  xscale_factor = sar/pinfo->picture.sar;
  fprintf(stderr, "*** h: %d, w: %d, hp: %d, wp: %d\n",
	  DisplayHeightMM(mydisplay, screen),
	  DisplayWidthMM(mydisplay, screen),
	  DisplayHeight(mydisplay, screen),
	  DisplayWidth(mydisplay, screen));
  fprintf(stderr, "*** src_sar: %f, dst_sar: %f, xscale: %f\n",
	  pinfo->picture.sar,
	  sar, xscale_factor);


  /* Scale init. */
  if(xscale_factor > 1) {
    scaled_image_width = (int)(((double)horizontal_size)*xscale_factor);
    scaled_image_height = vertical_size;
    if(scaled_image_width != horizontal_size)
       scaled_image_width &= ~1;
  } else {
    scaled_image_width = horizontal_size;
    scaled_image_height = (int)(((double)vertical_size)/xscale_factor);
    if(scaled_image_height != vertical_size)
      scaled_image_height &= ~1;
  }
  
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
  
  XMatchVisualInfo(mydisplay,screen,color_depth,TrueColor,&vinfo);
  printf("visual id is  %lx\n",vinfo.visualid);

  theCmap   = XCreateColormap(mydisplay, RootWindow(mydisplay,screen), 
			      vinfo.visual, AllocNone);

  xswa.background_pixel = 0;
  xswa.border_pixel     = 1;
  xswa.colormap         = theCmap;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  windows[0].win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
				 hint.x, hint.y, hint.width, hint.height, 
				 4, color_depth, CopyFromParent, vinfo.visual, 
				 xswamask, &xswa);

  //fprintf(stderr, "Window id: 0x%lx\n", windows[0].win);
  
  //window_stat = XCreateSimpleWindow(mydisplay, RootWindow(mydisplay,screen),
  //hint.x, hint.y, 200, 200, 0,
  //0, 0);

  XSelectInput(mydisplay, windows[0].win, StructureNotifyMask | ExposureMask);
  //XSelectInput(mydisplay, windows[0].win, StructureNotifyMask | KeyPressMask 
  //| ExposureMask);
  
  //XSelectInput(mydisplay, window_stat, StructureNotifyMask | KeyPressMask 
  //| ButtonPressMask | ExposureMask);

  /* Tell other applications about this window */
  
  snprintf(&title[0], 99, "Mj�lner: 0x%lx", windows[0].win);

  XSetStandardProperties(mydisplay, windows[0].win, 
			 &title[0], &title[0], None, NULL, 0, &hint);
  //XSetStandardProperties(mydisplay, window_stat, 
  //"stat", "stat", None, NULL, 0, &hint);

  /* Map window. */
  //XMapWindow(mydisplay, window_stat);
  XMapWindow(mydisplay, windows[0].win);
  
  
  // This doesn't work correctly
  /* Wait for map. */
  do {
    XNextEvent(mydisplay, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != windows[0].win);
  
  //   XSelectInput(mydisplay, mywindow, NoEventMask);

  XSync(mydisplay, False);
   
  /* Create the colormaps. */   
  mygc = XCreateGC(mydisplay, windows[0].win, 0L, &xgcv);
  //statgc = XCreateGC(mydisplay, window_stat, 0L, &xgcv);
   
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
    result = XvQueryExtension (mydisplay, &xv_version, &xv_release, 
                               &xv_request_base, &xv_event_base, 
                               &xv_error_base);
    if (result == Success) {
      fprintf (stderr, "Found Xv extension, checking for suitable adaptors\n");
      /* Check for available adaptors */
      result = XvQueryAdaptors (mydisplay, DefaultRootWindow (mydisplay), 
                                &xv_num_adaptors, &xv_adaptor_info);
      if (result == Success) {
        int i, j;

        /* Check adaptors */
        for (i = 0; i < xv_num_adaptors; i++) {
          if ((xv_adaptor_info[i].type & XvInputMask) &&
              (xv_adaptor_info[i].type & XvImageMask)) { 
            xv_port = xv_adaptor_info[i].base_id;
            fprintf(stderr, 
                    "Found adaptor \"%s\" checking for suitable formats\n",
                    xv_adaptor_info[i].name);
            /* Check image formats of adaptor */
            xv_formats = XvListImageFormats (mydisplay, xv_port, 
                                             &xv_num_formats);
            for (j = 0; j < xv_num_formats; j++) {
	      if (xv_formats[j].id == 0x32315659) { /* where is this from? */
                fprintf (stderr, "Found image format \"%s\", using it\n", 
                       xv_formats[j].guid);
                xv_id = xv_formats[j].id;
                break;
              } 
            }
            if (j != xv_num_formats) { /* Found matching format */
              fprintf (stderr, "Using Xvideo port %li for hw scaling\n", xv_port);
              /* allocate XvImages */
              xv_image = XvShmCreateImage (mydisplay, xv_port, xv_id, NULL,
                                            padded_width, padded_height, 
					   &shm_info);
              shm_info.shmid = picture_data_head->shmid;

              shm_info.shmaddr = picture_buf_base;

	      
              shm_info.readOnly = True;

	      xv_image->data = picture_data->y;


              XShmAttach (mydisplay, &shm_info);

	      shmctl (shm_info.shmid, IPC_RMID, 0);
	      //memset (xv_image->data, 128, xv_image->data_size); /*grayscale */
	      
	      CompletionType = XShmGetEventBase(mydisplay) + ShmCompletion;
	      
	      return; /* All set up! */
            } else {
              xv_port = 0;
            }
          }
        } 
      } 
    }
  }
#endif /* HAVE_XV */
  CompletionType = XShmGetEventBase(mydisplay) + ShmCompletion;  
  /* Create shared memory image */
  if(scaled_image_width * scaled_image_height < padded_width * padded_height) {
    windows[0].ximage = XShmCreateImage(mydisplay, vinfo.visual, color_depth,
					ZPixmap, NULL, &shm_info,
					padded_width,
					padded_height);
  } else {
    windows[0].ximage = XShmCreateImage(mydisplay, vinfo.visual, color_depth,
					ZPixmap, NULL, &shm_info,
					scaled_image_width,
					scaled_image_height);
  }
  
  if (windows[0].ximage == NULL) {
    fprintf(stderr, "Shared memory: couldn't create Shm image\n");
    goto shmemerror;
  }
  
  /* Get a shared memory segment */
  shm_info.shmid = shmget(IPC_PRIVATE,
			  windows[0].ximage->bytes_per_line * 
			  windows[0].ximage->height, 
			  IPC_CREAT | 0777);
  
  if (shm_info.shmid < 0) {
    fprintf(stderr, "Shared memory: Couldn't get segment\n");
    goto shmemerror;
  }
  
  
  /* Attach shared memory segment */
  shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
  
  

  if (shm_info.shmaddr == ((char *) -1)) {
    fprintf(stderr, "Shared memory: Couldn't attach segment\n");
    goto shmemerror;
  }
  
  windows[0].ximage->data = shm_info.shmaddr;
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
  XSync (mydisplay, False);
  
  /* revert to the previous xerrorhandler */
  XSetErrorHandler(prev_xerrhandler);
  
  windows[0].data = windows[0].ximage->data;
  
  pixel_stride = windows[0].ximage->bits_per_pixel;
  // If we have blue in the lowest bit then obviously RGB 
  mode = ((windows[0].ximage->blue_mask & 0x01) != 0) ? 1 : 2;
  /*
#ifdef WORDS_BIGENDIAN 
  if (windows[0].ximage->byte_order != MSBFirst)
#else
  if (windows[0].ximage->byte_order != LSBFirst) 
#endif
  {
    fprintf( stderr, "No support for non-native XImage byte order!\n" );
    exit(1);
  }
  */
  yuv2rgb_init(pixel_stride, mode);
  
  
  return;
  
 shmemerror:
  exit(1);

}


/* TODO display_change_size needs to be fixed */

void display_change_size(int new_width, int new_height) {
  int padded_width = windows[0].image->info->padded_width;
  int padded_height = windows[0].image->info->padded_height;
  
  // Check to not 'reallocate' if the size is the same or less than 
  // minimum size...
  if( (new_width < padded_width && new_height < padded_height) ||
      (scaled_image_width == new_width && scaled_image_height == new_height) ){
    scaled_image_width = new_width;
    scaled_image_height = new_height;
    return;
  }
  
  /* Save the new size so we know what to scale to. */
  scaled_image_width = new_width;
  scaled_image_height = new_height;
  
  /* Stop events temporarily, while creating new display_image */
  XSelectInput(mydisplay, windows[0].win, NoEventMask);

#ifndef HAVE_XV
  
  XSync(mydisplay,True);

  if(use_xshm) {
    
    /* Destroy old display */
    XShmDetach(mydisplay, &shm_info);
    XDestroyImage(windows[0].ximage);
    shmdt(shm_info.shmaddr);
    if(shm_info.shmaddr == ((char *) -1)) {
      fprintf(stderr, "Shared memory: Couldn't detache segment\n");
      exit(-10);
    }
    if(shmctl(shm_info.shmid, IPC_RMID, 0) == -1) {
      perror("shmctl ipc_rmid");
      exit(-10);
    }
    XSync(mydisplay, True);
    
    memset( &shm_info, 0, sizeof( XShmSegmentInfo ) );
    
    /* Create new display */
    windows[0].ximage = XShmCreateImage(mydisplay, vinfo.visual, 
					color_depth, ZPixmap, 
					NULL, &shm_info,
					scaled_image_width,
					scaled_image_height);
    
    if(windows[0].ximage == NULL) {
      fprintf(stderr, "Shared memory: couldn't create Shm image\n");
      exit(-10);
    }
    
    /* Get a shared memory segment */
    shm_info.shmid = shmget(IPC_PRIVATE,
			    windows[0].ximage->bytes_per_line * 
			    windows[0].ximage->height, 
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
    
    windows[0].ximage->data = shm_info.shmaddr;
    windows[0].data = windows[0].ximage->data;
    
    shm_info.readOnly = False;
    XShmAttach(mydisplay, &shm_info);
    
    XSync(mydisplay, 0);
  }
#endif  
  /* Force a change of the widow size. */
  XResizeWindow(mydisplay, windows[0].win, 
		scaled_image_width, scaled_image_height);
  
  /* Turn on events */
  //XSelectInput(mydisplay, windows[0].win, StructureNotifyMask 
  //| KeyPressMask | ExposureMask);
  XSelectInput(mydisplay, windows[0].win, StructureNotifyMask 
	       | ExposureMask);
}

void display_exit(void) 
{
  // Need to add some test to se if we can detatch/free/destroy things
  if(use_xshm) {
    
    XShmDetach(mydisplay, &shm_info);
    XDestroyImage(windows[0].ximage);
    shmdt(shm_info.shmaddr);
    shmctl(shm_info.shmid, IPC_RMID, 0);
  }
  display_process_exit();
}



 
void add_grid(unsigned char *data, XImage *ximg)
{
  int m,n;
  
  for(m = 0; m < ximg->height; m++) {
    if(m%16 == 0) {
      for(n = 0; n < ximg->width*4; n+=4) {
	data[m*ximg->width*4+n+1] = 127;
	data[m*ximg->width*4+n+2] = 127;
	data[m*ximg->width*4+n+3] = 127;
      }
    } else {
      for(n = 0; n < ximg->width*4; n+=16*4) {
	data[m*ximg->width*4+n+1] = 127;
	data[m*ximg->width*4+n+2] = 127;
	data[m*ximg->width*4+n+3] = 127;
      }
    }
  }
}


void add_2_box_sides(unsigned char *data,
		     unsigned char r, unsigned char g, unsigned char b,
		     XImage *ximg)
{
  int n;
  
  for(n = 0; n < 16*4; n+=4) {
    data[n+1] = b;
    data[n+2] = g;
    data[n+3] = r;
  }
  
  for(n = 0; n < ximg->width*4*16; n+=ximg->width*4) {
    data[n+1] = b;
    data[n+2] = g;
    data[n+3] = r;
  }
  
  return;
}


/* This assumes a 720x480 image */
void add_color_grid(debug_win *win)
{
  int m;
  
  for(m = 0; m < 30*45; m++) {
    if(win->mbs[m].skipped) {

      add_2_box_sides(&(win->data[m/45*win->ximage->width*4*16+(m%45)*16*4]),
		      150, 150, 150, win->ximage);
      
    } else if(win->mbs[m].modes.macroblock_intra) {

      add_2_box_sides(&(win->data[m/45*win->ximage->width*4*16+(m%45)*16*4]),
		      0, 255, 255, win->ximage);
      
    } else if(win->mbs[m].modes.macroblock_motion_forward &&
	      win->mbs[m].modes.macroblock_motion_backward) {
      
      add_2_box_sides(&(win->data[m/45*win->ximage->width*4*16+(m%45)*16*4]),
		      255, 0, 0, win->ximage);
      
    } else if(win->mbs[m].modes.macroblock_motion_forward) {
      
      add_2_box_sides(&(win->data[m/45*win->ximage->width*4*16+(m%45)*16*4]),
		      255, 255, 0, win->ximage);

    } else if(win->mbs[m].modes.macroblock_motion_backward) {
      
      add_2_box_sides(&(win->data[m/45*win->ximage->width*4*16+(m%45)*16*4]),
		      0, 0, 255, win->ximage);
    }
  }
}


Bool true_predicate(Display *dpy, XEvent *ev, XPointer arg)
{
    return True;
}

void display(yuv_image_t *current_image)
{
  XEvent ev;
  int nextframe = 0;  
  
  windows[0].image = current_image;
  
  if(show_window[0]) {
    draw_win(&windows[0]);
  }
  
  while(!nextframe) {

    if(run) {
      nextframe = 1;
      /* Can't use XCheckMaskEvent because of extensions... */
      if(XCheckIfEvent(mydisplay, &ev, true_predicate, NULL) == False) {
	continue;
      }
    } else {
      XNextEvent(mydisplay, &ev);
    }
    
    switch(ev.type) {
    case Expose:
      if(windows[0].win == ev.xexpose.window)
	if(show_window[0])
	  draw_win(&(windows[0]));
      break;
#if defined(HAVE_MLIB) || defined(HAVE_XV)
    case ConfigureNotify:
      if(ev.xconfigure.window == windows[0].win) {
	display_change_size( ev.xconfigure.width, ev.xconfigure.height );
      }
      break;    
#endif /* HAVE_MLIB || HAVE_XV */
    case ButtonPress:
      switch(ev.xbutton.button) {
      case 0x1:
	break;
      case 0x2:
	break;
      case 0x3:
	break;
      }
      break;
#if 0     
    case KeyPress:
      {
	char buff[2];
	static int debug_change = 0;
	KeySym keysym;
	XLookupString(&(ev.xkey), buff, 2, &keysym, NULL);
	buff[1] = '\0';

	switch(keysym) {
	case XK_Up:
	  
	  break;
	case XK_Down:
	  break;
	case XK_Left:
	  break;
	case XK_Right:
	  break;
	default:
	  break;
	}
	
	switch(buff[0]) {
	case 'n':
	  nextframe = 1;
	  break;
	case 'd':
	  debug_change = 1;
	  break;
	case 'w':
	  show_window[0] = !show_window[0];
	  break;
	case 'r':
	  run = !run;
	  nextframe = 1;
	  break;
	case 'p':
	  screenshot = 1;
	  draw_win(&(windows[0]));
	  break;
	case 'q':
	  display_exit();
	  break;
	case 's':
	  scalemode_change = 1;
	  break;
	case '1':
	case '2':
	case '3':
	case '4':
	  if(debug_change) 
	    break; /* Handled below the switch */
	  else if(scalemode_change) {
#ifdef HAVE_MLIB
	    switch(atoi(&buff[0])) {
	    case 1:
	      scalemode = MLIB_NEAREST;
	      break;
	    case 2:
	      scalemode = MLIB_BILINEAR;
	      break;
	    case 3:
	      scalemode = MLIB_BICUBIC;
	      break;
	    case 4:
	      scalemode = MLIB_BICUBIC2;
	      break;
	    default:
	      break;
	    }
#endif
	    scalemode_change = 0;	  
	  }
	  else { /* Scale size */
#if defined(HAVE_MLIB) || defined(HAVE_XV)
	    int x = atoi(&buff[0]);
	    display_change_size(windows[0].image->info->horizontal_size * x, 
				windows[0].image->info->vertical_size * x);
#endif
	  }
	  break;
	default:
	  break;
	} /* end case KeyPress */
	if(debug_change && buff[0] != 'd') {
	  debug = atoi(&buff[0]);
	  debug_change = 0;
	}
      }
      break;
#endif
    default:
      break;
    }
  }

  return;
}

void draw_win_x11(debug_win *dwin)
{ 
  yuv2rgb(dwin->data, dwin->image->y, dwin->image->u, dwin->image->v,
          dwin->image->info->padded_width,
          dwin->image->info->padded_height,
          dwin->image->info->padded_width*(pixel_stride/8),
          dwin->image->info->padded_width, dwin->image->info->padded_width/2 );
            
  if(dwin->grid) {
    if(dwin->color_grid) {
      add_color_grid(dwin);
    } else {  
      add_grid(dwin->data, dwin->ximage);
    }       
  }       

  if(screenshot) {
    screenshot = 0;
    screenshot_jpg(dwin->data, dwin->ximage);
  }

  XShmPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 
	       0, 0, 0, 0, 
               dwin->image->info->horizontal_size, dwin->image->info->vertical_size, 1);
  // XSync(mydisplay, False); or
  // XFlushmydisplay);
}


#ifdef HAVE_MLIB
void draw_win(debug_win *dwin)
{
  mlib_image *mimage_s;
  mlib_image *mimage_d;
  

  /* Put the decode rgb data at the end of the data segment. */
  char *address = dwin->data;
  
  int dest_size = dwin->ximage->bytes_per_line * dwin->ximage->height;
  int sorce_size = dwin->image->info->padded_width*(pixel_stride/8) * dwin->image->info->padded_height;
  
  int offs = dest_size - sorce_size - 4096;
  if( offs > 0 )
    address += offs;
    
  // Because mlib_YUV* reads out of bounds we need to make sure that the end
  // of the picture isn't on the pageboundary for in the last page allocated
  // There is also something strange with the mlib_ImageZoom in NEAREST mode.
			      
  /* We must some how guarantee that the ximage isn't used by X11. 
     Rigth now it's done by the XSync call at the bottom... */
  
  yuv2rgb(address, dwin->image->y, dwin->image->u, dwin->image->v,
	  dwin->image->info->padded_width, 
	  dwin->image->info->padded_height, 
	  dwin->image->info->padded_width*(pixel_stride/8),
	  dwin->image->info->padded_width,
	  dwin->image->info->padded_width/2 );

#ifdef SPU
  if(msgqid != -1) {
    mix_subpicture(address,
		   dwin->image->info->padded_width,
		   dwin->image->info->padded_height, 0);
  }
#endif

  if(dwin->grid) {
    if(dwin->color_grid) {
      add_color_grid(dwin);
    } else {  
      add_grid(dwin->data, dwin->ximage);
    }       
  }       
  
  if( (scaled_image_width != dwin->image->info->horizontal_size) ||
      (scaled_image_height != dwin->image->info->vertical_size )) {
    /* Destination image */
    mimage_d = mlib_ImageCreateStruct(MLIB_BYTE, 4,
				      scaled_image_width, 
				      scaled_image_height,
				      dwin->ximage->bytes_per_line, 
				      dwin->data);
    /* Source image */
    mimage_s = mlib_ImageCreateStruct(MLIB_BYTE, 4, 
				      dwin->image->info->horizontal_size, 
				      dwin->image->info->vertical_size,
				      dwin->image->info->padded_width*4, address);
    /* Extra fast 2x Zoom */
    if((scaled_image_width == 2 * dwin->image->info->horizontal_size) &&
       (scaled_image_height == 2 * dwin->image->info->vertical_size)) {
      mlib_ImageZoomIn2X(mimage_d, mimage_s, 
			 scalemode, MLIB_EDGE_DST_FILL_ZERO);
    } else {
      mlib_ImageZoom 
	(mimage_d, mimage_s,
	 (double)scaled_image_width/(double)dwin->image->info->horizontal_size, 
	 (double)scaled_image_height/(double)dwin->image->info->vertical_size,
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
#endif /* HAVE_MLIB */

#ifdef HAVE_XV

Bool predicate(Display *dpy, XEvent *ev, XPointer arg)
{

  if(ev->type == CompletionType) {
    return True;
  } else {
    return False;
  }
}

void draw_win(debug_win *dwin)
{
  unsigned char *dst;
  int size;
  
  if (xv_port == 0) { /* No xv found */
    draw_win_x11(dwin);
  } else {
    dst = xv_image->data;
#if 0 // HAVE_XV_NO_CP  // TODO hack, needs checking for correctness
    /* Copy Y data */
    size = dwin->image->info->padded_width*dwin->image->info->padded_height;
    memcpy(dst + xv_image->offsets[0], dwin->image->y, size); 
    /* Copy U data */
    size = dwin->image->info->padded_width*dwin->image->info->padded_height/4;
    memcpy(dst + xv_image->offsets[1], dwin->image->v, size);
    /* Copy V data */
    size = dwin->image->info->padded_width*dwin->image->info->padded_height/4;
    memcpy(dst + xv_image->offsets[2], dwin->image->u, size);
#else
    xv_image->data = dwin->image->y;
#endif

#ifdef SPU
  if(msgqid != -1) {
    mix_subpicture(xv_image->data,
		   dwin->image->info->padded_width,
		   dwin->image->info->padded_height, 1);
  }
#endif

    XvShmPutImage(mydisplay, xv_port, dwin->win, mygc, xv_image, 
                  0, 0, 
                  dwin->image->info->horizontal_size, dwin->image->info->vertical_size,
                  0, 0, 
                  scaled_image_width, scaled_image_height,
                  True);
    {
      XEvent ev;
      XEvent *e;
      e = &ev;
      
      /* this is to make sure that we are free to use the image again
         It waits for an XShmCompletionEvent */
      XIfEvent(mydisplay, &ev, predicate, NULL);

      
      /*
      if(ev.type == CompletionType) {
	fprintf(stderr, ".");
      } else {
	fprintf(stderr, ",%d, %d", ev.type,
		((XShmCompletionEvent *)e)->offset);
	
      }
      */
    }
    //XFlush(mydisplay);
  }
}
#endif /* HAVE_XV */

#if !defined(HAVE_MLIB) && !defined(HAVE_XV)
void draw_win(debug_win *dwin)
{
   draw_win_x11(dwin);
}
#endif /* Neither HAVE_MLIB or HAVE_XV */

