#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>

#include "../include/common.h"

#include "ifo.h"
void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat);
void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat);
void ifoRead_PGC(pgc_t *pgc, int offset);
void ifoPrint_PGC(pgc_t *pgc);
void ifoRead_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl, int offset);
void ifoPrint_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl);
void ifoRead_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt, int sector);
void ifoPrint_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt);


#define PUT(level, text...) \
if(level < debug) { \
  fprintf(stdout, ## text); \
}

#define CHECK_ZERO(arg) \
if(memcmp(my_friendly_zeros, &arg, sizeof(arg))) { \
 int i; \
 fprintf(stdout, "*** Zero check failed in %s:%i\n    for %s = 0x", \
	   __FILE__, __LINE__, # arg ); \
 for(i=0;i<sizeof(arg);i++) \
   fprintf(stdout, "%02x", *((uint8_t *)&arg + i)); \
 fprintf(stdout, "\n"); \
}


int debug = 10;
uint8_t my_friendly_zeros[2048];

int vob_mode = 0;
int ifo_mode = 0;
FILE *vob_file;
FILE *ifo_file;

char *program_name;

void usage()
{
  fprintf(stderr, "Usage: %s  [-v <vob>] [-i <ifo>]\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int c; 
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "v:i:h?")) != EOF) {
    switch (c) {
    case 'v':
      vob_file = fopen(optarg,"r");
      if(!vob_file) {
          perror(optarg);
          exit(1);
        }
      vob_mode=1;
      break;    
    case 'i':
      ifo_file = fopen(optarg,"r");
      if(!ifo_file) {
          perror(optarg);
          exit(1);
        }
      ifo_mode=1;
      break;    
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  
  if(!ifo_mode && !vob_mode) {
    usage();
    return 1;
  }
  
  if(ifo_mode) {
    uint8_t *data = malloc( DVD_BLOCK_LEN );
    vmgi_mat_t vmgi_mat;
    vtsi_mat_t vtsi_mat;
    pgc_t pgc;
    vmg_ptt_srpt_t vmg_ptt_srpt;
#if 0
#define DINFO(name) fprintf(stdout, #name " size: %i\n", sizeof(name) )
    DINFO(dvd_time_t);
    DINFO(vmgi_mat_t);
    DINFO(pgc_command_tbl_t);
    DINFO(cell_playback_tbl_t);
    DINFO(cell_position_tbl_t);
    DINFO(pgc_t);
    DINFO(title_info_t);
    DINFO(vmg_ptt_srpt_t);
    DINFO(vmgm_pgci_lu_t);
    DINFO(vmgm_pgci_ut_t);
    DINFO(ptl_mait_country_t);
    DINFO(vts_atributes_t);
    DINFO(vmg_vts_atrt_t);
    DINFO(cell_adr_t);
    DINFO(vmgm_c_adt_t);
    DINFO(vmgm_vobu_admap_t);
    fprintf(stdout, "\n\n");
#endif
    
    if(fread(data, DVD_BLOCK_LEN, 1, ifo_file) != 1) {
      exit(1);
    }
    
    if(strncmp("DVDVIDEO-VMG",data, 12) == 0) {
      memcpy(&vmgi_mat, data, sizeof(vmgi_mat));
      
      ifoPrint_VMGI_MAT(&vmgi_mat);
      
      ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
      ifoPrint_PGC(&pgc);
      
      ifoRead_VMG_PTT_SRPT(&vmg_ptt_srpt, vmgi_mat.vmg_ptt_srpt);
      ifoPrint_VMG_PTT_SRPT(&vmg_ptt_srpt);
      
      
      
      
    } else if(strncmp("DVDVIDEO-VTS",data, 12) == 0) {
      memcpy(&vtsi_mat, data, sizeof(vtsi_mat));
      ifoPrint_VTSI_MAT(&vtsi_mat);
    } 
  }
  
  if(vob_mode) {
  
  
  }
  
  return 0;
}

void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat) {
  CHECK_ZERO(vmgi_mat->zero_1);
  CHECK_ZERO(vmgi_mat->zero_2);
  CHECK_ZERO(vmgi_mat->zero_3);
  CHECK_ZERO(vmgi_mat->zero_4);
  CHECK_ZERO(vmgi_mat->zero_5);
  CHECK_ZERO(vmgi_mat->zero_6);
  CHECK_ZERO(vmgi_mat->zero_7);
  CHECK_ZERO(vmgi_mat->zero_8);
  PUT(5, "VMG Identifier: %.12s\n", vmgi_mat->vmg_identifier);
  PUT(5, "Last Sector of VMG: %08x\n", vmgi_mat->vmg_last_sector);
  PUT(5, "Last Sector of VMGI: %08x\n", vmgi_mat->vmgi_last_sector);
  PUT(5, "Specification version number: %01x.%01x\n", 
      vmgi_mat->specification_version>>4, vmgi_mat->specification_version&0xf);
  PUT(5, "VMG Category: %08x\n", vmgi_mat->vmg_category);
  PUT(5, "VMG Number of Volumes: %i\n", vmgi_mat->vmg_nr_of_volumes);
  PUT(5, "VMG This Volumes: %i\n", vmgi_mat->vmg_this_volume_nr);
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
  PUT(5, "Start sector of VMG_PTL_MAIT: %08x\n", vmgi_mat->vmg_plt_mait);
  PUT(5, "Start sector of VMG_VTS_ATRT: %08x\n", vmgi_mat->vmg_vts_atrt);
  PUT(5, "Start sector of VMG_TXTDT_MG: %08x\n", vmgi_mat->vmg_txtdt_mg);
  PUT(5, "Start sector of VMGM_C_ADT: %08x\n", vmgi_mat->vmgm_c_adt);
  PUT(5, "Start sector of VMGM_VOBU_ADMAP: %08x\n", 
      vmgi_mat->vmgm_voubu_admap);
  PUT(5, "Video attributes of VMGM_VOBS: %04x\n", 
      vmgi_mat->vmgm_video_attributes);
  PUT(5, "VMG Number of Audio attributes: %i\n", 
      vmgi_mat->nr_of_vmgm_audio_streams);
  PUT(5, "VMG Number of Sub-picture attributes: %i\n", 
      vmgi_mat->nr_of_vmgm_subp_streams);
}

void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat) {
  CHECK_ZERO(vtsi_mat->zero_1);
  CHECK_ZERO(vtsi_mat->zero_2);
  CHECK_ZERO(vtsi_mat->zero_3);
  CHECK_ZERO(vtsi_mat->zero_4);
  CHECK_ZERO(vtsi_mat->zero_5);
  CHECK_ZERO(vtsi_mat->zero_6);
  CHECK_ZERO(vtsi_mat->zero_7);
  CHECK_ZERO(vtsi_mat->zero_8);
  CHECK_ZERO(vtsi_mat->zero_9);
  CHECK_ZERO(vtsi_mat->zero_10);
  CHECK_ZERO(vtsi_mat->zero_11);
  CHECK_ZERO(vtsi_mat->zero_12);
  CHECK_ZERO(vtsi_mat->zero_13);
  CHECK_ZERO(vtsi_mat->zero_14);
  CHECK_ZERO(vtsi_mat->zero_15);
  CHECK_ZERO(vtsi_mat->zero_16);
  CHECK_ZERO(vtsi_mat->zero_17);
  PUT(5, "VTS Identifier: %.12s\n", vtsi_mat->vts_identifier);
  PUT(5, "Last Sector of VTS: %08x\n", vtsi_mat->vts_last_sector);
  PUT(5, "Last Sector of VTSI: %08x\n", vtsi_mat->vtsi_last_sector);
  PUT(5, "Specification version number: %01x.%01x\n", 
      vtsi_mat->specification_version>>4, vtsi_mat->specification_version&0xf);
  PUT(5, "VTS Category: %08x\n", vtsi_mat->vts_category);
  PUT(5, "End byte of VTSI_MAT: %08x\n", vtsi_mat->vtsi_last_byte);
  PUT(5, "Start sector of VTSM_VOBS: %08x\n", vtsi_mat->vtsm_vobs);
  PUT(5, "Start sector of VTSTT_VOBS: %08x\n", vtsi_mat->vtstt_vobs);
  PUT(5, "Start sector of VTS_PTT_SRPT: %08x\n", vtsi_mat->vts_ptt_srpt);
  PUT(5, "Start sector of VTS_PGCIT: %08x\n", vtsi_mat->vts_pgcit);
  PUT(5, "Start sector of VTSM_PGCI_UT: %08x\n", vtsi_mat->vtsm_pgci_ut);
  PUT(5, "Start sector of VTS_TMAPT: %08x\n", vtsi_mat->vts_tmapt);
  PUT(5, "Start sector of VTSM_C_ADT: %08x\n", vtsi_mat->vtsm_c_adt);
  PUT(5, "Start sector of VTSM_VOBU_ADMAP: %08x\n",vtsi_mat->vtsm_voubu_admap);
  PUT(5, "Start sector of VTS_C_ADT: %08x\n", vtsi_mat->vts_c_adt);
  PUT(5, "Start sector of VTS_VOBU_ADMAP: %08x\n", vtsi_mat->vts_voubu_admap);
  
  PUT(5, "Video attributes of VTSM_VOBS: %04x\n", 
      vtsi_mat->vtsm_video_attributes);
  PUT(5, "VTSM Number of Audio attributes: %i\n", 
      vtsi_mat->nr_of_vtsm_audio_streams);
  PUT(5, "VTSM Number of Sub-picture attributes: %i\n", 
      vtsi_mat->nr_of_vtsm_subp_streams);
  PUT(5, "Video attributes of VTS_VOBS: %04x\n", 
      vtsi_mat->vtsm_video_attributes);
  PUT(5, "VTS Number of Audio attributes: %i\n", 
      vtsi_mat->nr_of_vts_audio_streams);
  PUT(5, "VTS Number of Sub-picture attributes: %i\n", 
      vtsi_mat->nr_of_vts_subp_streams);
}

void ifoRead_PGC(pgc_t *pgc, int offset) {
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(pgc, PGC_SIZE, 1, ifo_file) != 1) {
    perror("First Play PGC");
    exit(1);
  }
  
  if(pgc->pgc_command_tbl_offset != 0) {
    pgc->pgc_command_tbl = malloc(sizeof(pgc_command_tbl_t));
    ifoRead_PGC_COMMAND_TBL(pgc->pgc_command_tbl, 
			    offset + pgc->pgc_command_tbl_offset);
  } else {
    pgc->pgc_command_tbl = NULL;
  }
    
}  


void ifoRead_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl, int offset) {
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cmd_tbl, PGC_COMMAND_TBL_SIZE, 1, ifo_file) != 1) {
    perror("PGC - Command Table");
    exit(1);
  }
  // assert(sizeof(command_data_t) == COMMAND_DATA_SIZE);
  
  if(cmd_tbl->nr_of_pre != 0) {
    int pre_cmd_size  = cmd_tbl->nr_of_pre  * sizeof(command_data_t);
    cmd_tbl->pre_commands  = malloc(pre_cmd_size);
    if(fread(cmd_tbl->pre_commands, pre_cmd_size, 
	     1, ifo_file) != 1) {
      perror("PGC - Command Table, Pre comands");
      exit(1);
    }
  }
  
  if(cmd_tbl->nr_of_post != 0) {
    int post_cmd_size = cmd_tbl->nr_of_post * sizeof(command_data_t);
    cmd_tbl->post_commands = malloc(post_cmd_size);
    if(fread(cmd_tbl->post_commands, post_cmd_size,
	     1, ifo_file) != 1) {
      perror("PGC - Command Table, Post commands");
      exit(1);
    }
  }
  
  if(cmd_tbl->nr_of_cell != 0) {
    int cell_cmd_size = cmd_tbl->nr_of_cell * sizeof(command_data_t);
    cmd_tbl->cell_commands = malloc(cell_cmd_size);
    if(fread(cmd_tbl->cell_commands, cell_cmd_size,
	     1, ifo_file) != 1) {
      perror("PGC - Command Table, Cell commands");
      exit(1);
    }
  }
}


void ifoPrint_PGC(pgc_t *pgc) {
  CHECK_ZERO(pgc->zero_1);
  
  PUT(5, "Number of Programs: %i\n", pgc->nr_of_programs);
  PUT(5, "Number of Cells: %i\n", pgc->nr_of_cells);
  PUT(5, "Playback time: %02x:%02x:%02x.%02x @ %s\n", 
      pgc->playback_time.hour,
      pgc->playback_time.minute,
      pgc->playback_time.second,
      pgc->playback_time.frame_u & 0x3f,
      pgc->playback_time.frame_u & 0xc0 ? "30fps" : "25fps");
  
  PUT(5, "Prohibited user operations: %08x\n", pgc->prohibited_ops);
  {
    int i;
    for(i=0;i<8;i++)
      PUT(5, "Audio stream %i status: %04x\n", i, pgc->audio_status[i]);
  }
  {
    int i;
    for(i=0;i<32;i++)
      PUT(5, "Sub-picture stream %2i status: %04x\n", i, pgc->subp_status[i]);
  }
  
  PUT(5, "Next PGC number: %i\n", pgc->next_pgc_nr);
  PUT(5, "Prev PGC number: %i\n", pgc->prev_pgc_nr);
  PUT(5, "GoUp PGC number: %i\n", pgc->goup_pgc_nr);
  
  PUT(5, "Still time: %i seconds (255=inf)\n", pgc->still_time);
  PUT(5, "PG Playback mode %02x\n", pgc->pg_playback_mode);
  {
    int i;
    for(i=0;i<16;i++)
      PUT(5, "Color %2i: %08x\n", i, pgc->palette[i]);
  }
  
  /* Memmory offsets to div. tables. */
  ifoPrint_PGC_COMMAND_TBL(pgc->pgc_command_tbl);
}

void ifoPrint_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl) {
  PUT(5, "Number of Pre commands: %i\n", cmd_tbl->nr_of_pre);
  {
    int i, j;
    for(i=0;i<cmd_tbl->nr_of_pre;i++) {
      for(j=0;j<8;j++) {
	PUT(5, "%02x ", cmd_tbl->pre_commands[i][j]);
      }
      PUT(5, "\n");
    }
  }
  PUT(5, "Number of Post commands: %i\n", cmd_tbl->nr_of_post);
  {
    int i, j;
    for(i=0;i<cmd_tbl->nr_of_post;i++) {
      for(j=0;j<8;j++) {
	PUT(5, "%02x ", cmd_tbl->post_commands[i][j]);
      }
      PUT(5, "\n");
    }
  }
  PUT(5, "Number of Cell commands: %i\n", cmd_tbl->nr_of_cell);
  {
    int i, j;
    for(i=0;i<cmd_tbl->nr_of_cell;i++) {
      for(j=0;j<8;j++) {
	PUT(5, "%02x ", cmd_tbl->cell_commands[i][j]);
      }
      PUT(5, "\n");
    }
  }
}

void ifoRead_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt, int sector) {
  int info_length;
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vmg_ptt_srpt, VMG_PTT_SRPT_SIZE, 1, ifo_file) != 1) {
    perror("VMG_PTT_SRPT");
    exit(1);
  }
  info_length = vmg_ptt_srpt->last_byte + 1 - VMG_PTT_SRPT_SIZE;
  vmg_ptt_srpt->title_info = malloc(info_length); 
  //assert(info_length >= vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t));
  if(fread(vmg_ptt_srpt->title_info, info_length, 1, ifo_file) != 1) {
    perror("VMG_PTT_SRPT");
    exit(1);
  }
  
}

void ifoPrint_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt) {
  CHECK_ZERO(vmg_ptt_srpt->zero_1);
  PUT(5, "Number of PartofTitle search pointers: %i\n", vmg_ptt_srpt->nr_of_srpts);
  {
    int i;
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
      PUT(5, "\tParental ID field: %04x\n", 
	  vmg_ptt_srpt->title_info[i].parental_id);
      PUT(5, "\tTitle set starting sector %08x\n", 
	  vmg_ptt_srpt->title_info[i].title_set_sector);
    }
  }
}

