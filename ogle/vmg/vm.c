/* Ogle - A video player
 * Copyright (C) 2000, 2001 Håkan Hjort
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


static dvd_reader_t *dvd;
static ifo_handle_t *vmgi;
static ifo_handle_t *vtsi;

dvd_state_t state;





/* Local prototypes */

static void saveRSMinfo(int cellN, int blockN);
static int set_PGN(void);
static link_t play_PGC(void);
static link_t play_PG(void);
static link_t play_Cell(void);
static link_t play_Cell_post(void);
static link_t play_PGC_post(void);
static link_t process_command(link_t link_values);

static void ifoOpenNewVTSI(dvd_reader_t *dvd, int vtsN);
static pgcit_t* get_PGCIT(void);
static int get_video_aspect(void);

/* Can only be called when in VTS_DOMAIN */
static int get_TT(int tt);
static int get_VTS_TT(int vtsN, int vts_ttn);
static int get_VTS_PTT(int vtsN, int vts_ttn, int part);

static int get_MENU(int menu); // VTSM & VMGM
static int get_FP_PGC(void); // FP

/* Called in any domain */
static int get_ID(int id);
static int get_PGC(int pgcN);
static int get_PGCN(void);


int set_sprm(unsigned int nr, uint16_t val)
{
  if(nr < 0 || nr > 23) {
    return 0;
  }

  state.registers.SPRM[nr] = val;

  return 1;
}


int vm_reset(void) // , register_t regs)
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
}  


int vm_init(char *dvdroot) // , register_t regs)
{
  dvd = DVDOpen(dvdroot);
  if(!dvd) {
    fprintf(stderr, "vm: faild to open/read the DVD\n");
    return -1;
  }
  vmgi = ifoOpenVMGI(dvd);
  if(!vmgi) {
    fprintf(stderr, "vm: faild to read VIDEO_TS.IFO\n");
    return -1;
  }
  if(!ifoRead_FP_PGC(vmgi)) {
    fprintf(stderr, "vm: ifoRead_FP_PGC faild\n");
    return -1;
  }
  if(!ifoRead_TT_SRPT(vmgi)) {
    fprintf(stderr, "vm: ifoRead_TT_SRPT faild\n");
    return -1;
  }
  if(!ifoRead_PGCI_UT(vmgi)) {
    fprintf(stderr, "vm: ifoRead_PGCI_UT faild\n");
    return -1;
  }
  if(!ifoRead_PTL_MAIT(vmgi)) {
    fprintf(stderr, "vm: ifoRead_PTL_MAIT faild\n");
    ; // return -1; Not really used for now..
  }
  if(!ifoRead_VTS_ATRT(vmgi)) {
    fprintf(stderr, "vm: ifoRead_VTS_ATRT faild\n");
    ; // return -1; Not really used for now..
  }
  //ifoRead_TXTDT_MGI(vmgi); Not implemented yet

  return 0;
}




// FIXME TODO XXX $$$ Handle error condition too...
int vm_start(void)
{
  link_t link_values;

  // Set pgc to FP pgc
  get_FP_PGC();
  link_values = play_PGC(); 
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;

  return 0; //??
}


int vm_eval_cmd(vm_cmd_t *cmd)
{
  link_t link_values;
  
  if(vmEval_CMD(cmd, 1, &state.registers, &link_values)) {
    link_values = process_command(link_values);
    assert(link_values.command == PlayThis);
    state.blockN = link_values.data1;
    return 1; // Something changed, Jump
  } else {
    return 0; // It updated some state thats all...
  }
}

int vm_get_next_cell(void)
{
  link_t link_values;
  link_values = play_Cell_post();
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;
  
  return 0; // ??
}

int vm_top_pg(void)
{
  link_t link_values;
  link_values = play_PG();
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;
  
  return 1; // Jump
}

int vm_next_pg(void)
{
  // Do we need to get a updated pgN value first?
  state.pgN += 1; 
  return vm_top_pg();
}

int vm_prev_pg(void)
{
  // Do we need to get a updated pgN value first?
  state.pgN -= 1;
  if(state.pgN == 0) {
    // Check for previous PGCN ? 
    state.pgN = 1;
    //return 0;
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
    if(get_PGCIT() != NULL && get_MENU(menuid) != -1) {
      link_values = play_PGC();
      link_values = process_command(link_values);
      assert(link_values.command == PlayThis);
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
    link_values = process_command(link_values);
    assert(link_values.command == PlayThis);
    state.blockN = link_values.data1;
  } else { 
    state.cellN = state.rsm_cellN;
    state.blockN = state.rsm_blockN;
    //state.pgN = ?? does this gets the righ value in play_Cell, no!
    if(set_PGN()) {
      ; /* Were at or past the end of the PGC, should not happen for a RSM */
      assert(0);
      play_PGC_post();
    }
  }
  
  return 1; // Jump
}

/**
 * Return the substream id for 'logical' audio stream audioN.
 *  0 <= audioN < 8
 */
int vm_get_audio_stream(int audioN)
{
  int streamN = -1;
  
  if(state.domain == VTSM_DOMAIN 
     || state.domain == VMGM_DOMAIN
     || state.domain == FP_DOMAIN) {
    audioN = 0;
  }
  
  if(audioN < 8) {
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
   * it is (ac3/lpcm/dts/sdds...) to find the right (sub)stream id */
  return streamN;
}

/**
 * Return the substream id for 'logical' subpicture stream subpN.
 * 0 <= subpN < 32
 */
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

  /* Should also check in vtsi/vmgi status that what kind of stream it is. */
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

void vm_get_angle_info(int *num_avail, int *current)
{
  *num_avail = 1;
  *current = 1;
  
  if(state.domain == VTS_DOMAIN) {
    // TTN_REG does not allways point to the correct title..
    title_info_t *title;
    if(state.TTN_REG > vmgi->tt_srpt->nr_of_srpts)
      return;
    title = &vmgi->tt_srpt->title[state.TTN_REG - 1];
    if(title->title_set_nr != state.vtsN || 
       title->vts_ttn != state.VTS_TTN_REG)
      return; 
    *num_avail = title->nr_of_angles;
    *current = state.AGL_REG;
    if(*current > *num_avail) // Is this really a good idee?
      *current = *num_avail; 
  }
}


void vm_get_audio_info(int *num_avail, int *current)
{
  if(state.domain == VTS_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vts_audio_streams;
    *current = state.AST_REG;
  } else if(state.domain == VTSM_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vtsm_audio_streams; // 1
    *current = 1;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    *num_avail = vmgi->vmgi_mat->nr_of_vmgm_audio_streams; // 1
    *current = 1;
  }
}

void vm_get_subp_info(int *num_avail, int *current)
{
  if(state.domain == VTS_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vts_subp_streams;
    *current = state.SPST_REG;
  } else if(state.domain == VTSM_DOMAIN) {
    *num_avail = vtsi->vtsi_mat->nr_of_vtsm_subp_streams; // 1
    *current = 0x41;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    *num_avail = vmgi->vmgi_mat->nr_of_vmgm_subp_streams; // 1
    *current = 0x41;
  }
}

subp_attr_t vm_get_subp_attr(int streamN)
{
  subp_attr_t attr;
  
  if(state.domain == VTS_DOMAIN) {
    attr = vtsi->vtsi_mat->vts_subp_attr[streamN];
  } else if(state.domain == VTSM_DOMAIN) {
    attr = vtsi->vtsi_mat->vtsm_subp_attr;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    attr = vmgi->vmgi_mat->vmgm_subp_attr;
  }
  return attr;
}

audio_attr_t vm_get_audio_attr(int streamN)
{
  audio_attr_t attr;
  
  if(state.domain == VTS_DOMAIN) {
    attr = vtsi->vtsi_mat->vts_audio_attr[streamN];
  } else if(state.domain == VTSM_DOMAIN) {
    attr = vtsi->vtsi_mat->vtsm_audio_attr;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    attr = vmgi->vmgi_mat->vmgm_audio_attr;
  }
  return attr;
}


void vm_get_video_res(int *width, int *height) {
  video_attr_t attr;
  
  if(state.domain == VTS_DOMAIN) {
    attr = vtsi->vtsi_mat->vts_video_attr;
  } else if(state.domain == VTSM_DOMAIN) {
    attr = vtsi->vtsi_mat->vtsm_video_attr;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    attr = vmgi->vmgi_mat->vmgm_video_attr;
  }
  if(attr.video_format != 0) 
    *height = 576;
  else
    *height = 480;
  switch(attr.picture_size) {
  case 0:
    *width = 720;
    break;
  case 1:
    *width = 704;
    break;
  case 2:
    *width = 352;
    break;
  case 3:
    *width = 352;
    *height /= 2;
    break;
  }
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
	&& state.cellN >= state.pgc->program_map[new_pgN])
    new_pgN++;
  
  if(new_pgN == state.pgc->nr_of_programs) /* We are at the last program */
    if(state.cellN > state.pgc->nr_of_cells)
      return 1; /* We are past the last cell */
  
  state.pgN = new_pgN;
  
  if(state.domain == VTS_DOMAIN) {
    playback_type_t *pb_ty;
    if(state.TTN_REG > vmgi->tt_srpt->nr_of_srpts)
      return 0; // ??
    pb_ty = &vmgi->tt_srpt->title[state.TTN_REG - 1].pb_ty;
    if(pb_ty->multi_or_random_pgc_title == /* One_Sequential_PGC_Title */ 0) {
#if 0 /* TTN_REG can't be trusted to have a correct value here... */
      vts_ptt_srpt_t *ptt_srpt = vtsi->vts_ptt_srpt;
      assert(state.VTS_TTN_REG <= ptt_srpt->nr_of_srpts);
      assert(get_PGCN() == ptt_srpt->title[state.VTS_TTN_REG - 1].ptt[0].pgcn);
      assert(1 == ptt_srpt->title[state.VTS_TTN_REG - 1].ptt[0].pgn);
#endif
      state.PTTN_REG = state.pgN;
    }
  }
  
  return 0;
}





static link_t play_PGC(void) 
{    
  link_t link_values;
  
  fprintf(stderr, "vm: play_PGC:");
  if(state.domain != FP_DOMAIN)
    fprintf(stderr, " state.pgcN (%i)\n", get_PGCN());
  else
    fprintf(stderr, " first_play_pgc\n");

  // This must be set before the pre-commands are executed because they
  // might contain a CallSS that will save resume state
  state.pgN = 1;
  state.cellN = 0;

  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG
       (This is what happens if you fall of the end of the pre_cmds)
     - or a error (are there more cases?) */
  if(state.pgc->command_tbl && state.pgc->command_tbl->nr_of_pre) {
    if(vmEval_CMD(state.pgc->command_tbl->pre_cmds, 
		  state.pgc->command_tbl->nr_of_pre, 
		  &state.registers, &link_values)) {
      // link_values contains the 'jump' return value
      return link_values;
    } else {
      fprintf(stderr, "PGC pre commands didn't do a Jump, Link or Call\n");
    }
  }
  return play_PG();
}  


static link_t play_PG(void)
{
  fprintf(stderr, "play_PG: state.pgN (%i)\n", state.pgN);
  
  assert(state.pgN > 0);
  if(state.pgN > state.pgc->nr_of_programs) {
    fprintf(stderr, "state.pgN (%i) == pgc->nr_of_programs + 1 (%i)\n", 
	    state.pgN, state.pgc->nr_of_programs + 1);
    assert(state.pgN == state.pgc->nr_of_programs + 1);
    return play_PGC_post();
  }
  
  state.cellN = state.pgc->program_map[state.pgN - 1];
  
  return play_Cell();
}


static link_t play_Cell(void)
{
  fprintf(stderr, "play_Cell: state.cellN (%i)\n", state.cellN);
  
  assert(state.cellN > 0);
  if(state.cellN > state.pgc->nr_of_cells) {
    fprintf(stderr, "state.cellN (%i) == pgc->nr_of_cells + 1 (%i)\n", 
	    state.cellN, state.pgc->nr_of_cells + 1);
    assert(state.cellN == state.pgc->nr_of_cells + 1); 
    return play_PGC_post();
  }
  

  /* Multi angle/Interleaved */
  switch(state.pgc->cell_playback[state.cellN - 1].block_mode) {
  case 0: // Normal
    assert(state.pgc->cell_playback[state.cellN - 1].block_type == 0);
    break;
  case 1: // The first cell in the block
    switch(state.pgc->cell_playback[state.cellN - 1].block_type) {
    case 0: // Not part of a block
      assert(0);
    case 1: // Angle block
      /* Loop and check each cell instead? So we don't get outsid the block. */
      state.cellN += state.AGL_REG - 1;
      assert(state.cellN <= state.pgc->nr_of_cells);
      assert(state.pgc->cell_playback[state.cellN - 1].block_mode != 0);
      assert(state.pgc->cell_playback[state.cellN - 1].block_type == 1);
      break;
    case 2: // ??
    case 3: // ??
    default:
      fprintf(stderr, "Invalid? Cell block_mode (%d), block_type (%d)\n",
	      state.pgc->cell_playback[state.cellN - 1].block_mode,
	      state.pgc->cell_playback[state.cellN - 1].block_type);
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
    link_t tmp = {LinkTailPGC, /* No Button */ 0, 0, 0};
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
  
  fprintf(stderr, "play_Cell_post: state.cellN (%i)\n", state.cellN);
  
  cell = &state.pgc->cell_playback[state.cellN - 1];
  
  /* Still time is already taken care of before we get called. */
  
  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;
    
    assert(state.pgc->command_tbl != NULL);
    assert(state.pgc->command_tbl->nr_of_cell >= cell->cell_cmd_nr);
    fprintf(stderr, "Cell command pressent, executing\n");
    if(vmEval_CMD(&state.pgc->command_tbl->cell_cmds[cell->cell_cmd_nr - 1], 1,
		  &state.registers, &link_values)) {
      return link_values;
    } else {
       fprintf(stderr, "Cell command didn't do a Jump, Link or Call\n");
      // Error ?? goto tail? goto next PG? or what? just continue?
    }
  }
  
  
  /* Where to continue after playing the cell... */
  /* Multi angle/Interleaved */
  switch(state.pgc->cell_playback[state.cellN - 1].block_mode) {
  case 0: // Normal
    assert(state.pgc->cell_playback[state.cellN - 1].block_type == 0);
    state.cellN++;
    break;
  case 1: // The first cell in the block
  case 2: // A cell in the block
  case 3: // The last cell in the block
  default:
    switch(state.pgc->cell_playback[state.cellN - 1].block_type) {
    case 0: // Not part of a block
      assert(0);
    case 1: // Angle block
      /* Skip the 'other' angles */
      state.cellN++;
      while(state.cellN <= state.pgc->nr_of_cells 
	    && state.pgc->cell_playback[state.cellN - 1].block_mode >= 2) {
	state.cellN++;
      }
      break;
    case 2: // ??
    case 3: // ??
    default:
      fprintf(stderr, "Invalid? Cell block_mode (%d), block_type (%d)\n",
	      state.pgc->cell_playback[state.cellN - 1].block_mode,
	      state.pgc->cell_playback[state.cellN - 1].block_type);
    }
    break;
  }
  
  
  /* Figure out the correct pgN for the new cell */ 
  if(set_PGN()) {
    fprintf(stderr, "last cell in this PGC\n");
    return play_PGC_post();
  }

  return play_Cell();
}


static link_t play_PGC_post(void)
{
  link_t link_values;

  fprintf(stderr, "play_PGC_post:\n");
  
  assert(state.pgc->still_time == 0); // FIXME $$$

  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - or a error (are there more cases?)
     - if you got to the end of the post_cmds, then what ?? */
  if(state.pgc->command_tbl &&
     vmEval_CMD(state.pgc->command_tbl->post_cmds,
		state.pgc->command_tbl->nr_of_post, 
		&state.registers, &link_values)) {
    return link_values;
  }
  
  // Or perhaps handle it here?
  {
    link_t link_next_pgc = {LinkNextPGC, 0, 0, 0};
    fprintf(stderr, "** Fell of the end of the pgc, continuing in NextPGC\n");
    assert(state.pgc->next_pgc_nr != 0);
    return link_next_pgc;
  }
}


static link_t process_command(link_t link_values)
{
  /* FIXME $$$ Move this to a separate function? */
  while(link_values.command != PlayThis) {
    
    vmPrint_LINK(link_values);
    /*
    fprintf(stderr, "%i %i %i %i\n", link_values.command, 
	    link_values.data1, link_values.data2, link_values.data3);
    */
    switch(link_values.command) {
    case LinkNoLink:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      exit(1);
      
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
      // Does pgN always contain the current value?
      link_values = play_PG();
      break;
    case LinkNextPG:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      // Does pgN always contain the current value?
      state.pgN += 1; // What if pgN becomes > pgc.nr_of_programs?
      link_values = play_PG();
      break;
    case LinkPrevPG:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      // Does pgN always contain the current value?
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
      assert(state.pgc->next_pgc_nr != 0);
      if(get_PGC(state.pgc->next_pgc_nr))
	assert(0);
      link_values = play_PGC();
      break;
    case LinkPrevPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      assert(state.pgc->prev_pgc_nr != 0);
      if(get_PGC(state.pgc->prev_pgc_nr))
	assert(0);
      link_values = play_PGC();
      break;
    case LinkGoUpPGC:
      if(link_values.data1 != 0)
	state.HL_BTNN_REG = link_values.data1 << 10;
      assert(state.pgc->goup_pgc_nr != 0);
      if(get_PGC(state.pgc->goup_pgc_nr))
	assert(0);
      link_values = play_PGC();
      break;
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
	  if(set_PGN()) {
	    /* Were at the end of the PGC, should not happen for a RSM */
	    assert(0);
	    link_values.command = LinkTailPGC;
	    link_values.data1 = 0;  /* No button */
	  }
	}
      }
      break;
    case LinkPGCN:
      if(get_PGC(link_values.data1))
	assert(0);
      link_values = play_PGC();
      break;
    case LinkPTTN:
      assert(state.domain == VTS_DOMAIN);
      if(link_values.data2 != 0)
	state.HL_BTNN_REG = link_values.data2 << 10;
      if(get_VTS_PTT(state.vtsN, state.VTS_TTN_REG, link_values.data1) == -1)
	assert(0);
      link_values = play_PG();
      break;
    case LinkPGN:
      if(link_values.data2 != 0)
	state.HL_BTNN_REG = link_values.data2 << 10;
      /* Update any other state, PTTN perhaps? */
      state.pgN = link_values.data1;
      link_values = play_PG();
      break;
    case LinkCN:
      if(link_values.data2 != 0)
	state.HL_BTNN_REG = link_values.data2 << 10;
      /* Update any other state, pgN, PTTN perhaps? */
      state.cellN = link_values.data1;
      link_values = play_Cell();
      break;
      
    case Exit:
      exit(1); // What should we do here??
      
    case JumpTT:
      assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
      if(get_TT(link_values.data1) == -1)
	assert(0);
      link_values = play_PGC();
      break;
    case JumpVTS_TT:
      assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
      if(get_VTS_TT(state.vtsN, link_values.data1) == -1)
	assert(0);
      link_values = play_PGC();
      break;
    case JumpVTS_PTT:
      assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
      if(get_VTS_PTT(state.vtsN, link_values.data1, link_values.data2) == -1)
	assert(0);
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
      if(get_MENU(link_values.data1) == -1)
	assert(0);
      link_values = play_PGC();
      break;
    case JumpSS_VTSM:
      if(link_values.data1 !=0) {
	assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
	state.domain = VTSM_DOMAIN;
	ifoOpenNewVTSI(dvd, link_values.data1);  // Also sets state.vtsN
      } else {
	// This happens on 'The Fifth Element' region 2.
	assert(state.domain == VTSM_DOMAIN);
      }
      // I don't know what title is supposed to be used for.
      // Alien or Aliens has this != 1, I think.
      //assert(link_values.data2 == 1);
      state.VTS_TTN_REG = link_values.data2;
      if(get_MENU(link_values.data3) == -1)
	assert(0);
      link_values = play_PGC();
      break;
    case JumpSS_VMGM_PGC:
      assert(state.domain == VMGM_DOMAIN ||
	     state.domain == VTSM_DOMAIN ||
	     state.domain == FP_DOMAIN); //??
      state.domain = VMGM_DOMAIN;
      if(get_PGC(link_values.data1) == -1)
	assert(0);
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
      if(get_MENU(link_values.data1) == -1)
	assert(0);
      link_values = play_PGC();
      break;
    case CallSS_VTSM:
      assert(state.domain == VTS_DOMAIN); //??   
      // Must be called before domain is changed
      saveRSMinfo(link_values.data2, /* We dont have block info */ 0);
      state.domain = VTSM_DOMAIN;
      if(get_MENU(link_values.data1) == -1)
	assert(0);
      link_values = play_PGC();
      break;
    case CallSS_VMGM_PGC:
      assert(state.domain == VTS_DOMAIN); //??   
      // Must be called before domain is changed
      saveRSMinfo(link_values.data2, /* We dont have block info */ 0);
      state.domain = VMGM_DOMAIN;
      if(get_PGC(link_values.data1) == -1)
	assert(0);
      link_values = play_PGC();
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
  assert(tt <= vmgi->tt_srpt->nr_of_srpts);
  
  state.TTN_REG = tt;
  
  return get_VTS_TT(vmgi->tt_srpt->title[tt - 1].title_set_nr,
		    vmgi->tt_srpt->title[tt - 1].vts_ttn);
}


static int get_VTS_TT(int vtsN, int vts_ttn)
{
  int pgcN;
  
  state.domain = VTS_DOMAIN;
  if(vtsN != state.vtsN)
    ifoOpenNewVTSI(dvd, vtsN); // Also sets state.vtsN
  
  pgcN = get_ID(vts_ttn); // This might return -1
  assert(pgcN != -1);

  //state.TTN_REG = ?? Must search tt_srpt for a matching entry...  
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
  
  assert(vts_ttn <= vtsi->vts_ptt_srpt->nr_of_srpts);
  assert(part <= vtsi->vts_ptt_srpt->title[vts_ttn - 1].nr_of_ptts);
  
  pgcN = vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgcn;
  pgN = vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgn;
  
  //state.TTN_REG = ?? Must search tt_srpt for a matchhing entry...
  state.VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  state.pgN = pgN; // ??
  
  return get_PGC(pgcN);
}



static int get_FP_PGC(void)
{  
  state.domain = FP_DOMAIN;

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
  fprintf(stderr, "** No such id/menu (%d) entry PGC\n", id);
  return -1; // error
}



static int get_PGC(int pgcN)
{
  pgcit_t *pgcit;
  
  pgcit = get_PGCIT();
  
  assert(pgcit != NULL); // ?? Make this return -1 instead
  if(pgcN < 1 || pgcN > pgcit->nr_of_pgci_srp)
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
    aspect = vtsi->vtsi_mat->vts_video_attr.display_aspect_ratio;  
  } else if(state.domain == VTSM_DOMAIN) {
    aspect = vtsi->vtsi_mat->vtsm_video_attr.display_aspect_ratio;
  } else if(state.domain == VMGM_DOMAIN) {
    aspect = vmgi->vmgi_mat->vmgm_video_attr.display_aspect_ratio;
  }
  assert(aspect == 0 || aspect == 3);
  state.registers.SPRM[14] &= ~(0x3 << 10);
  state.registers.SPRM[14] |= aspect << 10;
  
  return aspect;
}






static void ifoOpenNewVTSI(dvd_reader_t *dvd, int vtsN) 
{
  if(state.vtsN == vtsN) {
    return; // We alread have it
  }
  
  if(vtsi != NULL)
    ifoClose(vtsi);
  
  vtsi = ifoOpenVTSI(dvd, vtsN);
  if(vtsi == NULL) {
    fprintf(stderr, "ifoOpenVTSI failed\n");
    exit(1);
  }
  if(!ifoRead_VTS_PTT_SRPT(vtsi)) {
    fprintf(stderr, "ifoRead_VTS_PTT_SRPT failed\n");
    exit(1);
  }
  if(!ifoRead_PGCIT(vtsi)) {
    fprintf(stderr, "ifoRead_PGCIT failed\n");
    exit(1);
  }
  if(!ifoRead_PGCI_UT(vtsi)) {
    fprintf(stderr, "ifoRead_PGCI_UT failed\n");
    exit(1);
  }
  state.vtsN = vtsN;
}




static pgcit_t* get_MENU_PGCIT(ifo_handle_t *h, uint16_t lang)
{
  int i;
  
  if(h == NULL || h->pgci_ut == NULL) {
    fprintf(stderr, "*** pgci_ut handle is NULL ***\n");
    return NULL; // error?
  }
  
  i = 0;
  while(i < h->pgci_ut->nr_of_lus
	&& h->pgci_ut->lu[i].lang_code != lang)
    i++;
  if(i == h->pgci_ut->nr_of_lus) {
    fprintf(stderr, "** Wrong language, using %c%c instead of %c%c\n",
	    (char)(h->pgci_ut->lu[0].lang_code >> 8),
	    (char)(h->pgci_ut->lu[0].lang_code & 0xff),
	    (char)(lang >> 8), (char)(lang & 0xff));
    i = 0; // error?
  }
  
  return h->pgci_ut->lu[i].pgcit;
}

/* Uses state to decide what to return */
static pgcit_t* get_PGCIT(void) {
  pgcit_t *pgcit;
  
  if(state.domain == VTS_DOMAIN) {
    pgcit = vtsi->vts_pgcit;
  } else if(state.domain == VTSM_DOMAIN) {
    pgcit = get_MENU_PGCIT(vtsi, state.registers.SPRM[0]);
  } else if(state.domain == VMGM_DOMAIN) {
    pgcit = get_MENU_PGCIT(vmgi, state.registers.SPRM[0]);
  } else {
    pgcit = NULL;    /* Should never hapen */
  }
  
  return pgcit;
}
