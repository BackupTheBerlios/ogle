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


/* This is a verbal copy of the types from video_strean.c for debug/stats */




typedef struct {
  uint16_t macroblock_type;
  uint8_t spatial_temporal_weight_code;
  uint8_t frame_motion_type;
  uint8_t field_motion_type;
  uint8_t dct_type;

  uint8_t macroblock_quant;
  uint8_t macroblock_motion_forward;
  uint8_t macroblock_motion_backward;
  uint8_t macroblock_pattern;
  uint8_t macroblock_intra;
  uint8_t spatial_temporal_weight_code_flag;
  
} macroblock_modes_t;



typedef struct {
  uint16_t macroblock_escape;
  uint16_t macroblock_address_increment;
  //  uint8_t quantiser_scale_code; // in slice_data
  macroblock_modes_t modes; 
  
  uint8_t pattern_code[12];
  uint8_t cbp;
  uint8_t coded_block_pattern_1;
  uint8_t coded_block_pattern_2;
  uint16_t dc_dct_pred[3];

  int16_t dmv;
  int16_t mv_format;
  int16_t prediction_type;
  
  int16_t dmvector[2];
  int16_t motion_code[2][2][2];
  int16_t motion_residual[2][2][2];
  int16_t vector[2][2][2];


  int16_t f[6][8*8] __attribute__ ((aligned (8)));

  //int16_t f[6][8*8]; // CC

  int8_t motion_vector_count;
  int8_t motion_vertical_field_select[2][2];

  int16_t delta[2][2][2];

  int8_t skipped;

  uint8_t quantiser_scale;
  int intra_dc_mult;

  int16_t QFS[64]  __attribute__ ((aligned (8)));
  //int16_t QF[8][8];
  //int16_t F_bis[8][8];
} macroblock_t;

/* End copy */









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


void exit_program(int);

void draw_win(debug_win *dwin);


void display_init()
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

  int horizontal_size = 720;
  int vertical_size = 480;


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
				      horizontal_size,
				      vertical_size);
  
  windows[1].ximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info_ref1,
				      horizontal_size,
				      vertical_size);
  
  windows[2].ximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info_ref2,
				      horizontal_size,
				      vertical_size);
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
  
   
  //   bpp = myximage->bits_per_pixel;
  // If we have blue in the lowest bit then obviously RGB 
  //mode = ((myximage->blue_mask & 0x01) != 0) ? 1 : 2;
  //#ifdef WORDS_BIGENDIAN 
  // if (myximage->byte_order != MSBFirst)
  //#else
  //  if (myximage->byte_order != LSBFirst) 
  //#endif
  //   {
  //	 fprintf( stderr, "No support for non-native XImage byte order!\n" );
  //	 exit(1);
  //   }
  //   yuv2rgb_init(bpp, mode);
  
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

void Display_Image(Window win, XImage *myximage, unsigned char *ImageData)
{

  XShmPutImage(mydisplay, win, mygc, myximage, 
	       0, 0, 0, 0, myximage->width, myximage->height, 1);
  XFlush(mydisplay);
}




void compute_frame(yuv_image_t *image, unsigned char *data)
{
}
 
void add_grid(unsigned char *data)
{
  int m,n;
  
  for(m = 0; m < 480; m++) {
    if(m%16 == 0) {
      for(n = 0; n < 720*4; n+=4) {
	data[m*720*4+n+1] = 127;
	data[m*720*4+n+2] = 127;
	data[m*720*4+n+3] = 127;
      }
    } else {
      for(n = 0; n < 720*4; n+=16*4) {
	data[m*720*4+n+1] = 127;
	data[m*720*4+n+2] = 127;
	data[m*720*4+n+3] = 127;
      }
    }
  }
}

void add_2_box_sides(unsigned char *data,
		     unsigned char r,
		     unsigned char g,
		     unsigned char b)
{
  int n;
  
  for(n = 0; n < 16*4; n+=4) {
    data[n+1] = b;
    data[n+2] = g;
    data[n+3] = r;
  }
  
  for(n = 0; n < 720*4*16; n+=720*4) {
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

      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      150, 150, 150);
      
    } else if(win->mbs[m].modes.macroblock_intra) {

      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      0, 255, 255);
      
    } else if(win->mbs[m].modes.macroblock_motion_forward &&
	      win->mbs[m].modes.macroblock_motion_backward) {
      
      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      255, 0, 0);
      
    } else if(win->mbs[m].modes.macroblock_motion_forward) {
      
      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      255, 255, 0);

    } else if(win->mbs[m].modes.macroblock_motion_backward) {
      
      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      0, 0, 255);
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

  // TODO move out
  //  windows[0].win = mywindow;
  //  windows[0].data = ImageData;
  //  windows[0].ximage = myximage;
  windows[0].image = current_image;
  windows[0].mbs = cur_mbs;
  //  windows[1].win = window_ref1;
  //  windows[1].data = imagedata_ref1;
  //  windows[1].ximage = ximage_ref1;
  windows[1].image = ref_image1;
  windows[1].mbs = ref1_mbs;
  //  windows[2].win = window_ref2;
  //  windows[2].data = imagedata_ref2;
  //  windows[2].ximage = ximage_ref2;
  windows[2].image = ref_image2;
  windows[2].mbs = ref2_mbs;
  


  switch(picture_coding_type) {
  case 0x1:
  case 0x2:
    if(show_window[1]) {
      draw_win(&windows[1]);
    }
    if(show_window[2]) {
      draw_win(&windows[2]);
    }
    // No "break;" here. (This is right!)
  case 0x3:
    if(show_window[0]) {
      draw_win(&windows[0]);
    }
    break;
  default:
    break;
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

void draw_win(debug_win *dwin)
{
  int horizontal_size = 720;
  int vertical_size = 480;
  
  // Call the 'correct' YUV function !!
  mlib_VideoColorYUV2ABGR420(dwin->data,
			     dwin->image->y,
			     dwin->image->u,
			     dwin->image->v,
			     horizontal_size,
			     vertical_size,
			     horizontal_size*4, //TODO
			     horizontal_size,
			     horizontal_size/2);
  
  if(dwin->grid) {
    if(dwin->color_grid) {
      add_color_grid(dwin);
    } else {
      add_grid(dwin->data);
    }
  }
  
  Display_Image(dwin->win, dwin->ximage, dwin->data);

  return;
}
