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


/* these lengths in bytes, exclusive of start code and length */
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define COMMAND_BYTES 8


int get_q(char *buffer);
cmd_t *chk_for_msg(msg_t *msg);
int send_msg(msg_t *msg, int mtext_size);
int eval_msg(cmd_t *cmd);
int wait_init_msg(void);

static void get_next_nav_packet(char *buffer) {
  static int init_done = 0;
  if(!init_done) {
    wait_init_msg();
    init_done = 1;
  }
  get_q(buffer);
}

static void send_demux_sectors(int start_sector, int nr_sectors) {
  msg_t msg;
  cmd_t *sendcmd;
  
  sendcmd = (cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_CTL;
  sendcmd->cmdtype = CMD_CTRL_CMD;
  sendcmd->cmd.ctrl_cmd.ctrlcmd = CTRLCMD_PLAY_FROM_TO;
  sendcmd->cmd.ctrl_cmd.off_from = start_sector * 2048;
  sendcmd->cmd.ctrl_cmd.off_to = (start_sector + nr_sectors) * 2048;
  send_msg(&msg, sizeof(cmdtype_t) + sizeof(cmd_ctrl_cmd_t));
}

void send_use_file(char *file_name) {
  msg_t msg;
  cmd_t *sendcmd;
  
  sendcmd = (cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_DEMUX;
  sendcmd->cmdtype = CMD_FILE_OPEN;
  strcpy(&sendcmd->cmd.file_open.file[0], file_name);
  send_msg(&msg, sizeof(cmdtype_t) + sizeof(cmd_file_open_t));
}

void set_spu_palette(uint32_t palette[16]) {
  msg_t msg;
  cmd_t *sendcmd;
  int n;
  
  sendcmd = (cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_SPU_DECODE;
  sendcmd->cmdtype = CMD_SPU_SET_PALETTE;
  for(n = 0; n < 16; n++) {
    sendcmd->cmd.spu_palette.colors[n] = palette[n];
  }
  send_msg(&msg, sizeof(cmdtype_t) + sizeof(cmd_spu_palette_t));
}

void send_highlight(int x_start, int y_start, int x_end, int y_end, 
		    uint32_t btn_coli) {
  msg_t msg;
  cmd_t *sendcmd;
  int n;
  
  sendcmd = (cmd_t *)&msg.mtext;
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
  send_msg(&msg, sizeof(cmdtype_t) + sizeof(cmd_spu_highlight_t));
}


static int process_pci(pci_t *pci, uint16_t *btn_nr) {
  msg_t msg;
  int is_action = 0;
  
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0 
     && *btn_nr > pci->hli.hl_gi.btn_ns) {
    *btn_nr = 1; 
  }
  
  /* Check for and read any user input */
  /* Must not consume events after a 'enter/click' event. (?) */
  while(!is_action) {
    cmd_t *cmd = chk_for_msg(&msg);
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


int eval_cell(char *vob_name, cell_playback_tbl_t *cell, vm_cmd_t *cmd) {
  buffer_t buffer;
  pci_t pci;
  dsi_t dsi;
  int block = 0;
  int res = 0;
  static uint16_t state_systemreg_hili_button_nr = 1;

  memset(&pci, 0, sizeof(pci_t));
  memset(&dsi, 0, sizeof(dsi_t));
  
  //if(strcmp(vob_name, last_file_name) != 0)
    send_use_file(vob_name);
 
  block = 0;
  while(cell->first_sector + block <= cell->last_sector) {
    /* Get the pci/dsi data, always fist in a vobu. */
    send_demux_sectors(cell->first_sector + block, 1); 
    block++;
    
    get_next_nav_packet(&buffer.bytes[0]);
    assert(buffer.bytes[0] == PS2_PCI_SUBSTREAM_ID);
    buffer.bit_position = 8; // First byte is SUBSTREAM_ID
    read_pci_packet(&pci, &buffer);
    
    get_next_nav_packet(&buffer.bytes[0]);
    assert(buffer.bytes[0] == PS2_DSI_SUBSTREAM_ID);
    buffer.bit_position = 8; // First byte is SUBSTREAM_ID
    read_dsi_packet(&dsi, &buffer);
    
    if(pci->hli.hl_gi.hli_ss & 0x03) {
      fprintf(stdout, "Menu detected\n");
      print_pci_packet(stdout, pci);
    }
    
    /* Check for user input, and set highlight */
    res = process_pci(&pci, &state_systemreg_hili_button_nr);

    /* Demux/play the content of this vobu */
    /* Assume that the vobus are packed and continue after each other,
       this isn't correct. Certanly not for multiangle at least. */
    /* Needs to know the selected angle. */
    send_demux_sectors(cell->first_sector + block, dsi.dsi_gi.vobu_ea);
    block += dsi.dsi_gi.vobu_ea;
    
    if(res != 0) {
      memcpy(cmd, &pci.hli.btnit[state_systemreg_hili_button_nr - 1].cmd, 8);
      break;
    }
  }
  if(res != 0)
    return state_systemreg_hili_button_nr;
  
  /* Handle cell pause and still here */
  if(cell->category & 0xff00) {
    int time = (cell->category>>8) & 0xff;
    if(time == 0xff) {
      printf("-- Wait for user interaction --\n");
      while(res == 0) {
	//should be nanosleep
	sleep(1);
	/* Check for user input, and set highlight */
	res = process_pci(&pci, &state_systemreg_hili_button_nr);
      }
    } else {
      while(res == 0 && time > 0) {
	//should be nanosleep
	sleep(1); --time;
	/* Check for user input, and set highlight */
	res = process_pci(&pci, &state_systemreg_hili_button_nr);
      }
    }
  }
  
  memset(cmd, 0, sizeof(vm_cmd_t));
  return 0;
}

