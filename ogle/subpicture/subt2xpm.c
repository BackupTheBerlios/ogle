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

unsigned char color[4];
unsigned char trans[4];
unsigned char* buffer;

const unsigned char hex[]=".123";
unsigned char transferbuffer[10240];

unsigned int width = 0;
unsigned int height = 0;

unsigned int x;
unsigned int y;

int subtitles=0;
int spu_size;

const unsigned int colortable[16] = {
	0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
	0xffffff, 0xa0a0a0, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
//	0x00a0ff, 0x00ffa0, 0x40ff00, 0xff4000, 0x0040ff, 0x00ff40, 0x20ff00, 0xff2000
};


long lseek(int,long,int);


void debug_and_rewind(int window, int rearview) {
  int k;
  unsigned char debug_c;   //read next 0x10 bytes and rewind.
  off_t incoming_position;
	
  if(window<=0) {
    fprintf(stderr, "debug_and_rewind() window has zero or negative size\n");
    return;
  }

  incoming_position = lseek(fd, (off_t) 0, SEEK_CUR); 

  lseek(fd, (off_t) -rearview, SEEK_CUR);
  fprintf(stderr, "\n");  
  fprintf(stderr, "F0x%04x: ",(int)lseek(fd, 0, SEEK_CUR) );  
  for(k=0; k<window; k++) {
    read (fd, &debug_c, 1);
    fprintf(stderr, "%02x ", debug_c);
  }
  fprintf(stderr, "\n");
  lseek(fd, (off_t) rearview, SEEK_CUR);
  fprintf(stderr, "         ");  
  for(k=0; k<window; k++) {
    if(k==rearview) {
      fprintf(stderr, "^^ ");
    } else {
      fprintf(stderr, "   ");
    }
  }
  fprintf(stderr,"\n");

  lseek(fd,  incoming_position, SEEK_SET); 
}


void memory_and_rewind(int pos, int window, int rearview) {
  int k;
		
  if(window<=0) {
    fprintf(stderr, "memory_and_rewind() window has zero or negative size\n");
    return;
  }

  fprintf(stderr, "\n");  
  fprintf(stderr, "M0x%04x: ", (int) pos);
  for(k=pos-rearview; k< (pos+window-rearview); k++) {
    if(k >= spu_size) {
      fprintf(stderr, "** ");
    } else {
      fprintf(stderr, "%02x ", buffer[k]);
    }
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "         ");  
  for(k=pos-rearview; k< (pos+window-rearview); k++) {
    if(k==pos) {
      fprintf(stderr, "^^ ");
    } else {
      fprintf(stderr, "   ");
    }
  }
  fprintf(stderr,"\n");
}


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


void initialize() {
	unsigned int alpha;
	unsigned int k;
 	unsigned int compensated_color;
	unsigned int r,g,b;
	printf("/* XPM */\n");
  printf("static char *foo[] = {\n");
  printf("/* columns rows colors chars-per-pixel */\n");
  printf("\"%i %i 4 1\",\n", width, height);
	for(k=0; k<4; k++) {
		unsigned int tempcolor = colortable[color[k]];
		alpha=trans[k];
		r = tempcolor & 0xff0000;
		g = tempcolor & 0x00ff00;
		b = tempcolor & 0x0000ff;
		r = (r/(16-alpha)) & 0xff0000;
		g = (g/(16-alpha)) & 0x00ff00;
		b = (b/(16-alpha)) & 0x0000ff;
		compensated_color = r | g | b;
		if(alpha != 0) {
			printf("\"%c c #%06x", hex[k], compensated_color);
		} else {
			printf("\"%c c None", hex[k]);
			//printf("\"%c c #abcdef", hex[k]);
		}
		if(k!=3) {
			printf("\",\n");
		}		
	}
}

void send_rle (unsigned int vlc) {
	unsigned int length;
	static unsigned int colorid;
	
	if(vlc==0) {   // new line
		if (y >= height)
			return;
		//send_rle( ( (width-x) << 2) | colorid); // tidigare 
		send_rle( ( (width-x) << 2) | 0); //background...

	} else {  // data
		length = vlc >> 2;
		colorid = vlc & 3;
		if (x == 0) {
			printf ("\" /* %i */,\n\"", fieldoffset[field]);
		}

		while (length-- && (x < width)) {
			printf ("%c", hex[colorid]);
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
    num = read (fd, &dummy, 1); // subtitle id
    if(num == 0) //EOF
      return 0;
    subtitles++;
    fprintf(stderr,"Subtitle: %d\n",subtitles);
		
    if(dummy > 0x20 && dummy < 0x40) {
      fprintf(stderr,"Subtitle id: 0x%02x at position: 0x%04x\n", dummy, 
	      (int)lseek(fd, 0, SEEK_CUR)-1 );
      read(fd, &dummy, 1);
      spu_size = dummy;
    } else {
      fprintf(stderr, "New SPU, but not a new '0x20' packet\n");
      spu_size = dummy;
    }
    read (fd, &dummy, 1);
    spu_size = (spu_size << 8) + dummy;
    fprintf(stderr, "    SPU size: 0x%04x\n", spu_size);
		
    lseek (fd, -2, SEEK_CUR);
    read (fd, buffer, (unsigned int)spu_size);
	
    DCSQT_offset = (((unsigned int)buffer[2]) << 8) + buffer[3];
    
    fprintf(stderr, "    DCSQT offset: 0x%04x\n", DCSQT_offset);


    fprintf(stderr, "\nDisplay Control Sequence Table:\n");

    /* parse the DCSQT */
    i = DCSQT_offset;
    while (i < spu_size)
      {
	/* DCSQ */
	int start_time;
	int next_DCSQ_offset;
	int last_DCSQ = 0;
	fprintf(stderr, "\n\tDisplay Control Sequence:\n");
	
	start_time = (buffer[i+0] << 8) + buffer[i+1];
	i+=2;
	fprintf(stderr, "\t\tStart time: 0x%04x\n", start_time);

	next_DCSQ_offset = (buffer[i+0] << 8) + buffer[i+1];
	if(next_DCSQ_offset == (i-2)) {
	  last_DCSQ = 1;
	}
	i+=2;
	

	fprintf(stderr, "\t\tNext DCSQ offset: 0x%04x\n", next_DCSQ_offset);
	
	
	fprintf(stderr, "\n\t\tCommand Sequence Start:\n");
	
	while((dummy = buffer[i++]) != 0xff) {
	  /* Command Sequence */
	  
	  fprintf(stderr, "\n\t\t\tDisplay Control Command:\n");
	  
	  old_i=i;
	  switch (dummy)
	    {
	    case 0x01: /* display start */
	      fprintf(stderr, "\t\t\t\t%-30s", "0x01  display start");
	      break;
	    case 0x02: /* display stop */
	      fprintf(stderr, "\t\t\t\t%-30s","0x02 display stop");
	      //	      i = packet_size;
	      //subtitles++;
	      //fprintf(stderr,"i == %d  \t\t packet_size == %d\n",i,packet_size);
	      //read (fd, &dummy,1);
	      //if(dummy != 0xff) {  // Trailing 0xff
	      //		lseek (fd, -1, SEEK_CUR);
	      // }
	      break;
	    case 0x03: /* palette */
	      fprintf(stderr,"\t\t\t\t%-30s", "0x03  palette");
	    	fprintf(stderr,"\t\t\t\t%-30s%02x %02x\n","\t",
					buffer[i], buffer[i+1]);
	      color[0] = buffer[i] >> 4;
	      color[1] = buffer[i++] & 0xf;
	      color[2] = buffer[i] >> 4;
	      color[3] = buffer[i++] & 0xf;
	      break;
	    case 0x04: /* transparency palette */
	      fprintf(stderr, "\t\t\t\t%-30s","0x04  transparency palette");
	    	fprintf(stderr, "\t\t\t\t%-30s%02x %02x\n","\t",
					buffer[i], buffer[i+1]);
	      trans[3] = buffer [i] >> 4;
	      trans[2] = buffer [i++] & 0xf;
	      trans[1] = buffer [i] >> 4;
	      trans[0] = buffer [i++] & 0xf;
	      break;
	    case 0x05: /* image coordinates */
	      fprintf(stderr, "\t\t\t\t%-30s","0x05  image coordinates");
	     	fprintf(stderr, "\t\t\t\t%-30s%02x %02x %02x %02x %02x\n","\t",
						buffer[i], buffer[i+1],buffer[i+2], buffer[i+3], buffer[i+4]);
				width = 1 + ( ((buffer[i+1] & 0x0f) << 8) + buffer[i+2] )
		- ( (((unsigned int)buffer[i+0]) << 4) + (buffer[i+1] >> 4) );
	      height = 1 + ( ((buffer[i+4] & 0x0f) << 8) + buffer[i+5] )
		- ( (((unsigned int)buffer[i+3]) << 4) + (buffer[i+4] >> 4) );
	      i += 6;
	      break;
	    case 0x06: /* image 1 / image 2 offsets */
	      fprintf(stderr, "\t\t\t\t%-30s","0x06  image 1 / image 2 offsets");
	     	fprintf(stderr, "\t\t\t\t%-30s%02x %02x %02x\n","\t",
						buffer[i], buffer[i+1],buffer[i+2]);
				fieldoffset[0] = (((unsigned int)buffer[i+0]) << 8) + buffer[i+1];
	      fieldoffset[1] = (((unsigned int)buffer[i+2]) << 8) + buffer[i+3];
	      i += 4;
	      break;
	    default:
	      fprintf(stderr,
		      "\t\t\t\t*Unknown Display Control Command: %02x\n",
		      dummy);
	      memory_and_rewind(i, 32, 16);
	      exit(1);
	      break;
	    }
	}
	fprintf(stderr, "\n\t\tEnd of Command Sequence\n");
	
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

	
					
					

		

		
    
