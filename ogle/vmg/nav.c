/* Ogle - A video player
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
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include <ogle/msgevents.h>
#include <ogle/dvdevents.h>

#include <dvdread/nav_types.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>
#include "vm.h"

extern int wait_q(MsgEventQ_t *msgq, MsgEvent_t *ev); // com.c
extern int get_q(MsgEventQ_t *msgq, char *buffer);
extern void wait_for_init(MsgEventQ_t *msgq);
extern void handle_events(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_demux(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_spu(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern char *get_dvdroot(void);

extern dvd_state_t state;

static void do_run(void);


MsgEventQ_t *msgq;

char *program_name;

void usage(void)
{
  fprintf(stderr, "Usage: %s  -m <msgqid>\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int msgqid = -1;
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
      exit(1);
    }
  }
  
  if(msgqid == -1) {
    fprintf(stderr, "what?\n");
    exit(1);
  }
  {
    MsgEvent_t ev;
    
    if((msgq = MsgOpen(msgqid)) == NULL) {
      fprintf(stderr, "nav: couldn't get message q\n");
      exit(-1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_DVD_NAV;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "nav: register capabilities\n");
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DEMUX_MPEG2_PS | DEMUX_MPEG1;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "nav: didn't get demux cap\n");
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DECODE_DVD_SPU;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "nav: didn't get cap\n");
    }
    
    wait_for_init(msgq);
    
    /*  Call start here */
    vm_reset(get_dvdroot()); // hack placed here becaus it calles DVDOpen...

    ev.type = MsgEventQDemuxDVDRoot;
    strncpy(ev.demuxdvdroot.path, get_dvdroot(), sizeof(ev.demuxdvdroot.path));
    ev.demuxdvdroot.path[sizeof(ev.demuxdvdroot.path)-1] = '\0';
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "nav: didn't set dvdroot\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xe0; // Mpeg1/2 Video 
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "nav: didn't set demuxstream\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // AC3 1
    ev.demuxstream.subtype = 0x80;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "nav: didn't set demuxstream\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // SPU 1
    ev.demuxstream.subtype = 0x20;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "nav: didn't set demuxstream\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbf; // NAV
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "nav: didn't set demuxstream\n");
    }
  }
 
  
  //vm_reset(get_dvdroot());
  do_run();
  
  return 0;
}





/**
 * Update any info the demuxer needs, and then tell the demuxer
 * what range of sectors to process.
 */
static void send_demux_sectors(int start_sector, int nr_sectors, 
			       FlowCtrl_t flush) {
  static int audio_stream_id = -1;
  static int subp_stream_id = -1;
  MsgEvent_t ev;
  
  /* Tell the demuxer which audio stream to demux */ 
  {
    int sN = vm_get_audio_stream(state.AST_REG);
    if(sN < 0 || sN > 7) sN = 7; // XXX == -1 for _no audio_
    if(sN != audio_stream_id) {
      audio_stream_id = sN;
      
      fprintf(stderr, "nav: sending audio demuxstream %d\n", sN);
      
      ev.type = MsgEventQDemuxStreamChange;
      ev.demuxstreamchange.stream_id = 0xbd; // AC3
      ev.demuxstreamchange.subtype = 0x80 | audio_stream_id;
      if(send_demux(msgq, &ev) == -1) {
	fprintf(stderr, "nav: error, didn't set demuxstream\n");
      }
    }
  }

  /* Tell the demuxer which subpicture stream to demux */ 
  {
    int sN = vm_get_subp_active_stream();
    if(sN < 0 || sN > 31) sN = 31; // XXX == -1 for _no audio_
    if(sN != subp_stream_id) {
      subp_stream_id = sN;
      
      fprintf(stderr, "nav: sending subp demuxstream %d\n", sN);
    
      ev.type = MsgEventQDemuxStreamChange;
      ev.demuxstreamchange.stream_id = 0xbd; // SPU
      ev.demuxstreamchange.subtype = 0x20 | subp_stream_id;
      if(send_demux(msgq, &ev) == -1) {
	fprintf(stderr, "nav: error, didn't set demuxstream\n");
      }
    }
  }

  /* Tell the demuxer what file and which sectors to demux. */
  ev.type = MsgEventQDemuxDVD;
  if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN)
    ev.demuxdvd.titlenum = 0;
  else
    ev.demuxdvd.titlenum = state.vtsN;
  if(state.domain == VTS_DOMAIN)
    ev.demuxdvd.domain = DVD_READ_TITLE_VOBS;
  else
    ev.demuxdvd.domain = DVD_READ_MENU_VOBS;
  ev.demuxdvd.block_offset = start_sector;
  ev.demuxdvd.block_count = nr_sectors;
  ev.demuxdvd.flowcmd = flush;
  if(send_demux(msgq, &ev) == -1) {
    fprintf(stderr, "nav: error, send_demux_dvd\n");
  }
  //fprintf(stderr, "nav: sent demux_dvd (%d,%d)\n", start_sector, nr_sectors);
}

static void send_spu_palette(uint32_t palette[16]) {
  MsgEvent_t ev;
  int i;
  
  ev.type = MsgEventQSPUPalette;
  for(i = 0; i < 16; i++) {
    ev.spupalette.colors[i] = palette[i];
  }
  
  if(send_spu(msgq, &ev) == -1) {
    fprintf(stderr, "nav: send_spu_palette\n");
  }
}

static void send_highlight(int x_start, int y_start, int x_end, int y_end, 
			   uint32_t btn_coli) {
  MsgEvent_t ev;
  int i;
  
  ev.type = MsgEventQSPUHighlight;
  ev.spuhighlight.x_start = x_start;
  ev.spuhighlight.y_start = y_start;
  ev.spuhighlight.x_end = x_end;
  ev.spuhighlight.y_end = y_end;
  for(i = 0; i < 4; i++)
    ev.spuhighlight.color[i] = 0xf & (btn_coli >> (16 + 4*i));
  for(i = 0; i < 4; i++)
    ev.spuhighlight.contrast[i] = 0xf & (btn_coli >> (4*i));

  if(send_spu(msgq, &ev) == -1) {
    fprintf(stderr, "nav: send_highlight\n");
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
  int is_action = 0;

  /* MORE CODE HERE :) */
  
  /* Paranoia.. */
  
  // No highlight/button pci info to use
  if((pci->hli.hl_gi.hli_ss & 0x03) == 0)
    return 0;
  // Selected buton > than max button ?
  // FIXME $$$ check how btn_ofn affects things like this
  if(button_nr > pci->hli.hl_gi.btn_ns)
    button_nr = 1;
  
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
    button_nr = ce->button.nr;
    break;
  case DVDCtrlMouseActivate:
    is_action = 1;
    /* Fall through */
  case DVDCtrlMouseSelect:
    {
      int button;
      unsigned int x = ce->mouse.x;
      unsigned int y = ce->mouse.y;
      
      button = mouse_over_hl(pci, x, y);
      if(button)
	button_nr = button;
      else
	is_action = 0;
    }
    break;
  default:
    fprintf(stderr, "nav: Unknown ui->cmd\n");
    break;
  }
  
  /* Must check if the current selected button has auto_action_mode !!! */
  /* Don't do auto action if it's been selected with the mouse... ?? */
  switch(pci->hli.btnit[button_nr - 1].auto_action_mode) {
  case 0:
    break;
  case 1:
    if(ce->type != DVDCtrlMouseSelect) {
      fprintf(stderr, "nav: auto_action_mode set!\n");
      is_action = 1;
    }
    break;
  case 2:
  case 3:
  default:
    fprintf(stderr, "nav: Unknown auto_action_mode!! btn: %d\n", button_nr);
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
      fprintf(stderr, "nav: forced select button %d\n", 
	      pci->hli.hl_gi.fosl_btnn);
    }
  }
  
  /* Paranoia.. */
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0
     && button_nr > pci->hli.hl_gi.btn_ns) {
    button_nr = 1;
  }

  /* FIXME TODO XXX $$$ */
  
  /* Determine the correct area and send the information to the spu decoder. */
  /* Possible optimization: don't send if its the same as last time. 
     same, as in same hli info, same button number and same select/action state
     Note this send a highlight even if hli_ss == 0, it then turns the
     highlight off. */
  {
    btni_t *button = &pci->hli.btnit[button_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][0]);
  }
  
  /* Write the (updated) value back to the button register. */  
  *btn_reg = button_nr << 10;
}







static int pending_lbn;
static int block;
static int still_time;
static cell_playback_t *cell;


static void do_init_cell(int flush) {
  
  cell = &state.pgc->cell_playback_tbl[state.cellN - 1];
  still_time = 10 * cell->still_time;

  block = state.blockN;
  assert(cell->first_sector + block <= cell->last_vobu_start_sector);

  // FIXME XXX $$$ Only send when needed, and do send even if not playing
  // from start? (should we do pre_commands when jumping to say part 3?)
  /* Send the palette to the spu. */
  send_spu_palette(state.pgc->palette);
  
  /* Get the pci/dsi data */
  if(flush)
    send_demux_sectors(cell->first_sector + block, 1, FlowCtrlFlush);
  else
    send_demux_sectors(cell->first_sector + block, 1, FlowCtrlNone);
  
  pending_lbn = cell->first_sector + block;
}



static void do_run(void) {
  pci_t pci;
  dsi_t dsi;
  
  vm_start(); // see hack in main
  do_init_cell(0);
  pci.pci_gi.nv_pck_lbn = -1;
  dsi.dsi_gi.nv_pck_lbn = -1;
  
  
  while(1) {
    MsgEvent_t ev;
    int got_data;
    
    // For now.. later use the time instead..
    /* Have we read the last dsi packet we asked for? Then request the next. */
    if(pending_lbn == dsi.dsi_gi.nv_pck_lbn
       && cell->first_sector + block <= cell->last_vobu_start_sector) {
      int complete_video;
      
      /* If there is video data in this vobu, but not in the next. Then this 
	 data must be a complete image, so let the decoder know this. */
      if((dsi.vobu_sri.next_vobu & 0x80000000) == 0 
	 && dsi.dsi_gi.vobu_1stref_ea != 0 /* there is video in this */) {
	complete_video = FlowCtrlCompleteVideoUnit;
	fprintf(stderr, "nav: FlowCtrlCompleteVideoUnit = 1;\n");
      } else {
	complete_video = FlowCtrlNone;
      }
      
      /* Demux/play the content of this vobu. */
      if(dsi.dsi_gi.vobu_ea != 0) {
	send_demux_sectors(cell->first_sector + block + 1, 
			   dsi.dsi_gi.vobu_ea, complete_video);
      }
      
      /* The next vobu is where... (make this a function?) */
      
      if(0 /*angle && change_angle*/) {
	/* if( seamless )
	   else // non seamless
	*/
	;
      } else {
	/* .. top two bits are flags */  
	block += dsi.vobu_sri.next_vobu & 0x3fffffff;
      }
      
      
      /* TODO XXX $$$ Test earlier and merge the requests if posible? */
      /* If there is more data in this cell to demux, then get the
       * next nav pack. */
      if(cell->first_sector + block <= cell->last_vobu_start_sector) {
	send_demux_sectors(cell->first_sector + block, 1, FlowCtrlNone);
	pending_lbn = cell->first_sector + block;
      } else {
	fprintf(stderr, "nav: end of cell\n");
	; // end of cell!
	
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
	    do_init_cell(/* ?? */ 0);
	    dsi.dsi_gi.nv_pck_lbn = -1;
	  }
	}
#endif
      }
    }
    
    
    // Wait for data/input or for cell still time to end
    {
      if(cell->first_sector + block <= cell->last_vobu_start_sector) {
	got_data = wait_q(msgq, &ev); // Wait for a data packet or a message
      
      } else { 
	/* Handle cell still time here */
	got_data = 0;
	if(cell->still_time == 0xff) // Inf. still time
	  MsgNextEvent(msgq, &ev);
	else
	  while(still_time && MsgCheckEvent(msgq, &ev)) {
	    struct timespec req = {0, 100000000}; // 0.1s 
	    nanosleep(&req, NULL);
	    still_time--;
	  }
	
	if(!still_time) // No more still time (or there never was any..)
	  if(MsgCheckEvent(msgq, &ev)) { // and no more messages
	    // Let the vm run and give us a new cell to play
	    vm_get_next_cell();
	    do_init_cell(/* No jump */ 0);
	    dsi.dsi_gi.nv_pck_lbn = -1;
	  }
      }
    }
    /* If we are here we either have a message or an available data packet */ 
    
    
    
    /* User input events */
    if(!got_data) { // Then it must be a message (or error?)
      int res = 0;
      
      printf("nav: User input, MsgEvent.type: %d\n", ev.type);
          
      /* Do user input processing. Like audio change, 
       * subpicture change and answer attribute querry requests.
       * access menus, pause, play, jump forward/backward...
       */
      switch(ev.type) {
      case MsgEventQDVDCtrl:
	
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
	  
	  if(cell->first_sector <= pci.pci_gi.nv_pck_lbn
	     && cell->last_vobu_start_sector >= pci.pci_gi.nv_pck_lbn) {
	    /* Update selected/activated button, send highlight info to spu */
	    /* Returns true if a button is activated */
	    if(process_button(&ev.dvdctrl.cmd, &pci, &state.HL_BTNN_REG)) {
	      int button_nr = state.HL_BTNN_REG >> 10;
	      res = vm_eval_cmd(&pci.hli.btnit[button_nr - 1].cmd);
	    }
	  }
	  break;
	  
	case DVDCtrlMenuCall:
	  fprintf(stderr, "nav: Menu %i\n", ev.dvdctrl.cmd.menucall.menuid);
	  res = vm_menu_call(ev.dvdctrl.cmd.menucall.menuid, block);
	  break;
	  
	case DVDCtrlResume:
	  res = vm_resume();
	  break;
	  
	case DVDCtrlGoUp:
	case DVDCtrlForwardScan:
	case DVDCtrlBackwardScan:
	  fprintf(stderr, "nav: Unknown (not handled) DVDCtrlEvent %d\n",
		  ev.dvdctrl.cmd.type);
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
	case DVDCtrlPTTPlay:
	case DVDCtrlTitlePlay:
	case DVDCtrlTimeSearch:
	case DVDCtrlTimePlay:
	case DVDCtrlPauseOn:
	case DVDCtrlPauseOff:
	case DVDCtrlStop:
	  fprintf(stderr, "nav: Unknown (not handled) DVDCtrlEvent %d\n",
		  ev.dvdctrl.cmd.type);
	  break;
	case DVDCtrlAngleChange:
	  state.AGL_REG = ev.dvdctrl.cmd.anglechange.anglenr;
	  break;
	case DVDCtrlAudioStreamChange: // FIXME $$$ Temorary hack
	  state.AST_REG = ev.dvdctrl.cmd.audiostreamchange.streamnr; // XXX
	  break;
	case DVDCtrlSubpictureStreamChange: // FIXME $$$ Temorary hack
	  state.SPST_REG &= 0x40; // Keep the on/off bit.
	  state.SPST_REG |= ev.dvdctrl.cmd.subpicturestreamchange.streamnr;
	  fprintf(stderr, "DVDCtrlSubpictureStreamChange %x\n",state.SPST_REG);
	  break;
	case DVDCtrlSetSubpictureState:
	  if(ev.dvdctrl.cmd.subpicturestate.display == DVDTrue)
	    state.SPST_REG |= 0x40; // Turn it on
	  else
	    state.SPST_REG &= ~0x40; // Turn it off
	  fprintf(stderr, "DVDCtrlSetSubpictureState %x\n",state.SPST_REG);
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
	      (vm_get_audio_stream(streamN) != -1) ? DVDTrue : DVDFalse;
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
	      DVDAudioFormat_t af = DVD_AUDIO_FORMAT_Other;
	      audio_attr_t attr = vm_get_audio_attr(streamN);
	      memset(&send_ev.dvdctrl.cmd.audioattributes.attr, 0, 
		     sizeof(DVDAudioAttributes_t)); //TBD
	      switch(attr.audio_format) {
	      case 0:
		af = DVD_AUDIO_FORMAT_AC3;
		break;
	      case 2:
		af = DVD_AUDIO_FORMAT_MPEG1;
		break;
	      case 3:
		af = DVD_AUDIO_FORMAT_MPEG2;
		break;
	      case 4:
		af = DVD_AUDIO_FORMAT_LPCM;
		break;
	      case 6:
		af = DVD_AUDIO_FORMAT_DTS;
		break;
	      default:
		printf("(please send a bug report) ");
		break;
	      }
	      send_ev.dvdctrl.cmd.audioattributes.attr.AudioFormat 
		= af;
	      send_ev.dvdctrl.cmd.audioattributes.attr.LanguageExtension
		= attr.audio_type;
	      send_ev.dvdctrl.cmd.audioattributes.attr.Language 
		= attr.lang_code;
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
	    send_ev.dvdctrl.cmd.currentsubpicture.display 
	      = (cS & 0x40) ? DVDTrue : DVDFalse;
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
	      (vm_get_subp_stream(streamN) != -1) ? DVDTrue : DVDFalse;
	    MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
	  }
	  break;
 	case DVDCtrlGetSubpictureAttributes: // FIXME XXX $$$ Not done
	  {
	    MsgEvent_t send_ev;
	    int streamN = ev.dvdctrl.cmd.subpictureattributes.streamnr;
	    send_ev.type = MsgEventQDVDCtrl;
	    send_ev.dvdctrl.cmd.type = DVDCtrlSubpictureAttributes;
	    send_ev.dvdctrl.cmd.subpictureattributes.streamnr = streamN;
	    {
	      subp_attr_t attr = vm_get_subp_attr(streamN);
	      memset(&send_ev.dvdctrl.cmd.subpictureattributes.attr, 0, 
		     sizeof(DVDSubpictureAttributes_t)); //TBD
	      send_ev.dvdctrl.cmd.subpictureattributes.attr.Language 
		= attr.lang_code;
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
	default:
	  fprintf(stderr, "nav: Unknown (not handled) DVDCtrlEvent %d\n",
		  ev.dvdctrl.cmd.type);
	  break;
	}
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
	do_init_cell(/* Flush streams */1);
	dsi.dsi_gi.nv_pck_lbn = -1;
      }
      
    } else { // We got a data to read.
      char buffer[2048];
      int len;
      
      len = get_q(msgq, &buffer[0]);
      
      if(buffer[0] == PS2_PCI_SUBSTREAM_ID) {
	navRead_PCI(&pci, &buffer[1], len);
	/* Is this the packet we are waiting for? */
	if(pci.pci_gi.nv_pck_lbn != pending_lbn) {
	  //fprintf(stdout, "nav: Droped PCI packet\n");
	  pci.pci_gi.nv_pck_lbn = -1;
	  continue;
	}
	//fprintf(stdout, "nav: Got PCI packet\n");
	/*
	if(pci.hli.hl_gi.hli_ss & 0x03) {
	  fprintf(stdout, "Menu detected\n");
	  navPrint_PCI(&pci);
	}
	*/
	/* Evaluate and Instantiate the new pci packet */
	process_pci(&pci, &state.HL_BTNN_REG);
	  
      } else if(buffer[0] == PS2_DSI_SUBSTREAM_ID) {
	navRead_DSI(&dsi, &buffer[1], len);
	if(dsi.dsi_gi.nv_pck_lbn != pending_lbn) {
	  //fprintf(stdout, "nav: Droped DSI packet\n");
	  dsi.dsi_gi.nv_pck_lbn = -1;
	  continue;
	}
	//fprintf(stdout, "nav: Got DSI packet\n");	  
	//navPrint_DSI(&dsi);

      } else {
	int i;
	fprintf(stderr, "nav: Unknown NAV packet type\n");
	for(i=0;i<20;i++)
	  printf("%02x ", buffer[i]);
      }
    }
    
  }
  
}
