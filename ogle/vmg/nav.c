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

//#include "msgtypes.h"
#include "msgevents.h"
#include "dvdevents.h"

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

int process_button(DVDCtrlEvent_t *ce, pci_t *pci, uint16_t *btn_nr) {
  int is_action = 0;
  
  /* MORE CODE HERE :) */
  
  /* Paranoia.. */
  
  // No highlight/button pci info to use
  if((pci->hli.hl_gi.hli_ss & 0x03) == 0)
    return 0;
  // Selected buton > than max button ?
  // FIXME $$$ check how btn_ofn affects things like this
  if(*btn_nr > pci->hli.hl_gi.btn_ns)
    *btn_nr = 1;
  
  switch(ce->type) {
  case DVDCtrlUpperButtonSelect:
    *btn_nr = pci->hli.btnit[*btn_nr-1].up;
    break;
  case DVDCtrlLowerButtonSelect:
    *btn_nr = pci->hli.btnit[*btn_nr-1].down;
    break;
  case DVDCtrlLeftButtonSelect:
    *btn_nr = pci->hli.btnit[*btn_nr-1].left;
    break;
  case DVDCtrlRightButtonSelect:
    *btn_nr = pci->hli.btnit[*btn_nr-1].right;
    break;
  case DVDCtrlButtonActivate:
    is_action = 1;
    break;
  case DVDCtrlButtonSelectAndActivate:
    is_action = 1;
    *btn_nr = ce->button.nr;
    break;
  case DVDCtrlButtonSelect:
    *btn_nr = ce->button.nr;
    break;
  case DVDCtrlMouseSelect:
  case DVDCtrlMouseActivate:
    {
      int button;
      unsigned int x = ce->mouse.x;
      unsigned int y = ce->mouse.y;
      
      button = mouse_over_hl(pci, x, y);
      if(button) {
	*btn_nr = button;
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
  switch(pci->hli.btnit[*btn_nr - 1].auto_action_mode) {
  case 0:
    break;
  case 1:
    fprintf(stderr, "!!!auto_action_mode!!!\n");
    is_action = 1;
    break;
  case 2:
  case 3:
  default:
    fprintf(stderr, "Unknown auto_action_mode!! btn: %d\n", *btn_nr);
    exit(1);
  }
  
  
  /* If a new button has been selected or if one has been activated. */
  /* Determine the correct area and send the information to the spu decoder. */
  /* Possible optimization: don't send if its the same as last time. */
  {
    btni_t *button;
    button = &pci->hli.btnit[*btn_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][is_action]);
  }
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



vm_cmd_t eval_cell(char *vob_name, cell_playback_t *cell, 
		   int block, dvd_state_t *state) {
  char buffer[2048];
  int len, pending_packets, still_time, res = 0;
  pci_t pci;
  dsi_t dsi;
  vm_cmd_t cmd;
  /* To avoid having to do << 10 and >> 10 all the time we make a local copy */
  uint16_t sl_button_nr = state->HL_BTNN_REG >> 10;

  memset(&pci, 0, sizeof(pci_t));
  memset(&dsi, 0, sizeof(dsi_t));
  
  
  fprintf(stderr, "filename: %s\n", vob_name);
  //if(strcmp(vob_name, last_file_name) != 0)
    send_use_file(vob_name);
    
  block = 0;
  still_time = cell->still_time;
  
  /* Get the pci/dsi data */
  send_demux_sectors(cell->first_sector + block, 1, 0);
  pending_packets = 2;
  
  /* 
   * packets should allways be 0,1 or 2.
   */
  
  while(cell->first_sector + block <= cell->last_sector || still_time) {
    MsgEvent_t ev;
    int got_q;
    
    
    // For now..
    if(cell->first_sector + block <= cell->last_sector) { // || packets >0 ...
      
      assert(pending_packets > 0);
      got_q = wait_q(msgq, &ev); // Wait for a data packet or a message
    
    } else { 
      
      /* Handle cell pause and still time here */
      got_q = 0;
      if(cell->still_time != 0xff)
	while(still_time && MsgCheckEvent(msgq, &ev)) {
	  sleep(1); still_time--;
	}
      else // Inf. still time
	MsgNextEvent(msgq, &ev);
      
      if(!still_time) // No more still time
	if(MsgCheckEvent(msgq, &ev)) // and no more messages
	  break; 
    }
    
    
    
    
    
    
    /* Here we either have a message or an available data packet */ 
    
    if(!got_q) { // Then it must be a message (or error)
      
      switch(ev.type) {
      case MsgEventQDVDCtrl:
	
  /* Do async user input processing. Like angles change, audio change, 
   * subpicture change and answer attribute querry requests.
   */
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
	  
	  // A button has already been activated, discard this event
	  if(res == 0)
	    /* Processes user input, and send highlight info to spu */
	    res = process_button(&ev.dvdctrl.cmd, &pci, &sl_button_nr);
	  break;
	  
	case DVDCtrlMenuCall:
	  res = 2;
	  memset(&cmd.bytes, 0, 8);
	  cmd.bytes[0] = 0x30;

	  switch(ev.dvdctrl.cmd.menucall.menuid) {
	  case DVD_MENU_Title:
	    switch(state->domain) {
	    case VTS_DOMAIN:
	      cmd.bytes[1] = 0x08;
	      break;
	    default:
	      cmd.bytes[1] = 0x06;
	      break;
	    }
	    cmd.bytes[5] = 0x40 | ev.dvdctrl.cmd.menucall.menuid;
	    break;
	  default:
	    switch(state->domain) {
	    case VTS_DOMAIN:
	      cmd.bytes[1] = 0x08;
	      break;
	    case VTSM_DOMAIN:
	      cmd.bytes[1] = 0x06;
	      break;
	    default:
	      res = 0;
	      break;
	    }
	    cmd.bytes[5] = 0x80 | ev.dvdctrl.cmd.menucall.menuid;
	    break;
	  }
	  break;
 /* Handle other userinput */
 /* FIXME TODO XXX $$$ This should be a case ... and have code too */
	case DVDCtrlResume:
	  res = 2;
	  memset(&cmd.bytes, 0, 8);
	  cmd.bytes[0] = 0x20;
	  cmd.bytes[1] = 0x01;
	  cmd.bytes[7] = 0x10;
	  break;
	case DVDCtrlEventAudioStreamChange:
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
	default:
	  fprintf(stderr, "Unknown (not handled) DVDCtrlEvent %d\n",
		  ev.dvdctrl.cmd.type);
	  break;
	}
	break;
      default:
	handle_events(msgq, &ev);
      }
    
      if(0 /*angle && change_angle*/) {
	;
	continue;
      }
	
    } else { // We got a q entry so we read the data.
      
      assert(pending_packets > 0);
      
      len = get_q(msgq, &buffer[0]);
      
      if(buffer[0] == PS2_PCI_SUBSTREAM_ID) {
	assert(pending_packets == 2);
	pending_packets--;
	/* XXX inte läsa till pci utan något annat minne */
	read_pci_packet(&pci, &buffer[1], len);
	  
	if(pci.hli.hl_gi.hli_ss & 0x03) {
	  fprintf(stdout, "Menu detected\n");
	  print_pci_packet(stdout, &pci);
	}
	  
      } else if(buffer[0] == PS2_DSI_SUBSTREAM_ID) {
	assert(pending_packets == 1);
	pending_packets--;
	/* XXX inte läsa till dsi utan något annat minne */
	read_dsi_packet(&dsi, &buffer[1], len);
	print_dsi_packet(stderr, &dsi);
	  
      } else {
	fprintf(stderr, "vmg: Unknown NAV packet type");
      }
      
      // For now.. later use the time instead..
      if(pending_packets == 0 && res == 0) {
	int flush_video;
	
	/* Evaluate and Instantiate the new pci packets */ 
	process_pci(&pci, &sl_button_nr);
      
	if((dsi.vobu_sri.next & 0x80000000)== 0) {
	  flush_video = FlowCtrlCompleteVideoUnit;
	  fprintf(stderr, "flush_video = 1;\n");
	} else
	  flush_video = FlowCtrlNone;
	
	/* Demux/play the content of this vobu */
	send_demux_sectors(cell->first_sector + block + 1, 
			   dsi.dsi_gi.vobu_ea, flush_video);
	
	/* The next vobu is where... top two bits are flags */  
	block += dsi.vobu_sri.next & 0x3fffffff;
	  
	/* TODO XXX $$$ Test earlier and merge the requests if posible? */
	/* If there is more data in this cell to demux, then get the
	 * next nav pack. */
	if(cell->first_sector + block <= cell->last_sector) {
	  send_demux_sectors(cell->first_sector + block, 1, FlowCtrlNone);
	  pending_packets += 2;
	}
      }
    }
    
    // For now.. later use the time instead..
    if(pending_packets == 0) {
#if 0 
      /* TODO XXX $$$ This should only be done at the correct time */
      /* Handle forced activate button here */
      if(pci.hli.hl_gi.foac_btnn != 0 && 
	 (pci.hli.hl_gi.hli_ss & 0x03) != 0) {
	/* Forced action 0x3f is selected, 
	   otherwise use the specified button */
	if(pci.hli.hl_gi.foac_btnn != 0x3f)
	  if(pci.hli.hl_gi.foac_btnn <= pci.hli.hl_gi.btn_ns)
	    sl_button_nr = pci.hli.hl_gi.foac_btnn;
	res = 1;
      }
#endif
      /* FIXME TODO XXX $$$ */
      // If a button has been activated, exit (really run the command)
      // If we want to do this above instead then we must remember to 
      // discard 'packets' number of nav packets before starting to read 
      // again
      if(res != 0) {
	/* Do whatever */
	break;
      }
    }
  }
	
  /* FIXME TODO XXX $$$ */
  if(res == 1) {
    /* Save other state too (i.e maybe RSM info) */
    state->HL_BTNN_REG = sl_button_nr << 10;
    memcpy(&cmd, &pci.hli.btnit[sl_button_nr - 1].cmd, sizeof(vm_cmd_t));
    return cmd;
  }
  if(res == 2) { // Set resume info so that we can fail..
    /* Save other state too (i.e maybe RSM info) */
    state->HL_BTNN_REG = sl_button_nr << 10;
    state->rsm_blockN = block;
    /* cmd contains the command that the nav/ui want to run. */
    return cmd;
  }
  
  /* Save other state too (i.e maybe RSM info) */
  state->HL_BTNN_REG = sl_button_nr << 10;
  memset(&cmd, 0, sizeof(vm_cmd_t)); // mkNOP
  return cmd;
}


void do_run(void) {
  
  start_vm();
  
  /* For now we emulate to old system to be able to test */
  while(1) {
    vm_cmd_t cmd;
    char name[16];
    cell_playback_t *cell = &state.pgc->cell_playback_tbl[state.cellN - 1];
    
    //DEBUG
    {
      ifoPrint_time(5, &cell->playback_time); printf("\n");
      
      // Make file name
      if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
	snprintf(name, 14, "VIDEO_TS.VOB");
      } else {
	char part = '0'; 
	if(state.domain != VTSM_DOMAIN) // This is wrong
	  part += cell->first_sector/(1024  * 1024 * 1024 / 2048) + 1;
	snprintf(name, 14, "VTS_%02i_%c.VOB", state.vtsN, part);
      }
      printf("%s\t", name);
      printf("VOB ID: %3i, Cell ID: %3i at sector: 0x%08x - 0x%08x\n",
	     state.pgc->cell_position_tbl[state.cellN - 1].vob_id_nr,
	     state.pgc->cell_position_tbl[state.cellN - 1].cell_nr,
	     cell->first_sector, cell->last_sector);
    }
    
    cmd = eval_cell(name, cell, /* rsm_block */ 0, &state);
    
    eval_cmd(&cmd); // Returns True if it caused a jump
    get_next_cell();
  }
}  
