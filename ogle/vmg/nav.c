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

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include <ogle/msgevents.h>
#include <ogle/dvdevents.h>

#include "ifo.h" // vm_cmd_t
#include "nav.h"
#include "nav_read.h"
#include "nav_print.h"
#include "vm.h"

extern int wait_q(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int get_q(MsgEventQ_t *msgq, char *buffer);
extern void handle_events(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_demux(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_spu(MsgEventQ_t *msgq, MsgEvent_t *ev);

// vm.c
extern MsgEventQ_t *msgq;
extern dvd_state_t state;
extern int reset_vm(void);
extern int start_vm(void);
extern int eval_cmd(vm_cmd_t *cmd);
extern int get_next_cell();
extern int vm_menuCall(int menuid, int block);
extern int vm_resume(void);
extern int get_Audio_stream(int audioN);
extern int get_Spu_stream(int spuN);
extern int get_Audio_info(int *num_avail, int *current);
extern int get_Spu_info(int *num_avail, int *current);





static void send_demux_sectors(int start_sector, int nr_sectors, 
			       FlowCtrl_t flush) {
  MsgEvent_t ev;
  
  ev.type = MsgEventQPlayCtrl;
  ev.playctrl.cmd = PlayCtrlCmdPlayFromTo;
  ev.playctrl.from = start_sector * 2048;
  ev.playctrl.to = (start_sector + nr_sectors) * 2048;
  ev.playctrl.flowcmd = flush;
  if(send_demux(msgq, &ev) == -1) {
    fprintf(stderr, "vm: send_demux_sectors\n");
  }
}

void send_use_file(char *file_name) {
  MsgEvent_t ev;
  
  ev.type = MsgEventQChangeFile;
  strncpy(ev.changefile.filename, file_name, PATH_MAX);
  ev.changefile.filename[PATH_MAX] = '\0';
  
  if(send_demux(msgq, &ev) == -1) {
    fprintf(stderr, "vm: send_use_file\n");
  }
}

void set_spu_palette(uint32_t palette[16]) {
  MsgEvent_t ev;
  int n;
  
  ev.type = MsgEventQSPUPalette;
  for(n = 0; n < 16; n++) {
    ev.spupalette.colors[n] = palette[n];
  }
  
  if(send_spu(msgq, &ev) == -1) {
    fprintf(stderr, "vm: set_spu_palette\n");
  }
}

void send_highlight(int x_start, int y_start, int x_end, int y_end, 
		    uint32_t btn_coli) {
  MsgEvent_t ev;
  int n;
  
  ev.type = MsgEventQSPUHighlight;
  ev.spuhighlight.x_start = x_start;
  ev.spuhighlight.y_start = y_start;
  ev.spuhighlight.x_end = x_end;
  ev.spuhighlight.y_end = y_end;
  for(n = 0; n < 4; n++)
    ev.spuhighlight.color[n] = 0xf & (btn_coli >> (16 + 4*n));
  for(n = 0; n < 4; n++)
    ev.spuhighlight.contrast[n] = 0xf & (btn_coli >> (4*n));

  if(send_spu(msgq, &ev) == -1) {
    fprintf(stderr, "vm: send_highlight\n");
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

int process_button(DVDCtrlEvent_t *ce, pci_t *pci, uint16_t *btn_reg) {
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
    button_nr = pci->hli.btnit[button_nr-1].up;
    break;
  case DVDCtrlLowerButtonSelect:
    button_nr = pci->hli.btnit[button_nr-1].down;
    break;
  case DVDCtrlLeftButtonSelect:
    button_nr = pci->hli.btnit[button_nr-1].left;
    break;
  case DVDCtrlRightButtonSelect:
    button_nr = pci->hli.btnit[button_nr-1].right;
    break;
  case DVDCtrlButtonActivate:
    is_action = 1;
    break;
  case DVDCtrlButtonSelectAndActivate:
    is_action = 1;
    button_nr = ce->button.nr;
    break;
  case DVDCtrlButtonSelect:
    button_nr = ce->button.nr;
    break;
  case DVDCtrlMouseSelect:
  case DVDCtrlMouseActivate:
    {
      int button;
      unsigned int x = ce->mouse.x;
      unsigned int y = ce->mouse.y;
      
      button = mouse_over_hl(pci, x, y);
      if(button) {
	button_nr = button;
	if(ce->type == DVDCtrlMouseActivate)
	  is_action = 1;
      }
    }
    break;
  default:
    fprintf(stderr, "vmg: Unknown ui->cmd\n");
    break;
  }
  
  /* Must check if the current selected button has auto_action_mode !!! */
  /* Don't do auto action if it's been selected with the mouse... ?? */
  switch(pci->hli.btnit[button_nr - 1].auto_action_mode) {
  case 0:
    break;
  case 1:
    if(ce->type != DVDCtrlMouseSelect) {
      fprintf(stderr, "!!!auto_action_mode!!!\n");
      is_action = 1;
      }
    break;
  case 2:
  case 3:
  default:
    fprintf(stderr, "Unknown auto_action_mode!! btn: %d\n", button_nr);
    exit(1);
  }
  
  /* If a new button has been selected or if one has been activated. */
  /* Determine the correct area and send the information to the spu decoder. */
  /* Possible optimization: don't send if its the same as last time. */
  {
    btni_t *button;
    button = &pci->hli.btnit[button_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][is_action]);
  }
  
  /* Write the (updated) value to the button register. */
  *btn_reg = button_nr << 10;
  return is_action;
}


static void process_pci(pci_t *pci, uint16_t *btn_nr) {
  
  /* Check if this is alright, i.e. pci->hli.hl_gi.hli_ss == 1 only 
     for the first menu pic packet? What about looping menus */
  
  // FIXME TODO XXX $$$ Does not work for menu that only have one nav pack!
  
  if(pci->hli.hl_gi.hli_ss == 1) {
    if(pci->hli.hl_gi.fosl_btnn != 0)
      *btn_nr = pci->hli.hl_gi.fosl_btnn;
  }
  
  /* Paranoia.. */
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0
     && *btn_nr > pci->hli.hl_gi.btn_ns) {
    *btn_nr = 1;
  }

  /* FIXME TODO XXX $$$ */
  
  /* Determine the correct area and send the information to the spu decoder. */
  /* Possible optimization: don't send if its the same as last time. */
  {
    btni_t *button;
    button = &pci->hli.btnit[*btn_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][0]);
  }
  
}






int pending_packets, discard_packets;
int block;
int still_time;
cell_playback_t *cell;



void do_init_cell(void) {
  
  discard_packets += pending_packets;
  
  cell = &state.pgc->cell_playback_tbl[state.cellN - 1];
  still_time = cell->still_time;
  
  {
    char vob_name[16];
    // Make file name
    if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
      snprintf(vob_name, 14, "VIDEO_TS.VOB");
    } else {
      char part = '0'; 
      if(state.domain != VTSM_DOMAIN) // This is wrong
	part += cell->first_sector/(1024  * 1024 * 1024 / 2048) + 1;
      snprintf(vob_name, 14, "VTS_%02i_%c.VOB", state.vtsN, part);
    }
    printf("%s\t", vob_name);
    printf("VOB ID: %3i, Cell ID: %3i at sector: 0x%08x - 0x%08x\n",
	   state.pgc->cell_position_tbl[state.cellN - 1].vob_id_nr,
	   state.pgc->cell_position_tbl[state.cellN - 1].cell_nr,
	   cell->first_sector, cell->last_sector);

    fprintf(stderr, "filename: %s\n", vob_name);
    send_use_file(vob_name);
  }
  
  block = state.blockN;
  assert(cell->first_sector + block <= cell->last_sector);
  
  /* Get the pci/dsi data */
  send_demux_sectors(cell->first_sector + block, 1, 0);
  pending_packets += 2;
}

void do_next_cell(void) {
  
  // New_cell
  get_next_cell();
  block = 0; // or rsm_block??, get it from get_next_cell()!!

  do_init_cell();
}



void do_run(void) {
  pci_t pci;
  dsi_t dsi;
  
  memset(&pci, 0, sizeof(pci_t));
  memset(&dsi, 0, sizeof(dsi_t));
  
  start_vm();
  pending_packets = 0;
  discard_packets = 0;
  block = 0;
  do_init_cell();
  
  
  while(1) {
    /* To avoid doing << 10 and >> 10 all the time we make a local copy */

    MsgEvent_t ev;
    int got_data;
    
    
    // For now.. later use the time instead..
    
    /* If pending packets == 0 then we must have read a pci and a dsi packet */
    if(pending_packets == 0
       && cell->first_sector + block <= cell->last_sector) {
      int flush_video;
      
      /* Is there any video data in the next vobu? */
      if((dsi.vobu_sri.next & 0x80000000)== 0) {
	flush_video = FlowCtrlCompleteVideoUnit;
	fprintf(stderr, "flush_video = 1;\n");
      } else {
	flush_video = FlowCtrlNone;
      }
      
      /* Demux/play the content of this vobu */
      send_demux_sectors(cell->first_sector + block + 1, 
			 dsi.dsi_gi.vobu_ea, flush_video);
      
      
      /* The next vobu is where... (make this a function) */
      
      if(0 /*angle && change_angle*/) {
	;
      } else {
	/* .. top two bits are flags */  
	block += dsi.vobu_sri.next & 0x3fffffff;
      }
      
      
      /* TODO XXX $$$ Test earlier and merge the requests if posible? */
      /* If there is more data in this cell to demux, then get the
       * next nav pack. */
      if(cell->first_sector + block <= cell->last_sector) {
	send_demux_sectors(cell->first_sector + block, 1, FlowCtrlNone);
	pending_packets += 2;
      } else {
	; // end of cell!
	
#if 0 
	/* TODO XXX $$$ This should only be done at the correct time */
	/* Handle forced activate button here */
	if(pci.hli.hl_gi.foac_btnn != 0 && 
	   (pci.hli.hl_gi.hli_ss & 0x03) != 0) {
	  uint16_t sl_button_nr = state.HL_BTNN_REG >> 10;
	  
	  /* Forced action 0x3f means selected button, 
	     otherwise use the specified button */
	  if(pci.hli.hl_gi.foac_btnn != 0x3f)
	    sl_button_nr = pci.hli.hl_gi.foac_btnn;
	  if(sl_button_nr > pci.hli.hl_gi.btn_ns)
	    ; // error selected but out of range...
	  
	  state.HL_BTNN_REG = sl_button_nr << 10;
	  if(eval_cmd(&pci.hli.btnit[sl_button_nr - 1].cmd)) {
	    do_init_cell();
	  }
	}
#endif   	
      }
    }

    
    { // Wait for data/input or time out of still
      if(pending_packets != 0) {
	assert(pending_packets > 0);
	got_data = wait_q(msgq, &ev); // Wait for a data packet or a message
      } else { 
	/* Handle cell pause and still time here */
	got_data = 0;
	if(cell->still_time != 0xff)
	  while(still_time && MsgCheckEvent(msgq, &ev)) {
	    sleep(1); still_time--;
	  }
	else // Inf. still time
	  MsgNextEvent(msgq, &ev);
	
	if(!still_time) // No more still time
	  if(MsgCheckEvent(msgq, &ev)) // and no more messages
	    do_next_cell();  // ???? XXXX ???? or demux more?
      }
    }
    /* If we are here we either have a message or an available data packet */ 
    
    
    
    if(!got_data) { // Then it must be a message (or error?)
      int res = 0;
      
      printf("User input / Message type: %d\n", ev.type);
      
      /* User input events */
      
      /* FIXME XXX $$$ Watch out so that we don't use pci/dsi before 
	 reading the first packet!!! */
      
      /* Do user input processing. Like audio change, 
       * subpicture change and answer attribute querry requests.
       * access menus, pause, play, jump forward/backward...
       */
      
      
      /* FIXME XXX $$$ Watch out so that we don't use pci/dsi before 
	 reading the first packet!!! */
      
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
	  
	  if(/*have_pci &&*/ discard_packets == 0) {
	    /* Update selected/activated button, send highlight info to spu */
	    /* Returns true if a button is activated */
	    if(process_button(&ev.dvdctrl.cmd, &pci, &state.HL_BTNN_REG)) {
	      res = eval_cmd(&pci.hli.btnit[(state.HL_BTNN_REG>>10) - 1].cmd);
	    }
	  }
	  break;
	  
	case DVDCtrlMenuCall:
	  if(discard_packets == 0) {
	    res = vm_menuCall(ev.dvdctrl.cmd.menucall.menuid, block);
	  }
	  break;
	  
	case DVDCtrlResume:
	  if(discard_packets == 0) {
	    res = vm_resume();
	  }
	  break;
	  
	case DVDCtrlGoUp:
	case DVDCtrlForwardScan:
	case DVDCtrlBackwardScan:
	case DVDCtrlNextPGSearch:
	case DVDCtrlPrevPGSearch:
	case DVDCtrlTopPGSearch:
	case DVDCtrlPTTSearch:
	case DVDCtrlPTTPlay:
	case DVDCtrlTitlePlay:
	case DVDCtrlTimeSearch:
	case DVDCtrlTimePlay:
	case DVDCtrlPauseOn:
	case DVDCtrlPauseOff:
	case DVDCtrlStop:
	
	case DVDCtrlGetCurrentAudio:
	  {
	    MsgEvent_t send_ev;
	    int nS, cS;
	    get_Audio_info(&nS, &cS);
	    send_ev.type = MsgEventQDVDCtrl;
	    send_ev.dvdctrl.cmd.type = DVDCtrlCurrentAudio;
	    send_ev.dvdctrl.cmd.currentaudio.nrofstreams = nS;
	    send_ev.dvdctrl.cmd.currentaudio.currentstream = cS;
	    MsgSendEvent(msgq, ev.any.client, &send_ev);
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
	      (get_Audio_stream(streamN) != -1) ? DVDTrue : DVDFalse;
	    MsgSendEvent(msgq, ev.any.client, &send_ev);	    
	  }
	  break;
	case DVDCtrlGetAudioAttributes: // FIXME XXX $$$ Not done
	  {
	    MsgEvent_t send_ev;
	    send_ev.type = MsgEventQDVDCtrl;
	    send_ev.dvdctrl.cmd.type = DVDCtrlAudioAttributes;
	    send_ev.dvdctrl.cmd.audioattributes.streamnr 
	      = ev.dvdctrl.cmd.audiostreamenabled.streamnr;
	    memset(&send_ev.dvdctrl.cmd.audioattributes.attr, 0, 
		   sizeof(DVDAudioAttributes_t)); //TBD
	    MsgSendEvent(msgq, ev.any.client, &send_ev);	    
	  }
	  break;
	  
	default:
	  fprintf(stderr, "Unknown (not handled) DVDCtrlEvent %d\n",
		  ev.dvdctrl.cmd.type);
	  break;
	}
	break;
      default:
	handle_events(msgq, &ev);
      }
      
      if(res != 0) {/* a jump has occured */
	do_init_cell();
      }
      
    } else { // We got a data to read.
      char buffer[2048];
      int len;
      
      assert(pending_packets > 0);
      
      len = get_q(msgq, &buffer[0]);
      
      if(discard_packets > 0) {
	pending_packets--;
	discard_packets--;
	
      } else if(buffer[0] == PS2_PCI_SUBSTREAM_ID) {
	assert(pending_packets == 2);
	pending_packets--;
	/* XXX inte läsa till pci utan något annat minne? */
	read_pci_packet(&pci, &buffer[1], len);
	
	if(pci.hli.hl_gi.hli_ss & 0x03) {
	  fprintf(stdout, "Menu detected\n");
	  //print_pci_packet(stdout, &pci);
	}
	
	/* !!! Evaluate and Instantiate the new pci packets !!! */
	{
	  uint16_t sl_button_nr = state.HL_BTNN_REG >> 10;
	  process_pci(&pci, &sl_button_nr);
	  state.HL_BTNN_REG = sl_button_nr << 10;
	}
      } else if(buffer[0] == PS2_DSI_SUBSTREAM_ID) {
	assert(pending_packets == 1);
	pending_packets--;
	/* XXX inte läsa till dsi utan något annat minne? */
	read_dsi_packet(&dsi, &buffer[1], len);
	//print_dsi_packet(stderr, &dsi);
	  
      } else {
	fprintf(stderr, "vmg: Unknown NAV packet type");
      }
    }
    
  }
  
}
