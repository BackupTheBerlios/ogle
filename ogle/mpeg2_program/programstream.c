#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "programstream.h"


#define BUF_SIZE 2048
#define FALSE 0
#define TRUE  1

uint8_t    *buf;
unsigned int buf_len = 0;
unsigned int buf_start = 0;
unsigned int buf_fill = 0;
unsigned int bytes_read = 0;

extern char *optarg;
extern int   optind, opterr, optopt;

/* Variables for getbits */
unsigned int bits_left = 64;
uint64_t     cur_word = 0;

unsigned int nextbits(unsigned int nr_of_bits);

int audio       = 0;
int video       = 0;
int subtitle    = 0;
int subtitle_id = 0;
int debug       = 15;

char *program_name;
FILE *video_file;
FILE *audio_file;
FILE *subtitle_file;
int   infilefd;
int   using_pipe_for_input = FALSE;
void* infileaddr;
uint32_t offs;

void read_buf(void);

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

usage()
{
  fprintf(stderr, "Usage: %s [-v <video file>] [-a <audio file>] [-s <subtitle file> -i <subtitle_id>] <input file>\n", 
	  program_name);
}

resync() {
  fprintf(stderr, "Resyncing\n");
  offs++;
}

/* 2.3 Definition of bytealigned() function */
int bytealigned(void)
{
  return !(bits_left%8);
}

#ifdef DEBUG
#define GETBITS(a,b) getbits(a,b)
#else
#define GETBITS(a,b) getbits(a)
#endif

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
    uint32_t new_word = *(uint32_t *)(&buf[offs]);
    offs+=4;
    DPRINTF(4, " new word %08x\n", new_word);
  if(using_pipe_for_input && (offs >= BUF_SIZE)) {
      read_buf();
      offs = 0;
    }
    cur_word = (cur_word << 32) | new_word;
    bits_left = bits_left+32;
  }
  
  DPRINTF(4, " getbits %08x\n", result);
  return result;
}


void read_buf()
{
  int rv;
  rv = read(infilefd, buf, BUF_SIZE);
  if(rv == -1) {
    perror("read");
    exit(1);
  } else if(rv == 0){
    fprintf(stderr, "**EOF\n");
  }
}


unsigned int nextbits(unsigned int nr_of_bits)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  DPRINTF(4, "nextbits %08x",(result >> (32-nr_of_bits )));
  return result >> (32-nr_of_bits);
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

MPEG2_program_stream()
{

  DPRINTF(2,"MPEG2_program_stream()\n");
  do {
    pack();
  } while(nextbits(32) == MPEG2_PS_PACK_START_CODE); 
  if(GETBITS(32, "MPEG2_PS_PROGRAM_END_CODE") == MPEG2_PS_PROGRAM_END_CODE) {
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

#if 0
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
#endif

pack_header()
{
  uint64_t system_clock_reference_base;
  uint16_t system_clock_reference_extension;
  uint32_t program_mux_rate;
  uint8_t  pack_stuffing_length;
  int i;

  DPRINTF(2, "pack_header()\n");

  GETBITS(32,"pack_start_code");
  GETBITS(2, "01");
  system_clock_reference_base = 
    GETBITS(3,"system_clock_reference_base [32..30]") << 30;
  marker_bit();
  system_clock_reference_base |= 
    GETBITS(15, "system_clock_reference_base [29..15]") << 15;
  marker_bit();
  system_clock_reference_base |= 
    GETBITS(15, "system_clock_reference_base [14..0]");
  marker_bit();
  system_clock_reference_extension = 
    GETBITS(9, "system_clock_reference_extension");
  marker_bit();
  program_mux_rate = GETBITS(22, "program_mux_rate");
  marker_bit();
  marker_bit();
  GETBITS(5, "reserved");
  pack_stuffing_length = GETBITS(3, "pack_stuffing_length");
  for(i=0;i<pack_stuffing_length;i++) {
    GETBITS(8 ,"stuffing_byte");
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

void system_header()
{
  uint16_t header_length;
  uint32_t rate_bound;
  uint8_t  audio_bound;
  uint8_t  fixed_flag;
  uint8_t  CSPS_flag;
  uint8_t  system_audio_lock_flag;
  uint8_t  system_video_lock_flag;
  uint8_t  video_bound;
  uint8_t  stream_id;
  uint8_t  P_STD_buffer_bound_scale;
  uint16_t P_STD_buffer_size_bound;

  DPRINTF(2, "system_header()\n");
  
  GETBITS(32, "system_header_start_code");
  header_length          = GETBITS(16,"header_length");
  marker_bit();
  rate_bound             = GETBITS(22,"rate_bound");
  marker_bit();          
  audio_bound            = GETBITS(6,"audio_bound");
  fixed_flag             = GETBITS(1,"fixed_flag");
  CSPS_flag              = GETBITS(1,"CSPS_flag");
  system_audio_lock_flag = GETBITS(1,"system_audio_lock_flag");
  system_video_lock_flag = GETBITS(1,"system_video_lock_flag");
  marker_bit();
  video_bound            = GETBITS(5,"video_bound");
  GETBITS(8, "reserved_byte");

  DPRINTF(3, "header_length:          %hu\n", header_length);
  DPRINTF(3, "rate_bound:             %u\n", rate_bound);
  DPRINTF(3, "audio_bound:            %u\n", audio_bound);
  DPRINTF(3, "fixed_flag:             %u\n", fixed_flag);
  DPRINTF(3, "CSPS_flag:              %u\n", CSPS_flag);
  DPRINTF(3, "system_audio_lock_flag: %u\n", system_audio_lock_flag);
  DPRINTF(3, "system_video_lock_flag: %u\n", system_video_lock_flag);
  DPRINTF(3, "video_bound:            %u\n", video_bound);
  
  while(nextbits(1) == 1) {
    stream_id = GETBITS(8, "stream_id");
    GETBITS(2, "11");
    P_STD_buffer_bound_scale = GETBITS(1, "P_STD_buffer_bound_scale");
    P_STD_buffer_size_bound  = GETBITS(13, "P_STD_buffer_size_bound");
    DPRINTF(3, "stream_id:                %u\n", stream_id);
    DPRINTF(3, "P-STD_buffer_bound_scale: %u\n", P_STD_buffer_bound_scale);
    DPRINTF(3, "P-STD_buffer_size_bound:  %u\n", P_STD_buffer_size_bound);
  }
}

void PES_packet()
{
  int i = 0;
  int alloced;
  unsigned char *data;
  unsigned int tmp;
  unsigned short PES_packet_length;
  uint8_t stream_id;
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
  uint8_t program_packet_sequence_counter = 0;
  unsigned char P_STD_buffer_flag = 0;
  unsigned char PES_extension_field_flag = 0;
  unsigned char pack_field_length = 0;
  unsigned char PES_extension_field_length = 0;
  unsigned int bytes_read = 0;

  uint8_t original_stuff_length;
  uint8_t P_STD_buffer_scale;
  uint16_t P_STD_buffer_size;
  uint16_t N;
  
  DPRINTF(2, "PES_packet()\n");

  GETBITS(24 ,"packet_start_code_prefix");
  stream_id = GETBITS(8, "stream_id");
  PES_packet_length = GETBITS(16, "PES_packet_length");
  DPRINTF(3, "stream_id:         %02X\n", stream_id);
  DPRINTF(3, "PES_packet_length: %u\n", PES_packet_length);
  
  if((stream_id != MPEG2_PRIVATE_STREAM_2) &&
     (stream_id != MPEG2_PADDING_STREAM)) {

    GETBITS(2, "10");
    PES_scrambling_control    = GETBITS(2, "PES_scrambling_control");
    PES_priority              = GETBITS(1, "PES_priority");
    data_alignment_indicator  = GETBITS(1, "data_alignment_indicator");
    copyright                 = GETBITS(1, "copyright");
    original_or_copy          = GETBITS(1, "original_or_copy");
    PTS_DTS_flags             = GETBITS(2, "PTS_DTS_flags");
    ESCR_flag                 = GETBITS(1, "ESCR_flag");
    ES_rate_flag              = GETBITS(1, "ES_rate_flag");
    DSM_trick_mode_flag       = GETBITS(1, "DSM_trick_mode_flag");
    additional_copy_info_flag = GETBITS(1, "additional_copy_info_flag");
    PES_CRC_flag              = GETBITS(1, "PES_CRC_flag");
    PES_extension_flag        = GETBITS(1, "PES_extension_flag");
    PES_header_data_length    = GETBITS(8, "PES_header_data_length");
    
    bytes_read = 3;
    
    if(PTS_DTS_flags == 0x2) {
      GETBITS(4, "0010");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      
      bytes_read += 5;
    }

    if(PTS_DTS_flags == 0x3) {
      GETBITS(4, "0011");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();

      GETBITS(4, "0001");
      DTS                     = GETBITS(3, "DTS [32..30]")<<30;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [29..15]")<<15;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [14..0]");
      marker_bit();
      
      bytes_read += 10;
    }

    if(ESCR_flag == 0x01) {

      GETBITS(2, "reserved");
      ESCR_base                     = GETBITS(3, "ESCR_base [32..30]")<<30;
      marker_bit();
      ESCR_base                    |= GETBITS(15, "ESCR_base [29..15]")<<15;
      marker_bit();
      ESCR_base                    |= GETBITS(15, "ESCR_base [14..0]");
      marker_bit();
      ESCR_extension                = GETBITS(9, "ESCR_extension");
      marker_bit();

      bytes_read += 6;
    }
    
    if(ES_rate_flag == 0x01) {
      marker_bit();
      ES_rate =                       GETBITS(22, "ES_rate");
      marker_bit();

      bytes_read += 3;
    }
    
    if(DSM_trick_mode_flag == 0x01) {
      trick_mode_control = GETBITS(3, "trick_mode_control");
      
      if(trick_mode_control == 0x00) {
	field_id = GETBITS(2, "field_id");
	intra_slice_refresh = GETBITS(1, "intra_slice_refresh");
	frequency_truncation = GETBITS(2, "frequency_truncation");
      } else if(trick_mode_control == 0x01) {
	field_rep_cntrl = GETBITS(5, "field_rep_cntrl");
      } else if(trick_mode_control == 0x02) {
	field_id = GETBITS(2, "field_id");
	GETBITS(3, "reserved");
      } else if(trick_mode_control == 0x03) {
	field_id = GETBITS(2, "field_id");
	intra_slice_refresh = GETBITS(1, "intra_slice_refresh");
	frequency_truncation = GETBITS(2, "frequency_truncation");
      }

      bytes_read += 1;
    }
    
    if(additional_copy_info_flag == 0x01) {
      marker_bit();
      additional_copy_info = GETBITS(7, "additional_copy_info");

      bytes_read += 1;
    }

    if(PES_CRC_flag == 0x01) {
      previous_PES_packet_CRC = GETBITS(16, "previous_PES_packet_CRC");

      bytes_read += 2;
    }

    if(PES_extension_flag == 0x01) {
      PES_private_data_flag = GETBITS(1, "PES_private_data_flag");
      pack_header_field_flag = GETBITS(1, "pack_header_field_flag");
      program_packet_sequence_counter_flag =
	GETBITS(1, "program_packet_sequence_counter_flag");
      P_STD_buffer_flag = GETBITS(1, "P_STD_buffer_flag");
      GETBITS(3, "reserved");
      PES_extension_field_flag = GETBITS(1, "PES_extension_field_flag");

      bytes_read += 1;
    }

    if(PES_private_data_flag == 0x01) {
      //TODO
      GETBITS(32, "PES_private_data");
      GETBITS(32, "PES_private_data");
      GETBITS(32, "PES_private_data");
      GETBITS(32, "PES_private_data");

      bytes_read += 16;
    }

    if(pack_header_field_flag == 0x01) {
      pack_field_length = GETBITS(8, "pack_field_length");
      pack_header();

      bytes_read += 1;
      bytes_read += pack_field_length;
      /*
	{
	int n;
	for(n = 0; n < pack_field_length; n++) {
	fprintf(stderr, "%02x, ", data[i+n+1]);
	}
	fprintf(stderr, "\n");
	}
      */
    }

    if(program_packet_sequence_counter_flag == 0x01) {
      marker_bit();
      program_packet_sequence_counter = GETBITS(7, "program_packet_sequence_counter");
      marker_bit();
      original_stuff_length = GETBITS(7, "original_stuff_length");
      bytes_read += 2;
    }
    
    if(P_STD_buffer_flag == 0x01) {
      GETBITS(2, "01");
      P_STD_buffer_scale = GETBITS(1, "P_STD_buffer_scale");
      P_STD_buffer_size = GETBITS(13, "P_STD_buffer_size");

      bytes_read += 2;
    }      
    
    if(PES_extension_field_flag == 0x01) {
      int i;
      marker_bit();
      PES_extension_field_length = GETBITS(7, "PES_extension_field_length");
      for(i=0; i<PES_extension_field_length; i++) {
	GETBITS(8, "reserved");
      }

      bytes_read += 1;
      bytes_read += PES_extension_field_length;
    }
    
    N = PES_packet_length - bytes_read;
    
    //FIXME  kolla specen...    
    //FIXME  push pes..    

    
    push_stream_data(stream_id, N);
    
#if 0
    if(stream_id == 0xe0) {
      if(video) {
	push_stream_data(N, video_file);
      } else {
	drop_bytes(N);
      }
      //fwrite(&data[i+0], PES_packet_length-i, 1, video_file);
      //fprintf(stderr, "*len: %u\n", PES_packet_length-i);
    } else if(stream_id == MPEG2_PRIVATE_STREAM_1) {
      if(audio) {

	//if(data[i+0] == 0x80) {
	  
	  //fprintf(stderr, "%02x %02x\n", data[i], data[i+1]);
	// fwrite(&data[i], PES_packet_length-i, 1, audio_file);
	//}
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
#endif
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    push_stream_data(stream_id, PES_packet_length);
    //fprintf(stderr, "*PRIVATE_stream_2\n");
  } else if(stream_id == MPEG2_PADDING_STREAM) {
    //FIXME det st�r N i specen
    drop_bytes(PES_packet_length);
    //fprintf(stderr, "*PADDING_stream\n");
  }
}


void drop_bytes(int len)
{
  int n;
  for(n = 0; n < len; n++) {
    GETBITS(8, "drop");
  }
  //  offs+=len;
  return;

}

void push_stream_data(uint8_t stream_id, int len)
{
  if(stream_id == 0xe0) {
    if(video) {
      fwrite(&buf[offs], len, 1, video_file);
    } else {
      drop_bytes(len);
    }

    //fprintf(stderr, "*len: %u\n", PES_packet_length-i);
  } else if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    if(audio) {
      
      //if(data[i+0] == 0x80) {
      
      //fprintf(stderr, "%02x %02x\n", data[i], data[i+1]);
      // fwrite(&data[i], PES_packet_length-i, 1, audio_file);
      //}
    }
    if(subtitle) {
      //if(data[i] >= 0x20 && data[i]<=0x2f) {
      //fprintf(stderr, "subtitle 0x%02x exists\n", data[i]);
      //}
      //      if(data[i+0] == subtitle_id ){ 
      //	fprintf(stderr, "subtitle %02x %02x\n", data[i], data[i+1]);
      //	fwrite(&data[i+1], PES_packet_length-i-1, 1, subtitle_file);
      // }
  }
  }
  
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
    infilefd = 0;
    using_pipe_for_input = TRUE;
    buf = valloc(BUF_SIZE);
    if(!buf) {
      perror("buf");
      exit(1);
    }
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
    buf = (uint8_t *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, infilefd, 0);
    if(buf == MAP_FAILED) {
      perror("mmap");
      fprintf(stderr,"Using fopen\n");
      using_pipe_for_input = TRUE;
      DPRINTF(1, "using_pipe_for_input = TRUE");
    } else {
      rv = madvise(buf, statbuf.st_size, MADV_SEQUENTIAL);
      if(rv == -1) {
	perror("madvise");
	exit(1);
      }
      DPRINTF(1, "All mmap system ok!\n");
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
  
  GETBITS(32, "skip");
  GETBITS(32, "skip");

  while(1) {
    fprintf(stderr, "Searching for Program Stream\n");
    if(nextbits(32) == MPEG2_PS_PACK_START_CODE) {
      fprintf(stderr, "Found Program Stream\n");
      MPEG2_program_stream();
    }
    GETBITS(8, "");//    resync();
  }
}

