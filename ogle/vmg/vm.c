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

#include <ogle/msgevents.h>
#include "common.h"

#include "ifo.h"
#include "ifo_read.h"
#include "ifo_print.h"
#include "decoder.h"
#include "vm.h"

extern void do_run(void); // nav.c 
extern void set_spu_palette(uint32_t palette[16]); // nav.c
extern void wait_for_init(MsgEventQ_t *msgq); // com.c
extern int send_demux(MsgEventQ_t *msgq, MsgEvent_t *ev); // com.c

static int msgqid = -1;

MsgEventQ_t *msgq;


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
FILE *vmg_file;
FILE *vts_file;


dvd_state_t state;



// Should this be some where in the state, is it allready?
char lang[2] = "en";

int debug = 8;



//int start_vm(void);
int reset_vm(void);
int run_vm(void);
int eval_cmd(vm_cmd_t *cmd);
int get_next_cell();

static link_t play_PGC(void);
static link_t play_PG(void);
static link_t play_Cell(void);
static link_t play_Cell_post(void);
static link_t play_PGC_post(void);
static link_t process_command(link_t link_values);

static void ifoOpenVMGI(void);
static void ifoOpenVTSI(int vtsN);

static pgcit_t* get_VTS_PGCIT(int vtsN);
static pgcit_t* get_VTSM_PGCIT(int vtsN, char language[2]);
static pgcit_t* get_VMGM_PGCIT(char language[2]);
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





char *program_name;

void usage(void)
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
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xe0; // Mpeg2 Video 
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "vm: didn't set demuxstream\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // AC3 1
    ev.demuxstream.subtype = 0x80;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "vm: didn't set demuxstream\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // SPU 1
    ev.demuxstream.subtype = 0x20;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "vm: didn't set demuxstream\n");
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbf; // NAV
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      fprintf(stderr, "vm: didn't set demuxstream\n");
    }
    
    
  } else {
    fprintf(stderr, "what?\n");
  }
  
  /*  Call start here */
  reset_vm();

  do_run();
  
  return -1;
}





int reset_vm(void)
{ 
  // Setup State
  memset(state.registers.SPRM, 0, sizeof(uint16_t)*24);
  memset(state.registers.GPRM, 0, sizeof(state.registers.GPRM));
  state.registers.SPRM[0] = ('U'<<8)|'S'; // Player Menu Languange code
  state.AST_REG = 15;
  state.SPST_REG = 0; // 62 why?
  state.AGL_REG = 1;
  state.TTN_REG = 1;
  state.VTS_TTN_REG = 1;
  //state.PGC_REG = 0
  state.PTT_REG = 1;
  state.HL_BTNN_REG = 1 << 10;

  state.PTL_REG = 15;           // Parental Level
  state.registers.SPRM[20] = 1; // Player Regional Code
  
  state.pgN = 0;
  state.cellN = 0;

  state.domain = FP_DOMAIN;
  state.rsm_vtsN = 0;
  //state.rsm_pgcN = 0;
  state.rsm_cellN = 0;
  state.rsm_blockN = 0;
  
  state.vtsN = -1;
  ifoOpenVMGI(); // Check for error
  
  return 0;
}




// FIXME TODO XXX $$$ Handle error condition too...
int start_vm(void)
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
    state.blockN = link_values.data1;
    return 1; // Something changed
  } else {
    return 0; // It updated some state thats all...
  }
}

// FIXME TODO XXX $$$ Handle error condition too...
// How to tell if there is any need to do a Flush?
int get_next_cell(void)
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


// Must be called before domain is changed 
void saveRMSinfo(int cellN, int blockN)
{
  if(cellN != 0) {
    state.rsm_cellN = cellN;
    state.rsm_blockN = 0;
  } else {
    state.rsm_cellN = state.cellN;
    state.rsm_blockN = blockN;
  }
  state.rsm_vtsN = state.vtsN;
  state.rsm_pgcN = get_PGCN();
}

domain_t menuid2domain(DVDMenuID_t menuid)
{
  domain_t result;
  
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
  default:
    /* Handle error ? */
  }
  
  return result;
}


int vm_menuCall(DVDMenuID_t menuid, int block)
{
  domain_t old_domain;
  link_t link_values;
  
  /* Should check if we are allowed/can acces this menu */
  
  
  /* FIXME XXX $$$ How much state needs to be restored 
   * when we fail to find a menu? */
  
  old_domain = state.domain;
  
  switch(state.domain) {
  case VTS_DOMAIN:
    saveRMSinfo(0, block);
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
      return 1;
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
  link_t link_values;
  
  // Check and see if there is any rsm info!!
  if(state.rsm_vtsN == 0) {
    return 0;
  }
  
  state.domain = VTS_DOMAIN;
  ifoOpenVTSI(state.rsm_vtsN);
  get_PGC(state.rsm_pgcN);
  
  set_spu_palette(state.pgc->palette); // Erhum...
  
  /* These should never be set in SystemSpace and/or MenuSpace */ 
  // state.TT_REG = rsm_tt;
  // state.PGC_REG = state.rsm_pgcN;
  
  //state.HL_BTNN_REG = rsm_btnn;
  
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
  
  return 1;
}

int get_Audio_stream(int audioN)
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

int get_Spu_stream(int spuN)
{
  int streamN = -1;
  int source_aspect = get_video_aspect();
  
  if(state.domain == VTSM_DOMAIN 
     || state.domain == VMGM_DOMAIN
     || state.domain == FP_DOMAIN) {
    spuN = 0;
  }
  
  if(spuN < 32) { /* a valid logical stream */
    /* Is this logical stream present */ 
    if(state.pgc->subp_control[spuN] & (1<<31)) {
      if(source_aspect == 0) /* 4:3 */	     
	streamN = (state.pgc->subp_control[spuN] >> 24) & 0x1f;  
      if(source_aspect == 3) /* 16:9 */
	streamN = (state.pgc->subp_control[spuN] >> 16) & 0x1f;
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

int get_Spu_active_stream(void)
{
  int spuN = state.SPST_REG;
  
  if(state.domain == VTS_DOMAIN && !(spuN & 0x40)) { /* Is the spu off? */
    return -1;
  } else {
    return get_Spu_stream(spuN & ~0x40);
  }
}

void get_Audio_info(int *num_avail, int *current)
{
  if(state.domain == VTS_DOMAIN) {
    ifoOpenVTSI(state.vtsN);
    *num_avail = vtsi.vtsi_mat->nr_of_vts_audio_streams;
  } else if(state.domain == VTSM_DOMAIN) {
    ifoOpenVTSI(state.vtsN);
    *num_avail = vtsi.vtsi_mat->nr_of_vtsm_audio_streams;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    ifoOpenVMGI();
    *num_avail = vmgi.vmgi_mat->nr_of_vmgm_audio_streams;
  }
  *current = state.AST_REG;
}

void get_Spu_info(int *num_avail, int *current)
{
  if(state.domain == VTS_DOMAIN) {
    ifoOpenVTSI(state.vtsN);
    *num_avail = vtsi.vtsi_mat->nr_of_vts_subp_streams;
  } else if(state.domain == VTSM_DOMAIN) {
    ifoOpenVTSI(state.vtsN);
    *num_avail = vtsi.vtsi_mat->nr_of_vtsm_subp_streams;
  } else if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN) {
    ifoOpenVMGI();
    *num_avail = vmgi.vmgi_mat->nr_of_vmgm_subp_streams;
  }
  *current = state.SPST_REG;
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
      ifoPrint_COMMAND(i, &state.pgc->pgc_command_tbl->pre_commands[i]);
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
    break;
  case 1: // The first cell in the block
    switch(state.pgc->cell_playback_tbl[state.cellN - 1].block_type) {
    case 0: // Not part of a block
      assert(0);
    case 1: // Angle block
      /* Loop and check each cell instead? */
      state.cellN += state.AGL_REG - 1;
      assert(state.domain == VTSM_DOMAIN); // ??
      assert(state.cellN <= state.pgc->nr_of_cells);
      assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_mode != 0);
      assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_type == 1);
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
	    state.pgc->cell_playback_tbl[state.cellN - 1].block_mode,
	    state.pgc->cell_playback_tbl[state.cellN - 1].block_type);
  }

  
  /* Figure out the correct pgN for this cell and update the state. */ 
  {
    int i = 0; 
    while(i <= state.pgc->nr_of_programs 
	  && state.cellN >= state.pgc->pgc_program_map[i])
      i++;
    state.pgN = i;
  }
  
  
  {
    link_t tmp = {PlayThis, /* Block in Cell */ 0, 0, 0};
    return tmp;
  }
  
}


static link_t play_Cell_post(void)
{
  cell_playback_t *cell;
    
  cell = &state.pgc->cell_playback_tbl[state.cellN - 1];
  
  /* Still time is already taken care of before we get called. */
  
  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;
    
    assert(state.pgc->pgc_command_tbl != NULL);
    assert(state.pgc->pgc_command_tbl->nr_of_cell >= cell->cell_cmd_nr);
    ifoPrint_COMMAND(0, &state.pgc->pgc_command_tbl->cell_commands[cell->cell_cmd_nr - 1]);
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
    state.cellN++;
    break;
  case 1: // The first cell in the block
  case 2: // A cell in the block
  case 3: // The last cell in the block
  default:
    assert(state.pgc->cell_playback_tbl[state.cellN - 1].block_type == 1);
    /* Skip the 'other' angles */
    state.cellN++;
    while(state.pgc->cell_playback_tbl[state.cellN - 1].block_mode >= 2) {
      state.cellN++;
    }
    break;
  }
  
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
      ifoPrint_COMMAND(i, &state.pgc->pgc_command_tbl->post_commands[i]);
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
      // Check and see if there is any rsm info!!
      state.domain = VTS_DOMAIN;
      ifoOpenVTSI(state.rsm_vtsN);
      get_PGC(state.rsm_pgcN);
      
      set_spu_palette(state.pgc->palette); // Erhum...
      
      /* These should never be set in SystemSpace and/or MenuSpace */ 
      /* state.TT_REG = rsm_tt; ?? */
      /* state.PGC_REG = state.rsm_pgcN; ?? */
      
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
      assert(state.domain == VTSM_DOMAIN); //??
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
      ifoOpenVTSI(link_values.data1); //state.vtsN = link_values.data1;
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
      if(link_values.data1 != 0) {
	state.rsm_cellN = link_values.data1;
	state.rsm_blockN = 0;
      } else {
	state.rsm_cellN = state.cellN;
	//state.rsm_blockN = ; // must be set outside (i.e in nav.c)
      }
      state.rsm_vtsN = state.vtsN;
      state.rsm_pgcN = get_PGCN(); // Must be called before domain is changed
    
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
	//state.rsm_blockN = ; // must be set outside (i.e in nav.c)
      }
      state.rsm_vtsN = state.vtsN;
      state.rsm_pgcN = get_PGCN(); // Must be called before domain is changed
      
      state.domain = VMGM_DOMAIN;
      if(get_MENU(link_values.data1) != -1) {
	link_values = play_PGC();
      } else {
	link_values.command = LinkRSM;
	link_values.data1 = 0;
	link_values.data2 = 0;
	link_values.data3 = 0;
      }
      break;
    case CallSS_VTSM:
      assert(state.domain == VTS_DOMAIN); //??   
      if(link_values.data2 != 0) {
	state.rsm_cellN = link_values.data2;
	state.rsm_blockN = 0;
      } else {
	state.rsm_cellN = state.cellN;
	//state.rsm_blockN = ; // must be set outside (i.e in nav.c)
      }      
      state.rsm_vtsN = state.vtsN;
      state.rsm_pgcN = get_PGCN(); // Must be called before domain is changed
      
      state.domain = VTSM_DOMAIN;
      if(get_MENU(link_values.data1) != -1) {
	link_values = play_PGC();
      } else {
	link_values.command = LinkRSM;
	link_values.data1 = 0;
	link_values.data2 = 0;
	link_values.data3 = 0;
      }
      break;
    case CallSS_VMGM_PGC:
      assert(state.domain == VTS_DOMAIN); //??   
      if(link_values.data2 != 0) {
	state.rsm_cellN = link_values.data2;
	state.rsm_blockN = 0;
      } else {
	state.rsm_cellN = state.cellN;
	//state.rsm_blockN = ; // must be set outside (i.e in nav.c)
      }      
      state.rsm_vtsN = state.vtsN;
      state.rsm_pgcN = get_PGCN(); // Must be called before domain is changed
      
      state.domain = VMGM_DOMAIN;
      if(get_PGC(link_values.data1) != -1) {
	link_values = play_PGC();
      } else {
	link_values.command = LinkRSM;
	link_values.data1 = 0;
	link_values.data2 = 0;
	link_values.data3 = 0;
      }
      break;
    case PlayThis:
      /* Never happens. */
      break;
    }
    
  }
  return link_values;
}








static int get_TT(int tt)
{  
  ifoOpenVMGI();
  if(vmgi.vmg_ptt_srpt == NULL) {
    vmgi.vmg_ptt_srpt = malloc(sizeof(vmg_ptt_srpt_t));
    ifoRead_VMG_PTT_SRPT(vmgi.vmg_ptt_srpt, vmgi.vmgi_mat->vmg_ptt_srpt);
  }
  assert(tt <= vmgi.vmg_ptt_srpt->nr_of_srpts);
  
  state.TTN_REG = tt;
  
  return get_VTS_TT(vmgi.vmg_ptt_srpt->title_info[tt - 1].title_set_nr,
		    vmgi.vmg_ptt_srpt->title_info[tt - 1].vts_ttn);
}


static int get_VTS_TT(int vtsN, int vts_ttn)
{
  int pgcN;
  
  state.domain = VTS_DOMAIN;
  ifoOpenVTSI(vtsN); // Also sets state.vtsN
  
  pgcN = get_ID(vts_ttn); // This might return -1
  assert(pgcN != -1);
  
  state.VTS_TTN_REG = vts_ttn;
  state.PGC_REG = pgcN; // ??
  /* Any other registers? */
  
  return get_PGC(pgcN);
}


static int get_VTS_PTT(int vtsN, int /* is this really */ vts_ttn, int part)
{
  int pgcN, pgN;
  
  state.domain = VTS_DOMAIN;
  ifoOpenVTSI(vtsN);  
  if(vtsi.vts_ptt_srpt == NULL) {
    vtsi.vts_ptt_srpt = malloc(sizeof(vts_ptt_srpt_t));
    ifoRead_VTS_PTT_SRPT(vtsi.vts_ptt_srpt, vtsi.vtsi_mat->vts_ptt_srpt);
  }
  
  assert(vts_ttn <= vtsi.vts_ptt_srpt->nr_of_srpts);
  assert(part <= vtsi.vts_ptt_srpt->title_info[vts_ttn - 1].nr_of_ptts);
  
  pgcN = vtsi.vts_ptt_srpt->title_info[vts_ttn - 1].ptt_info[part - 1].pgcn;
  pgN = vtsi.vts_ptt_srpt->title_info[vts_ttn - 1].ptt_info[part - 1].pgn;
  
  //state.TTN_REG = ?? 
  state.VTS_TTN_REG = vts_ttn;
  state.PGC_REG = pgcN; // ??
  state.PTT_REG = part; // ?? part or PG ? does it matter?
  
  state.pgN = pgN; // ??
  
  return get_PGC(pgcN);
}



static int get_FP_PGC(void)
{  
  state.domain = FP_DOMAIN;
  ifoOpenVMGI();
  if(vmgi.first_play_pgc == NULL) {
    vmgi.first_play_pgc = malloc(sizeof(pgc_t));
    ifoRead_PGC(vmgi.first_play_pgc, vmgi.vmgi_mat->first_play_pgc);
  }
  state.pgc = vmgi.first_play_pgc;
  
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
  i = 0;
  while(i < pgcit->nr_of_pgci_srp && 
	((pgcit->pgci_srp[i].pgc_category >> 24) & 0x7f) != id)
    i++;
  if(i == pgcit->nr_of_pgci_srp) {
    printf("** No such menu\n");
    return -1; // error
  }
  assert(((pgcit->pgci_srp[i].pgc_category >> 24) & 0x80) == 0x80); 
  pgcN = i + 1;
  
  return pgcN;
}



static int get_PGC(int pgcN)
{
  pgcit_t *pgcit;
  
  pgcit = get_PGCIT();
  
  assert(pgcit != NULL);
  if(0 < pgcN && pgcN > pgcit->nr_of_pgci_srp)
    return -1; // error
  
  //state.pgcN = pgcN;
  state.pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  
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
  }
  
  return -1; // error
}


static int get_video_aspect(void)
{
  int aspect = 0;
  
  if(state.domain == VTS_DOMAIN) {
    ifoOpenVTSI(state.vtsN);
    aspect = vtsi.vtsi_mat->vts_video_attributes.display_aspect_ratio;  
  } else if(state.domain == VTSM_DOMAIN) {
    ifoOpenVTSI(state.vtsN);
    aspect = vtsi.vtsi_mat->vtsm_video_attributes.display_aspect_ratio;
  } else if(state.domain == VMGM_DOMAIN) {
    ifoOpenVMGI();
    aspect = vmgi.vmgi_mat->vmgm_video_attributes.display_aspect_ratio;
  }
  assert(aspect == 0 || aspect == 3);
  state.registers.SPRM[14] &= ~(0x3 << 10);
  state.registers.SPRM[14] |= aspect << 10;
  
  return aspect;
}



/* 
   All those that set pgc and/or pgcit and might later fail must 
   undo this before exiting!!
   That or some how make sure that pgcit is correct before calling 
   get_PGC (currently only from LinkPGCN).
   
*/








static void ifoOpenVMGI(void) 
{
  if(vmg_file == NULL) {
    vmgi.vmgi_mat = malloc(sizeof(vmgi_mat_t));
    vmg_file = ifoOpen_VMG(vmgi.vmgi_mat, "VIDEO_TS.IFO");
  }
  ifo_file = vmg_file;
}


static void ifoOpenVTSI(int vtsN) 
{
  if(vtsN != state.vtsN) {
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
    state.vtsN = vtsN;
  }
  ifo_file = vts_file;
}




static pgcit_t* get_VTS_PGCIT(int vtsN)
{
  ifoOpenVTSI(vtsN);
  if(vtsi.vts_pgcit == NULL) {
    vtsi.vts_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(vtsi.vts_pgcit, vtsi.vtsi_mat->vts_pgcit * DVD_BLOCK_LEN);
  }
  
  return vtsi.vts_pgcit;
}

static pgcit_t* get_VTSM_PGCIT(int vtsN, char language[2])
{
  int i;
  
  ifoOpenVTSI(vtsN);
  if(vtsi.vtsm_pgci_ut == NULL) {
    vtsi.vtsm_pgci_ut = malloc(sizeof(menu_pgci_ut_t));
    ifoRead_MENU_PGCI_UT(vtsi.vtsm_pgci_ut, vtsi.vtsi_mat->vtsm_pgci_ut);
  }
  
  i = 0;
  while(i < vtsi.vtsm_pgci_ut->nr_of_lang_units && 
	memcmp(&vtsi.vtsm_pgci_ut->menu_lu[i].lang_code, language, 2))
    i++;
  if(i == vtsi.vtsm_pgci_ut->nr_of_lang_units) {
    printf("** Wrong language\n");
    i = 0; // error?
  }
  
  return vtsi.vtsm_pgci_ut->menu_lu[i].menu_pgcit;
}

static pgcit_t* get_VMGM_PGCIT(char language[2])
{
  int i;
  
  ifoOpenVMGI();
  if(vmgi.vmgm_pgci_ut == NULL) {
    vmgi.vmgm_pgci_ut = malloc(sizeof(menu_pgci_ut_t));
    ifoRead_MENU_PGCI_UT(vmgi.vmgm_pgci_ut, vmgi.vmgi_mat->vmgm_pgci_ut);
  }
  
  i = 0;
  while(i < vmgi.vmgm_pgci_ut->nr_of_lang_units && 
	memcmp(&vmgi.vmgm_pgci_ut->menu_lu[i].lang_code, language, 2))
    i++;
  if(i == vmgi.vmgm_pgci_ut->nr_of_lang_units) {
    printf("** Wrong language\n");
    i = 0; // error?
  }
  
  return vmgi.vmgm_pgci_ut->menu_lu[i].menu_pgcit;
}

/* Uses state to decide what to return */
static pgcit_t* get_PGCIT(void) {
  pgcit_t *pgcit;
  
  if(state.domain == VTS_DOMAIN) {
    pgcit = get_VTS_PGCIT(state.vtsN);
  } else if(state.domain == VTSM_DOMAIN) {
    pgcit = get_VTSM_PGCIT(state.vtsN, /* XXX */ lang);
  } else if(state.domain == VMGM_DOMAIN) {
    pgcit = get_VMGM_PGCIT( /* XXX */ lang);
  } else {
    pgcit = NULL;    /* Should never hapen */
  }
  
  return pgcit;
}
