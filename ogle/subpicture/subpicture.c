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

#ifndef NDEBUG
#ifndef DEBUG
#define NDEBUG
#endif
#endif

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
XImage *picture_data = NULL;
char *data;
Display* display;
Visual* visual;
unsigned int depth = 8;

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
  uint32_t result = 0;

  assert(num <= 4);

  while(num > 0) {
    result <<= 8;
    result |= *byte_pos;
    byte_pos++;
    num--;
  }
  DPRINTF(5, "%s getbytes(%u): %i, 0x%0*x, ", 
          func, num, result, num*2, result);
  DPRINTBITS(6, num*8, result);
  return result;
}

static inline uint8_t get_nibble (void)
{
  uint8_t next = 0;
  if (aligned) {
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
  if (picture_data != NULL) {
    XDestroyImage(picture_data);
  } 
  /* Create an XImage to draw in very 8-bitish */
  uint8_t *data = malloc(width*height);
  picture_data = XCreateImage(display, visual, depth, XYPixmap, 0, data,
                              width, height, 8, 0);
}

void send_rle (unsigned int vlc) {
  unsigned int length;
  static unsigned int colorid;

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
  while (length-- && (x < width)) {
    data[y*width + x] = color[colorid];
    x++;
  }

  if(x>=width) {
    x=0;
    y++;
    field = 1-field;
    if(!aligned)
      get_nibble();
  }
}

int main (int argc, char *argv[]) {
  uint8_t dummy;

  uint8_t command;
  uint16_t DCSQT_offset;

  if (argc != 2)
    {
      printf("usage: %s file\n", argv[0]);
      exit (1);
    }

  fd = open (argv[1], 0);
  if(fd < 0) {
    fprintf(stderr,"file not found\n");
    exit(1);
  }

  buffer = malloc(65536);

  for(;;) {
    set_byte(buffer);

    spu_size = GETBYTES(2, "spu_size");
    DPRINTF(3, "SPU size: 0x%04x\n", spu_size);

    DCSQT_offset = GETBYTES(2, "DCSQT_offset");
    DPRINTF(3, "DCSQT offset: 0x%04x\n", DCSQT_offset);

    DPRINTF(3, "Display Control Sequence Table:\n");

    /* parse the DCSQT */
    set_byte(buffer+DCSQT_offset);
    while (1)
      {
	/* DCSQ */
	int start_time;
	int next_DCSQ_offset;
	int last_DCSQ = 0;
	DPRINTF(3, "\tDisplay Control Sequence:\n");
	
	start_time = GETBYTES(2, "start_time");
	DPRINTF(3, "\t\tStart time: 0x%04x\n", start_time);

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
	
	if(last_DCSQ) {
	  DPRINTF(3, "End of Display Control Sequence Table\n");
          break; // out of while loop
	}
      }

    field=0;
    aligned = 1;
    set_byte(buffer+4);

    fprintf(stderr, "\nReading picture data\n");
	 
    initialize();
    x=0;
    y=0;
    while(fieldoffset[1] < (unsigned int)DCSQT_offset) {
      unsigned int vlc;
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
  }

  free(buffer);
  return 0;
}

	
					
					

		

		
    
