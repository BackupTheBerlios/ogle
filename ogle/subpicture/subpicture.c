/*
 * subt2xpm.c - converts DVD subtitles to an XPM image
 * Copyright (C) 2000   Samuel Hocevar <sam@via.ecp.fr>
 *                       and Michel Lespinasse <walken@via.ecp.fr>
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
 *                                                     
 */


#define DEBUG

#ifndef NDEBUG
#ifndef DEBUG
#define NDEBUG
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

int fd;
int aligned;
uint16_t fieldoffset[2];
uint16_t field=0;

uint8_t color[4];
uint8_t contrast[4];
uint8_t* buffer;

uint16_t x_start;
uint16_t x_end;
uint16_t y_start;
uint16_t y_end;
unsigned int width = 0;
unsigned int height = 0;
unsigned int x;
unsigned int y;

uint16_t spu_size;

/* Variables pertaining to the x window */
XImage *subpicture_image = NULL;
XImage *subpicture_mask = NULL;
uint8_t *data;
uint8_t *mask_data;
Visual* visual;
unsigned int depth = 8;



		
Display *display;
Window win;
int screen_num;
Colormap cmap;
GC gc, gc1;
Pixmap shape_pixmap;

int InitWindow(int x, int y, int w, int h, char *display_name);
void draw_subpicture(int x, int y, int w, int h);
int create_shape(int w, int h);

#ifdef DEBUG
int debug;
#endif

#ifdef DEBUG
#define DPRINTF(level, text...) \
if(debug > level) \
{ \
    fprintf(stderr, ## text); \
}
#else
#define DPRINTF(level, text...)
#endif

#ifdef DEBUG
#define DPRINTBITS(level, bits, value) \
{ \
  int n; \
  for(n = 0; n < bits; n++) { \
    DPRINTF(level, "%u", (value>>(bits-n-1)) & 0x1); \
  } \
}
#else
#define DPRINTBITS(level, bits, value)
#endif

#ifdef DEBUG
#define GETBYTES(a,b) getbytes(a,b)
#else
#define GETBYTES(a,b) getbytes(a)
#endif

static uint8_t *byte_pos;

static inline void set_byte (uint8_t *byte) {
  byte_pos = byte;
}

#ifdef DEBUG
uint32_t getbytes(unsigned int num, char *func)
#else
static inline uint32_t getbytes(unsigned int num)
#endif
{
#ifdef DEBUG
  int tmpnum = num;
#endif
  uint32_t result = 0;

  assert(num <= 4);

  while(num > 0) {
    result <<= 8;
    result |= *byte_pos;
    byte_pos++;
    num--;
  }

  DPRINTF(5, "\n%s getbytes(%u): %i, 0x%0*x, ", 
          func, tmpnum, result, tmpnum*2, result);
  DPRINTBITS(5, tmpnum*8, result);
  DPRINTF(5, "\n");
  return result;
}

static inline uint8_t get_nibble (void)
{
  static uint8_t next = 0;
  if (aligned) {
    fieldoffset[field]++;
    next = GETBYTES(1, "get_nibble (aligned)");
    aligned = 0;
    return next >> 4;
  } else {
    aligned = 1;
    return next & 0xf;
  }
}


/* Start of a new picture */
void initialize() {
  char *apa;
  DPRINTF(5, "initialize()\n");
  if (subpicture_image != NULL) {
    XDestroyImage(subpicture_image);
  } 

  if (subpicture_mask != NULL) {
    XDestroyImage(subpicture_mask);
  } 
 /* Create an XImage to draw in very 8-bitish */

    
  DPRINTF(1, "malloc() data\n");
  apa = malloc(256);
  DPRINTF(1, "size: %d\n", (width+7)/8*8*height);
  data = malloc((width+7)/8*8*height);
  free(apa);
  DPRINTF(1, "malloc() mask_data\n");
  mask_data = malloc((width+7)/8*8*height);

  DPRINTF(5, "XCreateImage() subpicture_image\n");
  subpicture_image = XCreateImage(display, visual, depth, XYPixmap, 0, data,
                              width, height, 8, 0);
  
  DPRINTF(5, "XCreateImage() subpicture_mask\n");
  subpicture_mask = XCreateImage(display, visual, 1, XYBitmap, 0, mask_data,
                              width, height, 8, 0);

}

void send_rle (unsigned int vlc) {
  unsigned int length;
  static unsigned int colorid;
  DPRINTF(4, "send_rle: %08x\n", vlc)
  if(vlc==0) { // new line
    if (y >= height)
      return;
    /* Fill current line with background color */
    length = width-x;
    colorid = 0; 
  } else {  // data
    /* last two bits are colorid, the rest are run length */
    length = vlc >> 2;
    colorid = vlc & 3;
  }
  assert(length != 0);
  while (length-- && (x < width)) {
    DPRINTF(6, "pos: %d, %d, col: %d\n", x, y, color[colorid]);
    XPutPixel(subpicture_image, x, y, color[colorid]);
    if(contrast[colorid]) {
      XPutPixel(subpicture_mask, x, y, 0);
    } else {
      XPutPixel(subpicture_mask, x, y, 1);
    }      
    x++;
  }

  if(x>=width) {
    x=0;
    y++;
    field = 1-field;
    set_byte(&buffer[fieldoffset[field]]);
    if(!aligned)
      get_nibble();
  }
}
char *program_name;

void usage()
{
#ifdef DEBUG
  fprintf(stderr, "Usage: %s [-d <level>] <infile>\n", program_name);
#else
  fprintf(stderr, "Usage: %s <infile>\n", program_name);
#endif
}
int main (int argc, char *argv[]) {
  uint32_t dummy;
  int c;
  uint8_t command;
  uint16_t DCSQT_offset;

  program_name = argv[0];

  /* Parse command line options */
#ifdef DEBUG
  while ((c = getopt(argc, argv, "d:h?")) != EOF) {
#else
  while ((c = getopt(argc, argv, "h?")) != EOF) {
#endif
    
    switch (c) {
#ifdef DEBUG
    case 'd':
      debug = atoi(optarg);
      break;
#endif
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  if(argc - optind != 1){
    usage();
    return 1;
  }
  

  fd = open (argv[optind], 0);
  if(fd < 0) {
    fprintf(stderr,"file not found\n");
    exit(1);
  }

  InitWindow(0,0,100,100,NULL);
  
  buffer = malloc(65536);

  for(;;) {
    
    set_byte(buffer);
    if(!read(fd, buffer, 2)) {
      fprintf(stderr, "EOF\n");
      exit(0);
    }
    spu_size = GETBYTES(2, "spu_size");
    DPRINTF(3, "SPU size: 0x%04x\n", spu_size);
    read(fd, &buffer[2], spu_size-2);
    

    DCSQT_offset = GETBYTES(2, "DCSQT_offset");
    DPRINTF(3, "DCSQT offset: 0x%04x\n", DCSQT_offset);

    DPRINTF(3, "Display Control Sequence Table:\n");

    /* parse the DCSQT */
    set_byte(buffer+DCSQT_offset);
    while (1)
      {
	/* DCSQ */
	int start_time;
	static int next_DCSQ_offset = 0;
	int last_DCSQ = 0;
	DPRINTF(3, "\tDisplay Control Sequence:\n");
	
	start_time = GETBYTES(2, "start_time");
	DPRINTF(3, "\t\tStart time: 0x%04x\n", start_time);
	last_DCSQ = next_DCSQ_offset;
	next_DCSQ_offset = GETBYTES(2, "next_DCSQ_offset");
	DPRINTF(3, "\t\tNext DCSQ offset: 0x%04x\n", next_DCSQ_offset);
	
	DPRINTF(3, "\t\tCommand Sequence Start:\n");
	
	while((command = GETBYTES(1, "command")) != 0xff) {
	  /* Command Sequence */
	  
	  DPRINTF(3, "\t\t\tDisplay Control Command: 0x%02x\n", command);
	  
	  switch (command) {
            case 0x00: /* Menu */
              DPRINTF(3, "\t\t\t\tMenu...\n");
              break;
	    case 0x01: /* display start */
	      DPRINTF(3, "\t\t\t\tdisplay start\n");
	      break;
	    case 0x02: /* display stop */
	      DPRINTF(3, "\t\t\t\tdisplay stop\n");
	      break;
	    case 0x03: /* set colors */
	      DPRINTF(3, "\t\t\t\tset colors");
              dummy = GETBYTES(2, "set_colors");
	      color[0] = dummy      & 0xf;
	      color[1] = (dummy>>4) & 0xf;
	      color[2] = (dummy>>8) & 0xf;
	      color[3] = (dummy>>12);
              DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
                      color[0], color[1], color[2], color[3]);
              DPRINTF(3, "\n");
	      break;
	    case 0x04: /* set contrast */
	      DPRINTF(3, "\t\t\t\tset contrast");
              dummy = GETBYTES(2, "set_contrast");
	      contrast[0] = dummy      & 0xf;
	      contrast[1] = (dummy>>4) & 0xf;
	      contrast[2] = (dummy>>8) & 0xf;
	      contrast[3] = (dummy>>12);
              DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
                      contrast[0], contrast[1], contrast[2], contrast[3]);
              DPRINTF(3, "\n");
	      break;
	    case 0x05: /* set sp screen position */
	      DPRINTF(3, "\t\t\t\tset sp screen position\n");
              dummy = GETBYTES(3, "x coordinates");
              x_start = dummy>>12;
              x_end   = dummy & 0xfff;
              dummy = GETBYTES(3, "y coordinates");
              y_start = dummy>>12;
              y_end   = dummy & 0xfff;
              width  = x_end - x_start + 1;
              height = y_end - y_start + 1;
              DPRINTF(4, "\t\t\t\t\tx_start=%i x_end=%i, y_start=%i, y_end=%i "
                         "width=%i height=%i\n",
                      x_start, x_end, y_start, y_end, width, height);
              break;
	    case 0x06: /* set start address in PXD */
              DPRINTF(3, "\t\t\t\tset start address in PXD\n");
              fieldoffset[0] = GETBYTES(2, "field 0 start");
              fieldoffset[1] = GETBYTES(2, "field 1 start");
              DPRINTF(4, "\t\t\t\t\tfield_0=%i field_1=%i\n",
                  fieldoffset[0], fieldoffset[1]);
              break;
	    default:
              fprintf(stderr,
		      "\t\t\t\t*Unknown Display Control Command: %02x\n",
		      command);
	      exit(1);
	      break;
	    }
	}
	DPRINTF(3, "\t\tEnd of Command Sequence\n");
	
	if(last_DCSQ == next_DCSQ_offset) {
	  DPRINTF(3, "End of Display Control Sequence Table\n");
          break; // out of while loop
	}
	
      }

    field=0;
    aligned = 1;
    set_byte(&buffer[fieldoffset[field]]);

    fprintf(stderr, "\nReading picture data\n");
	 
    initialize();
    x=0;
    y=0;
    DPRINTF(5, "vlc decoding\n");
    while((fieldoffset[1] < DCSQT_offset) && (y < height)) {
      unsigned int vlc;
      DPRINTF(6, "fieldoffset[0]: %d, fieldoffset[1]: %d, DCSQT_offset: %d\n",
	      fieldoffset[0], fieldoffset[1], DCSQT_offset);
      vlc = get_nibble();
      if(vlc < 0x4) {   //  vlc!= 0xN
        vlc = (vlc << 4) | get_nibble();
        if(vlc < 0x10) {  // vlc != 0xN-
          vlc = (vlc << 4) | get_nibble();
          if(vlc < 0x40) {  // vlc != 0xNN-
            vlc = (vlc << 4) | get_nibble();  // vlc != 0xNN--
          }
        }
      }
      send_rle(vlc);
    }
    while (y < height )  // fill
      send_rle(0);
    
    draw_subpicture(x_start, y_start, width, height);
  }
  free(buffer);
  return 0;
}

	
void draw_subpicture(int x, int y, int w, int h)
{
  DPRINTF(3, "w,h: %d, %d\n", w, h);
  XMoveResizeWindow(display, win, x, y, w, h);
  XFlush(display);
  XSync(display, False);
  XPutImage(display, shape_pixmap, gc1, subpicture_mask, 0, 0, 0, 0, w, h);
  XShapeCombineMask(display, win, ShapeBounding, 0, 0, shape_pixmap, ShapeSet);
  XFlush(display);
  XSync(display, False);
  XPutImage(display, win, gc, subpicture_image, 0, 0, 0, 0, w, h);
  XFlush(display);
  XSync(display, False);
  sleep(3);
    
}
					
		


int InitWindow(int x, int y, int w, int h, char *display_name)
{
  XGCValues gcvalues;
  unsigned long gcvaluemask;
  XSetWindowAttributes attr;
  XColor colarr[16] = {
    {
      0,
      0, 0, 0,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      1,
      0, 0, 0,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      2,
      0, 65535, 0,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      3,
      0, 0, 10000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      4,
      0, 6000, 0,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      5,
      10000, 10000, 0,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      6,
      3000, 25000, 5000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      7,
      30000, 0, 6000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      8,
      0, 30000, 14000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      9,
      55000, 55000, 55000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      10,
      6000, 600, 6000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      11,
      140, 14000, 140,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      12,
      255, 255, 25500,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      13,
      2505, 0, 6000,
      (DoRed | DoGreen | DoBlue),
      0
    },

    {
      14,
      2505, 4440, 140,
      (DoRed | DoGreen | DoBlue),
      0
    },
    {
      15,
      255, 65000, 255,
      (DoRed | DoGreen | DoBlue),
      0
    },

  };
#if 0
typedef struct {
        unsigned long pixel;
        unsigned short red, green, blue;
        char flags;  /* do_red, do_green, do_blue */
        char pad;
} XColor;
#endif
 
 
  if ((display = XOpenDisplay(display_name)) == NULL) {
    fprintf(stderr, "Couldn't open display\n");
    exit(-1);
  }
  
  screen_num = DefaultScreen(display);
  

  visual = DefaultVisual(display, screen_num);
  XFlush(display);
  XSync(display, False);
  //    cmap = XCopyColormapAndFree(display, DefaultColormap(display, screen_num));
  cmap = XCreateColormap(display, RootWindow(display, screen_num), visual, AllocAll);
  XFlush(display);
  XSync(display, False);
  
  XStoreColors(display, cmap, colarr, 16);
  XInstallColormap(display, cmap);

  XFlush(display);
 
  
  attr.colormap = cmap;
  win = XCreateWindow(display,
		      RootWindow(display, screen_num),
		      x,y,w,h, 0, 8, InputOutput,
		      visual,
		      CWColormap,
		      &attr);

  gcvalues.foreground = 1;
  gcvalues.background = 0;
  gcvalues.function = GXcopy;
  gcvaluemask = GCForeground | GCBackground |GCFunction;
  gc = XCreateGC(display, win, gcvaluemask, &gcvalues);

  
  XMapWindow(display, win);
  XSync(display,True);
 XFlush(display);
  sleep(1);
 XFlush(display);
  sleep(1);
  
  XDrawLine(display, win,gc, 0,0,100,100);
  XFlush(display);
  create_shape(720, 576);
  gcvalues.foreground = 0;
  gcvalues.background = 1;
  gcvalues.function = GXcopy;
  gcvaluemask = GCForeground | GCBackground |GCFunction;
  gc1 = XCreateGC(display, shape_pixmap, gcvaluemask, &gcvalues);

  XDrawLine(display, shape_pixmap, gc1, 0,0,100,100);
  sleep(1);
  return 0;

}

int create_shape(int w, int h)
{
  shape_pixmap = XCreatePixmap(display, win, w, h, 1);
  XFlush(display);
  return 0;
}

void free_shape()
{
  XFreePixmap(display, shape_pixmap);
  return;
}

