/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include <ogle/msgevents.h>

#include "debug_print.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"

#include <a52dec/a52.h>
#include <libao/audio_out.h>
#include <a52dec/mm_accel.h>

#include "parse_config.h"

/* A/52 */
static ao_instance_t * output = NULL;
static sample_t * samples;
static int disable_dynrng = 0;
static clocktime_t a52_decode_data(uint8_t *start, uint8_t *end);



static int get_q();
static int attach_ctrl_shm(int shmid);
static int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);

static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);

static int ctrl_data_shmid;
static ctrl_data_t *ctrl_data;
static ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

static int msgqid = -1;
static MsgEventQ_t *msgq;

static int flush_to_scrid = -1;
static int prev_scr_nr = 0;

static int speaker_flags = -1;

char *program_name;

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", program_name);
}

static int get_speaker_flags(void)
{
  int front, rear, sub;
  int result = 0;
  
  front = get_num_front_speakers();
  rear = get_num_rear_speakers();
  sub = get_num_sub_speakers();

  if(front == 2 && rear == 0) {
    result = A52_STEREO;
    /* if(dolby) result = A52_DOLBY; */
  } else if(front == 3 && rear == 0) {
    result = A52_3F;
  } else if(front == 2 && rear == 2) {
    result = A52_2F2R;
  } else if(front == 3 && rear == 2) {
    result = A52_3F2R;
  } else if(front == 2 && rear == 1) {
    result = A52_2F1R;
  } else if(front == 3 && rear == 1) {
    result = A52_3F1R;
  }
  if(sub == 1) {
    result |= A52_LFE;
  }
  return result;
}


static void open_output(int flags)
{
  int i;
  int driver_num = 0;
  ao_driver_t * drivers;
  int ch_avail;

  /* Ideally, we would just pass `flags' through to liba52, since that
   * seems to be how it works internally, however until it offers
   * such an interface, we figure out which "driver" it needs.
   */

  switch(flags & A52_CHANNEL_MASK) {
  case A52_CHANNEL:
  case A52_STEREO:
  case A52_DOLBY:
    ch_avail = 2;
    break;
  case A52_3F:
  case A52_2F1R:
    ch_avail = 3;
    break;
  case A52_3F1R:
  case A52_2F2R:
    ch_avail = 4;
    break;
  case A52_3F2R:
    ch_avail = 5;
    break;
  default:
    ch_avail = 0;
  }

  if(flags & A52_LFE) {
    ch_avail++;
  }

  drivers = ao_drivers ();
  for(i = 0; drivers[i].name; i++) {
    if(!strcmp("oss", drivers[i].name)
      || !strcmp("solaris", drivers[i].name)) {
      if(ch_avail == 2) {
        printf("opening a 2 channel audio driver\n");
        driver_num = i;
      }
    } else if(!strcmp("oss4", drivers[i].name)) {
      if(ch_avail == 4) {
        printf("opening a 4 channel audio driver\n");
        driver_num = i;
      }
    } else if(!strcmp("oss6", drivers[i].name)) {
      if(ch_avail == 6) {
        printf("opening a 6 channel audio driver\n");
        driver_num = i;
      }
    }
  }

  if(output) {
    ao_close(output);
    free(output);
  }
  output = ao_open(drivers[driver_num].open, get_audio_device());
  if(output == NULL) {
    FATAL("Can not open audio output\n");
    exit(1);
  }
}

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

  {
    uint32_t accel;
    accel = MM_ACCEL_MLIB;

    open_output(get_speaker_flags());

    samples = a52_init(accel);
    if(samples == NULL) {
        FATAL("A/52 init failed\n");
        exit(1);
    }
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
  static clocktime_t time_offset = { 0, 0 };
  static clocktime_t last_rt = { -1, 0 };
  static clocktime_t in_outputbuf = { 0, 0 };
  static clocktime_t prev_scr_time = { 0, 0 };
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
  off = data_elem->off + 3;
  len = data_elem->len - 3;

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
      return 0;
    } else {
      flush_to_scrid = -1;
    }
  }
  if(ctrl_data->speed == 1.0) {
    clocktime_t real_time, scr_time;
    
    clocktime_get(&real_time);
    
    if(PTS_DTS_flags & 0x2) {
      PTS_TO_CLOCKTIME(scr_time, PTS);
    }
    
    if(ctrl_time[scr_nr].sync_master <= SYNC_AUDIO) {
      clocktime_t tmptime;
      ctrl_time[scr_nr].sync_master = SYNC_AUDIO;
      
      if(ctrl_time[scr_nr].offset_valid == OFFSET_NOT_VALID) {
	if(PTS_DTS_flags & 0x2) {
	  
	  // time_offset is our guess to how much is in the output q

	  if(TIME_S(last_rt) != -1) {
	    tmptime = last_rt;
	  } else {
	    tmptime = real_time;
	  }
	  
	  timeadd(&tmptime, &tmptime, &in_outputbuf);
	  {
	    clocktime_t t1, t2;
	    timesub(&t1, &scr_time, &prev_scr_time);
	    timesub(&t2, &t1, &in_outputbuf);
	    /*
	    if((TIME_SS(t2) < -CT_FRACTION/10000) ||
	       (TIME_SS(t2) > CT_FRACTION/10000)) {
	      fprintf(stderr, "*** diff: %ld.%09ld\n",
		      TIME_S(t2), TIME_SS(t2));
	    }
	    */
	  }
	  
	  fprintf(stderr, "*rt: %ld.%09ld, last_rt: %ld.%09ld\n "
		  "bt: %ld.%09ld,  tmptime: %ld.%09ld\n scr: %ld.%09ld\n",
		  (long)TIME_S(real_time), (long)TIME_SS(real_time),
		  (long)TIME_S(last_rt), (long)TIME_SS(last_rt),
		  (long)TIME_S(in_outputbuf), (long)TIME_SS(in_outputbuf),
		  (long)TIME_S(tmptime), (long)TIME_SS(tmptime),
		  (long)TIME_S(scr_time), (long)TIME_SS(scr_time));
	    
      	  set_sync_point(&ctrl_time[scr_nr],
			 &tmptime,
			 &scr_time,
			 ctrl_data->speed);
	  prev_scr_time = scr_time;

	}
	
      } else {
	/* offset valid */
	if(PTS_DTS_flags & 0x2) {
	  clocktime_t t1, t2;
	  /* clocktime_t calc_scr_time; */
	  

	  if(TIME_S(last_rt) != -1) {
	    tmptime = last_rt;
	  } else {
	    tmptime = real_time;
	  }
	  
	  timeadd(&tmptime, &tmptime, &in_outputbuf);
	  
	  timesub(&t1, &scr_time, &prev_scr_time);
	  timesub(&t2, &t1, &in_outputbuf);
	    
	  if((TIME_SS(t2) < -CT_FRACTION/10000) ||
	     (TIME_SS(t2) > CT_FRACTION/10000)) {
	    /* diff */
	    /*
	    fprintf(stderr, "** diff: %ld.%09ld\n",
		    TIME_S(t2), TIME_SS(t2));
	    */
	    /*
	    timeadd(&calc_scr_time, &scr_time, &in_outputbuf);
	    
	    set_sync_point(&ctrl_time[scr_nr],
			   &tmptime,
			   &calc_scr_time,
			   ctrl_data->speed);
	    prev_scr_time = scr_time;
	    
	    timeadd(&last_rt, &last_rt, &in_outputbuf);
	    */
	  }
	  /*
	    else {
	    // no diff
	  */
	    
	  /*  
	      fprintf(stderr, "rt: %ld.%09ld, last_rt: %ld.%09ld\n bt: %ld.%09ld,  tmptime: %ld.%09ld\n scr: %ld.%09ld\n",
	      TIME_S(real_time), TIME_SS(real_time),
	      TIME_S(last_rt), TIME_SS(last_rt),
	      TIME_S(in_outputbuf), TIME_SS(in_outputbuf),
	      TIME_S(tmptime), TIME_SS(tmptime),
	      TIME_S(scr_time), TIME_SS(scr_time));
	      
	    set_sync_point(&ctrl_time[scr_nr],
			   &tmptime,
			   &scr_time,
			   ctrl_data->speed);
	  */
	    prev_scr_time = scr_time;
	  
	    /*
	  }
	    */
	} else {
	  /* not pts set */
	  clocktime_t calc_scr_time;
	  
	  timeadd(&calc_scr_time, &scr_time, &in_outputbuf);
	  if(TIME_S(last_rt) != -1) {
	    tmptime = last_rt;
	  } else {
	    tmptime = real_time;
	  }
	  
	  timeadd(&tmptime, &tmptime, &in_outputbuf);
	  {
	    clocktime_t t1, t2;
	    timesub(&t1, &calc_scr_time, &prev_scr_time);
	    timesub(&t2, &t1, &in_outputbuf);
	    
	    if((TIME_SS(t2) < -1000) || (TIME_SS(t2) > 1000)) {
	      fprintf(stderr, "**** diff: %ld.%09ld\n",
		      (long)TIME_S(t2), (long)TIME_SS(t2));
	    }
	    
	  }
	  
	  fprintf(stderr, "rt: %ld.%09ld, last_rt: %ld.%09ld\n "
		  "bt: %ld.%09ld,  tmptime: %ld.%09ld\n calc_scr: %ld.%09ld\n",
		  (long)TIME_S(real_time), (long)TIME_SS(real_time),
		  (long)TIME_S(last_rt), (long)TIME_SS(last_rt),
		  (long)TIME_S(in_outputbuf), (long)TIME_SS(in_outputbuf),
		  (long)TIME_S(tmptime), (long)TIME_SS(tmptime),
		  (long)TIME_S(calc_scr_time), (long)TIME_SS(calc_scr_time));
	  
	  set_sync_point(&ctrl_time[scr_nr],
			 &tmptime,
			 &calc_scr_time,
			 ctrl_data->speed);
	  prev_scr_time = scr_time;

	  timeadd(&last_rt, &last_rt, &in_outputbuf);
	  
	}
      }
      if(PTS_DTS_flags & 0x2) {
	calc_realtime_left_to_scrtime(&time_offset, &real_time,
				      &scr_time,
				      &(ctrl_time[scr_nr].sync_point));
      }
      
      /*
       * primitive resync in case output buffer is emptied 
       */
      
      if(TIME_SS(time_offset) < 0 || TIME_S(time_offset) < 0) {
	TIME_S(time_offset) = 0;
	TIME_SS(time_offset) = 0;
	NOTE("resetting offset\n");
	set_sync_point(&ctrl_time[scr_nr],
		       &real_time,
		       &scr_time,
		       ctrl_data->speed);
	//DNOTE("offset reset\n");
      }
    }

    if(PTS_DTS_flags & 0x2) {
      calc_realtime_from_scrtime(&last_rt,
				 &scr_time,
				 &(ctrl_time[scr_nr].sync_point));

      calc_realtime_left_to_scrtime(&time_offset, &real_time,
				    &scr_time,
				    &(ctrl_time[scr_nr].sync_point));

      if(TIME_S(time_offset) > 10) {
	TIME_S(time_offset) = 0;
	TIME_SS(time_offset) = 0;
	//DNOTE("more than 10 secs in audio output buffer, somethings wrong?\n");
      }
    }

    if(ctrl_data->speed == 1.0) {
      
      /** TODO this is just so we don't buffer alot in the pipe **/
      
      if(PTS_DTS_flags & 0x2) {
#ifndef HAVE_CLOCK_GETTIME
	struct timespec bepa;
	clocktime_t apa = {0, 100000};
	timesub(&apa, &time_offset, &apa);
	bepa.tv_sec = apa.tv_sec;
	bepa.tv_nsec = apa.tv_usec*1000;
	
	if(bepa.tv_nsec > 10000 || bepa.tv_sec > 0) {
	  nanosleep(&bepa, NULL);
	}
#else
	
	clocktime_t apa = {0, 100000000};
	timesub(&apa, &time_offset, &apa);
	
	if(TIME_SS(apa) > 10000000 || TIME_S(apa) > 0) {
	  nanosleep(&apa, NULL);
	}
	
#endif 
      }
    }
  }
  prev_scr_nr = scr_nr;
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
  
  if(ctrl_data->speed == 1.0) {
    in_outputbuf = a52_decode_data(data_buffer+off, data_buffer+off+len);  
  } else {
    TIME_S(in_outputbuf) = 0;
    TIME_SS(in_outputbuf) = 0;
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

static clocktime_t a52_decode_data(uint8_t *start, uint8_t *end) {
  static a52_state_t state;
  
  static uint8_t buf[3840];
  static uint8_t *bufptr = buf;
  static uint8_t *bufpos = buf + 7;
  clocktime_t buf_time = {0, 0};
  int blocks = 0;
  /*
   * sample_rate and flags are static because this routine could
   * exit between the a52_syncinfo() and the ao_setup(), and we want
   * to have the same values when we get back !
   */
  
  static int sample_rate;
  static int flags;
  int print_skip = 0, print_error = 0;
  
  while(start < end) {
    *bufptr++ = *start++;
    if(bufptr == bufpos) {
      if(bufpos == buf + 7) {
	int coded_flags, bit_rate, length;
	
	if(print_error) {
	  DNOTE("A/52 error while decoding, restarting\n");
	  print_error = 0;
	}

	length = a52_syncinfo(buf, &coded_flags, &sample_rate, &bit_rate);
	if(!length) {
	  print_skip = 1;
	  for(bufptr = buf; bufptr < buf + 6; bufptr++)
	    bufptr[0] = bufptr[1];
	  continue;
	}
	bufpos = buf + length;
      } else {
	sample_t level, bias;
	int i;	
	if(print_skip) {
	  print_skip = 0;
	  DNOTE("skipped data to find a valid frame\n");
	}
 	
	/* Verify or set the sample rate and retrive flags, level and bias */
	if(ao_setup(output, sample_rate, &flags, &level, &bias)) {
	  DNOTE("ao_setup() error\n");
          open_output(get_speaker_flags());
	  if(ao_setup(output, sample_rate, &flags, &level, &bias)) {
	    goto error;
	    //exit(1);
	  }
	  speaker_flags = flags;
	  DNOTE("ao_setup() averted\n");
	}
	
	if(speaker_flags == -1)
	  speaker_flags = flags;
	
	flags = speaker_flags;
	flags |= A52_ADJUST_LEVEL;
	memset(&state, 0, sizeof(a52_state_t));
	/* flags (speaker) [in/out] level [in/out] bias [in] */
	if(a52_frame(&state, buf, &flags, &level, bias)) {
	  DNOTE("a52_frame() error\n");
	  goto error;
	}

	if(disable_dynrng)
	  a52_dynrng(&state, NULL, NULL);
	for(i = 0; i < 6; i++) {
	  if(a52_block(&state, samples)) {
	    DNOTE("a52_block() error\n");
	    goto error;
	  }
	  /* flags (output from decoder) verified by ao_play */
	  if(ao_play(output, flags, samples)) {
	    int flags2;
	    sample_t level2, bias2; // Dummy parameters
	    DNOTE("ao_play() error\n");
	    /* re-open... */
	    open_output(get_speaker_flags());
	    /* re-setup (set sample rate) */
	    if(ao_setup(output, sample_rate, &flags2, &level2, &bias2))
	      goto error;
	    speaker_flags = flags;
	    /* set the output mode to what ever flags is (hopefully) */
	    if(ao_play(output, flags, samples))
	      goto error;
	    DNOTE("ao_play() averted\n");
	  }
	  blocks++;
	}
	bufptr = buf;
	bufpos = buf + 7;
	//print_fps(0);
	continue;
      error:
	print_error = 1;
	bufptr = buf;
	bufpos = buf + 7;
      }
    }
  }

  if(sample_rate != 0) {
    TIME_S(buf_time) = (int32_t)(((int64_t)(256*blocks)*(int64_t)CT_FRACTION/sample_rate) / CT_FRACTION);
    TIME_SS(buf_time) = (int32_t)(((int64_t)(256*blocks)*(int64_t)CT_FRACTION/sample_rate) % CT_FRACTION);
  } else {
    TIME_S(buf_time) = 0;
    TIME_SS(buf_time) = 0;
  }

  return buf_time;
}
