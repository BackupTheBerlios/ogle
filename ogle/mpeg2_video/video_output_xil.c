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
#include <sys/ipc.h>

#include <glib.h>

#include "../include/common.h"
#include "video_types.h"
#include "yuv2rgb.h"

#include <xil/xil.h>
static XilSystemState state;
static XilImage display_image, render_image, three_band_image;
static int image_width, image_height;
static int resized;
static float horizontal_scale, vertical_scale;

int show_stat = 0;

extern buf_ctrl_head_t *buf_ctrl_head;

static XVisualInfo vinfo;
Display *display;
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

  display = XOpenDisplay(NULL);

  if (display == NULL)
    fprintf(stderr,"Can not open display\n");

  screen = DefaultScreen(display);

  hint.x = 0;
  hint.y = 0;
  hint.width = horizontal_size;
  hint.height = vertical_size;
  hint.flags = PPosition | PSize;

  /* Make the window */
  XGetWindowAttributes(display, DefaultRootWindow(display), &attribs);
  bpp = attribs.depth;
  if (bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) {
    fprintf(stderr,"Only 15,16,24, and 32bpp supported. Trying 24bpp!\n");
    bpp = 24;
  }
  
  XMatchVisualInfo(display,screen,bpp,TrueColor,&vinfo);
  printf("visual id is  %lx\n",vinfo.visualid);

  theCmap   = XCreateColormap(display, RootWindow(display,screen), 
			      vinfo.visual, AllocNone);

  xswa.background_pixel = 0;
  xswa.border_pixel     = 1;
  xswa.colormap         = theCmap;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  window = XCreateWindow(display, RootWindow(display,screen),
				 hint.x, hint.y, hint.width, hint.height, 
				 0, bpp, CopyFromParent, vinfo.visual, 
				 xswamask, &xswa);

  resized = 0;

  XSelectInput(display, window, StructureNotifyMask);

  /* Tell other applications about this window */
  XSetStandardProperties(display, window, 
			 hello, hello, None, NULL, 0, &hint);

  /* Map window. */
  XMapWindow(display, window);
  
  // This doesn't work correctly
  /* Wait for map. */
  do {
    XNextEvent(display, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != window);
  
  XFlush(display);
  XSync(display, False);
   
  /* Create the colormaps. */   
  mygc = XCreateGC(display, window, 0L, &xgcv);

  bpp = 32;
   
  yuv2rgb_init(bpp, MODE_BGR);

  /* Setup XIL */
  state = xil_open ();
  if (state == NULL) {
    exit (1);
  }

  /* Create display and render images */
  display_image = xil_create_from_window (state, display, window);
  xil_set_synchronize (display_image, 1);

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
  
  XGetGeometry (display, window, &root, &x, &y, &w, &h, &b, &d);

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
  XSelectInput(display, window, NoEventMask);

  /* Create new display_image */
  xil_destroy (display_image);
  display_image = xil_create_from_window (state, display, window);
  xil_set_synchronize (display_image, 1);

  /* Turn on events */
  XSelectInput (display, window, StructureNotifyMask);
}


void show_image (int id)
{
  XilMemoryStorage storage;
  XEvent event;
  yuv_image_t *image;

  image = &(buf_ctrl_head->picture_infos[id].picture);

  xil_export (render_image);
  xil_get_memory_storage (render_image, &storage);
  yuv2rgb(storage.byte.data, image->y, image->u, image->v,
	  image->padded_width,
	  image->padded_height, 
	  image->padded_width*(bpp/8),
	  image->padded_width, image->padded_width/2 );
  xil_import (render_image, TRUE);
  if (resized) {
    xil_scale (three_band_image, display_image, "bilinear",
               horizontal_scale, vertical_scale);
  } else {
    xil_copy (three_band_image, display_image);
  }
  /* Check if scale factors need recalculating */
  if (XCheckWindowEvent (display, window, StructureNotifyMask, &event))
    resize();
}

/********************* OUTPUT INDEPENDENT CODE AHEAD * *******************/

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
      show_image(buf_id);
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
      show_image(buf_id);
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
