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
#include <string.h>

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
#include "debug_print.h"
#include "video_types.h"
#include "yuv2rgb.h"
#include "screenshot.h"
#include "wm_state.h"
#include "display.h"
#include "ffb_asm.h"
#include "video_output.h"
#include "video_output_parse_config.h"

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


extern char *program_name;
//ugly hack
extern data_q_t *cur_data_q;
/*
#ifdef HAVE_XV
extern yuv_image_t *reserv_image;
#endif
*/
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

static XVisualInfo vinfo;
static XShmSegmentInfo shm_info;
static Display *mydisplay = NULL;
static int screen_nr;
static GC mygc;
static char title[100];
static int color_depth, pixel_stride, mode;


static int screenshot = 0;
static int screenshot_spu = 0;




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

extern AspectModeSrc_t aspect_mode;
extern ZoomMode_t zoom_mode;
extern uint16_t aspect_new_frac_n;
extern uint16_t aspect_new_frac_d;

extern MsgEventQ_t *msgq;
extern MsgEventClient_t input_client;
extern InputMask_t input_mask;

/* All display modules shall define these functions */
void display_init(int padded_width, int padded_height,
		  int picture_buffer_shmid,
		  char *picture_buffer_addr);
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
    WARNING("req_code: %d\n", ev->request_code);
    WARNING("minor_code: %d\n", ev->minor_code);
    WARNING("error_code: %d\n", ev->error_code);
    
    use_xshm = 0;
    WARNING("Disabling Xshm\n");
    return 0;
  } else {
    /* if we get another error we should handle it, 
     * so we give it to the previous errorhandler
     */
    ERROR("unexpected error serial: %lu, waited for: %lu\n",
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
static void display_init_xv(int picture_buffer_shmid,
			    char *picture_buffer_addr,
			    int padded_width,
			    int padded_height)
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
    WARNING("Xvideo extension not found\n");
    return;
  }
  
  NOTE("Found Xv extension %d.%d, checking for suitable adaptors\n",
       xv_version, xv_release);
  
  /* Check for available adaptors */
  result = XvQueryAdaptors(mydisplay, DefaultRootWindow (mydisplay), 
			   &xv_num_adaptors, &xv_adaptor_info);
  if(result != Success) {
    WARNING("No Xv adaptors found\n");
    return;
  }
      
  /* Check adaptors */
  for(i = 0; i < xv_num_adaptors; i++) {
    
    /* Is it usable for displaying XvImages */
    if(!(xv_adaptor_info[i].type & XvInputMask) ||
       !(xv_adaptor_info[i].type & XvImageMask))
      continue;
    
    xv_port = xv_adaptor_info[i].base_id;
      
    /* Check image formats of adaptor */
    xv_formats = XvListImageFormats(mydisplay, xv_port, &xv_num_formats);
    for(j = 0; j < xv_num_formats; j++) {
      if(xv_formats[j].id == 0x32315659) { /* YV12 */
	//if(xv_formats[j].id == 0x30323449) { /* I420 */
	xv_id = xv_formats[j].id;
	break;
      } 
    }
    /* No matching format found */
    if(j == xv_num_formats)
      continue;
      
    NOTE("Xv adaptor \"%s\" port %li image format %i\n",
	 xv_adaptor_info[i].name, xv_port, xv_id);
      
    /* Allocate XvImages */
    xv_image = XvShmCreateImage(mydisplay, xv_port, xv_id, NULL,
				padded_width,
				padded_height, 
				&shm_info);
    
    /* Got an Image? */
    if(xv_image == NULL)
      continue;
    
    /* Test and see if we really got padded_width x padded_height */
    if(xv_image->width != padded_width ||
       xv_image->height != padded_height) {
      FATAL("XvShmCreateImage got size: %d x %d\n",
	    xv_image->width, xv_image->height);
      exit(1);
    }
    
    shm_info.shmid = picture_buffer_shmid;
    shm_info.shmaddr = picture_buffer_addr;
    
    /* Set the data pointer to the decoders picture segment. */  
    //    xv_image->data = picture_data->y;
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
    // TODO: should revert to normal X in this case
    FATAL("XShmCreateImage failed\n");
    exit(1);
  }
  
  /* Test and see if we really got padded_width x padded_height */
  if(window.ximage->width != scale.image_width ||
     window.ximage->height != scale.image_height) {
    FATAL("XShmCreateImage got size: %d x %d\n",
	  window.ximage->width, window.ximage->height);
    exit(1);
  }
  
  /* Get a shared memory segment */
  shm_info.shmid = shmget(IPC_PRIVATE,
			  window.ximage->bytes_per_line * 
			  window.ximage->height, 
			  IPC_CREAT | 0777);
  
  if(shm_info.shmid == -1) {
    FATAL("display_init_xshm\n");
    perror("shmget");
    exit(1);
  }
  
  /* Attach shared memory segment */
  shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
  
  if(shm_info.shmaddr == ((char *) -1)) {
    FATAL("display_init_xshm\n");
    perror("shmat");
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

  // If we have blue in the lowest bit then obviously RGB 
  mode = ((window.ximage->blue_mask & 0x01) != 0) ? 1 : 2;
  /*
    #ifdef WORDS_BIGENDIAN 
    if (window.ximage->byte_order != MSBFirst)
    #else
    if (window.ximage->byte_order != LSBFirst) 
    #endif
    {
    FATAL("No support for non-native XImage byte order!\n" );
    exit(1);
    }
  */
  yuv2rgb_init(pixel_stride, mode);
}



void init_config(Display *dpy)
{
  int screen_nr;
  char *dpy_str;
  cfg_video_t *cfg_video = NULL;
  cfg_display_t *cfg_display;
  cfg_display_t *tmp_display;
  DpyInfoOrigin_t orig;
  
  screen_nr = DefaultScreen(dpy);
  dpy_str = DisplayString(dpy);


  
  if(get_video_config(&cfg_video) == 0) {
    ERROR("init_config(): Couldn't read any config files\n");
  }
  
  DpyInfoInit(dpy, screen_nr);
  
  cfg_display = cfg_video->display;
  tmp_display = cfg_display;

  if(dpy_str != NULL) {
    while(cfg_display) {
      if(cfg_display->name != NULL) {
	if(!strcmp(cfg_display->name, dpy_str)) {
	  break;
	}
      } else {
	tmp_display = cfg_display;
      }
      cfg_display = cfg_display->next;
    }
    
    if(cfg_display == NULL) {
      NOTE("NOTE[ogle_vo]: init_config(): using default config for '%s'\n",
	   dpy_str);
      cfg_display = tmp_display;
    }
  }
  
  DpyInfoSetUserGeometry(dpy, screen_nr,
			 cfg_display->geometry.width,
			 cfg_display->geometry.height);

  DpyInfoSetUserResolution(dpy, screen_nr,
			   cfg_display->resolution.horizontal_pixels,
			   cfg_display->resolution.vertical_pixels);
  
  orig = DpyInfoOriginX11;
  
  if(cfg_display->geometry_src) {
    if(!strcmp(cfg_display->geometry_src, "user")) {
      orig = DpyInfoOriginUser;
    } 
  }
  
  orig = DpyInfoSetUpdateGeometry(dpy, screen_nr, orig);
  {
    char *orig_str;
    switch(orig) {
    case DpyInfoOriginX11:
      orig_str = "X11";
      break;
    case DpyInfoOriginUser:
      orig_str = "user";
      break;
    default:
      orig_str = "";
    }
    NOTE("Using '%s' as source for geometry\n",
	 orig_str);
  }

  orig = DpyInfoOriginX11;

  if(cfg_display->resolution_src) {
    if(!strcmp(cfg_display->resolution_src, "user")) {
      orig = DpyInfoOriginUser;
    } else if(!strcmp(cfg_display->resolution_src, "XF86VidMode")) {
      orig = DpyInfoOriginXF86VidMode;
    }
  }
  
  orig = DpyInfoSetUpdateResolution(dpy, screen_nr, orig);
  {
    char *orig_str;
    switch(orig) {
    case DpyInfoOriginX11:
      orig_str = "X11";
      break;
    case DpyInfoOriginXF86VidMode:
      orig_str = "XF86VidMode";
      break;
    case DpyInfoOriginUser:
      orig_str = "user";
      break;
    default:
      orig_str = "";
    }
    NOTE("Using '%s' as source for resolution\n",
	 orig_str);
  }
  
  DpyInfoUpdateGeometry(dpy, screen_nr);
  DpyInfoUpdateResolution(dpy, screen_nr);
  
  /* Query and calculate the displays aspect rate. */
  {
    int width, height;
    int horizontal_pixels, vertical_pixels;
    int dpy_sar_frac_n, dpy_sar_frac_d;

    DpyInfoGetGeometry(dpy, screen_nr, &width, &height);
    DpyInfoGetResolution(dpy, screen_nr, &horizontal_pixels, &vertical_pixels);
    DpyInfoGetSAR(dpy, screen_nr, &dpy_sar_frac_n, &dpy_sar_frac_d);
    
    NOTE("Display w: %d, h: %d, hp: %d, vp: %d\n",
	 width, height, horizontal_pixels, vertical_pixels);
    NOTE("Display sar: %d/%d = %f\n",
	 dpy_sar_frac_n, dpy_sar_frac_d,
	 (double)dpy_sar_frac_n/(double)dpy_sar_frac_d);
  }
}

void display_reset(void)
{
  //DNOTE("DEBUG[vo]: display_reset()\n");
  if(use_xv) {
    XShmDetach(mydisplay, &shm_info);
    
  } else if(use_xshm) {
    
    XShmDetach(mydisplay, &shm_info);
    XDestroyImage(window.ximage);
    if(shmdt(shm_info.shmaddr) == -1) {
      FATAL("display_reset\n");
      perror("shmdt");
      exit(1);
    }
    
    if(shmctl(shm_info.shmid, IPC_RMID, 0) == -1) {
      FATAL("display_reset");
      perror("shmctl");
      exit(1);
    }
    
    memset(&shm_info, 0, sizeof(XShmSegmentInfo));
    
  }

}


void display_init(int padded_width, int padded_height,
		  int picture_buffer_shmid,
		  char *picture_buffer_addr)
{
  static int initialized = 0;
  Screen *scr;
  XSizeHints hint;
  XEvent xev;
  XGCValues xgcv;
  Colormap theCmap;
  XWindowAttributes attribs;
  XSetWindowAttributes xswa;
  unsigned long xswamask;

  scale.image_width = padded_width;
  scale.image_height = padded_height;
  
  if(!initialized) {
    
    if(getenv("USE_FFB2_YUV2RGB")) {
      use_ffb2_yuv2rgb = 1;
    }
    mydisplay = XOpenDisplay(NULL);
    
    if(mydisplay == NULL) {
      FATAL("Cannot open display\n");
      exit(1);
    }
    
    screen_nr = DefaultScreen(mydisplay);
    scr = XDefaultScreenOfDisplay(mydisplay);
    
    init_config(mydisplay);
    
    /* Assume (for now) that the window will be the same size as the source. */
    scale.image_width = padded_width;
    scale.image_height = padded_height;
    
    /* TODO search for correct visual ... */
    /* Make the window */
    XGetWindowAttributes(mydisplay, DefaultRootWindow(mydisplay), &attribs);
    color_depth = attribs.depth;
    if(color_depth != 15 && color_depth != 16 && 
       color_depth != 24 && color_depth != 32) {
      ERROR("Only 15, 16, 24, and 32bpp supported. Trying 24bpp!\n");
      color_depth = 24;
    }
    
    XMatchVisualInfo(mydisplay, screen_nr, color_depth, TrueColor, &vinfo);
    DNOTE("X11 visual id is %lx\n", vinfo.visualid);
    
    hint.x = 0;
    hint.y = 0;
    hint.width = scale.image_width;
    hint.height = scale.image_height;
    hint.flags = PPosition | PSize;
    
    theCmap   = XCreateColormap(mydisplay, RootWindow(mydisplay,screen_nr), 
				vinfo.visual, AllocNone);
    
    xswa.background_pixel = 0;
    xswa.border_pixel     = 1;
    xswa.colormap         = theCmap;
    xswamask = CWBackPixel | CWBorderPixel | CWColormap;
    
    window.win_state = WINDOW_STATE_NORMAL;
    
    window.win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen_nr),
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
    //DNOTE("xshmeventbase: %d\n", xshmeventbase);
    
  }
  
  /* Try to use XFree86 Xv (X video) extension for display.
     Sets use_xv to true on success. */
  display_init_xv(picture_buffer_shmid, picture_buffer_addr,
		  padded_width, padded_height);

  /* Try XShm if we didn't have/couldn't use Xv. 
     This allso falls back to normal X11 if XShm fails. */
  if(!use_xv) {
    display_init_xshm();
  }
  
  if(!initialized) {
    /*** sun ffb2+ ***/
    
    if(use_ffb2_yuv2rgb) {  
      fb_fd = open("/dev/fb", O_RDWR);
      yuyv_fb = (unsigned int *)mmap(NULL, 4*1024*1024, PROT_READ | PROT_WRITE,
				     MAP_SHARED, fb_fd, 0x0701a000);
      if(yuyv_fb == MAP_FAILED) {
	perror("mmap");
	exit(1);
      }
      
      rgb_fb = (uint8_t *)mmap(NULL, 4*2048*1024, PROT_READ | PROT_WRITE,
			       MAP_SHARED, fb_fd, 0x05004000);
      if(rgb_fb == MAP_FAILED) {
	perror("mmap");
	exit(1);
      }
    }
    /*** end sun ffb2+ ***/
    
    /* Let the user know what mode we are running in. */
    snprintf(&title[0], 99, "Ogle v%s %s%s", VERSION, 
	     use_xv ? "Xv " : "", use_xshm ? "XShm " : "");
    XStoreName(mydisplay, window.win, &title[0]);
  }
  initialized = 1;
}





/* TODO display_change_size needs to be fixed */
static void display_change_size(yuv_image_t *img, int new_width, 
				int new_height, int resize_window) {
  int padded_width = img->info->picture.padded_width;
  int padded_height = img->info->picture.padded_height;
  //int alloc_width, alloc_height;
  int alloc_size;
  
  DNOTE("resize: %d, %d\n", new_width, new_height);
  
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
    
    if(shmdt(shm_info.shmaddr) != 0) {
      FATAL("display_change_size");
      perror("shmdt");
      exit(1);
    }
    if(shmctl(shm_info.shmid, IPC_RMID, 0) != 0) {
      FATAL("display_change_size");
      perror("shmctl");
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
      FATAL("XShmCreateImage failed\n");
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
    shm_info.shmid = shmget(IPC_PRIVATE, alloc_size, IPC_CREAT | 0777);

    if(shm_info.shmid == -1) {
      FATAL("display_change_size");
      perror("shmget");
      exit(1);
    }
    
    /* Attach shared memory segment */
    shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
    if(shm_info.shmaddr == ((char *) -1)) {
      FATAL("display_change_size");
      perror("shmat");
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
  int dpy_sar_frac_n, dpy_sar_frac_d;
  int sar_frac_n, sar_frac_d; 
  int64_t scale_frac_n, scale_frac_d;
  int base_width, base_height, max_width, max_height;
  int new_width, new_height;
  
  if(aspect_mode == AspectModeSrcVM) {
    sar_frac_n // hack
      = aspect_new_frac_d * current_image->info->picture.horizontal_size;
    sar_frac_d // hack
      = aspect_new_frac_n * current_image->info->picture.vertical_size;
  }
  /* Use the stream aspect */ 
  else /* if(aspect_mode == AspectModeSrcMPEG) also default */ {
    sar_frac_n = current_image->info->picture.sar_frac_n;
    sar_frac_d = current_image->info->picture.sar_frac_d;
  }

  DpyInfoGetSAR(mydisplay, screen_nr, &dpy_sar_frac_n, &dpy_sar_frac_d);

  // TODO replace image->sar.. with image->dar
  scale_frac_n = (int64_t)dpy_sar_frac_n * (int64_t)sar_frac_d; 
  scale_frac_d = (int64_t)dpy_sar_frac_d * (int64_t)sar_frac_n;
  
#ifdef DEBUG
  DNOTE("vo: sar: %d/%d, dpy_sar %d/%d, scale: %lld, %lld\n",
	sar_frac_n, sar_frac_d,
	dpy_sar_frac_n, dpy_sar_frac_d,
	scale_frac_n, scale_frac_d); 
#endif
  
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
  //DNOTE("base %d x %d\n", base_width, base_height);
  
  /* Do we have a predetermined size for the window? */ 
  if(given_width != -1 && given_height != -1 &&
     (window.win_state != WINDOW_STATE_FULLSCREEN)) {
    max_width  = given_width;
    max_height = given_height;
  } else {
    if(!scale.lock_window_size) {
      /* Never make the window bigger than the screen. */
      DpyInfoGetResolution(mydisplay, screen_nr, &max_width, &max_height);
    } else {
      max_width = window.window_area.width;
      max_height = window.window_area.height;
    }
  }
  //DNOTE("max %d x %d\n", max_width, max_height);
  
  /* Fill the given area or keep the image at the same zoom level? */
  if((window.win_state == WINDOW_STATE_FULLSCREEN) 
     || (given_width != -1 && given_height != -1)) {
    /* Zoom so that the image fill the width. */
    /* If the height gets to large it's adjusted/fixed below. */
    new_width  = max_width;
    new_height = (base_height * max_width) / base_width;
  } else {
    //DNOTE("using zoom %d / %d\n", scale.zoom_n, scale.zoom_d);
    /* Use the provided zoom value. */
    new_width  = (base_width  * scale.zoom_n) / scale.zoom_d;
    new_height = (base_height * scale.zoom_n) / scale.zoom_d;
  }
  //DNOTE("new1 %d x %d\n", new_width, new_height);
  
  /* Don't ever make it larger than the max limits. */
  if(new_width > max_width) {
    new_height = (new_height * max_width) / new_width;
    new_width  = max_width;
  }
  if(new_height > max_height) {
    new_width  = (new_width * max_height) / new_height;
    new_height = max_height;
  }
  //DNOTE("new2 %d x %d\n", new_width, new_height);
  
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
    //DNOTE("zoom2 %d / %d\n", 
    //        scale.zoom_n, scale.zoom_d);
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

  DpyInfoUpdateResolution(mydisplay, screen_nr);
  
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
	    FATAL("keypress\n");
	    perror("MsgSendEvent");
	    display_exit(); //TODO clean up and exit
	    break;
	  default:
	    FATAL("keypress, couldn't send notification\n");
	    perror("MsgSendEvent");
	    display_exit(); //TODO clean up and exit
	    break;
	  }
	}
	//TODO this is just for testing, should be done from vm/ui
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
	    FATAL("buttonpress\n");
	    perror("MsgSendEvent");
	    display_exit(); //TODO clean up and exit
	    break;
	  default:
	    FATAL("buttonpress, couldn't send notification\n");
	    perror("MsgSendEvent");
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
	      FATAL("pointermotion\n");
	      perror("MsgSendEvent");
	      display_exit(); //TODO clean up and exit
	      break;
	    default:
	      FATAL("pointermotion, couldn't send notification\n");
	      perror("MsgSendEvent");
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
  
  if(aspect_mode == AspectModeSrcMPEG) {
    /* New source aspect ratio? */
    if(current_image->info->picture.sar_frac_n != sar_frac_n ||
       current_image->info->picture.sar_frac_d != sar_frac_d) {
      
      sar_frac_n = current_image->info->picture.sar_frac_n;
      sar_frac_d = current_image->info->picture.sar_frac_d;
      
      display_adjust_size(current_image, -1, -1);
    }
  }
  if(aspect_mode == AspectModeSrcVM) {
    /* New VM aspect ratio? */
    if(aspect_new_frac_n != sar_frac_n || aspect_new_frac_d != sar_frac_d) {
      
      sar_frac_n = aspect_new_frac_n;
      sar_frac_d = aspect_new_frac_d;
    
      display_adjust_size(current_image, -1, -1);
    }
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

  if(mydisplay) {
    XSync(mydisplay,True);
    if(use_xshm)
      XShmDetach(mydisplay, &shm_info);
    if(window.ximage != 0)
      XDestroyImage(window.ximage);
    if(shm_info.shmaddr > 0)
      shmdt(shm_info.shmaddr);
    if(shm_info.shmid > 0) {
      shmctl(shm_info.shmid, IPC_RMID, 0);
    }
  }
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
  int sar_frac_n, sar_frac_d; 
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




  if(screenshot || screenshot_spu) {
    if(aspect_mode == AspectModeSrcVM) {
      sar_frac_n // hack
	= aspect_new_frac_d * dwin->image->info->picture.horizontal_size;
      sar_frac_d // hack
	= aspect_new_frac_n * dwin->image->info->picture.vertical_size;
    }
    /* Use the stream aspect */ 
    else /* if(aspect_mode == AspectModeSrcMPEG) also default */ {
      sar_frac_n = dwin->image->info->picture.sar_frac_n;
      sar_frac_d = dwin->image->info->picture.sar_frac_d;
    }
  }


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

    DpyInfoGetResolution(mydisplay, screen_nr,
			 &clip_rect.x1, &clip_rect.y1);

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
		       dwin->image->info->picture.padded_height,
		       sar_frac_n, sar_frac_d);
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
    
    screenshot_yuv_jpg(dwin->image, dwin->ximage, sar_frac_n, sar_frac_d);
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
  /*
    DNOTE("window.video_area. x:%d, y: %d, w:%d, h:%d\n",
	  window.video_area.x,
	  window.video_area.y,
	  window.video_area.width,
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
  int sar_frac_n, sar_frac_d;   
  /* Set the source of the xv_image to the source of the image 
     that we want drawn. */ 
  xv_image->data = dwin->image->y;

  if(screenshot || screenshot_spu) { 
    if(aspect_mode == AspectModeSrcVM) {
      sar_frac_n // hack
	= aspect_new_frac_d * dwin->image->info->picture.horizontal_size;
      sar_frac_d // hack
	= aspect_new_frac_n * dwin->image->info->picture.vertical_size;
    }
    /* Use the stream aspect */ 
    else /* if(aspect_mode == AspectModeSrcMPEG) also default */ {
      sar_frac_n = dwin->image->info->picture.sar_frac_n;
      sar_frac_d = dwin->image->info->picture.sar_frac_d;
    }
  }
  
  if(screenshot) {
    screenshot = 0;
    
    screenshot_yuv_jpg(dwin->image, dwin->ximage, sar_frac_n, sar_frac_d);
  }
    
#ifdef SPU
  if(msgqid != -1) {
    //ugly hack
    if(mix_subpicture_yuv(dwin->image, cur_data_q->reserv_image)) {
      xv_image->data = cur_data_q->reserv_image->y;
    }
  }
#endif

  if(screenshot_spu) {
    screenshot_spu = 0;
    
    screenshot_yuv_jpg(dwin->image, dwin->ximage, sar_frac_n, sar_frac_d);
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

