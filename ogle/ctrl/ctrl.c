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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <ogle/msgevents.h>
#include "mpeg.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

int create_msgq();
int init_demux(char *msqqid_str);
int init_decoder(char *msgqid_str, char *decoderstr);
int get_buffer(int size, shm_bufinfo_t *bufinfo);
int create_q(int nr_of_elems, int buf_shmid, 
	     MsgEventClient_t writer, MsgEventClient_t reader);
int create_ctrl_data();
int init_ui(char *msgqid_str);
int register_stream(uint8_t stream_id, uint8_t subtype);

void int_handler();
void remove_q_shm();
void add_q_shmid(int shmid);
void destroy_msgq();

int msgqid;

int ctrl_data_shmid;
ctrl_data_t *ctrl_data;

char *program_name;
char msgqid_str[9];




char *input_file;

char *framerate = NULL;
char *output_bufs = NULL;
char *file_offset = NULL;
char *videodecode_debug = NULL;
char *demux_debug = NULL;

int ac3_audio_stream = -1;
int dts_audio_stream = -1;
int mpeg_audio_stream = -1;
int pcm_audio_stream = -1;
int mpeg_video_stream = -1;
int subpicture_stream = -1;
int nav_stream = -1;
char *ui = NULL;


void usage(void)
{
  fprintf(stderr, "Usage: %s [-h] [-a <ac3_stream#>] [-m <mpeg_audio_stream#>] [-p <pcm_audio_stream#>] [-v <mpeg_video_stream#>] [-s <subpicture_stream#>] [-n] [-f <fps>] [-r <#ouput_bufs>] [-o <file_offset>] [-d <videodebug_level>] [-D <demuxdebug_level>] <input file>\n", program_name);
}


int next_client_id = CLIENT_UNINITIALIZED+1;
int demux_registered = 0;
MsgEventClient_t demux_client;


typedef enum {
  CAP_started = 1,
  CAP_running = 2
} cap_state_t;

typedef struct {
  MsgEventClient_t client;
  int caps;
  cap_state_t state;
} caps_t;

static caps_t *caps_array = NULL;
static int nr_caps = 0;

int register_capabilities(MsgEventClient_t client, int caps, cap_state_t state)
{
  if(nr_caps >= 20) {
    fprintf(stderr, "ctrl: WARNING more than 20 capabilities registered\n");
  }
  nr_caps++;
  caps_array = realloc(caps_array, sizeof(caps_t)*nr_caps);
  caps_array[nr_caps-1].client = client;
  caps_array[nr_caps-1].caps = caps;
  caps_array[nr_caps-1].state = state;
  return 0;
}

int search_capabilities(int caps, MsgEventClient_t *client, int *ret_caps,
			cap_state_t *ret_state)
{
  int n;
  int nr = 0;

  if(client != NULL) {
    *client = CLIENT_NONE;
  }
  fprintf(stderr, "searching cap: %d\n", caps);
  
  for(n = 0; n < nr_caps; n++) {
    if((caps_array[n].caps & caps) == caps) {
      nr++;
      if(client != NULL) {
	*client = caps_array[n].client;
        fprintf(stderr, "found capclient: %ld\n", *client);
      }
      if(ret_caps != NULL) {
	*ret_caps = caps_array[n].caps;
	fprintf(stderr, "found cap: %x\n", *ret_caps);
      }

      fprintf(stderr, "state cap: %d\n", caps_array[n].state);

      if(ret_state != NULL) {
	*ret_state = caps_array[n].state;
      }
    }
  }

  
  return nr;
}


static int streamid_to_capability(uint8_t stream_id, uint8_t subtype)
{
  int cap = 0;

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    if((subtype >= 0x80) && (subtype < 0x88)) {
      cap = DECODE_AC3_AUDIO;

    } else if((subtype >= 0x88) && (subtype < 0x90)) {
      cap = DECODE_DTS_AUDIO;
      
    } else if((subtype >= 0x20) && (subtype < 0x40)) {
      cap = DECODE_DVD_SPU;
    }
    
  } else if((stream_id >= 0xc0) && (stream_id < 0xe0)) {
    cap = DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO;
    
  } else if((stream_id >= 0xe0) && (stream_id < 0xf0)) {
    cap = DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO;
    
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    cap = DECODE_DVD_NAV;
  }
  
  return cap;
}

static char *streamid_to_decoderstr(uint8_t stream_id, uint8_t subtype)
{
  char *name = NULL;

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    if((subtype >= 0x80) && (subtype < 0x88)) {
      name = getenv("DVDP_AC3");
      
    } else if((subtype >= 0x88) && (subtype < 0x90)) {
      name = getenv("DVDP_DTS");

    } else if((subtype >= 0x20) && (subtype < 0x40)) {
      name = getenv("DVDP_SPU");
    }
    
  } else if((stream_id >= 0xc0) && (stream_id < 0xe0)) {
    name = getenv("DVDP_MPEGAUDIO");
    
  } else if((stream_id >= 0xe0) && (stream_id < 0xf0)) {
    name = getenv("DVDP_VIDEO");
    
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    name = getenv("DVDP_VMG");
  }
  
  return name;
}

static char *capability_to_decoderstr(int capability, int *ret_capability)
{
  char *name = NULL;
  
  
  if((capability & DECODE_AC3_AUDIO) == capability) {
    name = getenv("DVDP_AC3");
    *ret_capability = DECODE_AC3_AUDIO;
  } else if((capability & DECODE_DTS_AUDIO) == capability) {
    name = getenv("DVDP_DTS");
    *ret_capability = DECODE_DTS_AUDIO;
  } else if((capability & (DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO))
	    == capability) {
    name = getenv("DVDP_MPEGAUDIO");
    *ret_capability = DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO;
  } else if((capability & DECODE_DVD_SPU) == capability) {
    name = getenv("DVDP_SPU");
    *ret_capability = DECODE_DVD_SPU;
  } else if((capability & (DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO))
	    == capability) {
    name = getenv("DVDP_VIDEO");
    *ret_capability = DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO;
  } else if((capability & (DEMUX_MPEG1 | DEMUX_MPEG2_PS))
	    == capability) {
    name = getenv("DVDP_DEMUX");
    *ret_capability = DEMUX_MPEG1 | DEMUX_MPEG2_PS;
  } else if((capability & (UI_MPEG_CLI | UI_DVD_CLI))
	    == capability) {
    name = getenv("DVDP_UI");
    *ret_capability = UI_MPEG_CLI | UI_DVD_CLI;
  } else if((capability & (DECODE_DVD_NAV))
	    == capability) {
    name = getenv("DVDP_VMG");
    *ret_capability = DECODE_DVD_NAV;
  }

  return name;
}

static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  MsgEvent_t s_ev;
  MsgEvent_t r_ev;
  MsgEventClient_t rcpt;
  char *decodername;
  int capability;
  
  switch(ev->type) {
  case MsgEventQInitReq:
    fprintf(stderr, "ctrl: _MsgEventQInitReq, new_id: %d\n", next_client_id);
    ev->type = MsgEventQInitGnt;
    ev->initgnt.newclientid = next_client_id++;
    MsgSendEvent(q, CLIENT_UNINITIALIZED, ev, 0);
    break;
  case MsgEventQRegister:
    fprintf(stderr, "ctrl: _MsgEventQRegister\n");
    register_capabilities(ev->registercaps.client,
			  ev->registercaps.capabilities,
			  CAP_running);
    break;
  case MsgEventQReqCapability:
    {
      char *decodername;
      MsgEvent_t retev;
      cap_state_t state = 0;
      fprintf(stderr, "ctrl: _MsgEventQReqCapability\n");
      retev.type = MsgEventQGntCapability;
      
      if(!search_capabilities(ev->reqcapability.capability,
			      &retev.gntcapability.capclient,
			      &retev.gntcapability.capability,
			      &state)) {
	int fullcap;
	
	decodername = capability_to_decoderstr(ev->reqcapability.capability,
					       &fullcap);
	
	if(decodername != NULL) {
	  register_capabilities(0,
				fullcap,
				CAP_started);
	
	  fprintf(stderr, "ctrl: starting decoder %d %s\n",
		  fullcap,
		  decodername);
	  init_decoder(msgqid_str, decodername);
	}
	
      }
      
      while(search_capabilities(ev->reqcapability.capability,
				&retev.gntcapability.capclient,
				&retev.gntcapability.capability,
				&state) &&
	    (state != CAP_running)) {
	MsgNextEvent(q, &r_ev);
	handle_events(q, &r_ev);
      }
      
      if(state == CAP_running) {
	MsgSendEvent(q, ev->reqcapability.client, &retev, 0);
      } else {
	fprintf(stderr, "ctrl: didn't find capability\n");
	retev.gntcapability.client = CLIENT_NONE;
	MsgSendEvent(q, ev->reqcapability.client, &retev, 0);
      }
    }
    break;
  case MsgEventQReqBuf:
    {
      shm_bufinfo_t bufinfo;
      
      fprintf(stderr, "ctrl: _got request for buffer size %d\n",
	      ev->reqbuf.size);
      if(get_buffer(ev->reqbuf.size, &bufinfo) == -1) {
	bufinfo.shmid = -1;
      }
      
      s_ev.type = MsgEventQGntBuf;
      s_ev.gntbuf.shmid = bufinfo.shmid;
      s_ev.gntbuf.size = bufinfo.size;
      MsgSendEvent(q, ev->reqbuf.client, &s_ev, 0);
    }
    break;
  case MsgEventQReqStreamBuf:
    {
      int shmid;
      cap_state_t state = 0;
      fprintf(stderr, "ctrl: _new stream %x, %x\n",
	      ev->reqstreambuf.stream_id,
	      ev->reqstreambuf.subtype);
      
      if(register_stream(ev->reqstreambuf.stream_id,
			 ev->reqstreambuf.subtype)) {
	// this stream is wanted
	
	
	// check if we have a decoder
	
	//TODO check which decoder handles the stream and start it
	//if there isn't already one running that is free to use
	// TODO clean up logic/functions
	

	capability = streamid_to_capability(ev->reqstreambuf.stream_id,
					    ev->reqstreambuf.subtype);
	// check if there is a decoder running or started
	if(!search_capabilities(capability, &rcpt, NULL, NULL)) {
	  
	  decodername = streamid_to_decoderstr(ev->reqstreambuf.stream_id,
					       ev->reqstreambuf.subtype);
	  
	  if((capability & VIDEO_OUTPUT) || (capability & DECODE_DVD_SPU)) {
	    fprintf(stderr, "****ctrl: registered VO or SPU started\n");
	  }
	  if(capability == DECODE_DVD_SPU) {
	    register_capabilities(0,
				  DECODE_DVD_SPU | VIDEO_OUTPUT,
				  CAP_started);
	  } else {	    
	    register_capabilities(0,
				  capability,
				  CAP_started);
	  }
	  fprintf(stderr, "ctrl: starting decoder %d %s\n", capability,
		  decodername);
	  init_decoder(msgqid_str, decodername);
	  fprintf(stderr, "ctrl: started decoder %d\n", capability);
	 
	}
	
	while(!search_capabilities(capability, &rcpt, NULL, &state) ||
	      (state != CAP_running)) {
	  MsgNextEvent(q, &r_ev);
	  handle_events(q, &r_ev);
	}
	
	// we now have a decoder running ready to decode the stream
	
	// send ctrl_data shm: let client know where the timebase
	// data is
	fprintf(stderr, "ctrl: sending ctrldata\n");
	s_ev.type = MsgEventQCtrlData;
	s_ev.ctrldata.shmid = ctrl_data_shmid;
	
	MsgSendEvent(q, rcpt, &s_ev, 0);
	
	// at this point we know both reader and writer client
	// for the buffer.
	
	// lets create the buffer
	
	shmid = create_q(ev->reqstreambuf.nr_of_elems,
			 ev->reqstreambuf.data_buf_shmid,
			 ev->reqstreambuf.client,
			 rcpt);
	
	
	// let the writer know which streambuffer to connect to
	s_ev.type = MsgEventQGntStreamBuf;
	s_ev.gntstreambuf.q_shmid = shmid;
	s_ev.gntstreambuf.stream_id =
	  ev->reqstreambuf.stream_id; 
	s_ev.gntstreambuf.subtype =
	  ev->reqstreambuf.subtype; 
	
	MsgSendEvent(q, ev->reqstreambuf.client, &s_ev, 0);
	
	// connect the streambuf  to the reader
	
	s_ev.type = MsgEventQDecodeStreamBuf;
	s_ev.decodestreambuf.q_shmid = shmid;
	s_ev.decodestreambuf.stream_id =
	  ev->reqstreambuf.stream_id;
	s_ev.decodestreambuf.subtype = 
	  ev->reqstreambuf.subtype;
	
	MsgSendEvent(q, rcpt, &s_ev, 0);
	
      } else {
	// we don't want this stream
	// respond with the shmid set to -1
	
	s_ev.type = MsgEventQGntStreamBuf;
	s_ev.gntstreambuf.q_shmid = -1;
	s_ev.gntstreambuf.stream_id =
	  ev->reqstreambuf.stream_id; 
	s_ev.gntstreambuf.subtype =
	  ev->reqstreambuf.subtype; 
	
	MsgSendEvent(q, ev->reqstreambuf.client, &s_ev, 0);
      }
    }
    break;
  case MsgEventQReqPicBuf:
    {
      int shmid;
      cap_state_t state;
      fprintf(stderr, "ctrl: _new pic q\n");
      
      // check if we have a decoder
      
      //TODO check which decoder handles the stream and start it
      //if there isn't already one running that is free to use
      // TODO clean up logic/functions
      
      //TODO hmm start here or have decoder request cap first or
      //
      if(!search_capabilities(VIDEO_OUTPUT, &rcpt, NULL, NULL)) {
	
	fprintf(stderr, "****ctrl: registered VO|SPU started\n");
	register_capabilities(0,
			      VIDEO_OUTPUT | DECODE_DVD_SPU,
			      CAP_started);
	
	init_decoder(msgqid_str, getenv("DVDP_VIDEO_OUT"));
	fprintf(stderr, "ctrl: started video_out\n");
      }
      while(!search_capabilities(VIDEO_OUTPUT, &rcpt, NULL, &state) ||
	    (state != CAP_running)) {
	MsgNextEvent(q, &r_ev);
	handle_events(q, &r_ev);
      }
      fprintf(stderr, "ctrl: got capability video_out\n");
      
      // we now have a decoder running ready to decode the stream
      
      // send ctrl_data shm: let client know where the timebase
      // data is
      fprintf(stderr, "ctrl: sending ctrldata\n");
      s_ev.type = MsgEventQCtrlData;
      s_ev.ctrldata.shmid = ctrl_data_shmid;
	
      MsgSendEvent(q, rcpt, &s_ev, 0);
	
      // at this point we know both reader and writer client
      // for the buffer.
      
      // lets create the buffer
	
      shmid = create_q(ev->reqpicbuf.nr_of_elems,
		       ev->reqpicbuf.data_buf_shmid,
		       ev->reqpicbuf.client,
		       rcpt);
      
      
      // let the writer know which picbuffer to connect to
      s_ev.type = MsgEventQGntPicBuf;
      s_ev.gntpicbuf.q_shmid = shmid;
      
      fprintf(stderr, "ctrl: create_q, q_shmid: %d picture_buf_shmid: %d\n",
	      shmid,  ev->reqpicbuf.data_buf_shmid);
      
      MsgSendEvent(q, ev->reqpicbuf.client, &s_ev, 0);
      
      // connect the picbuf  to the reader
      
      s_ev.type = MsgEventQAttachQ;
      s_ev.attachq.q_shmid = shmid;
      
      MsgSendEvent(q, rcpt, &s_ev, 0);
	
    }
    break;
  case MsgEventQSpeed:
    {
      clocktime_t rt;
      clocktime_t tmptime, tmptime2;
      
      clocktime_get(&rt);
      fprintf(stderr, "ctrl: _MsgEventQSpeed\n");
      fprintf(stderr, "ctrl: speed: %.2f\n", ev->speed.speed);
      if(ctrl_data->mode != MODE_SPEED) {
	TIME_S(ctrl_data->vt_offset) = 0;
	TIME_SS(ctrl_data->vt_offset) = 0;
	ctrl_data->rt_start = rt;
	ctrl_data->speed = 1.0;
      }
      timesub(&tmptime, &rt, &ctrl_data->rt_start);
      timemul(&tmptime2, &tmptime, ctrl_data->speed);
      timesub(&tmptime, &tmptime, &tmptime2);
      timeadd(&ctrl_data->vt_offset, &ctrl_data->vt_offset, &tmptime);
      
      ctrl_data->rt_start = rt;
      ctrl_data->speed = ev->speed.speed;
      
      
      if(ctrl_data->speed == 1.0) {
	ctrl_data->mode = MODE_SPEED;
      } else {
	ctrl_data->mode = MODE_SPEED;
      }

    }
    break;
  default:
    fprintf(stderr, "ctrl: handle_events: msgtype %d not handled\n",
	    ev->type);
    break;
  }
}

int main(int argc, char *argv[])
{
  struct sigaction sig;
  pid_t demux_pid = -1;
  int c;
  MsgEventQ_t q;
  MsgEvent_t ev;
  MsgEvent_t r_ev;
  MsgEventClient_t rcpt;
  cap_state_t state;

  
  sig.sa_handler = int_handler;

  if(sigaction(SIGINT, &sig, NULL) == -1) {
    perror("sigaction");
  }
  
  
  program_name = argv[0];
  
  while((c = getopt(argc, argv, "na:v:s:m:f:r:o:p:d:u:t:h?")) != EOF) {
    switch(c) {
    case 'a':
      ac3_audio_stream = atoi(optarg);
      break;
    case 't':
      dts_audio_stream = atoi(optarg);
      break;
    case 'm':
      mpeg_audio_stream = atoi(optarg);
      break;
    case 'p':
      pcm_audio_stream = atoi(optarg);
      break;
    case 'v':
      mpeg_video_stream = atoi(optarg);
      break;
    case 's':
      subpicture_stream = atoi(optarg);
      break;
    case 'n':
      nav_stream = 1;
      break;
    case 'u':
      ui = optarg;
      break;
    case 'f':
      framerate = optarg;
      break;
    case 'r':
      output_bufs = optarg;
      break;
    case 'o':
      file_offset = optarg;
      break;
    case 'd':
      videodecode_debug = optarg;
      break;      
    case 'D':
      demux_debug = optarg;
      break;      
    case 'h':
    case '?':
      usage();
      return 1;
      break;
    }
  }
  
  if(argc - optind > 1){
    usage();
    return 1;
  }
  
  if(argc - optind == 1){
    input_file = argv[optind];
  } else {
    input_file = NULL;
  }
  
  ctrl_data_shmid = create_ctrl_data();
  
  /* create msgq */
  
  create_msgq();
  

  sprintf(msgqid_str, "%d", msgqid);
  
  fprintf(stderr, "msgid: %d\n", msgqid);
  
  {
    struct msqid_ds msgqinfo;
    
    msgctl(msgqid, IPC_STAT, &msgqinfo);
    
    fprintf(stderr, "max_bytes: %ld\n", msgqinfo.msg_qbytes);

  }

  q.msqid = msgqid;
  q.mtype = CLIENT_RESOURCE_MANAGER;

  if(ui != NULL) {
    init_ui(msgqid_str);
  }
  
  demux_pid = init_demux(msgqid_str);


  while(!search_capabilities(DEMUX_MPEG2_PS, &rcpt, NULL, NULL)) {
    MsgNextEvent(&q, &ev);
    switch(ev.type) {
    case MsgEventQInitReq:
      fprintf(stderr, "ctrl: MsgEventQInitReq, new_id: %d\n", next_client_id);
      ev.type = MsgEventQInitGnt;
      ev.initgnt.newclientid = next_client_id++;
      MsgSendEvent(&q, CLIENT_UNINITIALIZED, &ev, 0);
      break;
    case MsgEventQRegister:
      fprintf(stderr, "ctrl: MsgEventQRegister\n");
      register_capabilities(ev.registercaps.client,
			    ev.registercaps.capabilities,
			    CAP_running);
      break;
    case MsgEventQReqCapability:
      fprintf(stderr, "ctrl: MsgEventQReqCapability\n");
      ev.type = MsgEventQGntCapability;
      state = 0;
      while(search_capabilities(ev.reqcapability.capability,
				&ev.gntcapability.capclient,
				&ev.reqcapability.capability,
				&state) &&
	    (state != CAP_running)) {
	MsgNextEvent(&q, &r_ev);
	handle_events(&q, &r_ev);
      }
      if(state == CAP_running) {
	MsgSendEvent(&q, ev.gntcapability.client, &ev, 0);
      } else {
	fprintf(stderr, "ctrl: didn't find capability\n");
	MsgSendEvent(&q, ev.gntcapability.client, &ev, 0);
      }
      break;
    default:
      fprintf(stderr, "ctrl2: msgtype not wanted\n");
      break;
    }
    
  }
  
  ev.type = MsgEventQCtrlData;
  ev.ctrldata.shmid = ctrl_data_shmid;
  
  MsgSendEvent(&q, rcpt, &ev, 0);
  

  ev.type = MsgEventQDemuxDefault;
  
  /* If any streams are specified on the commadline, dexmux only those */
  if((ac3_audio_stream & mpeg_audio_stream & pcm_audio_stream &
      dts_audio_stream & mpeg_video_stream & subpicture_stream &
      nav_stream) == -1) {
    ev.demuxdefault.state = 1;
  } else {
    ev.demuxdefault.state = 0;
  }
  
  MsgSendEvent(&q, rcpt, &ev, 0);
  
  while(1){
    MsgNextEvent(&q, &ev);
    handle_events(&q, &ev);
  }
  
  
  return 0;
}


int init_ui(char *msgqid_str)
{
  pid_t pid;
  int n;
  char *eargv[16];
  char *ui_name;
  char *ui_path = getenv("DVDP_UI");

  //TODO clean up filename handling
  
  if(ui_path == NULL) {
    fprintf(stderr, "DVDP_UI not set\n");
    return(-1);
  }

  if((ui_name = strrchr(ui_path, '/')+1) == NULL) {
    ui_name = ui_path;
  }
  if(ui_name > &ui_path[strlen(ui_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  fprintf(stderr, "init ui\n");

  /* fork/exec ctrl */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    n = 0;
    eargv[n++] = ui_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    eargv[n++] = NULL;
    
    if(execv(ui_path, eargv) == -1) {
      perror("execv ui");
      fprintf(stderr, "path: %s\n", ui_path);
    }

    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;
}

int init_demux(char *msgqid_str)
{
  pid_t pid;
  int n;
  char *eargv[16];
  char *demux_name;
  char *demux_path = getenv("DVDP_DEMUX");

  if(demux_path == NULL) {
    fprintf(stderr, "DVDP_DEMUX not set\n");
    return(-1);
  }

  if((demux_name = strrchr(demux_path, '/')+1) == NULL) {
    demux_name = demux_path;
  }
  if(demux_name > &demux_path[strlen(demux_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  fprintf(stderr, "init demux\n");

  /* fork/exec demuxer */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    n = 0;
    eargv[n++] = demux_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    if(file_offset != NULL) {
      eargv[n++] = "-o";
      eargv[n++] = file_offset;
    }
    
    if(demux_debug != NULL) {
      eargv[n++] = "-d";
      eargv[n++] = demux_debug;
    }
    
    eargv[n++] = NULL;
    
    if(execv(demux_path, eargv) == -1) {
      perror("execv demux");
      fprintf(stderr, "path: %s\n", demux_path);
    }

    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;
}


int init_decoder(char *msgqid_str, char *decoderstr)
{
  pid_t pid;
  char *eargv[16];
  char *decode_name;
  char *decode_path = decoderstr;
  int n;
  
  if(decode_path == NULL) {
    fprintf(stderr, "decoder not set\n");
    return(-1);
  }
  
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }
  
  fprintf(stderr, "ctrl: init decoder %s\n", decode_name);

  /* fork/exec decoder */

  switch(pid = fork()) {
  case 0:
    /* child process */
    n = 0;
    eargv[n++] = decode_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    /* TODO fix for different decoders 
    if(output_bufs != NULL) {
      eargv[n++] = "-r";
      eargv[n++] = output_bufs;
    }
    if(framerate != NULL) {
      eargv[n++] = "-f";
      eargv[n++] = framerate;
    }

    if(videodecode_debug != NULL) {
      eargv[n++] = "-d";
      eargv[n++] = videodecode_debug;
    }
    */
    eargv[n++] = NULL;
    
    if(execv(decode_path, eargv) == -1) {
      perror("execv decode");
      fprintf(stderr, "path: %s\n", decode_path);
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;

}
#if 0
int init_mpeg_video_decoder(char *msgqid_str)
{
  pid_t pid;
  char *eargv[16];
  char *decode_name;
  char *decode_path = getenv("DVDP_VIDEO");
  int n;

  if(decode_path == NULL) {
    fprintf(stderr, "DVDP_VIDEO not set\n");
    return(-1);
  }

  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  fprintf(stderr, "init mpeg_video\n");

  /* fork/exec decoder */

  switch(pid = fork()) {
  case 0:
    /* child process */
    n = 0;
    eargv[n++] = decode_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    if(output_bufs != NULL) {
      eargv[n++] = "-r";
      eargv[n++] = output_bufs;
    }
    if(framerate != NULL) {
      eargv[n++] = "-f";
      eargv[n++] = framerate;
    }

    if(videodecode_debug != NULL) {
      eargv[n++] = "-d";
      eargv[n++] = videodecode_debug;
    }
    eargv[n++] = NULL;
    
    if(execv(decode_path, eargv) == -1) {
      perror("execv decode");
      fprintf(stderr, "path: %s\n", decode_path);
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;

}

int init_dolby_ac3_decoder(char *msgqid_str)
{
  pid_t pid;
  char *decode_name;
  char *decode_path = getenv("DVDP_AC3");

  if(decode_path == NULL) {
    fprintf(stderr, "DVDP_AC3 not set\n");
    return(-1);
  }

  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }
  
  fprintf(stderr, "init ac3\n");

  /* fork/exec ac3 decoder */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl ac3");
      fprintf(stderr, "path: %s\n", decode_path);

    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

int init_spu_decoder(char *msgqid_str)
{
  pid_t pid;
  char *decode_name;
  char *decode_path = getenv("DVDP_SPU");

  
  if(decode_path == NULL) {
    fprintf(stderr, "DVDP_SPU not set\n");
    return(-1);
  }

  if(strcmp(decode_path, "RUNNING") == 0) {
    return 0;
  }

  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  fprintf(stderr, "init spu\n");

  /* fork/exec spu decoder */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl spu");
      fprintf(stderr, "path: %s\n", decode_path);
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

int init_mpeg_private_stream_2_decoder(char *msgqid_str)
{
  pid_t pid;
  char *decode_name;
  char *decode_path = getenv("DVDP_VMG");

  if(decode_path == NULL) {
    fprintf(stderr, "DVDP_VMG not set\n");
    return(-1);
  }
  
  if(strcmp(decode_path, "RUNNING") == 0) {
    return 0;
  }

  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }

  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  fprintf(stderr, "init vmg\n");

  /* fork/exec vmg processor */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl vmg");
      fprintf(stderr, "path: %s\n", decode_path);
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

int init_mpeg_audio_decoder(char *msgqid)
{
  pid_t pid;
  int n;
  char *eargv[16];
  char *decode_name;
  char *decode_path = getenv("DVDP_MPEGAUDIO");

  if(decode_path == NULL) {
    fprintf(stderr, "DVDP_MPEGAUDIO not set\n");
    return(-1);
  }
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  fprintf(stderr, "init mpeg_audio_decoder\n");

  /* fork/exec decoder */

  switch(pid = fork()) {
  case 0:
    /* child process */    
    n = 0;
    eargv[n++] = decode_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;
    
    eargv[n++] = NULL;
    
    if(execv(decode_path, eargv) == -1) {
      perror("execl mpeg audio");
      fprintf(stderr, "path: %s\n", decode_path);
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

#endif



/**
 * @todo fix how to decide which streams to decode
 *
 * -u <gui name>     starts with gui name
 * filename  which file to play
 * -v #n     decode video stream #n
 * -a #n     decode ac3 audio stream #n
 * -m #n     decode mpeg audio stream #n
 * -d #n     decode dts audio stream #n
 * -p #n     decode pcm audio stream #n
 * -n        decode mpeg private stream 2 (dvd nav data)
 * -s #n     decode dvd subpicture stream #n
 * -dvd      start with dvdgui and vm
 * 
 */
int register_stream(uint8_t stream_id, uint8_t subtype)
{
  int state;
  
  /* If any stream is specified on the commadline, dexmux only those */
  if((ac3_audio_stream & mpeg_audio_stream & pcm_audio_stream &
      dts_audio_stream & mpeg_video_stream & subpicture_stream &
      nav_stream) == -1)
    state = 0;
  else
    state = 1;
  

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    
    if(state) {

      if((subtype == (0x80 | (ac3_audio_stream & 0x7))) &&
	 (ac3_audio_stream >= 0) && (ac3_audio_stream < 8)) {
	return 1;
      }
      
    } else {
      
      /* dvd, it's an ac3 stream ok */
      if((subtype >= 0x80) && (subtype < 0x88)) {
	return 1;
      }
      
    }

    if(state) {

      if((subtype == (0x88 | (dts_audio_stream & 0x7))) &&
	 (dts_audio_stream >= 0) && (dts_audio_stream < 8)) {
	return 1;
      }
      
    } else {
      
      /* dvd, it's an dts stream ok */
      if((subtype >= 0x88) && (subtype < 0x90)) {
	return 1;
      }
      
    }

    if(state) {

      if((subtype == (0x20 | (subpicture_stream & 0x1f))) &&
	 (subpicture_stream >= 0)) {
	return 1;
      }

    } else {

      /* dvd, it's a spu stream ok */
      if(((subtype & 0xf0) == 0x20) || ((subtype & 0xf0) == 0x90)) {
	return 1;
      }
      
    }

  }
  
  if(state) {

    if((stream_id == (0xc0 | (mpeg_audio_stream & 0x1f))) &&
       (mpeg_audio_stream >= 0)) { 
      return 1;
    }
    
  } else {
    
    /* dvd, mpeg audio stream ok */
    if(((stream_id & 0xf0) == 0xc0) || ((stream_id & 0xf0) == 0xd0)) {
      return 1;
    }
    
  }

  if(state) {

    if((stream_id == (0xe0 | (mpeg_video_stream & 0x0f))) &&
       (mpeg_video_stream >= 0)) {
      return 1;
    }
    
  } else {
    
    /* dvd, video stream ok */
    if((stream_id & 0xf0) == 0xe0) {
      return 1;
    }
    
  }

  if(state) {

    if((stream_id == MPEG2_PRIVATE_STREAM_2) && (nav_stream >= 0)) { // nav
      return 1;
    }

  } else {

    /* dvd, nav stream ok */
    if(stream_id == MPEG2_PRIVATE_STREAM_2) { // nav packs
      return 1;
    }
    
  }

  return 0;
  
}


int create_msgq()
{
  
  if((msgqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
    perror("msgget msgqid");
    exit(-1);
  }

  return 0;
}  


void destroy_msgq()
{
  if(msgctl(msgqid, IPC_RMID, NULL) == -1) {
    perror("msgctl ipc_rmid");
  }
  msgqid = -1;

}


int get_buffer(int size, shm_bufinfo_t *bufinfo)
{
  int shmid;
  
  fprintf(stderr, "get_buffer\n");
  if((shmid = shmget(IPC_PRIVATE,
		     size,
		     IPC_CREAT | 0600)) == -1) {
    perror("get_buffer(), shmget()");
    return -1;
  }

  add_q_shmid(shmid);

  bufinfo->shmid = shmid;
  bufinfo->size = size;
  
  return 0;
}

  
int create_q(int nr_of_elems, int buf_shmid,
	     MsgEventClient_t writer,  MsgEventClient_t reader)
{
  
  int shmid;
  char *shmaddr;
  q_head_t *q_head;
  q_elem_t *q_elems;
  int n;

  fprintf(stderr, "create_q\n");
  fprintf(stderr, "shmget\n");
  if((shmid = shmget(IPC_PRIVATE,
		     sizeof(q_head_t) + nr_of_elems*sizeof(q_elem_t),
		     IPC_CREAT | 0600)) == -1) {
    perror("create_q(), shmget()");
    return -1;
  }
  
  
  fprintf(stderr, "shmat\n");
  if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    perror("create_q(), shmat()");
    
    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
      perror("ipc_rmid");
    }
    return -1;
  }

  add_q_shmid(shmid);
  
  q_head = (q_head_t *)shmaddr;
  

  q_head->data_buf_shmid = buf_shmid;
  q_head->nr_of_qelems = nr_of_elems;
  q_head->write_nr = 0;
  q_head->read_nr = 0;
  q_head->writer = writer;
  q_head->reader = reader;
  q_head->writer_requests_notification = 0;
  q_head->reader_requests_notification = 0;
  q_head->qid = shmid; /* to get a unique id for the q */

  q_elems = (q_elem_t *)(shmaddr+sizeof(q_head_t));
  
  for(n = 0; n < nr_of_elems; n++) {
    q_elems[n].in_use = 0;
  }
  if(shmdt(shmaddr) == -1) {
    perror("create_q(), shmdt()");
  }
  return shmid;
}




int create_ctrl_data()
{
  int shmid;
  char *shmaddr;
  ctrl_time_t *ctrl_time;
  int n;
  int nr_of_offsets = 32;
  
  if((shmid = shmget(IPC_PRIVATE,
		     sizeof(ctrl_data_t)+
		     nr_of_offsets*sizeof(ctrl_time_t),
		     IPC_CREAT | 0600)) == -1) {
    perror("create_ctrl_data(), shmget()");
    return -1;
  }
  
  if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    perror("create_ctrl_data(), shmat()");

    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
      perror("ipc_rmid");
    }

    return -1;
  }

  add_q_shmid(shmid);
  
  ctrl_data = (ctrl_data_t *)shmaddr;
  ctrl_data->mode = MODE_STOP;
  ctrl_data->sync_master = SYNC_NONE;
  ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  
  for(n = 0; n < nr_of_offsets; n++) {
    ctrl_time[n].offset_valid = OFFSET_NOT_VALID;
  }

  return shmid;

}


int *shm_ids = NULL;
int nr_shmids = 0;

void add_q_shmid(int shmid)
{
  nr_shmids++;
  
  if((shm_ids = (int *)realloc(shm_ids, sizeof(int)*nr_shmids)) == NULL) {
    perror("realloc");
    nr_shmids--;
    return;
  }

  shm_ids[nr_shmids-1] = shmid;
  
}


void remove_q_shm()
{
  int n;

  for(n = 0; n < nr_shmids; n++) {
    fprintf(stderr, "removing shmid: %d\n", shm_ids[n]);
    if(shmctl(shm_ids[n], IPC_RMID, NULL) == -1) {
      perror("ipc_rmid");
    }
  }
  nr_shmids = 0;
  free(shm_ids);
  shm_ids = NULL;
  
}

void int_handler()
{
  /* send quit msg to demuxer and decoders
   * (and wait for ack? (timeout))
   * 
   * remove all shared memory segments
   *
   * remove all msqqueues
   *
   * exit
   */
  
  
  fprintf(stderr, "Caught SIGINT, cleaning up\n");
  remove_q_shm();
  destroy_msgq();
  exit(0);

}
