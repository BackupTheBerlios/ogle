/* Ogle - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

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

#include <sys/ipc.h>
#include <sys/shm.h>


#include <ogle/msgevents.h>
#include "video_stream.h"
#include "video_types.h"
#include "video_tables.h"
#include "c_getbits.h"

#include "common.h"
#include "timemath.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif


extern void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);
//extern int chk_for_msg();
extern MsgEventQ_t *msgq;

extern int flush_to_scrnr;

int get_output_buffer(int padded_width, int padded_height, int nr_of_bufs);


int output_client;
int input_stream;

int ctrl_data_shmid;
ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

uint8_t PTS_DTS_flags;
uint64_t PTS;
uint64_t DTS;
int scr_nr;

uint64_t last_pts;
int last_scr_nr;
int prev_scr_nr;
int picture_has_pts;
uint64_t last_pts_to_dpy;
int last_scr_nr_to_dpy;
static int prev_coded_temp_ref = -2;

int dctstat[128];
int total_pos = 0;
int total_calls = 0;


unsigned int debug = 0;
int shm_ready = 0;
static int shmem_flag = 1;

long int frame_interval;
int forced_frame_rate = -1;
int nr_of_buffers = 4;
//buf_ctrl_head_t *buf_ctrl_head;
int picture_buffers_shmid;
int buf_ctrl_shmid;
char *buf_ctrl_shmaddr;
char *picture_buffers_shmaddr;

static char *picture_buf_base;
static data_buf_head_t *picture_ctrl_head;
static picture_data_elem_t *picture_ctrl_data;

static q_head_t *picture_q_head;
static q_elem_t *picture_q_elems;

//void init_out_q(int nr_of_bufs);
//void setup_shm(int horiz_size, int vert_size, int nr_of_bufs);


int last_temporal_ref_to_dpy = -1;

int MPEG2 = 0;

uint8_t intra_inverse_quantiser_matrix_changed = 1;
uint8_t non_intra_inverse_quantiser_matrix_changed = 1;


uint32_t stats_block_intra_nr = 0;
uint32_t stats_f_intra_compute_subseq_nr = 0;
uint32_t stats_f_intra_compute_first_nr = 0;

sequence_t seq;
picture_t pic;
slice_t slice_data;
macroblock_t mb;


yuv_image_t *image_bufs;

yuv_image_t *fwd_ref_image;
yuv_image_t *bwd_ref_image;
yuv_image_t *dst_image;



/* TODO , hack to recognize 24 fps progressive */
int last_gop_had_repeat_field_progressive_frame = 0;





//prototypes
int bytealigned(void);
void next_start_code(void);
void resync(void);
void drop_to_next_picture(void);
void video_sequence(void);
void marker_bit(void);
void sequence_header(void);
void sequence_extension(void);
void sequence_display_extension();
void extension_and_user_data(unsigned int i);
void picture_coding_extension(void);
void user_data(void);
void group_of_pictures_header(void);
void picture_header(void);
void picture_data(void);
int  mpeg1_slice(void);
void mpeg2_slice(void);

void reset_to_default_intra_quantiser_matrix();
void reset_to_default_non_intra_quantiser_matrix();
void reset_to_default_quantiser_matrix();


void user_control(macroblock_t *cur_mbs,
		  macroblock_t *ref1_mbs,
		  macroblock_t *ref2_mbs);

void motion_comp();
void motion_comp_add_coeff(unsigned int i);

void extension_data(unsigned int i);

void exit_program(int exitcode) __attribute__ ((noreturn));

int  writer_alloc(void);
void writer_free(int);
int  reader_alloc(void);
void reader_free(int);


// Not implemented
void quant_matrix_extension();
void picture_display_extension();
void picture_spatial_scalable_extension();
void picture_temporal_scalable_extension();
void sequence_scalable_extension();



void fprintbits(FILE *fp, unsigned int bits, uint32_t value)
{
  int n;
  for(n = 0; n < bits; n++) {
    fprintf(fp, "%u", (value>>(bits-n-1)) & 0x1);
  }
}


int get_vlc(const vlc_table_t *table, char *func) {
  int pos=0;
  int numberofbits=1;
  int vlc;
#if 1
  while(table[pos].numberofbits != VLC_FAIL) {
    vlc = nextbits(numberofbits);
    while(table[pos].numberofbits == numberofbits) {
      if(table[pos].vlc == vlc) {
	DPRINTF(3, "get_vlc(%s): len: %d, vlc: %d, val: %d\n",
		func, numberofbits, table[pos].vlc, table[pos].value);
	dropbits(numberofbits);
	return (table[pos].value);
      }
      pos++;
    }
    numberofbits++;
  }
#else
  int found = 0;
  numberofbits=0;
  while(1) {
    if(table[pos].numberofbits != numberofbits) {
      numberofbits = table[pos].numberofbits;
      if(numberofbits == VLC_FAIL)
	break;
      vlc = nextbits(numberofbits);
    }
    if(table[pos].vlc == vlc) {
      found = 1;
      break;
    }
    pos++;
  }
  if(found) {
    dropbits(numberofbits);
    return (table[pos].value);
  }
#endif
  fprintf(stderr, "*** get_vlc(vlc_table *table, \"%s\"): no matching " 
	  "bitstream found.\nnext 32 bits: %08x, ", func, nextbits(32));
  fprintbits(stderr, 32, nextbits(32));
  fprintf(stderr, "\n");
  //exit_program(-1);
  return VLC_FAIL;
}


void sighandler(int dummy)
{
  fprintf(stderr, "video_stream: sighandler\n");
  exit_program(0);
}


void init_program()
{
  struct sigaction sig;
  
  // Setup signal handler.
  // SIGINT == ctrl-c
  sig.sa_handler = sighandler;
  sigaction(SIGINT, &sig, NULL);
}

int msgqid = -1;

//char *infilename = NULL;


int main(int argc, char **argv)
{
  int c;
  int output_client = 0;
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:r:f:m:hs")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'r':
      nr_of_buffers = atoi(optarg);
      break;
    case 'f':
      forced_frame_rate = atoi(optarg);
      break;
    case 's':
      shmem_flag = 0;
      break;
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'h':
    case '?':
      printf ("Usage: %s [-d <level] [-r <buffers>] [-s]\n\n"
	      "  -d <level>   set debug level (default 0)\n"
	      "  -r <buffers> set nr of buffers (default 3)\n"
	      "  -s           disable shared memory\n", 
	      argv[0]);
      return 1;
    }
  }
  
  init_program();
  
  //init_out_q(nr_of_buffers);
  
  if(msgqid == -1) {
    if(optind < argc) {
      infilename = argv[optind];
      infile = fopen(argv[optind], "r");
    } else {
      infile = stdin;
    }
  } 
  
  if(msgqid != -1) {
    MsgEvent_t ev;
    if((msgq = MsgOpen(msgqid)) == NULL) {
      fprintf(stderr, "video_decoder: couldn't get message q\n");
      exit(-1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "video_decode: register capabilities\n");
    }

    /*
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = VIDEO_OUTPUT;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "video_decode: didn't get video_output\n");
    }
    */
    fprintf(stderr, "## video_stream:req cap\n");
    while(!input_stream) {
      MsgNextEvent(msgq, &ev);
      switch(ev.type) {
      case MsgEventQGntCapability:
	if(ev.gntcapability.capability & VIDEO_OUTPUT) {
	  output_client = ev.gntcapability.capclient;
	} else {
	  fprintf(stderr, "ui: capabilities not requested\n");
	}
	break;
      default:
	handle_events(msgq, &ev);
	break;
      }
    }

    fprintf(stderr, "## video_stream:got cap and stream\n");

    
  }

  
#ifdef GETBITSMMAP

  // get_next_packet(); // Really, open and mmap the FILE

  packet_offset = 0;
  packet_length = 0;  

  buf = (uint32_t *)mmap_base;
  buf_size = 0;
  offs = 0;
  bits_left = 0;
  read_buf(); // Read first data packet and fill 'curent_word'.

  while( 1 ) {
    DPRINTF(1, "Looking for new Video Sequence\n");
    video_sequence();
  }  
#else
#ifdef GETBITS32
  next_word();
#else
  fread(&buf[0], READ_SIZE, 1, infile);
  {
    uint32_t new_word1 = GUINT32_FROM_BE(buf[offs++]);
    uint32_t new_word2;
    if(offs >= READ_SIZE/4)
      read_buf();
    new_word2 = GUINT32_FROM_BE(buf[offs++]);
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = ((uint64_t)(new_word1) << 32) | new_word2;
  }
#endif
  while(!feof(infile)) {
    DPRINTF(1, "Looking for new Video Sequence\n");
    video_sequence();
  }
#endif
  return 0;
}



/* 5.2.3 Definition of next_start_code() function */
void next_start_code(void)
{
  while(!bytealigned()) {
    GETBITS(1, "next_start_code");
  }
  while(nextbits(24) != 0x000001) {
    GETBITS(8, "next_start_code");
  }
}

void resync(void) {
  DPRINTF(0, "Resyncing\n");
  GETBITS(8, "resync");
}

void drop_to_next_picture(void) {
  do {
    GETBITS(8, "drop");
    next_start_code();
    fprintf(stderr, "%08x\n", nextbits(32));
  } while(nextbits(32) != MPEG2_VS_PICTURE_START_CODE &&
	  nextbits(32) != MPEG2_VS_GROUP_START_CODE);
  fprintf(stderr, "Blaha!\n");
}


/* 6.2.2 Video Sequence */
void video_sequence(void) {

  next_start_code();

  if(nextbits(32) != MPEG2_VS_SEQUENCE_HEADER_CODE) {
    resync();
    return;
  }
  
  DPRINTF(1, "Found Sequence Header\n");

  sequence_header();
  DINDENT(1);
  if(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
    /* MPEG-2 always has an extension code right after the sequence header. */
    MPEG2 = 1;
    
    sequence_extension();

    /* Display init */
    if(!shm_ready) {
      //setup_shm(seq.mb_width * 16, seq.mb_height * 16, nr_of_buffers);
      get_output_buffer(seq.mb_width * 16, seq.mb_height * 16, nr_of_buffers);
      shm_ready = 1;
    }
    DINDENT(1);
    do {
      extension_and_user_data(0);
      DINDENT(1);
      do {
	if(nextbits(32) == MPEG2_VS_GROUP_START_CODE) {
	  group_of_pictures_header();
	  extension_and_user_data(1);
	}
	picture_header();
	DINDENT(1);
	picture_coding_extension();
	
	extension_and_user_data(2);
	
	picture_data();
	DINDENT(-1);
      } while((nextbits(32) == MPEG2_VS_PICTURE_START_CODE) ||
	      (nextbits(32) == MPEG2_VS_GROUP_START_CODE));
      DINDENT(-1);
      if(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE) {
	if(nextbits(32) != MPEG2_VS_SEQUENCE_HEADER_CODE) {
	  DPRINTF(0, "*** not a sequence header or end code\n");
	  break;
	}
	
	sequence_header();
	sequence_extension();
      }
    } while(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE);
    DINDENT(-1);
  } else {
    /* ERROR: This is an ISO/IEC 11172-2 Stream */
    
    /* No extension code following the sequence header implies MPEG-1 */
    MPEG2 = 0;
    
    // init values for MPEG-1
    pic.coding_ext.picture_structure = PIC_STRUCT_FRAME_PICTURE;
    pic.coding_ext.frame_pred_frame_dct = 1;
    pic.coding_ext.intra_vlc_format = 0;
    pic.coding_ext.concealment_motion_vectors = 0;
    //mb.modes.frame_motion_type = 0x2; // This implies the ones below..
    mb.prediction_type = PRED_TYPE_FRAME_BASED;
    mb.motion_vector_count = 1;
    mb.mv_format = MV_FORMAT_FRAME;
    mb.dmv = 0;
    seq.ext.chroma_format = 0x1;

    /* Display init */
    if(!shm_ready) {
      //setup_shm(seq.mb_width * 16, seq.mb_height * 16, nr_of_buffers);
      get_output_buffer(seq.mb_width * 16, seq.mb_height * 16, nr_of_buffers);
      shm_ready = 1;
    }

    next_start_code();
    extension_and_user_data(1);
    do {
      if(nextbits(32) == MPEG2_VS_GROUP_START_CODE) {
	group_of_pictures_header();
	extension_and_user_data(1);
      }
      picture_header();
      extension_and_user_data(1);
      picture_data();
    } while(nextbits(32) == MPEG2_VS_PICTURE_START_CODE ||
	    (nextbits(32) == MPEG2_VS_GROUP_START_CODE));
  }
  DINDENT(-1);  
  
  /* If we are exiting there should be a sequence end code following. */
  if(nextbits(32) == MPEG2_VS_SEQUENCE_END_CODE) {
    GETBITS(32, "Sequence End Code");
    DPRINTF(1, "Found Sequence End\n");
  } else {
    DPRINTF(0, "*** Didn't find Sequence End\n");
  }

}
  

/* 6.2.2.1 Sequence header */
void sequence_header(void)
{
  uint32_t sequence_header_code;
  long int frame_interval_pts = 0;

  DPRINTFI(1, "sequence_header()\n");
  DINDENT(2);

  sequence_header_code = GETBITS(32, "sequence header code");
  if(sequence_header_code != MPEG2_VS_SEQUENCE_HEADER_CODE) {
    fprintf(stderr, "wrong start_code sequence_header_code: %08x\n",
	    sequence_header_code);
  }
  
  seq.header.horizontal_size_value = GETBITS(12, "horizontal_size_value");
  seq.header.vertical_size_value = GETBITS(12, "vertical_size_value");
  seq.header.aspect_ratio_information = GETBITS(4, "aspect_ratio_information");
  seq.header.frame_rate_code = GETBITS(4, "frame_rate_code");
  seq.header.bit_rate_value = GETBITS(18, "bit_rate_value");  
  marker_bit();
  seq.header.vbv_buffer_size_value = GETBITS(10, "vbv_buffer_size_value");
  seq.header.constrained_parameters_flag 
    = GETBITS(1, "constrained_parameters_flag");
  
  
  /* When a sequence header is decoded all matrices shall either 
     be reset to their default values or downloaded from the bit stream. */
  if(GETBITS(1, "load_intra_quantiser_matrix")) {
    int n;
    intra_inverse_quantiser_matrix_changed = 1;
    
    for(n = 0; n < 64; n++) {
      seq.header.intra_quantiser_matrix[n] 
	= GETBITS(8, "intra_quantiser_matrix[n]");
    }
    
    /* 7.3.1 Inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.intra_inverse_quantiser_matrix[v*8+u] =
	    seq.header.intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
    
  } else {
    if(intra_inverse_quantiser_matrix_changed) {
      reset_to_default_intra_quantiser_matrix();
      intra_inverse_quantiser_matrix_changed = 0;
    }
  }
  
  if(GETBITS(1, "load_non_intra_quantiser_matrix")) {
    int n;
    non_intra_inverse_quantiser_matrix_changed = 1;
    
    for(n = 0; n < 64; n++) {
      seq.header.non_intra_quantiser_matrix[n] 
	= GETBITS(8, "non_intra_quantiser_matrix[n]");
    }
   
    /** 7.3.1 Inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.non_intra_inverse_quantiser_matrix[v*8+u] =
	    seq.header.non_intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
    
  } else {
    if(non_intra_inverse_quantiser_matrix_changed) {
      reset_to_default_non_intra_quantiser_matrix();
      non_intra_inverse_quantiser_matrix_changed = 0;
    }
  }

  
  
  DPRINTFI(2, "horizontal_size_value: %u\n", seq.header.horizontal_size_value);
  DPRINTFI(2, "vertical_size_value: %u\n", seq.header.vertical_size_value);
  DPRINTFI(2, "aspect_ratio_information:(0x%01x) ",  seq.header.aspect_ratio_information);
#ifdef DEBUG
  switch(seq.header.aspect_ratio_information) {
  case 0x0:
    DPRINTF(2, "forbidden\n");
    break;
  case 0x1:
    DPRINTF(2, "SAR = 1.0\n");
    break;
  case 0x2:
    DPRINTF(2, "DAR = 3/4\n");
    break;
  case 0x3:
    DPRINTF(2, "DAR = 9/16\n");
    break;
  case 0x4:
    DPRINTF(2, "DAR = 1/2.21\n");
    break;
  default:
    DPRINTF(2, "reserved\n");
    break;
  }
#endif
  DPRINTFI(2, "frame_rate_code:(0x%01x) ", seq.header.frame_rate_code);
  switch(seq.header.frame_rate_code) {
  case 0x0:
    DPRINTF(2, "forbidden\n");
    break;
  case 0x1:
    DPRINTF(2, "24000/1001 (23.976)\n");
    frame_interval_pts = 3754;
    break;
  case 0x2:
    DPRINTF(2, "24\n");
    frame_interval_pts = 3750;
    break;
  case 0x3:
    DPRINTF(2, "25\n");
    frame_interval_pts = 3600;
    break;
  case 0x4:
    DPRINTF(2, "30000/1001 (29.97)\n");
    /* TODO hack for 24fps progressive */
    if(last_gop_had_repeat_field_progressive_frame) {
      /* it's probably coded as 24 fps progressive */
      frame_interval_pts = 3754;
    } else {
      frame_interval_pts = 3003;
    }
    break;
  case 0x5:
    DPRINTF(2, "30\n");
    frame_interval_pts = 3000;
    break;
  case 0x6:
    DPRINTF(2, "50\n");
    frame_interval_pts = 1800;
    break;
  case 0x7:
    DPRINTF(2, "60000/1001 (59.94)\n");
    frame_interval_pts = 1502;
    break;
  case 0x8:
    DPRINTF(2, "60\n");
    frame_interval_pts = 1500;
    break;
  default:
    DPRINTF(2, "Reserved\n");
    fprintf(stderr, "reserved framerate\n");
    break;
  }
  
  
  if(forced_frame_rate == -1) { /* No forced frame rate */
    //buf_ctrl_head->frame_interval = frame_interval_pts;
    frame_interval = frame_interval_pts;
  } else {
    if(forced_frame_rate == 0) {
      //buf_ctrl_head->frame_interval = 1;
      frame_interval = 1;
    } else {
      //buf_ctrl_head->frame_interval = PTS_BASE/forced_frame_rate;
      frame_interval = PTS_BASE/forced_frame_rate;
    }
  }

  DPRINTFI(2, "bit_rate_value: (0x%03x) %d bits/second\n",
	  seq.header.bit_rate_value,
	  seq.header.bit_rate_value*400);
  DPRINTFI(2, "vbv_buffer_size_value: (0x%02x) min %d bits\n",
	  seq.header.vbv_buffer_size_value,
	  16*1024*seq.header.vbv_buffer_size_value);
  DPRINTFI(2, "constrained_parameters_flag: %01x\n",
	  seq.header.constrained_parameters_flag);
   
  seq.horizontal_size = seq.header.horizontal_size_value;
  seq.vertical_size = seq.header.vertical_size_value;
  
  seq.mb_width  = (seq.horizontal_size+15)/16;
  seq.mb_height = (seq.vertical_size+15)/16;

  DINDENT(-2);
}



#define INC_8b_ALIGNMENT(a) ((a+7)/8*8)



int get_output_buffer(int padded_width, int padded_height, int nr_of_bufs)
{
  int i;
  char *baseaddr;
  int picture_size;
  int picture_bufs_size;
  int picture_ctrl_size;
  int buf_size;
  MsgEvent_t ev;
  int num_pels = padded_width * padded_height;
  int pagesize = sysconf(_SC_PAGESIZE);
  int y_size   = INC_8b_ALIGNMENT(num_pels);
  int uv_size  = INC_8b_ALIGNMENT(num_pels/4);
  int yuv_size = y_size + 2 * uv_size; 
  int bufshmid;
  int qshmid;
  char *shmaddr;
  char *qshmaddr;
  picture_data_elem_t *data_elems;
  char *picture_data_base;
  int picture_data_offset;
  int n;

  fprintf(stderr,"vs: get ouput buffer\n");
  picture_size = ((yuv_size + (pagesize-1))/pagesize*pagesize);
  
  /* Mlib reads ?8? bytes beyond the last pel (in the v-picture), 
     if that pel just before a page boundary then *boom*!! */
  


  // TODO this is an ugly hack for not mixing in ref.frames
#ifdef HAVE_XV
  picture_bufs_size = (nr_of_bufs+1) * picture_size + pagesize;//Hack
  picture_ctrl_size = sizeof(data_buf_head_t) +
    sizeof(picture_data_elem_t)*(nr_of_bufs+1);
#else
  picture_bufs_size = nr_of_bufs * picture_size + pagesize;//Hack
  picture_ctrl_size = sizeof(data_buf_head_t) +
    sizeof(picture_data_elem_t)*nr_of_bufs;
#endif


  picture_ctrl_size = INC_8b_ALIGNMENT(picture_ctrl_size);

  buf_size = picture_ctrl_size + picture_bufs_size;
  
  // Get shared memory buffer
  
  
  ev.type = MsgEventQReqBuf;
  ev.reqbuf.size = buf_size;
  
  if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
    fprintf(stderr, "**video_decoder: couldn't send buffer request\n");
  }
  
  while(ev.type != MsgEventQGntBuf) {
    MsgNextEvent(msgq, &ev);
    if(ev.type == MsgEventQGntBuf) {
      DPRINTF(1, "video_decoder: got buffer id %d, size %d\n",
	      ev.gntbuf.shmid,
	      ev.gntbuf.size);
      bufshmid = ev.gntbuf.shmid;
      break;
    } else {
      handle_events(msgq, &ev);
    }
  }
  

  if(ev.gntbuf.shmid >= 0) {
    if((shmaddr = shmat(bufshmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("**video_decode: attach_buffer(), shmat()");
      return -1;
    }
    
    picture_buf_base = shmaddr;
    picture_ctrl_head = (data_buf_head_t *)picture_buf_base;
    picture_ctrl_head->shmid = bufshmid;
    picture_ctrl_head->nr_of_dataelems = nr_of_bufs;
    picture_ctrl_head->write_nr = 0;
    
    

    image_bufs = malloc(nr_of_bufs*sizeof(yuv_image_t));
    
    data_elems =(picture_data_elem_t *)(picture_buf_base + 
					sizeof(data_buf_head_t));
    picture_ctrl_data = data_elems;

    picture_data_offset = picture_ctrl_size;

  // TODO this is an ugly hack for not mixing in ref.frames
#ifdef HAVE_XV
    for(n = 0; n < (picture_ctrl_head->nr_of_dataelems+1); n++) {
#else
    for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
#endif
      data_elems[n].displayed = 1;
      data_elems[n].is_reference = 0;
      data_elems[n].picture.y_offset = picture_data_offset;
      image_bufs[n].y = picture_buf_base + picture_data_offset;
      data_elems[n].picture.v_offset = picture_data_offset + y_size;
      image_bufs[n].v = picture_buf_base + picture_data_offset + y_size;
      data_elems[n].picture.u_offset = picture_data_offset + y_size + uv_size;
      image_bufs[n].u = picture_buf_base + picture_data_offset+y_size+uv_size;

      data_elems[n].picture.horizontal_size = seq.horizontal_size;
      data_elems[n].picture.vertical_size = seq.vertical_size;
      data_elems[n].picture.start_x = 0;
      data_elems[n].picture.start_y = 0;
      data_elems[n].picture.padded_width = padded_width;
      data_elems[n].picture.padded_height = padded_height;
      
      picture_data_offset += picture_size;
      
    }


    fwd_ref_image = &image_bufs[0];
    bwd_ref_image = &image_bufs[1];
    

    /* send create decoder request msg*/
    ev.type = MsgEventQReqPicBuf;
    
    ev.reqpicbuf.nr_of_elems = nr_of_bufs;
    ev.reqpicbuf.data_buf_shmid = bufshmid;
      
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "**video_decode: couldn't send picbuf request\n");
    }
    
    /* wait for answer */
    
    while(ev.type != MsgEventQGntPicBuf) {
      MsgNextEvent(msgq, &ev);
      if(ev.type == MsgEventQGntPicBuf) {
	qshmid = ev.gntpicbuf.q_shmid;
      } else {
	handle_events(msgq, &ev);
      }
    }
    
    if(qshmid >= 0) {
      if((qshmaddr = shmat(qshmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
	perror("**video_decode: attach_picq_buffer(), shmat()");
	return -1;
      }
      

      picture_q_head = (q_head_t *)qshmaddr;
      picture_q_elems = (q_elem_t *)(qshmaddr+sizeof(q_head_t));
      
    } else {
      fprintf(stderr, "** video_decode: couldn't get qbuffer\n");
    }
    
  } else {
    fprintf(stderr, "** video_decode: couldn't get buffer\n");
  }
  
#ifdef HAVE_MMX
    emms();
#endif
  
  /* this should not be here */
  fprintf(stderr, "horizontal_size: %d, vertical_size: %d\n",
	  seq.horizontal_size, seq.vertical_size);
  fprintf(stderr, "padded_width: %d, padded_height: %d\n",
	  padded_width, padded_height);
  
  fprintf(stderr, "frame rate: ");
  switch(seq.header.frame_rate_code) {
  case 0x0:
    break;
  case 0x1:
    fprintf(stderr, "24000/1001 (23.976)\n");
    break;
  case 0x2:
    fprintf(stderr, "24\n");
    break;
  case 0x3:
    fprintf(stderr, "25\n");
    break;
  case 0x4:
    fprintf(stderr, "30000/1001 (29.97)\n");
    break;
  case 0x5:
    fprintf(stderr, "30\n");
    break;
  case 0x6:
    fprintf(stderr, "50\n");
    break;
  case 0x7:
    fprintf(stderr, "60000/1001 (59.94)\n");
    break;
  case 0x8:
    fprintf(stderr, "60\n");
    break;
  default:
    fprintf(stderr, "%f (computed)\n",
	    (double)(seq.header.frame_rate_code)*
	    ((double)(seq.ext.frame_rate_extension_n)+1.0)/
	    ((double)(seq.ext.frame_rate_extension_d)+1.0));
    break;
  }

}


/* 6.2.2.3 Sequence extension */
void sequence_extension(void) {
  long int frame_interval_pts = 0;
  uint32_t extension_start_code;
  
  extension_start_code = GETBITS(32, "extension_start_code");
  if(extension_start_code != MPEG2_VS_EXTENSION_START_CODE) {
    fprintf(stderr, "wrong start_code extension_start_code: %08x\n",
	    extension_start_code);
  }

  DPRINTFI(1, "sequence_extension()\n");
  DINDENT(2);
  
  seq.ext.extension_start_code_identifier 
    = GETBITS(4, "extension_start_code_identifier");
  seq.ext.profile_and_level_indication 
    = GETBITS(8, "profile_and_level_indication");
  seq.ext.progressive_sequence = GETBITS(1, "progressive_sequence");
  seq.ext.chroma_format = GETBITS(2, "chroma_format");
  seq.ext.horizontal_size_extension = GETBITS(2, "horizontal_size_extension");
  seq.ext.vertical_size_extension = GETBITS(2, "vertical_size_extension");
  seq.ext.bit_rate_extension = GETBITS(12, "bit_rate_extension");
  marker_bit();
  seq.ext.vbv_buffer_size_extension = GETBITS(8, "vbv_buffer_size_extension");
  seq.ext.low_delay = GETBITS(1, "low_delay");
  seq.ext.frame_rate_extension_n = GETBITS(2, "frame_rate_extension_n");
  seq.ext.frame_rate_extension_d = GETBITS(5, "frame_rate_extension_d");
  next_start_code();

  seq.horizontal_size |= (seq.ext.horizontal_size_extension << 12);
  seq.vertical_size |= (seq.ext.vertical_size_extension << 12);

#ifdef DEBUG
	   
  DPRINTFI(2, "profile_and_level_indication: ");
  if(seq.ext.profile_and_level_indication & 0x80) {
    DPRINTF(2, "(reserved)\n");
  } else {
    DPRINTF(2, "Profile: ");
    switch((seq.ext.profile_and_level_indication & 0x70)>>4) {
    case 0x5:
      DPRINTF(2, "Simple");
      break;
    case 0x4:
      DPRINTF(2, "Main");
      break;
    case 0x3:
      DPRINTF(2, "SNR Scalable");
      break;
    case 0x2:
      DPRINTF(2, "Spatially Scalable");
      break;
    case 0x1:
      DPRINTF(2, "High");
      break;
    default:
      DPRINTF(2, "(reserved)");
      break;
    }

    DPRINTF(2, ", Level: ");
    switch(seq.ext.profile_and_level_indication & 0x0f) {
    case 0xA:
      DPRINTF(2, "Low");
      break;
    case 0x8:
      DPRINTF(2, "Main");
      break;
    case 0x6:
      DPRINTF(2, "High 1440");
      break;
    case 0x4:
      DPRINTF(2, "High");
      break;
    default:
      DPRINTF(2, "(reserved)");
      break;
    }
    DPRINTF(2, "\n");
  }
 
  DPRINTFI(2, "progressive seq: %01x\n", seq.ext.progressive_sequence);

  DPRINTFI(2, "chroma_format:(0x%01x) ", seq.ext.chroma_format);
  switch(seq.ext.chroma_format) {
  case 0x0:
    DPRINTF(2, "reserved\n");
    break;
  case 0x1:
    DPRINTF(2, "4:2:0\n");
    break;
  case 0x2:
    DPRINTF(2, "4:2:2\n");
    break;
  case 0x3:
    DPRINTF(2, "4:4:4\n");
    break;
  }
#endif

  DPRINTFI(2, "horizontal_size: %u\n", seq.horizontal_size);
  DPRINTFI(2, "vertical_size: %u\n", seq.vertical_size);

  DPRINTFI(2, "bit_rate: %d bits/second\n",
	  400 * ((seq.ext.bit_rate_extension << 12) 
		 | seq.header.bit_rate_value));

  DPRINTFI(2, "vbv_buffer_size: min %d bits\n",
	   16*1024*((seq.ext.vbv_buffer_size_extension << 10) |
		    seq.header.vbv_buffer_size_value));
  DPRINTFI(2, "low_delay: %01x\n", seq.ext.low_delay);
  
  DPRINTFI(2, "frame_rate: ");
  switch(seq.header.frame_rate_code) {
  case 0x0:
    DPRINTF(2, "forbidden\n");
    break;
  case 0x1:
    DPRINTF(2, "24000/1001 (23.976)\n");
    frame_interval_pts = 3754;
    break;
  case 0x2:
    DPRINTF(2, "24\n");
    frame_interval_pts = 3750;
    break;
  case 0x3:
    DPRINTF(2, "25\n");
    frame_interval_pts = 3600;
    break;
  case 0x4:
    DPRINTF(2, "30000/1001 (29.97)\n");
    if(last_gop_had_repeat_field_progressive_frame) {
      /* it's probably coded as 24 fps progressive */
      frame_interval_pts = 3754;
    } else {
      frame_interval_pts = 3003;
    }
    break;
  case 0x5:
    DPRINTF(2, "30\n");
    frame_interval_pts = 3000;
    break;
  case 0x6:
    DPRINTF(2, "50\n");
    frame_interval_pts = 1800;
    break;
  case 0x7:
    DPRINTF(2, "60000/1001 (59.94)\n");
    frame_interval_pts = 1502;
    break;
  case 0x8:
    DPRINTF(2, "60\n");
    frame_interval_pts = 1500;
    break;
  default:
    DPRINTF(2, "%f (computed)\n",
	    (double)(seq.header.frame_rate_code)*
	    ((double)(seq.ext.frame_rate_extension_n)+1.0)/
	    ((double)(seq.ext.frame_rate_extension_d)+1.0));
    //TODO: frame_interval_nsec = ?
    fprintf(stderr, "fixme computed framerate\n");
    break;
  }

  if(forced_frame_rate == -1) { /* No forced frame rate */
    //buf_ctrl_head->frame_interval = frame_interval_pts;
    frame_interval = frame_interval_pts;
  } else {
    if(forced_frame_rate == 0) {
      //buf_ctrl_head->frame_interval = 1;
      frame_interval = 1;
    } else {
      //buf_ctrl_head->frame_interval = PTS_BASE/forced_frame_rate;
      frame_interval = PTS_BASE/forced_frame_rate;
    }
  }
  DINDENT(-2);
}


/* 6.2.2.2 Extension and user data */
void extension_and_user_data(unsigned int i) {
  
  DPRINTFI(1, "extension_and_user_data(%u)\n", i);
  DINDENT(2);
  while((nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) ||
	(nextbits(32) == MPEG2_VS_USER_DATA_START_CODE)) {
    if(i != 1) {
      if(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
	extension_data(i);
      }
    }
    if(nextbits(32) == MPEG2_VS_USER_DATA_START_CODE) {
      user_data();
    }
  }
  DINDENT(-2);
}


/* 6.2.3.1 Picture coding extension */
void picture_coding_extension(void)
{
  uint32_t extension_start_code;
  int m,n;

  DPRINTFI(1, "picture_coding_extension()\n");
  DINDENT(2);
  extension_start_code = GETBITS(32, "extension_start_code");
  pic.coding_ext.extension_start_code_identifier 
    = GETBITS(4, "extension_start_code_identifier");

  pic.coding_ext.f_code[0][0] = GETBITS(4, "f_code[0][0]");
  pic.coding_ext.f_code[0][1] = GETBITS(4, "f_code[0][1]");
  pic.coding_ext.f_code[1][0] = GETBITS(4, "f_code[1][0]");
  pic.coding_ext.f_code[1][1] = GETBITS(4, "f_code[1][1]");
  for(n = 0; n < 2; n++) {
    for(m = 0; m < 2; m++) {
      if(pic.coding_ext.f_code[n][m] == 0) {
	pic.coding_ext.f_code[n][m] = 1;
	fprintf(stderr, "f_code[%d][%d] == ZERO\n", n, m);
      }
    }
  }
  pic.coding_ext.intra_dc_precision = GETBITS(2, "intra_dc_precision");

  /** opt4 **/
  switch(pic.coding_ext.intra_dc_precision) {
  case 0x0:
    mb.intra_dc_mult = 8;
    break;
  case 0x1:
    mb.intra_dc_mult = 4;
    break;
  case 0x2:
    mb.intra_dc_mult = 2;
    break;
  case 0x3:
    mb.intra_dc_mult = 1;
    break;
  }
  /**/
  pic.coding_ext.picture_structure = GETBITS(2, "picture_structure");

#ifdef DEBUG
  /* Table 6-14 Meaning of picture_structure */
  DPRINTFI(2, "picture_structure: ");
  switch(pic.coding_ext.picture_structure) {
  case PIC_STRUCT_RESERVED:
    DPRINTF(2, "reserved");
    break;
  case PIC_STRUCT_TOP_FIELD:
    DPRINTF(2, "Top Field");
    break;
  case PIC_STRUCT_BOTTOM_FIELD:
    DPRINTF(2, "Bottom Field");
    break;
  case PIC_STRUCT_FRAME_PICTURE:
    DPRINTF(2, "Frame Picture");
    break;
  }
  DPRINTF(2, "\n");
#endif

  pic.coding_ext.top_field_first = GETBITS(1, "top_field_first");
  pic.coding_ext.frame_pred_frame_dct = GETBITS(1, "frame_pred_frame_dct");
  pic.coding_ext.concealment_motion_vectors 
    = GETBITS(1, "concealment_motion_vectors");
  pic.coding_ext.q_scale_type = GETBITS(1, "q_scale_type");
  pic.coding_ext.intra_vlc_format = GETBITS(1, "intra_vlc_format");
  pic.coding_ext.alternate_scan = GETBITS(1, "alternate_scan");
  pic.coding_ext.repeat_first_field = GETBITS(1, "repeat_first_field");
  pic.coding_ext.chroma_420_type = GETBITS(1, "chroma_420_type");
  pic.coding_ext.progressive_frame = GETBITS(1, "progressive_frame");
  pic.coding_ext.composite_display_flag = GETBITS(1, "composite_display_flag");
  
  DPRINTFI(2, "top_field_first: %01x\n", pic.coding_ext.top_field_first);
  DPRINTFI(2, "frame_pred_frame_dct: %01x\n", pic.coding_ext.frame_pred_frame_dct);
  DPRINTFI(2, "intra_vlc_format: %01x\n", pic.coding_ext.intra_vlc_format);
  DPRINTFI(2, "alternate_scan: %01x\n", pic.coding_ext.alternate_scan);
  DPRINTFI(2, "repeat_first_field: %01x\n", pic.coding_ext.repeat_first_field);
  DPRINTFI(2, "progressive_frame: %01x\n", pic.coding_ext.progressive_frame);
  DPRINTFI(2, "composite_display_flag: %01x\n", pic.coding_ext.composite_display_flag);
  
  if(pic.coding_ext.composite_display_flag) {
    pic.coding_ext.v_axis = GETBITS(1, "v_axis");
    pic.coding_ext.field_sequence = GETBITS(3, "field_sequence");
    pic.coding_ext.sub_carrier = GETBITS(1, "sub_carrier");
    pic.coding_ext.burst_amplitude = GETBITS(7, "burst_amplitude");
    pic.coding_ext.sub_carrier_phase = GETBITS(8, "sub_carrier_phase");
  }
  
  if(pic.coding_ext.progressive_frame &&
     pic.coding_ext.repeat_first_field) {
    last_gop_had_repeat_field_progressive_frame++;
  }
  seq.mb_width = (seq.horizontal_size+15)/16;
  
  if(seq.ext.progressive_sequence) {
    seq.mb_height = (seq.vertical_size+15)/16;
  } else {
    if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
      /* frame pic */
      seq.mb_height = 2*((seq.vertical_size+31)/32);
    } else {
      /* field_picture */
      seq.mb_height = ((seq.vertical_size+31)/32);
    }
  }
  DINDENT(-2);
  next_start_code();
}


/* 6.2.2.2.2 User data */
void user_data(void)
{
  uint32_t user_data_start_code;
  
  DPRINTFI(1, "user_data()\n");
  DINDENT(2);
  user_data_start_code = GETBITS(32, "user_data_start_code");
  while(nextbits(24) != 0x000001) {
    GETBITS(8, "user_data");
  }
  DINDENT(-2);
  next_start_code();  
}


/* 6.2.2.6 Group of pictures header */
void group_of_pictures_header(void)
{
  uint32_t group_start_code;
  uint32_t time_code;
  uint8_t closed_gop;
  uint8_t broken_link;
  
  DPRINTFI(1, "group_of_pictures_header()\n");
  DINDENT(2);
  group_start_code = GETBITS(32, "group_start_code");

  if(group_start_code != MPEG2_VS_GROUP_START_CODE) {
    fprintf(stderr, "wrong start_code group_start_code: %08x\n",
	    group_start_code);
  }
  /* TODO hack for progessive 24 */
  last_gop_had_repeat_field_progressive_frame = 0;

  time_code = GETBITS(25, "time_code");

  /* Table 6-11 --- time_code */
  DPRINTFI(2, "time_code: %02d:%02d.%02d'%02d\n",
	  (time_code & 0x00f80000)>>19,
	  (time_code & 0x0007e000)>>13,
	  (time_code & 0x00000fc0)>>6,
	  (time_code & 0x0000003f));
  
  /* These need to be in seq or some thing like that */
  closed_gop = GETBITS(1, "closed_gop");
  broken_link = GETBITS(1, "broken_link");

  last_temporal_ref_to_dpy = -1;
  prev_coded_temp_ref = -2;
  
  DINDENT(-2);
  next_start_code();
}


/* 6.2.3 Picture header */
void picture_header(void)
{
  uint32_t picture_start_code;
  DPRINTFI(1, "picture_header()\n");
  DINDENT(2);
  
  seq.mb_row = 0;
  picture_start_code = GETBITS(32, "picture_start_code");
  
  if(picture_start_code != MPEG2_VS_PICTURE_START_CODE) {
    fprintf(stderr, "wrong start_code picture_start_code: %08x\n",
	    picture_start_code);
  }
  
  /*
   * TODO better check if pts really is for this picture
   *
   * the first picture_start_code in a packet belongs to the
   * picture that the pts in the packet corresponds to.
   */

  if(PTS_DTS_flags & 0x02) {
    PTS_DTS_flags = 0;
    last_pts = PTS;
    prev_scr_nr = last_scr_nr;
    last_scr_nr = scr_nr;
    picture_has_pts = 1;
    DPRINTFI(1, "PTS: %016llx %lld.%06lld\n",
	     PTS,
	     PTS/PTS_BASE,
	     (PTS%PTS_BASE)*1000000/PTS_BASE);
  } else {
    picture_has_pts = 0;
  }
  
  if(PTS_DTS_flags & 0x02) {
    if(last_scr_nr != prev_scr_nr) {   
      fprintf(stderr, "=== last_scr_nr: %d, prev_scr_nr: %d\n",
	      last_scr_nr, prev_scr_nr);
      
      fprintf(stderr, "--- last_scr: %ld.%09ld, prev_scr: %ld.%09ld\n",
	      TIME_S (ctrl_time[last_scr_nr].realtime_offset),
	      TIME_SS(ctrl_time[last_scr_nr].realtime_offset),
	      TIME_S (ctrl_time[prev_scr_nr].realtime_offset),
	      TIME_SS(ctrl_time[prev_scr_nr].realtime_offset));

      fprintf(stderr, "+++ last_pts: %lld\n", last_pts);
    }
  }


  pic.header.temporal_reference = GETBITS(10, "temporal_reference");
  pic.header.picture_coding_type = GETBITS(3, "picture_coding_type");

  DPRINTFI(1, "temporal_reference: %d\n", pic.header.temporal_reference);
#ifdef DEBUG
  /* Table 6-12 --- picture_coding_type */
  DPRINTFI(1, "picture_coding_type: %01x, ", pic.header.picture_coding_type);

  switch(pic.header.picture_coding_type) {
  case PIC_CODING_TYPE_FORBIDDEN:
    DPRINTF(1, "forbidden\n");
    break;
  case PIC_CODING_TYPE_I:
    DPRINTF(1, "intra-coded (I)\n");
    break;
  case PIC_CODING_TYPE_P:
    DPRINTF(1, "predictive-coded (P)\n");
    break;
  case PIC_CODING_TYPE_B:
    DPRINTF(1, "bidirectionally-predictive-coded (B)\n");
    break;
  case PIC_CODING_TYPE_D:
    DPRINTF(1, "shall not be used (dc intra-coded (D) in ISO/IEC11172-2)\n");
    break;
  default:
    DPRINTF(1, "reserved\n");
    break;
  }
#endif

  pic.header.vbv_delay = GETBITS(16, "vbv_delay");
  
  /* To be able to use the same motion vector code for MPEG-1 and 2 we
     use f_code[] instead of forward_f_code/backward_f_code. 
     In MPEG-2 f_code[] values will be read from the bitstream (later) in 
     picture_coding_extension(). */

  if((pic.header.picture_coding_type == PIC_CODING_TYPE_P) ||
     (pic.header.picture_coding_type == PIC_CODING_TYPE_B)) {
    pic.header.full_pel_vector[0] = GETBITS(1, "full_pel_forward_vector");
    pic.header.forward_f_code = GETBITS(3, "forward_f_code");
    if(pic.header.forward_f_code == 0) {
      pic.header.forward_f_code = 1;
      fprintf(stderr, " ** forward_f_code == ZERO\n");
    }
    pic.coding_ext.f_code[0][0] = pic.header.forward_f_code;
    pic.coding_ext.f_code[0][1] = pic.header.forward_f_code;
  }
  
  if(pic.header.picture_coding_type == PIC_CODING_TYPE_B) {
    pic.header.full_pel_vector[1] = GETBITS(1, "full_pel_backward_vector");
    pic.header.backward_f_code = GETBITS(3, "backward_f_code");
    if(pic.header.backward_f_code == 0) {
      pic.header.backward_f_code = 1;
      fprintf(stderr, "** backward_f_code == ZERO\n");
    }
    pic.coding_ext.f_code[1][0] = pic.header.backward_f_code;
    pic.coding_ext.f_code[1][1] = pic.header.backward_f_code;
  }
  
  while(nextbits(1) == 1) {
    GETBITS(1, "extra_bit_picture");
    GETBITS(8, "extra_information_picture");
  }
  GETBITS(1, "extra_bit_picture");
  
  DINDENT(-2);
  next_start_code();
}


// Get id of empty buffer
int get_picture_buf()
{
  int n;
  MsgEvent_t ev;
  
  //fprintf(stderr, "vs: searching for free picture\n");
  //  search for empty buffer
  while(1) {
    /*
      for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
      fprintf(stderr, "vs: [%d] is_ref: @%d, disp: @%d\n",
      n+4,
      picture_ctrl_data[n].is_reference,
      picture_ctrl_data[n].displayed);
      }
    */
    ev.type = MsgEventQNone;
    for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
      
      if((picture_ctrl_data[n].is_reference == 0) &&
	 (picture_ctrl_data[n].displayed == 1)) {
	picture_ctrl_data[n].displayed = 0;
	// found buf
	//fprintf(stderr, "vs: found free picture buf: @%d\n", n);
	//fprintf(stderr, "|\n");

	return n;
      }
    }
    
    // no empty buffer
    picture_q_head->writer_requests_notification = 1;
    //fprintf(stderr, "vs: didn't find free picture, setting notification\n");
    /*
    for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
      fprintf(stderr, "vs: [%d] is_ref: @%d, disp: @%d\n",
	      n+4,
	      picture_ctrl_data[n].is_reference,
	      picture_ctrl_data[n].displayed);
    }
    */
    while(ev.type != MsgEventQNotify) {
      //fprintf(stderr, ".");
      for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
	
	if((picture_ctrl_data[n].is_reference == 0) &&
	   (picture_ctrl_data[n].displayed == 1)) {
	  picture_ctrl_data[n].displayed = 0;
	  // found buf;
	  //fprintf(stderr, "vs: found free2 picture buf: @%d\n", n);
	  //fprintf(stderr, "+\n");
	  
	  return n;
	}
      }
      //fprintf(stderr, "video_decode: waiting for notification1\n");
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
      /*
      for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
	fprintf(stderr, "vs: [%d] is_ref: @%d, disp: @%d\n", 
		n+4,
		picture_ctrl_data[n].is_reference,
		picture_ctrl_data[n].displayed);
      }
      */
    }
  }
}


// Put decoded picture into display queue
void dpy_q_put(int id)
{
  MsgEvent_t ev;
  int elem;
  
  elem = picture_q_head->write_nr;
  /*
  fprintf(stderr, "vs: try put picture in q, elem: @%d, bufid: @%d\n",
	  elem, id);
  */
  if(picture_q_elems[elem].in_use) {
    picture_q_head->writer_requests_notification = 1;
    //fprintf(stderr, "vs:  elem in use, setting notification\n");
    
    while(picture_q_elems[elem].in_use) {
      //fprintf(stderr, "video_decode: waiting for notification2\n");
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
    }
  }

  //fprintf(stderr, "vs:  elem free to fill\n");  
  
  picture_q_elems[elem].data_elem_index = id;
  picture_q_elems[elem].in_use = 1;
  
  picture_q_head->write_nr =
    (picture_q_head->write_nr + 1) % picture_q_head->nr_of_qelems;
  
  if(picture_q_head->reader_requests_notification) {
    //fprintf(stderr, "vs:  reader wants notify, sending...\n");  

    picture_q_head->reader_requests_notification = 0;
    ev.type = MsgEventQNotify;
    ev.notify.qid = picture_q_head->qid;
    if(MsgSendEvent(msgq, picture_q_head->reader, &ev, 0) == -1) {
      fprintf(stderr, "video_decode: couldn't send notification\n");
    }
  }
}


/* 6.2.3.6 Picture data */
void picture_data(void)
{
  static int buf_id;
  static int fwd_ref_buf_id = -1;
  static int bwd_ref_buf_id  = -1;
  int err;
  picture_data_elem_t *pinfos;
  static int bwd_ref_temporal_reference = -1;

  static int last_timestamped_temp_ref = -1;
  static int drop_frame = 0;
  int temporal_reference_error = 0;
  pinfos = picture_ctrl_data;
 
  DPRINTFI(1, "picture_data()\n");
  DINDENT(2);
  
  
  if(msgqid != -1) {
    MsgEvent_t ev;
    while(MsgCheckEvent(msgq, &ev) != -1) {
      handle_events(msgq, &ev);
      fprintf(stderr, "/");
    }
    //chk_for_msg();
  }
  
  DPRINTFI(1, "last_temporal_ref_to_dpy: %d\n", last_temporal_ref_to_dpy);
  DPRINTFI(1, "bwd_ref_temporal_reference: %d\n", bwd_ref_temporal_reference);

  if(prev_coded_temp_ref != pic.header.temporal_reference) {
    
    /* If this is a I/P picture then we must release the reference 
       frame that is going to be replaced. (It might not have been 
       displayed yet so it is not necessarily free for reuse.) */
    
    switch(pic.header.picture_coding_type) {
    case PIC_CODING_TYPE_I:
    case PIC_CODING_TYPE_P:
      
      /* check to see if a temporal reference has been skipped */
      
      if(bwd_ref_temporal_reference != -1) {
	
	fprintf(stderr, "** temporal reference skipped\n");
	
	last_temporal_ref_to_dpy = bwd_ref_temporal_reference;
	
	/* bwd_ref should not be in the dpy_q more than one time */
	bwd_ref_temporal_reference = -1;
	
	/* put bwd_ref in dpy_q */
	
	last_pts_to_dpy = pinfos[bwd_ref_buf_id].PTS;
	last_scr_nr_to_dpy = pinfos[bwd_ref_buf_id].scr_nr;
	
	if(flush_to_scrnr != -1) {
	  
	  pinfos[bwd_ref_buf_id].is_reference = 0;
	  pinfos[bwd_ref_buf_id].displayed = 1;
	} else {
	  
	  dpy_q_put(bwd_ref_buf_id);
	}	
      }
      DPRINTFI(1, "last_temporal_ref_to_dpy: %d\n", last_temporal_ref_to_dpy);
      DPRINTFI(1, "bwd_ref_temporal_reference: %d\n", bwd_ref_temporal_reference);
	

      if(fwd_ref_buf_id != -1) {
	/* current fwd_ref_image is not used as reference any more */
	pinfos[fwd_ref_buf_id].is_reference = 0;
      }
      
      /* get new buffer */
      buf_id = get_picture_buf();
      dst_image = &image_bufs[buf_id];

      
      /* Age the reference frame */
      fwd_ref_buf_id = bwd_ref_buf_id; 
      fwd_ref_image = bwd_ref_image; 
      
      /* and add the new (to be decoded) frame */
      bwd_ref_image = dst_image;
      bwd_ref_buf_id = buf_id;
      
      bwd_ref_temporal_reference = pic.header.temporal_reference;
      
      /* this buffer is used as reference picture by the decoder */
      pinfos[buf_id].is_reference = 1; 
      
      break;
    case PIC_CODING_TYPE_B:
      
      /* get new buffer */
      buf_id = get_picture_buf();
      dst_image = &image_bufs[buf_id];
      
      break;
    }
    DPRINTFI(1, "last_temporal_ref_to_dpy: %d\n", last_temporal_ref_to_dpy);
    DPRINTFI(1, "bwd_ref_temporal_reference: %d\n", bwd_ref_temporal_reference);
  
    /*
     * temporal reference is incremented by 1 for every frame.
     * 
     *  this can be used to keep track of the order in which the pictures
     *  shall be displayed. 
     *  it can not be used to calculate the time when a specific picture
     *  should be displayed (one can make a guess, but
     *  it isn't necessarily true that a frame with a temporal reference
     *  1 greater than the previous picture should be displayed
     *  1 frame interval later)
     *
     *
     * time stamp
     *
     * this tells when a picture shall be displayed
     * not all pictured have time stamps
     *
     *
     */
    
    /*
     * Time stamps
     *
     * case 1:
     *  The packet containing the picture header start code
     *  had a time stamp.
     *
     *  In this case the time stamp is used for this picture.
     *
     * case 2:
     *  The packet containing the picture header start code
     *  didn't have a time stamp.
     *
     *  In this case the time stamp for this picture must be calculated.
     *  
     *  case 2.1:
     *   There is a previously decoded picture with a time stamp
     *   in the same temporal reference context
     *
     *   If the temporal reference for the previous picture is lower
     *   than the temp_ref for this picture then we take
     *   the difference between the two temp_refs and multiply
     *   with the frame interval time and then add this to
     *   to the original time stamp to get the time stamp for this picture
     *
     *   timestamp = (this_temp_ref - timestamped_temp_ref)*
     *                frame_interval+timestamped_temp_ref_timestamp
     * 
     *   todo: We must take into account that the temporal reference wraps
     *   at 1024.
     *
     *
     *   If the temporal reference for the previous picture is higher
     *   than the current, we do the same.
     *   
     *   
     *  case 2.2:
     *   There is no previously decoded picture with a time stamp
     */
    
    
    
    
    /* If the packet containing the picture header start code had 
       a time stamp, that time stamp is used.
       
       Otherwise a time stamp is calculated from the last picture 
       produced for viewing. 
       Iff we predict the time stamp then we must also make sure to use 
       the same scr as the picure we predict from.
    */
    if(picture_has_pts) {

      last_timestamped_temp_ref = pic.header.temporal_reference;
      pinfos[buf_id].PTS = last_pts;
      pinfos[buf_id].PTS_DTS_flags = 0x02;
      pinfos[buf_id].scr_nr = last_scr_nr;
      /*
	fprintf(stderr, "#%ld.%09ld\n",
	TIME_S (pinfos[buf_id].pts_time),
	TIME_SS(pinfos[buf_id].pts_time));
      */
    } else {
      /* Predict if we don't already have a pts for the frame. */
      uint64_t calc_pts;

      switch(pic.header.picture_coding_type) {
      case PIC_CODING_TYPE_I:
      case PIC_CODING_TYPE_P:
	/* TODO: Is this correct? */
	
	/* First case: we don't use the temporal_reference
	 * In this case we can calculate the time stamp for
	 * the oldest reference frame (fwd_ref) to be displayed when
	 * we get a new reference frame
	 *
	 * The forward ref time stamp should be the 
	 * previous displayed frame's timestamp plus one frame interval,
	 * because when we get a new reference frame we know that the
	 * next frame to display is the old reference frame(fwd_ref)
	 */
	
	/* In case the fwd_ref picture already has a time stamp, do nothing
	 * Also check to see that we do have a fwd_ref picture
	 */
	
	/* Second case: We use the temporal_reference
	 * In this case we can look at the previous temporal ref
	 */
	if(last_timestamped_temp_ref != -1) {
	  calc_pts = last_pts +
	    (pic.header.temporal_reference - last_timestamped_temp_ref) *
	    frame_interval;
	  /*
	    calc_pts = last_pts_to_dpy +
	    buf_ctrl_head->frame_interval;
	  */
	} else {
	  if(last_temporal_ref_to_dpy == -1) {
	    calc_pts = last_pts_to_dpy +
	      (pic.header.temporal_reference - last_timestamped_temp_ref) *
	      frame_interval;
	  } else {
	    calc_pts = last_pts_to_dpy +
	      (pic.header.temporal_reference - last_temporal_ref_to_dpy) *
	      frame_interval;	    
	  }   
	}
	
	pinfos[buf_id].PTS = calc_pts;
	pinfos[buf_id].scr_nr = last_scr_nr;

	/*
	fprintf(stderr, "last_timestamped: %d\n",
		last_timestamped_temp_ref);

	fprintf(stderr, "last_pts: %lld\n",
		last_pts);

	fprintf(stderr, "last_pts_to_dpy: %lld\n",
		last_pts_to_dpy);
	
	fprintf(stderr, "pts_time %ld.%09ld\n",
		TIME_S (pinfos[buf_id].pts_time),
		TIME_SS(pinfos[buf_id].pts_time));
	
	fprintf(stderr, "realtime_offset %ld.%09ld\n",
		TIME_S (ctrl_time[last_scr_nr].realtime_offset),
		TIME_SS(ctrl_time[last_scr_nr].realtime_offset));
	*/

	
	break;
      case PIC_CODING_TYPE_B:
	/* In case we don't have a previous displayed picture
	 * we don't now what we should set this timestamp to
	 * unless we look at the temporal reference.
	 * To be able to use the temporal reference we need
	 * to have a pts in the same temporal reference 'sequence'.
	 * (the temporal reference sequence is reset to 0 for every
	 * GOP
	 */
	
	/* In case we don't use temporal reference
	 * we don't know what the pts should be, but we calculate it
	 * anyway and hope we end up with a time that is earlier
	 * than the next 'real' time stamp.
	 */
	
	/* We use temporal reference and calculate the timestamp
	 * from the last decoded picture which had a timestamp
	 */
	/* TODO: Check if there is a valid 'last_pts_to_dpy' to predict from.*/

	calc_pts = last_pts +
	  (pic.header.temporal_reference - last_timestamped_temp_ref) *
	  frame_interval;
	/*
	  calc_pts = last_pts_to_dpy + 
	  buf_ctrl_head->frame_interval;
	*/
	pinfos[buf_id].PTS = calc_pts;
	pinfos[buf_id].scr_nr = last_scr_nr;
	
	break;
      }
    }
    
    /* When it is a B-picture we are decoding we know that it is
     * the picture that is going to be displayed next.
     * We check to see if we are lagging behind the desired time
     * and in that case we don't decode/show this picture
     */
    
    /* Calculate the time remaining until this picture shall be viewed. */
    if(pic.header.picture_coding_type == PIC_CODING_TYPE_B) {
      clocktime_t realtime, calc_rt, err_time, pts_time;

      PTS_TO_CLOCKTIME(pts_time, pinfos[buf_id].PTS);
      clocktime_get(&realtime);
      if(ctrl_time[last_scr_nr].offset_valid == OFFSET_VALID) {
	timeadd(&calc_rt,
		&pts_time,
		&ctrl_time[last_scr_nr].realtime_offset);
      } else {
	calc_rt = realtime;
	fprintf(stderr, "*vs: offset not valid\n");
      }
      
      if(ctrl_data->mode == MODE_SPEED) {
	clocktime_t difftime;
	
	timesub(&difftime, &realtime, &(ctrl_data->rt_start));
	timemul(&difftime, &difftime, ctrl_data->speed);
	timeadd(&realtime, &(ctrl_data->rt_start), &difftime);
	timesub(&realtime, &realtime, &ctrl_data->vt_offset);
      }
      
      timesub(&err_time, &calc_rt, &realtime);
      
      /* If the picture should already have been displayed then drop it. */
      /* TODO: More investigation needed. */
      if((TIME_SS(err_time) < 0 || TIME_S(err_time) < 0) 
	 && forced_frame_rate) {
	fprintf(stderr, "!");
	
	/*
	  fprintf(stderr, "errpts %ld.%+010ld\n\n",
	  TIME_S(err_time), TIME_SSerr_time));
	*/
	
	/* mark the frame to be dropped */
	drop_frame = 1;
      }
    }
  }/* else {
      // either this is the second field of the frame or it is an error
      fprintf(stderr, "*** error prev_temp_ref == cur_temp_ref\n");
      
      }
   */

  { // Don't use floating point math!
    int sar_frac_n = 1;
    int sar_frac_d = 1;
    //double sar = 1.0;
    uint16_t hsize,vsize;
    
      //wrong on some dvd's ?
    if(seq.dpy_ext.display_horizontal_size) { 
      hsize = seq.dpy_ext.display_horizontal_size;
      vsize = seq.dpy_ext.display_vertical_size;
      /*      fprintf(stderr, "*****bepa %d, %d\n",
	      hsize, vsize);
      */
    } else {
      hsize = seq.horizontal_size;
      vsize = seq.vertical_size;
    }
    switch(seq.header.aspect_ratio_information) {
    case 0x0:
      DPRINTF(2, "forbidden\n");
      break;
    case 0x1:
      DPRINTF(2, "SAR = 1.0\n");
      sar_frac_n = 1;
      sar_frac_d = 1;
      //sar = 1.0;
      break;
    case 0x2:
      DPRINTF(2, "DAR = 3/4\n");
      sar_frac_n = 3 * hsize;
      sar_frac_d = 4 * vsize;
      //sar = 3.0/4.0*(double)hsize/(double)vsize;
      break;
    case 0x3:
      DPRINTF(2, "DAR = 9/16\n");
      if(seq.dpy_ext.display_horizontal_size) { 
	if(seq.dpy_ext.display_horizontal_size != seq.horizontal_size) {
	  // DVD with wrong mpeg display_extension
	  hsize = seq.horizontal_size;
	  vsize = seq.vertical_size;
	}
      }
      sar_frac_n = 9 * hsize;
      sar_frac_d = 16 * vsize;
      //sar = 9.0/16.0*(double)hsize/(double)vsize;
      break;
    case 0x4:
      DPRINTF(2, "DAR = 1/2.21\n");
      sar_frac_n = 100 * hsize;
      sar_frac_d = 221 * vsize;
      //sar = 1.0/2.21*(double)hsize/(double)vsize;
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
    pinfos[buf_id].picture.sar_frac_n = sar_frac_n;
    pinfos[buf_id].picture.sar_frac_d = sar_frac_d;
  }
  

  if(flush_to_scrnr != -1) {
    //    fprintf(stderr, "vs: flush picture\n");
  }


  /* now we can decode the picture if it shouldn't be dropped */
  if(!drop_frame && (flush_to_scrnr == -1)) {
    /* Decode the slices. */
    if( MPEG2 ) {
      do {
	int slice_nr;
	slice_nr = nextbits(32) & 0xff;
	mpeg2_slice();
	if(slice_nr >= seq.mb_height) {
	  break;
	}
	next_start_code();      
      } while((nextbits(32) >= MPEG2_VS_SLICE_START_CODE_LOWEST) &&
	      (nextbits(32) <= MPEG2_VS_SLICE_START_CODE_HIGHEST));
    } else {
      do {
	err = mpeg1_slice();
	if(err == -1) {
	  drop_to_next_picture();
	  break;
	}
      } while((nextbits(32) >= MPEG2_VS_SLICE_START_CODE_LOWEST) &&
	      (nextbits(32) <= MPEG2_VS_SLICE_START_CODE_HIGHEST));
    }
  } else {
    do {
      //TODO change to fast startcode finder
      GETBITS(8, "drop");
      next_start_code();
    } while((nextbits(32) >= MPEG2_VS_SLICE_START_CODE_LOWEST) &&
	    (nextbits(32) <= MPEG2_VS_SLICE_START_CODE_HIGHEST));
  }

  // Check 'err' here?
  /*
  fprintf(stderr, "coding_type: %d, temp_ref: %d\n",
	  pic.header.picture_coding_type,
	  pic.header.temporal_reference);
  */

  // Picture decoded
  if((prev_coded_temp_ref == pic.header.temporal_reference) ||
     (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE)) {
    
    if(pic.header.picture_coding_type == PIC_CODING_TYPE_B) {
      if(pic.header.temporal_reference == (last_temporal_ref_to_dpy+1)%1024) {
	
	last_temporal_ref_to_dpy = pic.header.temporal_reference;
	last_pts_to_dpy = pinfos[buf_id].PTS;
	last_scr_nr_to_dpy = pinfos[buf_id].scr_nr;//?
	
	if(drop_frame || (flush_to_scrnr != -1)) {
	  drop_frame = 0;
	  pinfos[buf_id].is_reference = 0; //this is never set in a B picture?
	  pinfos[buf_id].displayed = 1;
	  
	} else {
	  
	  dpy_q_put(buf_id);
	  
	}
	
	if(bwd_ref_buf_id == -1) {
	  fprintf(stderr, "decode: **B-frame before any reference frame!!!\n");
	}
	
	if(fwd_ref_buf_id == -1) { // Test closed_gop too....
	  fprintf(stderr, "decode: B-frame before forward ref frame\n");
	}
	
      } else {
	/* TODO: what should happen if the temporal reference is wrong */
	fprintf(stderr, "** temporal reference for B-picture incorrect\n");
	
	temporal_reference_error =
	  pic.header.temporal_reference - (last_temporal_ref_to_dpy + 1)%1024;
	
	last_temporal_ref_to_dpy = pic.header.temporal_reference;
	//last_temporal_ref_to_dpy++;
	last_pts_to_dpy = pinfos[buf_id].PTS;
	last_scr_nr_to_dpy = pinfos[buf_id].scr_nr;//?
	
	dpy_q_put(buf_id);
      }
    }
    /*
      if(temporal_reference_error) {
        if((bwd_ref_temporal_reference != -1)) {
          dpy_q_put(bwd_ref_buf_id);
        }
      }
    */
    
    if((bwd_ref_temporal_reference != -1) && 
       (bwd_ref_temporal_reference == (last_temporal_ref_to_dpy+1)%1024)) {

      last_temporal_ref_to_dpy = bwd_ref_temporal_reference;
      
      /* bwd_ref should not be in the dpy_q more than one time */
      bwd_ref_temporal_reference = -1;
      
      /* put bwd_ref in dpy_q */
      last_pts_to_dpy = pinfos[bwd_ref_buf_id].PTS;
      last_scr_nr_to_dpy = pinfos[bwd_ref_buf_id].scr_nr;

      if(flush_to_scrnr != -1) {
	pinfos[bwd_ref_buf_id].is_reference = 0;
	pinfos[bwd_ref_buf_id].displayed = 1;
      } else {
	
	dpy_q_put(bwd_ref_buf_id);
      }
    } else if(bwd_ref_temporal_reference < (last_temporal_ref_to_dpy+1)%1024) {
      
      fprintf(stderr, "** temporal reference in I or P picture incorrect\n");
      
    }
  }
  
  prev_coded_temp_ref = pic.header.temporal_reference;
  
  DINDENT(-2);
  next_start_code();
}


/* 6.2.2.2.1 Extension data */
void extension_data(unsigned int i)
{
  DPRINTFI(1, "extension_data(%d)", i);
  DINDENT(2);
  
  while(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
    GETBITS(32, "extension_start_code");
    
    if(i == 0) { /* follows sequence_extension() */
      
      if(nextbits(4) == MPEG2_VS_SEQUENCE_DISPLAY_EXTENSION_ID) {
	sequence_display_extension();
      }
      if(nextbits(4) == MPEG2_VS_SEQUENCE_SCALABLE_EXTENSION_ID) {
	sequence_scalable_extension();
      }
    }

    /* extension never follows a group_of_picture_header() */
    
    if(i == 2) { /* follows picture_coding_extension() */

      if(nextbits(4) == MPEG2_VS_QUANT_MATRIX_EXTENSION_ID) {
	quant_matrix_extension();
      }
      if(nextbits(4) == MPEG2_VS_PICTURE_DISPLAY_EXTENSION_ID) {
	/* the draft says picture_pan_scan_extension_id (must be wrong?) */
	picture_display_extension();
      }
      if(nextbits(4) == MPEG2_VS_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID) {
	picture_spatial_scalable_extension();
      }
      if(nextbits(4) == MPEG2_VS_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID) {
	picture_temporal_scalable_extension();
      }
    }
  }
  DINDENT(-2);
}

 
void reset_to_default_quantiser_matrix()
{
  memcpy(seq.header.intra_inverse_quantiser_matrix,
	 default_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.intra_inverse_quantiser_matrix));

  memcpy(seq.header.non_intra_inverse_quantiser_matrix,
	 default_non_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.non_intra_inverse_quantiser_matrix));
  
}

void reset_to_default_intra_quantiser_matrix()
{
  memcpy(seq.header.intra_inverse_quantiser_matrix,
	 default_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.intra_inverse_quantiser_matrix));
}

void reset_to_default_non_intra_quantiser_matrix()
{
  memcpy(seq.header.non_intra_inverse_quantiser_matrix,
	 default_non_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.non_intra_inverse_quantiser_matrix));
}



/* 6.2.3.2 Quant matrix extension */
void quant_matrix_extension()
{
  GETBITS(4, "extension_start_code_identifier");
  
  DPRINTFI(1, "quant_matrix_extension()\n");
  DINDENT(2);
  
  if(GETBITS(1, "load_intra_quantiser_matrix")) {
    int n;
    intra_inverse_quantiser_matrix_changed = 1;

    for(n = 0; n < 64; n++) {
      seq.header.intra_quantiser_matrix[n] 
	= GETBITS(8, "intra_quantiser_matrix[n]");
    }
    
    /* 7.3.1 Inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.intra_inverse_quantiser_matrix[v*8+u] =
	    seq.header.intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
  }
    
  if(GETBITS(1, "load_non_intra_quantiser_matrix")) {
    int n;
    non_intra_inverse_quantiser_matrix_changed = 1;
    
    for(n = 0; n < 64; n++) {
      seq.header.non_intra_quantiser_matrix[n] 
	= GETBITS(8, "non_intra_quantiser_matrix[n]");
    }
   
    /*  7.3.1 Inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.non_intra_inverse_quantiser_matrix[v*8+u] =
	    seq.header.non_intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
  }
  GETBITS(1, "load_chroma_intra_quantiser_matrix (always 0 in 4:2:0)");
  GETBITS(1, "load_chroma_non_intra_quantiser_matrix (always 0 in 4:2:0)");
  
  DINDENT(-2);
  next_start_code();
}


void picture_display_extension()
{
  uint8_t extension_start_code_identifier;
  uint16_t frame_centre_horizontal_offset;
  uint16_t frame_centre_vertical_offset;
  uint8_t number_of_frame_centre_offsets;
  int i;

  DPRINTFI(1, "picture_display_extension()\n");
  DINDENT(2);
  
  if((seq.ext.progressive_sequence == 1) || 
     ((pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD) ||
      (pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD))) {
    number_of_frame_centre_offsets = 1;
  } else {
    if(pic.coding_ext.repeat_first_field == 1) {
      number_of_frame_centre_offsets = 3;
    } else {
      number_of_frame_centre_offsets = 2;
    }
  }
  
  extension_start_code_identifier 
    = GETBITS(4,"extension_start_code_identifier");

  for(i = 0; i < number_of_frame_centre_offsets; i++) {
    frame_centre_horizontal_offset = GETBITS(16, "frame_centre_horizontal_offset");
    marker_bit();
    frame_centre_vertical_offset = GETBITS(16, "frame_centre_vertical_offset");
    marker_bit();
    DPRINTFI(2, "frame_centre_offset: %d, %d\n",
	     frame_centre_horizontal_offset,
	     frame_centre_vertical_offset);
  }
  DINDENT(-2);
  next_start_code();
}


void picture_spatial_scalable_extension()
{
  fprintf(stderr, "***ni picture_spatial_scalable_extension()\n");
  exit(-1);
}


void picture_temporal_scalable_extension()
{
  fprintf(stderr, "***ni picture_temporal_scalable_extension()\n");
  exit(-1);
}


void sequence_scalable_extension()
{
  fprintf(stderr, "***ni sequence_scalable_extension()\n");
  exit(-1);
}


void sequence_display_extension()
{
  
  DPRINTFI(1, "sequence_display_extension()\n");
  DINDENT(2);
  
  seq.dpy_ext.extension_start_code_identifier 
    = GETBITS(4,"extension_start_code_identifier");
  seq.dpy_ext.video_format = GETBITS(3, "video_format");
  seq.dpy_ext.colour_description = GETBITS(1, "colour_description");

#ifdef DEBUG
  /* Table 6-6. Meaning of video_format */
  DPRINTFI(2, "video_format: ");
  switch(seq.dpy_ext.video_format) {
  case 0x0:
    DPRINTF(2, "component\n");
    break;
  case 0x1:
    DPRINTF(2, "PAL\n");
    break;
  case 0x2:
    DPRINTF(2, "NTSC\n");
    break;
  case 0x3:
    DPRINTF(2, "SECAM\n");
    break;
  case 0x4:
    DPRINTF(2, "MAC\n");
    break;
  case 0x5:
    DPRINTF(2, "Unspecified video format\n");
    break;
  default:
    DPRINTF(2, "reserved\n");
    break;
  }
#endif


  if(seq.dpy_ext.colour_description) {
    seq.dpy_ext.colour_primaries = GETBITS(8, "colour_primaries");
    seq.dpy_ext.transfer_characteristics = GETBITS(8, "transfer_characteristics");
    seq.dpy_ext.matrix_coefficients = GETBITS(8, "matrix_coefficients");
    
#ifdef DEBUG
    /* Table 6-7. Colour Primaries */
    DPRINTFI(2, "colour primaries (Table 6-7): ");
    switch(seq.dpy_ext.colour_primaries) {
    case 0x0:
      DPRINTF(2, "(forbidden)\n");
      break;
    case 0x1:
      DPRINTF(2, "ITU-R 709\n");
      break;
    case 0x2:
      DPRINTF(2, "Unspecified Video\n");
      break;
    case 0x3:
      DPRINTF(2, "reserved\n");
      break;
    case 0x4:
      DPRINTF(2, "ITU-R 624-4 System M\n");
      break;
    case 0x5:
      DPRINTF(2, "ITU-R 624-4 System B,G\n");
      break;
    case 0x6:
      DPRINTF(2, "SMPTE 170M\n");
      break;
    case 0x7:
      DPRINTF(2, "SMPTE 240M\n");
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
#endif
    
#ifdef DEBUG
    /* Table 6-8. Transfer Characteristics */
    DPRINTFI(2, "transfer characteristics (Table 6-8): ");
    switch(seq.dpy_ext.transfer_characteristics) {
    case 0x0:
      DPRINTF(2, "(forbidden)\n");
      break;
    case 0x1:
      DPRINTF(2, "ITU-R 709\n");
      break;
    case 0x2:
      DPRINTF(2, "Unspecified Video\n");
      break;
    case 0x3:
      DPRINTF(2, "reserved\n");
      break;
    case 0x4:
      DPRINTF(2, "ITU-R 624-4 System M\n");
      break;
    case 0x5:
      DPRINTF(2, "ITU-R 624-4 System B,G\n");
      break;
    case 0x6:
      DPRINTF(2, "SMPTE 170M\n");
      break;
    case 0x7:
      DPRINTF(2, "SMPTE 240M\n");
      break;
    case 0x8:
      DPRINTF(2, "Linear transfer characteristics\n");
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
#endif
    
#ifdef DEBUG
    /* Table 6-9. Matrix Coefficients */
    DPRINTFI(2, "matrix coefficients (Table 6-9): ");
    switch(seq.dpy_ext.matrix_coefficients) {
    case 0x0:
      DPRINTF(2, "(forbidden)\n");
      break;
    case 0x1:
      DPRINTF(2, "ITU-R 709\n");
      break;
    case 0x2:
      DPRINTF(2, "Unspecified Video\n");
      break;
    case 0x3:
      DPRINTF(2, "reserved\n");
      break;
    case 0x4:
      DPRINTF(2, "FCC\n");
      break;
    case 0x5:
      DPRINTF(2, "ITU-R 624-4 System B,G\n");
      break;
    case 0x6:
      DPRINTF(2, "SMPTE 170M\n");
      break;
    case 0x7:
      DPRINTF(2, "SMPTE 240M\n");
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
#endif
  }

  seq.dpy_ext.display_horizontal_size = GETBITS(14, "display_horizontal_size");
  marker_bit();
  seq.dpy_ext.display_vertical_size = GETBITS(14, "display_vertical_size");

  DPRINTFI(2, "display_horizontal_size: %d\n",
	   seq.dpy_ext.display_horizontal_size);
  DPRINTFI(2, "display_vertical_size: %d\n",
	   seq.dpy_ext.display_vertical_size);

  DINDENT(-2);
  next_start_code();
}


void exit_program(int exitcode)
{
  // Detach the shared memory segments from this process  
  shmdt(picture_buffers_shmaddr);
  shmdt(buf_ctrl_shmaddr);
  
  shmctl(picture_buffers_shmid, IPC_RMID, 0);
  shmctl(buf_ctrl_shmid, IPC_RMID, 0);

  exit(exitcode);
}

