/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2005 Håkan Hjort, Björn Englund
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
#include <assert.h>
#include <time.h>
#include <sys/poll.h>
#include <errno.h>

#include <ogle/msgevents.h>
#include <ogle/dvdevents.h>
#include "debug_print.h"

#include <dvdread/nav_types.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>
#include "vm.h"
#include "interpret_config.h"
#include "queue.h"

extern int wait_q(MsgEventQ_t *msgq, MsgEvent_t *ev); // com.c
extern int get_q(MsgEventQ_t *msgq, unsigned char *buffer, int32_t *serial);
extern void wait_for_init(MsgEventQ_t *msgq);
extern void handle_events(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_demux(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_spu(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_videodecoder(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern char *get_dvdroot(void);
extern void free_dvdroot(void);

extern dvd_state_t state;
extern unsigned char discid[16];

MsgEvent_t dvdroot_return_ev;
MsgEventClient_t dvdroot_return_client;


typedef struct {
  int current_block; /* block offset from start of current cell 
                  start of current cell = lbn for first vobu in cell */
  int pending_lbn;   /* lbn (for next nav) that is requested from demux */
  int pending_serial;   /* serial (for next nav) that is requested  */
  cell_playback_t *cell;
  int still_time;
  pci_t pci;
  dsi_t dsi;
} nav_state_t;

static void do_run(dvd_state_t *vmstate, nav_state_t *ns);
static int process_user_data(MsgEvent_t ev, dvd_state_t *vmstate,
			     nav_state_t *ns);
static void reset_dvd(dvd_state_t *vmstate, nav_state_t *ns);

static void time_convert(DVDTimecode_t *dest, dvd_time_t *source)
{
  dest->Hours   = bcd2int(source->hour);
  dest->Minutes = bcd2int(source->minute);
  dest->Seconds = bcd2int(source->second);
  dest->Frames  = bcd2int(source->frame_u & 0x3f);
}

static DVDSubpictureState_t subpicture_state = DVD_SUBPICTURE_STATE_OFF;

MsgEventQ_t *msgq;

char *program_name;
int dlevel;

static int standalone = 1;


static int32_t dvd_serial = 0;

int32_t get_serial(void)
{
  return dvd_serial;
}

int32_t next_serial(void)
{
  dvd_serial++;
  
  return dvd_serial;
}

void usage(void)
{
  fprintf(stderr, "Usage: %s  -m <msgqid>\n", 
          program_name);
}


int main(int argc, char *argv[])
{
  int c; 
#ifdef SOCKIPC
  MsgEventQType_t msgq_type;
  char *msgqid;
#else
  int msgqid = -1;
#endif
  dvd_state_t *vmstate;
  nav_state_t *ns;
  
  program_name = argv[0];
  GET_DLEVEL();
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
#ifdef SOCKIPC
      if(get_msgqtype(optarg, &msgq_type, &msgqid) == -1) {
	fprintf(stderr, "unknown msgq type: %s\n", optarg);
	return 1;
      }
#else
      msgqid = atoi(optarg);
#endif
      standalone = 0;
      break;
    case 'h':
    case '?':
      usage();
      exit(1);
    }
  }
  
  if(standalone) {
    fprintf(stderr, "what?\n");
    exit(1);
  }

  srand(getpid());

  {
    MsgEvent_t ev;
    
    if((msgq = MsgOpen(msgq_type, msgqid, strlen(msgqid))) == NULL) {
      FATAL("%s", "couldn't get message q\n");
      exit(1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_DVD_NAV;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "registering capabilities\n");
      exit(1);
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DEMUX_MPEG2_PS | DEMUX_MPEG1;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "didn't get demux cap\n");
      exit(1);
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DECODE_DVD_SPU;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "didn't get spu cap\n");
      exit(1);
    }
  
    vmstate = &state;
    ns = malloc(sizeof(nav_state_t));

    vm_reset();

    interpret_config();

    while(1) {
      wait_for_init(msgq);
    
      /*  Call start here */
      // hack placed here because it calls DVDOpen...
      DNOTE("Opening DVD at \"%s\"\n", get_dvdroot());
      if(vm_init(get_dvdroot()) == -1) {
	// Failure, don't quit... just let the app know we didn't succeed
	dvdroot_return_ev.dvdctrl.cmd.retval.val = DVD_E_RootNotSet;
	MsgSendEvent(msgq, dvdroot_return_client, &dvdroot_return_ev, 0);
	free_dvdroot();
      } else {
	// Success, send ok and breakout of loop
	dvdroot_return_ev.dvdctrl.cmd.retval.val = DVD_E_Ok;
	MsgSendEvent(msgq, dvdroot_return_client, &dvdroot_return_ev, 0);
	break;
      }
    }
    
    ev.type = MsgEventQDemuxDVDRoot;
    strncpy(ev.demuxdvdroot.path, get_dvdroot(), sizeof(ev.demuxdvdroot.path));
    ev.demuxdvdroot.path[sizeof(ev.demuxdvdroot.path)-1] = '\0';
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed sending dvdroot to demuxer\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xe0; // Mpeg1/2 Video 
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux video stream id\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // AC3 1
    ev.demuxstream.subtype = 0x80;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux AC3 stream id\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // SPU 1
    ev.demuxstream.subtype = 0x20;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux subpicture stream id\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbf; // NAV
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux NAV stream id\n");
      exit(1);
    }
  }
  
  //vm_reset(get_dvdroot());
  do_run(vmstate, ns);
  
  return 0;
}


static int video_state = 1;

static int get_video_state(void)
{
  return video_state;
}

static void set_video_state(int state)
{
  video_state = state;
}


/**
 * returns the spustate (a mask telling the spu which types of subpictures
 * to show (forced / normal)
 */
static uint32_t get_spustate(dvd_state_t *vmstate)
{
  uint32_t spu_on = 0;
  int subpN = vm_get_subp_active_stream();
  
  if(subpN != 62) {
    if(vm_get_subp_display_flag()) {
      spu_on = 1;
    }
  }

  switch(subpicture_state) {
  case DVD_SUBPICTURE_STATE_OFF:
  case DVD_SUBPICTURE_STATE_ON:
    if(spu_on) {
      spu_on = 0x3;
    } else {
      spu_on = 0x1;
    }
    break;
  case DVD_SUBPICTURE_STATE_FORCEDOFF:
    if(vmstate->domain == VTS_DOMAIN) {
      spu_on = 0x0;
    } else {
      spu_on = 0x1;
    }
    break;
  case DVD_SUBPICTURE_STATE_DISABLED:
    spu_on = 0x0;
    break;
  }
  
  return spu_on;
}

/**
 * Send spustate to the spu (only if it has changed)
 */
static int send_spustate(uint32_t spustate)
{
  MsgEvent_t ev;
  static uint32_t prev_spustate = -1;

  if(spustate != prev_spustate) {
    prev_spustate = spustate;
    
    //send spu state to spumixer
    //fprintf(stderr, "nav: spustate: %02x\n", spustate);
    ev.type = MsgEventQSPUState;;
    ev.spustate.state = spustate;
    
    if(send_spu(msgq, &ev) == -1) {
      ERROR("%s", "faild sending highlight info\n");
      return -1;
    }
  }
  return 0;
}

static int send_demux_flush(void)
{
  MsgEvent_t ev;

  ev.type = MsgEventQDemuxDVD;
  ev.demuxdvd.titlenum = 0;
  ev.demuxdvd.domain = 0;
  ev.demuxdvd.block_offset = 0;
  ev.demuxdvd.block_count = 0;
  ev.demuxdvd.flowcmd = FlowCtrlFlush;
  ev.demuxdvd.serial = get_serial();
  ev.demuxdvd.nav_search = 0;

  if(send_demux(msgq, &ev) == -1) {
    FATAL("%s", "failed to send demux dvd flush\n");
    exit(1);
  }
}


/**
 * Update any info the demuxer needs, and then tell the demuxer
 * what range of sectors to process.
 */
static void send_demux_sectors(dvd_state_t *vmstate,
			       int start_sector, int nr_sectors, 
			       FlowCtrl_t flush) {
  static int video_aspect = -1;
  MsgEvent_t ev;
  
  /* Tell video out what aspect ratio this pice has */ 
  {
    video_attr_t attr = vm_get_video_attr();
    if(attr.display_aspect_ratio != video_aspect) {
      video_aspect = attr.display_aspect_ratio;
      
      //DNOTE("sending aspect %s\n", video_aspect ? "16:9" : "4:3");
      
      ev.type = MsgEventQSetSrcAspect;
      ev.setsrcaspect.mode_src = AspectModeSrcVM;
      if(video_aspect) {
	ev.setsrcaspect.aspect_frac_n = 16;
	ev.setsrcaspect.aspect_frac_d = 9;
      } else {
	ev.setsrcaspect.aspect_frac_n = 4;
	ev.setsrcaspect.aspect_frac_d = 3;
      }
      /* !!! FIXME should be sent to video out not spu */
      if(send_spu(msgq, &ev) == -1) {
	ERROR("%s", "failed to send aspect info\n");
      }
    }     
  }
  /* Tell the demuxer which audio stream to demux */ 
  {
    static uint8_t old_id = 0xbd;
    static uint8_t old_subtype = 0x80;
    static int old_audio_enabled = -1;
    uint8_t new_id;
    uint8_t new_subtype;
    int new_audio_enabled;
    int audioN = vm_get_audio_active_stream();
    
    new_id = 0;
    new_subtype = 0;
    new_audio_enabled = 0;

    if(audioN != 15) {
      audio_attr_t attr;
      int sN = vm_get_audio_stream_id(audioN);

      if(vm_get_audio_attr(audioN, &attr)) {
	
	switch(attr.audio_format) {
	case 0:
	  //af = DVD_AUDIO_FORMAT_AC3;
	  new_id = 0xbd; //private stream 1
	  new_subtype = 0x80 + sN; // AC-3
	  break;
	case 2:
	  //af = DVD_AUDIO_FORMAT_MPEG1;
	  new_id = 0xC0 + sN; //mpeg audio
	case 3:
	  //af = DVD_AUDIO_FORMAT_MPEG2;
	  new_id = 0xC0 + sN; //mpeg audio
	  break;
	case 4:
	  //af = DVD_AUDIO_FORMAT_LPCM;
	  new_id = 0xbd; //private stream 1
	  new_subtype = 0xA0 + sN; // lpcm
	  break;
	case 6:
	  //af = DVD_AUDIO_FORMAT_DTS;
	  new_id = 0xbd; //private stream 1
	  new_subtype = 0x88 + sN; // dts
	  break;
	default:
	  NOTE("%s", "please send a bug report, unknown Audio format!");
	  break;
	}
	
      } else {
	ERROR("send_demux: Audio stream out of range: %d\n", audioN);
	new_id = 0xbd; //private stream 1
	new_subtype = 0x80 + sN; // AC-3
      }
      
      if(old_id != new_id || old_subtype != new_subtype) {
	DNOTE("sending audio demuxstream %d\n", sN);
	DNOTE("oid: %02x, ost: %02x, nid: %02x, nst: %02x\n",
	      old_id, old_subtype, new_id, new_subtype);
	ev.type = MsgEventQDemuxStreamChange2;
	ev.demuxstreamchange2.old_stream_id = old_id;
	ev.demuxstreamchange2.old_subtype = old_subtype;
	ev.demuxstreamchange2.new_stream_id = new_id;
	ev.demuxstreamchange2.new_subtype = new_subtype;
	if(send_demux(msgq, &ev) == -1) {
	  ERROR("%s", "failed to send audio demuxstream\n");
	}
      }
      old_id = new_id;
      old_subtype = new_subtype;

      new_audio_enabled = 1;
    } else {
      // no audio decoded
      new_audio_enabled = 0;
    }
    
    if(old_audio_enabled != new_audio_enabled) {
      DNOTE("sending audio demuxstream state %s\n", 
	    (new_audio_enabled ? "enabled" : "disabled"));
      DNOTE("id: %02x, st: %02x\n",
	    old_id, old_subtype);
      ev.type = MsgEventQDemuxStreamEnable;
      ev.demuxstreamenable.state = new_audio_enabled;
      ev.demuxstreamenable.stream_id = old_id;
      ev.demuxstreamenable.subtype = old_subtype;
      
      if(send_demux(msgq, &ev) == -1) {
	ERROR("%s", "failed to send audio demuxstreamenable\n");
      }
    }
    old_audio_enabled = new_audio_enabled; 
    
  }

  /* Tell the demuxer which subpicture stream to demux */ 
  {
    static int old_subp_enabled = -1;
    static int subp_stream_id = -1;
    int new_subp_enabled = 0;
    
    int subpN = vm_get_subp_active_stream();
    
    if(subpN != 62) {
      int sN = vm_get_subp_stream_id(subpN);
      
      if(sN != subp_stream_id) {
	subp_stream_id = sN;
	
	DNOTE("sending subp demuxstream %d\n", sN);
	
	ev.type = MsgEventQDemuxStreamChange;
	ev.demuxstreamchange.stream_id = 0xbd; // SPU
	ev.demuxstreamchange.subtype = 0x20 | subp_stream_id;
	if(send_demux(msgq, &ev) == -1) {
	  ERROR("%s", "failed to send Subpicture demuxstream\n");
	}
      }
    
      new_subp_enabled = 1;
    } else {
      // no subpicture
      new_subp_enabled = 0;
    }

    if((subp_stream_id != -1) && (old_subp_enabled != new_subp_enabled)) {
      DNOTE("sending subp demuxstream state %s\n", 
	    (new_subp_enabled ? "enabled" : "disabled"));
      DNOTE("id: %02x, st: %02x\n",
	    0xbd, 0x20 | subp_stream_id);
      ev.type = MsgEventQDemuxStreamEnable;
      ev.demuxstreamenable.state = new_subp_enabled;
      ev.demuxstreamenable.stream_id = 0xbd;
      ev.demuxstreamenable.subtype = 0x20 | subp_stream_id;
      
      if(send_demux(msgq, &ev) == -1) {
	ERROR("%s", "failed to send audio demuxstreamenable\n");
      }
    }
    old_subp_enabled = new_subp_enabled; 

    send_spustate(get_spustate(vmstate));
    
  }

  {
    static int old_video_enabled = 1;
    int new_video_enabled = 0;
    new_video_enabled = get_video_state();

    if(old_video_enabled != new_video_enabled) {
#if 0
      DNOTE("sending video demuxstream state %s\n", 
	    (new_video_enabled ? "enabled" : "disabled"));
      ev.type = MsgEventQDemuxStreamEnable;
      ev.demuxstreamenable.state = new_video_enabled;
      ev.demuxstreamenable.stream_id = 0xe0;
      ev.demuxstreamenable.subtype = 0;
      
      if(send_demux(msgq, &ev) == -1) {
	ERROR("%s", "failed to send video demuxstreamenable\n");
      }
      if(!new_video_enabled) {
	flush = FlowCtrlCompleteVideoUnit;
      }
#else
      ev.type = MsgEventQSetDecodeVideoState;
      ev.decodevideostate.state = new_video_enabled;

      if(send_videodecoder(msgq, &ev) == -1) {
	ERROR("%s", "failed to send video state\n");
      }
      
#endif
    }
    old_video_enabled = new_video_enabled; 
  }

  
  /* Tell the demuxer what file and which sectors to demux. */
  ev.type = MsgEventQDemuxDVD;
  if(vmstate->domain == VMGM_DOMAIN || vmstate->domain == FP_DOMAIN) {
    ev.demuxdvd.titlenum = 0;
  } else {
    ev.demuxdvd.titlenum = vmstate->vtsN;
  }
  if(vmstate->domain == VTS_DOMAIN) {
    ev.demuxdvd.domain = DVD_READ_TITLE_VOBS;
  } else {
    ev.demuxdvd.domain = DVD_READ_MENU_VOBS;
  }
  ev.demuxdvd.block_offset = start_sector;
  ev.demuxdvd.block_count = nr_sectors;
  ev.demuxdvd.flowcmd = flush;
  ev.demuxdvd.serial = get_serial();
  ev.demuxdvd.nav_search = 0;
  if(send_demux(msgq, &ev) == -1) {
    FATAL("%s", "failed to send demux dvd block range\n");
    exit(1);
  }
  DNOTE("sent demux dvd block range (%d,%d)\n", start_sector, nr_sectors);
}


/**
 * Update any info the demuxer needs, and then tell the demuxer
 * what range of sectors to process for a nav pack.
 */
static uint32_t send_demux_nav(dvd_state_t *vmstate,
			       int start_sector, int nr_sectors, 
			       FlowCtrl_t flush) {
  MsgEvent_t ev;
  uint32_t serial;
  /* Tell the demuxer what file and which sectors to demux. */
  ev.type = MsgEventQDemuxDVD;
  if(vmstate->domain == VMGM_DOMAIN || vmstate->domain == FP_DOMAIN) {
    ev.demuxdvd.titlenum = 0;
  } else {
    ev.demuxdvd.titlenum = vmstate->vtsN;
  }
  if(vmstate->domain == VTS_DOMAIN) {
    ev.demuxdvd.domain = DVD_READ_TITLE_VOBS;
  } else {
    ev.demuxdvd.domain = DVD_READ_MENU_VOBS;
  }
  ev.demuxdvd.block_offset = start_sector;
  ev.demuxdvd.block_count = nr_sectors;
  ev.demuxdvd.flowcmd = flush;
  serial = get_serial();
  ev.demuxdvd.serial = serial;
  ev.demuxdvd.nav_search = 1;
  if(send_demux(msgq, &ev) == -1) {
    FATAL("%s", "failed to send demux dvd nav block range\n");
    exit(1);
  }
  DNOTE("sent demux dvd nav block range (%d,%d)\n", start_sector, nr_sectors);
  
  return serial;
}

static void send_spu_palette(uint32_t palette[16]) {
  MsgEvent_t ev;
  int i;
  
  ev.type = MsgEventQSPUPalette;
  for(i = 0; i < 16; i++) {
    ev.spupalette.colors[i] = palette[i];
  }
  
  //DNOTE("sending subpicture palette\n");
  
  if(send_spu(msgq, &ev) == -1) {
    ERROR("%s", "failed sending subpicture palette\n");
  }
}

static void send_highlight(int x_start, int y_start, int x_end, int y_end, 
			   uint32_t btn_coli) {
  MsgEvent_t ev;
  int i;
  
  ev.type = MsgEventQSPUHighlight;
  ev.spuhighlight.nav_serial = get_serial();
  ev.spuhighlight.x_start = x_start;
  ev.spuhighlight.y_start = y_start;
  ev.spuhighlight.x_end = x_end;
  ev.spuhighlight.y_end = y_end;
  for(i = 0; i < 4; i++)
    ev.spuhighlight.color[i] = 0xf & (btn_coli >> (16 + 4*i));
  for(i = 0; i < 4; i++)
    ev.spuhighlight.contrast[i] = 0xf & (btn_coli >> (4*i));

  if(send_spu(msgq, &ev) == -1) {
    ERROR("%s", "failed sending highlight info\n");
  }
}

/** 
 * Check if mouse coords are over any highlighted area.
 * 
 * @return The button number if the the coordinate is enclosed in the area.
 * Zero otherwise.
 */
int mouse_over_hl(pci_t *pci, unsigned int x, unsigned int y) {
  int button = 1;
  while(button <= pci->hli.hl_gi.btn_ns) {
    if( (x >= pci->hli.btnit[button-1].x_start)
	&& (x <= pci->hli.btnit[button-1].x_end) 
	&& (y >= pci->hli.btnit[button-1].y_start) 
	&& (y <= pci->hli.btnit[button-1].y_end )) 
      return button;
    button++;
  }
  return 0;
}

/** 
 * Update the highligted button in response to an input event.
 * Also send highlight information to the spu_mixer.
 *
 * @return One if the (possibly updated) button is activated.
 * Zero otherwise.
 */
static int process_button(DVDCtrlEvent_t *ce, pci_t *pci, uint16_t *btn_reg) {
  /* Keep the button register value in a local variable. */
  uint16_t button_nr = (*btn_reg) >> 10;
  int tmp, is_action = 0;

  /* MORE CODE HERE :) */
  
  /* Paranoia.. */
  
  // No highlight/button pci info to use or no buttons
  if((pci->hli.hl_gi.hli_ss & 0x03) == 0 || pci->hli.hl_gi.btn_ns == 0)
    return 0;
  
  // Selected button > than max button? then cap it
  if(button_nr > pci->hli.hl_gi.btn_ns)
    button_nr = pci->hli.hl_gi.btn_ns;
  
  // Selected button should never be 0.
  if(button_nr == 0) {
    //FATAL("%s", "send bug report, button number is 0, this is invalid.");
    button_nr = 1;
    *btn_reg = button_nr << 10;
  } 
    
  switch(ce->type) {
  case DVDCtrlUpperButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].up;
    break;
  case DVDCtrlLowerButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].down;
    break;
  case DVDCtrlLeftButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].left;
    break;
  case DVDCtrlRightButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].right;
    break;
  case DVDCtrlButtonActivate:
    is_action = 1;
    break;
  case DVDCtrlButtonSelectAndActivate:
    is_action = 1;
    /* Fall through */
  case DVDCtrlButtonSelect:
    tmp = ce->button.nr - pci->hli.hl_gi.btn_ofn;
    if(tmp > 0 && tmp <= pci->hli.hl_gi.nsl_btn_ns)
      button_nr = tmp;
    else /* not a valid button */
      is_action = 0;
    break;
  case DVDCtrlMouseActivate:
    is_action = 1;
    /* Fall through */
  case DVDCtrlMouseSelect:
    {
      int button;
      //int width, height;
      //vm_get_video_res(&width, &height);
      button = mouse_over_hl(pci, ce->mouse.x, ce->mouse.y);
      if(button)
	button_nr = button;
      else
	is_action = 0;
    }
    break;
  default:
    DNOTE("ignoring dvdctrl event (%d)\n", ce->type);
    break;
  }
  
  /* Must check if the current selected button has auto_action_mode !!! */
  /* Don't do auto action if it's been selected with the mouse... ?? */
  switch(pci->hli.btnit[button_nr - 1].auto_action_mode) {
  case 0:
    break;
  case 1:
    if(ce->type == DVDCtrlMouseSelect) {
      /* auto_action buttons can't be select if they are not activated
	 keep the previous selected button */
      button_nr = (*btn_reg) >> 10;
    } else {
      DNOTE("%s", "auto_action_mode set!\n");
      is_action = 1;
    }
    break;
  case 2:
  case 3:
  default:
    FATAL("send bug report, unknown auto_action_mode(%d) btn: %d\n", 
	  pci->hli.btnit[button_nr - 1].auto_action_mode, button_nr);
    navPrint_PCI(pci);
    exit(1);
  }
  
  /* If a new button has been selected or if one has been activated. */
  /* Determine the correct area and send the information to the spu decoder. */
  /* Don't send if its the same as last time. */
  if(is_action || button_nr != ((*btn_reg) >> 10)) {
    btni_t *button;
    button = &pci->hli.btnit[button_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][is_action]);
  }
  
  /* Write the (updated) value back to the button register. */
  *btn_reg = button_nr << 10;
  
  return is_action;
}


/** 
 * Update the highligted button in response to a new pci packet.
 * Also send highlight information to the spu_mixer.
 * 
 * @return One if the (possibly updated) button is activated.
 * Zero otherwise.
 */
static void process_pci(pci_t *pci, uint16_t *btn_reg) {
  /* Keep the button register value in a local variable. */
  uint16_t button_nr = (*btn_reg) >> 10;
	  
  /* Check if this is alright, i.e. pci->hli.hl_gi.hli_ss == 1 only 
   * for the first menu pic packet? Should be.
   * What about looping menus? Will reset it every loop.. */
  if(pci->hli.hl_gi.hli_ss == 1) {
    if(pci->hli.hl_gi.fosl_btnn != 0) {
      button_nr = pci->hli.hl_gi.fosl_btnn;
      DNOTE("forced select button %d\n", pci->hli.hl_gi.fosl_btnn);
    }
  }
  
  /* SPRM[8] can be changed by 
     A) user operations  
         user operations can only change SPRM[8] if buttons exist.
     B) navigation commands  
         navigation commands can change SPRM[8] always.
     C) highlight information
     
     if no buttons exist SPRM[8] is kept at it's value 
     (except when navigation commands change it)

     if SPRM[8] doesn't have a valid value  (button_nr > nr_buttons)  
     button_nr = nr_buttons  (except when nr_buttons == 0, 
     then button_nr isn't changed at all.
  */
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0 
     && button_nr > pci->hli.hl_gi.btn_ns
     && pci->hli.hl_gi.btn_ns != 0) {
    button_nr = pci->hli.hl_gi.btn_ns;
  }

  /* Determine the correct highlight and send the information to the spu. */
  if((pci->hli.hl_gi.hli_ss & 0x03) == 0 || 
     button_nr > pci->hli.hl_gi.btn_ns) {
    /* Turn off the highlight. */
    send_highlight(0, 0, 0, 0, 0 /* Transparent */);
  } else {
    /* Possible optimization: don't send if its the same as last time. 
       As in same hli info, same button number and same select/action state. */
    btni_t *button = &pci->hli.btnit[button_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][0]);
  }
  
  /* Write the (updated) value back to the button register. */  
  *btn_reg = button_nr << 10;
}


int process_seek(dvd_state_t *vmstate, int seconds, dsi_t *dsi,
		 cell_playback_t *cell)
{
  int res = 0;
  dvd_time_t current_time;
  
// bit 0: v0: *Video data* does not exist in the VOBU at the address
//        v1: *video data* does exists in the VOBU on the address
// bit 1: indicates whether theres *video data* between 
//        current vobu and last vobu. ??
// if address = 3fff ffff -> vobu does not exist
#define VALID_XWDA(OFFSET) \
  (((OFFSET) & SRI_END_OF_CELL) != SRI_END_OF_CELL && \
  ((OFFSET) & 0x80000000))
  
  vm_get_current_time(&current_time, &(dsi->dsi_gi.c_eltm));
  // Try using the Time Map Tables, should we use VOBU seeks for
  // small seek (< 8s) anyway? as they (may) have better resolution.
  // Fall back if we're crossing a cell bounduary...
  if(vm_time_play(&current_time, seconds)) {
    return 1; // Successfull 
  } else {
    // We have 120 60 30 10 7.5 7 6.5 ... 0.5 seconds markers
    if(seconds > 0) {
      const unsigned int time[19] = { 240, 120, 60, 20, 15, 14, 13, 12, 11, 
				       10,   9,  8,  7,  6,  5,  4,  3,  2, 1};
      const unsigned int hsec = seconds * 2;
      unsigned int diff, idx = 0;
	  
      diff = abs(hsec - time[0]);
      while(idx < 19 && abs(hsec - time[idx]) <= diff) {
	diff = abs(hsec - time[idx]);
	idx++;
      }
      idx--; // Restore it to the one that got us the diff
      
      // Make sure we have a VOBU that 'exists' (with in the cell)
      // What about the 'top' two bits here?  If there is no video at the
      // seek destination?  Check with the menus in Coruptor.
      while(idx < 19 && !VALID_XWDA(dsi->vobu_sri.fwda[idx])) {
	idx++;
      }
      if(idx < 19) {
	// Fake this, as a jump with blockN as destination
	// blockN is relative the start of the cell
	vmstate->blockN = dsi->dsi_gi.nv_pck_lbn +
	  (dsi->vobu_sri.fwda[idx] & 0x3fffffff) - cell->first_sector;
	res = 1;
      } else
	res = 0; // no new block found.. must be at the end of the cell..
    } else {
      const unsigned int time[19] = { 240, 120, 60, 20, 15, 14, 13, 12, 11, 
				       10,   9,  8,  7,  6,  5,  4,  3,  2, 1};
      const unsigned int hsec = (-seconds) * 2; // -
      unsigned int diff, idx = 0;
      
      diff = abs(hsec - time[0]);
      
      while(idx < 19 && abs(hsec - time[idx]) <= diff) {
	diff = abs(hsec - time[idx]);
	idx++;
      }
      idx--; // Restore it to the one that got us the diff
      
      // Make sure we have a VOBU that 'exists' (within the cell)
      // What about the 'top' two bits here?  If there is no video at the
      // seek destination?  Check with the menus in Coruptor.
      while(idx < 19 && !VALID_XWDA(dsi->vobu_sri.bwda[18-idx])) {
	idx++;
      }
      if(idx < 19) {
	// Fake this, as a jump with blockN as destination
	// blockN is relative the start of the cell
	vmstate->blockN = dsi->dsi_gi.nv_pck_lbn -
	  (dsi->vobu_sri.bwda[18-idx] & 0x3fffffff) - cell->first_sector;
	res = 1;
      } else
	res = 0; // no new_block found.. must be at the end of the cell..    
    }
  }
  return res;
}
 
 
void set_dvderror(MsgEvent_t *ev, int32_t serial, DVDResult_t err)
{
  ev->dvdctrl.cmd.type = DVDCtrlRetVal;
  ev->dvdctrl.cmd.retval.serial = serial;
  ev->dvdctrl.cmd.retval.val = err;
}


static int n_still_time;
static cell_playback_t *n_cell;

static int n_pending_lbn;

static int n_current_block; 


//static nav_state_t nav_state;

void set_pending_nav(nav_state_t *ns, uint32_t lbn, uint32_t serial)
{
 ns->pending_lbn = lbn;
 ns->pending_serial = serial;
}

uint32_t get_pending_lbn(nav_state_t *ns)
{
  return ns->pending_lbn;
}

uint32_t get_pending_serial(nav_state_t *ns)
{
  return ns->pending_serial;
}

void set_current_block(nav_state_t *ns, uint32_t block)
{
  ns->current_block = block;
}

uint32_t get_current_block(nav_state_t *ns)
{
  return ns->current_block;
}

int nav_within_cell(cell_playback_t *cell, uint32_t block)
{
  return (cell->first_sector + block <= cell->last_vobu_start_sector);
}

int navlbn_within_cell(cell_playback_t *cell, uint32_t lbn)
{
  return ((cell->first_sector <= lbn) && 
	  (lbn <= cell->last_vobu_start_sector));
}


uint32_t blk2lbn(cell_playback_t *cell, int32_t block)
{
  return cell->first_sector + block;
}

uint32_t cell_len(cell_playback_t *cell)
{
  return cell->last_sector - cell->first_sector + 1;
}

#define DVD_SERIAL(ev) ((ev)->dvdctrl.cmd.any.serial)

static unsigned int stop_state = 1;

/* Do user input processing. Like audio change, 
 * subpicture change and answer attribute query requests.
 * access menus, pause, play, jump forward/backward...
 */
int process_user_data(MsgEvent_t ev, dvd_state_t *vmstate, nav_state_t *ns)
{
  int res = 0;
  // pci_t *a_pci = &ns->pci;
  // dsi_t *a_dsi = &ns->dsi; 
  // cell_playback_t *a_cell = ns->cell;
  // int a_block = get_current_block(ns);
  // int *a_still_time = &ns->still_time;


  //fprintf(stderr, "nav: User input, MsgEvent.type: %d\n", ev.type);
  
  switch(ev.dvdctrl.cmd.type) {
  case DVDCtrlLeftButtonSelect:
  case DVDCtrlRightButtonSelect:
  case DVDCtrlUpperButtonSelect:
  case DVDCtrlLowerButtonSelect:
  case DVDCtrlButtonActivate:
  case DVDCtrlButtonSelect:
  case DVDCtrlButtonSelectAndActivate:
  case DVDCtrlMouseSelect:
  case DVDCtrlMouseActivate:
    
    // A button has already been activated, discard this event??
    if(navlbn_within_cell(ns->cell, ns->pci.pci_gi.nv_pck_lbn)) {
      /* Update selected/activated button, send highlight info to spu */
      /* Returns true if a button is activated */
      if(process_button(&ev.dvdctrl.cmd, &ns->pci, &vmstate->HL_BTNN_REG)) {
	int button_nr = vmstate->HL_BTNN_REG >> 10;
	//if a button is activated in a cell still, end still?
	if(ns->still_time > 0) {
	  ns->still_time = 0;
	}
	res = vm_eval_cmd(&ns->pci.hli.btnit[button_nr - 1].cmd);
      }
    }
    break;
  
  case DVDCtrlTimeSkip:
    if(ns->dsi.dsi_gi.nv_pck_lbn == -1) { // we are waiting for a new nav block
      res = 0;
      break;
    }
    res = process_seek(vmstate, ev.dvdctrl.cmd.timeskip.seconds,
		       &ns->dsi, ns->cell);
    if(res)
      NOTE("%s", "Doing time seek\n");
    break;  
  
  case DVDCtrlMenuCall:
    NOTE("Jumping to Menu %d\n", ev.dvdctrl.cmd.menucall.menuid);
    res = vm_menu_call(ev.dvdctrl.cmd.menucall.menuid, get_current_block(ns));
    if(!res)
      NOTE("%s", "No such menu!\n");
    break;
	  
  case DVDCtrlResume:
    res = vm_resume();
    break;
	  
  case DVDCtrlGoUp:
    res = vm_goup_pgc();
    break;
	  
  case DVDCtrlBackwardScan:
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
	
  case DVDCtrlPauseOn:
  case DVDCtrlPauseOff:
  case DVDCtrlForwardScan:  
    {
      MsgEvent_t send_ev;
      static double last_speed = 1.0;

      if(stop_state == 2) {
	res = 1;
      }

      if(stop_state != 1) {
	stop_state = 1;
	
	DNOTE(" DVDCtrlEvent %d start stop, todo\n",
	      ev.dvdctrl.cmd.type);
	
	send_ev.type = MsgEventQStop;
	send_ev.stop.state = 1;
	
	MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &send_ev, 0);
      }
      
      

      send_ev.type = MsgEventQSpeed;
      if(ev.dvdctrl.cmd.type == DVDCtrlForwardScan) {
	send_ev.speed.speed = ev.dvdctrl.cmd.scan.speed;
	last_speed = ev.dvdctrl.cmd.scan.speed;
      }
      /* Perhaps we should remeber the speed before the pause. */
      else if(ev.dvdctrl.cmd.type == DVDCtrlPauseOn)
	send_ev.speed.speed = 0.000000001;
      else if(ev.dvdctrl.cmd.type == DVDCtrlPauseOff)
	send_ev.speed.speed = last_speed;
	    
      /* Hack to exit STILL_MODE if we're in it. */
      if((!nav_within_cell(ns->cell, get_current_block(ns))) && 
	 ns->still_time > 0) {
	ns->still_time = 0;
      }
      MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &send_ev, 0);
    }
    break;
	  
  case DVDCtrlNextPGSearch:
    res = vm_next_pg();
    break;
  case DVDCtrlPrevPGSearch:
    res = vm_prev_pg();
    break;
  case DVDCtrlTopPGSearch:
    res = vm_top_pg();
    break;
	  
  case DVDCtrlPTTSearch:
    res = vm_jump_ptt(ev.dvdctrl.cmd.pttsearch.ptt);
    break;
  case DVDCtrlPTTPlay:
    res = vm_jump_title_ptt(ev.dvdctrl.cmd.pttplay.title,
			    ev.dvdctrl.cmd.pttplay.ptt);
    break;	
  case DVDCtrlTitlePlay:
    res = vm_jump_title(ev.dvdctrl.cmd.titleplay.title);
    break;
  case DVDCtrlTimeSearch:
    // not in One_Random_PGC_Title or Multi_PGC_Title
    //dsi.dsi_gi.c_eltm; /* Current 'nav' time */
    //ev.dvdctrl.cmd.timesearch.time; /* wanted time */
    //dsi.vobu_sri.[FWDA|BWDA]; /* Table for small searches */
    break;
  case DVDCtrlTimePlay:
    // not in One_Random_PGC_Title or Multi_PGC_Title
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
	  
	  
  case DVDCtrlStop:
    {
      MsgEvent_t send_ev;

      if(stop_state == 0) {
	stop_state = 2;
	reset_dvd(vmstate, ns);
      } else if(stop_state == 1) {
	stop_state = 0;
	DNOTE(" DVDCtrlEvent %d stop, todo\n",
	      ev.dvdctrl.cmd.type);
	
	send_ev.type = MsgEventQStop;
	send_ev.stop.state = 0;
	/* Hack to exit STILL_MODE if we're in it. */
	if((!nav_within_cell(ns->cell, get_current_block(ns))) && 
	   ns->still_time > 0) {
	  ns->still_time = 0;
	}
	MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &send_ev, 0);
      }
    }
    break;
	  
  case DVDCtrlAngleChange:
    /* FIXME $$$ need to actually change the playback angle too, no? */
    vmstate->AGL_REG = ev.dvdctrl.cmd.anglechange.anglenr;
    break;
  case DVDCtrlAudioStreamChange: // FIXME $$$ Temorary hack
    vmstate->AST_REG = ev.dvdctrl.cmd.audiostreamchange.streamnr; // XXX
    break;
  case DVDCtrlSubpictureStreamChange: // FIXME $$$ Temorary hack
    vmstate->SPST_REG &= 0x40; // Keep the on/off bit.
    vmstate->SPST_REG |= (ev.dvdctrl.cmd.subpicturestreamchange.streamnr & 0x3f);
    NOTE("DVDCtrlSubpictureStreamChange %x\n", vmstate->SPST_REG);
    break;
  case DVDCtrlSetSubpictureState:
    subpicture_state = ev.dvdctrl.cmd.subpicturestate.display;

    switch(subpicture_state) {
    case DVD_SUBPICTURE_STATE_ON:
      vmstate->SPST_REG |= 0x40; // Turn it on
      break;
    case DVD_SUBPICTURE_STATE_OFF:
    case DVD_SUBPICTURE_STATE_FORCEDOFF:
    case DVD_SUBPICTURE_STATE_DISABLED:
      vmstate->SPST_REG &= ~0x40; // Turn it off
      break;
    }
    
    send_spustate(get_spustate(vmstate));
    break;
  case DVDCtrlSetVideoState:
    set_video_state(ev.dvdctrl.cmd.videostate.display);
    break;
  case DVDCtrlGetCurrentDomain:
    {
      MsgEvent_t send_ev;
      int domain;
      domain = vm_get_domain();
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentDomain;
      send_ev.dvdctrl.cmd.domain.domain = domain;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetCurrentLocation:
    {
      MsgEvent_t send_ev;
      dvd_time_t current_time;
      dvd_time_t total_time;
      DVDLocation_t *location;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentLocation;
      /* should not return when domain is wrong( /= title). */
      /* how to get current time for searches in menu/system space? */
      /* a bit of a hack */
      location = &send_ev.dvdctrl.cmd.location.location;
      location->title = vmstate->TTN_REG;
      location->ptt = vmstate->PTTN_REG;
      vm_get_total_time(&total_time);
      time_convert(&location->title_total, &total_time);
      vm_get_current_time(&current_time, &(ns->pci.pci_gi.e_eltm));
      time_convert(&location->title_current, &current_time);
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetDVDVolumeInfo:
    {
      int nofv,vol,side,noft;
      MsgEvent_t send_ev;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlDVDVolumeInfo;
      vm_get_volume_info(&nofv,&vol,&side,&noft);
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.nrofvolumes = nofv;
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.volume = vol;
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.side = side;
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.nroftitles = noft;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetTitles:
    {
      MsgEvent_t send_ev;
      int titles;
      titles = vm_get_titles();
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlTitles;
      send_ev.dvdctrl.cmd.titles.titles = titles;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetNumberOfPTTs:
    {
      MsgEvent_t send_ev;
      int ptts, title;
      title = ev.dvdctrl.cmd.parts.title;
      ptts = vm_get_ptts_for_title(title);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlNumberOfPTTs;
      send_ev.dvdctrl.cmd.parts.title = title;
      send_ev.dvdctrl.cmd.parts.ptts = ptts;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetCurrentAudio:
    {
      MsgEvent_t send_ev;
      int nS, cS;
      vm_get_audio_info(&nS, &cS);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentAudio;
      send_ev.dvdctrl.cmd.currentaudio.nrofstreams = nS;
      send_ev.dvdctrl.cmd.currentaudio.currentstream = cS;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlIsAudioStreamEnabled:
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.audiostreamenabled.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlAudioStreamEnabled;
      send_ev.dvdctrl.cmd.audiostreamenabled.streamnr = streamN;
      send_ev.dvdctrl.cmd.audiostreamenabled.enabled =
	(vm_audio_stream_enabled(streamN)) ? DVDTrue : DVDFalse;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
    }
    break;
  case DVDCtrlGetCurrentUOPS: // FIXME XXX $$$ Not done
    {
      DVDUOP_t res = 0;
      MsgEvent_t send_ev;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.currentuops.type = DVDCtrlCurrentUOPS;
      {
	user_ops_t p_uops = vm_get_uops();
	/* This mess is needed...
	 * becuse we didn't do a endian swap in libdvdread */
	res |= (p_uops.title_or_time_play ? 0 : UOP_FLAG_TitleOrTimePlay);
	res |= (p_uops.chapter_search_or_play ? 0 : UOP_FLAG_ChapterSearchOrPlay);
	res |= (p_uops.title_play ? 0 : UOP_FLAG_TitlePlay);
	res |= (p_uops.stop ? 0 : UOP_FLAG_Stop);
	res |= (p_uops.go_up ? 0 : UOP_FLAG_GoUp);
	res |= (p_uops.time_or_chapter_search ? 0 : UOP_FLAG_TimeOrChapterSearch);
	res |= (p_uops.prev_or_top_pg_search ? 0 : UOP_FLAG_PrevOrTopPGSearch);
	res |= (p_uops.next_pg_search ? 0 : UOP_FLAG_NextPGSearch);

	res |= (p_uops.title_menu_call ? 0 : UOP_FLAG_TitleMenuCall);
	res |= (p_uops.root_menu_call ? 0 : UOP_FLAG_RootMenuCall);
	res |= (p_uops.subpic_menu_call ? 0 : UOP_FLAG_SubPicMenuCall);
	res |= (p_uops.audio_menu_call ? 0 : UOP_FLAG_AudioMenuCall);
	res |= (p_uops.angle_menu_call ? 0 : UOP_FLAG_AngleMenuCall);
	res |= (p_uops.chapter_menu_call ? 0 : UOP_FLAG_ChapterMenuCall);
	     
	res |= (p_uops.resume ? 0 : UOP_FLAG_Resume);
	res |= (p_uops.button_select_or_activate ? 0 : UOP_FLAG_ButtonSelectOrActivate);
	res |= (p_uops.still_off ? 0 : UOP_FLAG_StillOff);
	res |= (p_uops.pause_on ? 0 : UOP_FLAG_PauseOn);
	res |= (p_uops.audio_stream_change ? 0 : UOP_FLAG_AudioStreamChange);
	res |= (p_uops.subpic_stream_change ? 0 : UOP_FLAG_SubPicStreamChange);
	res |= (p_uops.angle_change ? 0 : UOP_FLAG_AngleChange);
	res |= (p_uops.karaoke_audio_pres_mode_change ? 0 : UOP_FLAG_KaraokeAudioPresModeChange);
	res |= (p_uops.video_pres_mode_change ? 0 : UOP_FLAG_VideoPresModeChange);
      }
      send_ev.dvdctrl.cmd.currentuops.uops = res;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetAudioAttributes: // FIXME XXX $$$ Not done
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.audioattributes.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlAudioAttributes;
      send_ev.dvdctrl.cmd.audioattributes.streamnr = streamN;
      {
	DVDAudioAttributes_t *a_attr =
	  &send_ev.dvdctrl.cmd.audioattributes.attr;
	DVDAudioFormat_t af = DVD_AUDIO_FORMAT_Other;
	audio_attr_t attr;

	if(vm_get_audio_attr(streamN, &attr)) {
	  
	  memset(a_attr, 0, sizeof(DVDAudioAttributes_t));
	  switch(attr.audio_format) {
	  case 0:
	    af = DVD_AUDIO_FORMAT_AC3;
	    break;
	  case 2:
	    if(attr.quantization == 1) {
	      af = DVD_AUDIO_FORMAT_MPEG1_DRC;
	    } else {
	      af = DVD_AUDIO_FORMAT_MPEG1;
	    }
	    break;
	  case 3:
	    if(attr.quantization == 1) {
	      af = DVD_AUDIO_FORMAT_MPEG2_DRC;
	    } else {
	      af = DVD_AUDIO_FORMAT_MPEG2;
	    }
	    break;
	  case 4:
	    af = DVD_AUDIO_FORMAT_LPCM;
	    switch(attr.quantization) {
	    case 0:
	      a_attr->SampleQuantization = 16;
	      break;
	    case 1:
	      a_attr->SampleQuantization = 20;
	      break;
	    case 2:
	      a_attr->SampleQuantization = 24;
	      break;
	    }
	    break;
	  case 6:
	    af = DVD_AUDIO_FORMAT_DTS;
	    break;
	  default:
	    WARNING("please send a bug report, unknown Audio format %d!", 
		    attr.audio_format);
	    break;
	  }
	  a_attr->AudioFormat = af;
	  a_attr->AppMode = attr.application_mode;
	  a_attr->LanguageExtension = attr.code_extension;
	  a_attr->Language = attr.lang_code;
	  a_attr->HasMultichannelInfo = attr.multichannel_extension;
	  switch(attr.sample_frequency) {
	  case 0:
	    a_attr->SampleFrequency = 48000;
	    break;
	  case 1:
	    a_attr->SampleFrequency = 96000;
	    break;
	  }
	  a_attr->NumberOfChannels = attr.channels + 1;
	  a_attr->AudioType = attr.lang_type;
	} else {
	  set_dvderror(&send_ev, DVD_SERIAL(&ev), DVD_E_Invalid);
	}
      }
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
    }
    break;
  case DVDCtrlGetCurrentSubpicture:
    {
      MsgEvent_t send_ev;
      int nS, cS;
      vm_get_subp_info(&nS, &cS);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentSubpicture;
      send_ev.dvdctrl.cmd.currentsubpicture.nrofstreams = nS;
      send_ev.dvdctrl.cmd.currentsubpicture.currentstream = cS & ~0x40;
      switch(subpicture_state) {
      case DVD_SUBPICTURE_STATE_FORCEDOFF:
      case DVD_SUBPICTURE_STATE_DISABLED:
	send_ev.dvdctrl.cmd.currentsubpicture.display = subpicture_state;
	break;
      default:
	send_ev.dvdctrl.cmd.currentsubpicture.display 
	  = (cS & 0x40) ? DVD_SUBPICTURE_STATE_ON : DVD_SUBPICTURE_STATE_OFF;
	break;
      }
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlIsSubpictureStreamEnabled:
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.subpicturestreamenabled.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlSubpictureStreamEnabled;
      send_ev.dvdctrl.cmd.subpicturestreamenabled.streamnr = streamN;
      send_ev.dvdctrl.cmd.subpicturestreamenabled.enabled =
	(vm_subp_stream_enabled(streamN)) ? DVDTrue : DVDFalse;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
    }
    break;
  case DVDCtrlGetSubpictureAttributes: // FIXME XXX $$$ Not done
    {
      MsgEvent_t send_ev;
      DVDSubpictureAttributes_t *s_attr;
      int streamN = ev.dvdctrl.cmd.subpictureattributes.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlSubpictureAttributes;
      send_ev.dvdctrl.cmd.subpictureattributes.streamnr = streamN;
      s_attr = &send_ev.dvdctrl.cmd.subpictureattributes.attr;

      {
	subp_attr_t attr;
	if(vm_get_subp_attr(streamN, &attr)) {
	  memset(s_attr, 0, sizeof(DVDSubpictureAttributes_t));
	  s_attr->Type  = attr.type;
	  s_attr->CodingMode  = attr.code_mode;
	  s_attr->Language  = attr.lang_code;
	  s_attr->LanguageExtension  = attr.code_extension;
	} else {
	  set_dvderror(&send_ev, DVD_SERIAL(&ev), DVD_E_Invalid);
	}
      }
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }	  
    break;
  case DVDCtrlGetCurrentAngle:
    {
      MsgEvent_t send_ev;
      int nS, cS;
      vm_get_angle_info(&nS, &cS);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentAngle;
      send_ev.dvdctrl.cmd.currentangle.anglesavailable = nS;
      send_ev.dvdctrl.cmd.currentangle.anglenr = cS;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetState:
    {
      MsgEvent_t send_ev;
      char *state_str;
      DVDCtrlLongStateEvent_t *state_ev;
      state_str = vm_get_state_str(get_current_block(ns));
      
      send_ev.type = MsgEventQDVDCtrlLong;
      send_ev.dvdctrllong.cmd.type = DVDCtrlLongState;
      state_ev = &(send_ev.dvdctrllong.cmd.state);
      if(state_str != NULL) {
	strncpy(state_ev->xmlstr, state_str, sizeof(state_ev->xmlstr));
	state_ev->xmlstr[sizeof(state_ev->xmlstr)-1] = '\0';
	
      } else {
	state_ev->xmlstr[0] = '\0';
      }
      
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
      
      free(state_str);
    }
    break;
  case DVDCtrlGetDiscID:
    {
      MsgEvent_t send_ev;

      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlDiscID;
      memcpy(send_ev.dvdctrl.cmd.discid.id, discid, sizeof(discid));
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetVolIds:
    {
      MsgEvent_t send_ev;
      int voltype;
      DVDCtrlLongVolIdsEvent_t *ids;

      send_ev.type = MsgEventQDVDCtrlLong;
      send_ev.dvdctrllong.cmd.type = DVDCtrlLongVolIds;
      
      ids = &(send_ev.dvdctrllong.cmd.volids);
      
      voltype = ev.dvdctrl.cmd.volids.voltype;

      ids->voltype = 0;
      if(voltype == 0) {
	if(vm_get_udf_volids(ids->volid, sizeof(ids->volid),
			     ids->volsetid, sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 1;
	} else if(vm_get_iso_volids(ids->volid, sizeof(ids->volid),
				    ids->volsetid, 
				    sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 2;
	}
      } else if(voltype == 1) {
	if(vm_get_udf_volids(ids->volid, sizeof(ids->volid),
			     ids->volsetid, sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 1;
	}
      } else if(voltype == 2) {
	if(vm_get_iso_volids(ids->volid, sizeof(ids->volid),
			     ids->volsetid, sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 2;
	}
      } 
	
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  default:
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
  }
  return res;
}

int process_long_user_data(MsgEvent_t ev, 
			   dvd_state_t *vmstate, nav_state_t *ns)
{
  int res = 0;

  //fprintf(stderr, "nav: User input, MsgEvent.type: %d\n", ev.type);
  
  switch(ev.dvdctrllong.cmd.type) {
  case DVDCtrlLongSetState:
    res = vm_set_state_str(ev.dvdctrllong.cmd.state.xmlstr);
    break;
  default:
    DNOTE("unknown (not handled) DVDCtrlLongEvent %d\n",
	  ev.dvdctrllong.cmd.type);
    break;
  }
  return res;
}




#define INF_STILL_TIME (10 * 0xff)

static void do_init_cell(dvd_state_t *vmstate, nav_state_t *ns, int flush) {
  uint32_t lbn, blk;
  uint32_t search_len;
  uint32_t serial;
  
  next_serial();
  ns->cell = &(vmstate->pgc->cell_playback[vmstate->cellN - 1]);
  ns->still_time = 10 * ns->cell->still_time;
  
  set_current_block(ns, vmstate->blockN);
  blk = get_current_block(ns);
  //  block = state.blockN;
  assert(nav_within_cell(ns->cell, blk));
  //assert(cell->first_sector + block <= cell->last_vobu_start_sector);

  // FIXME XXX $$$ Only send when needed, and do send even if not playing
  // from start? (should we do pre_commands when jumping to say part 3?)
  /* Send the palette to the spu. */
  send_spu_palette(vmstate->pgc->palette);

  lbn = blk2lbn(ns->cell, blk);
  search_len = cell_len(ns->cell) - blk;

  /* Get the pci/dsi data */
  if(flush) {
    serial = send_demux_nav(vmstate, lbn, search_len, FlowCtrlFlush);
  } else {
    serial = send_demux_nav(vmstate, lbn, search_len, FlowCtrlNone);
  }
#ifdef NAV_SEARCH_DEBUG
  WARNING("send_demux_nav (cellinit) (%x, %x), serial %x\n", 
	  lbn, search_len,  serial);  
#endif
  set_pending_nav(ns, lbn, serial);
}

static void reset_dvd(dvd_state_t *vmstate, nav_state_t *ns)
{
  fprintf(stderr, "\n*********** reset dvd ***************************\n");
  next_serial();
  send_demux_flush();
  fprintf(stderr, "\n*********** vm reset ***************************\n");
  vm_reset();

  interpret_config();
  
  vm_start(); // see hack in main
  do_init_cell(vmstate, ns, 0);
  
}

#if 0

#else

/*
 * wait for events (nav_pack || userop || timer)
 */



static int wait_for_msg(MsgEventQ_t *msgq, int timeout)
{
  int ret;
  struct pollfd fds[1];
  unsigned int nfds;
  fds[0].fd = msgq->socket.sd;
  fds[0].events = POLLIN;
  nfds = 1;
  
  // only check for messages
  ret = poll(fds, nfds, timeout);
  if(ret == -1) {
    ERROR("poll(): %s\n", strerror(errno));
    ret = -1;
  } else if(ret == 0) {
    //timeout
    ret = 0;
  } else {
    if((fds[0].revents & POLLIN) == POLLIN) {
      ret = 1;
    } else {
      ERROR("poll(): revents: %0x\n", fds[0].revents);
      ret = -1;
    }
  } 

  return ret;
}

#if 0
static int wait_msgq(MsgEventQ_t *msgq, int wait_for_nav, int timeout)
{
  q_head_t *q_head;
  q_elem_t *q_elems;
  int elem;
  volatile int *in_use;
  int ret;
  int rval;

  if(msgq->type != MsgEventQType_socket) {
    FATAL("%s", "socket needed, NI\n");
  }
  
  if(!wait_for_nav) {
    return wait_for_msg(msgq, timeout);
  }
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  in_use = &(q_elems[elem].in_use);
  if(!*in_use) {
    q_head->reader_requests_notification = 1;
    
    if(!*in_use) {
      return wait_for_msg(msgq, timeout);
    }
  }
  
  // nav pack available, check if there is a message also
  rval = 2;
  ret = wait_for_msg(msgq, 0);
  if(ret == 1) { 
    rval = 3;
  }
  
  
  return rval;
}

#endif
#if 0
int wait_for_events(MsgEventQ_t *msgq)
{
  int timer_timeout = next_timer();
  int still_time_timeout = still_time_left();
  int timeout = -1;
  int wait_for_nav;
  int r;
  
  timer_timeout = next_timer();
  still_time_timeout = still_time_left();
  timeout = -1;

  if(still_time_timeout) {
    timeout = still_time_left;
    wait_for_nav = 0;
  } else {
    //process navpacks only when not in still
    wait_for_nav = 1;    
  }
  if(timer_timeout < still_time_timeout) {
    timeout = timer_timeout;
  }
  
  r = wait_msgq(msgq, wait_for_nav, timeout);

  
  /*  process_timeout(timeout)
      if(cur_time > timer_end) {
       inc_counter_regs();
       dec_navtimer();
       timer_end = timer_end+1.0s;
       timer_start = cur_time;
      }
      if(still_time_start != 0) {
       if(cur_time > still_time_end) {
        still_time_start = 0;
	next_cell(vm);
	init_cell(vm);
       }
      }
  */
  if(userdata) {
    process_userdata();
  }
  if(navblovk) {
    nb = get_navblock();
    
    if(flush_in_progress()) {
      if(nav.serial == serial && nav.lbn == lbn) {
	//flush completed
	flush_done();	
      } else {
	// drop packet
	continue;
      }
    }
    process_nav(nb) {
      if dsi ...;
      if pci ...;
    }
  }
}

#endif

uint32_t next_vobu_offs(nav_state_t *ns) 
{
  return ns->dsi.vobu_sri.next_vobu & 0x3fffffff;
}

uint32_t nav_lbn(nav_state_t *ns)
{
  return ns->dsi.dsi_gi.nv_pck_lbn;
}

static void do_run(dvd_state_t *vmstate, nav_state_t *ns) {
  int32_t packet_serial;

  vm_start(); // see hack in main
  do_init_cell(vmstate, ns, 0);
  ns->pci.pci_gi.nv_pck_lbn = -1;
  ns->dsi.dsi_gi.nv_pck_lbn = -1;
  
  while(1) {
    MsgEvent_t ev;
    int got_data;
    uint32_t blk;
    static int last_pending = -1;
    static int last_serial = -1;

    // For now.. later use the time instead..
    /* Have we read the last dsi packet we asked for? Then request the next. */
    blk = get_current_block(ns);
    if(last_serial != packet_serial) {
      last_serial = packet_serial;
      last_pending = -1;
    }
    if((nav_lbn(ns) != -1) && (nav_lbn(ns) != last_pending)
       && (get_pending_serial(ns) == packet_serial)
       && nav_within_cell(ns->cell, blk)) {
      
      int complete_video;
      last_pending = nav_lbn(ns);
#ifdef NAV_SEARCH_DEBUG
      WARNING("got pending block(ser) %x(%x), serial (%x)\n",
	      get_pending_lbn(ns), get_pending_serial(ns), packet_serial); 
#endif
      /* If there is video data in this vobu, but not in the next. Then this 
	 data must be a complete image, so let the decoder know this. */
      if((ns->dsi.vobu_sri.next_vobu & 0x80000000) == 0 
	 && ns->dsi.dsi_gi.vobu_1stref_ea != 0 /* there is video in this */) {
	complete_video = FlowCtrlCompleteVideoUnit;
	//DNOTE("FlowCtrlCompleteVideoUnit = 1;\n");
      } else {
	complete_video = FlowCtrlNone;
      }
      
      /* Demux/play the content of this vobu. */
      if(ns->dsi.dsi_gi.vobu_ea != 0) {
	send_demux_sectors(vmstate, blk2lbn(ns->cell, blk) + 1, 
			   ns->dsi.dsi_gi.vobu_ea, complete_video);
      }
      
      /* VOBU still ? */
      /* if(cell->playback_mode) .. */
      /* need to defer the playing of the next VOBU untill the still
       * is interrupted.  */
	 
/* start - get_next_vobu() */     	 
      /* The next vobu is where... (make this a function?) */
      /* angle change points are at next ILVU, not sure if 
	 one VOBU = one ILVU */ 
      if(0 /*angle && change_angle*/) {
	/* if( seamless )
	   else // non seamless
	*/
	;
      } else {
	uint32_t next_nav;
	uint32_t blk = get_current_block(ns);
	/* .. top two bits are flags */  
	//block += dsi.vobu_sri.next_vobu & 0x3fffffff;
#ifdef NAV_SEARCH_DEBUG
	WARNING("block %x, next_vobu %x (%x)\n",
		blk, next_vobu_offs(ns),
		blk + next_vobu_offs(ns));
#endif
	next_nav = nav_lbn(ns) + next_vobu_offs(ns) - ns->cell->first_sector;
	set_current_block(ns, next_nav);
	
#ifdef NAV_SEARCH_DEBUG
	WARNING("nv_lbn %x, first %x next %x, (%x)\n",
		nav_lbn(ns),
		ns->cell->first_sector, next_nav,
		ns->cell->first_sector + next_nav);
#endif
      }

      blk = get_current_block(ns);
      /* TODO XXX $$$ Test earlier and merge the requests if posible? */
      /* If there is more data in this cell to demux, then get the
       * next nav pack. */
      if(nav_within_cell(ns->cell, blk)) {
	uint32_t lbn, search_len, serial;
	lbn = blk2lbn(ns->cell, blk);
	search_len = cell_len(ns->cell)-blk;
#ifdef NAV_SEARCH_DEBUG
	WARNING("send_demux_nav (%x, len %x), serial %x\n",
		lbn, search_len, get_serial());
#endif
	serial = send_demux_nav(vmstate, lbn, search_len, FlowCtrlNone);
	set_pending_nav(ns, lbn, serial);
      } else {
	//DNOTE("end of cell\n");
	; // end of cell!
	if(ns->still_time == INF_STILL_TIME) // Inf. still time
	  NOTE("%s", "Still picture select an item to continue.\n");
	else if(ns->still_time != 0)
	  NOTE("Pause for %d seconds,\n", ns->still_time/10);
#if 0 
	/* TODO XXX $$$ This should only be done at the correct time */
	/* Handle forced activate button here */
	if(pci.hli.hl_gi.foac_btnn != 0 && 
	   (pci.hli.hl_gi.hli_ss & 0x03) != 0) {
	  uint16_t button_nr = state.HL_BTNN_REG >> 10;
	  /* Forced action 0x3f means selected button, 
	     otherwise use the specified button */
	  if(pci.hli.hl_gi.foac_btnn != 0x3f)
	    button_nr = pci.hli.hl_gi.foac_btnn;
	  if(button_nr > pci.hli.hl_gi.btn_ns)
	    ; // error selected but out of range...
	  state.HL_BTNN_REG = button_nr << 10;
	  
	  if(vm_eval_cmd(&pci.hli.btnit[button_nr - 1].cmd)) {
	    do_init_cell(vmstate, ns, /* ?? */ 0);
	    dsi.dsi_gi.nv_pck_lbn = -1;
	  }
	}
#endif
      }
    } else {
#ifdef NAV_SEARCH_DEBUG
      WARNING("nav: pending %x(%x), got %x(%x)\n",
	      get_pending_lbn(ns), get_pending_serial(ns),
	      nav_lbn(ns), packet_serial);      
#endif
    }
    
    
    // Wait for data/input or for cell still time to end
    {
      if(nav_within_cell(ns->cell, get_current_block(ns))) {
	got_data = wait_q(msgq, &ev); // Wait for a data packet or a message
      } else { 
	/* Handle cell still time here */
	got_data = 0;
	if(ns->still_time == INF_STILL_TIME) { // Inf. still time
	  MsgNextEvent(msgq, &ev);
	} else {
	  while(ns->still_time && MsgCheckEvent(msgq, &ev)) {
	    struct timespec req = {0, 100000000}; // 0.1s 
	    nanosleep(&req, NULL);
	    ns->still_time--;
	  }
	}
	
	if(!ns->still_time) { // No more still time (or there never was any..)
	  if(MsgCheckEvent(msgq, &ev)) { // and no more messages
	    // Let the vm run and give us a new cell to play
	    vm_get_next_cell();
	    do_init_cell(vmstate, ns,  /* No jump */ 0);
	    ns->dsi.dsi_gi.nv_pck_lbn = -1;
	  }
	}
      }
    }
    /* If we are here we either have a message or an available data packet */ 
    
    
    /* User input events */
    if(!got_data) { // Then it must be a message (or error?)
      int res = 0;
      
      switch(ev.type) {
      case MsgEventQDVDCtrl:
	/* Do user input processing. Like audio change, 
	 * subpicture change and answer attribute query requests.
	 * access menus, pause, play, jump forward/backward...
	 */

	res = process_user_data(ev, vmstate, ns);
	break;
      case MsgEventQDVDCtrlLong:
	res = process_long_user_data(ev, vmstate, ns);
	break;

      default:
	handle_events(msgq, &ev);
	/* If( new dvdroot ) {
	   vm_reset(get_dvdroot());
	   block = 0;
	   res = 1;
	   }
	*/
      }      
      if(res != 0) {/* a jump has occured */
	do_init_cell(vmstate, ns,/* Flush streams */1);
	ns->dsi.dsi_gi.nv_pck_lbn = -1;
      }
      
    } else { // We got a data to read.
      unsigned char buffer[2048];
      int len;
      
      len = get_q(msgq, &buffer[0], &packet_serial);
#ifdef NAV_SEARCH_DEBUG
      WARNING("nav: pending_serial %x ,packet_serial: %x",
	      get_pending_serial(ns), packet_serial);
#endif
      if(buffer[0] == PS2_PCI_SUBSTREAM_ID) {
	navRead_PCI(&ns->pci, &buffer[1]);
	/* Is this the packet we are waiting for? */
	//	if(pci.pci_gi.nv_pck_lbn != pending_lbn) {
	if(get_pending_serial(ns) != packet_serial) {
	  WARNING("nav: Dropped PCI packet, pending %x(%x), got %x(%x)\n",
		  get_pending_lbn(ns), get_pending_serial(ns),
		  ns->pci.pci_gi.nv_pck_lbn, packet_serial);
	  ns->pci.pci_gi.nv_pck_lbn = -1;
	  continue;
	}
	//navPrint_PCI(&ns->pci);
	//fprintf(stdout, "nav: Got PCI packet\n");
	/*
	if(pci.hli.hl_gi.hli_ss & 0x03) {
	  fprintf(stdout, "nav: Menu detected\n");
	  navPrint_PCI(&pci);
	}
	*/
	/* Evaluate and Instantiate the new pci packet */
	process_pci(&ns->pci, &vmstate->HL_BTNN_REG);
        
      } else if(buffer[0] == PS2_DSI_SUBSTREAM_ID) {
	navRead_DSI(&ns->dsi, &buffer[1]);
	//	if(dsi.dsi_gi.nv_pck_lbn != pending_lbn) {
	if(get_pending_serial(ns) != packet_serial) {
	  WARNING("nav: Dropped DSI packet, pending %x(%x), got %x(%x)\n",
		  get_pending_lbn(ns), get_pending_serial(ns),
		  ns->dsi.dsi_gi.nv_pck_lbn, packet_serial);
	  //fprintf(stdout, "nav: Droped DSI packet\n");
	  ns->dsi.dsi_gi.nv_pck_lbn = -1;
	  continue;
	}
	//fprintf(stdout, "nav: Got DSI packet\n");
	//navPrint_DSI(&ns->dsi);

      } else {
	int i;
	ERROR("Unknown NAV packet type (%02x)\n", buffer[0]);
	for(i=0;i<20;i++)
	  printf("%02x ", buffer[i]);
      }
    }
    
  }
}

#endif
