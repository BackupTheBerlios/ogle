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

#include "debug_print.h"
#ifndef TRACE
#undef DNOTE
#if defined(__GNUC__) && ( __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 95))
#define DNOTE(str, args...) while(0)
#else /* __GNUC__ < 2 */
#define DNOTE(format, ...) while(0) 
#endif 
#endif /* TRACE */


static dvd_reader_t *dvd;
static ifo_handle_t *vmgi;
static ifo_handle_t *vtsi;

dvd_state_t state;



/* Local prototypes */

static void saveRSMinfo(int cellN, int blockN);
static int update_PGN(void);
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
static int get_TT_PTT(int tt, int ptt);
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
  state.registers.SPRM[20] = 1; // Player Regional Code (bit mask?)
  
  state.pgN = 0;
  state.cellN = 0;

  state.domain = FP_DOMAIN;
  state.rsm_vtsN = 0;
  state.rsm_cellN = 0;
  state.rsm_blockN = 0;
  
  state.vtsN = -1;

  return 0;
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

int vm_jump_ptt(int pttN)
{
  link_t link_values;
  
  //check domain!! should be only VTS Title domain
  
  if(get_VTS_PTT(state.vtsN, state.VTS_TTN_REG, pttN) == -1)
    return 0;
  
  link_values = play_PGC();
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;
  return 1; // Something changed, Jump
}

int vm_jump_title_ptt(int titleN, int pttN)
{
  link_t link_values;
  
  //check domain?!?  (VMGM & (Menu) Title & Stop)?
  
  if(get_TT_PTT(titleN, pttN) == -1)
    return 0;
  
  link_values = play_PGC();
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;
  return 1; // Something changed, Jump
}

int vm_jump_title(int titleN)
{
  link_t link_values;
  
  //check domain?  VMGM & (Menu) Title & Stop
  
  if(get_TT(titleN) == -1)
    return 0;
  
  link_values = play_PGC();
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;
  return 1; // Something changed, Jump
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
  
  /* FIXME XXX $$$ How much state needs to be restored 
   * when we fail to find a menu? */
  
  old_domain = state.domain;
  
  switch(state.domain) {
  case VTS_DOMAIN:
    saveRSMinfo(0, block);
    /* FALL THROUGH */
  case VTSM_DOMAIN:
    state.domain = menuid2domain(menuid);
    /* FALL THROUGH */
  case VMGM_DOMAIN: /* or should one be able to jump from vmgm to title dom? */
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


static int vm_resume_int(link_t *link_return)
{
  int i;
  link_t link_values;
  
  // Check and see if there is any rsm info!!
  if(state.rsm_vtsN == 0) {
    return 0; // Fail
  }
  
  state.domain = VTS_DOMAIN;
  ifoOpenNewVTSI(dvd, state.rsm_vtsN); // FIXME check return value
  get_PGC(state.rsm_pgcN); // FIXME check return value
  
  /* These should never be set in SystemSpace and/or MenuSpace */ 
  // state.TTN_REG = state.rsm_tt;
  // state.TT_PGCN_REG = state.rsm_pgcN;
  // state.HL_BTNN_REG = state.rsm_btnn; ??
  for(i = 0; i < 5; i++) {
    state.registers.SPRM[4 + i] = state.rsm_regs[i];
  }

  if(state.rsm_cellN == 0) {
    assert(state.cellN); // Checking if this ever happens
    /* assert( time/block/vobu is 0 ); */
    state.pgN = 1;
    link_values = play_PG();
  } else { 
    /* assert( time/block/vobu is _not_ 0 ); */
    /* play_Cell_at_time */
    state.cellN = state.rsm_cellN;
    state.blockN = state.rsm_blockN;
    //state.pgN = ?? does this gets the righ value in play_Cell, no!
    if(update_PGN()) {
      /* Were at or past the end of the PGC, should not happen for a RSM */
      assert(0);
      link_values = play_PGC_post();
    } else {
      link_t do_nothing = { PlayThis, state.blockN, 0, 0 };
      link_values = do_nothing;
    }
  }
  
  *link_return = link_values;
  return 1; // Success
}


int vm_resume(void)
{
  link_t link_values;
  
  if(!vm_resume_int(&link_values))
    return 0;
  
  link_values = process_command(link_values);
  assert(link_values.command == PlayThis);
  state.blockN = link_values.data1;
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

  return streamN;
}

int vm_get_subp_active_stream(void)
{
  int subpN = state.SPST_REG & ~0x40;
  int streamN = vm_get_subp_stream(subpN);
  
  /* If no such stream, then select the first one that exists. */
  if(streamN == -1)
    for(subpN = 0; subpN < 32; subpN++)
      if(state.pgc->subp_control[subpN] & (1<<31)) {
	streamN = vm_get_subp_stream(subpN);
	break;
      }
  
  /* We should instead send the on/off status to the spudecoder / mixer */
  /* If we are in the title domain see if the spu mixing is on */
  if(state.domain == VTS_DOMAIN && !(state.SPST_REG & 0x40)) { 
    return -1;
  } else {
    return streamN;
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

int vm_get_domain(void)
{
 return state.domain;
}

int vm_get_titles(void)
{
 int titles = 0;

 titles = vmgi->tt_srpt->nr_of_srpts;

 return titles;
}

int vm_get_ptts_for_title(int titleN)
{
 int ptts = 0;

 if(titleN <= vmgi->tt_srpt->nr_of_srpts) {
   ptts = vmgi->tt_srpt->title[titleN - 1].nr_of_ptts;
 }

 return ptts;
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

user_ops_t vm_get_uops(void)
{
  return state.pgc->prohibited_ops;
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

video_attr_t vm_get_video_attr(void)
{
  video_attr_t attr;
  
  if(state.domain == VTS_DOMAIN) {
    attr = vtsi->vtsi_mat->vts_video_attr;
  } else if(state.domain == VTSM_DOMAIN) {
    attr = vtsi->vtsi_mat->vtsm_video_attr;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    attr = vmgi->vmgi_mat->vmgm_video_attr;
  }
  return attr;
}

void vm_get_video_res(int *width, int *height)
{
  video_attr_t attr;
  
  attr = vm_get_video_attr();
  
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

unsigned int bcd2int(unsigned int bcd)
{
  unsigned int pot = 1; 
  unsigned int res = 0; 
  while(bcd != 0) {
    res += (bcd & 0x0f) * pot;
    bcd >>= 4;
    pot *= 10;
  }
  return res;
}

unsigned int int2bcd(unsigned int number)
{
  unsigned int pot = 1; 
  unsigned int res = 0; 
  while(number != 0) {
    res += (number % 10) * pot;
    number /= 10;
    pot <<= 4;
  }
  return res;
}

static void time_add(dvd_time_t *acc, dvd_time_t *diff)
{
  unsigned int acc_s, diff_s;
  int frame_rate, frames;
  
  assert((acc->frame_u & 0xc0) == (diff->frame_u & 0xc0));
  // conver from bcd to seconds
  acc_s = bcd2int(acc->hour) * 60 * 60;
  acc_s += bcd2int(acc->minute) * 60;
  acc_s += bcd2int(acc->second);
  
  diff_s = bcd2int(diff->hour) * 60 * 60;
  diff_s += bcd2int(diff->minute) * 60;
  diff_s += bcd2int(diff->second);
  
  // add time
  acc_s += diff_s;
  
  frames = bcd2int(acc->frame_u & 0x3f) + bcd2int(diff->frame_u & 0x3f);

  switch(acc->frame_u >> 6) {
  case 1:
    frame_rate = 25;
    break;
  case 3:
    frame_rate = 30; // 29.97
    break;
  default:
    frame_rate = 30; // ??
    break;
  }
   
  // normalize time
  acc_s += frames / frame_rate;
  // convert back to bcd
  acc->frame_u = int2bcd(frames % frame_rate) | (acc->frame_u & 0xc0);
  acc->second = int2bcd(acc_s % 60);
  acc->minute = int2bcd((acc_s / 60) % 60);
  acc->hour   = int2bcd(acc_s / (60 * 60));
}

void vm_get_total_time(dvd_time_t *current_time)
{
  *current_time = state.pgc->playback_time;
  /* This should give the same time as well.... */
  //vm_get_cell_stat_time(current_time, state.pgc->nr_of_cells);
}

void vm_get_current_time(dvd_time_t *current_time, pci_t *pci)
{
  vm_get_cell_stat_time(current_time, state.cellN);
  /* Add the time within the cell. */
  time_add(current_time, &(pci->pci_gi.e_eltm));
}

void vm_get_cell_stat_time(dvd_time_t *current_time, int cellN)
{
  dvd_time_t angle_time;
  playback_type_t *pb_ty;
  int i;
  
  current_time->hour = 0;
  current_time->minute = 0;
  current_time->second = 0;
  /* Frame code */
  current_time->frame_u = state.pgc->playback_time.frame_u & 0xc0;
  
  
  /* Should only be called for One_Sequential_PGC_Title PGCs. */
  if(state.domain != VTS_DOMAIN) {
    /* No time info */
    return;
  }
  pb_ty = &vmgi->tt_srpt->title[state.TTN_REG - 1].pb_ty;
  if(pb_ty->multi_or_random_pgc_title != /* One_Sequential_PGC_Title */ 0) {
    /*No time info */
    return;
  }
  
  assert(cellN <= state.pgc->nr_of_cells);
    
  for(i = 1; i < cellN; i++) {
    
    /* Multi angle/Interleaved */
    switch(state.pgc->cell_playback[i - 1].block_mode) {
    case 0: // Normal
      assert(state.pgc->cell_playback[i - 1].block_type == 0);
      time_add(current_time, &state.pgc->cell_playback[i - 1].playback_time);
      break;
    case 1: // The first cell in the block
      switch(state.pgc->cell_playback[i - 1].block_type) {
      case 0: // Not part of a block
	assert(0);
      case 1: // Angle block
	time_add(current_time, &state.pgc->cell_playback[i - 1].playback_time);
	angle_time = state.pgc->cell_playback[i - 1].playback_time;
	break;
      case 2: // ??
      case 3: // ??
      default:
	WARNING("Invalid? Cell block_mode (%d), block_type (%d)\n",
		state.pgc->cell_playback[i - 1].block_mode,
		state.pgc->cell_playback[i - 1].block_type);
      }
      break;
    case 2: // Cell in the block
    case 3: // Last cell in the block
      /* Check that the cells for each angle have equal duration. */
      assert(!memcmp(&angle_time, 
		     &state.pgc->cell_playback[i - 1].playback_time, 
		     sizeof(dvd_time_t)));
      break;
    default:
      WARNING("Cell is in block but did not enter at first cell!\n");
    }
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
static int update_PGN(void) {
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
  
  if(state.domain != FP_DOMAIN)
    DNOTE("play_PGC: state.pgcN (%i)\n", get_PGCN());
  else
    DNOTE("play_PGC: first_play_pgc\n");

  // These should have been set before play_PGC is called.
  // They won't be set automaticaly and the when the pre-commands are
  // executed they might be used (for instance in a CallSS that will 
  // save resume state)
  // state.pgN, state.cellN
  state.cellN = 0; // FIXME set cellN everytime pgN is set! ?!

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
      DNOTE("PGC pre commands didn't do a Jump, Link or Call\n");
    }
  }
  return play_PG();
}  


static link_t play_PG(void)
{
  DNOTE("play_PG: state.pgN (%i)\n", state.pgN);
  
  assert(state.pgN > 0);
  if(state.pgN > state.pgc->nr_of_programs) {
    DNOTE("state.pgN (%i) == pgc->nr_of_programs + 1 (%i)\n", 
	  state.pgN, state.pgc->nr_of_programs + 1);
    assert(state.pgN == state.pgc->nr_of_programs + 1);
    return play_PGC_post();
  }
  
  state.cellN = state.pgc->program_map[state.pgN - 1];
  
  return play_Cell();
}


static link_t play_Cell(void)
{
  DNOTE("play_Cell: state.cellN (%i)\n", state.cellN);
  
  assert(state.cellN > 0);
  if(state.cellN > state.pgc->nr_of_cells) {
    DNOTE("state.cellN (%i) == pgc->nr_of_cells + 1 (%i)\n", 
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
      WARNING("Invalid? Cell block_mode (%d), block_type (%d)\n",
	      state.pgc->cell_playback[state.cellN - 1].block_mode,
	      state.pgc->cell_playback[state.cellN - 1].block_type);
    }
    break;
  case 2: // Cell in the block
  case 3: // Last cell in the block
  // These might perhaps happen for RSM or LinkC commands?
  default:
    WARNING("Cell is in block but did not enter at first cell!\n");
  }
  
  /* Updates state.pgN and PTTN_REG */
  if(update_PGN()) {
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
  
  DNOTE("play_Cell_post: state.cellN (%i)\n", state.cellN);
  
  cell = &state.pgc->cell_playback[state.cellN - 1];
  
  /* Still time is already taken care of before we get called. */
  
  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;
    
    assert(state.pgc->command_tbl != NULL);
    assert(state.pgc->command_tbl->nr_of_cell >= cell->cell_cmd_nr);
    DNOTE("Cell command pressent, executing\n");
    if(vmEval_CMD(&state.pgc->command_tbl->cell_cmds[cell->cell_cmd_nr - 1], 1,
		  &state.registers, &link_values)) {
      return link_values;
    } else {
       DNOTE("Cell command didn't do a Jump, Link or Call\n");
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
      WARNING("Invalid? Cell block_mode (%d), block_type (%d)\n",
	      state.pgc->cell_playback[state.cellN - 1].block_mode,
	      state.pgc->cell_playback[state.cellN - 1].block_type);
    }
    break;
  }
  
  
  /* Figure out the correct pgN for the new cell */ 
  if(update_PGN()) {
    DNOTE("last cell in this PGC\n");
    return play_PGC_post();
  }

  return play_Cell();
}


static link_t play_PGC_post(void)
{
  link_t link_values;

  DNOTE("play_PGC_post:\n");
  
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
    DNOTE("** Fell of the end of the pgc, continuing in NextPGC\n");
    if(state.domain == FP_DOMAIN) {
      /* User should select a title them self, i.e. we should probably go 
	 to the STOP_DOMAIN.  Untill we have that implemented start playing
	 title track 1 instead since we're sure that that isn't optional. */
      if(get_TT(1) == -1)
	assert(0);
      /* This won't create an infinite loop becase we can't ever get to the
	 FP_DOMAIN again without executing a command. */
      return play_PGC();
    }
    assert(state.pgc->next_pgc_nr != 0);
    /* Should end up in the STOP_DOMAIN if next_pgc i 0. */
    return link_next_pgc;
  }
}


static link_t process_command(link_t link_values)
{
  link_t do_nothing = { PlayThis, state.blockN, 0, 0};
  
  /* FIXME $$$ Move this to a separate function? */
  while(link_values.command != PlayThis) {
    
#ifdef TRACE
    vmPrint_LINK(link_values);
#endif
    
    switch(link_values.command) {
    case LinkNoLink:
      exit(1);
      
    case LinkTopC:
      link_values = play_Cell();
      break;
    case LinkNextC:
      // What if cellN becomes > nr_of_cells?
      if(state.cellN == state.pgc->nr_of_cells)
	return do_nothing; // it should do nothing
      state.cellN += 1;
      link_values = play_Cell();
      break;
    case LinkPrevC:
      // What if cellN becomes < 1?
      if(state.cellN == 0)
	return do_nothing; // it should do nothing
      state.cellN -= 1;
      link_values = play_Cell();
      break;
      
    case LinkTopPG:
      // Does pgN always contain the current value?
      link_values = play_PG();
      break;
    case LinkNextPG:
      // Does pgN always contain the current value?
      if(state.pgN == state.pgc->nr_of_programs) {
	if(get_PGC(state.pgc->prev_pgc_nr))
	  return do_nothing; // it must do nothing, do not exit...
	link_values = play_PGC();
      } else {                                                                
	state.pgN += 1;
	link_values = play_PG();
      }                                                                       
      break;
    case LinkPrevPG:
      // Does pgN always contain the current value?
      if(state.pgN == 1) {
	if(get_PGC(state.pgc->prev_pgc_nr))
	  return do_nothing; // it must do nothing, do not exit...
	link_values = play_PGC();
      } else {                                                                
	state.pgN -= 1;
	link_values = play_PG();
      }                                                                       
      break;
      
    case LinkTopPGC:
      link_values = play_PGC();
      break;
    case LinkNextPGC:
      assert(state.pgc->next_pgc_nr != 0);
      if(get_PGC(state.pgc->next_pgc_nr))
	return do_nothing; // do nothing, do not exit... assert(0);
      link_values = play_PGC();
      break;
    case LinkPrevPGC:
      assert(state.pgc->prev_pgc_nr != 0);
      if(get_PGC(state.pgc->prev_pgc_nr))
	return do_nothing; // do nothing, do not exit... assert(0);
      link_values = play_PGC();
      break;
    case LinkGoUpPGC:
      assert(state.pgc->goup_pgc_nr != 0);
      if(get_PGC(state.pgc->goup_pgc_nr))
	return do_nothing; // do nothing, do not exit... assert(0);
      link_values = play_PGC();
      break;
    case LinkTailPGC:
      link_values = play_PGC_post();
      break;
      
    case LinkRSM:
      /* Updates link_values if successful */
      if(!vm_resume_int(&link_values)) {
	/* Nothing / Faild.  What should we do? Do we need closer interaction
	 * with the command evaluatore to be able to turn this in to a NOP? */
	return do_nothing;
      }
      break;
    case LinkPGCN:
      if(get_PGC(link_values.data1))
	assert(0);
      link_values = play_PGC();
      break;
    case LinkPTTN:
      assert(state.domain == VTS_DOMAIN);
      if(get_VTS_PTT(state.vtsN, state.VTS_TTN_REG, link_values.data1) == -1)
	assert(0);
      // not play_PGC(); Fixes Men In Black delux set / sepcial edition 
      link_values = play_PG();
      break;
    case LinkPGN:
      /* Update any other state, PTTN perhaps? */
      state.pgN = link_values.data1;
      link_values = play_PG();
      break;
    case LinkCN:
      /* Update any other state, pgN, PTTN perhaps? */
      state.cellN = link_values.data1;
      link_values = play_Cell();
      break;
      
    case Exit:
      exit(1); // What should we do here??
      
    case JumpTT:
      assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN);
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
      link_values = play_PGC();
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
      if(link_values.data1 == 0) {
	// 'The Fifth Element' region 2 has data1 == 0.
	assert(state.domain == VTSM_DOMAIN);
      } else if(link_values.data1 == state.vtsN) {
	// "Captain Scarlet & the Mysterons" has data1 == state.vtsN i VTSM
	assert(state.domain == VTSM_DOMAIN || 
	       state.domain == VMGM_DOMAIN || 
	       state.domain == FP_DOMAIN); //??
	state.domain = VTSM_DOMAIN;
      } else {
	// Normal case.
	assert(state.domain == VMGM_DOMAIN || 
	       state.domain == FP_DOMAIN); //??
	state.domain = VTSM_DOMAIN;
	ifoOpenNewVTSI(dvd, link_values.data1);  // Also sets state.vtsN
      }
      // I don't really know what title is supposed to be used for.
      // Alien or Aliens has this != 1, I think.
      //assert(link_values.data2 == 1);
      assert(link_values.data2 != 0);
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


static int get_TT_PTT(int tt, int ptt)
{  
  assert(tt <= vmgi->tt_srpt->nr_of_srpts);
  
  state.TTN_REG = tt;
  
  // Add this check for use call of this code.
  //assert(part <= vtsi->vts_ptt_srpt->title[vts_ttn - 1].nr_of_ptts);
  
  return get_VTS_PTT(vmgi->tt_srpt->title[tt - 1].title_set_nr,
		     vmgi->tt_srpt->title[tt - 1].vts_ttn, 
		     ptt);
}


static int get_VTS_TT(int vtsN, int vts_ttn)
{
  int pgcN;
  
  state.domain = VTS_DOMAIN;
  if(vtsN != state.vtsN)
    ifoOpenNewVTSI(dvd, vtsN); // Also sets state.vtsN
  
  pgcN = get_ID(vts_ttn); // This might return -1
  assert(pgcN != -1);
  //assert(pgcN == vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[0].pgcn); ??
  
  //state.TTN_REG = ?? Must search tt_srpt for a matching entry...  
  state.VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  return get_PGC(pgcN);
}


static int get_VTS_PTT(int vtsN, int /* is this really */ vts_ttn, int part)
{
  int pgcN, pgN, res;
  
  state.domain = VTS_DOMAIN;
  if(vtsN != state.vtsN)
    ifoOpenNewVTSI(dvd, vtsN); // Also sets state.vtsN
  
  assert(vts_ttn <= vtsi->vts_ptt_srpt->nr_of_srpts);
  assert(part <= vtsi->vts_ptt_srpt->title[vts_ttn - 1].nr_of_ptts);
  
  pgcN = vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgcn;
  pgN = vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgn;
  
  //assert(pgcN == get_ID(vts_ttn); ??
  
  //state.TTN_REG = ?? Must search tt_srpt for a matchhing entry...
  state.VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  res = get_PGC(pgcN); // This clobbers state.pgN (sets it to 1).
  state.pgN = pgN;
  return res;
}



static int get_FP_PGC(void)
{  
  state.domain = FP_DOMAIN;
  
  /* 'DVD Authoring & Production' claim that first_play_pgc is optional. */
  if(vmgi->first_play_pgc) {
    state.pgc = vmgi->first_play_pgc;
    state.pgN = 1;
  } else {
    if(get_TT(1) == -1)
      assert(0);
  }
  return 0;
}


static int get_MENU(int menu)
{
  assert(state.domain == VMGM_DOMAIN || state.domain == VTSM_DOMAIN);
  return get_PGC(get_ID(menu));
}


/***
 * Search PGCIT for a PGC that has entry_id == id and return the PGCN of that.
 */
static int get_ID(int id)
{
  int pgcN, i;
  pgcit_t *pgcit;
  
  /* Relies on state to get the correct pgcit. */
  pgcit = get_PGCIT();
  assert(pgcit != NULL);
  
  /* Get menu/title */
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    int entry_id = pgcit->pgci_srp[i].entry_id & 0x7F;
    int entryPGC = pgcit->pgci_srp[i].entry_id & 0x80;
    // Only 'enter' the one that has the entry flag set and is a match.
    if(entryPGC && (entry_id == id)) {
      pgcN = i + 1;
      return pgcN;
    }
  }
  DNOTE("** No such id/menu (%d) entry PGC\n", id);
  return -1; // error
}


/***
 * Update the state so that pgcN is the current PGC.  Sets PGN to 1 and
 * if we are in VTS_DOMAIN (really TT_DOM) updates the TT_PGC_REG.
 */
static int get_PGC(int pgcN)
{
  pgcit_t *pgcit;
  
  pgcit = get_PGCIT();
  
  assert(pgcit != NULL); // ?? Make this return -1 instead
  if(pgcN < 1 || pgcN > pgcit->nr_of_pgci_srp)
    return -1; // error
  
  //state.pgcN = pgcN;
  state.pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  state.pgN = 1;
  
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
    FATAL("ifoOpenVTSI failed\n");
    exit(1);
  }
  if(!ifoRead_VTS_PTT_SRPT(vtsi)) {
    FATAL("ifoRead_VTS_PTT_SRPT failed\n");
    exit(1);
  }
  if(!ifoRead_PGCIT(vtsi)) {
    FATAL("ifoRead_PGCIT failed\n");
    exit(1);
  }
  if(!ifoRead_PGCI_UT(vtsi)) {
    FATAL("ifoRead_PGCI_UT failed\n");
    exit(1);
  }
  state.vtsN = vtsN;
}




static pgcit_t* get_MENU_PGCIT(ifo_handle_t *h, uint16_t lang)
{
  int i;
  
  if(h == NULL || h->pgci_ut == NULL) {
    WARNING("*** pgci_ut handle is NULL ***\n");
    return NULL; // error?
  }
  
  i = 0;
  while(i < h->pgci_ut->nr_of_lus
	&& h->pgci_ut->lu[i].lang_code != lang)
    i++;
  if(i == h->pgci_ut->nr_of_lus) {
    NOTE("Language '%c%c' not found, using '%c%c' instead\n",
	 (char)(lang >> 8), (char)(lang & 0xff),
	 (char)(h->pgci_ut->lu[0].lang_code >> 8),
	 (char)(h->pgci_ut->lu[0].lang_code & 0xff));
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
