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

#include "common.h"

#include "ifo.h"
#include "ifo_read.h"
#include "ifo_print.h"
#include "decoder.h"

extern int demux_data(char *file_name, int start_sector, 
		      int last_sector, command_data_t *cmd); // nav.c
extern void set_spu_palette(uint32_t palette[16]); // nav.c
extern int msgqid;


#define PUT(level, text...) \
if(level < debug) { \
  fprintf(stdout, ## text); \
}


typedef enum {
  FP_DOMAIN = 1,
  VTS_DOMAIN = 2,
  VMGM_DOMAIN = 4,
  VTSM_DOMAIN = 8
} domain_t;  

/*
  State: SPRM, GPRM, Domain, pgc, pgN, cellN, ?
*/
struct {
  state_t registers;
  domain_t domain;
  int pgcN;
  int pgN;
  int cellN;
  int rsm_pgcN;
  int rsm_pgN;
  int rsm_cellN;
} state;

link_t link_values;

char lang[2] = "en";


int debug = 8;
FILE *vmg_file;
FILE *vts_file;

vmgi_mat_t vmgi_mat;
vtsi_mat_t vtsi_mat;
pgc_t pgc;
pgcit_t *pgcit;




void get_TT(int tt);
void get_VTS_TT(int vts, int tt);
void get_VTS_PTT(int tt, int part);
pgcit_t* get_VMGM_LU(char language[2]);
void get_VMGM_MENU(int menu);
void get_VMGM_PGC(int pgcN);
void get_VTSM(int vts, int title, int menu);
void get_PGC(int pgcN);





char *program_name;

void usage()
{
  fprintf(stderr, "Usage: %s  [-d <debug_level>] [-m <msgqid>]\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int c; 
  
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:m:h?")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      exit(1);
    }
  }
  
  // Setup State
  state.registers.SPRM[13] = 8; // Parental Level
  state.registers.SPRM[20] = 1; // Player Regional Code
  state.registers.SPRM[4] = 1; // Title Number (VTS#)
  state.registers.SPRM[5] = 1; // VTS Title Number (TT#)
  state.registers.SPRM[6] = 0; // Title PGC Number
  state.pgcN = 0;
  state.pgN = 0;
  state.cellN = 0;
  state.domain = FP_DOMAIN;
  memset(state.registers.GPRM, 0, sizeof(uint16_t)*16);
  
  
  ifoOpen_VMG(&vmgi_mat, "VIDEO_TS.IFO");
  
  ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
  //goto play_PGC;
  
  
  
 play_PGC:
  //DEBUG
  {
    int i;
    for(i=0;pgc.pgc_command_tbl &&
	  i<pgc.pgc_command_tbl->nr_of_pre;i++)
      ifoPrint_COMMAND(i, pgc.pgc_command_tbl->pre_commands[i]);
  }
  /* Send the palette to the spu. */
  set_spu_palette(pgc.palette);
  
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG1 (also a kind of jump/link)
       (This is what happens if you fall of the end of the pre_commands)
     - or a error (are there more cases?) */
  if(pgc.pgc_command_tbl &&
     eval(pgc.pgc_command_tbl->pre_commands, 
	  pgc.pgc_command_tbl->nr_of_pre, &state.registers, &link_values)) {
    goto process_jump;
  }
  if(state.pgN == 0)
    state.pgN = 1; //??
  goto play_PG;
  
    
 play_PG:
  printf("play_PG: state.pgN (%i)\n", state.pgN);
  assert(state.pgN > 0);
  if(state.pgN > pgc.nr_of_programs) {
    assert(state.pgN == pgc.nr_of_programs + 1);
    goto play_PGC_post; //?
  } else {
    if(state.cellN == 0)
      state.cellN = pgc.pgc_program_map[state.pgN-1];
    goto play_Cell;
  }
  
 play_Cell: 
  {
    char *name;
    cell_playback_tbl_t cell;
    printf("play_Cell: state.cellN (%i)\n", state.cellN);
    assert(state.cellN > 0);
    if(state.cellN <= pgc.nr_of_cells) { // Update pgN
      int i = 0; 
      while(i <= pgc.nr_of_programs && state.cellN >= pgc.pgc_program_map[i])
	i++;
      state.pgN = i; // ??
    } else {
      printf("state.cellN (%i) == pgc.nr_of_cells + 1 (%i)\n", 
	     state.cellN, pgc.nr_of_cells + 1);
      assert(state.cellN == pgc.nr_of_cells + 1); 
      goto play_PGC_post; 
    }
    cell = pgc.cell_playback_tbl[state.cellN-1];
    /* This is one work 'unit' for the video/audio decoder */
    //DEBUG
    ifoPrint_time(5, &cell.playback_time); printf("\n");
    
    // Make file name
    if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
      name = "VIDEO_TS.VOB";
    } else {
      char part = '0'; 
      if(state.domain != VTSM_DOMAIN)
	part += cell.first_sector/(1024  * 1024 * 1024 / 2048) + 1;
      name = malloc(16); // Bad!
      snprintf(name, 14, "VTS_%02i_%c.VOB", state.registers.SPRM[4], part);
    }
    
    PUT(6, "%s\t", name);
    PUT(5, "VOB ID: %3i, Cell ID: %3i at sector: 0x%08x - 0x%08x\n",
	pgc.cell_position_tbl[state.cellN-1].vob_id_nr,
	pgc.cell_position_tbl[state.cellN-1].cell_nr,
	cell.first_sector, cell.last_sector);
    // How to handle buttons and other nav data?
    /* Should this return any button_cmd that executes?? */
    {
      command_data_t cmd;
      int res = demux_data(name, cell.first_sector, cell.last_sector, &cmd);
      if(res) {
	// Remove me..
	state.registers.SPRM[8] = 0x400 * res;
	if(eval(&cmd, 1, &state.registers, &link_values)) {
	  goto process_jump;
	} else {
	  // Error ?? goto tail? goto next PG? or what? just continue?
	}
      }
    }
    /* Still time or some thing */
    if(cell.category & 0xff00) {
      int time = (cell.category>>8) & 0xff;
      if(time == 0xff) {
	printf("-- Wait for user interaction --\n");
      } else {
	sleep(time); // Really advance SCR clock..
      }
    }
    /* Deal with looking up and 'evaling' the Cell cmd, if any */
    if((cell.category & 0xff) != 0) {
      command_data_t *cmd =
	&pgc.pgc_command_tbl->cell_commands[(cell.category&0xff)-1];
      if(eval(cmd, 1, &state.registers, &link_values)) {
	goto process_jump;
      }
    }
    /* Where to continue after playing the cell... */
    switch((cell.category & 0xff000000)>>24) {
    default:
      state.cellN++;
    }
    goto play_Cell;
  }
  
 play_PGC_post:
  //DEBUG
  {
    int i;
    for(i=0; pgc.pgc_command_tbl &&
	  i<pgc.pgc_command_tbl->nr_of_post; i++)
      ifoPrint_COMMAND(i, pgc.pgc_command_tbl->post_commands[i]);
  }
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - or a error (are there more cases?)
     - if you got to the end of the post_commands, then what ?? */
  if(pgc.pgc_command_tbl &&
     eval(pgc.pgc_command_tbl->post_commands, 
	  pgc.pgc_command_tbl->nr_of_post, &state.registers, &link_values)) {
    goto process_jump;
  }
  //link_values = {LinkNextPGC, 0, 0, 0};
  printf("** fell of the end of the pgc\n");
  goto process_jump;
  
 process_jump:
  printf("%i %i %i %i\n", link_values.command, 
	 link_values.data1, link_values.data2, link_values.data3);
  switch(link_values.command) {
  case LinkNoLink: // Vill inte ha det h�r h�r..
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);
    
  case LinkTopC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    goto play_Cell;
  case LinkNextC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    state.cellN += 1; // >nr_of_cells?
    goto play_Cell;    
  case LinkPrevC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    state.cellN -= 1; // < 1?
    goto play_Cell;   
    
  case LinkTopPG:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    //state.pgN = ?
    goto play_PG;
  case LinkNextPG:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    state.pgN += 1; // What if pgN becomes > pgc.nr_of_programs?
    state.cellN = 0;
    goto play_PG;
  case LinkPrevPG:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    state.pgN -= 1; // What if pgN becomes < 1?
    state.cellN = 0;
    goto play_PG;
  
  case LinkTopPGC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);  
  case LinkNextPGC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);  
  case LinkPrevPGC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);  
  case LinkGoUpPGC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);  
  case LinkTailPGC:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    goto play_PGC_post;
  
  case LinkRSM:
    state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);  
  
  case LinkPGCN:
    get_PGC(link_values.data1);
    goto play_PGC;
  
  case LinkPTTN:
    state.registers.SPRM[8] = 0x400 * link_values.data2;
    exit(-1);  
  
  case LinkPGN:
    state.registers.SPRM[8] = 0x400 * link_values.data2;
    state.pgN = link_values.data1;
    state.cellN = 0;
    goto play_PG;
  
  case LinkCN:
    state.registers.SPRM[8] = 0x400 * link_values.data2;
    state.cellN = link_values.data1;
    goto play_Cell;
  
  case Exit:
    exit(-1); // What should we do here??
  case JumpTT:
    assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
    state.domain = VTS_DOMAIN;
    get_TT(link_values.data1);
    goto play_PGC;
  case JumpVTS_TT:
    assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
    state.domain = VTS_DOMAIN;
    get_VTS_TT(state.registers.SPRM[4], link_values.data1);
    goto play_PGC;
  case JumpVTS_PTT:
    assert(state.domain == VTSM_DOMAIN); //??
    state.domain = VTS_DOMAIN;    
    get_VTS_PTT(link_values.data1, link_values.data2);
    goto play_PGC;
    
  case JumpSS_FP:
    assert(state.domain == VMGM_DOMAIN || state.domain == VTSM_DOMAIN); //??   
    state.rsm_cellN = link_values.data1; //??
    // ?? more sets ??
    state.domain = FP_DOMAIN;
    ifoClose();
    ifoOpen_VMG(&vmgi_mat, "VIDEO_TS.IFO");
    //free pgc
    ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
    state.pgN = 0;
    state.cellN = 0;
    goto play_PGC;
  case JumpSS_VMGM_MENU:
    assert(state.domain == VMGM_DOMAIN || 
	   state.domain == VTSM_DOMAIN || 
	   state.domain == FP_DOMAIN); //??    
    state.domain = VMGM_DOMAIN;
    get_VMGM_MENU(link_values.data1);
    goto play_PGC;
  case JumpSS_VTSM:
    assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
    state.domain = VTSM_DOMAIN;   
    get_VTSM(link_values.data1, link_values.data2, link_values.data3);
    goto play_PGC;
  case JumpSS_VMGM_PGC:
    assert(state.domain == VMGM_DOMAIN || 
	   state.domain == VTSM_DOMAIN || 
	   state.domain == FP_DOMAIN); //??    
    state.domain = VMGM_DOMAIN;
    get_VMGM_PGC(link_values.data1);
    goto play_PGC;
    
  case CallSS_FP:
    assert(state.domain == VTS_DOMAIN); //??   
    state.rsm_cellN = link_values.data1; //??
    // ?? more sets ?? reset all registers to initial values?
    state.domain = FP_DOMAIN;
    ifoClose();
    ifoOpen_VMG(&vmgi_mat, "VIDEO_TS.IFO");
    //free pgc
    ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
    state.pgN = 0;
    state.cellN = 0;
    goto play_PGC;
  case CallSS_VMGM_MENU:
    assert(state.domain == VTS_DOMAIN); //??   
    exit(-1);
  case CallSS_VTSM:
    assert(state.domain == VTS_DOMAIN); //??   
    state.rsm_pgcN = state.pgcN;
    state.rsm_pgN = state.pgN;
    state.rsm_cellN = state.cellN; // = link_values.data2; ??
    state.domain = VTSM_DOMAIN;
    get_VTSM(state.registers.SPRM[4], 1, link_values.data1); // New function?
    goto play_PGC;
  case CallSS_VMGM_PGC:
    assert(state.domain == VTS_DOMAIN); //??   
    state.rsm_pgcN = state.pgcN;
    state.rsm_pgN = state.pgN;
    state.rsm_cellN = state.cellN;
    state.domain = VMGM_DOMAIN;
    get_VMGM_PGC(link_values.data1);
    goto play_PGC;
  
  }
  
  exit(0);
}


void get_TT(int tt) {
  vmg_ptt_srpt_t vmg_ptt_srpt;
  
  ifoClose();
  ifoOpen_VMG(&vmgi_mat, "VIDEO_TS.IFO");
  ifoRead_VMG_PTT_SRPT(&vmg_ptt_srpt, vmgi_mat.vmg_ptt_srpt);
  
  assert(tt <= vmg_ptt_srpt.nr_of_srpts);
  
  get_VTS_TT(vmg_ptt_srpt.title_info[tt-1].title_set_nr,
	     vmg_ptt_srpt.title_info[tt-1].vts_ttn);
  //ifoFree_VMG_PTT_SRPT(&vmg_ptt_srpt);
}

void get_VTS_TT(int vts, int tt) {
  char buffer[16];
  snprintf(buffer, 15, "VTS_%02i_0.IFO", vts);
  ifoClose();
  ifoOpen_VTS(&vtsi_mat, buffer);
  
  state.registers.SPRM[4] = vts;
  
  get_VTS_PTT(tt, 1);
}

void get_VTS_PTT(int tt, int part) {
  vts_ptt_srpt_t vts_ptt_srpt;
  
  ifoRead_VTS_PTT_SRPT(&vts_ptt_srpt, vtsi_mat.vts_ptt_srpt);
  
  assert(tt <= vts_ptt_srpt.nr_of_srpts);
  assert(part <= vts_ptt_srpt.title_info[tt-1].nr_of_ptts);
  
  state.pgcN = vts_ptt_srpt.title_info[tt-1].ptt_info[part-1].pgcn;
  state.pgN = vts_ptt_srpt.title_info[tt-1].ptt_info[part-1].pgn;
  state.cellN = 0;
  state.registers.SPRM[5] = tt;
  state.registers.SPRM[6] = state.pgcN;
  //ifoFree_VTS_PTT_SRPT(&vts_ptt_srpt);
  
  pgcit = malloc(sizeof(pgcit_t));
  ifoRead_PGCIT(pgcit, vtsi_mat.vts_pgcit * DVD_BLOCK_LEN);
  memcpy(&pgc, pgcit->pgci_srp[state.pgcN-1].pgc, sizeof(pgc_t));
  //ifoFree_PGCIT(&pgcit);
}


pgcit_t* get_VMGM_LU(char language[2]) {
  menu_pgci_ut_t pgci_ut;
  int i;
  
  ifoClose();
  ifoOpen_VMG(&vmgi_mat, "VIDEO_TS.IFO");
  
  ifoRead_MENU_PGCI_UT(&pgci_ut, vmgi_mat.vmgm_pgci_ut);
  i = 0;
  while(i < pgci_ut.nr_of_lang_units && 
	memcmp(&pgci_ut.menu_lu[i].lang_code, language, 2))
    i++;
  if(i == pgci_ut.nr_of_lang_units) {
    printf("** Wrong language\n");
    i = 0; //error?
  }
  //How/when to call ifoFree_MENU_PGCI_UT
  
  return pgci_ut.menu_lu[i].menu_pgcit;//? copy?
}

void get_VMGM_MENU(int menu) {
  int i;
  
  pgcit = get_VMGM_LU(lang);
  
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != menu)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    i = 0; // error!
  }
  state.pgcN = i + 1;
  state.pgN = 0;
  state.cellN = 0;
  
  memcpy(&pgc, pgcit->pgci_srp[state.pgcN-1].pgc, sizeof(pgc_t));
  //ifoFree_PGCIT(&pgcit);
}


void get_VMGM_PGC(int pgcN) {

  pgcit = get_VMGM_LU(lang);
   
  state.pgcN = pgcN;
  state.pgN = 0;
  state.cellN = 0;
  
  memcpy(&pgc, pgcit->pgci_srp[state.pgcN-1].pgc, sizeof(pgc_t));
  //ifoFree_PGCIT(&pgcit);
}

void get_VTSM(int vts, int title, int menu) {
  int i;
  menu_pgci_ut_t pgci_ut;
  
  char buffer[16];
  snprintf(buffer, 16, "VTS_%02i_0.IFO", vts);
  ifoClose();
  ifoOpen_VTS(&vtsi_mat, buffer);
  
  state.registers.SPRM[4] = vts;
  
  assert(title == 1); // I don't know what title is supposed to be used for.
  
  ifoRead_MENU_PGCI_UT(&pgci_ut, vtsi_mat.vtsm_pgci_ut);
  i = 0;
  while(i < pgci_ut.nr_of_lang_units && 
	memcmp(&pgci_ut.menu_lu[i].lang_code, lang, 2))
    i++;
  if(i == pgci_ut.nr_of_lang_units) {
    printf("** Wrong language\n");
    i = 0; //error?
  }
  pgcit = pgci_ut.menu_lu[i].menu_pgcit;
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != menu)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    i = 0; // error!
  }
  state.pgcN = i + 1;
  state.pgN = 0;
  state.cellN = 0;
  
  memcpy(&pgc, pgcit->pgci_srp[state.pgcN-1].pgc, sizeof(pgc_t));
  //ifoFree_MENU_PGCI_UT(&pgci_ut);
}


void get_PGC(int pgcN) {
  state.pgcN = pgcN;
  state.pgN = 0;
  state.cellN = 0;
  
  memcpy(&pgc, pgcit->pgci_srp[state.pgcN-1].pgc, sizeof(pgc_t));
}
