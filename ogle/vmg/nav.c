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
#include <assert.h>

#include "msgtypes.h"

#include "ifo.h" // vm_cmd_t
#include "nav.h"
#include "nav_read.h"
#include "nav_print.h"
#include "vm.h"


/* these lengths in bytes, exclusive of start code and length */
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define COMMAND_BYTES 8


int get_q(char *buffer);
mq_cmd_t *chk_for_msg(mq_msg_t *msg);
int send_msg(mq_msg_t *msg, int mtext_size);
int eval_msg(mq_cmd_t *cmd);
int wait_init_msg(void);
int mouse_over_hl(pci_t *pci, unsigned int x, unsigned int y);

static int get_next_nav_packet(char *buffer) {
  static int init_done = 0;
  if(!init_done) {
    wait_init_msg();
    init_done = 1;
  }
  return get_q(buffer);
}

static void send_demux_sectors(int start_sector, int nr_sectors) {
  mq_msg_t msg;
  mq_cmd_t *sendcmd;
  
  sendcmd = (mq_cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_CTL;
  sendcmd->cmdtype = CMD_CTRL_CMD;
  sendcmd->cmd.ctrl_cmd.ctrlcmd = CTRLCMD_PLAY_FROM_TO;
  sendcmd->cmd.ctrl_cmd.off_from = start_sector * 2048;
  sendcmd->cmd.ctrl_cmd.off_to = (start_sector + nr_sectors) * 2048;
  send_msg(&msg, sizeof(mq_cmdtype_t) + sizeof(mq_cmd_ctrl_cmd_t));
}

void send_use_file(char *file_name) {
  mq_msg_t msg;
  mq_cmd_t *sendcmd;
  
  sendcmd = (mq_cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_DEMUX;
  sendcmd->cmdtype = CMD_FILE_OPEN;
  strcpy(&sendcmd->cmd.file_open.file[0], file_name);
  send_msg(&msg, sizeof(mq_cmdtype_t) + sizeof(mq_cmd_file_open_t));
}

void set_spu_palette(uint32_t palette[16]) {
  mq_msg_t msg;
  mq_cmd_t *sendcmd;
  int n;
  
  sendcmd = (mq_cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_SPU_DECODE;
  sendcmd->cmdtype = CMD_SPU_SET_PALETTE;
  for(n = 0; n < 16; n++) {
    sendcmd->cmd.spu_palette.colors[n] = palette[n];
  }
  send_msg(&msg, sizeof(mq_cmdtype_t) + sizeof(mq_cmd_spu_palette_t));
}

void send_highlight(int x_start, int y_start, int x_end, int y_end, 
		    uint32_t btn_coli) {
  mq_msg_t msg;
  mq_cmd_t *sendcmd;
  int n;
  
  sendcmd = (mq_cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_SPU_DECODE;
  sendcmd->cmdtype = CMD_SPU_SET_HIGHLIGHT;
  sendcmd->cmd.spu_highlight.x_start = x_start;
  sendcmd->cmd.spu_highlight.y_start = y_start;
  sendcmd->cmd.spu_highlight.x_end = x_end;
  sendcmd->cmd.spu_highlight.y_end = y_end;
  for(n = 0; n < 4; n++)
    sendcmd->cmd.spu_highlight.color[n] = 0xf & btn_coli>>(16 + 4*n);
  for(n = 0; n < 4; n++)
    sendcmd->cmd.spu_highlight.contrast[n] = 0xf & btn_coli>>(4*n);
  send_msg(&msg, sizeof(mq_cmdtype_t) + sizeof(mq_cmd_spu_highlight_t));
}


static int process_pci(pci_t *pci, uint16_t *btn_nr) {
  mq_msg_t msg;
  int is_action = 0;
  
  /* Check if this is alright, i.e. pci->hli.hl_gi.hli_ss == 1 only 
     for the first menu pic packet? What about looping menus */
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
    mq_cmd_t *cmd = chk_for_msg(&msg);
    /* Exit when no more input. */

    if(cmd == NULL)
      break;
    
    switch(cmd->cmdtype) {
    case CMD_DVDCTRL_CMD:
      fprintf(stderr, "vmg: dvdctrl_cmd.cmd %d\n", cmd->cmd.dvdctrl_cmd.cmd);
      switch(cmd->cmd.dvdctrl_cmd.cmd) {
      case DVDCTRL_CMD_UP_BUTTON:
	*btn_nr = pci->hli.btnit[*btn_nr-1].up;
	break;
      case DVDCTRL_CMD_DOWN_BUTTON:
	*btn_nr = pci->hli.btnit[*btn_nr-1].down;
	break;
      case DVDCTRL_CMD_LEFT_BUTTON:
	*btn_nr = pci->hli.btnit[*btn_nr-1].left;
	break;
      case DVDCTRL_CMD_RIGHT_BUTTON:
	*btn_nr = pci->hli.btnit[*btn_nr-1].right;
	break;
      case DVDCTRL_CMD_ACTIVATE_BUTTON:
	is_action = 1;
	break;
      case DVDCTRL_CMD_SELECT_BUTTON_NR:
	*btn_nr = cmd->cmd.dvdctrl_cmd.button_nr;
	break;
      case DVDCTRL_CMD_SELECT_ACTIVATE_BUTTON_NR:
	*btn_nr = cmd->cmd.dvdctrl_cmd.button_nr;
	is_action = 1;
	break;
      case DVDCTRL_CMD_CHECK_MOUSE_SELECT:
	{
	  int button;
	  unsigned int x = cmd->cmd.dvdctrl_cmd.mouse_x;
	  unsigned int y = cmd->cmd.dvdctrl_cmd.mouse_y;
	  
	  button = mouse_over_hl(pci, x, y);
	  if(button) {
	    *btn_nr = button;
	  }
	}
	break;
      case DVDCTRL_CMD_CHECK_MOUSE_ACTIVATE:
	{
	  int button;
	  unsigned int x = cmd->cmd.dvdctrl_cmd.mouse_x;
	  unsigned int y = cmd->cmd.dvdctrl_cmd.mouse_y;
	  
	  button = mouse_over_hl(pci, x, y);
	  if(button) {
	    *btn_nr = button;
	    is_action = 1;
	  }
	}
	break;
      default:
	fprintf(stderr, "vmg: Unknown dvdctrl_cmd.cmd\n");
	break;
      }
      break;
    default:
      eval_msg(cmd);
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
    block++;
    
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
    /* Assume that the vobus are packed and continue after each other,
       this isn't correct. Certanly not for multiangle at least. */
    /* Needs to know the selected angle. */
    send_demux_sectors(cell->first_sector + block, dsi.dsi_gi.vobu_ea);
    block += dsi.dsi_gi.vobu_ea;
    
    if(res != 0) { /* Exit if we detected a button activation */
      break;
    }
  }
  
  /* Handle forced activate button here */
  if(res != 0 && (pci.hli.hl_gi.hli_ss & 0x03) != 0) {
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
  
  /* Handle cell pause and still here */
  if(cell->category & 0xff00) {
    int time = (cell->category>>8) & 0xff;
    if(time == 0xff) {
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
  
  /* Save other state too (i.e maybe RSM info) */
  state->registers.SPRM[8] = sl_button_nr << 10;
  memset(&cmd, 0, sizeof(vm_cmd_t)); // mkNOP
  return cmd;
}

