/* SKROMPF - A video player
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

#include <glib.h>

#include "../include/common.h"
#include "c_getbits.h"


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
int last_pts_valid;
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


yuv_image_t *ref_image1;
yuv_image_t *ref_image2;
yuv_image_t *dst_image;
yuv_image_t *b_image;
yuv_image_t *current_image;
yuv_image_t *ring;


macroblock_t *cur_mbs;
macroblock_t *ref1_mbs;
macroblock_t *ref2_mbs;




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
  
  cur_mbs = malloc(36*45*sizeof(macroblock_t));
  ref1_mbs = malloc(36*45*sizeof(macroblock_t));
  ref2_mbs = malloc(36*45*sizeof(macroblock_t));
  
  // Setup signal handler.
  // SIGINT == ctrl-c
  sig.sa_handler = sighandler;
  sigaction(SIGINT, &sig, NULL);
  

#ifdef TIMESTAT
  timestat_init();
#endif
  
  // init values for MPEG-1
  pic.coding_ext.picture_structure = 0x3;
  pic.coding_ext.frame_pred_frame_dct = 1;
  pic.coding_ext.intra_vlc_format = 0;
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
  int n;
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
      group_of_pictures_header();
      extension_and_user_data(1);
      do {
	picture_header();
	extension_and_user_data(1);
	picture_data();
      } while(nextbits(32) == MPEG2_VS_PICTURE_START_CODE);
    } while(nextbits(32) == MPEG2_VS_GROUP_START_CODE);
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
  long int frame_interval_nsec;
  
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
    frame_interval_nsec = 41708333;
    break;
  case 0x2:
    DPRINTF(1, "24\n");
    frame_interval_nsec = 41666667;
    break;
  case 0x3:
    DPRINTF(1, "25\n");
    frame_interval_nsec = 40000000;
    break;
  case 0x4:
    DPRINTF(1, "30000/1001 (29.97)\n");
    frame_interval_nsec = 33366667;
    break;
  case 0x5:
    DPRINTF(1, "30\n");
    frame_interval_nsec = 33333333;
    break;
  case 0x6:
    DPRINTF(1, "50\n");
    frame_interval_nsec = 20000000;
    break;
  case 0x7:
    DPRINTF(1, "60000/1001 (59.94)\n");
    frame_interval_nsec = 16683333;
    break;
  case 0x8:
    DPRINTF(1, "60\n");
    frame_interval_nsec = 16666667;
    break;
  default:
    DPRINTF(1, "Reserved\n");
    break;
  }
  
  
  if(forced_frame_rate == -1) { /* No forced frame rate */
    buf_ctrl_head->frame_interval = frame_interval_nsec;
  } else {
    if(forced_frame_rate == 0) {
      buf_ctrl_head->frame_interval = 1;
    } else {
      buf_ctrl_head->frame_interval = 1000000000/forced_frame_rate;
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
  buf_ctrl_shmid = shmget(IPC_PRIVATE, buf_ctrl_size, IPC_CREAT | 0666);
  if( buf_ctrl_shmid == -1) {
    perror("shmget buf_ctrl");
    exit_program(1);
  }

  /* Attach the shared memory to the process. */
  buf_ctrl_shmaddr = shmat(buf_ctrl_shmid, NULL, 0);
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
  
  
  // Set the semaphores to the correct starting values
  if(sem_init(&(buf_ctrl_head->pictures_ready_to_display), 1, 0) == -1){
    perror("sem_init ready_to_display");
  }
  if(sem_init(&(buf_ctrl_head->pictures_displayed), 1, 0) == -1) {
    perror("sem_init displayed");
  }
  
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
    = shmget(IPC_PRIVATE, picture_buffers_size, IPC_CREAT | 0666);
  if (picture_buffers_shmid == -1) {
    perror("shmget picture_buffers");
    exit_program(1);
  }

  // Attach the shared memory to the process
  picture_buffers_shmaddr = shmat(picture_buffers_shmid, NULL, 0);
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
    buf_ctrl_head->picture_infos[i].in_use = 0;
    buf_ctrl_head->picture_infos[i].displayed = 1;
    
    baseaddr += picture_buffer_size;
  }

  
  ref_image1 = &buf_ctrl_head->picture_infos[0].picture;
  ref_image2 = &buf_ctrl_head->picture_infos[1].picture;
  b_image = &buf_ctrl_head->picture_infos[2].picture;
  
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
  long int frame_interval_nsec;
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
    frame_interval_nsec = 41708333;
    break;
  case 0x2:
    DPRINTF(1, "24\n");
    frame_interval_nsec = 41666667;
    break;
  case 0x3:
    DPRINTF(1, "25\n");
    frame_interval_nsec = 40000000;
    break;
  case 0x4:
    DPRINTF(1, "30000/1001 (29.97)\n");
    frame_interval_nsec = 33366667;
    break;
  case 0x5:
    DPRINTF(1, "30\n");
    frame_interval_nsec = 33333333;
    break;
  case 0x6:
    DPRINTF(1, "50\n");
    frame_interval_nsec = 20000000;
    break;
  case 0x7:
    DPRINTF(1, "60000/1001 (59.94)\n");
    frame_interval_nsec = 16683333;
    break;
  case 0x8:
    DPRINTF(1, "60\n");
    frame_interval_nsec = 16666667;
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
    buf_ctrl_head->frame_interval = frame_interval_nsec;
  } else {
    if(forced_frame_rate == 0) {
      buf_ctrl_head->frame_interval = 1;
    } else {
      buf_ctrl_head->frame_interval = 1000000000/forced_frame_rate;
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
  case 0x0:
    DPRINTF(1, "reserved");
    break;
  case 0x1:
    DPRINTF(1, "Top Field");
    break;
  case 0x2:
    DPRINTF(1, "Bottom Field");
    break;
  case 0x3:
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
    if(pic.coding_ext.picture_structure == 0x03) {
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
  next_start_code();
}


/* 6.2.3 Picture header */
void picture_header(void)
{
  uint32_t picture_start_code;

  DPRINTF(3, "picture_header\n");

  seq.mb_row = 0;
  picture_start_code = GETBITS(32, "picture_start_code");
  
  //TODO better check if pts really is for this picture
  if(PTS_DTS_flags & 0x02) {
    last_pts = PTS;
    prev_scr_nr = last_scr_nr;
    last_scr_nr = scr_nr;
    last_pts_valid = 1;
  } else {
    last_pts_valid = 0;
  }
  
  if(PTS_DTS_flags & 0x02) {
    if(last_scr_nr != prev_scr_nr) {   
      fprintf(stderr, "=== last_scr_nr: %d, prev_scr_nr: %d\n",
	      last_scr_nr, prev_scr_nr);
      fprintf(stderr, "--- last_scr: %ld.%09ld, prev_scr: %ld.%09ld\n",
	      ctrl_time[last_scr_nr].realtime_offset.tv_sec,
	      ctrl_time[last_scr_nr].realtime_offset.tv_nsec,
	      ctrl_time[prev_scr_nr].realtime_offset.tv_sec,
	      ctrl_time[prev_scr_nr].realtime_offset.tv_nsec);
      fprintf(stderr, "+++ last_pts: %lld\n", last_pts);
    }
  }

  
  
  //  print_time_offset(PTS);
  
  if(picture_start_code != MPEG2_VS_PICTURE_START_CODE) {
    fprintf(stderr, "wrong start_code picture_start_code: %08x\n",
	    picture_start_code);
  }

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
    /*    fprintf(stderr, "decode: BUF[%d]: in_use:%d, displayed: %d\n",
	    n,
	    buf_ctrl_head->picture_infos[n].in_use,
	    buf_ctrl_head->picture_infos[n].displayed);
    */
    if((buf_ctrl_head->picture_infos[n].in_use == 0) &&
       (buf_ctrl_head->picture_infos[n].displayed == 1)) {
      buf_ctrl_head->picture_infos[n].displayed = 0;
      return n;
    }
  }

  // didn't find empty buffer, wait for a picture to display
  if(sem_wait(&(buf_ctrl_head->pictures_displayed)) == -1) {
    perror("sem_wait get_picture_buf");
  }
  id = buf_ctrl_head->dpy_q[dpy_q_displayed_pos];
  buf_ctrl_head->picture_infos[id].displayed = 1;
  dpy_q_displayed_pos = (dpy_q_displayed_pos+1) % buf_ctrl_head->nr_of_buffers; 

  for(n = 0; n < buf_ctrl_head->nr_of_buffers; n++) {
    /*    fprintf(stderr, "decode: buf[%d]: in_use:%d, displayed: %d\n",
	    n,
	    buf_ctrl_head->picture_infos[n].in_use,
	    buf_ctrl_head->picture_infos[n].displayed);
    */
    if((buf_ctrl_head->picture_infos[n].in_use == 0) &&
       (buf_ctrl_head->picture_infos[n].displayed == 1)) {
      buf_ctrl_head->picture_infos[n].displayed = 0;
      return n;
    }
  }

  //  fprintf(stderr, "decode: *** get_picture_buf\n");

  if(sem_wait(&(buf_ctrl_head->pictures_displayed)) == -1) {
    perror("sem_wait get_picture_buf");
  }
  id = buf_ctrl_head->dpy_q[dpy_q_displayed_pos];
  buf_ctrl_head->picture_infos[id].displayed = 1;
  dpy_q_displayed_pos = (dpy_q_displayed_pos+1) % buf_ctrl_head->nr_of_buffers; 

  for(n = 0; n < buf_ctrl_head->nr_of_buffers; n++) {
    /*    fprintf(stderr, "decode: buf[%d]: in_use:%d, displayed: %d\n",
	    n,
	    buf_ctrl_head->picture_infos[n].in_use,
	    buf_ctrl_head->picture_infos[n].displayed);
    */
    if((buf_ctrl_head->picture_infos[n].in_use == 0) &&
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
      buf_ctrl_head->frame_interval = 1000000000/forced_frame_rate;
    }
  }
  if(sem_post(&(buf_ctrl_head->pictures_ready_to_display)) != 0) {
    perror("sempost pictures_ready");
  }
  
}


/* 6.2.3.6 Picture data */
void picture_data(void)
{
  int buf_id;
  yuv_image_t *reserved_image;
  static int prev_ref_buf_id = -1;
  static int old_ref_buf_id  = -1;
  uint64_t calc_pts;
  int err;
  static int bepa = 0;

  fprintf(stderr, ".");

  if(bepa >= 200) {
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
	    200/((double)qpt.tv_sec+(double)qpt.tv_nsec/1000000000.0));

  }
  
  bepa++;


  DPRINTF(3, "picture_data\n");
  
  if(msgqid != -1) {
    chk_for_msg();
  }
  
  /* If this is a I/P picture then we must release the reference 
     frame that is going to be replace. (It might not have been 
     displayed yet so it is not nessesary free for reuse.) */
  switch(pic.header.picture_coding_type) {
  case PIC_CODING_TYPE_I:
  case PIC_CODING_TYPE_P:
    if(old_ref_buf_id != -1) {
      buf_ctrl_head->picture_infos[old_ref_buf_id].in_use = 0;
    }
    break;
  case PIC_CODING_TYPE_B:
    break;
  }
  
  
  buf_id = get_picture_buf();
  

  reserved_image = &(buf_ctrl_head->picture_infos[buf_id].picture);
  //fprintf(stderr, "decode: decode start buf %d\n", buf_id);
  
  /* This is just a marker to later know if there is a real time 
     stamp for this picure. */
  buf_ctrl_head->picture_infos[buf_id].pts_time.tv_sec = -1;
  
  /* 
     If the packet containing the picture header start code hade 
     a time stamp, that time stamp used.
     
     Otherwise a time stamp is calculated from the last picture 
     produce for viewing. 
     Iff we predict the time stamp then we must also use the scr 
     from that picure.
  */
  if(last_pts_valid) {
    buf_ctrl_head->picture_infos[buf_id].PTS = last_pts;
    buf_ctrl_head->picture_infos[buf_id].pts_time.tv_sec = last_pts/90000;
    buf_ctrl_head->picture_infos[buf_id].pts_time.tv_nsec =
      (last_pts%90000)*(1000000000/90000);
    buf_ctrl_head->picture_infos[buf_id].realtime_offset =
      ctrl_time[last_scr_nr].realtime_offset;
    buf_ctrl_head->picture_infos[buf_id].scr_nr = last_scr_nr;
  } else {
    switch(pic.header.picture_coding_type) {
    case PIC_CODING_TYPE_I:
    case PIC_CODING_TYPE_P:
      /* TODO: Is this correct? */
      buf_ctrl_head->picture_infos[buf_id].realtime_offset =
	ctrl_time[last_scr_nr].realtime_offset;
      break;
    case PIC_CODING_TYPE_B:
      /* TODO: Check if there is a valid 'last_pts_to_dpy' to predict from. */
      calc_pts = last_pts_to_dpy + 90000/(1000000000/buf_ctrl_head->frame_interval);
      buf_ctrl_head->picture_infos[buf_id].PTS = calc_pts;
      buf_ctrl_head->picture_infos[buf_id].pts_time.tv_sec = calc_pts/90000;
      buf_ctrl_head->picture_infos[buf_id].pts_time.tv_nsec =
	(calc_pts%90000)*(1000000000/90000);

      buf_ctrl_head->picture_infos[buf_id].realtime_offset =
	ctrl_time[last_scr_nr_to_dpy].realtime_offset;
      buf_ctrl_head->picture_infos[buf_id].scr_nr = last_scr_nr_to_dpy;
      /*
      fprintf(stderr, "\n\nB-picture: no valid pts\n");
      fprintf(stderr, "B-picture: calculatedpts %lld.%09lld\n\n",
	      calc_pts/90000, (calc_pts%90000)*(1000000000/90000));
      */
      break;
    }
  }
  
  /* 
     If it's a I or P picture that we are about to decode then we can 
     prepare the old picuture (prev_ref_buf_id) for output (viewing).
     
     If it didn't get a pts from the stream, predict one like we do
     for B pictures above.
     
     If it's a B picutre that we are about to decode then we must now
     decide if we shall decode it or skipp it.
  */ 
  switch(pic.header.picture_coding_type) {
  case PIC_CODING_TYPE_I:
  case PIC_CODING_TYPE_P:
    buf_ctrl_head->picture_infos[buf_id].in_use = 1;
    
    ref_image1 = ref_image2; // Age the reference frame.
    ref_image2 = reserved_image; // and add the new (to be decoded) frame.
    
    dst_image = reserved_image;
    
    /* Only if we have a prev_ref_buf. */
    if(prev_ref_buf_id != -1) {
      
      /* Predict if we don't already have a pts for the frame. */
      if(buf_ctrl_head->picture_infos[prev_ref_buf_id].pts_time.tv_sec == -1) {
	calc_pts = last_pts_to_dpy + 90000/(1000000000/buf_ctrl_head->frame_interval);
	buf_ctrl_head->picture_infos[prev_ref_buf_id].PTS = calc_pts;
	buf_ctrl_head->picture_infos[prev_ref_buf_id].pts_time.tv_sec = calc_pts/90000;
	buf_ctrl_head->picture_infos[prev_ref_buf_id].pts_time.tv_nsec =
	  (calc_pts%90000)*(1000000000/90000);
	buf_ctrl_head->picture_infos[prev_ref_buf_id].realtime_offset =
	  ctrl_time[last_scr_nr_to_dpy].realtime_offset;
	buf_ctrl_head->picture_infos[prev_ref_buf_id].scr_nr = last_scr_nr_to_dpy;
	/*	
	fprintf(stderr, "\n\nI/P-picture: no valid pts\n");
	fprintf(stderr, "I/P-picture: calculatedpts %lld.%09lld\n\n",
		calc_pts/90000, (calc_pts%90000)*(1000000000/90000));
	*/
      }
      
      /* Put it in the display queue. */
      last_pts_to_dpy = buf_ctrl_head->picture_infos[prev_ref_buf_id].PTS;
      last_scr_nr_to_dpy = buf_ctrl_head->picture_infos[prev_ref_buf_id].scr_nr;
      dpy_q_put(prev_ref_buf_id);
      old_ref_buf_id = prev_ref_buf_id;
    }
    
    prev_ref_buf_id = buf_id;
    break;
  case PIC_CODING_TYPE_B:
    dst_image = reserved_image;

    /* Calculate the time remaining until this picutre shall be viewed. */
    {
      struct timespec realtime, calc_rt, err_time;
      
      clock_gettime(CLOCK_REALTIME, &realtime);
      
      timeadd(&calc_rt,
	      &(buf_ctrl_head->picture_infos[buf_id].pts_time),
	      &(buf_ctrl_head->picture_infos[buf_id].realtime_offset));
      timesub(&err_time, &calc_rt, &realtime);

      /* If the picture should already have been displayed then drop it. */
      /* TODO: More investigation needed. */
      if(err_time.tv_nsec < 0 && forced_frame_rate != 0) {
	fprintf(stderr, "\n***Drop B-frame in decoder\n");
	fprintf(stderr, "errpts %ld.%+010ld\n\n",
		err_time.tv_sec,
		err_time.tv_nsec);
	
	buf_ctrl_head->picture_infos[buf_id].in_use = 0;
	buf_ctrl_head->picture_infos[buf_id].displayed = 1;
	last_pts_to_dpy = buf_ctrl_head->picture_infos[buf_id].PTS;
	last_scr_nr_to_dpy = buf_ctrl_head->picture_infos[buf_id].scr_nr;//?
	do {
	  GETBITS(8, "drop");
	  next_start_code();
	} while((nextbits(32) >= MPEG2_VS_SLICE_START_CODE_LOWEST) &&
		(nextbits(32) <= MPEG2_VS_SLICE_START_CODE_HIGHEST));
	// Start processing the next picture.
	return;
	
      }
    }
    break;
  }
  
  /* Decode the slices. */
  if( MPEG2 )
    do {
      mpeg2_slice();
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
  
  // Check 'err' here?

  // Picture decoded
  switch(pic.header.picture_coding_type) {
  case PIC_CODING_TYPE_I:
  case PIC_CODING_TYPE_P:
    break;
  case PIC_CODING_TYPE_B:
    // B-picture
    if(prev_ref_buf_id == -1) {
      fprintf(stderr, "decode: B-frame before any frame!!! (Error).\n");
    }
    if(old_ref_buf_id == -1) { // Test closed_gop too....
      fprintf(stderr, "decode: B-frame before forward ref frame\n");
    }
    last_pts_to_dpy = buf_ctrl_head->picture_infos[buf_id].PTS;
    last_scr_nr_to_dpy = buf_ctrl_head->picture_infos[buf_id].scr_nr;//?
    dpy_q_put(buf_id);
    break;
  }

  
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


/* This should be moved into a separate file. (And cleand up) */
void motion_comp()
{
  int padded_width,x,y;
  int stride;
  int d;
  uint8_t *dst_y,*dst_u,*dst_v;
  int half_flag_y[2];
  int half_flag_uv[2];
  int int_vec_y[2];
  int int_vec_uv[2];

  uint8_t *pred_y;
  uint8_t *pred_u;
  uint8_t *pred_v;
  int field;

  DPRINTF(2, "dct_type: %d\n", mb.modes.dct_type);

  padded_width = seq.mb_width * 16;
  
  // handle interlaced blocks
  if (mb.modes.dct_type) {
    d = 1;
    stride = padded_width * 2;
  } else {
    d = 8;
    stride = padded_width;
  }

  x = seq.mb_column;
  y = seq.mb_row;
    
  dst_y = &dst_image->y[x * 16 + y * padded_width * 16];
  dst_u = &dst_image->u[x * 8 + y * padded_width/2 * 8];
  dst_v = &dst_image->v[x * 8 + y * padded_width/2 * 8];

  
  if(mb.modes.macroblock_motion_forward) {

    int vcount, i;
    
    DPRINTF(2, "forward_motion_comp\n");
    DPRINTF(3, "x: %d, y: %d\n", x, y);
    
    if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
      fprintf(stderr, "**** DP remove this and check if working\n");
      //exit(-1);
    }

    if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
      vcount = mb.motion_vector_count;
    } else { 
      vcount = 1;
    }


    for(i = 0; i < vcount; i++) {
      
      field = mb.motion_vertical_field_select[i][0];

      half_flag_y[0]  = (mb.vector[i][0][0] & 1);
      half_flag_y[1]  = (mb.vector[i][0][1] & 1);
      half_flag_uv[0] = ((mb.vector[i][0][0]/2) & 1);
      half_flag_uv[1] = ((mb.vector[i][0][1]/2) & 1);

      int_vec_y[0]  = (mb.vector[i][0][0] >> 1) + x * 16;
      int_vec_y[1]  = (mb.vector[i][0][1] >> 1)*vcount + y * 16;
      int_vec_uv[0] = ((mb.vector[i][0][0]/2) >> 1)  + x * 8;
      int_vec_uv[1] = ((mb.vector[i][0][1]/2) >> 1)*vcount + y * 8;
      
      /* Check for vectors pointing outside the reference frames */
      if( (int_vec_y[0] < 0) || (int_vec_y[0] > (seq.mb_width - 1) * 16) ||
	  (int_vec_y[1] < 0) || (int_vec_y[1] > (seq.mb_height - 1) * 16) ||
	  (int_vec_uv[0] < 0) || (int_vec_uv[0] > (seq.mb_width - 1) * 8) ||
	  (int_vec_uv[1] < 0) || (int_vec_uv[1] > (seq.mb_height - 1) * 8) ) { 
	fprintf(stderr, "Y (%i,%i), UV (%i,%i)\n", 
		int_vec_y[0], int_vec_y[1], int_vec_uv[0], int_vec_uv[1]);
      }
      
      pred_y = &ref_image1->y[int_vec_y[0] + int_vec_y[1] * padded_width];
      pred_u = &ref_image1->u[int_vec_uv[0] + int_vec_uv[1] * padded_width/2];
      pred_v = &ref_image1->v[int_vec_uv[0] + int_vec_uv[1] * padded_width/2];
      
      DPRINTF(3, "x: %d, y: %d\n", x, y);
      
      
      if (half_flag_y[0] && half_flag_y[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpXY_U8_U8_16x8(dst_y + i*padded_width,
					pred_y+field*padded_width,
					padded_width*2, padded_width*2);
	} else {
	  mlib_VideoInterpXY_U8_U8_16x16(dst_y, pred_y,
					 padded_width, padded_width);
	}
      } else if (half_flag_y[0]) {
	if(vcount == 2) {
	  mlib_VideoInterpX_U8_U8_16x8(dst_y + i*padded_width,
				       pred_y+field*padded_width,
				       padded_width*2, padded_width*2);
	} else {
	  mlib_VideoInterpX_U8_U8_16x16(dst_y, pred_y,
					padded_width, padded_width);
	}
      } else if (half_flag_y[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpY_U8_U8_16x8(dst_y + i*padded_width,
				       pred_y+field*padded_width,
				       padded_width*2, padded_width*2);
	} else {
	  mlib_VideoInterpY_U8_U8_16x16(dst_y, pred_y,
					padded_width, padded_width);
	}
      } else {
	if(vcount == 2) {
	  mlib_VideoCopyRef_U8_U8_16x8(dst_y + i*padded_width,
				       pred_y+field*padded_width,
				       padded_width*2);
	} else {
	  mlib_VideoCopyRef_U8_U8_16x16(dst_y, pred_y,
					padded_width);
	}
      }

      if (half_flag_uv[0] && half_flag_uv[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpXY_U8_U8_8x4(dst_u + i*padded_width/2, 
				       pred_u+field*padded_width/2,
				       padded_width*2/2, padded_width*2/2);

	  mlib_VideoInterpXY_U8_U8_8x4(dst_v + i*padded_width/2,
				       pred_v+field*padded_width/2,
				       padded_width*2/2, padded_width*2/2);
	} else {
	  mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u,
					 padded_width/2, padded_width/2);
	  mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v,
					 padded_width/2, padded_width/2);
	}
      } else if (half_flag_uv[0]) {
	if(vcount == 2) {
	  mlib_VideoInterpX_U8_U8_8x4(dst_u + i*padded_width/2,
				      pred_u+field*padded_width/2,
				      padded_width*2/2, padded_width*2/2);

	  mlib_VideoInterpX_U8_U8_8x4(dst_v + i*padded_width/2,
				      pred_v+field*padded_width/2,
				      padded_width*2/2, padded_width*2/2);
	} else {
	  mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u,
					padded_width/2, padded_width/2);
	  mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v, 
					padded_width/2, padded_width/2);
	}
      } else if (half_flag_uv[1]) {
	if(vcount == 2) {
	  mlib_VideoInterpY_U8_U8_8x4(dst_u + i*padded_width/2,
				      pred_u+field*padded_width/2,
				      padded_width*2/2, padded_width*2/2);
	  
	  mlib_VideoInterpY_U8_U8_8x4(dst_v + i*padded_width/2,
				      pred_v+field*padded_width/2,
				      padded_width*2/2, padded_width*2/2);
	} else {
	  mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u,
					padded_width/2, padded_width/2);
	  
	  mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v,
					padded_width/2, padded_width/2);
	}
      } else {
	if(vcount == 2) {
	  mlib_VideoCopyRef_U8_U8_8x4(dst_u + i*padded_width/2,
				      pred_u+field*padded_width/2,
				      padded_width*2/2);
	  
	  mlib_VideoCopyRef_U8_U8_8x4(dst_v + i*padded_width/2,
				      pred_v+field*padded_width/2,
				      padded_width*2/2);
	} else {
	  mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, padded_width/2);
	  mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, padded_width/2);
	}
      }
    }
  }
  
  if(mb.modes.macroblock_motion_backward) {
    int vcount, i;
    
    DPRINTF(2, "backward_motion_comp\n");
    DPRINTF(3, "x: %d, y: %d\n", x, y);
    
    if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
      fprintf(stderr, "**** DP remove this and check if working\n");
      //exit(-1);
    }

    if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
      vcount = mb.motion_vector_count;
    } else { 
      vcount = 1;
    }
    
    for(i = 0; i < vcount; i++) {
      
      field = mb.motion_vertical_field_select[i][1];

      half_flag_y[0]  = (mb.vector[i][1][0] & 1);
      half_flag_y[1]  = (mb.vector[i][1][1] & 1);
      half_flag_uv[0] = ((mb.vector[i][1][0]/2) & 1);
      half_flag_uv[1] = ((mb.vector[i][1][1]/2) & 1);

      int_vec_y[0]  = (mb.vector[i][1][0] >> 1) + (signed int)x * 16;
      int_vec_y[1]  = (mb.vector[i][1][1] >> 1)*vcount + (signed int)y * 16;
      int_vec_uv[0] = ((mb.vector[i][1][0]/2) >> 1) + (signed int)x * 8;
      int_vec_uv[1] = ((mb.vector[i][1][1]/2) >> 1)*vcount + (signed int)y * 8;
      
      /* Check for vectors pointing outside the reference frames */
      if( (int_vec_y[0] < 0) || (int_vec_y[0] > (seq.mb_width - 1) * 16) ||
	  (int_vec_y[1] < 0) || (int_vec_y[1] > (seq.mb_height - 1) * 16) ||
	  (int_vec_uv[0] < 0) || (int_vec_uv[0] > (seq.mb_width - 1) * 8) ||
	  (int_vec_uv[1] < 0) || (int_vec_uv[1] > (seq.mb_height - 1) * 8) ) { 
	fprintf(stderr, "Y (%i,%i), UV (%i,%i)\n", 
               int_vec_y[0], int_vec_y[1], int_vec_uv[0], int_vec_uv[1]);
      }

      pred_y = &ref_image2->y[int_vec_y[0] + int_vec_y[1] * padded_width];
      pred_u = &ref_image2->u[int_vec_uv[0] + int_vec_uv[1] * padded_width/2];
      pred_v = &ref_image2->v[int_vec_uv[0] + int_vec_uv[1] * padded_width/2];
      
 
      
      if(mb.modes.macroblock_motion_forward) {
	if (half_flag_y[0] && half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveXY_U8_U8_16x8(dst_y + i*padded_width,
					     pred_y+field*padded_width,
					     padded_width*2, padded_width*2);
	  } else {
	    mlib_VideoInterpAveXY_U8_U8_16x16(dst_y,  pred_y,  
					      padded_width,   padded_width);
	  }
	} else if (half_flag_y[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveX_U8_U8_16x8(dst_y + i*padded_width,
					    pred_y+field*padded_width,
					    padded_width*2, padded_width*2);
	  } else {
	    mlib_VideoInterpAveX_U8_U8_16x16(dst_y, pred_y,
					     padded_width, padded_width);
	  }
	} else if (half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveY_U8_U8_16x8(dst_y + i*padded_width,
					    pred_y+field*padded_width,
					    padded_width*2, padded_width*2);
	  } else {
	    mlib_VideoInterpAveY_U8_U8_16x16(dst_y, pred_y,
					     padded_width, padded_width);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRefAve_U8_U8_16x8(dst_y + i*padded_width,
					    pred_y+field*padded_width,
					    padded_width*2);
	  } else {
	    mlib_VideoCopyRefAve_U8_U8_16x16(dst_y, pred_y, padded_width);
	  }
	}
	if (half_flag_uv[0] && half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_u + i*padded_width/2,
					    pred_u+field*padded_width/2,
					    padded_width*2/2, padded_width*2/2);
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_v + i*padded_width/2,
					    pred_v+field*padded_width/2,
					    padded_width*2/2, padded_width*2/2);
	  } else {
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_u, pred_u,
					    padded_width/2, padded_width/2);
	    
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_v, pred_v,
					    padded_width/2, padded_width/2);
	  }
	} else if (half_flag_uv[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_u + i*padded_width/2,
					   pred_u+field*padded_width/2,
					   padded_width*2/2, padded_width*2/2);
	    
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_v + i*padded_width/2,
					   pred_v+field*padded_width/2,
					   padded_width*2/2, padded_width*2/2);
	  } else {
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_u, pred_u,
					   padded_width/2, padded_width/2);
	
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_v, pred_v,
					   padded_width/2, padded_width/2);
	  }
	} else if (half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_u + i*padded_width/2,
					   pred_u+field*padded_width/2,
					   padded_width*2/2, padded_width*2/2);
	   
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_v + i*padded_width/2,
					   pred_v+field*padded_width/2,
					   padded_width*2/2, padded_width*2/2);
	  } else {
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_u, pred_u,
					   padded_width/2, padded_width/2);
	 
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_v, pred_v,
					   padded_width/2, padded_width/2);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_u + i*padded_width/2,
					   pred_u+field*padded_width/2,
					   padded_width*2/2);

	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_v + i*padded_width/2,
					   pred_v+field*padded_width/2,
					   padded_width*2/2);
	  } else {
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_u, pred_u, padded_width/2);
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_v, pred_v, padded_width/2);
	  }
	}
      } else {
	if (half_flag_y[0] && half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpXY_U8_U8_16x8(dst_y + i*padded_width,
					  pred_y+field*padded_width,
					  padded_width*2, padded_width*2);
	  } else {
	    mlib_VideoInterpXY_U8_U8_16x16(dst_y, pred_y,
					   padded_width, padded_width);
	  }
	} else if (half_flag_y[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpX_U8_U8_16x8(dst_y + i*padded_width,
					 pred_y+field*padded_width,
					 padded_width*2, padded_width*2);
	  } else {
	    mlib_VideoInterpX_U8_U8_16x16(dst_y, pred_y,
					  padded_width, padded_width);
	  }
	} else if (half_flag_y[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpY_U8_U8_16x8(dst_y + i*padded_width,
					 pred_y+field*padded_width,
					 padded_width*2, padded_width*2);
	  } else {
	    mlib_VideoInterpY_U8_U8_16x16(dst_y, pred_y,
					  padded_width, padded_width);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRef_U8_U8_16x8(dst_y + i*padded_width,
					 pred_y+field*padded_width,
					 padded_width*2);
	  } else {
	    mlib_VideoCopyRef_U8_U8_16x16(dst_y, pred_y, padded_width);
	  }
	}
	if (half_flag_uv[0] && half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpXY_U8_U8_8x4(dst_u + i*padded_width/2,
					 pred_u+field*padded_width/2,
					 padded_width*2/2, padded_width*2/2);
	    
	    mlib_VideoInterpXY_U8_U8_8x4(dst_v + i*padded_width/2,
					 pred_v+field*padded_width/2, 
					 padded_width*2/2, padded_width*2/2);
	  } else {
	    mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u,
					   padded_width/2, padded_width/2);
	    
	    mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v,
					   padded_width/2, padded_width/2);
	  }
	} else if (half_flag_uv[0]) {
	  if(vcount == 2) {
	    mlib_VideoInterpX_U8_U8_8x4(dst_u + i*padded_width/2,
					pred_u+field*padded_width/2,
					padded_width*2/2, padded_width*2/2);
	    
	    mlib_VideoInterpX_U8_U8_8x4(dst_v + i*padded_width/2,
					pred_v+field*padded_width/2, 
					padded_width*2/2, padded_width*2/2);
	  } else {
	    mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u,
					  padded_width/2, padded_width/2);
	    
	    mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v,
					  padded_width/2, padded_width/2);
	  }
	} else if (half_flag_uv[1]) {
	  if(vcount == 2) {
	    mlib_VideoInterpY_U8_U8_8x4(dst_u + i*padded_width/2,
					pred_u+field*padded_width/2,
					padded_width*2/2, padded_width*2/2);

	    mlib_VideoInterpY_U8_U8_8x4(dst_v + i*padded_width/2,
					pred_v+field*padded_width/2, 
					padded_width*2/2, padded_width*2/2);
	  } else {
	    mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u,
					  padded_width/2, padded_width/2);

	    mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v,
					  padded_width/2, padded_width/2);
	  }
	} else {
	  if(vcount == 2) {
	    mlib_VideoCopyRef_U8_U8_8x4(dst_u + i*padded_width/2,
					pred_u+field*padded_width/2,
					padded_width*2/2);
	    
	    mlib_VideoCopyRef_U8_U8_8x4(dst_v + i*padded_width/2,
					pred_v+field*padded_width/2, 
					padded_width*2/2);
	  } else {
	    mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, padded_width/2);
	    mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, padded_width/2);
	  }
	}
      }
    }
  }
  
}


void motion_comp_add_coeff(unsigned int i)
{
  int padded_width,x,y;
  int stride;
  int d;
  uint8_t *dst_y,*dst_u,*dst_v;

  padded_width = seq.mb_width * 16; //seq.horizontal_size;

  if (mb.modes.dct_type) {
    d = 1;
    stride = padded_width * 2;
  } else {
    d = 8;
    stride = padded_width;
  }

  x = seq.mb_column;
  y = seq.mb_row;

  dst_y = &dst_image->y[x * 16 + y * padded_width * 16];
  dst_u = &dst_image->u[x * 8 + y * padded_width/2 * 8];
  dst_v = &dst_image->v[x * 8 + y * padded_width/2 * 8];


  switch(i) {
  case 0:
    mlib_VideoAddBlock_U8_S16(dst_y, mb.QFS, stride);
    break;
  case 1:
    mlib_VideoAddBlock_U8_S16(dst_y + 8, mb.QFS, stride);
    break;
  case 2:
    mlib_VideoAddBlock_U8_S16(dst_y + padded_width * d, mb.QFS, stride);
    break;
  case 3:
    mlib_VideoAddBlock_U8_S16(dst_y + padded_width * d + 8, mb.QFS, stride);
    break;
  case 4:
    mlib_VideoAddBlock_U8_S16(dst_u, mb.QFS, padded_width/2);
    break;
  case 5:
    mlib_VideoAddBlock_U8_S16(dst_v, mb.QFS, padded_width/2);
    break;
  }
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
     ((pic.coding_ext.picture_structure == 0x1) ||
      (pic.coding_ext.picture_structure == 0x2))) {
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

