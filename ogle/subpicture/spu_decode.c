
//#define DEBUG



#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>


struct timeval tp1, tp2, tp3;

int fd;
int aligned;
uint16_t fieldoffset[2];
uint16_t field=0;

uint8_t color[4];
uint8_t contrast[4];
uint8_t* buffer;



uint16_t spu_size;

		

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
  aligned = 1;
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




char *program_name;





int get_data(uint8_t *databuf, int bufsize)
{
  int r;
  int spu_size;
  int bytes_to_read;
  
  if(bufsize < 2) {
    // databuf not big enough
    fprintf(stderr, "buffer too small\n");
    return -1;
  }
 

  // get first 2 bytes of spu (size of spu)
  bytes_to_read = 2;
  while(bytes_to_read > 0) {
    r = read(fd, &databuf[2-bytes_to_read], bytes_to_read);
    
    if(r > 0) {
      bytes_to_read -= r;
    } else if(r < 0) {
      perror("read");
      return -1;
    } else if(r == 0) {
      fprintf(stderr, "EOF, %d bytes read\n", 2-bytes_to_read);
      return 0;
    }
  }
  
  spu_size = (databuf[0]<<8) | databuf[1];
  
  if(spu_size > bufsize) {
    // databuf not big enough
    fprintf(stderr, "buffer too small\n");
    return -1;
  }
  
  // already read the first two bytes
  bytes_to_read = spu_size - 2;
  
  // get the rest of the spu
  while(bytes_to_read > 0) {
    r = read(fd, &databuf[spu_size-bytes_to_read], bytes_to_read);
    
    if(r > 0) {
      bytes_to_read -= r;
    } else if(r < 0) {
      perror("read");
      return -1;
    } else  {
      fprintf(stderr, "premature EOF, %d of %d bytes read\n",
	      spu_size-bytes_to_read, spu_size);
      return 0;
    }   
  }

  return spu_size;
  
}


typedef struct {
  int start_time;
  int stop_time;
  int width;
  int height;
  int x_start;
  int x_end;
  int y_start;
  int y_end;
  int display_start;
  int display_end;
  int menu;
  uint8_t color[4];
  uint8_t contrast[4];
} spu_t;

char *get_spu_buffer(int size)
{
  return malloc(sizeof(spu_t)+size);
}

  


#define MAX_BUF_SIZE 65536

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
  int next_DCSQ_offset;
  unsigned int x;
  unsigned int y;
  int nr_vlc;
  unsigned char *shm_buffer;
  int shm_buf_pos = 0;
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
  
  buffer = malloc(MAX_BUF_SIZE);
  shm_buffer = malloc(65536*2);

  while(1) {

    int debug_multiple_start = 0;
    spu_t spu_info = { 0 };
    char *spu = NULL;
    unsigned char *spu_data = NULL;
    
    gettimeofday(&tp1, NULL);
    
    if(get_data(buffer, MAX_BUF_SIZE) <= 0) {
      fprintf(stderr, "Couldn't get spu\n");
      exit(-1);
    }
    
    // init start position
    set_byte(buffer);
    
    spu_size = GETBYTES(2, "spu_size");
    DPRINTF(3, "SPU size: 0x%04x\n", spu_size);
    
    DCSQT_offset = GETBYTES(2, "DCSQT_offset");
    DPRINTF(3, "DCSQT offset: 0x%04x\n", DCSQT_offset);
    
    DPRINTF(3, "Display Control Sequence Table:\n");
    
    /* parse the DCSQT */
    set_byte(buffer+DCSQT_offset);
    
    next_DCSQ_offset = DCSQT_offset;
    
    while(1) {
      /* DCSQ */
      int start_time;
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
	  spu_info.menu = 1;
	  break;
	case 0x01: /* display start */
	  DPRINTF(3, "\t\t\t\tdisplay start\n");
	  if(debug_multiple_start) {
	    fprintf(stderr, "Multiple start in spu\n");
	    exit(-1);
	  }
	  debug_multiple_start = 1;
	  spu_info.start_time = start_time;
	  break;
	case 0x02: /* display stop */
	  DPRINTF(3, "\t\t\t\tdisplay stop\n");
	  spu_info.stop_time = start_time;
	  break;
	case 0x03: /* set colors */
	  DPRINTF(3, "\t\t\t\tset colors");
	  dummy = GETBYTES(2, "set_colors");
	  spu_info.color[0] = dummy      & 0xf;
	  spu_info.color[1] = (dummy>>4) & 0xf;
	  spu_info.color[2] = (dummy>>8) & 0xf;
	  spu_info.color[3] = (dummy>>12);
	  DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
		  spu_info.color[0], spu_info.color[1],
		  spu_info.color[2], spu_info.color[3]);
	  DPRINTF(3, "\n");
	  break;
	case 0x04: /* set contrast */
	  DPRINTF(3, "\t\t\t\tset contrast");
	  dummy = GETBYTES(2, "set_contrast");
	  spu_info.contrast[0] = dummy      & 0xf;
	  spu_info.contrast[1] = (dummy>>4) & 0xf;
	  spu_info.contrast[2] = (dummy>>8) & 0xf;
	  spu_info.contrast[3] = (dummy>>12);
	  DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
		  spu_info.contrast[0], spu_info.contrast[1],
		  spu_info.contrast[2], spu_info.contrast[3]);
	  DPRINTF(3, "\n");
	  break;
	case 0x05: /* set sp screen position */
	  DPRINTF(3, "\t\t\t\tset sp screen position\n");
	  dummy = GETBYTES(3, "x coordinates");
	  spu_info.x_start = dummy>>12;
	  spu_info.x_end   = dummy & 0xfff;
	  dummy = GETBYTES(3, "y coordinates");
	  spu_info.y_start = dummy>>12;
	  spu_info.y_end   = dummy & 0xfff;
	  spu_info.width  = spu_info.x_end - spu_info.x_start + 1;
	  spu_info.height = spu_info.y_end - spu_info.y_start + 1;
	  DPRINTF(4, "\t\t\t\t\tx_start=%i x_end=%i, y_start=%i, y_end=%i "
		  "width=%i height=%i\n",
		  spu_info.x_start, spu_info.x_end,
		  spu_info.y_start, spu_info.y_end,
		  spu_info.width, spu_info.height);
	  break;
	case 0x06: /* set start address in PXD */
	  DPRINTF(3, "\t\t\t\tset start address in PXD\n");
	  fieldoffset[0] = GETBYTES(2, "field 0 start");
	  fieldoffset[1] = GETBYTES(2, "field 1 start");
	  DPRINTF(4, "\t\t\t\t\tfield_0=%i field_1=%i\n",
		  fieldoffset[0], fieldoffset[1]);
	  break;
	case 0x07: /* wipe */
	  {
	    uint16_t num;
	    uint8_t color_left[4];
	    uint8_t contrast_left[4];
	    uint8_t color_right[4];
	    uint8_t contrast_right[4];
	    uint16_t y_start;
	    uint16_t y_end;
	    uint16_t x_pos;
	    uint16_t u1, u2, u3; /* x_start, x_end, ? */
	    int type;
	    
	    DPRINTF(3, "\t\t\t\twipe\n");
	    
	    num = GETBYTES(2,"cmd 7 length");
	    
	    y_start = GETBYTES(2, "wipe y_start");

	    dummy = GETBYTES(2, "wipe y_end");
	    type = (dummy >> 12) & 0xf;
	    y_end = dummy & 0xfff;

	    u1 = GETBYTES(2, "wipe unknown 1");

	    dummy = GETBYTES(2, "wipe color_left");
	    color_left[0] = dummy & 0xf;
	    color_left[1] = (dummy >> 4) & 0xf;
	    color_left[2] = (dummy >> 8) & 0xf;
	    color_left[3] = (dummy >> 12) & 0xf;

	    dummy = GETBYTES(2, "wipe contrast_left");
	    contrast_left[0] = dummy & 0xf;
	    contrast_left[1] = (dummy >> 4) & 0xf;
	    contrast_left[2] = (dummy >> 8) & 0xf;
	    contrast_left[3] = (dummy >> 12) & 0xf;

	    if(type == 1) {

	      u2 = GETBYTES(2, "wipe unknown 2");
	      u3 = GETBYTES(2, "wipe unknown 2");

	    } else if(type == 2) {
	      
	      x_pos = GETBYTES(2, "wipe x_pos");
	      
	      dummy = GETBYTES(2, "wipe color_right");
	      color_right[0] = dummy & 0xf;
	      color_right[1] = (dummy >> 4) & 0xf;
	      color_right[2] = (dummy >> 8) & 0xf;
	      color_right[3] = (dummy >> 12) & 0xf;
	      
	      dummy = GETBYTES(2, "wipe contrast_right");
	      contrast_right[0] = dummy & 0xf;
	      contrast_right[1] = (dummy >> 4) & 0xf;
	      contrast_right[2] = (dummy >> 8) & 0xf;
	      contrast_right[3] = (dummy >> 12) & 0xf;
	      
	      
	      u2 = GETBYTES(2, "wipe unknown 2");
	      u3 = GETBYTES(2, "wipe unknown 2");

	    } else {
	      fprintf(stderr, "unknown cmd 07 type\n");
	      exit(-1);
	    }

	    if((u1 != 0) || (u2 != 0x0fff) || (u3 != 0xffff)) {
	      fprintf(stderr, "unknown bits in cmd 7 used\n");
	      exit(-1);
	    }

	    DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
		    contrast_left[0], contrast_left[1],
		    contrast_left[2], contrast_left[3]
		    );
	    DPRINTF(3, "\n");

	    /*
	      for(n = 0; n < num-2; n++) {
	      DPRINTF(4, ":%02x ", (unsigned char) GETBYTES(1, "field 0 start"));
	      }
	    */
	  }
	  break;
	default:
	  fprintf(stderr,
		  "\t\t\t\t*Unknown Display Control Command: %02x\n",
		  command);
	  //exit(1);
	  break;
	}
      }
      DPRINTF(3, "\t\tEnd of Command Sequence\n");
      
      
      if(last_DCSQ == next_DCSQ_offset) {
	DPRINTF(3, "End of Display Control Sequence Table\n");
	break; // out of while loop
      } else {
	set_byte(buffer+next_DCSQ_offset);
      }
    }
    if(spu == NULL) {
      spu = get_spu_buffer(0);
      // spu = get_spu_buffer(spu_info.width*spu_info.height);
      (*(spu_t *)spu) = spu_info;
      // spu_data = spu+sizeof(spu_t); 
    }
    
    gettimeofday(&tp2, NULL);
    
    shm_buf_pos = 0;
    
    field=0;
    aligned = 1;
    set_byte(&buffer[fieldoffset[field]]);
    
    //    fprintf(stderr, "\nReading picture data\n");
    
    //    initialize(spu_info.width, spu_info.height);
    x=0;
    y=0;
    nr_vlc = 0;
    
    DPRINTF(5, "vlc decoding\n");
    while((fieldoffset[1] < DCSQT_offset) && (y < spu_info.height)) {
      unsigned int vlc;
      unsigned int length;
      static unsigned int colorid;
      unsigned char pixel_data;
      
      DPRINTF(6, "fieldoffset[0]: %d, fieldoffset[1]: %d, DCSQT_offset: %d\n",
	      fieldoffset[0], fieldoffset[1], DCSQT_offset);


      /*
	llcc

	01cc - 11cc
	len: 1 - 3

	00ll llcc

	0001 00cc - 0011 11cc
	len: 4 - 15

	
	0000 llll llcc
	
	0000 0100 00cc - 0000 1111 11cc
	len: 16 - 63
	

	0000 00ll llll llcc

	0000 0001 0000 00cc - 0000 0011 1111 11cc
	len: 64 - 255, 0

       */

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
      
      nr_vlc++;
      DPRINTF(4, "send_rle: %08x\n", vlc);
      /* last two bits are colorid, the rest are run length */
      length = vlc >> 2;
      
      colorid = vlc & 3;
      pixel_data = (contrast[colorid] << 4) | (color[colorid] & 0x0f);
      
      if(length==0) { // new line
	//   if (y >= height)
	// return;
	/* Fill current line with background color */
	length = spu_info.width-x;
      }
      if(length+x > spu_info.width) {
	fprintf(stderr, "tried to write past line-end\n");
	length = spu_info.width-x;
      }
      
      shm_buffer[shm_buf_pos++] = pixel_data;
      shm_buffer[shm_buf_pos++] = length;
      
      x = x+length;
      
      if(x >= spu_info.width) {
	x=0;
	y++;
	field = 1-field;
	set_byte(&buffer[fieldoffset[field]]);
      }
      
    }
    gettimeofday(&tp3, NULL);
    printf("%ld.%06ld %ld.%06ld %ld.%06ld\n",
	   tp1.tv_sec, tp1.tv_usec,
	   tp2.tv_sec, tp2.tv_usec,
	   tp3.tv_sec, tp3.tv_usec);

  }
  free(buffer);
  return 0;
}
  

	
