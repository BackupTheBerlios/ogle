/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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

#include <errno.h>

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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "common.h"
#include "video_types.h"
#include "yuv2rgb.h"
#include "screenshot.h"
#include "wm_state.h"

#include "ffb_asm.h"

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

/* needed for sun ffb2 */
#include <sys/mman.h>

static uint32_t *yuyv_fb;
static uint8_t *rgb_fb;

static int use_ffb2_yuv2rgb = 0;
static int fb_fd;

/* end needed for sun ffb2 */


extern int XShmGetEventBase(Display *dpy);
static int CompletionType;
static int xshmeventbase;

typedef struct {
  int x0;
  int y0;
  int x1;
  int y1;
} rect_t;

typedef struct {
  int x;
  int y;
  unsigned int width;
  unsigned int height;
} area_t;

typedef struct {
  Window win;
  XImage *ximage;
  yuv_image_t *image;
  area_t video_area;
  area_t window_area;
  int video_area_resize_allowed;
  WindowState_t win_state;
} window_info;

static window_info window;

struct {
  int width;
  int height;
  int horizontal_pixels;
  int vertical_pixels;
} dpy_size;

static XVisualInfo vinfo;
static XShmSegmentInfo shm_info;
static Display *mydisplay;
static GC mygc;
static char title[100];
static int color_depth, pixel_stride, mode;


static int screenshot = 0;
static int screenshot_spu = 0;


static int dpy_sar_frac_n;  /* Display (i.e. monitor) aspect. */
static int dpy_sar_frac_d;  /*(set in display_init) */

static struct {
  int zoom_n;              /* Zoom factor. */
  int zoom_d;
  int preserve_aspect;     /* Lock aspect. */
  int lock_window_size;    /* Never change the size of the window. */
  int image_width;         /* Current destination size. */
  int image_height;        /* (set in display_change_size */
} scale = { 1, 1, 1, 0, 720, 480 };

#ifdef HAVE_MLIB
static int scalemode = MLIB_BILINEAR;
#endif /* HAVE_MLIB */


static int use_xshm = 1;
static int use_xv = 0;


extern int msgqid;
extern yuv_image_t *image_bufs;

extern void display_process_exit(void);

extern ZoomMode_t zoom_mode;

extern MsgEventQ_t *msgq;
extern MsgEventClient_t input_client;
extern InputMask_t input_mask;

/* All display modules shall define these functions */
void display_init(yuv_image_t *picture_data,
		  data_buf_head_t *picture_data_head,
		  char *picture_buf_base);
void display(yuv_image_t *current_image);
void display_poll(yuv_image_t *current_image);
void display_exit(void);

/* Local prototypes */
static void draw_win_xv(window_info *dwin);
static void draw_win_x11(window_info *dwin);
static void display_change_size(yuv_image_t *img, int new_width, 
				int new_height, int resize_window);

static Bool true_predicate(Display *dpy, XEvent *ev, XPointer arg)
{
    return True;
}

static Cursor hidden_cursor;


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))




static rect_t clip(rect_t *r1, rect_t *r2)
{
  rect_t r;
  
  r.x0 = MAX(r1->x0, r2->x0);
  r.y0 = MAX(r1->y0, r2->y0);
  
  r.x1 = MIN(r1->x1, r2->x1);
  r.y1 = MIN(r1->y1, r2->y1);

  if((r.x0 >= r.x1) || (r.y0 >= r.y1)) {
    r.x0 = 0;
    r.y0 = 0;
    r.x1 = 0;
    r.y1 = 0;
  }

  return r;
}

static void create_transparent_cursor(Display *dpy, Window win)
{
  Pixmap cursor_mask;
  XColor dummy_col;
  
  unsigned char cursor_data[] = {
    0x0 
  };
  
  cursor_mask = XCreateBitmapFromData(dpy, win, cursor_data, 1, 1);
  hidden_cursor = XCreatePixmapCursor(dpy, cursor_mask, cursor_mask,
				      &dummy_col, &dummy_col, 0, 0);
  XFreePixmap(dpy, cursor_mask);
  
}

static void hide_cursor(Display *dpy, Window win)
{
  XDefineCursor(dpy, win, hidden_cursor);
}

static void restore_cursor(Display *dpy, Window win)
{
  XUndefineCursor(dpy, win);
}


static unsigned long req_serial;
static int (*prev_xerrhandler)(Display *dpy, XErrorEvent *ev);

static int xshm_errorhandler(Display *dpy, XErrorEvent *ev)
{
  if(ev->serial == req_serial) {
    /* this could be an error to the xshmattach request 
     * we assume that xshm doesn't work,
     * eg we are using a remote display
     */
    fprintf(stderr, "req_code: %d\n", ev->request_code);
    fprintf(stderr, "minor_code: %d\n", ev->minor_code);
    fprintf(stderr, "error_code: %d\n", ev->error_code);
    
    use_xshm = 0;
    fprintf(stderr, "*vo: Disabling Xshm\n");
    return 0;
  } else {
    /* if we get another error we should handle it, 
     * so we give it to the previous errorhandler
     */
    fprintf(stderr, "*vo: unexpected error serial: %lu, waited for: %lu\n",
	    ev->serial, req_serial);
    return prev_xerrhandler(dpy, ev);
  }
}

/* This section of the code looks for the Xv extension for hardware
 * yuv->rgb and scaling. If it is not found, or any suitable adapter
 * is not found, use_xv will not get set. Otherwise it allocates a
 * xv image and returns.
 *
 * The variable use_xv tells if Xv is used */
static void display_init_xv(yuv_image_t *picture_data,
			    data_buf_head_t *picture_data_head,
			    char *picture_buf_base) 
{
#ifdef HAVE_XV
  int i, j;
  int result;

  xv_port = 0; /* We have no port yet. */
  
  /* Check for the Xvideo extension */
  result = XvQueryExtension(mydisplay, &xv_version, &xv_release, 
			    &xv_request_base, &xv_event_base, 
			    &xv_error_base);
  if(result != Success) {
    fprintf(stderr, "Xv not found\n");
    return;
  }
  
  fprintf(stderr, "Found Xv extension, checking for suitable adaptors\n");
  /* Check for available adaptors */
  result = XvQueryAdaptors(mydisplay, DefaultRootWindow (mydisplay), 
			   &xv_num_adaptors, &xv_adaptor_info);
  if(result != Success)
    return;
  
      
  /* Check adaptors */
  for(i = 0; i < xv_num_adaptors; i++) {
    
    /* Is it usable for displaying XvImages */
    if(!(xv_adaptor_info[i].type & XvInputMask) ||
       !(xv_adaptor_info[i].type & XvImageMask))
      continue;
    
    xv_port = xv_adaptor_info[i].base_id;
    fprintf(stderr, "Found adaptor \"%s\" checking for suitable formats\n",
	    xv_adaptor_info[i].name);
      
    /* Check image formats of adaptor */
    xv_formats = XvListImageFormats(mydisplay, xv_port, &xv_num_formats);
    for(j = 0; j < xv_num_formats; j++) {
      if(xv_formats[j].id == 0x32315659) { /* YV12 */
	//if(xv_formats[j].id == 0x30323449) { /* I420 */
	fprintf(stderr, "Found image format \"%s\", using it\n", 
		xv_formats[j].guid);
	xv_id = xv_formats[j].id;
	break;
      } 
    }
    /* No matching format found */
    if(j == xv_num_formats)
      continue;
      
    fprintf(stderr, "Using Xvideo port %li for hw scaling\n", xv_port);
      
    /* Allocate XvImages */
    xv_image = XvShmCreateImage(mydisplay, xv_port, xv_id, NULL,
				picture_data->info->picture.padded_width,
				picture_data->info->picture.padded_height, 
				&shm_info);
    
    /* Got an Image? */
    if(xv_image == NULL)
      continue;
    
    /* Test and see if we really got padded_width x padded_height */
    if(xv_image->width != picture_data->info->picture.padded_width ||
       xv_image->height != picture_data->info->picture.padded_height) {
      fprintf(stderr, "vo: XvShmCreateImage got size: %d x %d\n",
	      xv_image->width, xv_image->height);
      exit(1);
    }
    
    shm_info.shmid = picture_data_head->shmid;
    shm_info.shmaddr = picture_buf_base;
    
    /* Set the data pointer to the decoders picture segment. */  
    xv_image->data = picture_data->y;
    shm_info.readOnly = True;
    
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
#if 0
      shmctl(shm_info.shmid, IPC_RMID, 0); // only works on Linux..
#endif
      CompletionType = XShmGetEventBase(mydisplay) + ShmCompletion;
    }
    use_xv = 1;
    /* All set up! */
    break;
  }
#endif /* HAVE_XV */
}
     

/* This section of the code tries to use the MIT XShm extension for 
 * accellerated transfers to to X. XShm extension is need and only
 * the actual attach of the shm segment and subsequent usage of
 * XShmPutImage is conditional on use_shm. 
 * I.e fallback to normal X11 is implicit and uses all the same 
 * structures (ximage creatd by XShmCreateImage).
 *
 * The variable use_xshm tells if XShm is used */
static void display_init_xshm()
{
  
  /* Create shared memory image */
  window.ximage = XShmCreateImage(mydisplay, vinfo.visual, color_depth,
				  ZPixmap, NULL, &shm_info,
				  scale.image_width,
				  scale.image_height);
  
  /* Got an Image? */
  if(window.ximage == NULL) {
    fprintf(stderr, "vo: XShmCreateImage failed\n");
    exit(1);
  }
  
  /* Test and see if we really got padded_width x padded_height */
  if(window.ximage->width != scale.image_width ||
     window.ximage->height != scale.image_height) {
    fprintf(stderr, "vo: XShmCreateImage got size: %d x %d\n",
	    window.ximage->width, window.ximage->height);
    exit(1);
  }
  
  /* Get a shared memory segment */
  shm_info.shmid = shmget(IPC_PRIVATE,
			  window.ximage->bytes_per_line * 
			  window.ximage->height, 
			  IPC_CREAT | 0777);
  
  if(shm_info.shmid < 0) {
    fprintf(stderr, "vo: shmget failed\n");
    exit(1);
  }
  
  /* Attach shared memory segment */
  shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
  
  if(shm_info.shmaddr == ((char *) -1)) {
    fprintf(stderr, "vo: shmat failed\n");
    exit(1);
  }
  
  /* Set the data pointer to the allocated segment. */  
  window.ximage->data = shm_info.shmaddr;
  shm_info.readOnly = False;
  

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
  
  
  CompletionType = XShmGetEventBase(mydisplay) + ShmCompletion;  
  
  pixel_stride = window.ximage->bits_per_pixel;

  
  fprintf(stderr, "vo: pixel_stride: %d\n", pixel_stride);
  

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
}





void display_init(yuv_image_t *picture_data,
		  data_buf_head_t *picture_data_head,
		  char *picture_buf_base)
{
  int screen;
  Screen *scr;
  
  XSizeHints hint;
  XEvent xev;
  XGCValues xgcv;
  Colormap theCmap;
  XWindowAttributes attribs;
  XSetWindowAttributes xswa;
  unsigned long xswamask;

  if(getenv("USE_FFB2_YUV2RGB")) {
    use_ffb2_yuv2rgb = 1;
  }
  mydisplay = XOpenDisplay(NULL);
  
  if(mydisplay == NULL) {
    fprintf(stderr, "vo: Can not open display\n");
    exit(1);
  }
  
  screen = DefaultScreen(mydisplay);
  scr = XDefaultScreenOfDisplay(mydisplay);
  
  
  /* Query and calculate the displays aspect rate. */
  dpy_size.width = DisplayWidthMM(mydisplay, screen);
  dpy_size.height = DisplayHeightMM(mydisplay, screen);
  
  dpy_size.horizontal_pixels = DisplayWidth(mydisplay, screen);
  dpy_size.vertical_pixels = DisplayHeight(mydisplay, screen);
  
  dpy_sar_frac_n = dpy_size.height * dpy_size.horizontal_pixels;
  dpy_sar_frac_d = dpy_size.width * dpy_size.vertical_pixels;
  
  fprintf(stderr, "*d* h: %d, w: %d, hp: %d, wp: %d\n",
	  dpy_size.height, dpy_size.width,
	  dpy_size.vertical_pixels, dpy_size.horizontal_pixels);
  fprintf(stderr, "*s* h: %d, w: %d, hp: %d, wp: %d\n",
	  XHeightMMOfScreen(scr), XWidthMMOfScreen(scr),
	  XHeightOfScreen(scr), XWidthOfScreen(scr));
  fprintf(stderr, "*** display_sar: %d/%d\n", dpy_sar_frac_n, dpy_sar_frac_d);
  
  
  /* Assume (for now) that the window will be the same size as the source. */
  scale.image_width = picture_data->info->picture.horizontal_size;
  scale.image_height = picture_data->info->picture.vertical_size;
  
  
  /* Make the window */
  XGetWindowAttributes(mydisplay, DefaultRootWindow(mydisplay), &attribs);
  color_depth = attribs.depth;
  if(color_depth != 15 && color_depth != 16 && 
     color_depth != 24 && color_depth != 32) {
    fprintf(stderr,"Only 15, 16, 24, and 32bpp supported. Trying 24bpp!\n");
    color_depth = 24;
  }
  
  XMatchVisualInfo(mydisplay, screen, color_depth, TrueColor, &vinfo);
  fprintf(stderr, "vo: X11 visual id is %lx\n", vinfo.visualid);

  hint.x = 0;
  hint.y = 0;
  hint.width = scale.image_width;
  hint.height = scale.image_height;
  hint.flags = PPosition | PSize;
  
  theCmap   = XCreateColormap(mydisplay, RootWindow(mydisplay,screen), 
			      vinfo.visual, AllocNone);
  
  xswa.background_pixel = 0;
  xswa.border_pixel     = 1;
  xswa.colormap         = theCmap;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  window.win_state = WINDOW_STATE_NORMAL;

  window.win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
			     hint.x, hint.y, hint.width, hint.height, 
			     4, color_depth, CopyFromParent, vinfo.visual, 
			     xswamask, &xswa);
  
  XSelectInput(mydisplay, window.win,
	       StructureNotifyMask | ExposureMask |
	       KeyPressMask | ButtonPressMask |
	       PointerMotionMask | PropertyChangeMask);

  create_transparent_cursor(mydisplay, window.win);
  
  /* Tell other applications/the window manager about us. */
  snprintf(&title[0], 99, "Ogle v%s", VERSION);
  XSetStandardProperties(mydisplay, window.win, &title[0], &title[0], 
			 None, NULL, 0, &hint);
  
  /* Map window. */
  XMapWindow(mydisplay, window.win);
  
  /* Wait for map. */
  do {
    XNextEvent(mydisplay, &xev);
    switch(xev.type) {
    case ConfigureNotify:
      // remove all configure notify in queue
      while(XCheckTypedEvent(mydisplay, ConfigureNotify, &xev) == True); 
      
      if(xev.xconfigure.window == window.win) {
	Window dummy_win;
	window.window_area.width = xev.xconfigure.width;
	window.window_area.height = xev.xconfigure.height;

	/*
	display_adjust_size(current_image, 
			    xev.xconfigure.width, 
			    xev.xconfigure.height);
	*/
	window.video_area.width = scale.image_width;
	window.video_area.height = scale.image_height;
	window.video_area.x = (window.window_area.width - 
			       window.video_area.width) / 2;
	window.video_area.y = (window.window_area.height -
			       window.video_area.height) / 2;

	XTranslateCoordinates(mydisplay, window.win,
			      DefaultRootWindow(mydisplay), 
			      0,
			      0,
			      &window.window_area.x,
			      &window.window_area.y,
			      &dummy_win);
      }
      break;
    default:
      break;
    }
  } while (xev.type != MapNotify || xev.xmap.event != window.win);
  
  XSync(mydisplay, False);
  
  /* Create the colormaps. (needed in the PutImage calls) */   
  mygc = XCreateGC(mydisplay, window.win, 0L, &xgcv);
  

  

  xshmeventbase = XShmGetEventBase(mydisplay);  
  fprintf(stderr, "xshmeventbase: %d\n", xshmeventbase);

  /* Try to use XFree86 Xv (X video) extension for display.
     Sets use_xv to true on success. */
  display_init_xv(picture_data, picture_data_head, picture_buf_base);

  /* Try XShm if we didn't have/couldn't use Xv. 
     This allso falls back to normal X11 if XShm fails. */
  if(!use_xv) {
    display_init_xshm();
  }
  
  /*** sun ffb2 ***/
  
  if(use_ffb2_yuv2rgb) {  
    fb_fd = open("/dev/fb", O_RDWR);
    yuyv_fb = (unsigned int *)mmap(NULL, 4*1024*1024, PROT_READ | PROT_WRITE,
				   MAP_SHARED, fb_fd, 0x0701a000);
    if(yuyv_fb == MAP_FAILED) {
      perror("mmap");
      exit(-1);
    }
    
    rgb_fb = (uint8_t *)mmap(NULL, 4*2048*1024, PROT_READ | PROT_WRITE,
				  MAP_SHARED, fb_fd, 0x05004000);
    if(rgb_fb == MAP_FAILED) {
      perror("mmap");
      exit(-1);
    }
  }
  /*** end sun ffb2 ***/

  /* Let the user know what mode we are running in. */
  snprintf(&title[0], 99, "Ogle v%s %s%s", VERSION, 
	   use_xv ? "Xv " : "", use_xshm ? "XShm " : "");
  XStoreName(mydisplay, window.win, &title[0]);
  
}





/* TODO display_change_size needs to be fixed */
static void display_change_size(yuv_image_t *img, int new_width, 
				int new_height, int resize_window) {
  int padded_width = img->info->picture.padded_width;
  int padded_height = img->info->picture.padded_height;
  //int alloc_width, alloc_height;
  int alloc_size;
  fprintf(stderr, "vo resize: %d, %d\n", new_width, new_height);
  
  
  /* If we cant scale (i.e no Xv or mediaLib) exit give up now. */
#ifndef HAVE_MLIB
  if(!use_xv)
    return;
#endif
  
  // Check to not 'reallocate' if the size is the same...
  if(scale.image_width == new_width && scale.image_height == new_height) {
    /* Might still need to force a change of the widow size. */
    if(resize_window == True) {
      XResizeWindow(mydisplay, window.win, 
		    scale.image_width, scale.image_height);
    }
    return;
  }
  
  
  if(!use_xv) {
    
    XSync(mydisplay,True);
    
    /* Destroy old display */
    if(use_xshm)
      XShmDetach(mydisplay, &shm_info);
    XDestroyImage(window.ximage);
    shmdt(shm_info.shmaddr);
    if(shm_info.shmaddr == ((char *) -1)) {
      fprintf(stderr, "vo: Shared memory: Couldn't detach segment\n");
      exit(1);
    }
    if(shmctl(shm_info.shmid, IPC_RMID, 0) == -1) {
      perror("vo: shmctl ipc_rmid");
      exit(1);
    }
    
    memset(&shm_info, 0, sizeof(XShmSegmentInfo));
    
    XSync(mydisplay, True);
    
    /* Create new display */
    window.ximage = XShmCreateImage(mydisplay, vinfo.visual, 
				    color_depth, ZPixmap, 
				    NULL, &shm_info,
				    new_width,
				    new_height);

    if(window.ximage == NULL) {
      fprintf(stderr, "vo: Shared memory: couldn't create Shm image\n");
      exit(1);
    }

    // or that we allocate less than the minimum size...
    if(window.ximage->bytes_per_line *  window.ximage->height
       < padded_width * padded_height * 4) {
      alloc_size = padded_width * padded_height * 4;
    } else {
      alloc_size = window.ximage->bytes_per_line * window.ximage->height;
    }
    
    /* Get a shared memory segment */
    shm_info.shmid = shmget(IPC_PRIVATE,
			    alloc_size,
			    IPC_CREAT | 0777);


    if(shm_info.shmid < 0) {
      fprintf(stderr, "vo: Shared memory: Couldn't get segment\n");
      exit(1);
    }
  
    /* Attach shared memory segment */
    shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
    if(shm_info.shmaddr == ((char *) -1)) {
      fprintf(stderr, "vo: Shared memory: Couldn't attach segment\n");
      exit(1);
    }
    
    window.ximage->data = shm_info.shmaddr;
    
    shm_info.readOnly = False;
    if(use_xshm)
      XShmAttach(mydisplay, &shm_info);
    
    XSync(mydisplay, False);
  }
  
  /* Save the new size so we know what to scale to. */
  scale.image_width = new_width;
  scale.image_height = new_height;
  
  /* Force a change of the window size. */
  if(resize_window == True) {
    XResizeWindow(mydisplay, window.win, 
		  scale.image_width, scale.image_height);
  }
}


static void display_adjust_size(yuv_image_t *current_image,
				int given_width, int given_height) {
  int sar_frac_n, sar_frac_d; 
  int64_t scale_frac_n, scale_frac_d;
  int base_width, base_height, max_width, max_height;
  int new_width, new_height;
  
  /* Use the stream aspect or a forced/user aspect. */ 
  //  if(!scale_override_aspect) {
  sar_frac_n = current_image->info->picture.sar_frac_n;
  sar_frac_d = current_image->info->picture.sar_frac_d;
  //  } else {
  //    sar_frac_n = scale_override_aspect_n;
  //    sar_frac_d = scale_override_aspect_d;
  //  }
  
  // TODO replace image->sar.. with image->dar
  scale_frac_n = (int64_t)dpy_sar_frac_n * (int64_t)sar_frac_d; 
  scale_frac_d = (int64_t)dpy_sar_frac_d * (int64_t)sar_frac_n;
  
  fprintf(stderr, "vo: sar: %d/%d, dpy_sar %d/%d, scale: %lld, %lld\n",
	  sar_frac_n, sar_frac_d,
	  dpy_sar_frac_n, dpy_sar_frac_d,
	  scale_frac_n, scale_frac_d); 
  
  /* Keep either the height or the width constant. */ 
  if(scale_frac_n > scale_frac_d) {
    base_width = (current_image->info->picture.horizontal_size *
		  scale_frac_n) / scale_frac_d;
    base_height = current_image->info->picture.vertical_size;
  } else {
    base_width = current_image->info->picture.horizontal_size;
    base_height = (current_image->info->picture.vertical_size *
		   scale_frac_d) / scale_frac_n;
  }
  //fprintf(stderr, "vo: base %d x %d\n", base_width, base_height);
  
  /* Do we have a predetermined size for the window? */ 
  if(given_width != -1 && given_height != -1 &&
     (window.win_state != WINDOW_STATE_FULLSCREEN)) {
    max_width  = given_width;
    max_height = given_height;
  } else {
    if(!scale.lock_window_size) {
      /* Never make the window bigger than the screen. */
      max_width  = DisplayWidth(mydisplay, DefaultScreen(mydisplay));
      max_height = DisplayHeight(mydisplay, DefaultScreen(mydisplay));
    } else {
      max_width = window.window_area.width;
      max_height = window.window_area.height;
    }
  }
  //fprintf(stderr, "vo: max %d x %d\n", max_width, max_height);
  
  /* Fill the given area or keep the image at the same zoom level? */
  if((window.win_state == WINDOW_STATE_FULLSCREEN) 
     || (given_width != -1 && given_height != -1)) {
    /* Zoom so that the image fill the width. */
    /* If the height gets to large it's adjusted/fixed below. */
    new_width  = max_width;
    new_height = (base_height * max_width) / base_width;
  } else {
    fprintf(stderr, "vo: using zoom %d / %d\n", scale.zoom_n, scale.zoom_d);
    /* Use the provided zoom value. */
    new_width  = (base_width  * scale.zoom_n) / scale.zoom_d;
    new_height = (base_height * scale.zoom_n) / scale.zoom_d;
  }
  //fprintf(stderr, "vo: new1 %d x %d\n", new_width, new_height);
  
  /* Don't ever make it larger than the max limits. */
  if(new_width > max_width) {
    new_height = (new_height * max_width) / new_width;
    new_width  = max_width;
  }
  if(new_height > max_height) {
    new_width  = (new_width * max_height) / new_height;
    new_height = max_height;
  }
  //fprintf(stderr, "vo: new2 %d x %d\n", new_width, new_height);
  
  /* Remeber what zoom level we ended up with. */
  if(window.win_state != WINDOW_STATE_FULLSCREEN) {
    /* Update zoom values. Use the smalles one. */
    if((new_width * base_height) < (new_height * base_width)) {
      scale.zoom_n = new_width;
      scale.zoom_d = base_width;
    } else {
      scale.zoom_n = new_height;
      scale.zoom_d = base_height;
    }
    fprintf(stderr, "vo: zoom2 %d / %d\n", scale.zoom_n, scale.zoom_d);
  }
  
  /* Don't care about aspect and can't change the window size, use it all. */
  if(!scale.preserve_aspect &&
     (scale.lock_window_size ||
      (window.win_state == WINDOW_STATE_FULLSCREEN))) {
    new_width  = max_width;
    new_height = max_height;
  }
  
  if((scale.lock_window_size ||
      (window.win_state == WINDOW_STATE_FULLSCREEN))
     || (given_width != -1 && given_height != -1))
    display_change_size(current_image, new_width, new_height, False);
  else
    display_change_size(current_image, new_width, new_height, True);
}  



static void display_toggle_fullscreen(yuv_image_t *current_image) {
  
  if(window.win_state != WINDOW_STATE_FULLSCREEN) {
    
    ChangeWindowState(mydisplay, window.win, WINDOW_STATE_FULLSCREEN);
    window.win_state = WINDOW_STATE_FULLSCREEN;
  } else {
 
    ChangeWindowState(mydisplay, window.win, WINDOW_STATE_NORMAL);  
    window.win_state = WINDOW_STATE_NORMAL;
  }
  
}


void check_x_events(yuv_image_t *current_image)
{
  XEvent ev;
  static clocktime_t prev_time;
  clocktime_t cur_time;
  static Bool cursor_visible = True;
  static Time last_motion;
  
  while(XCheckIfEvent(mydisplay, &ev, true_predicate, NULL) != False) {
    
    switch(ev.type) {
    case KeyPress:
      // send keypress to whoever wants it
      if(input_mask & INPUT_MASK_KeyPress) {
	MsgEvent_t m_ev;
	KeySym keysym;
	XLookupString(&(ev.xkey), NULL, 0, &keysym, NULL);
	m_ev.type = MsgEventQInputKeyPress;
	m_ev.input.x = (ev.xkey.x - window.video_area.x) *
	  current_image->info->picture.horizontal_size /
	  window.video_area.width;
	m_ev.input.y = (ev.xkey.y - window.video_area.y) *
	  current_image->info->picture.vertical_size /
	  window.video_area.height;
	m_ev.input.x_root = ev.xkey.x_root;
	m_ev.input.y_root = ev.xkey.y_root;
	m_ev.input.mod_mask = ev.xkey.state;
	m_ev.input.input = keysym;

	if(MsgSendEvent(msgq, input_client, &m_ev, IPC_NOWAIT) == -1) {
	  switch(errno) {
	  case EAGAIN:
	    // msgq full, drop message
	    break;
#ifdef EIDRM
	  case EIDRM:
#endif
	  case EINVAL:
	    fprintf(stderr, "vo: keypress: no msgq\n");
	    display_exit(); //TODO clean up and exit
	    break;
	  default:
	    fprintf(stderr, "vo: keypress: couldn't send notification\n");
	    display_exit(); //TODO clean up and exit
	    break;
	  }
	}
#if 1
	//TODO this is just for testing, should be done from vm/ui
	if(keysym >= XK_1 && keysym <= XK_9) {
	  m_ev.type = MsgEventQSpeed;
	  switch(keysym) {
	  case XK_1:
	    m_ev.speed.speed = 0.125;
	    break;
	  case XK_2:
	    m_ev.speed.speed = 0.25;
	    break;
	  case XK_3:
	    m_ev.speed.speed = 0.5;
	    break;
	  case XK_4:
	    m_ev.speed.speed = 0.75;
	    break;
	  case XK_5:
	    m_ev.speed.speed = 1.0;
	    break;
	  case XK_6:
	    m_ev.speed.speed = 1.5;
	    break;
	  case XK_7:
	    m_ev.speed.speed = 2.0;
	    break;
	  case XK_8:
	    m_ev.speed.speed = 4.0;
	    break;
	  case XK_9:
	    m_ev.speed.speed = 8.0;
	    break;
	  default:
	    break;
	  }	    
	  
	  if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &m_ev, IPC_NOWAIT) == -1) {
	    switch(errno) {
	    case EAGAIN:
	      // msgq full, drop message
	      break;
	    case EIDRM:
	    case EINVAL:
	      fprintf(stderr, "vo: speed: no msgq\n");
	      display_exit(); //TODO clean up and exit
	      break;
	    default:
	      fprintf(stderr, "vo: speed: couldn't send notification\n");
	      display_exit(); //TODO clean up and exit
	      break;
	    }
	  }
	
	
	}
#endif
	// hack
	if(keysym == XK_i) {
	  screenshot = 1;
	}
	if(keysym == XK_I) {
	  screenshot_spu = 1;
	}
      }
      break;
    case ButtonPress:
      // send buttonpress to whoever wants it
      if(input_mask & INPUT_MASK_ButtonPress) {
	MsgEvent_t m_ev;
	
	m_ev.type = MsgEventQInputButtonPress;
	m_ev.input.x = (ev.xbutton.x - window.video_area.x)*
	  current_image->info->picture.horizontal_size /
	  window.video_area.width;
	m_ev.input.y = (ev.xbutton.y - window.video_area.y) *
	  current_image->info->picture.vertical_size /
	  window.video_area.height;
	m_ev.input.x_root = ev.xbutton.x_root;
	m_ev.input.y_root = ev.xbutton.y_root;
	m_ev.input.mod_mask = ev.xbutton.state;
	m_ev.input.input = ev.xbutton.button;

	if(MsgSendEvent(msgq, input_client, &m_ev, IPC_NOWAIT) == -1) {
	  switch(errno) {
	  case EAGAIN:
	    // msgq full, drop message
	    break;
#ifdef EIDRM
	  case EIDRM:
#endif
	  case EINVAL:
	    fprintf(stderr, "vo: buttonpress: no msgq\n");
	    display_exit(); //TODO clean up and exit
	    break;
	  default:
	    fprintf(stderr, "vo: buttonpress: couldn't send notification\n");
	    display_exit(); //TODO clean up and exit
	    break;
	  }
	}

      }
      

      if(cursor_visible == False) {
	restore_cursor(mydisplay, window.win);
	cursor_visible = True;
      }
      clocktime_get(&prev_time);
      break;
    case MotionNotify:
      if((ev.xmotion.time - last_motion) > 50) {
	last_motion = ev.xmotion.time;
	
	// send motion notify to whoever wants it
	if(input_mask & INPUT_MASK_PointerMotion) {
	  MsgEvent_t m_ev;
	  
	  m_ev.type = MsgEventQInputPointerMotion;
	  m_ev.input.x = (ev.xmotion.x - window.video_area.x) *
	    current_image->info->picture.horizontal_size /
	    window.video_area.width;
	  m_ev.input.y = (ev.xmotion.y - window.video_area.y) *
	    current_image->info->picture.vertical_size /
	    window.video_area.height;
	  m_ev.input.x_root = ev.xmotion.x_root;
	  m_ev.input.y_root = ev.xmotion.y_root;
	  m_ev.input.mod_mask = ev.xmotion.state;
	  m_ev.input.input = 0;

	  if(MsgSendEvent(msgq, input_client, &m_ev, IPC_NOWAIT) == -1) {
	    switch(errno) {
	    case EAGAIN:
	      // msgq full, drop message
	      break;
#ifdef EIDRM
	    case EIDRM:
#endif
	    case EINVAL:
	      fprintf(stderr, "vo: pointermotion: no msgq\n");
	      display_exit(); //TODO clean up and exit
	      break;
	    default:
	      fprintf(stderr, "vo: pointermotion: couldn't send notification\n");
	      display_exit(); //TODO clean up and exit
	      break;
	    }
	  }
	}
      }
      if(cursor_visible == False) {
	restore_cursor(mydisplay, window.win);
	cursor_visible = True;
      }
      clocktime_get(&prev_time);
      break;
    case Expose:
      // remove all Expose events in queue
      while(XCheckTypedEvent(mydisplay, Expose, &ev) == True);
      
      if(ev.xexpose.window == window.win) {

	if(use_xv) {
	  draw_win_xv(&window);
 	} else {
	  draw_win_x11(&window);
	}
      }
      break;
    case ConfigureNotify:
      // remove all configure notify in queue
      while(XCheckTypedEvent(mydisplay, ConfigureNotify, &ev) == True); 
      
      if(ev.xconfigure.window == window.win) {
	Window dummy_win;
	window.window_area.width = ev.xconfigure.width;
	window.window_area.height = ev.xconfigure.height;
	
	display_adjust_size(current_image, 
			    ev.xconfigure.width, 
			    ev.xconfigure.height);
	
	window.video_area.width = scale.image_width;
	window.video_area.height = scale.image_height;
	window.video_area.x = (window.window_area.width - 
			       window.video_area.width) / 2;
	window.video_area.y = (window.window_area.height -
			       window.video_area.height) / 2;

	XTranslateCoordinates(mydisplay, window.win,
			      DefaultRootWindow(mydisplay), 
			      0,
			      0,
			      &window.window_area.x,
			      &window.window_area.y,
			      &dummy_win);

	
	// top border
	if(window.video_area.y > 0) {
	  XClearArea(mydisplay, window.win,
		     0, 0,
		     window.window_area.width,
		     window.video_area.y - 0,
		     False);
	}
	// bottom border
	if((window.video_area.y + window.video_area.height) <
	   window.window_area.height) {
	  XClearArea(mydisplay, window.win,
		     0, (window.video_area.y + window.video_area.height),
		     window.window_area.width,
		     window.window_area.height -
		     (window.video_area.y + window.video_area.height),
		     False);
	}
	// left border
	if(window.video_area.x > 0) {
	  XClearArea(mydisplay, window.win,
		     0, 0,
		     window.video_area.x - 0,
		     window.window_area.height,
		     False);
	}
	// right border
	if((window.video_area.x + window.video_area.width) <
	   window.window_area.width) {
	  XClearArea(mydisplay, window.win,
		     (window.video_area.x + window.video_area.width), 0,
		     window.window_area.width -
		     (window.video_area.x + window.video_area.width),
		     window.window_area.height,
		     False);
	}
	
      }
      break;    
    default:
      break;
    }
  }

  if(cursor_visible == True) {
    clocktime_get(&cur_time);
    timesub(&cur_time, &cur_time, &prev_time);
    if(TIME_S(cur_time) >= 2) {
      hide_cursor(mydisplay, window.win);
      cursor_visible = False;
    }
  }
}

void display(yuv_image_t *current_image)
{
  static int sar_frac_n, sar_frac_d; 
  
  /* New source aspect ratio? */
  if(current_image->info->picture.sar_frac_n != sar_frac_n ||
     current_image->info->picture.sar_frac_d != sar_frac_d) {
    
    sar_frac_n = current_image->info->picture.sar_frac_n;
    sar_frac_d = current_image->info->picture.sar_frac_d;
    
    display_adjust_size(current_image, -1, -1);
  }
  
  if(((window.win_state == WINDOW_STATE_NORMAL) && 
      (zoom_mode == ZoomModeFullScreen)) ||
     ((window.win_state == WINDOW_STATE_FULLSCREEN) &&
      (zoom_mode == ZoomModeResizeAllowed))) {
    display_toggle_fullscreen(current_image);
  }
  
  window.image = current_image;

  check_x_events(current_image);

  if(use_xv)
    draw_win_xv(&window);
  else
    draw_win_x11(&window);
  
  return;
}

void display_poll(yuv_image_t *current_image)
{
  check_x_events(current_image);
}


void display_exit(void) 
{
  // FIXME TODO $$$ X isn't async signal safe.. cant free/detach things here..
  
  // Need to add some test to se if we can detatch/free/destroy things
  
  XSync(mydisplay,True);
  if(use_xshm)
    XShmDetach(mydisplay, &shm_info);
  XDestroyImage(window.ximage);
  shmdt(shm_info.shmaddr);
  shmctl(shm_info.shmid, IPC_RMID, 0);
  
  fprintf(stderr, "vo: removed shm segment\n");
  display_process_exit();
}


static Bool predicate(Display *dpy, XEvent *ev, XPointer arg)
{
  if(ev->type == CompletionType) {
    return True;
  } else {
    return False;
  }
}


static void draw_win_x11(window_info *dwin)
{
  char *address = dwin->ximage->data;
  
#ifdef HAVE_MLIB
  int dest_size = dwin->ximage->bytes_per_line * dwin->ximage->height;
  int sorce_size = (dwin->image->info->picture.padded_width*(pixel_stride/8) 
		    * dwin->image->info->picture.padded_height);
  
  mlib_image *mimage_s;
  mlib_image *mimage_d;
  
  /* Put the decode rgb data close to the end of the data segment. */
  // Because mlib_YUV* reads out of bounds we need to make sure that the end
  // of the picture isn't on the pageboundary for in the last page allocated
  // There is also something strange with the mlib_ImageZoom in NEAREST mode.
  int offs = dest_size - sorce_size - 4096;
  if(offs > 0)
    address += offs;
#endif /* HAVE_MLIB */

  /*** sun ffb2 ***/

  if(use_ffb2_yuv2rgb) {
    int stride;
    int m;
    unsigned char *y;
    unsigned char *u;
    unsigned char *v;
    area_t fb_area;
    area_t src_area;
    rect_t fb_rect;
    rect_t clip_rect;

    window.video_area.width = dwin->image->info->picture.horizontal_size;
    window.video_area.height = dwin->image->info->picture.vertical_size;
    window.video_area.x = (int)(window.window_area.width - 
				window.video_area.width) / 2;
    window.video_area.y = (int)(window.window_area.height -
				window.video_area.height) / 2;
  
    
    stride = dwin->image->info->picture.padded_width;
    y = dwin->image->y;
    u = dwin->image->u;
    v = dwin->image->v;
    
    /*
    fprintf(stderr, "win.x: %d, win.y: %d, win.w: %u, win.h: %u\n",
	    window.window_area.x,
	    window.window_area.y,
	    window.window_area.width,
	    window.window_area.height);

    fprintf(stderr, "vid.x: %d, vid.y: %d, vid.w: %u, vid.h: %u\n",
	    window.video_area.x,
	    window.video_area.y,
	    window.video_area.width,
	    window.video_area.height);
    */

    fb_rect.x0 = window.window_area.x+window.video_area.x;
    fb_rect.y0 = window.window_area.y+window.video_area.y;
    fb_rect.x1 = fb_rect.x0 + window.video_area.width;
    fb_rect.y1 = fb_rect.y0 + window.video_area.height;
    
    clip_rect.x0 = window.window_area.x;
    clip_rect.y0 = window.window_area.y;
    clip_rect.x1 = clip_rect.x0 + window.window_area.width;
    clip_rect.y1 = clip_rect.y0 + window.window_area.height;

    fb_rect = clip(&fb_rect, &clip_rect);
    
    clip_rect.x0 = 0;
    clip_rect.y0 = 0;
    clip_rect.x1 = dpy_size.horizontal_pixels;
    clip_rect.y1 = dpy_size.vertical_pixels;

    fb_rect = clip(&fb_rect, &clip_rect);
    
    fb_area.x = fb_rect.x0;
    fb_area.y = fb_rect.y0;
    fb_area.width = fb_rect.x1 - fb_rect.x0;
    fb_area.height = fb_rect.y1 - fb_rect.y0;

    if(fb_area.width > 0 && fb_area.height > 0) {
      src_area.x = fb_area.x - (window.window_area.x+window.video_area.x);
      src_area.y = fb_area.y - (window.window_area.y+window.video_area.y);
      src_area.width = fb_area.width;
      src_area.height = fb_area.height;
      

    /*
    fprintf(stderr, "===: fb.x: %d fb.y: %d fb.w: %u fb.h: %u\n",
	    fb_area.x, fb_area.y, fb_area.width, fb_area.height);

    fprintf(stderr, "===: src.x: %d src.y: %d src.w: %u src.h: %u\n",
	    src_area.x, src_area.y, src_area.width, src_area.height);
    */



#ifdef USE_SPARCASM
    for(m = 0; m < src_area.height; m+=2) {
      uint8_t  *yp;
      uint8_t *up;
      uint8_t *vp;

      y = dwin->image->y+(src_area.y+m)*stride;
      u = dwin->image->u+(src_area.y+m)/2*stride/2;
      v = dwin->image->v+(src_area.y+m)/2*stride/2;

      yp = &y[src_area.x];
      up = &u[src_area.x/2];
      vp = &v[src_area.x/2];
      
      yuv_plane_to_yuyv_packed(yp, up, vp,
			       &yuyv_fb[(fb_area.y+m)*1024+(fb_area.x/4)*2],
			       src_area.width);
    }
#else  /* USE_SPARCASM */
    
    for(m = 0; m < src_area.height; m++) {
      int n;
      y = dwin->image->y+(src_area.y+m)*stride;
      
      u = dwin->image->u+(src_area.y+m)/2*stride/2;
      v = dwin->image->v+(src_area.y+m)/2*stride/2;
      
      for(n = 0; n < src_area.width/2; n++) {
	unsigned int pixel_data;
	
	pixel_data =
	  (y[src_area.x+n*2]<<24) |
	  (u[src_area.x/2+n]<<16) |
	  (y[src_area.x+n*2+1]<<8) | 
	  (v[src_area.x/2+n]);
	yuyv_fb[(fb_area.y+m)*1024+(fb_area.x/2+n)] = pixel_data;
	
      }
    }
#endif /* USE_SPARCASM */
	
    
#ifdef SPU
    if(msgqid != -1) {
      mix_subpicture_rgb(&rgb_fb[fb_area.y*8192+fb_area.x*4], 2048,
			 fb_area.height, 
			 4); 
      // Should have mode to or use a mix_subpicture_init(pixel_s,mode);
    }
#endif
    }
    return;
  }  
  /*** end sun ffb2 ***/
  
  /* We must some how guarantee that the ximage isn't used by X11. 
     This is done by the XIfEvent at the bottom */
  
  yuv2rgb(address, dwin->image->y, dwin->image->u, dwin->image->v,
	  dwin->image->info->picture.padded_width, 
	  dwin->image->info->picture.padded_height, 
	  dwin->image->info->picture.padded_width*(pixel_stride/8),
	  dwin->image->info->picture.padded_width,
	  dwin->image->info->picture.padded_width/2);
  
#ifdef SPU
  if(msgqid != -1) {
    mix_subpicture_rgb(address, dwin->image->info->picture.padded_width,
		       dwin->image->info->picture.padded_height, 
		       (pixel_stride/8)); 
    // Should have mode to or use a mix_subpicture_init(pixel_s,mode);
  }
#endif
  
  if(screenshot_spu) {
    screenshot_spu = 0;
    screenshot_rgb_jpg(address,
		       dwin->image->info->picture.padded_width,
		       dwin->image->info->picture.padded_height);
  }
  

#ifdef HAVE_MLIB
  if((scale.image_width != dwin->image->info->picture.horizontal_size) ||
     (scale.image_height != dwin->image->info->picture.vertical_size)) {
    /* Destination image */
    mimage_d = mlib_ImageCreateStruct(MLIB_BYTE, 4,
				      scale.image_width, 
				      scale.image_height,
				      dwin->ximage->bytes_per_line, 
				      dwin->ximage->data);
    /* Source image */
    mimage_s = mlib_ImageCreateStruct(MLIB_BYTE, 4, 
				      dwin->image->info->picture.horizontal_size, 
				      dwin->image->info->picture.vertical_size,
				      dwin->image->info->picture.padded_width*4, address);
    /* Extra fast 2x Zoom */
    if((scale.image_width == 2 * dwin->image->info->picture.horizontal_size) &&
       (scale.image_height == 2 * dwin->image->info->picture.vertical_size)) {
      mlib_ImageZoomIn2X(mimage_d, mimage_s, 
			 scalemode, MLIB_EDGE_DST_FILL_ZERO);
    } else {
      mlib_ImageZoom 
	(mimage_d, mimage_s,
	 (double)scale.image_width/(double)dwin->image->info->picture.horizontal_size, 
	 (double)scale.image_height/(double)dwin->image->info->picture.vertical_size,
	 scalemode, MLIB_EDGE_DST_FILL_ZERO);
    }
    mlib_ImageDelete(mimage_s);
    mlib_ImageDelete(mimage_d);
  }
#endif /* HAVE_MLIB */
  
  if(screenshot) {
    screenshot = 0;
    screenshot_yuv_jpg(dwin->image, dwin->ximage);
  }
  
  window.video_area.width = scale.image_width;
  window.video_area.height = scale.image_height;
  window.video_area.x = (window.window_area.width - 
			 window.video_area.width) / 2;
  window.video_area.y = (window.window_area.height -
			 window.video_area.height) / 2;

  /*
  
  fprintf(stderr, "dwin->win: %d, mygc: %d, dwin->ximage: %d, 0, 0,\n",
	  dwin->win, mygc, dwin->ximage);
  fprintf(stderr, "x: %d %x,  y: %d %x, w: %d %x, h: %d %x\n",
	   window.video_area.x,
	   window.video_area.x,
	  window.video_area.y, 
	  window.video_area.y, 
	  window.video_area.width,
	  window.video_area.width,
	  window.video_area.height,
	  window.video_area.height);
  */

  if(use_xshm) {
    XShmPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 0, 0, 
		 window.video_area.x,
		 window.video_area.y, 
		 window.video_area.width,
		 window.video_area.height,
		 True);

    
    {
      XEvent ev;
      
      /* this is to make sure that we are free to use the image again
	 It waits for an XShmCompletionEvent */
      XIfEvent(mydisplay, &ev, predicate, NULL);
    }
  } else {
    XPutImage(mydisplay, dwin->win, mygc, dwin->ximage, 0, 0,
	      window.video_area.x,
	      window.video_area.y, 
	      window.video_area.width,
	      window.video_area.height);
    XSync(mydisplay, False);
  }
  
}


static void draw_win_xv(window_info *dwin)
{
#ifdef HAVE_XV
  unsigned char *dst;
  
  dst = xv_image->data;
  
  /* Set the source of the xv_image to the source of the image 
     that we want drawn. */ 
  xv_image->data = dwin->image->y;

  if(screenshot) {
    screenshot = 0;
    screenshot_yuv_jpg(dwin->image, dwin->ximage);
  }
    
#ifdef SPU
  if(msgqid != -1) {
    //ugly hack
    if(mix_subpicture_yuv(dwin->image, reserv_image)) {
      xv_image->data = reserv_image->y;
    }
  }
#endif

  if(screenshot_spu) {
    screenshot_spu = 0;
    screenshot_yuv_jpg(dwin->image, dwin->ximage);
  }
  
  window.video_area.width = scale.image_width;
  window.video_area.height = scale.image_height;
  window.video_area.x = (int)(window.window_area.width - 
			      window.video_area.width) / 2;
  window.video_area.y = (int)(window.window_area.height -
			      window.video_area.height) / 2;
  
  XvShmPutImage(mydisplay, xv_port, dwin->win, mygc, xv_image, 
		0, 0, 
		dwin->image->info->picture.horizontal_size,
		dwin->image->info->picture.vertical_size,
		window.video_area.x,
		window.video_area.y,
		window.video_area.width,
		window.video_area.height,
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





