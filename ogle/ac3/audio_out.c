/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2002 Björn Englund, Håkan Hjort
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
#include <unistd.h>
#include <inttypes.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include <ogle/msgevents.h>

#include "debug_print.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"

#include "parse_config.h"
#include "decode.h"
/* temporary */
int flush_audio(void);

static int get_q();
static int attach_ctrl_shm(int shmid);
static int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);

static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);

static int ctrl_data_shmid;

ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

static int msgqid = -1;
static MsgEventQ_t *msgq;

static int flush_to_scrid = -1;
static int prev_scr_nr = 0;

char *program_name;

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", program_name);
}

adec_handle_t *ahandle;

int main(int argc, char *argv[])
{
  MsgEvent_t ev;
  int c;
  
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
      msgqid = atoi(optarg);
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
  
  if(parse_config() == -2) {
    FATAL("Couldn't read config files\n");
    exit(1);
  }


  if(msgqid != -1) {
    if((msgq = MsgOpen(msgqid)) == NULL) {
      FATAL("couldn't get message q\n");
      exit(1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_AC3_AUDIO;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      DPRINTF(1, "a52: register capabilities\n");
    }
    
    while(ev.type != MsgEventQDecodeStreamBuf) {
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
    }

  } else {
    FATAL("what? need a msgid\n");
  }


  ahandle = adec_init(AudioType_A52);

  while(1) {
    get_q();
  }

  return 0;
}


static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    DPRINTF(1, "a52: got notify\n");
    break;
  case MsgEventQFlushData:
    DPRINTF(1, "a52: got flush\n");
    flush_to_scrid = ev->flushdata.to_scrid;
    break;
  case MsgEventQDecodeStreamBuf:
    DPRINTF(1, "a52: got stream %x, %x buffer \n",
	    ev->decodestreambuf.stream_id,
	    ev->decodestreambuf.subtype);
    attach_stream_buffer(ev->decodestreambuf.stream_id,
			  ev->decodestreambuf.subtype,
			  ev->decodestreambuf.q_shmid);
    
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  case MsgEventQSpeed:
    if(ev->speed.speed == 1.0) {
      set_speed(&ctrl_time[prev_scr_nr].sync_point, ev->speed.speed);
    } else {
      if(ctrl_time[prev_scr_nr].sync_point.speed == 1.0) {
	set_speed(&ctrl_time[prev_scr_nr].sync_point, ev->speed.speed);
	if((ctrl_time[prev_scr_nr].sync_master == SYNC_AUDIO)) {
	  ctrl_time[prev_scr_nr].sync_master = SYNC_NONE;
	}
      }
    }
    break;
  default:
    DNOTE("unrecognized event type: %d\n", ev->type);
    break;
  }
}




int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("a52: attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }    
  
  return 0;
}

int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;
  q_head_t *q_head;

  //DNOTE("a52_decoder: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("ac52_decoder: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("a52_decoder: attach_data_buffer(), shmat()");
      return -1;
    }
    
    data_buf_shmid = shmid;
    data_buf_shmaddr = shmaddr;
  }    

  return 0;
}




int get_q()
{
  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  data_elem_t *data_elems;
  data_elem_t *data_elem;
  int elem;
  uint8_t *data_buffer;
  uint8_t PTS_DTS_flags;
  uint64_t PTS;
  uint64_t DTS;
  int scr_nr;
  int off;
  int len;
  int pts_offset;
  MsgEvent_t ev;
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;

  while(MsgCheckEvent(msgq, &ev) != -1) {
    handle_events(msgq, &ev);
  }
  
  if(!q_elems[elem].in_use) {
    q_head->reader_requests_notification = 1;
    
    while(!q_elems[elem].in_use) {
      DPRINTF(1, "a52: waiting for notification1\n");
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
    }
  }

  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_buffer = data_buf_shmaddr + data_head->buffer_start_offset;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];
  
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  PTS = data_elem->PTS;
  DTS = data_elem->DTS;
  scr_nr = data_elem->scr_nr;
  /*
    p[0] is the number of frames of audio which have a sync code in this pack
    p[1]<<8 | p[2] is the starting index of the frame which 
      the PTS value belong to
  */
#if 0
  {
    uint8_t *tbuf;
    uint8_t *lbuf;  
    uint8_t *cbuf;  
    int toff, tlen, pts_off;
    toff = data_elem->off;
    tbuf = data_buffer;
    tlen = data_elem->len-3;
    lbuf = tbuf+toff+3;
    cbuf = data_buffer+toff;
    
    fprintf(stderr, "%02x %02x %02x %02x, %02x %02x\n",
	    cbuf[-1], cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4]);

    pts_off = (data_buffer+toff)[1]<<8 | (data_buffer+toff)[2];

    fprintf(stderr, "buflen: %d, ", tlen); 
    fprintf(stderr, "nr of frames with sync: %d, offset to pts frame: %d\n",
	    (data_buffer+toff)[0], pts_off);

    if(pts_off != 0) {
      pts_off--;
      if(tlen-pts_off >= 7) {
	int length, coded_flags, sample_rate, bit_rate;
	
	length = a52_syncinfo(tbuf+toff+3+pts_off,
			      &coded_flags, &sample_rate, &bit_rate);
	fprintf(stderr, "%02x %02x %02x, %02x %02x %02x %02x %02x %02x %02x, %02x %02x %02x\n", 
		(lbuf+pts_off)[-3],
		(lbuf+pts_off)[-2],
		(lbuf+pts_off)[-1],
		(lbuf+pts_off)[0],
		(lbuf+pts_off)[1],
		(lbuf+pts_off)[2],
		(lbuf+pts_off)[3],
		(lbuf+pts_off)[4],
		(lbuf+pts_off)[5],
		(lbuf+pts_off)[6],
		(lbuf+pts_off)[7],
		(lbuf+pts_off)[8],
		(lbuf+pts_off)[9]);
	
	fprintf(stderr, "frame_len: %d\n", length);
      } else {
	fprintf(stderr, "not enough headerbytes\n");
      }
    } else {
      fprintf(stderr, "**** no pts offset *****\n");
    }
  }
#endif
  
  off = data_elem->off + 3;
  len = data_elem->len - 3;
	  
  pts_offset = (data_buffer+off)[-2]<<8 | (data_buffer+off)[-1];
  //  fprintf(stderr, "pts_offset: %d\n", pts_offset);
  if(pts_offset ==  0) {
    fprintf(stderr, "**pts_offset: 0, report bug\n");
  } else {
    pts_offset--;
  }
  
  if(flush_to_scrid != -1) {
    if(ctrl_time[scr_nr].scr_id < flush_to_scrid) {

      q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
      
      // release elem
      data_elem->in_use = 0;
      q_elems[elem].in_use = 0;
      
      if(q_head->writer_requests_notification) {
	q_head->writer_requests_notification = 0;
	ev.type = MsgEventQNotify;
	if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
	  WARNING("couldn't send notification\n");
	}
      }
      fprintf(stderr, "flushed, packet droped on scr\n");
      return 0;
    } else {
      //TODO flush audio now that we have flushed all packets
      fprintf(stderr, "flushing audio driver\n");
      flush_audio();
      fprintf(stderr, "flushed audio\n");
      flush_to_scrid = -1;
    }
  }
  
  prev_scr_nr = scr_nr;
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
  
  if(ctrl_data->speed == 1.0) {
    adec_decode(ahandle, data_buffer+off, len, pts_offset, PTS, scr_nr);
    //decode_a52_data(data_buffer+off, len, pts_offset, PTS, scr_nr);
    //a52_decode_data(data_buffer+off, data_buffer+off+len);  
  }

  // release elem
  data_elem->in_use = 0;
  q_elems[elem].in_use = 0;
  
  if(q_head->writer_requests_notification) {
    q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
      WARNING("couldn't send notification\n");
    }
  }

  return 0;
}


int flush_audio(void)
{
  //flush sound_card
  adec_flush(ahandle);
#if 0
  //reset decoder
  audio_decoder_flush();
  
  //reset output_buffer
  audio_play_flush();
  output_samples_ptr = output_buffer;
#endif
  return 0;
}
#if 0
play_samples(samples)
{
 if(samples_in_buf > 200 ms)
   wait(40 ms)
 play_time = cur_time+time_in_buf
 if(play_time != pts)
   cur_time = play_time;
 ao_play(samples);

}
#endif
/*
  decode one ac3 frame, convert from float to int, ao_play

  a52_init(mm_accel)
     - inits and returns sample output buffer
  a52_syncinfo(*buf, *flags, *sample_rate, *bit_rate)
     - buf must be at least 7 bytes
     - returns the size of the frame or 0
     - fills flags, sample_rate, bit_rate.
     - size is 128 - 3840
  a52_frame(*state, *buf, *flags, *level, bias)
     - buf shall point to the beginning of the full coded frame
     - flags input is the supported speaker config, returns what will be used
     -  level, bias strange things
  a52_block(*state, *sample)
     - shall be called 6 times per frame
     - return data to sample each time


  samples = a52_init
  card_nr_channels
  card_channels[card_nr_channels] = { FL, FR, FC, SL, SR, LFE }
  codebuf[3840]
  buf_ptr = code_buf

  get_data => start, len
  
  cp(start, len, buf_ptr)
  buf_ptr+=len;

  while(buf_ptr-code_buf >= 7) {
   // we have enough bytes for header
   len = a52_syncinfo(code_buf, &chnls_in_stream, &sample_rate, &bit_rate);
   if(len == 0) {
    //shift 1 byte to left
    for(n = buf_code; n < buf_ptr; n++) {
     *n = *(n+1);
    }
    buf_ptr--;
    continue;
   } else {
     cur_len = buf_ptr-code_buf
     total_len = len;
     needed_len = total_len - cur_len;
     if(needed_len > 0) {
       return needed_len;
     }
     chnls = wanted_chnls;
     a52_frame(&state, code_buf, &chnls, level, bias);
     for(i = 0; i < 6; i++) {
       a52_block(&state, samples);
       float_to_int(samples, ac3_chnls, int_samples, card_chnls);
       play_sample(int_samples);
     }

  }
  return 0;  // less than 7 bytes input data
}


 */
