#include <stdio.h>
#include <stdlib.h>
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
      video=1;
      break;
    case 'a':
      audio_file = fopen(optarg,"w");
      audio=1;
      break;
    case 's':
      subtitle_file = fopen(optarg,"w");
      subtitle=1;
      break;
    case 'i':
      subtitle_id =atoi(optarg);
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
  if(!strcmp(argv[optind], "-"))
    infile = stdin;
  else
    infile = fopen(argv[optind],"r");

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
      program_stream();
    }
    resync();
  }

}

resync() {
  fprintf(stderr, "Resyncing\n");
  buf_start = 0;
  buf_fill = 0;

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

unsigned int nextbits(unsigned int nr_of_bits)
{
  /* Only called on byte boundaries */
  unsigned int bits;
  int n = 0;
  unsigned int nr_of_bytes = (nr_of_bits+7)/8;
  
  for(;n < nr_of_bytes; n++) {
    ((unsigned char *)&bits)[n] = inspect_byte(n);
  }
  
  return bits;
}
  

program_stream()
{

  fprintf(stderr, "program_stream()\n");
  do {
    pack();
  } while(nextbits(32) == MPEG2_PS_PACK_START_CODE); 
  if(nextbits(32) == MPEG2_PS_PROGRAM_END_CODE) {
    fprintf(stderr, "MPEG Program End\n");
    buf_free(4);
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
  unsigned long int system_clock_reference_base;
  unsigned int system_clock_reference_extension;
  unsigned int program_mux_rate;
  unsigned int pack_stuffing_length;
  
  unsigned char *data;
  int alloced;

  //fprintf(stderr, "pack_header()\n");

  buf_free(4); // pack_start_code
  
  alloced = get_bytes(10, &data);
  
  if(data[0]&0xC0 != 0x40) {
    fprintf(stderr, "*ph 01\n");
  }

  // 01ccc1cc cccccccc ccccc1cc cccccccc ccccc1ee eeeeeee1 mmmmmmmm mmmmmmmm mmmmmm11 rrrrrsss
  
  system_clock_reference_base =
    (data[0]&0x38)<<27 | (data[0]&0x03)<<28 | (data[1])<<20 |
    (data[2]&0xf8)<<12 | (data[2]&0x03)<<13 | (data[3])<<5 |
    ((data[3]>>2)&0x3f);
  
  
  system_clock_reference_extension =
    (data[4]&0x03)<<7 | ((data[5]>>1)&0x7f);
  
  program_mux_rate =
    (data[6])<<14 | (data[7])<<6 | ((data[8]>>2)&0x3f);
  
  pack_stuffing_length = data[9]&0x07;
  
  if(alloced == 1) {
    buf_free(10);
  }
  if(pack_stuffing_length) {
    if(get_bytes(pack_stuffing_length, &data) == 1) {
      buf_free(pack_stuffing_length);
    }
  }
  /*
    fprintf(stderr, "system_clock_reference_base: %lu\n",
    system_clock_reference_base);
    fprintf(stderr, "system_clock_reference_extension: %u\n",
    system_clock_reference_extension);
    fprintf(stderr, "program_mux_rate: %u\n",
    program_mux_rate);
    fprintf(stderr, "pack_stuffing_length: %u\n",
    pack_stuffing_length);
  
    fprintf(stderr, "next_header: %08x\n", nextbits(32));
  */
  if(nextbits(32) == MPEG2_PS_SYSTEM_HEADER_START_CODE) {
    //fprintf(stderr, "Found System_Header\n");
    system_header();
  }
}




system_header()
{
  unsigned short header_length;
  unsigned int rate_bound;
  unsigned char audio_bound;
  unsigned char fixed_flag;
  unsigned char CSPS_flag;
  unsigned char system_audio_lock_flag;
  unsigned char system_video_lock_flag;
  unsigned char video_bound;


  int alloced;
  unsigned char *data;
  
  //fprintf(stderr, "system_header()\n");
  
  
  buf_free(4); // system_header_start_code
  
  alloced = get_bytes(8, &data);
  
  header_length = data[0]<<8 | data[1];
  
  rate_bound = (data[2]&0x7f)<<15 | (data[3])<<7 | (data[4]>>1)&0x7f;

  audio_bound = (data[5]>>2)&0x3f;
  
  fixed_flag = (data[5]>>1)&0x01;
  
  CSPS_flag = (data[5])&0x01;
  
  system_audio_lock_flag = (data[6]>>7)&0x01;

  system_video_lock_flag = (data[6]>>6)&0x01;
  
  video_bound = data[6]&0x1f;
  
  if(alloced == 1) {
    buf_free(8);
  }
#if 0
  fprintf(stderr, "header_length: %hu\n", header_length);
  fprintf(stderr, "rate_bound: %u\n", rate_bound);
  fprintf(stderr, "audio_bound: %u\n", audio_bound);
  fprintf(stderr, "fixed_flag: %u\n", fixed_flag);
  fprintf(stderr, "CSPS_flag: %u\n", CSPS_flag);
  fprintf(stderr, "system_audio_lock_flag: %u\n", system_audio_lock_flag);
  fprintf(stderr, "system_video_lock_flag: %u\n", system_video_lock_flag);
  fprintf(stderr, "video_bound: %u\n", video_bound);
#endif
  
  //hhhhhhhh hhhhhhhh 1rrrrrrr rrrrrrrr rrrrrrr1 aaaaaafc av1vvvvv rrrrrrrr
  
  alloced = get_bytes(header_length-6, &data);
  {
    int n = 0;
    while(data[0+n*3]&0x80) {
#if 0
      fprintf(stderr, "stream_id: %x\n", data[0+n*3]);
      fprintf(stderr, "P-STD_buffer_bound_scale: %u\n",
	      (data[1]>>5)&0x01);
      fprintf(stderr, "P-STD_buffer_size_scale: %u\n",
	      (data[1]&0x1f)<<8 | (data[2]));
#endif
      n++;
    }
    
    buf_free(header_length-6);
    
    if(header_length-6 != n*3) {
      fprintf(stderr, "* header_length error\n");
    }
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
    

  //fprintf(stderr, "PES_packet()\n");

  buf_free(3); //packet_start_code_prefix
  
  tmp = nextbits(24);
  stream_id = (tmp>>24)&0xff;
  //fprintf(stderr, "stream_id: %02X\n", stream_id);
  
  PES_packet_length = (tmp>>8)&0xffff;
  //fprintf(stderr, "PES_packet_length: %u\n", PES_packet_length);
  
  buf_free(3);
  
  alloced = get_bytes(PES_packet_length, &data);
  
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
  
