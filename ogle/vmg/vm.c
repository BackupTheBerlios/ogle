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

int current_vts_nr = -1; // 0 is ok too..
FILE *vmg_file;
FILE *vts_file;

/* Current (state) variables */
pgcit_t *pgcit;
pgc_t *pgc;


dvd_state_t state;



// Should this be some where in the state, is it allready?
char lang[2] = "en";

int debug = 8;



//int start_vm(void);
int reset_vm(void);
int run_vm(void);

link_t play_PGC();
link_t play_PG();
link_t play_Cell();
link_t play_Cell_post();
link_t play_PGC_post();
link_t process_command(link_t link_values);
  
int get_TT(int tt);
int get_VTS_TT(int vtsN, int vts_ttn);
int get_VTS_PTT(int vtsN, int vts_ttn, int part);
int get_VTS_PGC(int vtsN, int pgcN);
pgcit_t* get_VMGM_LU(char language[2]);
int get_VMGM_MENU(int menu);
int get_VTSM(int vtsN, int title, int menu);
int get_VMGM_PGC(int pgcN);
int get_PGC(int pgcN);
int get_PGCN(void);
int get_FP_PGC(void);



void ifoOpenVMGI() 
{
  if(vmg_file == NULL) {
    vmgi.vmgi_mat = malloc(sizeof(vmgi_mat_t));
    vmg_file = ifoOpen_VMG(vmgi.vmgi_mat, "VIDEO_TS.IFO");
  }
  ifo_file = vmg_file;
}


void ifoOpenVTSI(int vtsN) 
{
  if(vtsN != current_vts_nr) {
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
    current_vts_nr = vtsN;
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
  reset_vm();
  run_vm();
  
  return -1;
}





int reset_vm()
{
  
  // Setup State
  memset(state.registers.SPRM, 0, sizeof(uint16_t)*24);
  memset(state.registers.GPRM, 0, sizeof(state.registers.GPRM));
  state.PTL_REG = 8;            // Parental Level
  state.registers.SPRM[20] = 1; // Player Regional Code
  state.TTN_REG = 1;            // Title Number
  state.VTS_TTN_REG = 1;        // VTS Title Number
  state.PGC_REG = 0;            // Title PGC Number
  state.registers.SPRM[7] = 1;  // PTT Number for One_Sequential_PGC_Title
  state.pgN = 0;
  state.cellN = 0;

  state.domain = FP_DOMAIN;
  state.rsm_vtsN = 0;
  state.rsm_pgcN = 0;
  state.rsm_cellN = 0;
  state.rsm_blockN = 0;
  
  current_vts_nr = -1;
  ifoOpenVMGI(); // Check for error
  //assert(ifo_file);
  
  return 0;
}



// FIXME TODO XXX $$$ Handle error condition too...
int run_vm()
{
  link_t link_values;

  // Set pgc to FP pgc
  state.domain = FP_DOMAIN;
  get_FP_PGC();
  link_values = play_PGC(); 
  
  if(link_values.command != PlayThis) {
    /* At the end of this PGC or we encountered a command. */
    // Terminates first when it gets a PlayThis (or error).
    link_values = process_command(link_values);
    assert(link_values.command == PlayThis);
  }

  /* For now we emulate to old system to be able to test */
  while(1) {
    char name[16];
    vm_cmd_t cmd;
    cell_playback_tbl_t *cell;
    cell = &pgc->cell_playback_tbl[state.cellN - 1];

    //DEBUG
    ifoPrint_time(5, &cell->playback_time); printf("\n");
  
    // Make file name
    if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
      snprintf(name, 14, "VIDEO_TS.VOB");
    } else {
      char part = '0'; 
      if(state.domain != VTSM_DOMAIN)
	part += cell->first_sector/(1024  * 1024 * 1024 / 2048) + 1;
      snprintf(name, 14, "VTS_%02i_%c.VOB", current_vts_nr, part);
    }
    
    printf("%s\t", name);
    printf("VOB ID: %3i, Cell ID: %3i at sector: 0x%08x - 0x%08x\n",
	pgc->cell_position_tbl[state.cellN - 1].vob_id_nr,
	pgc->cell_position_tbl[state.cellN - 1].cell_nr,
	cell->first_sector, cell->last_sector);
    
    cmd = eval_cell(name, cell, link_values.data1, &state);

    if(eval(&cmd, 1, &state.registers, &link_values)) {
      link_values = process_command(link_values);
    } else {
      link_values = play_Cell_post();
      link_values = process_command(link_values);
    }
    assert(link_values.command == PlayThis);
  }
  /* end of emulation code */

  return 0;
}  



int eval_cmd(vm_cmd_t *cmd)
{
  link_t link_values;
  
  if(eval(cmd, 1, &state.registers, &link_values)) {
    if(link_values.command != PlayThis) {
      /* At the end of this PGC or we encountered a command. */
      // Terminates first when it gets a PlayThis (or error).
      link_values = process_command(link_values);
      assert(link_values.command == PlayThis);
    }
    return 1; // Something changed
  } else {
    return 0; // It updated some state thats all...
  }
}

// FIXME TODO XXX $$$ Handle error condition too...
// How to tell if there is any need to do a Flush?
int get_next_cell()
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
  return 0;
}






link_t play_PGC() 
{    
  link_t link_values;
  
  printf("play_PGC:\n"); // state.pgcN (%i)\n", state.pgcN);

  // This must be set before the pre-commands are executed because they
  // might contain a CallSS that will save resume state
  state.pgN = 1;
  state.cellN = 0;

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
    return link_values;
  }
  
  return play_PG();
}  


link_t play_PG()
{
  printf("play_PG: state.pgN (%i)\n", state.pgN);
  
  assert(state.pgN > 0);
  if(state.pgN > pgc->nr_of_programs) {
    assert(state.pgN == pgc->nr_of_programs + 1);
    return play_PGC_post();
  }
  
  state.cellN = pgc->pgc_program_map[state.pgN - 1];
  
  return play_Cell();
}


link_t play_Cell()
{
  printf("play_Cell: state.cellN (%i)\n", state.cellN);
  
  
  assert(state.cellN > 0);
  if(state.cellN > pgc->nr_of_cells) {
    printf("state.cellN (%i) == pgc->nr_of_cells + 1 (%i)\n", 
	   state.cellN, pgc->nr_of_cells + 1);
    assert(state.cellN == pgc->nr_of_cells + 1); 
    return play_PGC_post();
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
    // Might perhaps happen for RSM
  default:
    fprintf(stderr, "Invalid/Unknown block_mode (%d), block_type (%d)\n",
	    pgc->cell_playback_tbl[state.cellN - 1].block_mode,
	    pgc->cell_playback_tbl[state.cellN - 1].block_type);
  }

  
  /* Figure out the correct pgN for this cell and update the state. */ 
  {
    int i = 0; 
    while(i <= pgc->nr_of_programs && state.cellN >= pgc->pgc_program_map[i])
      i++;
    state.pgN = i;
  }
  
  
  {
    link_t tmp = {PlayThis, 0, 0, 0};
    return tmp;
  }
  
}


link_t play_Cell_post()
{
  cell_playback_tbl_t *cell;
    
  cell = &pgc->cell_playback_tbl[state.cellN - 1];
  
  /* Still time is already taken care of before we get called. */
  
  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;
    
    assert(pgc->pgc_command_tbl != NULL);
    assert(pgc->pgc_command_tbl->nr_of_cell >= cell->cell_cmd_nr);
    ifoPrint_COMMAND(0, &pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1]);
    if(eval(&pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1], 1,
	    &state.registers, &link_values)) {
      return link_values;
    } else {
      // Error ?? goto tail? goto next PG? or what? just continue?
    }
  }
  
  
  /* Where to continue after playing the cell... */
  /* Multi angle */
  switch(pgc->cell_playback_tbl[state.cellN - 1].block_mode) {
  case 0: // Normal
    state.cellN++;
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
    break;
  }
  
  return play_Cell();
} 


link_t play_PGC_post()
{
  link_t link_values;
  
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
    return link_values;
  }
  
  // Or perhaps handle it here?
  {
    link_t link_next_pgc = {LinkNextPGC, 0, 0, 0};
    printf("** fell of the end of the pgc\n");
    assert(pgc->next_pgc_nr != 0);
    return link_next_pgc;
  }
}


link_t process_command(link_t link_values)
{
  /* FIXME $$$ Move this to a separate function? */
  while(link_values.command != PlayThis) {
  
  printf("%i %i %i %i\n", link_values.command, 
	 link_values.data1, link_values.data2, link_values.data3);
  switch(link_values.command) {
  case LinkNoLink: // Vill inte ha det h�r h�r..
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    exit(-1);
    
  case LinkTopC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    link_values = play_Cell();
    break;
  case LinkNextC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    state.cellN += 1; // >nr_of_cells?
    link_values = play_Cell();
    break;
  case LinkPrevC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    state.cellN -= 1; // < 1?
    link_values = play_Cell();   
    break;
    
  case LinkTopPG:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    // Perhaps update pgN to current value first?
    //state.pgN = ?
    link_values = play_PG();
    break;
  case LinkNextPG:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    // Perhaps update pgN to current value first?
    state.pgN += 1; // What if pgN becomes > pgc.nr_of_programs?
    link_values = play_PG();
    break;
  case LinkPrevPG:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    // Perhaps update pgN to current valu first?
    state.pgN -= 1; // What if pgN becomes < 1?
    link_values = play_PG();
    break;
  
  case LinkTopPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    exit(-1);
  case LinkNextPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    exit(-1);
  case LinkPrevPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    exit(-1);
  case LinkGoUpPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    exit(-1);
  case LinkTailPGC:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    link_values = play_PGC_post();
    break;
  
  case LinkRSM:
    if(link_values.data1 != 0)
      state.registers.SPRM[8] = link_values.data1 << 10;
    
    get_VTS_PGC(state.rsm_vtsN, state.rsm_pgcN);
    
    set_spu_palette(pgc->palette); // Erhum...
    
    state.domain = VTS_DOMAIN;
    /* These should never be set in SystemSpace and/or MenuSpace */ 
    /* state.TT_REG = rsm_tt; ?? */
    /* state.PGC_REG = state.rsm_pgcN; ?? */
    
    ifoOpenVTSI(state.rsm_vtsN);
    
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
      //state.rsm_blockN;
      link_values.command = PlayThis;
      link_values.data1 = 0;
    }
    break;
    
  case LinkPGCN:
    /* Make sure that pgcit is correct !!! */
    get_PGC(link_values.data1);
    link_values = play_PGC();
    break;
  
  case LinkPTTN:
    if(link_values.data2 != 0)
      state.registers.SPRM[8] = link_values.data2 << 10;
    exit(-1);  
  
  case LinkPGN:
    if(link_values.data2 != 0)
      state.registers.SPRM[8] = link_values.data2 << 10;
    state.pgN = link_values.data1;
    link_values = play_PG();
    break;
  
  case LinkCN:
    if(link_values.data2 != 0)
      state.registers.SPRM[8] = link_values.data2 << 10;
    state.cellN = link_values.data1;
    link_values = play_Cell();
    break;
  
  case Exit:
    exit(-1); // What should we do here??
    
  case JumpTT:
    assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
    state.domain = VTS_DOMAIN;
    get_TT(link_values.data1); // Sets all necessary state
    link_values = play_PGC();
    break;
  case JumpVTS_TT:
    assert(state.domain == VTSM_DOMAIN || state.domain == VTS_DOMAIN); //??
    state.domain = VTS_DOMAIN;
    if(get_VTS_TT(current_vts_nr, link_values.data1) == -1)
      assert(2 ==  3);
    link_values = play_PGC();
    break;    
  case JumpVTS_PTT:
    assert(state.domain == VTSM_DOMAIN); //??
    state.domain = VTS_DOMAIN;    
    get_VTS_PTT(current_vts_nr, link_values.data1, link_values.data2);
    link_values = play_PG();
    break;
    
  case JumpSS_FP:
    assert(state.domain == VMGM_DOMAIN || state.domain == VTSM_DOMAIN); //??   
    state.domain = FP_DOMAIN;
    get_FP_PGC();
    link_values = play_PGC();
    break;
  case JumpSS_VMGM_MENU:
    assert(state.domain == VMGM_DOMAIN || 
	   state.domain == VTSM_DOMAIN || 
	   state.domain == FP_DOMAIN); //??    
    state.domain = VMGM_DOMAIN;
    get_VMGM_MENU(link_values.data1);
    link_values = play_PGC();
    break;
  case JumpSS_VTSM:
    assert(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN); //??
    state.domain = VTSM_DOMAIN;   
    get_VTSM(link_values.data1, link_values.data2, link_values.data3);
    link_values = play_PGC();
    break;
  case JumpSS_VMGM_PGC:
    assert(state.domain == VMGM_DOMAIN || 
	   state.domain == VTSM_DOMAIN || 
	   state.domain == FP_DOMAIN); //??    
    state.domain = VMGM_DOMAIN;
    get_VMGM_PGC(link_values.data1);
    link_values = play_PGC();
    break;
    
  case CallSS_FP:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data1 != 0) {
      state.rsm_cellN = link_values.data1;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = ; // must be set outside (in nav.c)
    }
    state.rsm_vtsN = current_vts_nr;
    state.rsm_pgcN = get_PGCN();
    state.domain = FP_DOMAIN;
    
    get_FP_PGC();
    link_values = play_PGC();
    break;
  case CallSS_VMGM_MENU:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data2 != 0) {
      state.rsm_cellN = link_values.data2;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = ; // must be set outside (in nav.c)
    }
    state.rsm_vtsN = current_vts_nr;
    state.rsm_pgcN = get_PGCN();
    state.domain = VMGM_DOMAIN;
    
    if(get_VMGM_MENU(link_values.data1) != -1) {
      link_values = play_PGC();
    } else {
      link_values.command = LinkRSM;
      link_values.data1 = 0;
      link_values.data2 = 0;
      link_values.data3 = 0;
      // Sets link_valuse it self
    }
    break;
  case CallSS_VTSM:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data2 != 0) {
      state.rsm_cellN = link_values.data2;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = ; // must be set outside (in nav.c)
    }      
    state.rsm_vtsN = current_vts_nr;
    state.rsm_pgcN = get_PGCN();
    state.domain = VTSM_DOMAIN;
    
    if(get_VTSM(current_vts_nr, 1, link_values.data1) != -1) {
      link_values = play_PGC();
    } else {
      link_values.command = LinkRSM;
      link_values.data1 = 0;
      link_values.data2 = 0;
      link_values.data3 = 0;
      // Sets link_values it self
    }
    break;
  case CallSS_VMGM_PGC:
    assert(state.domain == VTS_DOMAIN); //??   
    if(link_values.data2 != 0) {
      state.rsm_cellN = link_values.data2;
      state.rsm_blockN = 0;
    } else {
      state.rsm_cellN = state.cellN;
      //state.rsm_blockN = ; // must be set outside (in nav.c)
    }      
    state.rsm_vtsN = current_vts_nr;
    state.rsm_pgcN = get_PGCN();
    state.domain = VMGM_DOMAIN;
    
    if(get_VMGM_PGC(link_values.data1) != -1) {
      link_values = play_PGC();
    } else {
      link_values.command = LinkRSM;
      link_values.data1 = 0;
      link_values.data2 = 0;
      link_values.data3 = 0;
      // Sets link_values it self
    }
    break;
  case PlayThis:
    /* Never happens. */
    break;
  }
  
  }
  return link_values;
}






int get_FP_PGC()
{  
  ifoOpenVMGI(); // Check for error?
  if(vmgi.first_play_pgc == NULL) {
    vmgi.first_play_pgc = malloc(sizeof(pgc_t));
    ifoRead_PGC(vmgi.first_play_pgc, vmgi.vmgi_mat->first_play_pgc);
  }
  pgc = vmgi.first_play_pgc;
  
  return 0;
}

int get_TT(int tt)
{  
  ifoOpenVMGI();
  if(vmgi.vmg_ptt_srpt == NULL) {
    vmgi.vmg_ptt_srpt = malloc(sizeof(vmg_ptt_srpt_t));
    ifoRead_VMG_PTT_SRPT(vmgi.vmg_ptt_srpt, vmgi.vmgi_mat->vmg_ptt_srpt);
  }
  assert(tt <= vmgi.vmg_ptt_srpt->nr_of_srpts);
  
  state.TTN_REG = tt; // ??
  
  // Here VTS_TTN_REG gets set also.. should it?
  return get_VTS_TT(vmgi.vmg_ptt_srpt->title_info[tt - 1].title_set_nr,
		    vmgi.vmg_ptt_srpt->title_info[tt - 1].vts_ttn);
}

int get_VTS_TT(int vtsN, int vts_ttn)
{
  int i, pgcN;

  ifoOpenVTSI(vtsN);
  
  if(vtsi.vts_pgcit == NULL) {
    vtsi.vts_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(vtsi.vts_pgcit, vtsi.vtsi_mat->vts_pgcit * DVD_BLOCK_LEN);
  }
  pgcit = vtsi.vts_pgcit;
    
  /* Get menu */
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != vts_ttn)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    return -1; // error
  }
  assert(((pgcit->pgci_srp[i].pgc_category >> 24) & 0x80) == 0x80); 
  pgcN = i + 1;

  if(get_PGC(pgcN) == -1) {
    return -1;
  }
  
  state.VTS_TTN_REG = vts_ttn; // ??
  state.PGC_REG = pgcN; // ??

  return 0;
}

int get_VTS_PTT(int vtsN, int /* is this really */ vts_ttn, int part)
{
  int pgcN, pgN;
  
  ifoOpenVTSI(vtsN);
  
  if(vtsi.vts_ptt_srpt == NULL) {
    vtsi.vts_ptt_srpt = malloc(sizeof(vts_ptt_srpt_t));
    ifoRead_VTS_PTT_SRPT(vtsi.vts_ptt_srpt, vtsi.vtsi_mat->vts_ptt_srpt);
  }
  
  assert(vts_ttn <= vtsi.vts_ptt_srpt->nr_of_srpts);
  assert(part <= vtsi.vts_ptt_srpt->title_info[vts_ttn - 1].nr_of_ptts);
  
  pgcN = vtsi.vts_ptt_srpt->title_info[vts_ttn - 1].ptt_info[part - 1].pgcn;
  pgN = vtsi.vts_ptt_srpt->title_info[vts_ttn - 1].ptt_info[part - 1].pgn;
  
  if(vtsi.vts_pgcit == NULL) {
    vtsi.vts_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(vtsi.vts_pgcit, vtsi.vtsi_mat->vts_pgcit * DVD_BLOCK_LEN);
  }
  pgcit = vtsi.vts_pgcit;
  
  if(get_PGC(pgcN) == -1) {
    return -1;
  }
  
  if(state.domain == VTS_DOMAIN) {
    state.VTS_TTN_REG = vts_ttn; // ??
    // state.TTN_REG =// ?? 
    state.PGC_REG = pgcN; // ??
  }
  state.pgN = pgN; // ??
  state.cellN = 0; // ??
  
  return 0;
}


int get_VTS_PGC(int vtsN, int pgcN)
{
  
  ifoOpenVTSI(vtsN);
  if(vtsi.vts_pgcit == NULL) {
    vtsi.vts_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(vtsi.vts_pgcit, vtsi.vtsi_mat->vts_pgcit * DVD_BLOCK_LEN);
  }
  pgcit = vtsi.vts_pgcit;
  
  return get_PGC(pgcN);
}



pgcit_t* get_VMGM_LU(char language[2])
{
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

int get_VMGM_MENU(int menu)
{
  int i;
  
  pgcit = get_VMGM_LU(lang);
  
  /* Get menu */
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != menu)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    return -1; // error
  }
  
  return get_PGC(i + 1);
}


int get_VMGM_PGC(int pgcN)
{
  pgcit = get_VMGM_LU(lang);
   
  return get_PGC(pgcN);
}

int get_VTSM(int vtsN, int title, int menu)
{
  int pgcN, i;
  
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
    return -1; // error
  }
  pgcit = vtsi.vtsm_pgci_ut->menu_lu[i].menu_pgcit;
  
  /* Get menu */
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != menu)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    return -1; // error
  }
  pgcN = i + 1;
  
  return get_PGC(pgcN);
}


int get_PGC(int pgcN)
{
  assert(pgcit != NULL);
  if(pgcN > pgcit->nr_of_pgci_srp)
    return -1; // error
  
  pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  
  return 0;
}

int get_PGCN()
{
  int pgcN = 1;

  assert(pgcit != NULL);
  
  while(pgcN <= pgcit->nr_of_pgci_srp) {
    if(pgcit->pgci_srp[pgcN - 1].pgc == pgc)
      return pgcN;
  }
  
  return -1; // error
}


/* 
   All those that set pgc and/or pgcit and might later fail must 
   undo this before exiting!!
   That or some how make sure that pgcit is correct before calling 
   get_PGC (currently only from LinkPGCN).
   
*/
