/* 
 * Copyright (C) 2000 Björn Englund <d4bjorn@dtek.chalmers.se>, 
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
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
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include <assert.h>

#include "ifo.h"
#include "ifo_print.h"
#include "vmcmd.h"


#define PUT(level, text...) \
do { \
  if(level < debug) { \
    fprintf(stdout, ## text); \
  } \
} while(0)

extern int debug;



void ifoPrint_time(int level, dvd_time_t *time) {
  char *rate = NULL;
  assert((time->hour >> 4) <= 9 && (time->hour & 0xf) <= 9);
  assert((time->minute >> 4) <= 6 && (time->minute & 0xf) <= 9);
  assert((time->second >> 4) <= 6 && (time->second & 0xf) <= 9);
  assert((time->frame_u & 0xf) <= 9);
  
  PUT(level, "%02x:%02x:%02x.%02x", 
      time->hour,
      time->minute,
      time->second,
      time->frame_u & 0x3f);
  switch((time->frame_u >> 6) & 0x03) {
  case 0: 
    rate = "(please send a bug report)";
    break;
  case 1:
    rate = "25.00";
    break;
  case 2:
    rate = "(please send a bug report)";
    break;
  case 3:
    rate = "29.97";
    break;
  } 
  PUT(level, " @ %s fps", rate);
}

void ifoPrint_video_attributes(int level, video_attr_t *attr) {
  
  /* The following test is shorter bu not correct ISO C,
     memcmp(attr,my_friendly_zeros, sizeof(video_attr_t)) */
  if(attr->mpeg_version == 0 
     && attr->video_format == 0 
     && attr->display_aspect_ratio == 0 
     && attr->permitted_df == 0 
     && attr->unknown1 == 0 
     && attr->line21_CC_1 == 0 
     && attr->line21_CC_2 == 0 
     && attr->video_format == 0 
     && attr->letterboxed == 0 
     && attr->film_mode == 0) {
    PUT(level,"-- Unspecified --");
    return;
  }
  
  switch(attr->mpeg_version) {
  case 0:
    PUT(level,"mpeg1 ");
    break;
  case 1:
    PUT(level,"mpeg2 ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  switch(attr->video_format) {
  case 0:
    PUT(level,"ntsc ");
    break;
  case 1:
    PUT(level,"pal ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  switch(attr->display_aspect_ratio) {
  case 0:
    PUT(level,"4:3 ");
    break;
  case 3:
    PUT(level,"16:9 ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  switch(attr->permitted_df) {
  case 0:
    PUT(level,"pan&scan+letterboxed ");
    break;
  case 1:
    PUT(level,"only pan&scan "); //??
    break;
  case 2:
    PUT(level,"only letterboxed ");
    break;
  case 3:
    // not specified
    break;
  default:
    PUT(0, "(please send a bug report)");
  }
  
  PUT(level, "U%x ", attr->unknown1);
  assert(!attr->unknown1);
  
  if(attr->line21_CC_1 || attr->line21_CC_2) {
    PUT(level, "CC line21 ");
    if(attr->line21_CC_1)
      PUT(level, "1 ");
     if(attr->line21_CC_2)
      PUT(level, "2 ");
  }
  
  {
    int height = 480;
    if(attr->video_format != 0) 
      height = 576;
    switch(attr->picture_size) {
    case 0:
      PUT(level, "720x%d ", height);
      break;
    case 1:
      PUT(level, "704x%d ", height);
      break;
    case 2:
      PUT(level, "352x%d ", height);
      break;
    case 3:
      PUT(level, "352x%d ", height/2);
      break;      
    default:
      PUT(0, "(please send a bug report) ");
    }
  }

  if(attr->letterboxed)
    PUT(level, "letterboxed ");
  
  if(attr->film_mode)
    PUT(level, "film");
  else
    PUT(level, "video"); //camera
}

void ifoPrint_audio_attributes(int level, audio_attr_t *attr) {
  
  if(attr->audio_format == 0
     && attr->multichannel_extension == 0
     && attr->lang_type == 0
     && attr->application_mode == 0
     && attr->quantization == 0
     && attr->sample_frequency == 0
     && attr->channels == 0
     && attr->audio_type == 0
     && attr->unknown1 == 0
     && attr->unknown1 == 0) {
    PUT(level,"-- Unspecified --");
    return;
  }
  
  switch(attr->audio_format) {
  case 0:
    PUT(level, "ac3 ");
    break;
  case 1:
    PUT(0, "(please send a bug report) ");
    break;
  case 2:
    PUT(level, "mpeg1 ");
    break;
  case 3:
    PUT(level, "mpeg2ext ");
    break;
  case 4:
    PUT(level, "lpcm ");
    break;
  case 5:
    PUT(0, "(please send a bug report) ");
    break;
  case 6:
    PUT(level, "dts ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  if(attr->multichannel_extension)
    PUT(level, "multichannel_extension ");
  
  switch(attr->lang_type) {
  case 0:
    // not specified
    assert(attr->lang_code[0] == 0 && attr->lang_code[1] == 0);
    break;
  case 1:
    PUT(level, "%c%c ", attr->lang_code[0], attr->lang_code[1]);
    break;
  default:
    PUT(level, "(please send a bug report) ");
  }

  switch(attr->application_mode) {
  case 0:
    // not specified
    break;
  case 1:
    PUT(level, "karaoke mode ");
    break;
  case 2:
    PUT(level, "surround sound mode ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  switch(attr->quantization) {
  case 0:
    PUT(level, "16bit ");
    break;
  case 1:
    PUT(level, "20bit ");
    break;
  case 2:
    PUT(level, "24bit ");
    break;
  case 3:
    PUT(level, "drc ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  switch(attr->sample_frequency) {
  case 0:
    PUT(level, "48kHz ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }
  
  PUT(level, "%dCh ", attr->channels + 1);
  
  switch(attr->audio_type) {
  case 0:
    PUT(level, "Not specified ");
    break;
  case 1: // Normal audio
    PUT(level, "Normal Caption ");
    break;
  case 2: // visually imparied
    PUT(level, "Audio for visually impaired ");
    break;
  case 3: // Directors 1
    PUT(level, "Director's comments 1 ");
    break;
  case 4: // directors 2
    PUT(level, "Director's comments 2 ");
    break;
    //case 4: // Music score ?    
  default:
    PUT(0, "(please send a bug report) ");
  }
    
  PUT(level, "%d ", attr->unknown1);
  PUT(level, "%d ", attr->unknown2);
}

void ifoPrint_subp_attributes(int level, subp_attr_t *attr) {
  
  if(attr->user_selectable == 0
     && attr->lang_code[0] == 0
     && attr->lang_code[1] == 0
     && attr->zero1 == 0
     && attr->zero2 == 0
     && attr->lang_extension== 0) {
    PUT(level,"-- Unspecified --");
    return;
  }
  
  if(attr->user_selectable & 0x80)
    PUT(level, "user_selectable ");
  if(isalpha((int)attr->lang_code[0]) && isalpha((int)attr->lang_code[1]))
    PUT(level, "%c%c ", attr->lang_code[0], attr->lang_code[1]);
  else
    PUT(level, "%02x%02x ", attr->lang_code[0], attr->lang_code[1]);
  
  PUT(level, "%d ", attr->zero1);
  PUT(level, "%d ", attr->zero2);

  switch(attr->lang_extension) {
  case 0:
    PUT(level, "Not specified ");
    break;
  case 1:
    PUT(level, "Caption with normal size character ");
    break;
  case 2:
    PUT(level, "Caption with bigger size character ");
    break;
  case 3: 
    PUT(level, "Caption for children ");
    break;
  case 4:
    PUT(level, "reserved ");
    break;
  case 5:
    PUT(level, "Closed Caption with normal size character ");
    break;
  case 6:
    PUT(level, "Closed Caption with bigger size character ");
    break;
  case 7:
    PUT(level, "Closed Caption for children ");
    break;
  case 8:
    PUT(level, "reserved ");
    break;
  case 9:
    PUT(level, "Forced Caption");
    break;
  case 10:
    PUT(level, "reserved ");
    break;
  case 11:
    PUT(level, "reserved ");
    break;
  case 12:
    PUT(level, "reserved ");
    break;
  case 13:
    PUT(level, "Director's comments with normal size character ");
    break;
  case 14:
    PUT(level, "Director's comments with bigger size character ");
    break;
  case 15:
    PUT(level, "Director's comments for children ");
    break;
  default:
    PUT(0, "(please send a bug report) ");
  }

}

void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat) {
  int i;
  
  PUT(5, "VMG Identifier: %.12s\n", vmgi_mat->vmg_identifier);
  PUT(5, "Last Sector of VMG: %08x\n", vmgi_mat->vmg_last_sector);
  PUT(5, "Last Sector of VMGI: %08x\n", vmgi_mat->vmgi_last_sector);
  PUT(5, "Specification version number: %01x.%01x\n", 
      vmgi_mat->specification_version>>4, vmgi_mat->specification_version&0xf);
  /* Byte 2 of 'VMG Category' (00xx0000) is the Region Code */
  PUT(5, "VMG Category: %08x\n", vmgi_mat->vmg_category);
  PUT(5, "VMG Number of Volumes: %i\n", vmgi_mat->vmg_nr_of_volumes);
  PUT(5, "VMG This Volume: %i\n", vmgi_mat->vmg_this_volume_nr);
  PUT(5, "Disc side %i\n", vmgi_mat->disc_side);
  PUT(5, "VMG Number of Title Sets %i\n", vmgi_mat->vmg_nr_of_title_sets);
  PUT(5, "Provider ID: %.32s\n", vmgi_mat->provider_identifier);
  PUT(5, "VMG POS Code: %032llx\n", vmgi_mat->vmg_pos_code);
  PUT(5, "End byte of VMGI_MAT: %08x\n", vmgi_mat->vmgi_last_byte);
  PUT(5, "Start byte of First Play PGC FP PGC: %08x\n", 
      vmgi_mat->first_play_pgc);
  PUT(5, "Start sector of VMGM_VOBS: %08x\n", vmgi_mat->vmgm_vobs);
  PUT(5, "Start sector of VMG_PTT_SRPT: %08x\n", vmgi_mat->vmg_ptt_srpt);
  PUT(5, "Start sector of VMGM_PGCI_UT: %08x\n", vmgi_mat->vmgm_pgci_ut);
  PUT(5, "Start sector of VMG_PTL_MAIT: %08x\n", vmgi_mat->vmg_ptl_mait);
  PUT(5, "Start sector of VMG_VTS_ATRT: %08x\n", vmgi_mat->vmg_vts_atrt);
  PUT(5, "Start sector of VMG_TXTDT_MG: %08x\n", vmgi_mat->vmg_txtdt_mgi);
  PUT(5, "Start sector of VMGM_C_ADT: %08x\n", vmgi_mat->vmgm_c_adt);
  PUT(5, "Start sector of VMGM_VOBU_ADMAP: %08x\n", 
      vmgi_mat->vmgm_vobu_admap);
  PUT(5, "Video attributes of VMGM_VOBS: ");
  ifoPrint_video_attributes(5, &vmgi_mat->vmgm_video_attributes);
  PUT(5, "\n");
  PUT(5, "VMGM Number of Audio attributes: %i\n", 
      vmgi_mat->nr_of_vmgm_audio_streams);
  for(i = 0; i < vmgi_mat->nr_of_vmgm_audio_streams; i++) {
    PUT(5, "\tAudio stream %i status: ", i+1);
    ifoPrint_audio_attributes(5, &vmgi_mat->vmgm_audio_attributes[i]);
    PUT(5, "\n");
  }
  PUT(5, "VMGM Number of Sub-picture attributes: %i\n", 
      vmgi_mat->nr_of_vmgm_subp_streams);
  for(i = 0; i < vmgi_mat->nr_of_vmgm_subp_streams; i++) {
    PUT(5, "\tSub-picture stream %2i status: ", i+1);
    ifoPrint_subp_attributes(5, &vmgi_mat->vmgm_subp_attributes[i]);
    PUT(5, "\n");
  }
}


void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat) {
  int i;

  PUT(5, "VTS Identifier: %.12s\n", vtsi_mat->vts_identifier);
  PUT(5, "Last Sector of VTS: %08x\n", vtsi_mat->vts_last_sector);
  PUT(5, "Last Sector of VTSI: %08x\n", vtsi_mat->vtsi_last_sector);
  PUT(5, "Specification version number: %01x.%01x\n", 
      vtsi_mat->specification_version>>4, vtsi_mat->specification_version&0xf);
  PUT(5, "VTS Category: %08x\n", vtsi_mat->vts_category);
  PUT(5, "End byte of VTSI_MAT: %08x\n", vtsi_mat->vtsi_last_byte);
  PUT(5, "Start sector of VTSM_VOBS:  %08x\n", vtsi_mat->vtsm_vobs);
  PUT(5, "Start sector of VTSTT_VOBS: %08x\n", vtsi_mat->vtstt_vobs);
  PUT(5, "Start sector of VTS_PTT_SRPT: %08x\n", vtsi_mat->vts_ptt_srpt);
  PUT(5, "Start sector of VTS_PGCIT:    %08x\n", vtsi_mat->vts_pgcit);
  PUT(5, "Start sector of VTSM_PGCI_UT: %08x\n", vtsi_mat->vtsm_pgci_ut);
  PUT(5, "Start sector of VTS_TMAPT:    %08x\n", vtsi_mat->vts_tmapt);
  PUT(5, "Start sector of VTSM_C_ADT:      %08x\n", vtsi_mat->vtsm_c_adt);
  PUT(5, "Start sector of VTSM_VOBU_ADMAP: %08x\n",vtsi_mat->vtsm_vobu_admap);
  PUT(5, "Start sector of VTS_C_ADT:       %08x\n", vtsi_mat->vts_c_adt);
  PUT(5, "Start sector of VTS_VOBU_ADMAP:  %08x\n", vtsi_mat->vts_vobu_admap);
  
  PUT(5, "Video attributes of VTSM_VOBS: ");
  ifoPrint_video_attributes(5, &vtsi_mat->vtsm_video_attributes);
  PUT(5, "\n");
  
  PUT(5, "VTSM Number of Audio attributes: %i\n", 
      vtsi_mat->nr_of_vtsm_audio_streams);
  for(i = 0; i < vtsi_mat->nr_of_vtsm_audio_streams; i++) {
    PUT(5, "\tAudio stream %i status: ", i+1);
    ifoPrint_audio_attributes(5, &vtsi_mat->vtsm_audio_attributes[i]);
    PUT(5, "\n");
  }
  
  PUT(5, "VTSM Number of Sub-picture attributes: %i\n", 
      vtsi_mat->nr_of_vtsm_subp_streams);
  for(i = 0; i < vtsi_mat->nr_of_vtsm_subp_streams; i++) {
    PUT(5, "\tSub-picture stream %2i status: ", i+1);
    ifoPrint_subp_attributes(5, &vtsi_mat->vtsm_subp_attributes[i]);
    PUT(5, "\n");
  }
  
  PUT(5, "Video attributes of VTS_VOBS: ");
  ifoPrint_video_attributes(5, &vtsi_mat->vts_video_attributes);
  PUT(5, "\n");
  
  PUT(5, "VTS Number of Audio attributes: %i\n", 
      vtsi_mat->nr_of_vts_audio_streams);
  for(i = 0; i < vtsi_mat->nr_of_vts_audio_streams; i++) {
    PUT(5, "\tAudio stream %i status: ", i+1);
    ifoPrint_audio_attributes(5, &vtsi_mat->vts_audio_attributes[i]);
    PUT(5, "\n");
  }
  
  PUT(5, "VTS Number of Subpicture attributes: %i\n", 
      vtsi_mat->nr_of_vts_subp_streams);
  for(i = 0; i < vtsi_mat->nr_of_vts_subp_streams; i++) {
    PUT(5, "\tSub-picture stream %2i status: ", i+1);
    ifoPrint_subp_attributes(5, &vtsi_mat->vts_subp_attributes[i]);
    PUT(5, "\n");
  }
}




typedef struct
{
  uint32_t bit_position;
  uint8_t bytes [2048];
} buffer_t;

void print_bits(int num, uint8_t bits) {
  int i;
  for(i = 0; i < num; i++)
    putchar('0' + (bits >> (num-i-1) & 1));
}

void ifoPrint_COMMAND(int row, vm_cmd_t *command) {
  int i;

  printf("(%03d) ", row + 1);
  for(i=0;i<8;i++)
    printf("%02x ", command->bytes[i]);
  printf("| ");

  vmcmd(&command->bytes[0]);
  printf("\n");
}




void ifoPrint_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl) {
  int i;
  
  if(cmd_tbl == NULL) {
    PUT(5, "No Command table present\n");
    return;
  }
  
  PUT(5, "Number of Pre commands: %i\n", cmd_tbl->nr_of_pre);
  for(i = 0; i < cmd_tbl->nr_of_pre; i++) {
    ifoPrint_COMMAND(i, &cmd_tbl->pre_commands[i]);
  }

  PUT(5, "Number of Post commands: %i\n", cmd_tbl->nr_of_post);
  for(i = 0; i < cmd_tbl->nr_of_post; i++) {
      ifoPrint_COMMAND(i, &cmd_tbl->post_commands[i]);
  }

  PUT(5, "Number of Cell commands: %i\n", cmd_tbl->nr_of_cell);
  for(i = 0; i < cmd_tbl->nr_of_cell; i++) {
    ifoPrint_COMMAND(i, &cmd_tbl->cell_commands[i]);
  }
}

void ifoPrint_PGC_PROGRAM_MAP(pgc_program_map_t *program_map, int nr) {
  int i;
  
  if(program_map == NULL) {
    PUT(5, "No Program map present\n");
    return;
  }
  
  for(i = 0; i < nr; i++) {
    PUT(5, "Program %3i Entry Cell: %3i\n", i+1, program_map[i]);
  }
}

void ifoPrint_CELL_PLAYBACK_TBL(cell_playback_tbl_t *cell_playback, int nr) {
  int i;
  if(cell_playback == NULL) {
    PUT(5, "No Cell Playback info present\n");
    return;
  }
  for(i=0;i<nr;i++) {
    /* lowest bit indicating cell command, lowest ?byte? cell command nr? */ 
    /* 0000xx00 might be still, play and then pause or repeat count */
    PUT(5, "Cell: %3i category %08x ", i+1, cell_playback[i].category);
    ifoPrint_time(5, &cell_playback[i].playback_time); PUT(5,"\n");
    PUT(5, "\tStart sector: %08x\tFirst ILVU end  sector: %08x\n", 
	cell_playback[i].first_sector, 
	cell_playback[i].first_ilvu_end_sector);
    PUT(5, "\tEnd   sector: %08x\tLast VOBU start sector: %08x\n", 
	cell_playback[i].last_sector, 
	cell_playback[i].last_vobu_start_sector);
  }
}

void ifoPrint_CELL_POSITION_TBL(cell_position_tbl_t *cell_position, int nr) {
  int i;
  if(cell_position == NULL) {
    PUT(5, "No Cell Position info present\n");
    return;
  }
  for(i=0;i<nr;i++) {
    PUT(5, "Cell: %3i has VOB ID: %3i, Cell ID: %3i\n", i+1, 
	cell_position[i].vob_id_nr, cell_position[i].cell_nr);
  }
}


void ifoPrint_PGC(pgc_t *pgc) {
  int i;
  
  PUT(5, "Number of Programs: %i\n", pgc->nr_of_programs);
  PUT(5, "Number of Cells: %i\n", pgc->nr_of_cells);
  /* Check that time is 0:0:0:0 also if nr_of_programs==0 */
  PUT(5, "Playback time: ");
  ifoPrint_time(5, &pgc->playback_time); PUT(5, "\n");
  /* If no programs/no time then does this mean anything? */
  PUT(5, "Prohibited user operations: %08x\n", pgc->prohibited_ops);
  
  for(i = 0; i < 8; i++) {
    if(pgc->audio_status[i] & 0x8000) { /* The 'is present' bit */
      PUT(5, "Audio stream %i status: %04x\n", i+1, pgc->audio_status[i]);
    }
  }
  
  for(i = 0; i < 32; i++) {
    if(pgc->subp_status[i] & 0x80000000) { /* The 'is present' bit */
      PUT(5, "Subpicture stream %2i status: %08x\n", i+1, pgc->subp_status[i]);
    }
  }
  
  PUT(5, "Next PGC number: %i\n", pgc->next_pgc_nr);
  PUT(5, "Prev PGC number: %i\n", pgc->prev_pgc_nr);
  PUT(5, "GoUp PGC number: %i\n", pgc->goup_pgc_nr);
  if(pgc->nr_of_programs != 0) {
    PUT(5, "Still time: %i seconds (255=inf)\n", pgc->still_time);
    PUT(5, "PG Playback mode %02x\n", pgc->pg_playback_mode);
  }
  
  if(pgc->nr_of_programs != 0) {
    for(i = 0; i < 16; i++) {
      PUT(6, "Color %2i: %08x\n", i, pgc->palette[i]);
    }
  }
  
  /* Memmory offsets to div. tables. */
  ifoPrint_PGC_COMMAND_TBL(pgc->pgc_command_tbl);
  ifoPrint_PGC_PROGRAM_MAP(pgc->pgc_program_map, pgc->nr_of_programs);
  ifoPrint_CELL_PLAYBACK_TBL(pgc->cell_playback_tbl, pgc->nr_of_cells);
  ifoPrint_CELL_POSITION_TBL(pgc->cell_position_tbl, pgc->nr_of_cells);
}



void ifoPrint_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt) {
  int i;
  
  PUT(5, "Number of PartofTitle search pointers: %i\n",
      vmg_ptt_srpt->nr_of_srpts);
  for(i=0;i<vmg_ptt_srpt->nr_of_srpts;i++) {
    PUT(5, "VMG Title set index %i\n", i+1);
    PUT(5, "\tTitle set number (VTS): %i", 
	vmg_ptt_srpt->title_info[i].title_set_nr);
    PUT(5, "\tVTS_TTN: %i\n", vmg_ptt_srpt->title_info[i].vts_ttn);
    PUT(5, "\tNumber of PTTs: %i\n", vmg_ptt_srpt->title_info[i].nr_of_ptts);
    PUT(5, "\tNumber of angles: %i\n", 
	vmg_ptt_srpt->title_info[i].nr_of_angles);
    PUT(5, "\tTitle playback type: %02x\n", 
	vmg_ptt_srpt->title_info[i].playback_type);
    PUT(5, "\tParental ID field: %04x\n", /* What does this mean? */
	vmg_ptt_srpt->title_info[i].parental_id);
    PUT(5, "\tTitle set starting sector %08x\n", 
	vmg_ptt_srpt->title_info[i].title_set_sector);
  }
}

void ifoPrint_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt) {
  int i, j;
  PUT(5, " nr_of_srpts %i last byte %i\n", 
      vts_ptt_srpt->nr_of_srpts, 
      vts_ptt_srpt->last_byte);
  for(i=0;i<vts_ptt_srpt->nr_of_srpts;i++) {
    for(j=0;j<vts_ptt_srpt->title_info[i].nr_of_ptts;j++) {
      PUT(5, "VTS_PTT_SRPT - Title %3i part %3i: PGC: %3i PG: %3i\n",
	  i+1, j+1, 
	  vts_ptt_srpt->title_info[i].ptt_info[j].pgcn,
	  vts_ptt_srpt->title_info[i].ptt_info[j].pgn );
    }
  }
}


static void hexdump(uint8_t *ptr, int len) {
  while(len--)
    PUT(5, "%02x ", *ptr++);
}

void ifoPrint_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait) {
  int i, j;
  
  PUT(5, "Number of Countries: %i\n", ptl_mait->nr_of_countries);
  PUT(5, "Number of VTSs: %i\n", ptl_mait->nr_of_vtss);
  //PUT(5, "Last byte: %i\n", ptl_mait->last_byte);
  
  for(i = 0; i < ptl_mait->nr_of_countries; i++) {
    PUT(5, "Country code: %.2s\n", ptl_mait->countries[i].country_code);
    /*
      PUT(5, "Start byte: %04x %i\n", 
      ptl_mait->countries[i].pf_ptl_mai_start_byte, 
      ptl_mait->countries[i].pf_ptl_mai_start_byte);
    */
    /* This seems to be pointing at a array with 8 2byte fields per VTS
       ? and one extra for the menu? always an odd number of VTSs on
       all the dics I tested so it might be padding to even also.
       If it is for the menu it probably the first entry.  */
    for(j=0;j<8;j++) {
      hexdump( (uint8_t *)ptl_mait->countries - VMG_PTL_MAIT_COUNTRY_SIZE 
	       + ptl_mait->countries[i].pf_ptl_mai_start_byte
	       + j*(ptl_mait->nr_of_vtss+1)*2, (ptl_mait->nr_of_vtss+1)*2);
      PUT(5, "\n");
    }
  }
}

void ifoPrint_C_ADT(c_adt_t *c_adt) {
  int i, entries;
  
  PUT(5, "Number of VOBs in this VOBS: %i\n", c_adt->nr_of_vobs);
  entries = (c_adt->last_byte + 1 - C_ADT_SIZE)/sizeof(c_adt_t);
  
  for(i = 0; i < entries; i++) {
    PUT(7, "VOB ID: %3i, Cell ID: %3i   ", 
	c_adt->cell_adr_table[i].vob_id, c_adt->cell_adr_table[i].cell_id);
    PUT(7, "Sector (first): 0x%08x   (last): 0x%08x\n",
	c_adt->cell_adr_table[i].start_sector, 
	c_adt->cell_adr_table[i].last_sector);
  }
}

void ifoPrint_VOBU_ADMAP(vobu_admap_t *vobu_admap) {
  int i, entries;
  
  entries = (vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE)/4;
  for(i = 0; i < entries; i++) {
    PUT(9, "VOBU %5i  First sector: 0x%08x\n", i+1,
	vobu_admap->vobu_start_sectors[i]);
  }
}



void ifoPrint_PGCIT(pgcit_t *pgcit) {
  int i;
  
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    PUT(5, "\nProgram (PGC): %3i\t", i+1);
    /* low 16 == ?Parental control mask? 
       highest byte menu type? 82 == root? */
    PUT(5, "PGC Category: 0x%08x\n", pgcit->pgci_srp[i].pgc_category);
    ifoPrint_PGC(pgcit->pgci_srp[i].pgc);
  }
}

void ifoPrint_MENU_PGCI_UT(menu_pgci_ut_t *pgci_ut) {
  int i;
  
  PUT(5, "Number of Menu Language Units (PGCI_LU): %3i\n",
      pgci_ut->nr_of_lang_units);
  for(i = 0; i < pgci_ut->nr_of_lang_units; i++) {
    PUT(5, "\nMenu Language Code: %c%c\n",
	pgci_ut->menu_lu[i].lang_code[0],
	pgci_ut->menu_lu[i].lang_code[1]);
    PUT(5, "Menu Existence: %02x\n", pgci_ut->menu_lu[i].exists);
    ifoPrint_PGCIT(pgci_ut->menu_lu[i].menu_pgcit);
  }
}

void ifoPrint_VTS_ATTRIBUTES(vts_attributes_t *vts_attributes) {
  int i;
  
  PUT(5, "VTS_CAT Application type: %08x\n", vts_attributes->vts_cat);
  
  PUT(5, "Video attributes of VTSM_VOBS: ");
  ifoPrint_video_attributes(5, &vts_attributes->vtsm_vobs_attributes);
  PUT(5, "\n");
  PUT(5, "Number of Audio streams: %i\n", 
      vts_attributes->nr_of_vtsm_audio_streams);
  
  for(i = 0; i < vts_attributes->nr_of_vtsm_audio_streams; i++) {
    PUT(5, "\tAudio stream %i attributes: ", i+1);
    ifoPrint_audio_attributes(5, &vts_attributes->vtsm_audio_attributes[i]);
    PUT(5, "\n");
  }
  PUT(5, "Number of Subpicture streams: %i\n", 
      vts_attributes->nr_of_vtsm_subp_streams);
  for(i = 0; i < vts_attributes->nr_of_vtsm_subp_streams; i++) {
    PUT(5, "\tSub-picture stream %2i attributes: ", i+1);
    ifoPrint_subp_attributes(5, &vts_attributes->vtsm_subp_attributes[i]);
    PUT(5, "\n");
  }
   
  PUT(5, "Video attributes of VTSTT_VOBS: ");
  ifoPrint_video_attributes(5, &vts_attributes->vtstt_vobs_video_attributes);
  PUT(5, "\n");
  PUT(5, "Number of Audio streams: %i\n", 
      vts_attributes->nr_of_vtstt_audio_streams);
  for(i = 0; i < vts_attributes->nr_of_vtstt_audio_streams; i++) {
    PUT(5, "\tAudio stream %i attributes: ", i+1);
    ifoPrint_audio_attributes(5, &vts_attributes->vtstt_audio_attributes[i]);
    PUT(5, "\n");
  }
  
  PUT(5, "Number of Subpicture streams: %i\n", 
      vts_attributes->nr_of_vtstt_subp_streams);
  for(i = 0; i < vts_attributes->nr_of_vtstt_subp_streams; i++) {
    PUT(5, "\tSub-picture stream %2i attributes: ", i+1);    
    ifoPrint_subp_attributes(5, &vts_attributes->vtstt_subp_attributes[i]);
    PUT(5, "\n");
  }
}

void ifoPrint_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt) {
  int i;
  
  PUT(5, "Number of Video Title Sets: %3i\n", vts_atrt->nr_of_vtss);
  for(i = 0; i < vts_atrt->nr_of_vtss; i++) {
    PUT(5, "\nVideo Title Set %i\n", i+1);
    ifoPrint_VTS_ATTRIBUTES(&vts_atrt->vts_attributes[i]);
  }
}


