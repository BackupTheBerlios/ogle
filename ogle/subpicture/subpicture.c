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

#include <inttypes.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef WIN32
#define ssize_t int
int read(int, void*, unsigned int);
void exit(int);
int open(char*,int,...);
#else
#include <string.h>
#include <unistd.h>
#endif

int fd;
int aligned;
unsigned int fieldoffset[2];
unsigned int field=0;

uint8_t color[4];
uint8_t contrast[4];
uint8_t* buffer;

const uint8_t hex[]=".123";
uint8_t transferbuffer[10240];

unsigned int width = 0;
unsigned int height = 0;

unsigned int x;
unsigned int y;

int subtitles=0;
int spu_size;

/* Variables pertaining to the x window */
XImage picture_data = NULL;
char *data;

unsigned int get_byte (void)
{
  unsigned char byte;
  byte = buffer[fieldoffset[field]++];
  return byte;
}

unsigned int get_nibble (void)
{
  static int next;
  if (aligned) {
    next = get_byte ();
    aligned = 0;
    return next >> 4;
  } else {
    aligned = 1;
    return next & 0xf;
  }
}


/* Start of a new picture */
void initialize() {
  if (picture_data != null) {
    XDestroyImage(picture_data);
  } 
  char *data = malloc(width*height);
  /* Create an XImage to draw in very 8-bitish*/
  picture_data = XCreateImage(display, visual, depth, XYPixmap, 0, data,
      width, height, 8, 0);
}

void send_rle (unsigned int vlc) {
  unsigned int length;
  static unsigned int colorid;

  if(vlc==0) {   // new line
    if (y >= height)
      return;
    send_rle( ( (width-x) << 2) | 0); //background...

  } else {  // data
    length = vlc >> 2;
    colorid = vlc & 3;

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
}

int main (int argc, char *argv[]) {
  unsigned char dummy;

  int DCSQT_offset;
  int i;
  int old_i;
  ssize_t num;

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

  buffer = (unsigned char *)malloc(65536 * sizeof(unsigned char));

  for(;;) {
    debug_and_rewind(16,0);

    set_byte(buffer);

    spu_size = GETBYTES(2, "spu_size");
    DPRINTF(3, "SPU size: 0x%04x\n", spu_size);

    DCSQT_offset = GETBYTES(2, "DCSQT_offset");
    DPRINTF(3, "    DCSQT offset: 0x%04x\n", DCSQT_offset);

    DPRINTF(3, "Display Control Sequence Table:\n");

    /* parse the DCSQT */
    set_byte(buffer+DCSQT_offset);
    while (i < spu_size)
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
	      DPRINTF(3, "\t\t\t\tdisplay start");
	      break;
	    case 0x02: /* display stop */
	      DPRINTF(3, "\t\t\t\tdisplay stop");
	      break;
	    case 0x03: /* set colors */
	      DPRINTF(3, "\t\t\t\tset colors");
              dummy = GETBYTES(2, "set_colors");
	      color[0] = dummy      & 0xf;
	      color[1] = (dummy>>4) & 0xf;
	      color[2] = (dummy>>8) & 0xf;
	      color[3] = (dummy>>12);
	      break;
	    case 0x04: /* set contrast */
	      DPRINTF(3, "\t\t\t\tset contrast");
              dummy = GETBYTES(2, "set_contrast");
	      contrast[0] = dummy      & 0xf;
	      contrast[1] = (dummy>>4) & 0xf;
	      contrast[2] = (dummy>>8) & 0xf;
	      contrast[3] = (dummy>>12);
	      break;
	    case 0x05: /* set sp screen position */
	      fprintf(stderr, "\t\t\t\tset sp screen position");
              dummy = GETBYTES(3, "x coordinates");
              x_start = dummy>>12;
              x_end   = dummy & 0xfff;
              dummy = GETBYTES(3, "y coordinates");
              y_start = dummy>>12;
              y_end   = dummy & 0xfff;
              width  = x_end - x_start + 1;
              height = y_end - y_start + 1;
              break;
	    case 0x06: /* set start address in PXD */
              fprintf(stderr, "\t\t\t\tset start address in PXD");
              fieldoffset[0] = GETBYTES(2, "field 0 start");
              fieldoffset[1] = GETBYTES(2, "field 1 start");
              break;
	    default:
              fprintf(stderr,
		      "\t\t\t\t*Unknown Display Control Command: %02x\n",
		      command);
	      exit(1);
	      break;
	    }
	}
	fprintf(stderr, "\t\tEnd of Command Sequence\n");
	
	if(last_DCSQ) {
	  fprintf(stderr, "End of Display Control Sequence Table\n");
	  if(spu_size-i != 0) {
	    fprintf(stderr, "Skipping %d bytes to end of SPU\n",
		    spu_size-i);
	    memory_and_rewind(i, 32, 16);
	  }
	  i+=spu_size-i;
	}
      }

    field=0;
    aligned = 1;
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
    printf ("\"\n}\n");
  }

  free(buffer);
  return 0;
}

	
					
					

		

		
    
