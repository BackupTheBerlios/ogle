/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort, Martin Norbäck
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
//#include <siginfo.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h> // for byte swapping only
#include <sys/msg.h>
#include <errno.h>

#include "programstream.h"
#include "common.h"
#include "msgtypes.h"
#include "queue.h"
#include "mpeg.h"
#include "ip_sem.h"



#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

typedef enum {
  STREAM_NOT_REGISTERED = 0,
  STREAM_DISCARD = 1,
  STREAM_DECODE = 2,
  STREAM_MUTED = 3
} stream_state_t;
  
typedef struct {
  FILE *file;
  int shmid;
  char *shmaddr;
  stream_state_t state; // not_registerd, in_use, muted, ...
} buf_data_t;

buf_data_t id_reg[256];
buf_data_t id_reg_ps1[256];

int register_id(uint8_t id, int subtype);
int id_registered(uint8_t id, uint8_t subtype);
int init_id_reg();
int wait_for_msg(cmdtype_t cmdtype);
int eval_msg(cmd_t *cmd);
int get_buffer(int size);
int send_msg(msg_t *msg, int mtext_size);
int attach_decoder_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
int id_stat(uint8_t id, uint8_t subtype);
char *id_qaddr(uint8_t id, uint8_t subtype);
FILE *id_file(uint8_t id, uint8_t subtype);
void id_add(uint8_t stream_id, uint8_t subtype, stream_state_t state, int shmid, char *shmaddr, FILE *file);
int put_in_q(char *q_addr, int off, int len, uint8_t PTS_DTS_flags,
	     uint64_t PTS, uint64_t DTS);
int attach_buffer(int shmid, int size);
int chk_for_msg(void);

typedef struct {
  uint8_t *buf_start;
  int len;
  int in_use;
} q_elem;

int msgqid = -1;



int scr_discontinuity = 0;
uint64_t SCR_base;
uint16_t SCR_ext;
uint8_t SCR_flags;


int packnr = 0;


#define MPEG1 0x0
#define MPEG2 0x1

uint8_t    *buf;

int shmid;
char *shmaddr;
int shmsize;
unsigned int bytes_read = 0;

char *data_buf_addr;


int off_from;
int off_to;

extern char *optarg;
extern int   optind, opterr, optopt;

/* Variables for getbits */
unsigned int bits_left = 64;
uint64_t     cur_word = 0;

unsigned int nextbits(unsigned int nr_of_bits);

int synced = 0;

int audio       = 0;
int debug       = 0;

char *program_name;
FILE *video_file;
FILE *audio_file;
FILE *subtitle_file;
int video_stream = -1;

int   infilefd;
void* infileaddr;
long  infilelen;
uint32_t offs;

// #define DEBUG

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

#ifdef STATS
    static uint32_t stat_video_unaligned_packet_offset = 0;
    static uint32_t stat_video_unaligned_packet_end    = 0;
    static uint32_t stat_video_n_packets               = 0;
    static uint32_t stat_audio_n_packets               = 0;
    static uint32_t stat_subpicture_n_packets          = 0;
    static uint32_t stat_n_packets                     = 0;
#endif //STATS

void usage()
{
  fprintf(stderr, "Usage: %s [-v <video file>] [-a <audio file>] [-s <subtitle file> -i <subtitle_id>] [-d <debug level>] <input file>\n", 
	  program_name);
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
static inline uint32_t getbits(unsigned int nr)
#endif
{
  uint32_t result;
  
  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  bits_left -=nr;
  if(bits_left <= 32) {
    uint32_t new_word = GUINT32_FROM_BE(*(uint32_t *)(&buf[offs]));
    offs+=4;
    cur_word = (cur_word << 32) | new_word;
    bits_left = bits_left+32;
  }
  
  DPRINTF(5, "%s getbits(%u): %0*i, 0x%0*x, ", 
             func, nr, (nr-1)/3+1, result, (nr-1)/4+1, result);
  DPRINTBITS(6, nr, result);
  DPRINTF(5, "\n");
  
  return result;
}


static inline void drop_bytes(int len)
{  
  uint rest;
  uint todo;

  // This is always byte-aligned. Trust us.

  len  -= bits_left/8;
  rest  = len % 4;
  offs += (len - rest);
  todo  = bits_left+rest*8;

  while(todo > 32) {
    GETBITS(32, "skip");
    todo -= 32;
  }
  GETBITS(todo, "skip");

  return;
}

unsigned int nextbits(unsigned int nr_of_bits)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  DPRINTF(4, "nextbits %08x\n",(result >> (32-nr_of_bits )));
  return result >> (32-nr_of_bits);
}


static inline void marker_bit(void)                                                           {
  if(!GETBITS(1, "markerbit")) {
    fprintf(stderr, "*** demux: incorrect marker_bit in stream\n");
    //exit(-1);
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

/* Table 2-24 -- Stream_id table */
void dprintf_stream_id (int debuglevel, int stream_id)
{
#ifdef DEBUG
  DPRINTF(debuglevel, "0x%02x ", stream_id);
  if ((stream_id & 0xf0) == 0xb0) // 1011 xxxx
    switch (stream_id & 0x0f) { 
      case 0x8:
        DPRINTF(debuglevel, "[all audio streams]\n");
        return;
      case 0x9:
        DPRINTF(debuglevel, "[all video streams]\n");
        return;
      case 0xc:
        DPRINTF(debuglevel, "[program stream map]\n");
        return;
      case 0xd:
        DPRINTF(debuglevel, "[private_stream_1]\n");
        return;
      case 0xe:
        DPRINTF(debuglevel, "[padding stream]\n");
        return;
      case 0xf:
        DPRINTF(debuglevel, "[private_stream_2]\n");
        return;
    }
  else if ((stream_id & 0xe0) == 0xc0) { // 110x xxxx
    DPRINTF(debuglevel, "[audio stream number %i]\n", stream_id & 0x1f);
    return;
  } else if ((stream_id & 0xf0) == 0xe0) {
    DPRINTF(debuglevel, "[video stream number %i]\n", stream_id & 0x1f);
    return;
  } else if ((stream_id & 0xf0) == 0xf0) {
    switch (stream_id & 0x0f) {
      case 0x0:
        DPRINTF(debuglevel, "[ECM]\n");
        return;
      case 0x1:
        DPRINTF(debuglevel, "[EMM]\n");
        return;
      case 0x2:
        DPRINTF(debuglevel, "[DSM CC]\n");
        return;
      case 0x3:
        DPRINTF(debuglevel, "[ISO/IEC 13522 stream]\n");
        return;
      case 0xf:
        DPRINTF(debuglevel, "[program stream directory]\n");
        return;
      default:
        DPRINTF(debuglevel, "[reserved data stream - number %i]\n",
                stream_id & 0x0f);
        return;
    }
  }
  DPRINTF(debuglevel, "[illegal stream_id]\n");
#else
#endif
}


/* demuxer state */

int system_header_set = 0;
int stream_open = 0;



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
  int min_bufsize = 0;
  
  DPRINTF(1, "system_header()\n");

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

  DPRINTF(1, "header_length:          %hu\n", header_length);
  DPRINTF(1, "rate_bound:             %u [%u bits/s]\n",
          rate_bound, rate_bound*50*8);
  DPRINTF(1, "audio_bound:            %u\n", audio_bound);
  DPRINTF(1, "fixed_flag:             %u\n", fixed_flag);
  DPRINTF(1, "CSPS_flag:              %u\n", CSPS_flag);
  DPRINTF(1, "system_audio_lock_flag: %u\n", system_audio_lock_flag);
  DPRINTF(1, "system_video_lock_flag: %u\n", system_video_lock_flag);
  DPRINTF(1, "video_bound:            %u\n", video_bound);
  
  while(nextbits(1) == 1) {
    stream_id = GETBITS(8, "stream_id");
    GETBITS(2, "11");
    P_STD_buffer_bound_scale = GETBITS(1, "P_STD_buffer_bound_scale");
    P_STD_buffer_size_bound  = GETBITS(13, "P_STD_buffer_size_bound");
    DPRINTF(1, "stream_id:                ");
    dprintf_stream_id(1, stream_id);
    DPRINTF(1, "P-STD_buffer_bound_scale: %u\n", P_STD_buffer_bound_scale);
    DPRINTF(1, "P-STD_buffer_size_bound:  %u [%u bytes]\n",
        P_STD_buffer_size_bound,
	    P_STD_buffer_size_bound*(P_STD_buffer_bound_scale?1024:128));
    min_bufsize+= P_STD_buffer_size_bound*(P_STD_buffer_bound_scale?1024:128);
  }

  if(!system_header_set) {
    system_header_set = 1;
    if(msgqid != -1) {
      get_buffer(min_bufsize);
    }
  }
  
}



int pack_header()
{
  uint64_t system_clock_reference;
  uint64_t system_clock_reference_base;
  uint16_t system_clock_reference_extension;
  uint32_t program_mux_rate;
  uint8_t  pack_stuffing_length;
  int i;
  uint8_t fixed_bits;
  int version;
  static uint64_t prev_scr = 0;
  
  DPRINTF(2, "pack_header()\n");

  GETBITS(32,"pack_start_code");
  
  fixed_bits = nextbits(4);
  if(fixed_bits == 0x02) {
    version = MPEG1;
  } else if((fixed_bits >> 2) == 0x01) {
    version = MPEG2;
  } else {
    version = -1;
    fprintf(stderr, "unknown MPEG version\n");
  }
  
  
  switch(version) {
  case MPEG2:
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
    /* 2.5.2 or 2.5.3.4 (definition of system_clock_reference) */
    system_clock_reference = 
      system_clock_reference_base * 300 + system_clock_reference_extension;
    SCR_base = system_clock_reference_base;
    SCR_ext = system_clock_reference_extension;
    SCR_flags = 0x3;

    /*
    fprintf(stderr, "**** scr base %llx, ext %x, tot %llx\n",
	    system_clock_reference_base,
	    system_clock_reference_extension,
	    system_clock_reference);
    */

    marker_bit();
    program_mux_rate = GETBITS(22, "program_mux_rate");
    marker_bit();
    marker_bit();
    GETBITS(5, "reserved");
    pack_stuffing_length = GETBITS(3, "pack_stuffing_length");
    for(i=0;i<pack_stuffing_length;i++) {
      GETBITS(8 ,"stuffing_byte");
    }
    DPRINTF(1, "system_clock_reference_base: %llu\n",
	    system_clock_reference_base);
    DPRINTF(1, "system_clock_reference_extension: %u\n",
	    system_clock_reference_extension);
    DPRINTF(1, "system_clock_reference: %llu [%6f s]\n", 
	    system_clock_reference, ((double)system_clock_reference)/27000000.0);

    if(system_clock_reference < prev_scr) {
      scr_discontinuity = 1;
      fprintf(stderr, "*** Backward scr discontinuity\n");
      fprintf(stderr, "system_clock_reference: [%6f s]\n", 
	      ((double)system_clock_reference)/27000000.0);
      fprintf(stderr, "prev system_clock_reference: [%6f s]\n", 
	      ((double)prev_scr)/27000000.0);
      
    } else if((system_clock_reference - prev_scr) > 2700000) {
      scr_discontinuity = 1;
      fprintf(stderr, "*** Forward scr discontinuity\n");
      fprintf(stderr, "system_clock_reference: [%6f s]\n", 
	      ((double)system_clock_reference)/27000000.0);
      fprintf(stderr, "prev system_clock_reference: [%6f s]\n", 
	      ((double)prev_scr)/27000000.0);
    }
    prev_scr = system_clock_reference;
    
    DPRINTF(1, "program_mux_rate: %u [%u bits/s]\n",
	    program_mux_rate, program_mux_rate*50*8);
    DPRINTF(3, "pack_stuffing_length: %u\n",
	    pack_stuffing_length);
    break;
  case MPEG1:
    GETBITS(4, "0010");
    
    system_clock_reference_base = 
      GETBITS(3,"system_clock_reference_base [32..30]") << 30;
    marker_bit();
    system_clock_reference_base |= 
      GETBITS(15, "system_clock_reference_base [29..15]") << 15;
    marker_bit();
    system_clock_reference_base |= 
      GETBITS(15, "system_clock_reference_base [14..0]");
    marker_bit();
    marker_bit();
    program_mux_rate = GETBITS(22, "program_mux_rate");
    marker_bit();
    DPRINTF(2, "system_clock_reference_base: %llu\n",
	    system_clock_reference_base);
    DPRINTF(2, "program_mux_rate: %u [%u bits/s]\n",
	    program_mux_rate, program_mux_rate*50*8);

    // no extension in MPEG-1 ??
    system_clock_reference = system_clock_reference_base * 300;
    SCR_base = system_clock_reference_base;
    SCR_flags = 0x1;
    DPRINTF(1, "system_clock_reference: %llu [%6f s]\n", 
	    system_clock_reference, ((double)system_clock_reference)/27000000.0);

    
    if(system_clock_reference < prev_scr) {
      scr_discontinuity = 1;
      fprintf(stderr, "*** Backward scr discontinuity ******\n");
      fprintf(stderr, "system_clock_reference: [%6f s]\n", 
	      ((double)system_clock_reference)/27000000.0);
      fprintf(stderr, "prev system_clock_reference: [%6f s]\n", 
	      ((double)prev_scr)/27000000.0);
      
    } else if((system_clock_reference - prev_scr) > 2700000) {
      scr_discontinuity = 1;
      fprintf(stderr, "*** Forward scr discontinuity ******\n");
      fprintf(stderr, "system_clock_reference: [%6f s]\n", 
	      ((double)system_clock_reference)/27000000.0);
      fprintf(stderr, "prev system_clock_reference: [%6f s]\n", 
	      ((double)prev_scr)/27000000.0);
    }
    prev_scr = system_clock_reference;
    
    
    break;
  }
  if(nextbits(32) == MPEG2_PS_SYSTEM_HEADER_START_CODE) {
    system_header();
  }

  return version;

}

void push_stream_data(uint8_t stream_id, int len,
		      uint8_t PTS_DTS_flags,
		      uint64_t PTS,
		      uint64_t DTS)
{
  uint8_t *data = &buf[offs-(bits_left/8)];
  uint8_t subtype;

  //  fprintf(stderr, "pack nr: %d, stream_id: %02x (%02x), offs: %d, len: %d",
  //	  packnr, stream_id, data[0], offs-(bits_left/8), len);
  //  if(SCR_flags) {
  //    fprintf(stderr, ", SCR_base: %llx", SCR_base);
  //  }
  
  //  if(PTS_DTS_flags & 0x2) {
  //    fprintf(stderr, ", PTS: %llx", PTS);
  //  }

  //  fprintf(stderr, "\n");
  //fprintf(stderr, "stream_id: %02x\n", stream_id);

#ifdef STATS
  stat_n_packets++;
#endif //STATS

  DPRINTF(5, "bitsleft: %d\n", bits_left);

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    subtype = data[0];
  } else {
    subtype = 0;
  }
  if((!id_registered(stream_id, subtype)) && system_header_set) {
    register_id(stream_id, subtype);
  }
  
  if(id_stat(stream_id, subtype) == STREAM_DECODE) {
    //  if(stream_id == MPEG2_PRIVATE_STREAM_1) { 
    /*
      if((stream_id == 0xe0) || (stream_id == 0xc0) ||
      ((stream_id == MPEG2_PRIVATE_STREAM_1) &&
      (subtype == 0x80)) ||
      (stream_id == MPEG2_PRIVATE_STREAM_2)) {
    */
    //fprintf(stderr, "demux: put_in_q stream_id: %x %x\n",
    //      stream_id, subtype);

    if(msgqid != -1) {
      if(stream_id == MPEG2_PRIVATE_STREAM_1) {
	
	if((subtype >= 0x80) && (subtype < 0x90)) {
	  put_in_q(id_qaddr(stream_id, subtype), offs-(bits_left/8)+4, len-4,
		   PTS_DTS_flags, PTS, DTS);
	} else if((subtype >= 0x20) && (subtype < 0x40)) {
	  put_in_q(id_qaddr(stream_id, subtype), offs-(bits_left/8)+1, len-1,
		   PTS_DTS_flags, PTS, DTS);
	} else {
	  put_in_q(id_qaddr(stream_id, subtype), offs-(bits_left/8), len,
		   PTS_DTS_flags, PTS, DTS);
	}
      } else {
	put_in_q(id_qaddr(stream_id, subtype), offs-(bits_left/8), len,
		 PTS_DTS_flags, PTS, DTS);
      }
    } else {
      if(stream_id == MPEG2_PRIVATE_STREAM_1) {
	
	if((subtype >= 0x80) && (subtype < 0x90)) {
	  fwrite(&buf[offs-(bits_left/8)+4], len-4, 1,
		 id_file(stream_id, subtype));
	} else if((subtype >= 0x20) && (subtype < 0x40)) {
	  fprintf(stderr, "FILE: %d\n",	 id_file(stream_id, subtype));
	  fwrite(&buf[offs-(bits_left/8)+1], len-1, 1,
		 id_file(stream_id, subtype));
	  fflush(id_file(stream_id, subtype));
	} else {
	  fwrite(&buf[offs-(bits_left/8)], len, 1,
		 id_file(stream_id, subtype));
	}
      } else {
	fwrite(&buf[offs-(bits_left/8)], len, 1,
	       id_file(stream_id, subtype));
      }
      
    }
    
  }
  
  drop_bytes(len);
  
}


void PES_packet()
{
  uint16_t PES_packet_length;
  uint8_t stream_id;
  uint8_t PES_scrambling_control;
  uint8_t PES_priority;
  uint8_t data_alignment_indicator;
  uint8_t copyright;
  uint8_t original_or_copy;
  uint8_t PTS_DTS_flags = 0;
  uint8_t ESCR_flag;
  uint8_t ES_rate_flag;
  uint8_t DSM_trick_mode_flag;
  uint8_t additional_copy_info_flag;
  uint8_t PES_CRC_flag;
  uint8_t PES_extension_flag;
  uint8_t PES_header_data_length;
  
  uint64_t PTS = 0;
  uint64_t DTS = 0;
  uint64_t ESCR_base;
  uint32_t ESCR_extension;
  
  uint32_t ES_rate;
  uint8_t trick_mode_control;
  uint8_t field_id;
  uint8_t intra_slice_refresh;
  uint8_t frequency_truncation;

  uint8_t field_rep_cntrl;

  uint8_t additional_copy_info;
  uint16_t previous_PES_packet_CRC;
  uint8_t PES_private_data_flag = 0;
  uint8_t pack_header_field_flag = 0;
  uint8_t program_packet_sequence_counter_flag = 0;
  uint8_t program_packet_sequence_counter = 0;
  uint8_t P_STD_buffer_flag = 0;
  uint8_t PES_extension_field_flag = 0;
  uint8_t pack_field_length = 0;
  uint8_t PES_extension_field_length = 0;
  unsigned int bytes_read = 0;

  uint8_t original_stuff_length;
  uint8_t P_STD_buffer_scale;
  uint16_t P_STD_buffer_size;
  uint16_t N;
  
  DPRINTF(2, "PES_packet()\n");

  GETBITS(24 ,"packet_start_code_prefix");
  stream_id = GETBITS(8, "stream_id");
  PES_packet_length = GETBITS(16, "PES_packet_length");
  DPRINTF(2, "stream_id:         ");
  dprintf_stream_id(2, stream_id);
  DPRINTF(2, "PES_packet_length: %u\n", PES_packet_length);
  
  if((stream_id != MPEG2_PRIVATE_STREAM_2) &&
     (stream_id != MPEG2_PADDING_STREAM)) {

    DPRINTF(1, "packet() stream: %02x\n", stream_id);

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
      DPRINTF(1, "PES_packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);
    }

    if(PTS_DTS_flags == 0x3) {
      GETBITS(4, "0011");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      DPRINTF(1, "PES_packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);

      GETBITS(4, "0001");
      DTS                     = GETBITS(3, "DTS [32..30]")<<30;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [29..15]")<<15;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [14..0]");
      marker_bit();
      DPRINTF(1, "PES_packet() DTS: %llu [%6f s]\n", DTS, ((double)DTS)/90E3);
      
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
      DPRINTF(1, "ESCR_base: %llu [%6f s]\n", ESCR_base, ((double)ESCR_base)/90E3);

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
    
    N = (PES_header_data_length+3)-bytes_read;
    drop_bytes(N);
    bytes_read += N;

    N = PES_packet_length-bytes_read;
    
    //FIXME  kolla specen...    
    //FIXME  push pes..    

    
    push_stream_data(stream_id, N, PTS_DTS_flags, PTS, DTS);
    
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    push_stream_data(stream_id, PES_packet_length,
		     PTS_DTS_flags, PTS, DTS);
    //fprintf(stderr, "*PRIVATE_stream_2, %d\n", PES_packet_length);
  } else if(stream_id == MPEG2_PADDING_STREAM) {
    drop_bytes(PES_packet_length);
    //    push_stream_data(stream_id, PES_packet_length);
  }
}



// MPEG-1 packet

void packet()
{
  uint8_t stream_id;
  uint16_t packet_length;
  uint8_t P_STD_buffer_scale;
  uint16_t P_STD_buffer_size;
  uint64_t PTS = 0;
  uint64_t DTS = 0;
  int N;
  uint8_t pts_dts_flags = 0;

  GETBITS(24 ,"packet_start_code_prefix");
  stream_id = GETBITS(8, "stream_id");
  packet_length = GETBITS(16, "packet_length");
  N = packet_length;
  
  if(stream_id != MPEG2_PRIVATE_STREAM_2) {
    
    DPRINTF(1, "packet() stream: %02x\n", stream_id);
    while(nextbits(8) == 0xff) {
      GETBITS(8, "stuffing byte");
      N--;
    }
    
    if(nextbits(2) == 0x1) {
      GETBITS(2, "01");
      P_STD_buffer_scale = GETBITS(1, "P_STD_buffer_scale");
      P_STD_buffer_size = GETBITS(13, "P_STD_buffer_size");
      
      N -= 2;
    }
    if(nextbits(4) == 0x2) {
      pts_dts_flags = 0x2;
      GETBITS(4, "0010");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      
      DPRINTF(1, "packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);
      
      N -= 5;
    } else if(nextbits(4) == 0x3) {
      pts_dts_flags = 0x3;
      GETBITS(4, "0011");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      DPRINTF(1, "packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);

      GETBITS(4, "0001");
      DTS                     = GETBITS(3, "DTS [32..30]")<<30;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [29..15]")<<15;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [14..0]");
      marker_bit();
      DPRINTF(1, "packet() DTS: %llu [%6f s]\n", DTS, ((double)DTS)/90E3);

      N -= 10;
    } else {
      
      GETBITS(8, "00001111");
      N--;
    }
  }

  push_stream_data(stream_id, N, pts_dts_flags, PTS, DTS);
  
}



void pack()
{
  uint32_t start_code;
  uint8_t stream_id;
  uint8_t is_PES = 0;
  int mpeg_version;
  

  SCR_flags = 0;

  /* TODOD clean up */
  if(msgqid != -1) {
    if(off_to != -1) {
      if(off_to <= offs-(bits_left/8)) {
	fprintf(stderr, "demux: off_to %d offs %d pack\n", off_to, offs);
	off_to = -1;
	wait_for_msg(CMD_CTRL_CMD);
      }
    }
    if(off_from != -1) {
      fprintf(stderr, "demux: off_from pack\n");
      offs = off_from;
      bits_left = 64;
      off_from = -1;
      GETBITS(32, "skip1");
      GETBITS(32, "skip2");
    }

    chk_for_msg();
  }
  
  mpeg_version = pack_header();
  switch(mpeg_version) {
  case MPEG1:

    /* TODO clean up */
    if(msgqid != -1) {
      if(off_to != -1) {
	if(off_to <= offs-(bits_left/8)) {
	  fprintf(stderr, "demux: off_to %d offs %d mpeg1\n", off_to, offs);
	  off_to = -1;
	  wait_for_msg(CMD_CTRL_CMD);
	}
      }
      if(off_from != -1) {
	fprintf(stderr, "demux: off_from mpeg1\n");
	offs = off_from;
	bits_left = 64;
	off_from = -1;
	GETBITS(32, "skip1");
	GETBITS(32, "skip2");
      }
      chk_for_msg();
    }

    next_start_code();
    while(nextbits(32) >= 0x000001BC) {
      packet();

      /* TODO clean up */
      if(msgqid != -1) {
	if(off_to != -1) {
	  if(off_to <= offs-(bits_left/8)) {
	    fprintf(stderr, "demux: off_to %d offs %d packet\n", off_to, offs);
	    off_to = -1;
	    wait_for_msg(CMD_CTRL_CMD);
	  }
	}
	if(off_from != -1) {
	  fprintf(stderr, "demux: off_from packet\n");
	  offs = off_from;
	  bits_left = 64;
	  off_from = -1;
	  GETBITS(32, "skip1");
	  GETBITS(32, "skip2");
	}
	chk_for_msg();
      }

      next_start_code();
    }
    break;
  case MPEG2:
    while((((start_code = nextbits(32))>>8)&0x00ffffff) ==
	  MPEG2_PES_PACKET_START_CODE_PREFIX) {
      stream_id = (start_code&0xff);
      
      is_PES = 0;
      if((stream_id >= 0xc0) && (stream_id < 0xe0)) {
	/* ISO/IEC 11172-3/13818-3 audio stream */
	is_PES = 1;
	
      } else if((stream_id >= 0xe0) && (stream_id < 0xf0)) {
	/* ISO/IEC 11172-3/13818-3 video stream */
	is_PES = 1;

      } else {
	switch(stream_id) {
	case 0xBC:
	  /* program stream map */
	  fprintf(stderr, "Program Stream map\n");
	case 0xBD:
	  /* private stream 1 */
	case 0xBE:
	  /* padding stream */
	case 0xBF:
	  /* private stream 2 */
	  is_PES = 1;
	  break;
	case 0xBA:
				//fprintf(stderr, "Pack Start Code\n");
	  is_PES = 0;
	  break;
	default:
	  is_PES = 0;
	  fprintf(stderr, "unknown stream_id: 0x%02x\n", stream_id);
	  break;
	}
      }
      if(!is_PES) {
	break;
      }
      
      PES_packet();
      SCR_flags = 0;

      /* TODO clean up */
      if(msgqid != -1) {
	if(off_to != -1) {
	  if(off_to <= offs-(bits_left/8)) {
	    fprintf(stderr, "demux: off_to %d offs %d mpeg2\n", off_to, offs);
	    off_to = -1;
	    wait_for_msg(CMD_CTRL_CMD);
	  }
	}
	if(off_from != -1) {
	  fprintf(stderr, "demux: off_from mpeg2\n");
	  offs = off_from;
	  bits_left = 64;
	  off_from = -1;
	  GETBITS(32, "skip1");
	  GETBITS(32, "skip2");
	}
	
	chk_for_msg();
      }
    }
    break;
  }

  packnr++;

}


void MPEG2_program_stream()
{

  DPRINTF(2,"MPEG2_program_stream()\n");
  do {
    pack();
  } while(nextbits(32) == MPEG2_PS_PACK_START_CODE); 
  if(GETBITS(32, "MPEG2_PS_PROGRAM_END_CODE") == MPEG2_PS_PROGRAM_END_CODE) {
    DPRINTF(1, "MPEG Program End\n");
    //system_header_set = 0;
  } else {
    synced = 0;
    fprintf(stderr, "demux: *** Lost Sync\n");
    fprintf(stderr, "demux: *** at offset: %u bytes\n", offs);
  }
}


void segvhandler (int id)
{
  // If we got here we ran off the end of a mmapped file.
  // Or it's some other bug. Remove the sigaction call in 
  // main if you suspect this.
  if (offs > infilelen) {
    DPRINTF(1, "Reached end of mmapped file.\n");
    // TODO
    //
    // loadinputfile( next_file );
    // goto while(1)-loop in main.;
  } else {
    fprintf(stderr, "Segmentation fault. Idiot.\n");
  }

#ifdef STATS
  printf("\n----------------------------------------------\n");
  printf("Unaligned video packets:\t\t%u\n", 
	 stat_video_unaligned_packet_offset);
  printf("Video packets with unaligned ends:\t%u\n",
	 stat_video_unaligned_packet_end);
  printf("Total video packets:\t\t\t%u\n", 
	 stat_video_n_packets);
  printf("Total audio packets:\t\t\t%u\n", 
	 stat_audio_n_packets);
  printf("Total subpicture packets:\t\t%u\n", 
	 stat_subpicture_n_packets);
  printf("Total packets:\t\t\t\t%u\n",
	 stat_n_packets);
#endif //STATS

  exit(0);
}

void loadinputfile(char *infilename)
{
  struct stat statbuf;
  int rv;

#if 0
  struct {
    uint32_t type;
    struct load_file_packet body;
  } lf_pack;
#endif

  infilefd = open(infilename, O_RDONLY);
  if(infilefd == -1) {
    perror(infilename);
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
    exit(1);
  }
  infilelen = statbuf.st_size;
#ifdef HAVE_MADVISE
  rv = madvise(buf, infilelen, MADV_SEQUENTIAL);
  if(rv == -1) {
    perror("madvise");
    exit(1);
  }
#endif
  DPRINTF(1, "All mmap systems ok!\n");

}


char *stream_opts[] = {
#define OPT_STREAM_NR    0
  "nr",
#define OPT_FILE         1
  "file",
  NULL
};


int main(int argc, char **argv)
{
  int c, rv; 
  struct sigaction sig;
  char *options;
  char *opt_value;
  int stream_nr;
  char *file;
  int stream_id;
  int lost_sync = 1;
  
  program_name = argv[0];
  
  init_id_reg();
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "v:a:s:i:d:m:o:h?")) != EOF) {
    switch (c) {
    case 'v':
      stream_nr = -1;
      file = NULL;
      options = optarg;
      while (*options != '\0') {
	switch(getsubopt(&options, stream_opts, &opt_value)) {
	case OPT_STREAM_NR:
	  if(opt_value == NULL) {
	    exit(-1);
	  }
	  
	  stream_nr = atoi(opt_value);
	  break;
	case OPT_FILE:
	  if(opt_value == NULL) {
	    exit(-1);
	  }
	  
	  file = opt_value;
	  break;
	default:
	  fprintf(stderr, "Unknown suboption\n");
	  exit(-1);
	  break;
	}
      }
      
      if((stream_nr == -1)) {
	fprintf(stderr, "Missing suboptions\n");
	exit(-1);
      } 
      
      if((stream_nr < 0) && (stream_nr > 0xf)) {
	fprintf(stderr, "Invalid stream nr\n");
	exit(-1);
      }
      
      stream_id = (0xe0 | stream_nr);
      
      if(file != NULL) {
	video_file = fopen(file,"w");
	if(!video_file) {
	  perror(optarg);
	  exit(1);
	}

	id_add(stream_id, 0, STREAM_DECODE, 0, NULL, video_file);
	
      } else {
	fprintf(stderr, "Video stream %d disabled\n", stream_nr);
	
	id_add(stream_id, 0, STREAM_DISCARD, 0, NULL, NULL);
      }
	
	  

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

      stream_nr = -1;
      file = NULL;
      options = optarg;
      while (*options != '\0') {
	switch(getsubopt(&options, stream_opts, &opt_value)) {
	case OPT_STREAM_NR:
	  if(opt_value == NULL) {
	    exit(-1);
	  }
	  
	  stream_nr = atoi(opt_value);
	  break;
	case OPT_FILE:
	  if(opt_value == NULL) {
	    exit(-1);
	  }
	  
	  file = opt_value;
	  break;
	default:
	  fprintf(stderr, "Unknown suboption: %s\n", options);
	  exit(-1);
	  break;
	}
      }
      
      if((stream_nr == -1)) {
	fprintf(stderr, "Missing suboptions\n");
	exit(-1);
      } 
      
      if((stream_nr < 0) && (stream_nr > 0x1f)) {
	fprintf(stderr, "Invalid stream nr\n");
	exit(-1);
      }
      
      stream_id = (0x20 | stream_nr);
      
      if(file != NULL) {
	subtitle_file = fopen(file,"w");
	if(!subtitle_file) {
	  perror(optarg);
	  exit(1);
	}

	id_add(MPEG2_PRIVATE_STREAM_1, stream_id, STREAM_DECODE, 0, NULL, subtitle_file);
	
      } else {
	fprintf(stderr, "Subtitle stream %d disabled\n", stream_nr);
	
	id_add(MPEG2_PRIVATE_STREAM_1, stream_id, STREAM_DISCARD, 0, NULL, NULL);
      }


      break;
    case 'd':
      debug = atoi(optarg);
      break;
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'o':
      offs = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }

  if(msgqid == -1) {
    if(argc - optind != 1){
      usage();
      return 1;
    }
  }


  if(msgqid != -1) {
    init_id_reg();
    /* wait for load_file command */
    wait_for_msg(CMD_FILE_OPEN);
  } else {
    loadinputfile(argv[optind]);
  }
  // Setup signal handler for SEGV, to detect that we ran 
  // out of file without a test.
  
  sig.sa_handler = segvhandler;
  rv = sigaction(SIGSEGV, &sig, NULL);
  if(rv == -1) {
    perror("sighandler");
    exit(1);
  }
  DPRINTF(1, "Signal handler installed!\n");
  
  
  GETBITS(32, "skip");
  GETBITS(32, "skip");

  while(1) {
    //    fprintf(stderr, "Searching for Program Stream\n");
    if(nextbits(32) == MPEG2_PS_PACK_START_CODE) {
      if(!synced) {
	synced = 1;
	fprintf(stderr, "demux: Found Program Stream\n");
      }
      MPEG2_program_stream();
      lost_sync = 1;
    } else if(nextbits(32) == MPEG2_VS_SEQUENCE_HEADER_CODE) {
      fprintf(stderr, "demux: Found Video Sequence Header, seems to be an Elementary Stream (video)\n");
      
      lost_sync = 1;
    }
    GETBITS(8, "resync");
    DPRINTF(1, "resyncing");
    if(lost_sync) {
      lost_sync = 0;
      fprintf(stderr, "demux: Lost sync, resyncing\n");
    }
  }
}

/*
int create_shm(int size)
{
  
  if((shmid =
      shmget(IPC_PRIVATE, picture_buffers_size, IPC_CREAT | 0600)) = -1) {
    perror("shmget shmid");
    exit(-1);
  }

  shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU);
  if(shmaddr == -1) {
    perror("shmat");
    exit(-1);
  }

  return 0;
}  
*/


int register_id(uint8_t id, int subtype)
{
  /* TODO clean up */
  msg_t msg;
  cmd_t *cmd;
  data_buf_head_t *data_buf_head;

  if(msgqid != -1) {
    
    data_buf_head = (data_buf_head_t *)data_buf_addr;
    
    cmd = (cmd_t *)&msg.mtext;
    
    /* send create decoder request msg*/
    msg.mtype = MTYPE_CTL;
    cmd->cmdtype = CMD_DEMUX_NEW_STREAM;
    cmd->cmd.new_stream.stream_id = id;
    cmd->cmd.new_stream.subtype = subtype; // different private streams
    cmd->cmd.new_stream.nr_of_elems = 300;
    cmd->cmd.new_stream.data_buf_shmid = data_buf_head->shmid;
    send_msg(&msg, sizeof(cmdtype_t)+sizeof(cmd_new_stream_t));
    
    /* wait for answer */
    
    while(!id_registered(id, subtype)) {
      //fprintf(stderr, "waiting for answer\n");
      wait_for_msg(CMD_DEMUX_STREAM_BUFFER);
    }
    //fprintf(stderr, "got answer\n");
    //TODO maybe not set buffer in wait_for_msg ?
  } else {
    /* TODO fix */
    /*
      id_add(...);

    */
  }
  
  return 0;
}

int id_stat(uint8_t id, uint8_t subtype)
{
    if(id != MPEG2_PRIVATE_STREAM_1) {
      return id_reg[id].state;
    } else {
      return id_reg_ps1[subtype].state;
    }
}

char *id_qaddr(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    return id_reg[id].shmaddr;
  } else {
    return id_reg_ps1[subtype].shmaddr;
  }
}

FILE *id_file(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    return id_reg[id].file;
  } else {
    return id_reg_ps1[subtype].file;
  }
}
  
int id_registered(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    if(id_reg[id].state != STREAM_NOT_REGISTERED) {
      return 1;
    }
  } else {
    if(id_reg_ps1[subtype].state != STREAM_NOT_REGISTERED) {
      return 1;
    }
  }

  return 0;
}

void id_add(uint8_t stream_id, uint8_t subtype, stream_state_t state, int shmid, char *shmaddr, FILE *file)
{
    if(stream_id != MPEG2_PRIVATE_STREAM_1) {
      id_reg[stream_id].shmid = shmid;
      id_reg[stream_id].shmaddr = shmaddr;
      id_reg[stream_id].state = state;
      id_reg[stream_id].file = file;
      
    } else {
      id_reg_ps1[subtype].shmid = shmid;
      id_reg_ps1[subtype].shmaddr = shmaddr;
      id_reg_ps1[subtype].state = state;
      id_reg_ps1[subtype].file = file;
      
    }
}
  

int init_id_reg()
{
  int n;
  
  for(n = 0; n < 256; n++) {
    id_reg[n].state = STREAM_NOT_REGISTERED;
    id_reg[n].file = NULL;
  }

  for(n = 0; n < 256; n++) {
    id_reg_ps1[n].state = STREAM_NOT_REGISTERED;
    id_reg_ps1[n].file = NULL;
  }
  
  id_reg[0xBE].state = STREAM_DISCARD; //padding stream
  
  return 0;
}



int chk_for_msg(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  
  if(msgrcv(msgqid, &msg, sizeof(msg.mtext), MTYPE_DEMUX, IPC_NOWAIT) == -1) {
    if(errno == ENOMSG) {
      //fprintf(stderr ,"demux: no msg in q\n");
      return 0;
    }
    perror("msgrcv");
    return -1;
  } else {
    fprintf(stderr, "demux: got msg\n");
    eval_msg(cmd);
  }
  
  return 1;
}

int wait_for_msg(cmdtype_t cmdtype)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  cmd->cmdtype = CMD_NONE;
  
  while(cmd->cmdtype != cmdtype) {
    if(msgrcv(msgqid, &msg, sizeof(msg.mtext), MTYPE_DEMUX, 0) == -1) {
      perror("msgrcv");
      return -1;
    } else {
      //fprintf(stderr, "demux: got msg\n");
      eval_msg(cmd);
    }
  }
  return 0;
}

int eval_msg(cmd_t *cmd)
{
  
  switch(cmd->cmdtype) {
  case CMD_FILE_OPEN:
    fprintf(stderr, "demux: open file: %s\n", cmd->cmd.file_open.file);
    loadinputfile(cmd->cmd.file_open.file);
    break;
  case CMD_DEMUX_GNT_BUFFER:
    fprintf(stderr, "demux: got buffer id %d, size %d\n",
	    cmd->cmd.gnt_buffer.shmid,
	    cmd->cmd.gnt_buffer.size);
    attach_buffer(cmd->cmd.gnt_buffer.shmid,
		  cmd->cmd.gnt_buffer.size);
    break;
  case CMD_DEMUX_STREAM_BUFFER:
    fprintf(stderr, "demux: got stream %x, %x buffer \n",
	    cmd->cmd.stream_buffer.stream_id,
	    cmd->cmd.stream_buffer.subtype);
    attach_decoder_buffer(cmd->cmd.stream_buffer.stream_id,
			  cmd->cmd.stream_buffer.subtype,
			  cmd->cmd.stream_buffer.q_shmid);

    break;
  case CMD_CTRL_CMD:
    {
      static ctrlcmd_t prevcmd = CTRLCMD_PLAY;
      switch(cmd->cmd.ctrl_cmd.ctrlcmd) {
      case CTRLCMD_STOP:
	if(prevcmd != CTRLCMD_STOP) {
	  wait_for_msg(CMD_ALL);
	}
	break;
      case CTRLCMD_PLAY:
	break;
      case CTRLCMD_PLAY_FROM:
	off_from = cmd->cmd.ctrl_cmd.off_from;
	off_to = -1;
	break;
      case CTRLCMD_PLAY_TO:
	off_from = -1;
	off_to = cmd->cmd.ctrl_cmd.off_to;
	break;
      case CTRLCMD_PLAY_FROM_TO:
	off_from = cmd->cmd.ctrl_cmd.off_from;
	off_to = cmd->cmd.ctrl_cmd.off_to;
	break;
      default:
	break;
      }
    }
    break;
  default:
    fprintf(stderr, "demux: unrecognized command\n");
    return -1;
    break;
  }
  
  return 0;
}


  
int attach_decoder_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;

  fprintf(stderr, "demux: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("attach_decoder_buffer(), shmat()");
      return -1;
    }
    id_add(stream_id, subtype, STREAM_DECODE, shmid, shmaddr, NULL);
    
  } else {
    id_add(stream_id, subtype, STREAM_DISCARD, 0, NULL, NULL);
  }
    
  return 0;
  
}


int attach_buffer(int shmid, int size)
{
  char *shmaddr;
  data_buf_head_t *data_buf_head;
  data_elem_t *data_elems;
  int n;
  
  fprintf(stderr, "demux: attach_buffer() shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("attach_buffer(), shmat()");
      return -1;
    }

    data_buf_addr = shmaddr;
    data_buf_head = (data_buf_head_t *)data_buf_addr;
    data_buf_head->shmid = shmid;
    data_buf_head->nr_of_dataelems = 900;
    data_buf_head->write_nr = 0;
    
    data_elems = (data_elem_t *)(data_buf_addr+sizeof(data_buf_head_t));
    for(n = 0; n < data_buf_head->nr_of_dataelems; n++) {
      data_elems[n].in_use = 0;
    }
    
    

  } else {
    return -1;
  }
    
  return 0;
  
}


int get_buffer(int size)
{
  msg_t msg;
  cmd_t *cmd;
  
  size+= sizeof(data_buf_head_t)+900*sizeof(data_elem_t);

  msg.mtype = MTYPE_CTL;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DEMUX_REQ_BUFFER;
  cmd->cmd.req_buffer.size = size;
  
  if(msgsnd(msgqid, &msg,
	    sizeof(cmdtype_t)+sizeof(cmd_req_buffer_t),
	    0) == -1) {
    perror("msgsnd");
  }

  wait_for_msg(CMD_DEMUX_GNT_BUFFER);
  
  return 0;
}

int send_msg(msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("ctrl: msgsnd1");
    return -1;
  }
  return 0;
}





int put_in_q(char *q_addr, int off, int len, uint8_t PTS_DTS_flags,
	     uint64_t PTS, uint64_t DTS)
{
  q_head_t *q_head = NULL;
  q_elem_t *q_elem;
  data_buf_head_t *data_buf_head;
  data_elem_t *data_elems;
  int data_elem_nr;
  int elem;
  int n;
  int nr_waits = 0;
  
  static int scr_nr = 0;
  
  /* First find a entry in the big shared buffer pool. */
  
  data_buf_head = (data_buf_head_t *)data_buf_addr;
  data_elems = (data_elem_t *)(data_buf_addr+sizeof(data_buf_head_t));
  data_elem_nr = data_buf_head->write_nr;
  
  /* It's a circular list and it the next packet is still in use we need
   * to wait for it to be become available. 
   * We migh also do someting smarter here but provided that we have a
   * large enough buffer this will not be common.
   */
  while(data_elems[data_elem_nr].in_use) {
    nr_waits++;
    /* If this element is in use we have to wait untill it is released.
     * We know which consumer q we need to wait on. 
     * Normaly a wait on a consumer queue is because there are no free buf 
     * entries (i.e the wait should suspend on the first try). Here we
     * instead want to wait untill the number of empty buffers changes
     * (i.e. not necessarily between 0 and 1). 
     * Because of this we will loop and call ip_sem_wait untill it suspends
     * us or the buffer is no longer in use. 
     * This will effectivly lower the maximum number of buffers untill
     * we are at the 'normal case' of waiting for there to be 1 free buffer.
     * Then we need to check if it was this specific buffer that was consumed
     * if not then try again.
     */
    /* This is a contention case, normally we shouldn't get here */
    
    //fprintf(stderr, "demux: put_in_q(), elem %d in use\n", data_elem_nr);
    
    q_head = (q_head_t *)data_elems[data_elem_nr].q_addr;
    
    if(ip_sem_wait(&q_head->queue, BUFS_EMPTY) == -1) {
      perror("demux: put_in_q(), sem_wait()");
      exit(1); // XXX 
    }
  }
  /* Now the element should be free. 'paranoia check' */
  if(data_elems[data_elem_nr].in_use) {
    fprintf(stderr, "demux: somethings wrong, elem %d still in use\n",
	    data_elem_nr);
  }
  
  /* If decremented earlier then increment it now to restor the normal 
   * maximum free buffers for the queue. */
  for(n = 0; n < nr_waits; n++) {
    if(ip_sem_post(&q_head->queue, BUFS_EMPTY) == -1) {
      perror("demux: put_in_q(), sem_post()");
      exit(1); // XXX 
    }
  }
  
  
  
  
  if(PTS_DTS_flags & 0x2) {
    DPRINTF(1, "PTS: %llu.%09llu\n", 
	    PTS/90000, (PTS%90000)*(1000000000/90000));
  }

  data_elems[data_elem_nr].PTS_DTS_flags = PTS_DTS_flags;
  data_elems[data_elem_nr].PTS = PTS;
  data_elems[data_elem_nr].DTS = DTS;
  data_elems[data_elem_nr].SCR_base = SCR_base;
  data_elems[data_elem_nr].SCR_ext = SCR_ext;
  data_elems[data_elem_nr].SCR_flags = SCR_flags;
  data_elems[data_elem_nr].off = off;
  data_elems[data_elem_nr].len = len;
  data_elems[data_elem_nr].q_addr = q_addr;
  data_elems[data_elem_nr].in_use = 1;

  if(scr_discontinuity) {
    scr_discontinuity = 0;
    scr_nr = (scr_nr+1)%16;
    fprintf(stderr, "changed to scr_nr: %d\n", scr_nr);
  }

  data_elems[data_elem_nr].scr_nr = scr_nr;

  data_buf_head->write_nr = 
    (data_buf_head->write_nr+1) % data_buf_head->nr_of_dataelems;

  //  fprintf(stderr, "demux: put_in_q() off: %d, len %d\n", off, len);
  
  
  
  
  /* Now put the data buffer in the right consumer queue. */
  q_head = (q_head_t *)q_addr;
  q_elem = (q_elem_t *)(q_addr+sizeof(q_head_t));
  elem = q_head->write_nr;
  
  /* Make sure that there is room for it. */
  if(ip_sem_wait(&q_head->queue, BUFS_EMPTY) == -1) {
    perror("demux: put_in_q(), sem_wait()");
    exit(1); // XXX 
  }

  q_elem[elem].data_elem_index = data_elem_nr;
  /*
  q_elem[elem].PTS_DTS_flags = PTS_DTS_flags;
  q_elem[elem].PTS = PTS;
  q_elem[elem].DTS = DTS;
  q_elem[elem].SCR_base = SCR_base;
  q_elem[elem].SCR_ext = SCR_ext;
  q_elem[elem].SCR_flags = SCR_flags;
  q_elem[elem].off = off;
  q_elem[elem].len = len;
  */
  
  q_head->write_nr = (q_head->write_nr+1)%q_head->nr_of_qelems;
  
  /* Let the consumber know that there is data available for consumption. */
  if(ip_sem_post(&q_head->queue, BUFS_FULL) == -1) {
    perror("demux: put_in_q(), sem_post()");
    exit(1); // XXX 
  }
  
  return 0;
}
