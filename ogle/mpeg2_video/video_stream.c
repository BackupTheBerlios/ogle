/* SKROMPF - A video player
 * Copyright (C) 2000 Bj�rn Englund, H�kan Hjort
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
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <semaphore.h>

#include "video_stream.h"
#include "video_types.h"
#include "video_tables.h"

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

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#if defined USE_SYSV_SEM
#include <sys/sem.h>
#endif
#include <glib.h>

#include "../include/common.h"
#include "c_getbits.h"

#ifndef HAVE_SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#if defined USE_SYSV_SEM
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
  int val;                    /* value for SETVAL */
  struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
  unsigned short int *array;  /* array for GETALL, SETALL */
  struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif
#endif

/*
void timeadd(struct timespec *d, struct timespec *s1, struct timespec *s2);
void timesub(struct timespec *d, struct timespec *s1, struct timespec *s2);
*/


extern int chk_for_msg();

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


int dctstat[128];
int total_pos = 0;
int total_calls = 0;


unsigned int debug = 2;
int shm_ready = 0;
static int shmem_flag = 1;

int forced_frame_rate = -1;
int nr_of_buffers = 3;
buf_ctrl_head_t *buf_ctrl_head;
int picture_buffers_shmid;
int buf_ctrl_shmid;
char *buf_ctrl_shmaddr;
char *picture_buffers_shmaddr;



void init_out_q(int nr_of_bufs);
void setup_shm(int horiz_size, int vert_size, int nr_of_bufs);

double time_pic[4] = { 0.0, 0.0, 0.0, 0.0};
double time_max[4] = { 0.0, 0.0, 0.0, 0.0};
double time_min[4] = { 1.0, 1.0, 1.0, 1.0};
double num_pic[4]  = { 0.0, 0.0, 0.0, 0.0};


int last_temporal_ref_to_dpy = -1;

int MPEG2 = 0;

uint8_t intra_inverse_quantiser_matrix_changed = 1;
uint8_t non_intra_inverse_quantiser_matrix_changed = 1;


uint32_t stats_block_intra_nr = 0;
uint32_t stats_f_intra_compute_subseq_nr = 0;
uint32_t stats_f_intra_compute_first_nr = 0;

#ifdef TIMESTAT

void timestat_init();
void timestat_print();

struct timespec picture_decode_start_time;
struct timespec picture_decode_end_time;
struct timespec picture_display_start_time;
struct timespec picture_display_end_time;

struct timespec picture_time;
struct timespec picture_decode_time;
struct timespec picture_display_time;

struct timespec picture_time0;
struct timespec picture_time1;
struct timespec picture_time2;

struct timespec picture_decode_time_tot[3];
struct timespec picture_display_time_tot[3];

struct timespec picture_decode_time_min[3];
struct timespec picture_display_time_min[3];

struct timespec picture_decode_time_max[3];
struct timespec picture_display_time_max[3];

uint32_t picture_decode_nr[3];
uint32_t picture_display_nr[3];

struct timespec *picture_decode_start_times;
struct timespec *picture_decode_end_times;
struct timespec *picture_display_start_times;
struct timespec *picture_display_end_times;
uint8_t *picture_type;


uint32_t picture_decode_num;
uint32_t picture_display_num;

typedef struct {
  struct timespec dec_start;
  struct timespec dec_end;
  struct timespec dec_tot;
  struct timespec dec_min;
  struct timespec disp_start;
  struct timespec disp_end;
  struct timespec disp_tot;
  struct timespec disp_min;
  struct timespec pic_tot;
  struct timespec pic_min;
  struct timespec should_sleep;
  struct timespec did_sleep;
  int type;
} picture_time_t;

picture_time_t *picture_timestat;

#endif


sequence_t seq;
picture_t pic;
slice_t slice_data;
macroblock_t mb;


yuv_image_t *fwd_ref_image;
yuv_image_t *bwd_ref_image;
yuv_image_t *dst_image;





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

void display_init();
void display_exit();
void display_process();

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


#ifdef HAVE_CLOCK_GETTIME


static void timesub(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1-s2

  d->tv_sec = s1->tv_sec - s2->tv_sec;
  d->tv_nsec = s1->tv_nsec - s2->tv_nsec;
  
  if(d->tv_nsec >= 1000000000) {
    d->tv_sec += 1;
    d->tv_nsec -= 1000000000;
  } else if(d->tv_nsec <= -1000000000) {
    d->tv_sec -= 1;
    d->tv_nsec += 1000000000;
  }

  if((d->tv_sec > 0) && (d->tv_nsec < 0)) {
    d->tv_sec -= 1;
    d->tv_nsec += 1000000000;
  } else if((d->tv_sec < 0) && (d->tv_nsec > 0)) {
    d->tv_sec += 1;
    d->tv_nsec -= 1000000000;
  }

}  

static void timeadd(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1+s2
  
  d->tv_sec = s1->tv_sec + s2->tv_sec;
  d->tv_nsec = s1->tv_nsec + s2->tv_nsec;
  if(d->tv_nsec >= 1000000000) {
    d->tv_nsec -=1000000000;
    d->tv_sec +=1;
  } else if(d->tv_nsec <= -1000000000) {
    d->tv_nsec +=1000000000;
    d->tv_sec -=1;
  }

  if((d->tv_sec > 0) && (d->tv_nsec < 0)) {
    d->tv_sec -= 1;
    d->tv_nsec += 1000000000;
  } else if((d->tv_sec < 0) && (d->tv_nsec > 0)) {
    d->tv_sec += 1;
    d->tv_nsec -= 1000000000;
  }
}  

static int timecompare(struct timespec *s1, struct timespec *s2) {

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

#else


static void timesub(struct timeval *d,
	     struct timeval *s1, struct timeval *s2)
{
  // d = s1-s2

  d->tv_sec = s1->tv_sec - s2->tv_sec;
  d->tv_usec = s1->tv_usec - s2->tv_usec;
  
  if(d->tv_usec >= 1000000) {
    d->tv_sec += 1;
    d->tv_usec -= 1000000;
  } else if(d->tv_usec <= -1000000) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  }

  if((d->tv_sec > 0) && (d->tv_usec < 0)) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  } else if((d->tv_sec < 0) && (d->tv_usec > 0)) {
    d->tv_sec += 1;
    d->tv_usec -= 1000000;
  }

}  

static void timeadd(struct timeval *d,
	     struct timeval *s1, struct timeval *s2)
{
  // d = s1+s2
  
  d->tv_sec = s1->tv_sec + s2->tv_sec;
  d->tv_usec = s1->tv_usec + s2->tv_usec;
  if(d->tv_usec >= 1000000) {
    d->tv_usec -=1000000;
    d->tv_sec +=1;
  } else if(d->tv_usec <= -1000000) {
    d->tv_usec +=1000000;
    d->tv_sec -=1;
  }

  if((d->tv_sec > 0) && (d->tv_usec < 0)) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  } else if((d->tv_sec < 0) && (d->tv_usec > 0)) {
    d->tv_sec += 1;
    d->tv_usec -= 1000000;
  }
}  

static int timecompare(struct timeval *s1, struct timeval *s2) {

  if(s1->tv_sec > s2->tv_sec) {
    return 1;
  } else if(s1->tv_sec < s2->tv_sec) {
    return -1;
  }

  if(s1->tv_usec > s2->tv_usec) {
    return 1;
  } else if(s1->tv_usec < s2->tv_usec) {
    return -1;
  }
  
  return 0;
}

#endif


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
  exit_program(-1);
  return VLC_FAIL;
}


void sighandler(void)
{
  exit_program(0);
}


void init_program()
{
  struct sigaction sig;
  
  // Setup signal handler.
  // SIGINT == ctrl-c
  sig.sa_handler = sighandler;
  sigaction(SIGINT, &sig, NULL);
  

#ifdef TIMESTAT
  timestat_init();
#endif
  
  // init values for MPEG-1
  pic.coding_ext.picture_structure = PIC_STRUCT_FRAME_PICTURE;
  pic.coding_ext.frame_pred_frame_dct = 1;
  pic.coding_ext.intra_vlc_format = 0;
  pic.coding_ext.concealment_motion_vectors = 0;
  //mb.modes.frame_motion_type = 0x2; // This implies the ones below..
  //mb.prediction_type = PRED_TYPE_FRAME_BASED;
  //mb.motion_vector_count = 1;
  //mb.mv_format = MV_FORMAT_FRAME;
  //mb.dmv = 0;
  mb.motion_vector_count = 1;
  seq.ext.chroma_format = 0x1;

}

int msgqid = -1;

//char *infilename = NULL;


int main(int argc, char **argv)
{
  int c;

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
  
  init_out_q(nr_of_buffers);
  
  if(msgqid == -1) {
    if(optind < argc) {
      infilename = argv[optind];
      infile = fopen(argv[optind], "r");
    } else {
      infile = stdin;
    }
  }
  
#ifdef GETBITSMMAP
  get_next_packet(); // Realy, open and mmap the FILE
  packet.offset = 0;
  packet.length = 0;  
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
    DPRINTF(0, "Looking for new Video Sequence\n");
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
  DPRINTF(1, "Resyncing\n");
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
  
  DPRINTF(0, "Found Sequence Header\n");

  sequence_header();

  if(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
    /* MPEG-2 always have a extension code right after the sequence header. */
    MPEG2 = 1;
    
    sequence_extension();

    /* Display init */
    if(!shm_ready) {
      setup_shm(seq.mb_width * 16, seq.mb_height * 16, nr_of_buffers);
      shm_ready = 1;
      
      switch(fork()) {
      case 0:
	display_process();
	display_exit();
	exit(0);
	break;
      case -1:
	perror("fork");
	break;
      default:
	break;
      }
    }
      
    do {
      extension_and_user_data(0);
      do {
	if(nextbits(32) == MPEG2_VS_GROUP_START_CODE) {
	  group_of_pictures_header();
	  extension_and_user_data(1);
	}
	picture_header();
	picture_coding_extension();
	
	extension_and_user_data(2);
	
	picture_data();
	
      } while((nextbits(32) == MPEG2_VS_PICTURE_START_CODE) ||
	      (nextbits(32) == MPEG2_VS_GROUP_START_CODE));
      
      if(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE) {
	if(nextbits(32) != MPEG2_VS_SEQUENCE_HEADER_CODE) {
	  DPRINTF(0, "*** not a sequence header or end code\n");
	  break;
	}
	
	sequence_header();
	sequence_extension();
      }
    } while(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE);
    
  } else {
    /* ERROR: This is an ISO/IEC 11172-2 Stream */
    
    /* No extension code following the sequence header implies MPEG-1 */
    MPEG2 = 0;
    
    /* Display init */
    if(!shm_ready) {
      setup_shm(seq.mb_width * 16, seq.mb_height * 16, nr_of_buffers);
      shm_ready = 1;
      
      switch(fork()) {
      case 0:
	display_process();
	display_exit();
	exit(0);
	break;
      case -1:
	perror("fork");
	break;
      default:
	break;
      }
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
    
  
  /* If we are exiting there should be a sequence end code following. */
  if(nextbits(32) == MPEG2_VS_SEQUENCE_END_CODE) {
    GETBITS(32, "Sequence End Code");
    DPRINTF(0, "Found Sequence End\n");
  } else {
    DPRINTF(0, "*** Didn't find Sequence End\n");
  }

}
  

/* 6.2.2.1 Sequence header */
void sequence_header(void)
{
  uint32_t sequence_header_code;
#ifdef HAVE_CLOCK_GETTIME
  long int frame_interval_nsec;
#else
    long int frame_interval_usec;
#endif

  DPRINTF(0, "sequence_header\n");
  
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


  
  DPRINTF(1, "vertical: %u\n", seq.header.horizontal_size_value);
  DPRINTF(1, "horisontal: %u\n", seq.header.vertical_size_value);

  DPRINTF(1, "frame rate: ");
  switch(seq.header.frame_rate_code) {
  case 0x0:
    DPRINTF(1, "forbidden\n");
    break;
  case 0x1:
    DPRINTF(1, "24000/1001 (23.976)\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 41708333;
#else
    frame_interval_usec = 41708;
#endif
    break;
  case 0x2:
    DPRINTF(1, "24\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 41666667;
#else
    frame_interval_usec = 41667;
#endif
    break;
  case 0x3:
    DPRINTF(1, "25\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 40000000;
#else
    frame_interval_usec = 40000;
#endif
    break;
  case 0x4:
    DPRINTF(1, "30000/1001 (29.97)\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 33366667;
#else
    frame_interval_usec = 33367;
#endif
    break;
  case 0x5:
    DPRINTF(1, "30\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 33333333;
#else
    frame_interval_usec = 33333;
#endif
    break;
  case 0x6:
    DPRINTF(1, "50\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 20000000;
#else
    frame_interval_usec = 20000;
#endif
    break;
  case 0x7:
    DPRINTF(1, "60000/1001 (59.94)\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 16683333;
#else
    frame_interval_usec = 16683;
#endif
    break;
  case 0x8:
    DPRINTF(1, "60\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 16666667;
#else
    frame_interval_usec = 16667;
#endif
    break;
  default:
    DPRINTF(1, "Reserved\n");
    break;
  }
  
  
  if(forced_frame_rate == -1) { /* No forced frame rate */
#ifdef HAVE_CLOCK_GETTIME
    buf_ctrl_head->frame_interval = frame_interval_nsec;
#else
    buf_ctrl_head->frame_interval = frame_interval_usec;
#endif
  } else {
    if(forced_frame_rate == 0) {
      buf_ctrl_head->frame_interval = 1;
    } else {
#ifdef HAVE_CLOCK_GETTIME
      buf_ctrl_head->frame_interval = 1000000000/forced_frame_rate;
#else
      buf_ctrl_head->frame_interval = 1000000/forced_frame_rate;
#endif
    }
  }

  seq.horizontal_size = seq.header.horizontal_size_value;
  seq.vertical_size = seq.header.vertical_size_value;
  
  seq.mb_width  = (seq.horizontal_size+15)/16;
  seq.mb_height = (seq.vertical_size+15)/16;
}


#define INC_8b_ALIGNMENT(a) ((a) + (a)%8)

void init_out_q(int nr_of_bufs)
{ 
  int buf_ctrl_size  = (sizeof(buf_ctrl_head_t) + 
			nr_of_bufs * sizeof(picture_info_t) +
			nr_of_bufs * sizeof(int));

  /* Get shared memory and identifiers. */
  buf_ctrl_shmid = shmget(IPC_PRIVATE, buf_ctrl_size, IPC_CREAT | 0600);
  if( buf_ctrl_shmid == -1) {
    perror("shmget buf_ctrl");
    exit_program(1);
  }

  /* Attach the shared memory to the process. */
  buf_ctrl_shmaddr = shmat(buf_ctrl_shmid, NULL, SHM_SHARE_MMU);
  if(buf_ctrl_shmaddr == (char *) -1) {
    perror("shmat buf_ctrl");
    exit_program(1);
  }

  // Init the buf_ctrl data
  buf_ctrl_head = (buf_ctrl_head_t *)buf_ctrl_shmaddr;

  // Number of pictures
  buf_ctrl_head->nr_of_buffers = nr_of_bufs;
  
  // Set up the pointer to the picture_info array
  buf_ctrl_head->picture_infos = (picture_info_t *)(((char *)buf_ctrl_head) +
						    sizeof(buf_ctrl_head_t));
  
  // Setup the pointer to the dpy_q
  buf_ctrl_head->dpy_q = (int *)(((char *)buf_ctrl_head) +
				 sizeof(buf_ctrl_head_t) +
				 nr_of_bufs * sizeof(picture_info_t));
  
#if defined USE_POSIX_SEM

  // Set the semaphores to the correct starting values
  if(sem_init(&(buf_ctrl_head->pictures_ready_to_display), 1, 0) == -1){
    perror("sem_init ready_to_display");
  }
  if(sem_init(&(buf_ctrl_head->pictures_displayed), 1, 0) == -1) {
    perror("sem_init displayed");
  }

#elif defined USE_SYSV_SEM

  if((buf_ctrl_head->semid_pics =
      semget(IPC_PRIVATE, 2, (IPC_CREAT | IPC_EXCL | 0700))) == -1) {
    perror("init_out_q(), semget(semid_pics)");
  } else {
    union semun arg;
    
    arg.val = 0;
    if(semctl(buf_ctrl_head->semid_pics, PICTURES_READY_TO_DISPLAY, SETVAL, arg) == -1) {
      perror("semctl() semid_pics");
    }
    
    arg.val = 0;
    if(semctl(buf_ctrl_head->semid_pics, PICTURES_DISPLAYED, SETVAL, arg) == -1) {
      perror("semctl() semid_pics");
    }
  }
  
  
#else
#error No semaphore type set
#endif
  
}



#define INC_8b_ALIGNMENT(a) ((a) + (a)%8)

void setup_shm(int padded_width, int padded_height, int nr_of_bufs)
{ 
  int num_pels = padded_width * padded_height;
  int pagesize = sysconf(_SC_PAGESIZE);
  int y_size   = INC_8b_ALIGNMENT(num_pels);
  int uv_size  = INC_8b_ALIGNMENT(num_pels/4);
  int yuv_size = y_size + 2 * uv_size; 
  
  int picture_buffer_size = (yuv_size + yuv_size % pagesize);
  /* Mlib reads ?8? bytes beyond the last pel (in the v-picture), 
     if that pel just before a page boundary then *boom*!! */
  int picture_buffers_size = nr_of_bufs * picture_buffer_size + pagesize;//Hack
  
  int i;
  char *baseaddr;

  // Get shared memory and identifiers
  picture_buffers_shmid
    = shmget(IPC_PRIVATE, picture_buffers_size, IPC_CREAT | 0600);
  if (picture_buffers_shmid == -1) {
    perror("shmget picture_buffers");
    exit_program(1);
  }

  // Attach the shared memory to the process
  picture_buffers_shmaddr = shmat(picture_buffers_shmid, NULL, SHM_SHARE_MMU);
  if(picture_buffers_shmaddr == (char *) -1) {
    perror("shmat picture_buffers");
    exit_program(1);
  }

  // Setup the pointers to the pictures in the buffer
  baseaddr = picture_buffers_shmaddr;

  for(i=0 ; i< nr_of_bufs ; i++) {
    
    buf_ctrl_head->picture_infos[i].picture.y = baseaddr;
    buf_ctrl_head->picture_infos[i].picture.u = baseaddr + y_size;
    buf_ctrl_head->picture_infos[i].picture.v = baseaddr + y_size + uv_size;
    buf_ctrl_head->picture_infos[i].picture.horizontal_size = seq.horizontal_size;
    buf_ctrl_head->picture_infos[i].picture.vertical_size = seq.vertical_size;
    buf_ctrl_head->picture_infos[i].picture.start_x = 0;
    buf_ctrl_head->picture_infos[i].picture.start_y = 0;
    buf_ctrl_head->picture_infos[i].picture.padded_width = padded_width;
    buf_ctrl_head->picture_infos[i].picture.padded_height = padded_height;
    buf_ctrl_head->picture_infos[i].is_ref = 0;
    buf_ctrl_head->picture_infos[i].displayed = 1;
    
    baseaddr += picture_buffer_size;
  }

  
  fwd_ref_image = &buf_ctrl_head->picture_infos[0].picture;
  bwd_ref_image = &buf_ctrl_head->picture_infos[1].picture;
  
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
#ifdef HAVE_CLOCK_GETTIME
  long int frame_interval_nsec;
#else
  long int frame_interval_usec;
#endif
  uint32_t extension_start_code;
  
  extension_start_code = GETBITS(32, "extension_start_code");
  if(extension_start_code != MPEG2_VS_EXTENSION_START_CODE) {
    fprintf(stderr, "wrong start_code extension_start_code: %08x\n",
	    extension_start_code);
  }
  
  seq.ext.extension_start_code_identifier 
    = GETBITS(4, "extension_start_code_identifier");
  seq.ext.profile_and_level_indication 
    = GETBITS(8, "profile_and_level_indication");

#ifdef DEBUG
  DPRINTF(2, "profile_and_level_indication: ");
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
 
#endif
      
  seq.ext.progressive_sequence = GETBITS(1, "progressive_sequence");

  DPRINTF(1, "progressive seq: %01x\n", seq.ext.progressive_sequence);

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


  DPRINTF(1, "bit rate: %d bits/s\n",
	  400 * ((seq.ext.bit_rate_extension << 12) 
		 | seq.header.bit_rate_value));


  DPRINTF(1, "frame rate: ");
  switch(seq.header.frame_rate_code) {
  case 0x0:
    DPRINTF(1, "forbidden\n");
    break;
  case 0x1:
    DPRINTF(1, "24000/1001 (23.976)\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 41708333;
#else
    frame_interval_usec = 41708;
#endif
    break;
  case 0x2:
    DPRINTF(1, "24\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 41666667;
#else
    frame_interval_usec = 41667;
#endif
    break;
  case 0x3:
    DPRINTF(1, "25\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 40000000;
#else
    frame_interval_usec = 40000;
#endif
    break;
  case 0x4:
    DPRINTF(1, "30000/1001 (29.97)\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 33366667;
#else
    frame_interval_usec = 33367;
#endif
    break;
  case 0x5:
    DPRINTF(1, "30\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 33333333;
#else
    frame_interval_usec = 33333;
#endif
    break;
  case 0x6:
    DPRINTF(1, "50\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 20000000;
#else
    frame_interval_usec = 20000;
#endif
    break;
  case 0x7:
    DPRINTF(1, "60000/1001 (59.94)\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 16683333;
#else
    frame_interval_usec = 16683;
#endif
    break;
  case 0x8:
    DPRINTF(1, "60\n");
#ifdef HAVE_CLOCK_GETTIME
    frame_interval_nsec = 16666667;
#else
    frame_interval_usec = 16667;
#endif
    break;
  default:
    DPRINTF(1, "%f (computed)\n",
	    (double)(seq.header.frame_rate_code)*
	    ((double)(seq.ext.frame_rate_extension_n)+1.0)/
	    ((double)(seq.ext.frame_rate_extension_d)+1.0));
    //TODO: frame_interval_nsec = ?
    break;
  }

  if(forced_frame_rate == -1) { /* No forced frame rate */
#ifdef HAVE_CLOCK_GETTIME
    buf_ctrl_head->frame_interval = frame_interval_nsec;
#else
    buf_ctrl_head->frame_interval = frame_interval_usec;
#endif
  } else {
    if(forced_frame_rate == 0) {
      buf_ctrl_head->frame_interval = 1;
    } else {
#ifdef HAVE_CLOCK_GETTIME
      buf_ctrl_head->frame_interval = 1000000000/forced_frame_rate;
#else
      buf_ctrl_head->frame_interval = 1000000/forced_frame_rate;
#endif
    }
  }
}


/* 6.2.2.2 Extension and user data */
void extension_and_user_data(unsigned int i) {
  
  DPRINTF(3, "extension_and_user_data(%u)\n", i);

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
}


/* 6.2.3.1 Picture coding extension */
void picture_coding_extension(void)
{
  uint32_t extension_start_code;

  DPRINTF(3, "picture_coding_extension\n");

  extension_start_code = GETBITS(32, "extension_start_code");
  pic.coding_ext.extension_start_code_identifier 
    = GETBITS(4, "extension_start_code_identifier");

  pic.coding_ext.f_code[0][0] = GETBITS(4, "f_code[0][0]");
  pic.coding_ext.f_code[0][1] = GETBITS(4, "f_code[0][1]");
  pic.coding_ext.f_code[1][0] = GETBITS(4, "f_code[1][0]");
  pic.coding_ext.f_code[1][1] = GETBITS(4, "f_code[1][1]");
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
  DPRINTF(1, "picture_structure: ");
  switch(pic.coding_ext.picture_structure) {
  case PIC_STRUCT_RESERVED:
    DPRINTF(1, "reserved");
    break;
  case PIC_STRUCT_TOP_FIELD:
    DPRINTF(1, "Top Field");
    break;
  case PIC_STRUCT_BOTTOM_FIELD:
    DPRINTF(1, "Bottom Field");
    break;
  case PIC_STRUCT_FRAME_PICTURE:
    DPRINTF(1, "Frame Picture");
    break;
  }
  DPRINTF(1, "\n");
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
  
  DPRINTF(1, "top_field_first: %01x\n", pic.coding_ext.top_field_first);
  DPRINTF(1, "frame_pred_frame_dct: %01x\n", pic.coding_ext.frame_pred_frame_dct);
  DPRINTF(1, "intra_vlc_format: %01x\n", pic.coding_ext.intra_vlc_format);
  DPRINTF(1, "alternate_scan: %01x\n", pic.coding_ext.alternate_scan);
  DPRINTF(1, "repeat_first_field: %01x\n", pic.coding_ext.repeat_first_field);
  DPRINTF(1, "progressive_frame: %01x\n", pic.coding_ext.progressive_frame);
  DPRINTF(1, "composite_display_flag: %01x\n", pic.coding_ext.composite_display_flag);
  
  if(pic.coding_ext.composite_display_flag) {
    pic.coding_ext.v_axis = GETBITS(1, "v_axis");
    pic.coding_ext.field_sequence = GETBITS(3, "field_sequence");
    pic.coding_ext.sub_carrier = GETBITS(1, "sub_carrier");
    pic.coding_ext.burst_amplitude = GETBITS(7, "burst_amplitude");
    pic.coding_ext.sub_carrier_phase = GETBITS(8, "sub_carrier_phase");
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
  
  next_start_code();
}


/* 6.2.2.2.2 User data */
void user_data(void)
{
  uint32_t user_data_start_code;
  
  DPRINTF(3, "user_data\n");

  user_data_start_code = GETBITS(32, "user_date_start_code");
  while(nextbits(24) != 0x000001) {
    GETBITS(8, "user_data");
  }
  
  next_start_code();  
}


/* 6.2.2.6 Group of pictures header */
void group_of_pictures_header(void)
{
  uint32_t group_start_code;
  uint32_t time_code;
  uint8_t closed_gop;
  uint8_t broken_link;
  
  DPRINTF(3, "group_of_pictures_header\n");

  group_start_code = GETBITS(32, "group_start_code");

  if(group_start_code != MPEG2_VS_GROUP_START_CODE) {
    fprintf(stderr, "wrong start_code group_start_code: %08x\n",
	    group_start_code);
  }

  time_code = GETBITS(25, "time_code");

  /* Table 6-11 --- time_code */
  DPRINTF(2, "time_code: %02d:%02d.%02d'%02d\n",
	  (time_code & 0x00f80000)>>19,
	  (time_code & 0x0007e000)>>13,
	  (time_code & 0x00000fc0)>>6,
	  (time_code & 0x0000003f));
  
  /* These need to be in seq or some thing like that */
  closed_gop = GETBITS(1, "closed_gop");
  broken_link = GETBITS(1, "broken_link");

  last_temporal_ref_to_dpy = -1;
  
  next_start_code();
}


/* 6.2.3 Picture header */
void picture_header(void)
{
  uint32_t picture_start_code;

  DPRINTF(3, "picture_header\n");

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
    last_pts = PTS;
    prev_scr_nr = last_scr_nr;
    last_scr_nr = scr_nr;
    picture_has_pts = 1;
  } else {
    picture_has_pts = 0;
  }
  
  if(PTS_DTS_flags & 0x02) {
    if(last_scr_nr != prev_scr_nr) {   
      fprintf(stderr, "=== last_scr_nr: %d, prev_scr_nr: %d\n",
	      last_scr_nr, prev_scr_nr);
#ifdef HAVE_CLOCK_GETTIME

      fprintf(stderr, "--- last_scr: %ld.%09ld, prev_scr: %ld.%09ld\n",
	      ctrl_time[last_scr_nr].realtime_offset.tv_sec,
	      ctrl_time[last_scr_nr].realtime_offset.tv_nsec,
	      ctrl_time[prev_scr_nr].realtime_offset.tv_sec,
	      ctrl_time[prev_scr_nr].realtime_offset.tv_nsec);
#else

      fprintf(stderr, "--- last_scr: %ld.%06ld, prev_scr: %ld.%06ld\n",
	      ctrl_time[last_scr_nr].realtime_offset.tv_sec,
	      ctrl_time[last_scr_nr].realtime_offset.tv_usec,
	      ctrl_time[prev_scr_nr].realtime_offset.tv_sec,
	      ctrl_time[prev_scr_nr].realtime_offset.tv_usec);

#endif

      fprintf(stderr, "+++ last_pts: %lld\n", last_pts);


    }
  }

  //  print_time_offset(PTS);

  pic.header.temporal_reference = GETBITS(10, "temporal_reference");
  pic.header.picture_coding_type = GETBITS(3, "picture_coding_type");

#ifdef DEBUG
  /* Table 6-12 --- picture_coding_type */
  DPRINTF(1, "picture coding type: %01x, ", pic.header.picture_coding_type);

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
    pic.coding_ext.f_code[0][0] = pic.header.forward_f_code;
    pic.coding_ext.f_code[0][1] = pic.header.forward_f_code;
  }
  
  if(pic.header.picture_coding_type == PIC_CODING_TYPE_B) {
    pic.header.full_pel_vector[1] = GETBITS(1, "full_pel_backward_vector");
    pic.header.backward_f_code = GETBITS(3, "backward_f_code");
    pic.coding_ext.f_code[1][0] = pic.header.backward_f_code;
    pic.coding_ext.f_code[1][1] = pic.header.backward_f_code;
  }
  
  while(nextbits(1) == 1) {
    GETBITS(1, "extra_bit_picture");
    GETBITS(8, "extra_information_picture");
  }
  GETBITS(1, "extra_bit_picture");
  
  next_start_code();
}


// Get id of empty buffer
int get_picture_buf()
{
  static int dpy_q_displayed_pos = 0;
  int n;
  int id;
  
  //  search for empty buffer
  for(n = 0; n < buf_ctrl_head->nr_of_buffers; n++) {
    /*    fprintf(stderr, "decode: BUF[%d]: is_ref:%d, displayed: %d\n",
	    n,
	    buf_ctrl_head->picture_infos[n].is_ref,
	    buf_ctrl_head->picture_infos[n].displayed);
    */
    if((buf_ctrl_head->picture_infos[n].is_ref == 0) &&
       (buf_ctrl_head->picture_infos[n].displayed == 1)) {
      buf_ctrl_head->picture_infos[n].displayed = 0;
      return n;
    }
  }

  // didn't find empty buffer, wait for a picture to display
#if defined USE_POSIX_SEM
  if(sem_wait(&(buf_ctrl_head->pictures_displayed)) == -1) {
    perror("sem_wait get_picture_buf");
  }
#elif USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = PICTURES_DISPLAYED;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    
    if(semop(buf_ctrl_head->semid_pics, &sops, 1) == -1) {
      perror("video_decode: semop() wait");
    }
  }

#else
#error No semaphore type set
#endif
  id = buf_ctrl_head->dpy_q[dpy_q_displayed_pos];
  buf_ctrl_head->picture_infos[id].displayed = 1;
  dpy_q_displayed_pos = (dpy_q_displayed_pos+1) % buf_ctrl_head->nr_of_buffers; 

  for(n = 0; n < buf_ctrl_head->nr_of_buffers; n++) {
    /*    fprintf(stderr, "decode: buf[%d]: is_ref:%d, displayed: %d\n",
	    n,
	    buf_ctrl_head->picture_infos[n].is_ref,
	    buf_ctrl_head->picture_infos[n].displayed);
    */
    if((buf_ctrl_head->picture_infos[n].is_ref == 0) &&
       (buf_ctrl_head->picture_infos[n].displayed == 1)) {
      buf_ctrl_head->picture_infos[n].displayed = 0;
      return n;
    }
  }

  //  fprintf(stderr, "decode: *** get_picture_buf\n");
#if defined USE_POSIX_SEM
  if(sem_wait(&(buf_ctrl_head->pictures_displayed)) == -1) {
    perror("sem_wait get_picture_buf");
  }
#elif USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = PICTURES_DISPLAYED;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    
    if(semop(buf_ctrl_head->semid_pics, &sops, 1) == -1) {
      perror("video_decode: semop() wait");
    }
  }

#else
#error No semaphore type set
#endif
  id = buf_ctrl_head->dpy_q[dpy_q_displayed_pos];
  buf_ctrl_head->picture_infos[id].displayed = 1;
  dpy_q_displayed_pos = (dpy_q_displayed_pos+1) % buf_ctrl_head->nr_of_buffers; 

  for(n = 0; n < buf_ctrl_head->nr_of_buffers; n++) {
    /*    fprintf(stderr, "decode: buf[%d]: is_ref:%d, displayed: %d\n",
	    n,
	    buf_ctrl_head->picture_infos[n].is_ref,
	    buf_ctrl_head->picture_infos[n].displayed);
    */
    if((buf_ctrl_head->picture_infos[n].is_ref == 0) &&
       (buf_ctrl_head->picture_infos[n].displayed == 1)) {
      buf_ctrl_head->picture_infos[n].displayed = 0;
      // fprintf(stderr, " buf_id %d found empty\n", n);
      return n;
    }
  }
  
  fprintf(stderr, "decode: error get_picture_buf\n");
  return -1;
}

// Put decoded picture into display queue
void dpy_q_put(int id)
{
  static int dpy_q_put_pos = 0;
  buf_ctrl_head->dpy_q[dpy_q_put_pos] = id;
  //fprintf(stderr, "decode: dpy_q[%d]=%d\n", dpy_q_put_pos, id);
  dpy_q_put_pos = (dpy_q_put_pos + 1) % (buf_ctrl_head->nr_of_buffers);
  //fprintf(stderr, "decode: sem_post\n");

  if(forced_frame_rate != -1) {
    if(forced_frame_rate == 0) {
      buf_ctrl_head->frame_interval = 1;
    } else {
#ifdef HAVE_CLOCK_GETTIME
      buf_ctrl_head->frame_interval = 1000000000/forced_frame_rate;
#else
      buf_ctrl_head->frame_interval = 1000000/forced_frame_rate;
#endif
    }
  }
#if defined USE_POSIX_SEM
  if(sem_post(&(buf_ctrl_head->pictures_ready_to_display)) != 0) {
    perror("sempost pictures_ready");
  }
#elif defined USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = PICTURES_READY_TO_DISPLAY;
    sops.sem_op =  1;
    sops.sem_flg = 0;

    if(semop(buf_ctrl_head->semid_pics, &sops, 1) == -1) {
      perror("video_decode: semop() post");
    }
  }
  
#else
#error No semaphore type set
#endif
}

#define FPS_FRAMES 480
/* 6.2.3.6 Picture data */
void picture_data(void)
{
  static int buf_id;
  static int fwd_ref_buf_id = -1;
  static int bwd_ref_buf_id  = -1;
  uint64_t calc_pts;
  int err;
  picture_info_t *pinfos;
  static int bepa = FPS_FRAMES;
  static int bwd_ref_temporal_reference = -1;
  static int prev_coded_temp_ref = -2;
  static int last_timestamped_temp_ref = -1;
  static int drop_frame = 0;
  pinfos = buf_ctrl_head->picture_infos;

  //fprintf(stderr, ".");
  
#ifdef HAVE_CLOCK_GETTIME

  if(bepa >= FPS_FRAMES) {
    static struct timespec qot;
    struct timespec qtt;
    struct timespec qpt;
    
    bepa = 0; 
    
    clock_gettime(CLOCK_REALTIME, &qtt);
    // d = s1-s2
    qpt.tv_sec = qtt.tv_sec - qot.tv_sec;
    qpt.tv_nsec = qtt.tv_nsec - qot.tv_nsec;
    
    if(qpt.tv_nsec >= 1000000000) {
      qpt.tv_sec += 1;
      qpt.tv_nsec -= 1000000000;
    } else if(qpt.tv_nsec <= -1000000000) {
      qpt.tv_sec -= 1;
      qpt.tv_nsec += 1000000000;
    }
    
    if((qpt.tv_sec > 0) && (qpt.tv_nsec < 0)) {
      qpt.tv_sec -= 1;
      qpt.tv_nsec += 1000000000;
    } else if((qpt.tv_sec < 0) && (qpt.tv_nsec > 0)) {
      qpt.tv_sec += 1;
    qpt.tv_nsec -= 1000000000;
    }
    //    timesub(&qpt, &qtt, &qot);
    
    qot = qtt;

    fprintf(stderr, "decode fps: %.3f\n",
	    FPS_FRAMES/((double)qpt.tv_sec+(double)qpt.tv_nsec/1000000000.0));
  }

#else

  if(bepa >= FPS_FRAMES) {
    static struct timeval qot;
    struct timeval qtt;
    struct timeval qpt;
    
    bepa = 0; 
    
    gettimeofday(&qtt, NULL);
    // d = s1-s2
    qpt.tv_sec = qtt.tv_sec - qot.tv_sec;
    qpt.tv_usec = qtt.tv_usec - qot.tv_usec;
    
    if(qpt.tv_usec >= 1000000) {
      qpt.tv_sec += 1;
      qpt.tv_usec -= 1000000;
    } else if(qpt.tv_usec <= -1000000) {
      qpt.tv_sec -= 1;
      qpt.tv_usec += 1000000;
    }
    
    if((qpt.tv_sec > 0) && (qpt.tv_usec < 0)) {
      qpt.tv_sec -= 1;
      qpt.tv_usec += 1000000;
    } else if((qpt.tv_sec < 0) && (qpt.tv_usec > 0)) {
      qpt.tv_sec += 1;
    qpt.tv_usec -= 1000000;
    }
    //    timesub(&qpt, &qtt, &qot);
    
    qot = qtt;

    fprintf(stderr, "decode fps: %.3f\n",
	    FPS_FRAMES/((double)qpt.tv_sec+(double)qpt.tv_usec/1000000.0));
  }

#endif
  
  bepa++;


  DPRINTF(3, "picture_data\n");
  
  if(msgqid != -1) {
    chk_for_msg();
  }
  
  
  if(prev_coded_temp_ref != pic.header.temporal_reference) {
    
    /* If this is a I/P picture then we must release the reference 
       frame that is going to be replaced. (It might not have been 
       displayed yet so it is not necessarily free for reuse.) */
    
    switch(pic.header.picture_coding_type) {
    case PIC_CODING_TYPE_I:
    case PIC_CODING_TYPE_P:
      if(fwd_ref_buf_id != -1) {
	/* current fwd_ref_image is not used as reference any more */
	pinfos[fwd_ref_buf_id].is_ref = 0;
      }
      
      /* get new buffer */
      buf_id = get_picture_buf();
      dst_image = &(pinfos[buf_id].picture);
      
      /* Age the reference frame */
      fwd_ref_buf_id = bwd_ref_buf_id; 
      fwd_ref_image = bwd_ref_image; 
      
      /* and add the new (to be decoded) frame */
      bwd_ref_image = dst_image;
      bwd_ref_buf_id = buf_id;
      
      bwd_ref_temporal_reference = pic.header.temporal_reference;
      
      /* this buffer is used as reference picture by the decoder */
      pinfos[buf_id].is_ref = 1; 
      
      break;
    case PIC_CODING_TYPE_B:
      
      /* get new buffer */
      buf_id = get_picture_buf();
      dst_image = &(pinfos[buf_id].picture);
      
      break;
    }
  
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
    
    
    
    /* This is just a marker to later know if there is a real time 
       stamp for this picure. */
    pinfos[buf_id].pts_time.tv_sec = -1;
    
    /* If the packet containing the picture header start code had 
       a time stamp, that time stamp used.
       
       Otherwise a time stamp is calculated from the last picture 
       produced for viewing. 
       Iff we predict the time stamp then we must also make sure to use 
       the same scr as the picure we predict from.
    */
    if(picture_has_pts) {
      last_timestamped_temp_ref = pic.header.temporal_reference;
      pinfos[buf_id].PTS = last_pts;
      pinfos[buf_id].pts_time.tv_sec = last_pts/90000;
#ifdef HAVE_CLOCK_GETTIME
      pinfos[buf_id].pts_time.tv_nsec =
	(last_pts%90000)*(1000000000/90000);
#else
      pinfos[buf_id].pts_time.tv_usec =
	(last_pts%90000)*(1000000/90000);
#endif
      pinfos[buf_id].realtime_offset =
	ctrl_time[last_scr_nr].realtime_offset;
      pinfos[buf_id].scr_nr = last_scr_nr;
      /*
      fprintf(stderr, "#%ld.%09ld\n",
	      pinfos[buf_id].pts_time.tv_sec,
	      pinfos[buf_id].pts_time.tv_nsec);
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
	
#ifdef HAVE_CLOCK_GETTIME

	calc_pts = last_pts +
	  (pic.header.temporal_reference - last_timestamped_temp_ref) *
	  90000/(1000000000/buf_ctrl_head->frame_interval);
	/*
	  calc_pts = last_pts_to_dpy +
	  90000/(1000000000/buf_ctrl_head->frame_interval);
	*/
	pinfos[buf_id].PTS = calc_pts;
	pinfos[buf_id].pts_time.tv_sec = calc_pts/90000;
	pinfos[buf_id].pts_time.tv_nsec =
	  (calc_pts%90000)*(1000000000/90000);
	pinfos[buf_id].realtime_offset =
	  ctrl_time[last_scr_nr].realtime_offset;
	pinfos[buf_id].scr_nr = last_scr_nr;
#else

	calc_pts = last_pts +
	  (pic.header.temporal_reference - last_timestamped_temp_ref) *
	  90000/(1000000/buf_ctrl_head->frame_interval);
	/*
	  calc_pts = last_pts_to_dpy +
	  90000/(1000000/buf_ctrl_head->frame_interval);
	*/
	pinfos[buf_id].PTS = calc_pts;
	pinfos[buf_id].pts_time.tv_sec = calc_pts/90000;
	pinfos[buf_id].pts_time.tv_usec =
	  (calc_pts%90000)*(1000000/90000);
	pinfos[buf_id].realtime_offset =
	  ctrl_time[last_scr_nr].realtime_offset;
	pinfos[buf_id].scr_nr = last_scr_nr;
#endif	
	
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

#ifdef HAVE_CLOCK_GETTIME

	calc_pts = last_pts +
	  (pic.header.temporal_reference - last_timestamped_temp_ref) *
	  90000/(1000000000/buf_ctrl_head->frame_interval);
	/*
	  calc_pts = last_pts_to_dpy + 
	  90000/(1000000000/buf_ctrl_head->frame_interval);
	*/
	buf_ctrl_head->picture_infos[buf_id].PTS = calc_pts;
	buf_ctrl_head->picture_infos[buf_id].pts_time.tv_sec = calc_pts/90000;
	buf_ctrl_head->picture_infos[buf_id].pts_time.tv_nsec =
	  (calc_pts%90000)*(1000000000/90000);
	pinfos[buf_id].realtime_offset =
	  ctrl_time[last_scr_nr].realtime_offset;
	pinfos[buf_id].scr_nr = last_scr_nr;

#else

	calc_pts = last_pts +
	  (pic.header.temporal_reference - last_timestamped_temp_ref) *
	  90000/(1000000/buf_ctrl_head->frame_interval);
	/*
	  calc_pts = last_pts_to_dpy + 
	  90000/(1000000/buf_ctrl_head->frame_interval);
	*/
	buf_ctrl_head->picture_infos[buf_id].PTS = calc_pts;
	buf_ctrl_head->picture_infos[buf_id].pts_time.tv_sec = calc_pts/90000;
	buf_ctrl_head->picture_infos[buf_id].pts_time.tv_usec =
	  (calc_pts%90000)*(1000000/90000);
	pinfos[buf_id].realtime_offset =
	  ctrl_time[last_scr_nr].realtime_offset;
	pinfos[buf_id].scr_nr = last_scr_nr;

#endif	
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

#ifdef HAVE_CLOCK_GETTIME
      
      struct timespec realtime, calc_rt, err_time;
      
      clock_gettime(CLOCK_REALTIME, &realtime);
      
      timeadd(&calc_rt,
	      &(pinfos[buf_id].pts_time),
	      &(pinfos[buf_id].realtime_offset));
      timesub(&err_time, &calc_rt, &realtime);
	
      /* If the picture should already have been displayed then drop it. */
      /* TODO: More investigation needed. */
      if(((err_time.tv_nsec < 0) || (err_time.tv_sec < 0)) && (forced_frame_rate != 0)) {
	fprintf(stderr, "!");
	  
	/*
	fprintf(stderr, "errpts %ld.%+010ld\n\n",
		err_time.tv_sec,
		err_time.tv_nsec);
	*/

#else

      struct timeval realtime, calc_rt, err_time;
      
      gettimeofday(&realtime, NULL);
      
      timeadd(&calc_rt,
	      &(pinfos[buf_id].pts_time),
	      &(pinfos[buf_id].realtime_offset));
      timesub(&err_time, &calc_rt, &realtime);
	
      /* If the picture should already have been displayed then drop it. */
      /* TODO: More investigation needed. */
      if(((err_time.tv_usec < 0) || (err_time.tv_sec < 0)) && (forced_frame_rate != 0)) {
	fprintf(stderr, "!");
	  
	/*
	fprintf(stderr, "errpts %ld.%+06ld\n\n",
		err_time.tv_sec,
		err_time.tv_usec);
	*/

#endif	  
	/* mark the frame to be dropped */
	drop_frame = 1;
	  
      }
    }
  }
  
  /*
 else {
 // either this is the second field of the frame or it is an error
    fprintf(stderr, "*** error prev_temp_ref == cur_temp_ref\n");
    
  }
  */
  /* now we can decode the picture if it shouldn't be dropped */
  if(!drop_frame) {
    /* Decode the slices. */
    if( MPEG2 )
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
    else {
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
	
	if(drop_frame) {
	  drop_frame = 0;
	  pinfos[buf_id].is_ref = 0; // this is never set in a B picture ?
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
      }
    }
    
    if((bwd_ref_temporal_reference != -1) && 
       (bwd_ref_temporal_reference == (last_temporal_ref_to_dpy+1)%1024)) {

      if((pinfos[bwd_ref_buf_id].pts_time.tv_sec == -1)) {
	fprintf(stderr, "***** NO pts set\n");
      }
      last_temporal_ref_to_dpy = bwd_ref_temporal_reference;
      
      /* bwd_ref should not be in the dpy_q more than one time */
      bwd_ref_temporal_reference = -1;
      
      /* put bwd_ref in dpy_q */
      
      last_pts_to_dpy = pinfos[bwd_ref_buf_id].PTS;
      last_scr_nr_to_dpy = pinfos[bwd_ref_buf_id].scr_nr;
      
      dpy_q_put(bwd_ref_buf_id);
      
    } else if(bwd_ref_temporal_reference < (last_temporal_ref_to_dpy+1)%1024) {
      
      fprintf(stderr, "** temporal reference in I or P picture incorrect\n");
      
    }
  }
  
  prev_coded_temp_ref = pic.header.temporal_reference;
  
  
  /* Temporarily broken :) */
#if 0  

  {
    static int first = 1;
    static int frame_nr = 0;
    static int previous_picture_type;
    
    double diff;
    static double old_time = 0.0;

#ifdef HAVE_MMX
    emms();
#endif


    // The time for the lastframe 
    diff = (((double)tv.tv_sec + (double)(tv.tv_usec)/1000000.0)-
	    ((double)otv.tv_sec + (double)(otv.tv_usec)/1000000.0));
    
    
    if(first) {
      first = 0;
    } else {
      switch(previous_picture_type) { 
      case PIC_CODING_TYPE_I:
	time_pic[0] += diff;
	num_pic[0]++;
	if(diff > time_max[0]) {
	  time_max[0] = diff;
	}
	if(diff < time_min[0]) {
	  time_min[0] = diff;
	}
	break;
      case PIC_CODING_TYPE_P:
	time_pic[1] += diff;
	num_pic[1]++;
	if(diff > time_max[1]) {
	  time_max[1] = diff;
	}
	if(diff < time_min[1]) {
	  time_min[1] = diff;
	}
	break;
      case PIC_CODING_TYPE_B:
	time_pic[2] += diff;
	num_pic[2]++;
	if(diff > time_max[2]) {
	  time_max[2] = diff;
	}
	if(diff < time_min[2]) {
	  time_min[2] = diff;
	}
	break;
      }
      time_pic[3] += diff;
      num_pic[3]++;
      if(diff > time_max[3]) {
	time_max[3] = diff;
      }
      if(diff < time_min[3]) {
	time_min[3] = diff;
      }
      
      if(frame_nr == 0) {
	
	frame_nr = 25;
	otva.tv_sec = tva.tv_sec;
	otva.tv_usec = tva.tv_usec;
	gettimeofday(&tva, NULL);
	
	fprintf(stderr, "decode: frame rate: %f fps\n",
		25.0/(((double)tva.tv_sec+
		       (double)(tva.tv_usec)/1000000.0)-
		      ((double)otva.tv_sec+
		       (double)(otva.tv_usec)/1000000.0))
		);
	
      }
      
      frame_nr--;
    }
    
    previous_picture_type = pic.header.picture_coding_type;
    gettimeofday(&otv, NULL); // Start time (for this frame) 
  }
#endif
  

  next_start_code();
}


/* 6.2.2.2.1 Extension data */
void extension_data(unsigned int i)
{
  DPRINTF(3, "extension_data(%d)", i);
  
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
  
  next_start_code();
}


void picture_display_extension()
{
  uint8_t extension_start_code_identifier;
  uint16_t frame_centre_horizontal_offset;
  uint16_t frame_centre_vertical_offset;
  uint8_t number_of_frame_centre_offsets;
  int i;
  
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
  
  DPRINTF(2, "picture_display_extension()\n");
  extension_start_code_identifier 
    = GETBITS(4,"extension_start_code_identifier");

  for(i = 0; i < number_of_frame_centre_offsets; i++) {
    frame_centre_horizontal_offset = GETBITS(16, "frame_centre_horizontal_offset");
    marker_bit();
    frame_centre_vertical_offset = GETBITS(16, "frame_centre_vertical_offset");
    marker_bit();
    
  }
  
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
  uint8_t extension_start_code_identifier;
  uint8_t video_format;
  uint8_t colour_description;
  
  uint8_t colour_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coefficients;
  
  uint16_t display_horizontal_size;
  uint16_t display_vertical_size;
  
  
  DPRINTF(2, "sequence_display_extension()\n");
  extension_start_code_identifier 
    = GETBITS(4,"extension_start_code_identifier");
  video_format = GETBITS(3, "video_format");
  colour_description = GETBITS(1, "colour_description");

#ifdef DEBUG
  /* Table 6-6. Meaning of video_format */
  DPRINTF(2, "video_format: ");
  switch(video_format) {
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


  if(colour_description) {
    colour_primaries = GETBITS(8, "colour_primaries");
    transfer_characteristics = GETBITS(8, "transfer_characteristics");
    matrix_coefficients = GETBITS(8, "matrix_coefficients");
    
#ifdef DEBUG
    /* Table 6-7. Colour Primaries */
    DPRINTF(2, "colour primaries (Table 6-7): ");
    switch(colour_primaries) {
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
    DPRINTF(2, "transfer characteristics (Table 6-8): ");
    switch(transfer_characteristics) {
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
    DPRINTF(2, "matrix coefficients (Table 6-9): ");
    switch(matrix_coefficients) {
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

  display_horizontal_size = GETBITS(14, "display_horizontal_size");
  marker_bit();
  display_vertical_size = GETBITS(14, "display_vertical_size");

  DPRINTF(2, "display_horizontal_size: %d\n", display_horizontal_size);
  DPRINTF(2, "display_vertical_size: %d\n", display_vertical_size);
  
  next_start_code();
}


void exit_program(int exitcode)
{
  // Detach the shared memory segments from this process  
  shmdt(picture_buffers_shmaddr);
  shmdt(buf_ctrl_shmaddr);
  
  shmctl(picture_buffers_shmid, IPC_RMID, 0);
  shmctl(buf_ctrl_shmid, IPC_RMID, 0);

#ifdef HAVE_MMX
  emms();
#endif
  
#if 0
  { /* Print some frame rate info. */
    int n;
    for(n=0; n<4; n++) {
      fprintf(stderr,"frames: %.0f\n", num_pic[n]);
      fprintf(stderr,"max time: %.4f\n", time_max[n]);
      fprintf(stderr,"min time: %.4f\n", time_min[n]);
      fprintf(stderr,"fps: %.4f\n", num_pic[n]/time_pic[n]);
    }
  }
#endif
  
#ifdef TIMESTAT
  timestat_print();
#endif

  fprintf(stderr, "\nCompiled with:\n");
#ifdef DCFLAGS
  fprintf(stderr, "\tCFLAGS = %s\n", DCFLAGS);
#endif
#ifdef DLDFLAGS
  fprintf(stderr, "\tLDFLAGS = %s\n", DLDFLAGS);
#endif

  exit(exitcode);
}












#ifdef TIMESTAT


#define TIMESTAT_NOF 24*60
void timestat_init()
{
  int n;

  for(n = 0; n < 3; n++) {
    
    picture_decode_time_tot[n].tv_sec = 0;
    picture_decode_time_tot[n].tv_nsec = 0;
    
    picture_display_time_tot[n].tv_sec = 0;
    picture_display_time_tot[n].tv_nsec = 0;

    picture_decode_time_min[n].tv_sec = 99;
    picture_decode_time_min[n].tv_nsec = 0;
    
    picture_display_time_min[n].tv_sec = 99;
    picture_display_time_min[n].tv_nsec = 0;
    
    picture_decode_time_max[n].tv_sec = 0;
    picture_decode_time_max[n].tv_nsec = 0;

    picture_display_time_max[n].tv_sec = 0;
    picture_display_time_max[n].tv_nsec = 0;

    picture_decode_nr[n] = 0;
    picture_display_nr[n] = 0;
  }

  picture_timestat = calloc(TIMESTAT_NOF, sizeof(picture_time_t));

  picture_time.tv_sec = 0;
  picture_time.tv_nsec = 0;

  for(n = 0; n < TIMESTAT_NOF; n++) {
    picture_timestat[n].dec_start = picture_time;
    picture_timestat[n].dec_end = picture_time;
    picture_timestat[n].disp_start = picture_time;
    picture_timestat[n].disp_end = picture_time;
    picture_timestat[n].should_sleep = picture_time;
    picture_timestat[n].did_sleep = picture_time;
    picture_timestat[n].type = 0xff;
  }

  picture_decode_start_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_decode_end_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_display_start_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_display_end_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_type = calloc(TIMESTAT_NOF, sizeof(uint8_t));

  picture_decode_num = 0;
  picture_display_num = 0;
  
}

void timestat_print()
{
  int n;
  FILE *stat_file;
  FILE *gnuplot_file;
  picture_time_t tmppic;
  int statnr = 1;
  fprintf(stderr, "\ntimestat\n");


  for(n = 0; n < picture_decode_num; n++) {

    timesub(&(picture_timestat[n].dec_tot),
	    &(picture_timestat[n].dec_end),
	    &(picture_timestat[n].dec_start));
    
    picture_timestat[n].dec_min = picture_timestat[n].dec_tot;


    timesub(&(picture_timestat[n].disp_tot),
	    &(picture_timestat[n].disp_end),
	    &(picture_timestat[n].disp_start));

    picture_timestat[n].disp_min = picture_timestat[n].disp_tot;
    

    timesub(&(picture_timestat[n].pic_tot),
	    &(picture_timestat[n].disp_end),
	    &(picture_timestat[n].dec_start));

    picture_timestat[n].pic_min = picture_timestat[n].pic_tot;
    
  }
  
  if((stat_file = fopen("video_stats", "r")) == NULL) {
    perror("no previous stats");
  } else {
    n = 0;
    fread(&statnr, sizeof(statnr), 1, stat_file);
    statnr++;
    while(fread(&tmppic, sizeof(tmppic), 1, stat_file) && 
	  (n < picture_decode_num)) {
      if(timecompare(&(picture_timestat[n].dec_min), &(tmppic.dec_min)) > 0) {
	picture_timestat[n].dec_min = tmppic.dec_min;
      }
      timeadd(&(picture_timestat[n].dec_tot),
	      &(picture_timestat[n].dec_tot),
	      &(tmppic.dec_tot));
      
      if(timecompare(&(picture_timestat[n].disp_min), &(tmppic.disp_min)) > 0) {
	picture_timestat[n].disp_min = tmppic.disp_min;
      }
      timeadd(&(picture_timestat[n].disp_tot),
	      &(picture_timestat[n].disp_tot),
	      &(tmppic.disp_tot));

      if(timecompare(&(picture_timestat[n].pic_min), &(tmppic.pic_min)) > 0) {
	picture_timestat[n].pic_min = tmppic.pic_min;
      }
      timeadd(&(picture_timestat[n].pic_tot),
	      &(picture_timestat[n].pic_tot),
	      &(tmppic.pic_tot));
      n++;

    }
    picture_decode_num = n;
    fclose(stat_file);
  }
  
  if((stat_file = fopen("video_stats", "w")) == NULL) {
    perror("fopen");
  } else {
    fwrite(&statnr, sizeof(statnr), 1, stat_file);
    if(fwrite(picture_timestat, sizeof(picture_time_t),
	      picture_decode_num, stat_file) != picture_decode_num) {
      perror("fwrite");
    }
    fprintf(stderr, "Wrote stats to file: video_stats\n");
    fclose(stat_file);
  }

  if((gnuplot_file = fopen("stats_dec.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      timesub(&picture_time, &(picture_timestat[n].dec_end), &(picture_timestat[n].dec_start));
      timesub(&picture_time0, &(picture_timestat[n].dec_start), &(picture_timestat[0].dec_start));
      fprintf(gnuplot_file, "%d.%09ld %d.%09ld\n",
	      (int)picture_time0.tv_sec,
	      picture_time0.tv_nsec,
	      (int)picture_time.tv_sec,
	      picture_time.tv_nsec);
    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_disp.dat\n");
    fclose(gnuplot_file);
  } 

  
  if((gnuplot_file = fopen("stats_disp.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      timesub(&picture_time, &(picture_timestat[n].disp_end), &(picture_timestat[n].disp_start));
      timesub(&picture_time0, &(picture_timestat[n].disp_start), &(picture_timestat[0].dec_start));
      fprintf(gnuplot_file, "%d.%09ld %d.%09ld\n",
	      (int)picture_time0.tv_sec,
	      picture_time0.tv_nsec,
	      (int)picture_time.tv_sec,
	      picture_time.tv_nsec);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_disp.dat\n");
    fclose(gnuplot_file);
  } 


  if((gnuplot_file = fopen("stats_pic.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      timesub(&picture_time, &(picture_timestat[n].disp_end), &(picture_timestat[n].dec_start));
      timesub(&picture_time0, &(picture_timestat[n].dec_start), &(picture_timestat[0].dec_start));
      fprintf(gnuplot_file, "%d.%09ld %d.%09ld\n",
	      (int)picture_time0.tv_sec,
	      picture_time0.tv_nsec,
	      (int)picture_time.tv_sec,
	      picture_time.tv_nsec);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_pic.dat\n");
    fclose(gnuplot_file);
  } 





  /* ---------------- */

  if((gnuplot_file = fopen("avgstats_dec.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      
      fprintf(gnuplot_file, "%d %.09f\n",
	      n,
	      ((double)picture_timestat[n].dec_tot.tv_sec+
	      (double)picture_timestat[n].dec_tot.tv_nsec/
	      1000000000.0)/(double)statnr);

    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 

  
  if((gnuplot_file = fopen("avgstats_disp.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %.09f\n",
	      n,
	      ((double)picture_timestat[n].disp_tot.tv_sec+
	      (double)picture_timestat[n].disp_tot.tv_nsec/
	       1000000000.0)/(double)statnr);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 


  if((gnuplot_file = fopen("avgstats_pic.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %.09f\n",
	      n,
	      ((double)picture_timestat[n].pic_tot.tv_sec+
	       (double)picture_timestat[n].pic_tot.tv_nsec/
	       1000000000.0)/(double)statnr);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_pic.dat\n");
    fclose(gnuplot_file);
  } 

  fprintf(stderr, "statnr: %d\n", statnr);

  

  /* ------------------------------ */



  if((gnuplot_file = fopen("minstats_dec.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %d.%09ld\n",
	      n,
	      (int)picture_timestat[n].dec_min.tv_sec,
	      picture_timestat[n].dec_min.tv_nsec);
      
    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 

  
  if((gnuplot_file = fopen("minstats_disp.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %d.%09ld\n",
	      n,
	      (int)picture_timestat[n].disp_min.tv_sec,
	      picture_timestat[n].disp_min.tv_nsec);

    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 


  if((gnuplot_file = fopen("minstats_pic.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %d.%09ld\n",
	      n,
	      (int)picture_timestat[n].pic_min.tv_sec,
	      picture_timestat[n].pic_min.tv_nsec);

    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_pic.dat\n");
    fclose(gnuplot_file);
  } 



  /* ------------------------------- */
  



  if((gnuplot_file = fopen("stats_type.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      
      fprintf(gnuplot_file, "%d %d\n",
	      n,
	      picture_timestat[n].type);

      
    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 



  /* ------------------------------- */
    
    /*
    for(n = 0; n < picture_decode_num; n++) {
      fprintf(gnuplot_file, "%.9f %d\n",
	      1.0/24.0*n,
	      n);
    }
    */

  
  for(n = 0; n < 3; n++) {
    
    fprintf(stderr, "avg_decode_time: %.4f s,  %.2f fps \n",
	    ((double)picture_decode_time_tot[n].tv_sec +
	     (double)picture_decode_time_tot[n].tv_nsec/1000000000.0)/
	    (double)picture_decode_nr[n],
	    (double)picture_decode_nr[n]/
	    ((double)picture_decode_time_tot[n].tv_sec +
	     (double)picture_decode_time_tot[n].tv_nsec/1000000000.0));

    fprintf(stderr, "avg_display_time: %.4f s\n",
	    ((double)picture_display_time_tot[n].tv_sec +
	     (double)picture_display_time_tot[n].tv_nsec/1000000000.0)/
	    (double)picture_display_nr[n]);



    fprintf(stderr, "display_time_max: %d.%09ld\n",
	    (int)picture_display_time_max[0].tv_sec,
	    picture_display_time_max[0].tv_nsec);
    fprintf(stderr, "display_time_max: %d.%09ld\n",
	    (int)picture_display_time_max[1].tv_sec,
	    picture_display_time_max[1].tv_nsec);
    fprintf(stderr, "display_time_max: %d.%09ld\n",
	    (int)picture_display_time_max[2].tv_sec,
	    picture_display_time_max[2].tv_nsec);

    fprintf(stderr, "display_time_min: %d.%09ld\n",
	    (int)picture_display_time_min[0].tv_sec,
	    picture_display_time_min[0].tv_nsec);
    fprintf(stderr, "display_time_min: %d.%09ld\n",
	    (int)picture_display_time_min[1].tv_sec,
	    picture_display_time_min[1].tv_nsec);
    fprintf(stderr, "display_time_min: %d.%09ld\n",
	    (int)picture_display_time_min[2].tv_sec,
	    picture_display_time_min[2].tv_nsec);

    /*
    picture_decode_time_tot[n].tv_sec = 0;
    picture_decode_time_tot[n].tv_nsec = 0;
    
    picture_display_time_tot[n].tv_sec = 0;
    picture_display_time_tot[n].tv_nsec = 0;

    picture_decode_time_min[n].tv_sec = 99;
    picture_decode_time_min[n].tv_nsec = 0;
    
    picture_display_time_min[n].tv_sec = 99;
    picture_display_time_min[n].tv_nsec = 99;
    
    picture_decode_time_max[n].tv_sec = 0;
    picture_decode_time_max[n].tv_nsec = 0;

    picture_display_time_max[n].tv_sec = 0;
    picture_display_time_max[n].tv_nsec = 0;

    picture_decode_nr[n] = 0;
    picture_display_nr[n] = 0;
    */
  }
  
}


#endif

#ifdef HAVE_CLOCK_GETTIME

void print_time_offset(uint64_t PTS)
{
  struct timespec curtime;
  struct timespec ptstime;
  struct timespec predtime;
  struct timespec offset;

  ptstime.tv_sec = PTS/90000;
  ptstime.tv_nsec = (PTS%90000)*(1000000000/90000);

  clock_gettime(CLOCK_REALTIME, &curtime);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);

  fprintf(stderr, "video: offset: %ld.%09ld\n", offset.tv_sec, offset.tv_nsec);
}

#else

void print_time_offset(uint64_t PTS)
{
  struct timeval curtime;
  struct timeval ptstime;
  struct timeval predtime;
  struct timeval offset;

  ptstime.tv_sec = PTS/90000;
  ptstime.tv_usec = (PTS%90000)*(1000000/90000);

  gettimeofday(&curtime, NULL);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);

  fprintf(stderr, "video: offset: %ld.%06ld\n", offset.tv_sec, offset.tv_usec);
}

#endif
