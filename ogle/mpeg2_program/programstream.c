#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "programstream.h"


#define READ_SIZE 2048
#define ALLOC_SIZE 2048
#define BUF_SIZE_MAX 2048*8


char buf[BUF_SIZE_MAX];
unsigned int buf_len = 0;
unsigned int buf_start = 0;
unsigned int buf_fill = 0;
unsigned int bytes_read = 0;

extern char *optarg;
extern int optind, opterr, optopt;

/* Variables for getbits */
unsigned int bits_left = 64;
uint64_t cur_word = 0;

unsigned int nextbits(unsigned int nr_of_bits);

int audio = 0;
int video = 0;
int subtitle = 0;
int subtitle_id = 0;

char *program_name;
FILE *video_file;
FILE *audio_file;
FILE *subtitle_file;
FILE *infile;

#define DEBUG

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

int   infilefd;
void* infileaddr;

usage()
{
  fprintf(stderr, "Usage: %s [-v <video file>] [-a <audio file>] [-s <subtitle file> -i <subtitle_id>] <input file>\n", 
	  program_name);
}

main(int argc, char **argv)
{
  int c; 
  program_name = argv[0];

  /* Parse command line options */
  while ((c = getopt(argc, argv, "v:a:s:i:h?")) != EOF) {
    switch (c) {
    case 'v':
      video_file = fopen(optarg,"w");
      if(!video_file) {
	  perror(optarg);
	  exit(1);
	}
      video=1;
      break;
    case 'a':
      audio_file = fopen(optarg,"w");
      if(!audio_file) {
	  perror(optarg);
	  exit(1);
	}
      audio=1;
      break;
    case 's':
      subtitle_file = fopen(optarg,"w");
      if(!subtitle_file) {
	  perror(optarg);
	  exit(1);
	}
      subtitle=1;
      break;
    case 'i':
      subtitle_id = atoi(optarg);
      if(! (subtitle_id>=0x20 && subtitle_id<=0x2f)) {
	fprintf(stderr, "Invalid subtitle_id range.\n");
	exit(1);
      } 
      break;
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
  if(!strcmp(argv[optind], "-")) {
    infile = stdin;
  } else {
    struct stat statbuf;
    int rv;

    infilefd = open(argv[optind], O_RDONLY);
    if(infilefd == -1) {
      perror(argv[optind]);
      exit(1);
    }
    rv = fstat(infilefd, &statbuf);
    if (rv == -1) {
      perror("fstat");
      exit(1);
    }
    infileaddr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, infilefd, 0);
    if(infileaddr == MAP_FAILED) {
      perror("mmap");
      exit(1);
    }
    rv = madvise(infileaddr, statbuf.st_size, MADV_SEQUENTIAL);
    if(rv == -1) {
      perror("madvise");
      exit(1);
    }
  }
  //  printf("infile: %s\n",argv[optind]);

  if(subtitle ^ !!subtitle_id) {  // both arguments needed.
    if(!subtitle) {
      fprintf(stderr, "No subtitle filename given.\n");
    } else { 
      fprintf(stderr, "No subtitle_id given.\n");
    }
    exit(1);
  }

  while(1) {
    fprintf(stderr, "Searching for Program Stream\n");
    if(nextbits(32) == MPEG2_PS_PACK_START_CODE) {
      fprintf(stderr, "Found Program Stream\n");
      MPEG2_program_stream();
    }
    resync();
  }

}

resync() {
  fprintf(stderr, "Resyncing\n");
  buf_start = 0;
  buf_fill = 0;
}

/* 2.3 Definition of bytealigned() function */
int bytealigned(void)
{
  return !(bits_left%8);
}

#ifdef DEBUG
uint32_t getbits(unsigned int nr, char *func)
#else
//static inline   
uint32_t getbits(unsigned int nr)
#endif
{
  uint32_t result;

  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  bits_left -=nr;
  if(bits_left <= 32) {
    uint32_t new_word = buf[offs++];
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = (cur_word << 32) | new_word;
    bits_left = bits_left+32;
  }

  return result;
}

unsigned int nextbits(unsigned int nr_of_bits)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  return result >> (32-nr);
}

void marker_bit(void)                                                           {
  if(!GETBITS(1, "markerbit")) {
    fprintf(stderr, "*** incorrect marker_bit in stream\n");
    exit(-1);
  }
}
 
/* 2.3 Definition of next_start_code() function */
void next_start_code(void)
{ 
  while(!bytealigned()) {
    GETBITS(1, "next_start_code");
  } 
  while(nextbits(24) != 0x000001) {
    GETBITS(8, "next_start_code");
  } 
}

get_data()
{
  //  fprintf(stderr, "get_data()\n");
  if(READ_SIZE > buf_len-buf_fill) {
    /* alloc one more buffer */
    if(buf_len < BUF_SIZE_MAX) {
      buf_len += ALLOC_SIZE;
      fprintf(stderr, "Alloc one buffer\n");
    } else {
      fprintf(stderr, "*** BUF_SIZE_MAX exceeded\n");
      exit(0);
    }
  }
  
  if(!fread(&buf[(buf_start+buf_fill)%buf_len], READ_SIZE, 1, infile)) {
    if(feof(stdin)) {
      fprintf(stderr, "*End Of File\n");
      exit(0);
    } else {
      fprintf(stderr, "**File Error\n");
      exit(0);
    }
  } else {
    bytes_read+=READ_SIZE;
    buf_fill+= READ_SIZE;
  }
  //  fprintf(stderr, "start: %d,  fill: %d,  len: %d\n",
  //	  buf_start, buf_fill, buf_len);

}

buf_free(unsigned int nr)
{
  //  fprintf(stderr, "free(%u)\n", nr);

  buf_fill-= nr;
  buf_start+= nr;
  if(buf_start == buf_len) {
    buf_start = 0;
  }
  //  fprintf(stderr, "start: %d,  fill: %d,  len: %d\n",
  //  buf_start, buf_fill, buf_len);
  
}
  
unsigned char inspect_byte(unsigned int offset)
{
  //  fprintf(stderr, "inspect_byte(%u)\n", offset);
  if(offset >= buf_fill) {
    get_data();
  }
  
  return buf[(buf_start+offset)%buf_len];
}

MPEG2_program_stream()
{

  DPRINTF(2,"MPEG2_program_stream()\n");
  do {
    pack();
  } while(nextbits(32) == MPEG2_PS_PACK_START_CODE); 
  if(getbits(32) == MPEG2_PS_PROGRAM_END_CODE) {
    DPRINTF(2, "MPEG Program End\n");
  } else {
    fprintf(stderr, "*** Lost Sync\n");
    fprintf(stderr, "*** after: %u bytes\n", bytes_read);
  }
}
  
  
pack()
{
  unsigned int start_code;
  unsigned char stream_id;
  unsigned char is_PES = 0;

  //fprintf(stderr, "pack()\n");
  
  pack_header();
  while((((start_code = nextbits(32))>>8)&0x00ffffff) ==
	MPEG2_PES_PACKET_START_CODE_PREFIX) {
    
    stream_id = (start_code&0xff);
    
    is_PES = 0;
    if(((stream_id&0xc0) == 0xc0) || ((stream_id&0xe0) == 0x0e0)) {
      is_PES = 1;
    } else {
      switch(stream_id) {
      case 0xBC:
      case 0xBD:
      case 0xBE:
      case 0xBF:
	is_PES = 1;
	break;
      case 0xBA:
				//fprintf(stderr, "Pack Start Code\n");
	is_PES = 0;
	break;
      default:
	is_PES = 0;
	fprintf(stderr, "unknown stream_id: %02x\n", stream_id);
	break;
      }
    }
    if(!is_PES) {
      break;
    }
    
    PES_packet();
    
  }
}

int get_bytes(unsigned int len, unsigned char **data) 
{
  int n;

  //  fprintf(stderr, "get_bytes(%u)\n", len);
  //fprintf(stderr, "start: %d,  fill: %d,  len: %d\n",
  //	  buf_start, buf_fill, buf_len);

  if(len > buf_fill) {
    get_data();
  }

  
  if(buf_start+len > buf_len) {
    /*TODO */
    fprintf(stderr, "Hit a buffer boundary\n");
    exit(0);
  } else {
    *data = &buf[buf_start];
    return 1;
  }
}


pack_header()
{
  uint64_t system_clock_reference_base;
  uint16_t system_clock_reference_extension;
  uint32_t program_mux_rate;
  uint8_t  pack_stuffing_length;

  DPRINTF(2, "pack_header()\n");

  getbits(32); // pack_start_code
  getbits(2); // '01'
  system_clock_reference_base = getbits(3) << 30; // [32..30]
  marker_bit();
  system_clock_reference_base |= getbits(15) << 15; // [29..15]
  marker_bit();
  system_clock_reference_base |= getbits(15); // [14..0]
  marker_bit();
  system_clock_reference_extension = getbits(9);
  marker_bit();
  program_mux_rate = getbits(22);
  marker_bit();
  marker_bit();
  getbits(5); // reserved
  pack_stuffing_length = getbits(3);
  for(i=0;i<pack_stuffing_length;i++) {
    getbits(8); // stuffing_byte
  }
  DPRINTF(3, "system_clock_reference_base: %lu\n",
      system_clock_reference_base);
  DPRINTF(3, "system_clock_reference_extension: %u\n",
      system_clock_reference_extension);
  DPRINTF(3, "program_mux_rate: %u\n",
      program_mux_rate);
  DPRINTF(3, "pack_stuffing_length: %u\n",
      pack_stuffing_length);

  if(nextbits(32) == MPEG2_PS_SYSTEM_HEADER_START_CODE) {
    system_header();
  }
}

system_header()
{
  uint16_t header_length;
  uint32_t rate_bound;
  uint8_t audio_bound;
  uint8_t fixed_flag;
  uint8_t CSPS_flag;
  uint8_t system_audio_lock_flag;
  uint8_t system_video_lock_flag;
  uint8_t video_bound;

  DPRINTF(2, "system_header()\n");
  
  getbits(32); // system_header_start_code
  header_length          = getbits(16);
  marker_bit();
  rate_bound             = getbits(22);
  marker_bit();          
  audio_bound            = getbits(6);
  fixed_flag             = getbits(1);
  CSPS_flag              = getbits(1);
  system_audio_lock_flag = getbits(1);
  system_video_lock_flag = getbits(1);
  marker_bit();
  video_bound            = getbits(5);
  getbits(8); // reserved_byte

  DPRINTF(3, "header_length:          %hu\n", header_length);
  DPRINTF(3, "rate_bound:             %u\n", rate_bound);
  DPRINTF(3, "audio_bound:            %u\n", audio_bound);
  DPRINTF(3, "fixed_flag:             %u\n", fixed_flag);
  DPRINTF(3, "CSPS_flag:              %u\n", CSPS_flag);
  DPRINTF(3, "system_audio_lock_flag: %u\n", system_audio_lock_flag);
  DPRINTF(3, "system_video_lock_flag: %u\n", system_video_lock_flag);
  DPRINTF(3, "video_bound:            %u\n", video_bound);
  
  while(nextbits(1) == 1) {
    stream_id = getbits(8);
    getbits(2); // '11'
    P_STD_buffer_bound_scale = getbits(1);
    P_STD_buffer_size_bound  = getbits(13);
    DPRINTF(3, "stream_id:                %u\n", stream_id);
    DPRINTF(3, "P-STD_buffer_bound_scale: %u\n", P_STD_buffer_bound_scale);
    DPRINTF(3, "P-STD_buffer_size_bound:  %u\n", P_STD_buffer_size_bound);
  }
}

PES_packet()
{
  int i = 0;
  int alloced;
  unsigned char *data;
  unsigned int tmp;
  unsigned short PES_packet_length;
  unsigned char stream_id;
  unsigned char PES_scrambling_control;
  unsigned char PES_priority;
  unsigned char data_alignment_indicator;
  unsigned char copyright;
  unsigned char original_or_copy;
  unsigned char PTS_DTS_flags;
  unsigned char ESCR_flag;
  unsigned char ES_rate_flag;
  unsigned char DSM_trick_mode_flag;
  unsigned char additional_copy_info_flag;
  unsigned char PES_CRC_flag;
  unsigned char PES_extension_flag;
  unsigned char PES_header_data_length;
  
  unsigned long int PTS;
  unsigned long int DTS;
  unsigned long int ESCR_base;
  unsigned short ESCR_extension;
  
  unsigned int ES_rate;
  unsigned char trick_mode_control;
  unsigned char field_id;
  unsigned char intra_slice_refresh;
  unsigned char frequency_truncation;

  unsigned char field_rep_cntrl;

  unsigned char additional_copy_info;
  unsigned short previous_PES_packet_CRC;
  unsigned char PES_private_data_flag = 0;
  unsigned char pack_header_field_flag = 0;
  unsigned char program_packet_sequence_counter_flag = 0;
  unsigned char P_STD_buffer_flag = 0;
  unsigned char PES_extension_field_flag = 0;
  unsigned char pack_field_length = 0;
  unsigned char PES_extension_field_length = 0;
    

  DPRINTF(2, "PES_packet()\n");

  getbits(24); // packat_start_code_prefix;
  stream_id = getbits(8);
  PES_packet_length = getbits(16);
  DPRINTF(3, "stream_id:         %02X\n", stream_id);
  DPRINTF(3, "PES_packet_length: %u\n", PES_packet_length);
  
  if((stream_id != MPEG2_PRIVATE_STREAM_2) &&
     (stream_id != MPEG2_PADDING_STREAM)) {
    
    PES_scrambling_control = (data[i+0]>>4)&0x03;
    PES_priority = (data[i+0]>>3)&0x01;
    data_alignment_indicator = (data[i+0]>>2)&0x01;
    copyright = (data[i+0]>>1)&0x01;
    original_or_copy = (data[i+0])&0x01;
    PTS_DTS_flags = (data[i+1]>>6)&0x03;
    ESCR_flag = (data[i+1]>>5)&0x01;
    ES_rate_flag = (data[i+1]>>4)&0x01;
    DSM_trick_mode_flag = (data[i+1]>>3)&0x01;
    additional_copy_info_flag = (data[i+1]>>2)&0x01;
    PES_CRC_flag = (data[i+1]>>1)&0x01;
    PES_extension_flag = (data[i+1])&0x01;
    PES_header_data_length = data[i+2];
    
    //fprintf(stderr, "scrambling: %01x\n", PES_scrambling_control);
    i+=3;
    
    if(PTS_DTS_flags == 0x02 || PTS_DTS_flags == 0x03) {
      PTS = (data[i+0]&0x0e)<<29 | (data[i+1])<<22 | (data[i+2]&0xfe)<<14 |
	(data[i+3])<<7 | (data[i+4]>>1)&0x7f;
      i+=5;
    }

    if(PTS_DTS_flags == 0x03) {
      DTS = (data[i+0]&0x0e)<<29 | (data[i+1])<<22 | (data[i+2]&0xfe)<<14 |
	(data[i+3])<<7 | (data[i+4]>>1)&0x7f;
      i+=5;
    }

    if(ESCR_flag == 0x01) {
      ESCR_base = (data[i+0]&0x38)<<27 | (data[i+0]&0x03)<<28 |
	(data[i+1])<<20 | (data[i+2]&0xf8)<<12 | (data[i+2]&0x03)<<13 |
	(data[i+3])<<5 | (data[i+4]>>3)&0x1f;
      ESCR_extension = (data[i+4]&0x03)<<7 | (data[i+5]>>1)&0x7f;
      i+=6;
    }
    
    if(ES_rate_flag == 0x01) {
      ES_rate = (data[i+0]&0x7f)<<15 | (data[i+1])<<7 | (data[i+2]>>1)&0x7f;
      i+=3;
    }
    
    if(DSM_trick_mode_flag == 0x01) {
      trick_mode_control = (data[i+0]>>5)&0x07;
      
      if(trick_mode_control == 0x00) {
	field_id = (data[i+0]>>3)&0x03;
	intra_slice_refresh = (data[i+0]>>2)&0x01;
	frequency_truncation = (data[i+0])&0x03;
      } else if(trick_mode_control == 0x01) {
	field_rep_cntrl = data[i+0]&0x1f;
      } else if(trick_mode_control == 0x02) {
	field_id = (data[i+0]>>3)&0x03;
      } else if(trick_mode_control == 0x03) {
	field_id = (data[i+0]>>3)&0x03;
	intra_slice_refresh = (data[i+0]>>2)&0x01;
	frequency_truncation = (data[i+0])&0x03;
      }
      i+=1;
    }
    
    if(additional_copy_info_flag == 0x01) {
      additional_copy_info = (data[i+0])&0x7f;
      i+=1;
    }

    if(PES_CRC_flag == 0x01) {
      previous_PES_packet_CRC = (data[i+0]<<8) | data[i+1];
      i+=2;
    }

    if(PES_extension_flag == 0x01) {
      PES_private_data_flag = (data[i+0]>>7)&0x01;
      pack_header_field_flag = (data[i+0]>>6)&0x01;
      program_packet_sequence_counter_flag = (data[i+0]>>5)&0x01;
      P_STD_buffer_flag = (data[i+0]>>4)&0x01;
      PES_extension_field_flag = (data[i+0])&0x01;
      i+=1;
    }

    if(PES_private_data_flag == 0x01) {
      
      i+=16;
    }

    if(pack_header_field_flag == 0x01) {
      pack_field_length = data[i+0];
      //fprintf(stderr,"*pack_header length: %u\n", pack_field_length);
      /*
	{
	int n;
	for(n = 0; n < pack_field_length; n++) {
	fprintf(stderr, "%02x, ", data[i+n+1]);
	}
	fprintf(stderr, "\n");
	}
      */
      i+=(pack_field_length+1);
    }

    if(program_packet_sequence_counter_flag == 0x01) {
      //fprintf(stderr, "*packet_sequence counter\n");
      i+=2;
    }
    
    if(P_STD_buffer_flag == 0x01) {
      //fprintf(stderr, "*P-STD\n");
      i+=2;
    }      
		
    if(PES_extension_field_flag == 0x01) {
      //fprintf(stderr, "*PES_extension_field\n");
      PES_extension_field_length = data[i+0]&0x7f;
      i+=(PES_extension_field_length+1);
      
    }
    if(stream_id == 0xe0) {
      if(video)
	fwrite(&data[i+0], PES_packet_length-i, 1, video_file);
      //fprintf(stderr, "*len: %u\n", PES_packet_length-i);
    } else if(stream_id == MPEG2_PRIVATE_STREAM_1) {
      if(audio) {
	if(data[i+0] == 0x80) {
	  //fprintf(stderr, "%02x %02x\n", data[i], data[i+1]);
	  fwrite(&data[i], PES_packet_length-i, 1, audio_file);
	}
      }
      if(subtitle) {
	//if(data[i] >= 0x20 && data[i]<=0x2f) {
	//fprintf(stderr, "subtitle 0x%02x exists\n", data[i]);
	//}
	if(data[i+0] == subtitle_id ){ 
	  fprintf(stderr, "subtitle %02x %02x\n", data[i], data[i+1]);
	  fwrite(&data[i+1], PES_packet_length-i-1, 1, subtitle_file);
	}
      }
    }
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    //fprintf(stderr, "*PRIVATE_stream_2\n");
  } else if(stream_id == MPEG2_PADDING_STREAM) {
    //fprintf(stderr, "*PADDING_stream\n");
  }

  
  buf_free(PES_packet_length);
}
  
