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
#include "msgevents.h"

#include "ifo.h"
#include "ifo_read.h"
#include "ifo_print.h"
#include "decoder.h"
#include "vm.h"

extern vm_cmd_t eval_cell(char *vob_name, cell_playback_tbl_t *cell, 
			  int block, dvd_state_t *state); // nav.c
extern void set_spu_palette(uint32_t palette[16]); // nav.c
extern void wait_for_init(MsgEventQ_t *msgq); // com.c

static int msgqid = -1;

MsgEventQ_t *msgq;

#define PUT(level, text...) \
if(level < debug) { \
  fprintf(stdout, ## text); \
}

/**
 * The following structure defines a complete VMGI file.
 */
struct {
    vmgi_mat_t *vmgi_mat;
    vmg_ptt_srpt_t *vmg_ptt_srpt;
    pgc_t *first_play_pgc;    
    menu_pgci_ut_t *vmgm_pgci_ut;
    vmg_ptl_mait_t *vmg_ptl_mait;
    vmg_vts_atrt_t *vmg_vts_atrt;
    vmg_txtdt_mgi_t *vmg_txtdt_mgi;
    c_adt_t *vmgm_c_adt;
    vobu_admap_t *vmgm_vobu_admap;
} vmgi;

/**
 * The following structure defines a complete VTSI file.
 */
struct {
    vtsi_mat_t *vtsi_mat;
    vts_ptt_srpt_t *vts_ptt_srpt;
    pgcit_t *vts_pgcit;
    menu_pgci_ut_t *vtsm_pgci_ut;
    c_adt_t *vtsm_c_adt;
    vobu_admap_t *vtsm_vobu_admap;
    c_adt_t *vts_c_adt;
    vobu_admap_t *vts_vobu_admap;
} vtsi;

int open_vts_nr = -1; // 0 is ok too..
FILE *vmg_file;
FILE *vts_file;

/* Current (state) variables */
pgcit_t *pgcit;
pgc_t *pgc;
int start_block;

dvd_state_t state;


link_t link_values;

// Should be some where in the state
char lang[2] = "en";

int debug = 8;



int start_vm(void);
int run_vm(void);

void get_TT(int tt);
int get_VTS_PTT(int vtsN, int tt, int part);
int get_VTS_PGC(int vtsN, int pgc);
pgcit_t* get_VMGM_LU(char language[2]);
int get_VMGM_MENU(int menu);
int get_VMGM_PGC(int pgcN);
int get_VTSM(int vtsN, int title, int menu);
int get_PGC(int pgcN);
void get_FP_PGC(void);



void ifoOpenVMGI() {
  if(vmg_file == NULL) {
    vmgi.vmgi_mat = malloc(sizeof(vmgi_mat_t));
    vmg_file = ifoOpen_VMG(vmgi.vmgi_mat, "VIDEO_TS.IFO");
  }
  ifo_file = vmg_file;
}

void ifoOpenVTSI(int vtsN) {
  if(vtsN != open_vts_nr) {
    char buffer[16];
    snprintf(buffer, 15, "VTS_%02i_0.IFO", vtsN);
    /* Free stuff too... */
    if(vtsi.vtsi_mat != NULL)
      free(vtsi.vtsi_mat);
    if(vtsi.vts_ptt_srpt != NULL)
      free(vtsi.vts_ptt_srpt);
    if(vtsi.vts_pgcit != NULL)
      free(vtsi.vts_pgcit);
    if(vtsi.vtsm_pgci_ut != NULL)
      free(vtsi.vtsm_pgci_ut);
    if(vtsi.vtsm_c_adt != NULL)
      free(vtsi.vtsm_c_adt);
    if(vtsi.vtsm_vobu_admap != NULL)
      free(vtsi.vtsm_vobu_admap);
    if(vtsi.vts_c_adt != NULL)
      free(vtsi.vts_c_adt);
    if(vtsi.vts_vobu_admap != NULL)
      free(vtsi.vts_vobu_admap);
    memset(&vtsi, 0, sizeof(vtsi));
    ifoClose();
    vtsi.vtsi_mat = malloc(sizeof(vtsi_mat_t));
    vts_file = ifoOpen_VTS(vtsi.vtsi_mat, buffer);
    open_vts_nr = vtsN;
  }
  ifo_file = vts_file;
}



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
  
  if(msgqid != -1) {
    MsgEvent_t ev;
    
    if((msgq = MsgOpen(msgqid)) == NULL) {
      fprintf(stderr, "vm: couldn't get message q\n");
      exit(-1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_DVD_NAV;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
      fprintf(stderr, "vm: register capabilities\n");
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DEMUX_MPEG2_PS | DEMUX_MPEG1;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
      fprintf(stderr, "vm: didn't get demux cap\n");
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DECODE_DVD_SPU;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
      fprintf(stderr, "vm: didn't get cap\n");
    }
    
    wait_for_init(msgq);
    
  } else {
    fprintf(stderr, "what?\n");
  }
  
  /*  Call start here */
  start_vm();
  run_vm();
  
  return -1;
}
  
int start_vm()
{
  
  // Setup State
  memset(state.registers.SPRM, 0, sizeof(uint16_t)*24);
  memset(state.registers.GPRM, 0, sizeof(state.registers.GPRM));
  state.PTL_REG = 8; // Parental Level
  state.registers.SPRM[20] = 1; // Player Regional Code
  state.VTS_REG = 1; // Title Number (VTS#)
  state.TT_REG = 1;  // VTS Title Number (TT#)
  state.PGC_REG = 0; // Title PGC Number
  state.pgcN = 0;
  state.pgN = 0;
  state.cellN = 0;
  state.domain = FP_DOMAIN;

  ifoOpenVMGI(); // Check for error
  //assert(ifo_file);
  get_FP_PGC();
  
  return 0;
}
  
int run_vm()
{
  
 play_PGC:
  { //DEBUG
    int i;
    for(i=0;pgc->pgc_command_tbl &&
	  i<pgc->pgc_command_tbl->nr_of_pre;i++)
      ifoPrint_COMMAND(i, &pgc->pgc_command_tbl->pre_commands[i]);
  }
  
  // FIXME XXX $$$ Only send when needed, and do send even if not playing
  // from start? (should we do pre_commands when jumping to say part 3?)
  /* Send the palette to the spu. */
  set_spu_palette(pgc->palette);
  
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG1 (also a kind of jump/link)
       (This is what happens if you fall of the end of the pre_commands)
     - or a error (are there more cases?) */
  if(pgc->pgc_command_tbl && eval(pgc->pgc_command_tbl->pre_commands, 
				  pgc->pgc_command_tbl->nr_of_pre, 
				  &state.registers, &link_values)) {
    // link_values contains the 'jump' return value
    goto process_jump;
  }
  if(state.pgN == 0) {
    state.pgN = 1;
  }
  goto play_PG;
  
  
    
 play_PG:
  printf("play_PG: state.pgN (%i)\n", state.pgN);
  
  assert(state.pgN > 0);
  if(state.pgN > pgc->nr_of_programs) {
    assert(state.pgN == pgc->nr_of_programs + 1);
    goto play_PGC_post; // ??
  } 
  
  if(state.cellN == 0) {
    state.cellN = pgc->pgc_program_map[state.pgN - 1];
  }
  goto play_Cell;
  
  
  
 play_Cell:
  {
    char *name;
    cell_playback_tbl_t *cell;
    printf("play_Cell: state.cellN (%i)\n", state.cellN);
    
    assert(state.cellN > 0);
    if(state.cellN > pgc->nr_of_cells) {
      printf("state.cellN (%i) == pgc->nr_of_cells + 1 (%i)\n", 
	     state.cellN, pgc->nr_of_cells + 1);
      assert(state.cellN == pgc->nr_of_cells + 1); 
      goto play_PGC_post; 
    }
    
    /* Multi angle */
    switch(pgc->cell_playback_tbl[state.cellN - 1].block_mode) {
    case 0: // Normal
      break;
    case 1: // The first cell in the block
      switch(pgc->cell_playback_tbl[state.cellN - 1].block_type) {
      case 0: // Not part of a block
	assert(0);
      case 1: // Angle block
	/* Loop and check each cell instead? */
	state.cellN += state.AGL_REG - 1;
	assert(state.domain == VTSM_DOMAIN); // ??
	assert(state.cellN <= pgc->nr_of_cells);
	assert(pgc->cell_playback_tbl[state.cellN - 1].block_mode != 0);
	assert(pgc->cell_playback_tbl[state.cellN - 1].block_type == 1);
	break;
      case 2:
      case 3:
      default:
      }
      /* fall though */
    case 2:
    case 3:
    default:
      fprintf(stderr, "Invalid/Unknown block_mode (%d), block_type (%d)\n",
	      pgc->cell_playback_tbl[state.cellN - 1].block_mode,
	      pgc->cell_playback_tbl[state.cellN - 1].block_type);
    }
    start_block = 0;
    
    { // Update pgN
      int i = 0; 
      while(i <= pgc->nr_of_programs && state.cellN >= pgc->pgc_program_map[i])
	i++;
      state.pgN = i; // ??
    }
    cell = &pgc->cell_playback_tbl[state.cellN - 1];
    
    //DEBUG
    ifoPrint_time(5, &cell->playback_time); printf("\n");
    
    // Make file name
    if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
      name = malloc(16); //TODO ugly
      snprintf(name, 14, "VIDEO_TS.VOB");
    } else {
      char part = '0'; 
      if(state.domain != VTSM_DOMAIN)
	part += cell->first_sector/(1024  * 1024 * 1024 / 2048) + 1;
      name = malloc(16); // Bad!
      snprintf(name, 14, "VTS_%02i_%c.VOB", state.VTS_REG, part);
    }
    
    PUT(6, "%s\t", name);
    PUT(5, "VOB ID: %3i, Cell ID: %3i at sector: 0x%08x - 0x%08x\n",
	pgc->cell_position_tbl[state.cellN - 1].vob_id_nr,
	pgc->cell_position_tbl[state.cellN - 1].cell_nr,
	cell->first_sector, cell->last_sector);
    
    /* This should return any button_cmd that is to be executed,
     * or a NOP in case no button was pressed */
    {
      vm_cmd_t cmd;
      
      cmd = eval_cell(name, cell, start_block, &state);
      free(name);
      
      if(eval(&cmd, 1, &state.registers, &link_values)) {
	goto process_jump;
      } else {
	// Error ?? goto tail? goto next PG? or what? just continue?
      }
    }
    
    /* Deal with a Cell cmd, if any */
    if(cell->cell_cmd_nr != 0) {
      assert(pgc->pgc_command_tbl != NULL);
      assert(pgc->pgc_command_tbl->nr_of_cell >= cell->cell_cmd_nr);
      ifoPrint_COMMAND(0, &pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1]);
      if(eval(&pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1], 1,
	      &state.registers, &link_values)) {
	goto process_jump;
      } else {
	// Error ?? goto tail? goto next PG? or what? just continue?
      }
    }

    /* Where to continue after playing the cell... */
    /* Multi angle */
    switch(pgc->cell_playback_tbl[state.cellN - 1].block_mode) {
    case 0: // Normal
      state.cellN++;
      start_block = 0;
      break;
    case 1: // The first cell in the block
    case 2: // A cell in the block
    case 3: // The last cell in the block
    default:
      assert(pgc->cell_playback_tbl[state.cellN - 1].block_type == 1);
      /* Skip the 'other' angles */
      state.cellN++;
      while(pgc->cell_playback_tbl[state.cellN - 1].block_mode >= 2) {
	state.cellN++;
      }
      start_block = 0;
      break;
    }
    goto play_Cell;
  }
  
  
 play_PGC_post:
  assert(pgc->still_time == 0); // FIXME $$$

  { //DEBUG
    int i;
    for(i=0; pgc->pgc_command_tbl &&
	  i<pgc->pgc_command_tbl->nr_of_post; i++)
      ifoPrint_COMMAND(i, &pgc->pgc_command_tbl->post_commands[i]);
  }
  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - or a error (are there more cases?)
     - if you got to the end of the post_commands, then what ?? */
  if(pgc->pgc_command_tbl &&
     eval(pgc->pgc_command_tbl->post_commands, 
	  pgc->pgc_command_tbl->nr_of_post, &state.registers, &link_values)) {
    goto process_jump;
  } else {
    link_t link_next_pgc = {LinkNextPGC, 0, 0, 0};
    printf("** fell of the end of the pgc\n");
    assert(pgc->next_pgc_nr != 0);
    link_values = link_next_pgc; // Struct assignment
    goto process_jump;
  }
  
  
  
 process_jump:
  printf("%i %i %i %i\n", link_values.command, 
	 link_values.data1, link_values.data2, link_values.data3);
  switch(link_values.command) {
  case LinkNoLink: // Vill inte ha det här här..
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);
    
  case LinkTopC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    start_block = 0;
    goto play_Cell;
  case LinkNextC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    state.cellN += 1; // >nr_of_cells?
    start_block = 0;
    goto play_Cell;    
  case LinkPrevC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    state.cellN -= 1; // < 1?
    start_block = 0;
    goto play_Cell;   
    
  case LinkTopPG:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    // Perhaps update pgN to current valu first?
    //state.pgN = ?
    goto play_PG;
  case LinkNextPG:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    // Perhaps update pgN to current valu first?
    state.pgN += 1; // What if pgN becomes > pgc.nr_of_programs?
    state.cellN = 0;
    goto play_PG;
  case LinkPrevPG:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    // Perhaps update pgN to current valu first?
    state.pgN -= 1; // What if pgN becomes < 1?
    state.cellN = 0;
    goto play_PG;
  
  case LinkTopPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);
  case LinkNextPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);
  case LinkPrevPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);
  case LinkGoUpPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    exit(-1);
  case LinkTailPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    goto play_PGC_post;
  
  case LinkRSM:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data1;
    
    fprintf(stderr, "!!!!!!!!!!!!!!!!!! %d !!!!!!!!!!!!!!!!!!\n", state.pgcN);
    /* Restore VTS/Title nr too? */
    get_VTS_PGC(state.rsm_vtsN, state.rsm_pgcN);
    
    set_spu_palette(pgc->palette); // Erhum...
    
    state.domain = VTS_DOMAIN;
    state.VTS_REG = state.rsm_vtsN;
    /* state.TT_REG = tt; */
    //state.pgcN = state.rsm_pgcN; set by get_VTS_PGC
    state.PGC_REG = state.rsm_pgcN; // ??
    state.cellN = state.rsm_cellN;
    
    if(state.cellN == 0) {
      assert(state.cellN); // Checking if this ever happens
      /* assert( time/block/vobu is 0 ); */
      state.pgN = 1;
      goto play_PG;
    } else { 
      /* assert( time/block/vobu is _not_ 0 ); */
      /* play_Cell_at_time */
      //state.pgN = state.rsm_pgN;  this gets the righ value in play_Cell
      start_block = state.rsm_blockN;
      goto play_Cell;
    }
    
  case LinkPGCN:
    /* Make sure that pgcit is correct !!! */
    get_PGC(link_values.data1);
    state.pgN = 0;
    state.cellN = 0;
    goto play_PGC;
  
  case LinkPTTN:
    if(link_values.data2 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data2;
    exit(-1);  
  
  case LinkPGN:
    if(link_values.data2 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data2;
    state.pgN = link_values.data1;
    state.cellN = 0;
    goto play_PG;
  
  case LinkCN:
    if(link_values.data2 != 0)
      state.registers.SPRM[8] = 0x400 * link_values.data2;
    state.cellN = link_values.data1;
    start_block = 0;
    goto play_Cell;
  
  case Exit:
    exit(-1); // What should we do here??
  case JumpTT:
    assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
    state.domain = VTS_DOMAIN;
    get_TT(link_values.data1); // Sets all necessary state
    goto play_PGC;
  case JumpVTS_TT:
    assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
    state.domain = VTS_DOMAIN;
    get_VTS_PTT(state.VTS_REG, link_values.data1, 1);
    goto play_PGC;
  case JumpVTS_PTT:
    assert(state.domain == VTSM_DOMAIN); //??
    state.domain = VTS_DOMAIN;    
    get_VTS_PTT(state.VTS_REG, link_values.data1, link_values.data2);
    goto play_PGC;
    
  case JumpSS_FP:
    assert(state.domain == VMGM_DOMAIN || state.domain == VTSM_DOMAIN); //??   
    state.domain = FP_DOMAIN;
    get_FP_PGC();
    state.pgN = 0;
    state.cellN = 0;
    goto play_PGC;
  case JumpSS_VMGM_MENU:
    assert(state.domain == VMGM_DOMAIN || 
	   state.domain == VTSM_DOMAIN || 
	   state.domain == FP_DOMAIN); //??    
    state.domain = VMGM_DOMAIN;
    get_VMGM_MENU(link_values.data1);
    state.pgN = 0;
    state.cellN = 0;
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
    state.pgN = 0;
    state.cellN = 0;
    goto play_PGC;
    
  case CallSS_FP:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data1 != 0) {
      state.rsm_cellN = link_values.data1;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = start_block; // ??
    }
    state.rsm_vtsN = state.VTS_REG;
    state.rsm_pgcN = state.pgcN;
    state.rsm_pgN = state.pgN;
    state.domain = FP_DOMAIN;
    
    get_FP_PGC();
    state.pgN = 0; // Start at top of the PGC
    state.cellN = 0;
    goto play_PGC;
  case CallSS_VMGM_MENU:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data2 != 0) {
      state.rsm_cellN = link_values.data2;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = start_block; // ??
    }
    state.rsm_vtsN = state.VTS_REG;
    state.rsm_pgcN = state.pgcN;
    state.rsm_pgN = state.pgN;
    state.domain = VMGM_DOMAIN;
    
    if(get_VMGM_MENU(link_values.data1) == -1) {
      link_values.command = LinkRSM;
      link_values.data1 = 0;
      goto process_jump;
    } else {
      state.pgN = 0;
      state.cellN = 0;
      goto play_PGC;
    }
  case CallSS_VTSM:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data2 != 0) {
      state.rsm_cellN = link_values.data2;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = start_block; // ??
    }      
    state.rsm_vtsN = state.VTS_REG;
    state.rsm_pgcN = state.pgcN;
    state.rsm_pgN = state.pgN;
    state.domain = VTSM_DOMAIN;
    
    if(get_VTSM(state.VTS_REG, 1, link_values.data1) == -1) {
      link_values.command = LinkRSM;
      link_values.data1 = 0;
      goto process_jump;      
    } else {
      goto play_PGC;
    }
  case CallSS_VMGM_PGC:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data2 != 0) {
      state.rsm_cellN = link_values.data2;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = start_block; // ??
    }      
    state.rsm_vtsN = state.VTS_REG;
    state.rsm_pgcN = state.pgcN;
    state.rsm_pgN = state.pgN;
    state.domain = VMGM_DOMAIN;
    
    if(get_VMGM_PGC(link_values.data1) == -1) {
      link_values.command = LinkRSM;
      link_values.data1 = 0;
      goto process_jump;      
    } else {
      state.pgN = 0;
      state.cellN = 0;
      goto play_PGC;
    }
  }
  
  exit(0);
}

void get_FP_PGC() {
  
  ifoOpenVMGI(); // Check for error?
  if(vmgi.first_play_pgc == NULL) {
    vmgi.first_play_pgc = malloc(sizeof(pgc_t));
    ifoRead_PGC(vmgi.first_play_pgc, vmgi.vmgi_mat->first_play_pgc);
  }
  pgc = vmgi.first_play_pgc;
}

void get_TT(int tt) {
  
  ifoOpenVMGI();
  if(vmgi.vmg_ptt_srpt == NULL) {
    vmgi.vmg_ptt_srpt = malloc(sizeof(vmg_ptt_srpt_t));
    ifoRead_VMG_PTT_SRPT(vmgi.vmg_ptt_srpt, vmgi.vmgi_mat->vmg_ptt_srpt);
  }
  assert(tt <= vmgi.vmg_ptt_srpt->nr_of_srpts);
  
  get_VTS_PTT(vmgi.vmg_ptt_srpt->title_info[tt - 1].title_set_nr,
	      vmgi.vmg_ptt_srpt->title_info[tt - 1].vts_ttn, 1);
}

int get_VTS_PTT(int vtsN, int tt, int part) {
  int pgcN, pgN;
  
  ifoOpenVTSI(vtsN);
  if(vtsi.vts_ptt_srpt == NULL) {
    vtsi.vts_ptt_srpt = malloc(sizeof(vts_ptt_srpt_t));
    ifoRead_VTS_PTT_SRPT(vtsi.vts_ptt_srpt, vtsi.vtsi_mat->vts_ptt_srpt);
  }
  
  assert(tt <= vtsi.vts_ptt_srpt->nr_of_srpts);
  assert(part <= vtsi.vts_ptt_srpt->title_info[tt - 1].nr_of_ptts);
  
  pgcN = vtsi.vts_ptt_srpt->title_info[tt - 1].ptt_info[part - 1].pgcn;
  pgN = vtsi.vts_ptt_srpt->title_info[tt - 1].ptt_info[part - 1].pgn;
  
  if(vtsi.vts_pgcit == NULL) {
    vtsi.vts_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(vtsi.vts_pgcit, vtsi.vtsi_mat->vts_pgcit * DVD_BLOCK_LEN);
  }
  pgcit = vtsi.vts_pgcit;
  
  if(get_PGC(pgcN) == -1) {
    return -1;
  }
  
  state.VTS_REG = vtsN; // ??
  state.TT_REG = tt; // ??
  state.PGC_REG = pgcN; // ??
  //state.pgcN = pgcN; // ?? set in get_PGC
  state.pgN = pgN; // ??
  state.cellN = 0; // ??
  
  return 0;
}


int get_VTS_PGC(int vtsN, int pgcN) {
  
  ifoOpenVTSI(vtsN);
  if(vtsi.vts_pgcit == NULL) {
    vtsi.vts_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(vtsi.vts_pgcit, vtsi.vtsi_mat->vts_pgcit * DVD_BLOCK_LEN);
  }
  pgcit = vtsi.vts_pgcit;
  
  return get_PGC(pgcN);
}



pgcit_t* get_VMGM_LU(char language[2]) {
  int i;
  
  ifoOpenVMGI();
  if(vmgi.vmgm_pgci_ut == NULL) {
    vmgi.vmgm_pgci_ut = malloc(sizeof(menu_pgci_ut_t));
    ifoRead_MENU_PGCI_UT(vmgi.vmgm_pgci_ut, vmgi.vmgi_mat->vmgm_pgci_ut);
  }
  
  i = 0;
  while(i < vmgi.vmgm_pgci_ut->nr_of_lang_units && 
	memcmp(&vmgi.vmgm_pgci_ut->menu_lu[i].lang_code,  /*XXX*/ language, 2))
    i++;
  if(i == vmgi.vmgm_pgci_ut->nr_of_lang_units) {
    printf("** Wrong language\n");
    i = 0; //error?
  }
  
  return vmgi.vmgm_pgci_ut->menu_lu[i].menu_pgcit;
}

int get_VMGM_MENU(int menu) {
  int i;
  
  pgcit = get_VMGM_LU(lang);
  
  /* Get menu */
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != menu)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    i = 0; // error!
    return -1;
  }
  
  return get_PGC(i + 1);
}


int get_VMGM_PGC(int pgcN) {
 
  pgcit = get_VMGM_LU(lang);
   
  return get_PGC(pgcN);
}

int get_VTSM(int vtsN, int title, int menu) {
  int i;
  
  assert(title == 1); // I don't know what title is supposed to be used for.
  
  ifoOpenVTSI(vtsN);
  if(vtsi.vtsm_pgci_ut == NULL) {
    vtsi.vtsm_pgci_ut = malloc(sizeof(menu_pgci_ut_t));
    ifoRead_MENU_PGCI_UT(vtsi.vtsm_pgci_ut, vtsi.vtsi_mat->vtsm_pgci_ut);
  }
  
  i = 0;
  while(i < vtsi.vtsm_pgci_ut->nr_of_lang_units && 
	memcmp(&vtsi.vtsm_pgci_ut->menu_lu[i].lang_code, /*XXX*/ lang, 2))
    i++;
  if(i == vtsi.vtsm_pgci_ut->nr_of_lang_units) {
    printf("** Wrong language\n");
    i = 0; //error?
    return -1;
  }
  pgcit = vtsi.vtsm_pgci_ut->menu_lu[i].menu_pgcit;
  
  /* Get menu */
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != menu)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    i = 0; // error!
    return -1;
  }
  
  return get_PGC(i+1);
}


int get_PGC(int pgcN) {
  
  assert(pgcit != NULL);
  if(pgcN > pgcit->nr_of_pgci_srp) // error
    return -1;
  
  // Maybe this shouldn't be set here...
  state.pgcN = pgcN; // ??
 
  pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  return 0;
}


/* 
   All those that set pgc and/or pgcit and might later fail must 
   undo this before exiting!!
   That or some how make sure that pgcit is correct before calling 
   get_PGC (currently only from LinkPGCN).
   
*/
