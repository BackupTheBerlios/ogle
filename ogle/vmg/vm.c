/* SKROMPF - A video player
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#include <ogle/msgevents.h>

#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>

#include "decoder.h"
#include "vmcmd.h"
#include "vm.h"

extern void set_spu_palette(uint32_t palette[16]); // nav.c



dvd_reader_t *dvd;
ifo_handle_t *vmgi;
ifo_handle_t *vtsi;

dvd_state_t state;




int vm_start(void);
int vm_reset(char *dvdroot);
int vm_run(void);
int vm_eval_cmd(vm_cmd_t *cmd);
int vm_get_next_cell(void);

static void saveRSMinfo(int cellN, int blockN);
static link_t play_PGC(void);
static link_t play_PG(void);
static link_t play_Cell(void);
static link_t play_Cell_post(void);
static link_t play_PGC_post(void);

static link_t process_command(link_t link_values);

static void ifoOpenNewVTSI(dvd_reader_t *dvd, int vtsN);

static pgcit_t* get_VTS_PGCIT();
static pgcit_t* get_VTSM_PGCIT(uint16_t lang);
static pgcit_t* get_VMGM_PGCIT(uint16_t lang);
static pgcit_t* get_PGCIT(void);
static int get_video_aspect(void);

/* Can only be called when in VTS_DOMAIN */
static int get_TT(int tt);
static int get_VTS_TT(int vtsN, int vts_ttn);
static int get_VTS_PTT(int vtsN, int vts_ttn, int part);
static int get_ID(int id);

static int get_MENU(int menu); // VTSM & VMGM
static int get_FP_PGC(void); // FP

/* Called in any domain */
static int get_PGC(int pgcN);
static int get_PGCN(void);








int vm_reset(char *dvdroot)
{ 
  // Setup State
  memset(state.registers.SPRM, 0, sizeof(uint16_t)*24);
  memset(state.registers.GPRM, 0, sizeof(state.registers.GPRM));
  state.registers.SPRM[0] = ('e'<<8)|'n'; // Player Menu Languange code
  state.AST_REG = 0; // 15 why?
  state.SPST_REG = 0; // 62 why?
  state.AGL_REG = 1;
  state.TTN_REG = 1;
  state.VTS_TTN_REG = 1;
  //state.TT_PGCN_REG = 0
  state.PTTN_REG = 1;
  state.HL_BTNN_REG = 1 << 10;

  state.PTL_REG = 15; // Parental Level
  state.registers.SPRM[12] = ('U'<<8)|'S'; // Parental Management Country Code
  state.registers.SPRM[16] = ('e'<<8)|'n'; // Initial Language Code for Audio
  state.registers.SPRM[18] = ('e'<<8)|'n'; // Initial Language Code for Spu
  state.registers.SPRM[20] = 1; // Player Regional Code
  
  state.pgN = 0;
  state.cellN = 0;

  state.domain = FP_DOMAIN;
  state.rsm_vtsN = 0;
  state.rsm_cellN = 0;
  state.rsm_blockN = 0;
  
  state.vtsN = -1;
  
  dvd = DVDOpen(dvdroot);

  vmgi = ifoOpenVMGI(dvd); // Check for error?
  
  return 0;
}




// FIXME TODO XXX $$$ Handle error condition too...
int vm_start(void)
{
  link_t link_values;

  // Set pgc to FP pgc
  get_FP_PGC();
  link_values = play_PGC(); 
  
  if(link_values.command != PlayThis) {
    /* At the end of this PGC or we encountered a command. */
    // Terminates first when it gets a PlayThis (or error).
    link_values = process_command(link_values);
    assert(link_values.command == PlayThis);
  }
  state.blockN = link_values.data1;

  return 0; //??
}


int vm_eval_cmd(vm_cmd_t *cmd)
{
  link_t link_values;
  
  vmPrint_CMD(0, cmd);
  
  if(eval(cmd, 1, &state.registers, &link_values)) {
    if(link_values.command != PlayThis) {
      /* At the end of this PGC or we encountered a command. */
      // Terminates first when it gets a PlayThis (or error).
      link_values = process_command(link_values);
      assert(link_values.command == PlayThis);
    }
    state.blockN = link_values.data1;
    return 1; // Something changed, Jump
  } else {
    return 0; // It updated some state thats all...
  }
}

// FIXME TODO XXX $$$ Handle error condition too...
// How to tell if there is any need to do a Flush?
int vm_get_next_cell(void)
{
  link_t link_values;
  // Calls play_Cell wich either returns PlayThis or a command
  link_values = play_Cell_post();
  
  if(link_values.command != PlayThis) {
    /* At the end of this PGC or we encountered a command. */
    // Terminates first when it gets a PlayThis (or error).
    link_values = process_command(link_values);
    assert(link_values.command == PlayThis);
  }
  state.blockN = link_values.data1;
  
  return 0; // ??
}

int vm_top_pg(void)
{
  link_t link_values;
  // Calls play_Cell wich either returns PlayThis or a command
  link_values = play_PG();
  
  if(link_values.command != PlayThis) {
    /* At the end of this PGC or we encountered a command. */
    // Terminates first when it gets a PlayThis (or error).
    link_values = process_command(link_values);
    assert(link_values.command == PlayThis);
  }
  state.blockN = link_values.data1;
  
  return 1; // Jump
}

int vm_next_pg(void)
{
  // Udate PG first?
  state.pgN += 1; 
  return vm_top_pg();
}

int vm_prev_pg(void)
{
  // Udate PG first?
  state.pgN -= 1;
  if(state.pgN == 0) {
    state.pgN = 1;
    return 0;
  }
  return vm_top_pg();
}


static domain_t menuid2domain(DVDMenuID_t menuid)
{
  domain_t result = VTSM_DOMAIN; // Really shouldn't have to..
  
  switch(menuid) {
  case DVD_MENU_Title:
    result = VMGM_DOMAIN;
    break;
  case DVD_MENU_Root:
  case DVD_MENU_Subpicture:
  case DVD_MENU_Audio:
  case DVD_MENU_Angle:
  case DVD_MENU_Part:
    result = VTSM_DOMAIN;
    break;
  }
  
  return result;
}


int vm_menu_call(DVDMenuID_t menuid, int block)
{
  domain_t old_domain;
  link_t link_values;
  
  /* Should check if we are allowed/can acces this menu */
  
  
  /* FIXME XXX $$$ How much state needs to be restored 
   * when we fail to find a menu? */
  
  old_domain = state.domain;
  
  switch(state.domain) {
  case VTS_DOMAIN:
    saveRSMinfo(0, block);
    /* FALL THROUGH */
  case VTSM_DOMAIN:
  case VMGM_DOMAIN:
    state.domain = menuid2domain(menuid);
    if(get_MENU(menuid) != -1) {
      link_values = play_PGC();
      
      if(link_values.command != PlayThis) {
	/* At the end of this PGC or we encountered a command. */
	// Terminates first when it gets a PlayThis (or error).
	link_values = process_command(link_values);
	assert(link_values.command == PlayThis);
      }
      state.blockN = link_values.data1;
      return 1; // Jump
    } else {
      state.domain = old_domain;
    }
    break;
  case FP_DOMAIN: /* FIXME XXX $$$ What should we do here? */
    break;
  }
  
  return 0;
}


int vm_resume(void)
{
  int i;
  link_t link_values;
  
  // Check and see if there is any rsm info!!
  if(state.rsm_vtsN == 0) {
    return 0;
  }
  
  state.domain = VTS_DOMAIN;
  ifoOpenNewVTSI(dvd, state.rsm_vtsN);
  get_PGC(state.rsm_pgcN);
  
  set_spu_palette(state.pgc->palette); // Erhum...
  
  /* These should never be set in SystemSpace and/or MenuSpace */ 
  // state.TTN_REG = state.rsm_tt;
  // state.TT_PGCN_REG = state.rsm_pgcN;
  // state.HL_BTNN_REG = state.rsm_btnn;
  for(i = 0; i < 5; i++) {
    state.registers.SPRM[4 + i] = state.rsm_regs[i];
  }

  if(state.rsm_cellN == 0) {
    assert(state.cellN); // Checking if this ever happens
    state.pgN = 1;
    link_values = play_PG();
    if(link_values.command != PlayThis) {
      /* At the end of this PGC or we encountered a command. */
      // Terminates first when it gets a PlayThis (or error).
      link_values = process_command(link_values);
      assert(link_values.command == PlayThis);
    }
    state.blockN = link_values.data1;
  } else { 
    //state.pgN = ?? this gets the righ value in play_Cell
    // FIXME $$$ XXX No it doesn't !!!
    state.cellN = state.rsm_cellN;
    
    state.blockN = state.rsm_blockN;
  }
  
  return 1; // Jump
}

int vm_get_audio_stream(int audioN)
{
  int streamN = -1;
  
  if(state.domain == VTSM_DOMAIN 
     || state.domain == VMGM_DOMAIN
     || state.domain == FP_DOMAIN) {
    audioN = 0;
  }
  
  if(audioN < 7) {
    /* Is there any contol info for this logical stream */ 
    if(state.pgc->audio_control[audioN] & (1<<15)) {
      streamN = (state.pgc->audio_control[audioN] >> 8) & 0x07;  
    }
  }
  
  if(state.domain == VTSM_DOMAIN 
     || state.domain == VMGM_DOMAIN
     || state.domain == FP_DOMAIN) {
    if(streamN == -1)
      streamN = 0;
  }
  
   /* Should also check in vtsi/vmgi status that what kind of stream
   * it is (ac3/lpcm/dts/sdds...) */
  return streamN;
}

int vm_get_subp_stream(int subpN)
{
  int streamN = -1;
  int source_aspect = get_video_aspect();
  
  if(state.domain == VTSM_DOMAIN 
     || state.domain == VMGM_DOMAIN
     || state.domain == FP_DOMAIN) {
    subpN = 0;
  }
  
  if(subpN < 32) { /* a valid logical stream */
    /* Is this logical stream present */ 
    if(state.pgc->subp_control[subpN] & (1<<31)) {
      if(source_aspect == 0) /* 4:3 */	     
	streamN = (state.pgc->subp_control[subpN] >> 24) & 0x1f;  
      if(source_aspect == 3) /* 16:9 */
	streamN = (state.pgc->subp_control[subpN] >> 16) & 0x1f;
    }
  }
  
  /* Paranoia.. if no stream select 0 anyway */
  if(state.domain == VTSM_DOMAIN 
     || state.domain == VMGM_DOMAIN
     || state.domain == FP_DOMAIN) {
    if(streamN == -1)
      streamN = 0;
  }
  
  return streamN;
}

int vm_get_subp_active_stream(void)
{
  int subpN = state.SPST_REG;
  
  if(state.domain == VTS_DOMAIN && !(subpN & 0x40)) { /* Is the subp off? */
    return -1;
  } else {
    return vm_get_subp_stream(subpN & ~0x40);
  }
}

void vm_get_audio_info(int *num_avail, int *current)
{
  if(state.domain == VTS_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vts_audio_streams;
    *current = state.AST_REG;
  } else if(state.domain == VTSM_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vtsm_audio_streams;
    *current = 1;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    *num_avail = vmgi->vmgi_mat->nr_of_vmgm_audio_streams;
    *current = 1;
  }
}

void vm_get_subp_info(int *num_avail, int *current)
{
  if(state.domain == VTS_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vts_subp_streams;
    *current = state.SPST_REG;
  } else if(state.domain == VTSM_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vtsm_subp_streams;
    *current = 0x41;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    *num_avail = vmgi->vmgi_mat->nr_of_vmgm_subp_streams;
    *current = 0x41;
  }
}

subp_attr_t vm_get_subp_attr(int streamN)
{
  subp_attr_t attr;
  
  if(state.domain == VTS_DOMAIN) {
    attr = vtsi->vtsi_mat->vts_subp_attributes[streamN];
  } else if(state.domain == VTSM_DOMAIN) {
    attr = vtsi->vtsi_mat->vtsm_subp_attributes[0];
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    attr = vmgi->vmgi_mat->vmgm_subp_attributes[0];
  }
  return attr;
}

audio_attr_t vm_get_audio_attr(int streamN)
{
  audio_attr_t attr;
  
  if(state.domain == VTS_DOMAIN) {
    attr = vtsi->vtsi_mat->vts_audio_attributes[streamN];
  } else if(state.domain == VTSM_DOMAIN) {
    attr = vtsi->vtsi_mat->vtsm_audio_attributes[0];
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    attr = vmgi->vmgi_mat->vmgm_audio_attributes[0];
  }
  return attr;
}

uint16_t vm_get_subp_lang(int streamN)
{
  subp_attr_t attr = vm_get_subp_attr(streamN);
  return attr.lang_code;
}

uint16_t vm_get_audio_lang(int streamN)
{
  audio_attr_t attr = vm_get_audio_attr(streamN);
  return attr.lang_code;
}

// Must be called before domain is changed (get_PGCN())
static void saveRSMinfo(int cellN, int blockN)
{
  int i;
  
  if(cellN != 0) {
    state.rsm_cellN = cellN;
    state.rsm_blockN = 0;
  } else {
    state.rsm_cellN = state.cellN;
    state.rsm_blockN = blockN;
  }
  state.rsm_vtsN = state.vtsN;
  state.rsm_pgcN = get_PGCN();
  
  //assert(state.rsm_pgcN == state.TT_PGCN_REG); // for VTS_DOMAIN
  
  for(i = 0; i < 5; i++) {
    state.rsm_regs[i] = state.registers.SPRM[4 + i];
  }
}



/* Figure out the correct pgN from the cell and update state. */ 
static int set_PGN(void) {
  int new_pgN = 0;
  
  while(new_pgN < state.pgc->nr_of_programs 
	&& state.cellN >= state.pgc->pgc_program_map[new_pgN])
    new_pgN++;
  
  if(new_pgN == state.pgc->nr_of_programs) /* We are at the last program */
    if(state.cellN > state.pgc->nr_of_cells)
      return 1; /* We are past the last cell */
  
  state.pgN = new_pgN;
  
  if(state.domain == VTS_DOMAIN) {
    playback_type_t *pb_ty;
    if(vmgi->tt_srpt == NULL)
      ifoRead_TT_SRPT(vmgi);
    pb_ty = &vmgi->tt_srpt->title_info[state.TTN_REG].pb_ty;
    if(pb_ty->multi_or_random_pgc_title == /* One_Sequential_PGC_Title */ 0) {
      if(vtsi->vts_ptt_srpt == NULL)
	ifoRead_VTS_PTT_SRPT(vtsi);
      assert(state.VTS_TTN_REG <= vtsi->vts_ptt_srpt->nr_of_srpts);
      assert(get_PGCN() == vtsi->vts_ptt_srpt->title_info[state.VTS_TTN_REG - 1].ptt_info[0].pgcn);
      assert(1 == vtsi->vts_ptt_srpt->title_info[state.VTS_TTN_REG - 1].ptt_info[0].pgn);
      state.PTTN_REG = state.pgN;
    }
  }
  
  return 0;
}





static link_t play_PGC(void) 
{    
  link_t link_values;
  
  printf("play_PGC:\n"); // state.pgcN (%i)\n", state.pgcN);

  // This must be set before the pre-commands are executed because they
  // might contain a CallSS that will save resume state
  state.pgN = 1;
  state.cellN = 0;

  { //DEBUG
    int i;
    for(i = 0; state.pgc->pgc_command_tbl &&
	  i < state.pgc->pgc_command_tbl->nr_of_pre;i++)
      vmPrint_CMD(i, &state.pgc->pgc_command_tbl->pre_commands[i]);
  }

  
  // FIXME XXX $$$ Only send when needed, and do send even if not playing
  // from start? (should we do pre_commands when jumping to say part 3?)
  /* Send the palette to the spu. */
  set_spu_palette(state.pgc->palette);
  

  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG1 (also a kind of jump/link)
       (This is what happens if you fall of the end of the pre_commands)
     - or a error (are there more cases?) */
  if(state.pgc->pgc_command_tbl 
     && eval(state.pgc->pgc_command_tbl->pre_commands, 
	     state.pgc->pgc_command_tbl->nr_of_pre, 
	     &state.registers, &link_values)) {
    // link_values contains the 'jump' return value
    return link_values;
  }
  
  return play_PG();
}  


static link_t play_PG(void)
{
  printf("play_PG: state.pgN (%i)\n", state.pgN);
  
  assert(state.pgN > 0);
  if(state.pgN > state.pgc->nr_of_programs) {
    assert(state.pgN == state.pgc->nr_of_programs + 1);
    return play_PGC_post();
  }
  
  state.cellN = state.pgc->pgc_program_map[state.pgN - 1];
  
  return play_Cell();
}


static link_t play_Cell(void)
{
  printf("play_Cell: state.cellN (%i)\n", state.cellN);
  
  assert(state.cellN > 0);
  if(state.cellN > state.pgc->nr_of_cells) {
    printf("state.cellN (%i) == pgc->nr_of_cells + 1 (%i)\n", 
	   state.cellN, state.pgc->nr_of_cells + 1);
    assert(state.cellN == state.pgc->nr_of_cells + 1); 
    return play_PGC_post();
  }
  

  /* Multi angle/Interleaved */
  switch(state.pgc->cell_playback_tbl[state.cellN - 1].block_mode) {
  case 0: // Normal
    assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_type == 0);
    break;
  case 1: // The first cell in the block
    switch(state.pgc->cell_playback_tbl[state.cellN - 1].block_type) {
    case 0: // Not part of a block
      assert(0);
    case 1: // Angle block
      /* Loop and check each cell instead? So we don't get outsid the block. */
      state.cellN += state.AGL_REG - 1;
      assert(state.domain == VTSM_DOMAIN); // ??
      assert(state.cellN <= state.pgc->nr_of_cells);
      assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_mode != 0);
      assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_type == 1);
      break;
    case 2: // ??
    case 3: // ??
    default:
      fprintf(stderr, "Invalid? Cell block_mode (%d), block_type (%d)\n",
	      state.pgc->cell_playback_tbl[state.cellN - 1].block_mode,
	      state.pgc->cell_playback_tbl[state.cellN - 1].block_type);
    }
    break;
  case 2: // Cell in the block
  case 3: // Last cell in the block
  // These might perhaps happen for RSM or LinkC commands?
  default:
    fprintf(stderr, "Cell is in block but did not enter at first cell!\n");
  }
  
  /* Updates state.pgN and PTTN_REG */
  if(set_PGN()) {
    /* Should not happen */
    link_t tmp = {LinkTailPGC, /* Block in Cell */ 0, 0, 0};
    assert(0);
    return tmp;
  }
  
  {
    link_t tmp = {PlayThis, /* Block in Cell */ 0, 0, 0};
    return tmp;
  }

}

static link_t play_Cell_post(void)
{
  cell_playback_t *cell;
  
  printf("play_Cell_post: state.cellN (%i)\n", state.cellN);
  
  cell = &state.pgc->cell_playback_tbl[state.cellN - 1];
  
  /* Still time is already taken care of before we get called. */
  
  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;
    
    assert(state.pgc->pgc_command_tbl != NULL);
    assert(state.pgc->pgc_command_tbl->nr_of_cell >= cell->cell_cmd_nr);
    vmPrint_CMD(0, &state.pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1]);
    if(eval(&state.pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1],
	    1, &state.registers, &link_values)) {
      return link_values;
    } else {
      // Error ?? goto tail? goto next PG? or what? just continue?
    }
  }
  
  
  /* Where to continue after playing the cell... */
  /* Multi angle/Interleaved */
  switch(state.pgc->cell_playback_tbl[state.cellN - 1].block_mode) {
  case 0: // Normal
    assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_type == 0);
    state.cellN++;
    break;
  case 1: // The first cell in the block
  case 2: // A cell in the block
  case 3: // The last cell in the block
  default:
    switch(state.pgc->cell_playback_tbl[state.cellN - 1].block_type) {
    case 0: // Not part of a block
      assert(0);
    case 1: // Angle block
      /* Skip the 'other' angles */
      state.cellN++;
      while(state.cellN <= state.pgc->nr_of_cells 
	    && state.pgc->cell_playback_tbl[state.cellN - 1].block_mode >= 2) {
	state.cellN++;
      }
      break;
    case 2: // ??
    case 3: // ??
    default:
      fprintf(stderr, "Invalid? Cell block_mode (%d), block_type (%d)\n",
	      state.pgc->cell_playback_tbl[state.cellN - 1].block_mode,
	      state.pgc->cell_playback_tbl[state.cellN - 1].block_type);
    }
    break;
  }
  
  
  /* Figure out the correct pgN for the new cell */ 
  if(set_PGN())
    play_PGC_post();

  return play_Cell();
}


static link_t play_PGC_post(void)
{
  link_t link_values;
  
  assert(state.pgc->still_time == 0); // FIXME $$$

  { //DEBUG
    int i;
    for(i = 0; state.pgc->pgc_command_tbl &&
	  i < state.pgc->pgc_command_tbl->nr_of_post; i++)
      vmPrint_CMD(i, &state.pgc->pgc_command_tbl->post_commands[i]);
  }
  
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - or a error (are there more cases?)
     - if you got to the end of the post_commands, then what ?? */
  if(state.pgc->pgc_command_tbl &&
     eval(state.pgc->pgc_command_tbl->post_commands,
	  state.pgc->pgc_command_tbl->nr_of_post, 
	  &state.registers, &link_values)) {
    return link_values;
  }
  
  // Or perhaps handle it here?
  {
    link_t link_next_pgc = {LinkNextPGC, 0, 0, 0};
    printf("** fell of the end of the pgc\n");
    assert(state.pgc->next_pgc_nr != 0);
    return link_next_pgc;
  }
}


static link_t process_command(link_t link_values)
{
  /* FIXME $$$ Move this to a separate function? */
  while(link_values.command != PlayThis) {
    
    printf("%i %i %i %i\n", link_values.command, link_values.data1, 
	   link_values.data2, link_values.data3);
    
    switch(link_values.command) {
    case LinkNoLink:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      exit(-1);
      
    case LinkTopC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      link_values = play_Cell();
      break;
    case LinkNextC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      state.cellN += 1; // What if cellN becomes > nr_of_cells?
      link_values = play_Cell();
      break;
    case LinkPrevC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      state.cellN -= 1; // What if cellN becomes < 1?
      link_values = play_Cell();
      break;
      
    case LinkTopPG:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      // Perhaps update pgN to current value first?
      //state.pgN = ?
      link_values = play_PG();
      break;
    case LinkNextPG:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      // Perhaps update pgN to current value first?
      state.pgN += 1; // What if pgN becomes > pgc.nr_of_programs?
      link_values = play_PG();
      break;
    case LinkPrevPG:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      // Perhaps update pgN to current valu first?
      state.pgN -= 1; // What if pgN becomes < 1?
      link_values = play_PG();
      break;
      
    case LinkTopPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      link_values = play_PGC();
      break;
    case LinkNextPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      exit(-1);
    case LinkPrevPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      exit(-1);
    case LinkGoUpPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      exit(-1);
    case LinkTailPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      link_values = play_PGC_post();
      break;
      
    case LinkRSM:
      {
	int i;
	// Check and see if there is any rsm info!!
	state.domain = VTS_DOMAIN;
	ifoOpenNewVTSI(dvd, state.rsm_vtsN);
	get_PGC(state.rsm_pgcN);
	
	set_spu_palette(state.pgc->palette); // Erhum...
	
	/* These should never be set in SystemSpace and/or MenuSpace */ 
	/* state.TTN_REG = rsm_tt; ?? */
	/* state.TT_PGCN_REG = state.rsm_pgcN; ?? */
	for(i = 0; i < 5; i++) {
	  state.registers.SPRM[4 + i] = state.rsm_regs[i];
	}
	
	if(link_values.data1 != 0)
	  state.HL_BTNN_REG = link_values.data1 << 10;
	
	if(state.rsm_cellN == 0) {
	  assert(state.cellN); // Checking if this ever happens
	  /* assert( time/block/vobu is 0 ); */
	  state.pgN = 1;
	  link_values = play_PG();
	} else { 
	  /* assert( time/block/vobu is _not_ 0 ); */
	  /* play_Cell_at_time */
	  //state.pgN = ?? this gets the righ value in play_Cell
	  state.cellN = state.rsm_cellN;
	  link_values.command = PlayThis;
	  link_values.data1 = state.rsm_blockN;
	}
      }
      break;
    case LinkPGCN:
      get_PGC(link_values.data1);
      link_values = play_PGC();
      break;
    case LinkPTTN:
      assert(state.domain == VTS_DOMAIN);
      if(link_values.data2 != 0)
	state.HL_BTNN_REG = link_values.data2 << 10;
      if(get_VTS_PTT(state.vtsN, state.VTS_TTN_REG, link_values.data1) == -1)
	assert(3 == 5);
      link_values = play_PG();
      break;
    case LinkPGN:
      if(link_values.data2 != 0)
	state.HL_BTNN_REG = link_values.data2 << 10;
      state.pgN = link_values.data1;
      link_values = play_PG();
      break;
    case LinkCN:
      if(link_values.data2 != 0)
	state.HL_BTNN_REG = link_values.data2 << 10;
      state.cellN = link_values.data1;
      link_values = play_Cell();
      break;
      
    case Exit:
      exit(-1); // What should we do here??
      
    case JumpTT:
      assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
      if(get_TT(link_values.data1) == -1)
	assert(1 == 2);
      link_values = play_PGC();
      break;
    case JumpVTS_TT:
      assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
      if(get_VTS_TT(state.vtsN, link_values.data1) == -1)
	assert(2 ==  3);
      link_values = play_PGC();
      break;
    case JumpVTS_PTT:
      assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
      if(get_VTS_PTT(state.vtsN, link_values.data1, link_values.data2) == -1)
	assert(3 == 4);
      link_values = play_PG();
      break;
      
    case JumpSS_FP:
      assert(state.domain == VMGM_DOMAIN || state.domain == VTSM_DOMAIN); //??
      get_FP_PGC();
      link_values = play_PGC();
      break;
    case JumpSS_VMGM_MENU:
      assert(state.domain == VMGM_DOMAIN || 
	     state.domain == VTSM_DOMAIN || 
	     state.domain == FP_DOMAIN); //??
      state.domain = VMGM_DOMAIN;
      get_MENU(link_values.data1);
      link_values = play_PGC();
      break;
    case JumpSS_VTSM:
      assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
      // I don't know what title is supposed to be used for.
      // To set state.TTN_REG ? or ? alien or aliens had this != 1, I think.
      assert(link_values.data2 == 1);
      state.domain = VTSM_DOMAIN;
      ifoOpenNewVTSI(dvd, link_values.data1); //state.vtsN = link_values.data1;
      get_MENU(link_values.data3);
      link_values = play_PGC();
      break;
    case JumpSS_VMGM_PGC:
      assert(state.domain == VMGM_DOMAIN ||
	     state.domain == VTSM_DOMAIN ||
	     state.domain == FP_DOMAIN); //??
      state.domain = VMGM_DOMAIN;
      get_PGC(link_values.data1);
      link_values = play_PGC();
      break;
      
    case CallSS_FP:
      assert(state.domain == VTS_DOMAIN); //??   
      // Must be called before domain is changed
      saveRSMinfo(link_values.data1, /* We dont have block info */ 0);
      get_FP_PGC();
      link_values = play_PGC();
      break;
    case CallSS_VMGM_MENU:
      assert(state.domain == VTS_DOMAIN); //??   
      // Must be called before domain is changed
      saveRSMinfo(link_values.data2, /* We dont have block info */ 0);      
      state.domain = VMGM_DOMAIN;
      if(get_MENU(link_values.data1) != -1) {
	link_values = play_PGC();
      } else {
	link_values.command = LinkRSM; /* This might be dangerous */
	link_values.data1 = 0;
	link_values.data2 = 0;
	link_values.data3 = 0;
      }
      break;
    case CallSS_VTSM:
      assert(state.domain == VTS_DOMAIN); //??   
      // Must be called before domain is changed
      saveRSMinfo(link_values.data2, /* We dont have block info */ 0);
      state.domain = VTSM_DOMAIN;
      if(get_MENU(link_values.data1) != -1) {
	link_values = play_PGC();
      } else {
	link_values.command = LinkRSM; /* This might be dangerous */
	link_values.data1 = 0;
	link_values.data2 = 0;
	link_values.data3 = 0;
      }
      break;
    case CallSS_VMGM_PGC:
      assert(state.domain == VTS_DOMAIN); //??   
      // Must be called before domain is changed
      saveRSMinfo(link_values.data2, /* We dont have block info */ 0);
      state.domain = VMGM_DOMAIN;
      if(get_PGC(link_values.data1) != -1) {
	link_values = play_PGC();
      } else {
	link_values.command = LinkRSM; /* This might be dangerous */
	link_values.data1 = 0;
	link_values.data2 = 0;
	link_values.data3 = 0;
      }
      break;
    case PlayThis:
      /* Should never happen. */
      break;
    }
    
  }
  return link_values;
}








static int get_TT(int tt)
{  
  if(vmgi->tt_srpt == NULL) {
    ifoRead_TT_SRPT(vmgi);
  }
  assert(tt <= vmgi->tt_srpt->nr_of_srpts);
  
  state.TTN_REG = tt;
  
  return get_VTS_TT(vmgi->tt_srpt->title_info[tt - 1].title_set_nr,
		    vmgi->tt_srpt->title_info[tt - 1].vts_ttn);
}


static int get_VTS_TT(int vtsN, int vts_ttn)
{
  int pgcN;
  
  state.domain = VTS_DOMAIN;
  if(vtsN != state.vtsN)
    ifoOpenNewVTSI(dvd, vtsN); // Also sets state.vtsN
  
  pgcN = get_ID(vts_ttn); // This might return -1
  assert(pgcN != -1);
  
  state.VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  return get_PGC(pgcN);
}


static int get_VTS_PTT(int vtsN, int /* is this really */ vts_ttn, int part)
{
  int pgcN, pgN;
  
  state.domain = VTS_DOMAIN;
  if(vtsN != state.vtsN)
    ifoOpenNewVTSI(dvd, vtsN); // Also sets state.vtsN
  
  if(vtsi->vts_ptt_srpt == NULL)
    ifoRead_VTS_PTT_SRPT(vtsi);

  
  assert(vts_ttn <= vtsi->vts_ptt_srpt->nr_of_srpts);
  assert(part <= vtsi->vts_ptt_srpt->title_info[vts_ttn - 1].nr_of_ptts);
  
  pgcN = vtsi->vts_ptt_srpt->title_info[vts_ttn - 1].ptt_info[part - 1].pgcn;
  pgN = vtsi->vts_ptt_srpt->title_info[vts_ttn - 1].ptt_info[part - 1].pgn;
  
  //state.TTN_REG = ?? 
  state.VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  state.pgN = pgN; // ??
  
  return get_PGC(pgcN);
}



static int get_FP_PGC(void)
{  
  state.domain = FP_DOMAIN;
  if(vmgi->first_play_pgc == NULL)
    ifoRead_FP_PGC(vmgi);

  state.pgc = vmgi->first_play_pgc;
  
  return 0;
}


static int get_MENU(int menu)
{
  assert(state.domain == VMGM_DOMAIN || state.domain == VTSM_DOMAIN);
  return get_PGC(get_ID(menu));
}

static int get_ID(int id)
{
  int pgcN, i;
  pgcit_t *pgcit;
  
  /* Relies on state to get the correct pgcit. */
  pgcit = get_PGCIT();
  assert(pgcit != NULL);
  
  /* Get menu/title */
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    if((pgcit->pgci_srp[i].entry_id & 0x7f) == id) {
      assert((pgcit->pgci_srp[i].entry_id & 0x80) == 0x80);
      pgcN = i + 1;
      return pgcN;
    }
  }
  printf("** No such menu\n");
  return -1; // error
}



static int get_PGC(int pgcN)
{
  pgcit_t *pgcit;
  
  pgcit = get_PGCIT();
  
  assert(pgcit != NULL);
  if(pgcN < 0 || pgcN > pgcit->nr_of_pgci_srp)
    return -1; // error
  
  //state.pgcN = pgcN;
  state.pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  
  if(state.domain == VTS_DOMAIN)
    state.TT_PGCN_REG = pgcN;

  return 0;
}

static int get_PGCN()
{
  pgcit_t *pgcit;
  int pgcN = 1;

  pgcit = get_PGCIT();
  
  assert(pgcit != NULL);
  
  while(pgcN <= pgcit->nr_of_pgci_srp) {
    if(pgcit->pgci_srp[pgcN - 1].pgc == state.pgc)
      return pgcN;
    pgcN++;
  }
  
  return -1; // error
}


static int get_video_aspect(void)
{
  int aspect = 0;
  
  if(state.domain == VTS_DOMAIN) {
    aspect = vtsi->vtsi_mat->vts_video_attributes.display_aspect_ratio;  
  } else if(state.domain == VTSM_DOMAIN) {
    aspect = vtsi->vtsi_mat->vtsm_video_attributes.display_aspect_ratio;
  } else if(state.domain == VMGM_DOMAIN) {
    aspect = vmgi->vmgi_mat->vmgm_video_attributes.display_aspect_ratio;
  }
  assert(aspect == 0 || aspect == 3);
  state.registers.SPRM[14] &= ~(0x3 << 10);
  state.registers.SPRM[14] |= aspect << 10;
  
  return aspect;
}






static void ifoOpenNewVTSI(dvd_reader_t *dvd, int vtsN) 
{
  if(vtsi != NULL)
    ifoClose(vtsi);
  
  vtsi = ifoOpenVTSI(dvd, vtsN);
  
  state.vtsN = vtsN;
}




static pgcit_t* get_VTS_PGCIT()
{
  if(vtsi->vts_pgcit == NULL)
    ifoRead_PGCIT(vtsi);
  
  return vtsi->vts_pgcit;
}

static pgcit_t* get_VTSM_PGCIT(uint16_t lang)
{
  int i;
  
  if(vtsi->vtsm_pgci_ut == NULL)
    ifoRead_MENU_PGCI_UT(vtsi);
  
  if(vtsi->vtsm_pgci_ut == NULL)
    return NULL; // error!
  
  i = 0;
  while(i < vtsi->vtsm_pgci_ut->nr_of_lang_units
	&& vtsi->vtsm_pgci_ut->menu_lu[i].lang_code != lang)
    i++;
  if(i == vtsi->vtsm_pgci_ut->nr_of_lang_units) {
    printf("** Wrong language **\n");
    i = 0; // error?
  }
  
  return vtsi->vtsm_pgci_ut->menu_lu[i].menu_pgcit;
}

static pgcit_t* get_VMGM_PGCIT(uint16_t lang)
{
  int i;
  
  if(vmgi->vmgm_pgci_ut == NULL)
    ifoRead_MENU_PGCI_UT(vmgi);
  
  if(vmgi->vmgm_pgci_ut == NULL)
    return NULL; // error!
 
  i = 0;
  while(i < vmgi->vmgm_pgci_ut->nr_of_lang_units
	&& vmgi->vmgm_pgci_ut->menu_lu[i].lang_code != lang)
    i++;
  if(i == vmgi->vmgm_pgci_ut->nr_of_lang_units) {
    printf("** Wrong language **\n");
    i = 0; // error?
  }
  
  return vmgi->vmgm_pgci_ut->menu_lu[i].menu_pgcit;
}

/* Uses state to decide what to return */
static pgcit_t* get_PGCIT(void) {
  pgcit_t *pgcit;
  
  if(state.domain == VTS_DOMAIN) {
    pgcit = get_VTS_PGCIT();
  } else if(state.domain == VTSM_DOMAIN) {
    pgcit = get_VTSM_PGCIT(state.registers.SPRM[0]);
  } else if(state.domain == VMGM_DOMAIN) {
    pgcit = get_VMGM_PGCIT(state.registers.SPRM[0]);
  } else {
    pgcit = NULL;    /* Should never hapen */
  }
  
  return pgcit;
}
