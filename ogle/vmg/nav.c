/* SKROMPF - A video player
 * Copyright (C) 2000 Bj�rn Englund, H�kan Hjort, Martin Norb�ck
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

#include "ifo.h" // vm_cmd_t
#include "nav.h"
#include "nav_read.h"
#include "nav_print.h"
#include "vm.h"


/* these lengths in bytes, exclusive of start code and length */
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define COMMAND_BYTES 8

extern MsgEventQ_t *msgq;

extern int get_q(MsgEventQ_t *msgq, char *buffer);
extern void handle_events(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_demux(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_spu(MsgEventQ_t *msgq, MsgEvent_t *ev);

int mouse_over_hl(pci_t *pci, unsigned int x, unsigned int y);

static int get_next_nav_packet(char *buffer) {
  return get_q(msgq, buffer);
}

static void send_demux_sectors(int start_sector, int nr_sectors) {
  MsgEvent_t ev;
  
  ev.type = MsgEventQPlayCtrl;
  ev.playctrl.cmd = PlayCtrlCmdPlayFromTo;
  ev.playctrl.from = start_sector * 2048;
  ev.playctrl.to = (start_sector + nr_sectors) * 2048;
  
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


static int process_pci(pci_t *pci, uint16_t *btn_nr) {
  int is_action = 0;
  
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
  
  /* Check for and read any user input */
  /* Must not consume events after a 'enter/click' event. (?) */
  while(!is_action) {
    MsgEvent_t ev;
    
    /* Exit when no more input. */
    if(MsgCheckEvent(msgq, &ev) == -1)
      break;
    
    switch(ev.type) {
    case MsgEventQUserInput:
      fprintf(stderr, "vmg: dvdctrl_cmd.cmd %d\n", ev.userinput.cmd);
      switch(ev.userinput.cmd) {
      case InputCmdButtonUp:
	*btn_nr = pci->hli.btnit[*btn_nr-1].up;
	break;
      case InputCmdButtonDown:
	*btn_nr = pci->hli.btnit[*btn_nr-1].down;
	break;
      case InputCmdButtonLeft:
	*btn_nr = pci->hli.btnit[*btn_nr-1].left;
	break;
      case InputCmdButtonRight:
	*btn_nr = pci->hli.btnit[*btn_nr-1].right;
	break;
      case InputCmdButtonActivate:
	is_action = 1;
	break;
      case InputCmdButtonActivateNr:
	is_action = 1;
	/* Fall through */
      case InputCmdButtonSelectNr:
	*btn_nr = ev.userinput.button_nr;
	break;
      case InputCmdMouseActivate:
      case InputCmdMouseMove:
	{
	  int button;
	  unsigned int x = ev.userinput.mouse_x;
	  unsigned int y = ev.userinput.mouse_y;
	  
	  button = mouse_over_hl(pci, x, y);
	  if(button) {
	    *btn_nr = button;
	    if(ev.userinput.cmd == InputCmdMouseActivate)
	      is_action = 1;
	  }
	}
	break;
      default:
	fprintf(stderr, "vmg: Unknown ev.userinput.cmd\n");
	break;
      }
      break;
    default:
      handle_events(msgq, &ev);
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
      fprintf(stderr, "Unknown auto_action_mode!!\n");
      exit(1);
    }
  }
  
  
  /* If there is highlight information, determine the correct area and 
     send the information to the spu decoder. */
  /* Possible optimization: don't send if its the same as last time. */
  if(pci->hli.hl_gi.hli_ss & 0x03) {
    btni_t *button;
    button = &pci->hli.btnit[*btn_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][is_action]);
    return is_action;
  }
  
  return 0;
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

vm_cmd_t eval_cell(char *vob_name, cell_playback_tbl_t *cell, 
		   dvd_state_t *state) {
  char buffer[2048];
  int len;
  pci_t pci;
  dsi_t dsi;
  vm_cmd_t cmd;
  int block = 0;
  int res = 0;
  /* To avoid having to do << 10 and >> 10 all the time we make a local copy */
  uint16_t sl_button_nr = state->registers.SPRM[8] >> 10;

  memset(&pci, 0, sizeof(pci_t));
  memset(&dsi, 0, sizeof(dsi_t));
  
  //if(strcmp(vob_name, last_file_name) != 0)
    send_use_file(vob_name);
 
  block = 0;
  while(cell->first_sector + block <= cell->last_sector) {
    /* Get the pci/dsi data, always fist in a vobu. */
    send_demux_sectors(cell->first_sector + block, 1); 
    
    len = get_next_nav_packet(&buffer[0]);
    assert(buffer[0] == PS2_PCI_SUBSTREAM_ID);
    read_pci_packet(&pci, &buffer[1], len);
    
    len = get_next_nav_packet(&buffer[0]);
    assert(buffer[0] == PS2_DSI_SUBSTREAM_ID);
    read_dsi_packet(&dsi, &buffer[1], len);
    
    if(pci.hli.hl_gi.hli_ss & 0x03) {
      fprintf(stdout, "Menu detected\n");
      print_pci_packet(stdout, &pci);
    }
    
    /* Check for user input, and set highlight */
    res = process_pci(&pci, &sl_button_nr);

    /* Demux/play the content of this vobu */
    send_demux_sectors(cell->first_sector + block + 1, dsi.dsi_gi.vobu_ea);
    
    /* Decide where the next vobu is.. top two bits are flags */  
    if(0 /*angle && change_angle*/) {
      ;
    } else {
      block += dsi.vobu_sri.next_vobu & 0x3fffffff;
    }
  
    if(res != 0) { /* Exit if we detected a button activation */
      break;
    }
  }
  
  /* Handle forced activate button here */
  if(res == 0 && (pci.hli.hl_gi.hli_ss & 0x03) != 0) {
    if(pci.hli.hl_gi.foac_btnn != 0) {
      /* Forced action 0x3f is selected, otherwise use the specified button */
      if(pci.hli.hl_gi.foac_btnn != 0x3f && pci.hli.hl_gi.foac_btnn <= 36)
	sl_button_nr = pci.hli.hl_gi.foac_btnn;
      res = 1;
    }
  }
  
  
  /* A co XXX */
  if(res != 0) {
    /* Save other state too (i.e maybe RSM info) */
    state->registers.SPRM[8] = sl_button_nr << 10;
    memcpy(&cmd, &pci.hli.btnit[sl_button_nr - 1].cmd, sizeof(vm_cmd_t));
    return cmd;
  }
  
  /* Handle cell pause and still time here */
  if(cell->still_time) {
    int time = cell->still_time;
    if(time == 0xff) { /* Inf. still time */
      printf("-- Wait for user interaction --\n");
      while(res == 0) {
	//should be nanosleep
	sleep(1);
	/* Check for user input, and set highlight */
	res = process_pci(&pci, &sl_button_nr);
      }
    } else {
      while(res == 0 && time > 0) {
	//should be nanosleep
	sleep(1); --time;
	/* Check for user input, and set highlight */
	res = process_pci(&pci, &sl_button_nr);
      }
    }
  }
  
  /* A co XXX */
  if(res != 0) {
    /* Save other state too (i.e maybe RSM info) */
    state->registers.SPRM[8] = sl_button_nr << 10;
    memcpy(&cmd, &pci.hli.btnit[sl_button_nr - 1].cmd, sizeof(vm_cmd_t));
    return cmd;
  }
  
  /* Save other state too (i.e maybe RSM info) */
  state->registers.SPRM[8] = sl_button_nr << 10;
  memset(&cmd, 0, sizeof(vm_cmd_t)); // mkNOP
  return cmd;
}

