#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>

#include "../include/common.h"

#include "ifo.h"


#define PUT(level, text...) \
if(level < debug) { \
  fprintf(stdout, ## text); \
}

/*
  State: SPRM, GPRM, Domain, pgc, pgN, cellN, ?
*/


int debug = 8;
uint8_t my_friendly_zeros[2048];

FILE *vmg_file;
FILE *vts_file;

char *program_name;

void usage()
{
  fprintf(stderr, "Usage: %s  [-d <debug_level>] [-v <vob>] [-i <ifo>]\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int c; 
  pgc_t pgc;
  vmgi_mat_t vmgi_mat;
  vmg_ptt_srpt_t vmg_ptt_srpt;
  uint8_t *data = malloc( DVD_BLOCK_LEN );
  
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:h?")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  
  // Setup State
  
  ifoOpen_VMG(&vmgi_mat, "VIDEO_TS.IFO");
  
  ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
  //ifoPrint_PGC(&pgc);
  
  
  
 play_PGC:
  //DEBUG
  for(i=0;i<pgc->pgc_command_tbl->nr_of_pre;i++) {
    ifoPrint_COMMAND(pgc->pgc_command_tbl->pre_commands[i]);
  }
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG1 (also a kind of jump/link)
       (This is what happens if you fall of the end of the pre_commands)
     - or a error (are there more cases?) */
  if(eval(pgc->pgc_command_tbl->pre_commands, 
	  pgc->pgc_command_tbl->nr_of_pre, state.registers, &link_values)) {
    goto process_jump;
  }
  if(state.pgN == 0)
    state.pgN = 1; //??
  goto play_PG;
  
    
 play_PG:
  if(state.pgN > pgc->nr_of_programs) {
    assert(state.pgN == pgc->nr_of_programs + 1);
    goto play_PGC_post; //?
  } else {
    if(state.cellN == 0)
      state.cellN = pgc->pgc_program_map[State.pgN];
    goto play_Cell;
  }
  
 play_Cell:
  cell_playback_tbl_t cell;
  if(state.cellN > pgc->nr_of_cells) {
    assert(state.cell == pgc->nr_of_cells + 1); 
    goto play_PGC_post; 
  }
  cell = pgc->cell_playback_tbl[state.CellN];
  /* This is one work 'unit' for the video/audio decoder */
  //DEBUG
  ifoPrint_time(5, cell->playback_time);
  PUT(5,"VOB ID: %3i, Cell ID: %3i at sector: %08x - %08x\n",
      pgc->cell_position[State.CellN].vob_id_nr,
      pgc->cell_position[State.CellN].cell_nr,
      cell.first_sector, cell.last_sector);
  // How to handle buttons and other nav data?
  /* Should this return any button_cmd that executes?? (no, I think) */
  if(demux_data(cell->first_sector, cell->last_sector, 
		state.registers, &link_values)) {
    goto process_jump;
  }
  /* Still time or some thing */
  if(cell.category & 0xff00) {
    //sleep( );
  }
  /* Deal with looking up and 'evaling' the Cell cmd, if any */
  if(cell.category & 0xff != 0) {
    command_data_t *cmd 
      = pgc->pgc_command_tbl->cell_commands[cell.category&0xff];
    if(eval(cmd, 1, state.registers, &link_values)) {
      goto process_jump;
    }
  }
  /* Where to continue after playing the cell... */
  switch((cell.category & 0xff000000)>>24) {
  default:
    State.CellN++;
  }
  goto play_Cell;
  
 play_PGC_post:
  //DEBUG
  for(i=0;i<pgc->pgc_command_tbl->nr_of_pre;i++) {
    ifoPrint_COMMAND(pgc->pgc_command_tbl->pre_commands[i]);
  }
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - or a error (are there more cases?)
     - if you got to the end of the post_commands, then what ?? */
  if(eval(pgc->pgc_command_tbl->pre_commands, 
	  pgc->pgc_command_tbl->nr_of_pre, state.registers, &link_values)) {
    goto process_jump;
  }
  link_values = {LinkNextPGC, 0, 0, 0};
  goto process_jump;
  
 new_PGC:
  if(state.VTSN != 0 && !state.loaded[state.VTSN])
    ifoRead_VTSI(...??);
  if(state.domain & MENU_DOMAIN)
    if(state.domain == VMGM_DOMAIN)
      getPGC_LU(vmgm.menu_lu, lang, state.pgcN, 0);
    else
      getPGC_LU(vtsm[state.VTSN].menu_lu, lang, state.pgcN, 0);
  else
    if(state.domain == FP_DOMAIN)
      getPGC(vmgm.first_play_pgc);
    else
      getPGC_IT(state.pgcN, 0);
  goto play_PGC;
    
  
 process_jump:
  switch(link_values.command) {
  case LinkNoLink: // Vill inte ha det här här..
    break;
  case LinkTopC:
    goto play_Cell;
  case LinkNextC:
    state.cellN += 1; //?
    goto play_Cell;    
  case LinkPrevC:
    state.cellN -= 1; //?
    goto play_Cell;    
  case LinkTopPG:
    goto play_PG;
  case LinkNextPG:
    state.PGN += 1; //?
    goto play_PG;
  case LinkPrevPG:
    state.PGN -= 1; //?
    goto play_PG;
  case LinkTopPGC:
    state.PGN = 0;
    state.CellN = 0;
    goto play_PGC; // Or PGN = 1; goto play_PG; ???
  case LinkNextPGC:
    state.PGCN = pgc->next_pgc_nr;
    state.PGN = 0;
    state.CellN = 0;
    goto new_PGC;
  case LinkPrevPGC:
    state.PGCN = pgc->prev_pgc_nr;
    state.PGN = 0;
    state.CellN = 0;
    goto new_PGC;
  case LinkGoUpPGC:
    state.PGCN = pgc->goup_pgc_nr;
    state.PGN = 0;
    state.CellN = 0;
    goto new_PGC;
  case LinkTailPGC:
    goto play_PGC_post;
  case LinkRSM:
    // ???
    break;
  case LinkPGCN:
    state.PGCN = link_values.data1; // ??
    goto new_PGC;
  case LinkPTTN:
    lookup_PTTN(link_values.data1, link_values.data2); //load VTS/PGC, set PGN
    goto play_PG; // ?
  case LinkPGN:
    state.PGN = link_values.data1; // ??
    goto play_PG:
  case LinkCN:
    state.CellN = link_values.data1; // ??
    goto play_Cell;
    
  case Exit:
    break; // ????
  case JumpTT:
    get_TT(link_values.data1);
    goto new_PGC;
  case JumpVTS_TT:
    get_VTS_TT(link_values.data1, link_values.data2);
    goto new_PGC;
  case JumpVTS_PTT:
    get_VTS_PTT(link_values.data1, link_values.data2, link_values.data3);
    //readPGC();
    goto playPG;
    
    
  case JumpSS_FP:
  case JumpSS_VMGM_MENU:
  case JumpSS_VTSM:
  case JumpSS_VMGM_PGC:
    
  case CallSS_FP:
  case CallSS_VMGM_MENU:
  case CallSS_VTSM:
  case CallSS_VMGM_PGC:

  case default:
    break;
  }
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
  
  if(strncmp("DVDVIDEO-VTS",data, 12) == 0) {
    vtsi_mat_t vtsi_mat;
    pgcit_t vts_pgcit;
    menu_pgci_ut_t vtsm_pgci_ut;
      
    PUT(1, "VTS top level\n-------------\n");
    memcpy(&vtsi_mat, data, sizeof(vtsi_mat));
    //ifoPrint_VTSI_MAT(&vtsi_mat);
      
      
    PUT(1, "\nPGCI Unit table\n--------------------\n");
    if(vtsi_mat.vts_pgcit != 0) {
      ifoRead_PGCIT(&vts_pgcit, vtsi_mat.vts_pgcit, 0);
      //ifoPrint_PGCIT(&vts_pgcit);
    } else
      PUT(5, "No PGCI Unit table present\n");
      
    PUT(1, "\nMenu PGCI Unit table\n--------------------\n");
    if(vtsi_mat.vtsm_pgci_ut != 0) {
      ifoRead_MENU_PGCI_UT(&vtsm_pgci_ut, vtsi_mat.vtsm_pgci_ut);
      //ifoPrint_MENU_PGCI_UT(&vtsm_pgci_ut);
    } else
      PUT(5, "No Menu PGCI Unit table present\n");
  }
  
  return 0;
}

