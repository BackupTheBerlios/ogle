#include <stdio.h>
#include <stdlib.h>


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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <glib.h>

#include "../include/common.h"
#include "video_types.h"
#include "yuv2rgb.h"


extern buf_ctrl_head_t *buf_ctrl_head;

typedef struct {
  Window win;
  unsigned char *data;
  XImage *ximage;
  yuv_image_t *image;
  int grid;
  int color_grid;
  macroblock_t *mbs;
} debug_win;

debug_win windows[3];


static XVisualInfo vinfo;
XShmSegmentInfo shm_info;
XShmSegmentInfo shm_info_ref1;
XShmSegmentInfo shm_info_ref2;
Display *mydisplay;
Window mywindow;
Window window_stat;
GC mygc;
GC statgc;
int bpp, mode;

extern unsigned int debug;
int show_window[3] = {1,0,0};
int show_stat = 0;
int run = 0;

//static int horizontal_size = 480;
//static int vertical_size = 216;



void exit_program(int);

void draw_win(debug_win *dwin);


void display_init(int padded_width, int padded_height,
		  int horizontal_size, int vertical_size)
{
  int screen;
  char *hello = "I hate X11";
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

  windows[0].win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
				 hint.x, hint.y, hint.width, hint.height, 
				 0, bpp, CopyFromParent, vinfo.visual, 
				 xswamask, &xswa);

  windows[1].win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
				 hint.x, hint.y, hint.width, hint.height, 
				 0, bpp, CopyFromParent, vinfo.visual, 
				 xswamask, &xswa);

  windows[2].win = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
				 hint.x, hint.y, hint.width, hint.height, 
				 0, bpp, CopyFromParent, vinfo.visual, 
				 xswamask, &xswa);

  window_stat = XCreateSimpleWindow(mydisplay, RootWindow(mydisplay,screen),
				    hint.x, hint.y, 200, 200, 0,
				    0, 0);

  XSelectInput(mydisplay, windows[0].win, StructureNotifyMask | KeyPressMask 
	       | ButtonPressMask | ExposureMask);
  XSelectInput(mydisplay, windows[1].win, StructureNotifyMask | KeyPressMask 
	       | ButtonPressMask | ExposureMask);
  XSelectInput(mydisplay, windows[2].win, StructureNotifyMask | KeyPressMask 
	       | ButtonPressMask | ExposureMask);
  XSelectInput(mydisplay, window_stat, StructureNotifyMask | KeyPressMask 
	       | ButtonPressMask | ExposureMask);

  /* Tell other applications about this window */
  XSetStandardProperties(mydisplay, windows[0].win, 
			 hello, hello, None, NULL, 0, &hint);
  XSetStandardProperties(mydisplay, windows[1].win, 
			 "ref1", "ref1", None, NULL, 0, &hint);
  XSetStandardProperties(mydisplay, windows[2].win, 
			 "ref2", "ref2", None, NULL, 0, &hint);
  XSetStandardProperties(mydisplay, window_stat, 
			 "stat", "stat", None, NULL, 0, &hint);

  /* Map window. */
  XMapWindow(mydisplay, window_stat);
  XMapWindow(mydisplay, windows[2].win);
  XMapWindow(mydisplay, windows[1].win);
  XMapWindow(mydisplay, windows[0].win);
  
  
  // This doesn't work correctly
  /* Wait for map. */
  do {
    XNextEvent(mydisplay, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != windows[0].win);
  
  //   XSelectInput(mydisplay, mywindow, NoEventMask);

  XFlush(mydisplay);
  XSync(mydisplay, False);
   
  /* Create the colormaps. */   
  mygc = XCreateGC(mydisplay, windows[0].win, 0L, &xgcv);
  statgc = XCreateGC(mydisplay, window_stat, 0L, &xgcv);
   
  /* Create shared memory image */
  windows[0].ximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info,
				      padded_width,
				      padded_height);
  
  windows[1].ximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info_ref1,
				      padded_width,
				      padded_height);
  
  windows[2].ximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info_ref2,
				      padded_width,
				      padded_height);
  if (windows[0].ximage == NULL || 
      windows[1].ximage == NULL ||
      windows[2].ximage == NULL) {
    fprintf(stderr, "Shared memory: couldn't create Shm image\n");
    goto shmemerror;
  }
  
  /* Get a shared memory segment */
  shm_info.shmid = shmget(IPC_PRIVATE,
			  windows[0].ximage->bytes_per_line * 
			  windows[0].ximage->height, 
			  IPC_CREAT | 0777);
  
  shm_info_ref1.shmid = shmget(IPC_PRIVATE,
			       windows[1].ximage->bytes_per_line * 
			       windows[1].ximage->height, 
			       IPC_CREAT | 0777);
  
  shm_info_ref2.shmid = shmget(IPC_PRIVATE,
			       windows[2].ximage->bytes_per_line * 
			       windows[2].ximage->height, 
			       IPC_CREAT | 0777);
  if (shm_info.shmid < 0 || 
      shm_info_ref1.shmid < 0 || 
      shm_info_ref2.shmid < 0) {
    fprintf(stderr, "Shared memory: Couldn't get segment\n");
    goto shmemerror;
  }
  
  /* Attach shared memory segment */
  shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
  shm_info_ref1.shmaddr = (char *) shmat(shm_info_ref1.shmid, 0, 0);
  shm_info_ref2.shmaddr = (char *) shmat(shm_info_ref2.shmid, 0, 0);
  if (shm_info.shmaddr == ((char *) -1) || 
      shm_info_ref1.shmaddr == ((char *) -1) ||
      shm_info_ref2.shmaddr == ((char *) -1)) {
    fprintf(stderr, "Shared memory: Couldn't attach segment\n");
    goto shmemerror;
  }
  
  windows[0].ximage->data = shm_info.shmaddr;
  windows[1].ximage->data = shm_info_ref1.shmaddr;
  windows[2].ximage->data = shm_info_ref2.shmaddr;
  shm_info.readOnly = False;
  shm_info_ref1.readOnly = False;
  shm_info_ref2.readOnly = False;
  XShmAttach(mydisplay, &shm_info);
  XShmAttach(mydisplay, &shm_info_ref1);
  XShmAttach(mydisplay, &shm_info_ref2);
  XSync(mydisplay, 0);

  windows[0].data = windows[0].ximage->data;
  windows[1].data = windows[1].ximage->data;
  windows[2].data = windows[2].ximage->data;
  
  
  bpp = windows[0].ximage->bits_per_pixel;
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
  yuv2rgb_init(bpp, mode);
  
  return;
  
 shmemerror:
  // exit_program();
  exit(1);

}

void display_exit(void) 
{
  // Need to add some test to se if we can detatch/free/destroy things
  XShmDetach(mydisplay, &shm_info);
  XShmDetach(mydisplay, &shm_info_ref1);
  XShmDetach(mydisplay, &shm_info_ref2);
  XDestroyImage(windows[0].ximage);
  XDestroyImage(windows[1].ximage);
  XDestroyImage(windows[2].ximage);
  shmdt(shm_info.shmaddr);
  shmdt(shm_info_ref1.shmaddr);
  shmdt(shm_info_ref2.shmaddr);
  shmctl(shm_info.shmid, IPC_RMID, 0);
  shmctl(shm_info_ref1.shmid, IPC_RMID, 0);
  shmctl(shm_info_ref2.shmid, IPC_RMID, 0);
}

void Display_Image(Window win, XImage *myximage,
		   unsigned char *ImageData, yuv_image_t *image)
{

  XShmPutImage(mydisplay, win, mygc, myximage, 
	       0, 0, 0, 0, image->horizontal_size, image->vertical_size, 1);

  XSync(mydisplay, False);
}




void compute_frame(yuv_image_t *image, unsigned char *data)
{
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
		     unsigned char r,
		     unsigned char g,
		     unsigned char b,
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


void frame_done(yuv_image_t *current_image, macroblock_t *cur_mbs,
		yuv_image_t *ref_image1, macroblock_t *ref1_mbs,
		yuv_image_t *ref_image2, macroblock_t *ref2_mbs, 
		uint8_t picture_coding_type)
{
  XEvent ev;
  int nextframe = 0;  

  windows[0].image = current_image;
  windows[0].mbs = cur_mbs;

  windows[1].image = current_image;
  windows[1].mbs = ref1_mbs;

  windows[2].image = current_image;
  windows[2].mbs = ref2_mbs;
  

  if(show_window[1]) {
    draw_win(&windows[1]);
  }
  if(show_window[2]) {
    draw_win(&windows[2]);
  }
  if(show_window[0]) {
    draw_win(&windows[0]);
  }
  while(!nextframe) {

    if(run) {
      nextframe = 1;
      if(XCheckMaskEvent(mydisplay, 0xFFFFFFFF, &ev) == False) {
	continue;
      }
    } else {
      XNextEvent(mydisplay, &ev);
    }
    
    switch(ev.type) {
    case Expose:
      {
	int n;
	for(n = 0; n < 3; n++) {
	  if(windows[n].win == ev.xexpose.window) {
	    if(show_window[n]) {
	      draw_win(&(windows[n]));
	    }
	    break;
	  }
	}
      }
      break;
    case ButtonPress:
      switch(ev.xbutton.button) {
      case 0x1:
	{
	  char text[64];
	  int n;
	  for(n = 0; n < 3; n++) {
	    if(windows[n].win == ev.xbutton.window) {
	      break;
	    }
	  }

	  snprintf(text, 16, "%4d, %4d", ev.xbutton.x, ev.xbutton.y);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 0*13, text, strlen(text));
	  snprintf(text, 16, "block: %4d", ev.xbutton.x/16+ev.xbutton.y/16*45);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 1*13, text, strlen(text));
	  snprintf(text, 16, "intra: %3s", (windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].modes.macroblock_intra ? "yes" : "no"));
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 2*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 3*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 4*13, text, strlen(text));


	  snprintf(text, 32, "vector[1][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 5*13, text, strlen(text));

	  snprintf(text, 32, "vector[1][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 6*13, text, strlen(text));


	  snprintf(text, 32, "vector[0][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 7*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 8*13, text, strlen(text));


	  snprintf(text, 32, "vector[1][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 9*13, text, strlen(text));

	  snprintf(text, 32, "vector[1][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 10*13, text, strlen(text));



	  snprintf(text, 32, "field_select[0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 11*13, text, strlen(text));

	  snprintf(text, 32, "field_select[0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 12*13, text, strlen(text));

	  snprintf(text, 32, "field_select[1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 13*13, text, strlen(text));

	  snprintf(text, 32, "field_select[1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 14*13, text, strlen(text));

	  snprintf(text, 32, "motion_vector_count: %2d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vector_count);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 15*13, text, strlen(text));


	  snprintf(text, 32, "delta[0][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 16*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 17*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 18*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 19*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 20*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 21*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 22*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 23*13, text, strlen(text));

	  snprintf(text, 32, "skipped: %2d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].skipped);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 24*13, text, strlen(text));

	}
	break;
      case 0x2:
	break;
      case 0x3:
	break;
      }
      break;
    case KeyPress:
      {
	int n;
	char buff[2];
	static int debug_change = 0;
	static int window_change = 0;

	XLookupString(&(ev.xkey), buff, 2, NULL, NULL);
	buff[1] = '\0';
	switch(buff[0]) {
	case 'n':
	  nextframe = 1;
	  break;
	case 'g':
	  for(n = 0; n < 3; n++) {
	    if(ev.xkey.window == windows[n].win) {
	      windows[n].grid = !windows[n].grid;
	      if(show_window[n]) {
		draw_win(&(windows[n]));
	      }
	      break;
	    }
	  }
	  break;
	case 'c':
	  for(n = 0; n < 3; n++) {
	    if(ev.xkey.window == windows[n].win) {
	      windows[n].color_grid = !windows[n].color_grid;
	      if(show_window[n]) {
		draw_win(&(windows[n]));
	      }
	      break;
	    }
	  }
	  break;
	case 'd':
	  debug_change = 2;
	  break;
	case 'w':
	  window_change = 2;
	  break;
	case 's':
	  show_stat = !show_stat;
	  break;
	case 'r':
	  run = !run;
	  nextframe = 1;
	  break;
	case 'q':
	  exit_program(0);
	  break;
	default:
	  if(debug_change) {
	    debug = atoi(&buff[0]);
	  }
	  if(window_change) {
	    int w;
	    w = atoi(&buff[0]);
	    if((w >= 0) && (w < 3)) {
	      show_window[w] = !show_window[w];
	    }
	  }
	  break;
	    
	}
	if(debug_change > 0) {
	  debug_change--;
	}
	if(window_change > 0) {
	  window_change--;
	}
      }
      break;
    default:
      break;
    }
      

  }

  return;
}



void user_control(macroblock_t *cur_mbs,
		  macroblock_t *ref1_mbs,
		  macroblock_t *ref2_mbs)
{
  XEvent ev;
  int nextframe = 0;  
  
  windows[0].mbs = cur_mbs;
  
  windows[1].mbs = ref1_mbs;

  windows[2].mbs = ref2_mbs;
  
  fprintf(stderr, "*** user_control\n");

  while(!nextframe) {

    if(run) {
      nextframe = 1;
      if(XCheckMaskEvent(mydisplay, 0xFFFFFFFF, &ev) == False) {
	continue;
      }
    } else {
      XNextEvent(mydisplay, &ev);
    }
    
    switch(ev.type) {
    case Expose:
      /*
      {
	int n;
	for(n = 0; n < 3; n++) {
	  if(windows[n].win == ev.xexpose.window) {
	    if(show_window[n]) {
	      draw_win(&(windows[n]));
	    }
	    break;
	  }
	}
      }
      */
      break;
    case ButtonPress:
      switch(ev.xbutton.button) {
      case 0x1:
	{
	  char text[64];
	  int n;
	  for(n = 0; n < 3; n++) {
	    if(windows[n].win == ev.xbutton.window) {
	      break;
	    }
	  }

	  snprintf(text, 16, "%4d, %4d", ev.xbutton.x, ev.xbutton.y);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 0*13, text, strlen(text));
	  snprintf(text, 16, "block: %4d", ev.xbutton.x/16+ev.xbutton.y/16*45);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 1*13, text, strlen(text));
	  snprintf(text, 16, "intra: %3s", (windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].modes.macroblock_intra ? "yes" : "no"));
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 2*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 3*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 4*13, text, strlen(text));


	  snprintf(text, 32, "vector[1][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 5*13, text, strlen(text));

	  snprintf(text, 32, "vector[1][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 6*13, text, strlen(text));


	  snprintf(text, 32, "vector[0][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 7*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 8*13, text, strlen(text));


	  snprintf(text, 32, "vector[1][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 9*13, text, strlen(text));

	  snprintf(text, 32, "vector[1][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 10*13, text, strlen(text));



	  snprintf(text, 32, "field_select[0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 11*13, text, strlen(text));

	  snprintf(text, 32, "field_select[0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 12*13, text, strlen(text));

	  snprintf(text, 32, "field_select[1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 13*13, text, strlen(text));

	  snprintf(text, 32, "field_select[1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 14*13, text, strlen(text));

	  snprintf(text, 32, "motion_vector_count: %2d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vector_count);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 15*13, text, strlen(text));


	  snprintf(text, 32, "delta[0][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 16*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 17*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 18*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 19*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 20*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 21*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 22*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 23*13, text, strlen(text));

	  snprintf(text, 32, "skipped: %2d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].skipped);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 24*13, text, strlen(text));

	}
	break;
      case 0x2:
	break;
      case 0x3:
	break;
      }
      break;
    case KeyPress:
      {
	int n;
	char buff[2];
	static int debug_change = 0;
	static int window_change = 0;

	XLookupString(&(ev.xkey), buff, 2, NULL, NULL);
	buff[1] = '\0';
	switch(buff[0]) {
	case 'n':
	  nextframe = 1;
	  break;
	case 'g':
	  for(n = 0; n < 3; n++) {
	    if(ev.xkey.window == windows[n].win) {
	      windows[n].grid = !windows[n].grid;
	      if(show_window[n]) {
		draw_win(&(windows[n]));
	      }
	      break;
	    }
	  }
	  break;
	case 'c':
	  for(n = 0; n < 3; n++) {
	    if(ev.xkey.window == windows[n].win) {
	      windows[n].color_grid = !windows[n].color_grid;
	      if(show_window[n]) {
		draw_win(&(windows[n]));
	      }
	      break;
	    }
	  }
	  break;
	case 'd':
	  debug_change = 2;
	  break;
	case 'w':
	  window_change = 2;
	  break;
	case 's':
	  show_stat = !show_stat;
	  break;
	case 'r':
	  run = !run;
	  nextframe = 1;
	  break;
	case 'q':
	  exit_program(0);
	  break;
	default:
	  if(debug_change) {
	    debug = atoi(&buff[0]);
	  }
	  if(window_change) {
	    int w;
	    w = atoi(&buff[0]);
	    if((w >= 0) && (w < 3)) {
	      show_window[w] = !show_window[w];
	    }
	  }
	  break;
	    
	}
	if(debug_change > 0) {
	  debug_change--;
	}
	if(window_change > 0) {
	  window_change--;
	}
      }
      break;
    default:
      break;
    }
      

  }

  return;
}



void draw_win(debug_win *dwin)
{

  yuv2rgb(dwin->data, dwin->image->y, dwin->image->u, dwin->image->v,
	  dwin->image->padded_width,
	  dwin->image->padded_height, 
	  dwin->image->padded_width*(bpp/8),
	  dwin->image->padded_width, dwin->image->padded_width/2 );
  
  if(dwin->grid) {
    if(dwin->color_grid) {
      add_color_grid(dwin);
    } else {
      add_grid(dwin->data, dwin->ximage);
    }
  }

  
  Display_Image(dwin->win, dwin->ximage, dwin->data, dwin->image);

  return;
}



int get_next_picture_buf_id()
{
  int id;
  static int dpy_q_get_pos = 0;
  
  if(sem_wait(&(buf_ctrl_head->pictures_ready_to_display)) == -1) {
    perror("sem_wait get_next_picture_buf_id");
  }

  id = buf_ctrl_head->dpy_q[dpy_q_get_pos];
  dpy_q_get_pos = (dpy_q_get_pos + 1) % (buf_ctrl_head->nr_of_buffers);
  
  return id;
}



void release_picture_buf(int id)
{
  sem_post(&(buf_ctrl_head->pictures_displayed));
  return;
}



void display(int id)
{
  frame_done(&(buf_ctrl_head->picture_infos[id].picture), NULL,
	     NULL, NULL,
	     NULL, NULL,
	     1);
}


int timecompare(struct timespec *s1, struct timespec *s2) {

  if(s1->tv_sec > s2->tv_sec) {
    return 1;
  } else if(s1->tv_sec < s2->tv_sec) {
    return -1;
  }

  if(s1->tv_nsec > s2->tv_nsec) {
    return 1;
  } else if(s1->tv_nsec < s2->tv_nsec) {
    return -1;
  }
  
  return 0;
}

void timesub(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1-s2
  //  s1 is greater than s2
  if(timecompare(s1, s2) > 0) {
    d->tv_sec = s1->tv_sec - s2->tv_sec;
    d->tv_nsec = s1->tv_nsec - s2->tv_nsec;
    if(d->tv_nsec < 0) {
      d->tv_nsec +=1000000000;
      d->tv_sec -=1;
    }
  } else {
    d->tv_sec = s2->tv_sec - s1->tv_sec;
    d->tv_nsec = s2->tv_nsec - s1->tv_nsec;
    if(d->tv_nsec < 0) {
      d->tv_nsec +=1000000000;
      d->tv_sec -=1;
    }
    d->tv_sec = -d->tv_sec;
    d->tv_nsec = -d->tv_nsec;
  }    
}  

void timeadd(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1+s2
  
  d->tv_sec = s1->tv_sec + s2->tv_sec;
  d->tv_nsec = s1->tv_nsec + s2->tv_nsec;
  if(d->tv_nsec >= 1000000000) {
    d->tv_nsec -=1000000000;
    d->tv_sec +=1;
  }
}  


void display_process()
{
  struct timespec real_time, prefered_time, wait_time, frame_interval;
  struct timespec real_time2, diff, avg_time, oavg_time;
  int buf_id;
  int drop = 0;
  int frame_nr = 0;
  int avg_nr = 23;
  int first = 1;
  frame_interval.tv_sec = 0;
  //frame_interval.tv_nsec = 28571428; //35 fps
  //frame_interval.tv_nsec = 33366700; // 29.97 fps
  frame_interval.tv_nsec = 41666666; //24 fps
  prefered_time.tv_sec = 0;

  while(1) {
    buf_id = get_next_picture_buf_id();
    frame_interval.tv_nsec = buf_ctrl_head->frame_interval;
    if(first) {
      first = 0;
      display_init(buf_ctrl_head->picture_infos[buf_id].picture.padded_width,
		   buf_ctrl_head->picture_infos[buf_id].picture.padded_height,
		   buf_ctrl_head->picture_infos[buf_id].picture.horizontal_size,
		   buf_ctrl_head->picture_infos[buf_id].picture.vertical_size);
      display(buf_id);
    }
    clock_gettime(CLOCK_REALTIME, &real_time);
    if(prefered_time.tv_sec == 0) {
      prefered_time = real_time;
    }
    timesub(&wait_time, &prefered_time, &real_time);
    /*
    fprintf(stderr, "pref: %d.%09ld\n",
	    prefered_time.tv_sec, prefered_time.tv_nsec);

    fprintf(stderr, "real: %d.%09ld\n",
	    real_time.tv_sec, real_time.tv_nsec);

    fprintf(stderr, "wait: %d.%09ld, ",
	    wait_time.tv_sec, wait_time.tv_nsec);
    */     

    if((wait_time.tv_nsec > 5000000) && (wait_time.tv_sec >= 0)) {
      nanosleep(&wait_time, NULL);
    } else {
      //fprintf(stderr, "---less than 0.005 s\n");
    }
    
    frame_nr++;
    avg_nr++;
    if(avg_nr == 24) {
      avg_nr = 0;
      oavg_time = avg_time;
      clock_gettime(CLOCK_REALTIME, &avg_time);
      
      fprintf(stderr, "display: frame rate: %.3f fps\t",
	      24.0/(((double)avg_time.tv_sec+
		     (double)(avg_time.tv_nsec)/1000000000.0)-
		    ((double)oavg_time.tv_sec+
		     (double)(oavg_time.tv_nsec)/1000000000.0))
	      );
    }
      
    clock_gettime(CLOCK_REALTIME, &real_time2);
    timesub(&diff, &prefered_time, &real_time2);
    /*
    fprintf(stderr, "diff: %d.%09ld\n",
	    diff.tv_sec, diff.tv_nsec);
    */
    /*
    fprintf(stderr, "rt: %d.%09ld, pt: %d.%09ld, diff: %d.%+010ld\n",
	    real_time2.tv_sec, real_time2.tv_nsec,
	    prefered_time.tv_sec, prefered_time.tv_nsec,
	    diff.tv_sec, diff.tv_nsec);
    */
    if(drop) {
      if(wait_time.tv_nsec > -5000000) {
	fprintf(stderr, "$ %+010ld \n", wait_time.tv_nsec);
	drop = 0;
      } else {
	fprintf(stderr, "# %+010ld \n", wait_time.tv_nsec);
      }
    }
    if(!drop) {
      display(buf_id);
    } else {
      fprintf(stderr, "* \n");
      drop = 0;
    }
    timeadd(&prefered_time, &prefered_time, &frame_interval);
    if(wait_time.tv_nsec < 0) {
      drop = 1;
    }
    //    fprintf(stderr, "display: done with buf %d\n", buf_id);
    release_picture_buf(buf_id);
  }
}
