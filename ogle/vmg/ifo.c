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
void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat);
void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat);

void ifoRead_PGC(pgc_t *pgc, int offset);
void ifoPrint_PGC(pgc_t *pgc);

void ifoRead_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt, int sector);
void ifoPrint_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt);

void ifoRead_VMGM_PGCI_UT(vmgm_pgci_ut_t *vmgm_pgci_ut, int sector);
void ifoPrint_VMGM_PGCI_UT(vmgm_pgci_ut_t *vmgm_pgci_ut);

void ifoRead_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait, int sector);
void ifoPrint_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait);

void ifoRead_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt, int sector);
void ifoPrint_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt);

void ifoRead_C_ADT(c_adt_t *c_adt, int sector);
void ifoPrint_C_ADT(c_adt_t *c_adt);

void ifoRead_VOBU_ADMAP(vobu_admap_t *vobu_admap, int sector);
void ifoPrint_VOBU_ADMAP(vobu_admap_t *vobu_admap);



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

void hexdump(uint8_t *ptr, int len) {
  while(len--)
    fprintf(stdout, "%02x ", *ptr++);
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
    pgc_t pgc;
    
    if(fread(data, DVD_BLOCK_LEN, 1, ifo_file) != 1) {
      exit(1);
    }
    
    if(strncmp("DVDVIDEO-VMG",data, 12) == 0) {
      vmgi_mat_t vmgi_mat;
      vmg_ptt_srpt_t vmg_ptt_srpt;
      vmgm_pgci_ut_t vmgm_pgci_ut;
      vmg_ptl_mait_t vmg_ptl_mait;
      vmg_vts_atrt_t vmg_vts_atrt;
      c_adt_t vmgm_c_adt;
      vobu_admap_t vmgm_vobu_admap;
     
      PUT(1, "VMG top level\n-------------\n");
      memcpy(&vmgi_mat, data, sizeof(vmgi_mat));
      ifoPrint_VMGI_MAT(&vmgi_mat);
      
      
      PUT(1, "\nFirst Play PGC\n--------------\n");
      ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
      ifoPrint_PGC(&pgc);
      
      PUT(1, "\nPart of Title (and Chapter) Search pointer table\n");
      PUT(1,   "------------------------------------------------\n");
      /* This needs to be in a IFO? */
      if(vmgi_mat.vmg_ptt_srpt != 0) {
	ifoRead_VMG_PTT_SRPT(&vmg_ptt_srpt, vmgi_mat.vmg_ptt_srpt);
	ifoPrint_VMG_PTT_SRPT(&vmg_ptt_srpt);
      } else
	PUT(5, "No PartOfTitle Searchpointer information present\n");
      
      PUT(1, "\nMenu PGCI Unit table\n");
      PUT(1,   "--------------------\n");
      if(vmgi_mat.vmgm_pgci_ut != 0) {
	ifoRead_VMGM_PGCI_UT(&vmgm_pgci_ut, vmgi_mat.vmgm_pgci_ut);
	ifoPrint_VMGM_PGCI_UT(&vmgm_pgci_ut);
      } else
	PUT(5, "No PGCI Unit table present\n");
       
      
      PUT(1, "\nParental Manegment Information table\n");
      PUT(1,   "------------------------------------\n");
      if(vmgi_mat.vmg_ptl_mait != 0) {
	ifoRead_VMG_PTL_MAIT(&vmg_ptl_mait, vmgi_mat.vmg_ptl_mait);
	ifoPrint_VMG_PTL_MAIT(&vmg_ptl_mait);
      } else
	PUT(5, "No Parental Management information present\n");
      
      PUT(1, "\nVideo Title Set Attribute table\n");
      PUT(1,   "-------------------------------\n");
      if(vmgi_mat.vmg_ptl_mait != 0) {
	ifoRead_VMG_VTS_ATRT(&vmg_vts_atrt, vmgi_mat.vmg_vts_atrt);
	ifoPrint_VMG_VTS_ATRT(&vmg_vts_atrt);
      } else
	PUT(5, "No Video Title Set Attribute table present\n");
      
      
      PUT(1, "\nCell Adress table\n");
      PUT(1,   "-----------------\n");
      if(vmgi_mat.vmgm_c_adt != 0) {
	ifoRead_C_ADT(&vmgm_c_adt, vmgi_mat.vmgm_c_adt);
	//ifoPrint_C_ADT(&vmgm_c_adt);
      } else
	PUT(5, "No Cell Adress table present\n");
      
      PUT(1, "\nVideo Title set Menu VOBU address map\n");
      PUT(1,   "-----------------\n");
      if(vmgi_mat.vmgm_vobu_admap != 0) {
	ifoRead_VOBU_ADMAP(&vmgm_vobu_admap, vmgi_mat.vmgm_vobu_admap);
	//ifoPrint_VOBU_ADMAP(&vmgm_vobu_admap);
      } else
	PUT(5, "No Menu VOBU address map present\n");
      
    } else if(strncmp("DVDVIDEO-VTS",data, 12) == 0) {
      vtsi_mat_t vtsi_mat;
      c_adt_t vtsm_c_adt;
      vobu_admap_t vtsm_vobu_admap;
      
      PUT(1, "VTS top level\n-------------\n");
      memcpy(&vtsi_mat, data, sizeof(vtsi_mat));
      ifoPrint_VTSI_MAT(&vtsi_mat);
      
      
      
      PUT(1, "\nCell Adress table\n");
      PUT(1,   "-----------------\n");
      if(vtsi_mat.vtsm_c_adt != 0) {
	ifoRead_C_ADT(&vtsm_c_adt, vtsi_mat.vtsm_c_adt);
	ifoPrint_C_ADT(&vtsm_c_adt);
      } else
	PUT(5, "No Cell Adress table present\n");
      
      PUT(1, "\nVideo Title set Menu VOBU address map\n");
      PUT(1,   "-----------------\n");
      if(vtsi_mat.vtsm_vobu_admap != 0) {
	ifoRead_VOBU_ADMAP(&vtsm_vobu_admap, vtsi_mat.vtsm_vobu_admap);
	ifoPrint_VOBU_ADMAP(&vtsm_vobu_admap);
      } else
	PUT(5, "No Menu VOBU address map present\n");
    } 
  }
  
  if(vob_mode) {
  
  }

  return 0;
}

void ifoPrint_time(int level, dvd_time_t *time) {
  PUT(level, "%02x:%02x:%02x.%02x @ %s", 
      time->hour,
      time->minute,
      time->second,
      time->frame_u & 0x3f,
      time->frame_u & 0xc0 ? "30fps" : "25fps");
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
  /* Byte 2 of 'VMG Category' (00xx0000) is the Region Code */
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
  PUT(5, "Start sector of VMG_PTL_MAIT: %08x\n", vmgi_mat->vmg_ptl_mait);
  PUT(5, "Start sector of VMG_VTS_ATRT: %08x\n", vmgi_mat->vmg_vts_atrt);
  PUT(5, "Start sector of VMG_TXTDT_MG: %08x\n", vmgi_mat->vmg_txtdt_mg);
  PUT(5, "Start sector of VMGM_C_ADT: %08x\n", vmgi_mat->vmgm_c_adt);
  PUT(5, "Start sector of VMGM_VOBU_ADMAP: %08x\n", 
      vmgi_mat->vmgm_vobu_admap);
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
  PUT(5, "Start sector of VTSM_VOBU_ADMAP: %08x\n",vtsi_mat->vtsm_vobu_admap);
  PUT(5, "Start sector of VTS_C_ADT: %08x\n", vtsi_mat->vts_c_adt);
  PUT(5, "Start sector of VTS_VOBU_ADMAP: %08x\n", vtsi_mat->vts_vobu_admap);
  
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

void ifoRead_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl, int offset) {
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cmd_tbl, PGC_COMMAND_TBL_SIZE, 1, ifo_file) != 1) {
    perror("PGC - Command Table");
    exit(1);
  }
  assert(sizeof(command_data_t) == COMMAND_DATA_SIZE);
  
  if(cmd_tbl->nr_of_pre != 0) {
    int pre_cmd_size  = cmd_tbl->nr_of_pre * sizeof(command_data_t);
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

void ifoRead_PGC_PROGRAM_MAP(pgc_progam_map_t *progam_map, int nr, int offset){
  int size = nr * sizeof(pgc_progam_map_t);
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(progam_map, size, 1, ifo_file) != 1) {
    perror("PGC - Program Map");
    exit(1);
  }
}

void ifoRead_CELL_PLAYBACK_TBL(cell_playback_tbl_t *cell_playback, int nr, int offset) {
  int size = nr * sizeof(cell_playback_tbl_t);
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cell_playback, size, 1, ifo_file) != 1) {
    perror("PGC - Cell Playback info");
    exit(1);
  }
}

void ifoRead_CELL_POSITION_TBL(cell_position_tbl_t *cell_position, int nr, int offset) {
  int size = nr * sizeof(cell_position_tbl_t);
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cell_position, size, 1, ifo_file) != 1) {
    perror("PGC - Cell Position info");
    exit(1);
  }
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
  
  if(pgc->pgc_program_map_offset != 0) {
    assert(pgc->nr_of_programs != 0);
    pgc->pgc_progam_map = malloc(pgc->nr_of_programs * 
				 sizeof(pgc_progam_map_t));
    ifoRead_PGC_PROGRAM_MAP(pgc->pgc_progam_map, pgc->nr_of_programs, 
			    offset + pgc->pgc_program_map_offset);
  } else {
    assert(pgc->nr_of_programs == 0);
    pgc->pgc_progam_map = NULL;
  }
  
  assert(pgc->nr_of_cells == 0 
	 ? pgc->cell_playback_tbl_offset == 0 && 
	   pgc->cell_position_tbl_offset == 0
	 : pgc->cell_playback_tbl_offset != 0 && 
	   pgc->cell_position_tbl_offset != 0);

  if(pgc->cell_playback_tbl_offset != 0) {
    pgc->cell_playback_tbl = malloc(pgc->nr_of_cells * 
				    sizeof(cell_playback_tbl_t));
    ifoRead_CELL_PLAYBACK_TBL(pgc->cell_playback_tbl, pgc->nr_of_cells,
			      offset + pgc->cell_playback_tbl_offset);
  } else {
    pgc->cell_playback_tbl = NULL;
  }
  
  if(pgc->cell_position_tbl_offset != 0) {
    pgc->cell_position_tbl = malloc(pgc->nr_of_cells * 
				    sizeof(cell_position_tbl_t));
    ifoRead_CELL_POSITION_TBL(pgc->cell_position_tbl, pgc->nr_of_cells, 
			      offset + pgc->cell_position_tbl_offset);
  } else {
    pgc->cell_position_tbl = NULL;
  }  
}  

void ifoPrint_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl) {
  if(cmd_tbl == NULL) {
    PUT(5, "No Command table present\n");
    return;
  }
  
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

void ifoPrint_PGC_PROGRAM_MAP(pgc_progam_map_t *progam_map, int nr) {
  int i;
  if(progam_map == NULL) {
    PUT(5, "No Program map present\n");
    return;
  }
  for(i=0;i<nr;i++) {
    PUT(5, "Program %3i Entry Cell: %3i\n", i, progam_map[i]);
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
    PUT(5, "Cell: %3i category %08x ", i, cell_playback[i].category);
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
    PUT(5, "Cell: %3i has VOB ID: %3i, Cell ID: %3i\n", i, 
	cell_position[i].vob_id_nr, cell_position[i].cell_nr);
    CHECK_ZERO(cell_position[i].zero_1);
  }
}


void ifoPrint_PGC(pgc_t *pgc) {
  CHECK_ZERO(pgc->zero_1);
  
  PUT(5, "Number of Programs: %i\n", pgc->nr_of_programs);
  PUT(5, "Number of Cells: %i\n", pgc->nr_of_cells);
  assert(pgc->nr_of_cells <= pgc->nr_of_programs);
  /* Check that time is 0:0:0:0 also if nr_of_programs==0 */
  PUT(5, "Playback time: ");
  ifoPrint_time(5, &pgc->playback_time); PUT(5, "\n");
  /* If no programs/no time then does this mean anything? */
  PUT(5, "Prohibited user operations: %08x\n", pgc->prohibited_ops);
  {
    int i;
    for(i=0;i<8;i++)
      if(pgc->audio_status[i] & 0x8000) { /* The 'is present' bit */
	PUT(5, "Audio stream %i status: %04x\n", i, pgc->audio_status[i]);
      } else {
	CHECK_ZERO(pgc->audio_status[i]);
      }
  }
  {
    int i;
    for(i=0;i<32;i++)
      if(pgc->subp_status[i] & 0x80000000) { /* The 'is present' bit */
	PUT(5, "Subpicture stream %2i status: %08x\n", i, pgc->subp_status[i]);
      } else { 
	CHECK_ZERO(pgc->subp_status[i]);
      }
  }
  
  if(pgc->nr_of_programs != 0) {
    PUT(5, "Next PGC number: %i\n", pgc->next_pgc_nr);
    PUT(5, "Prev PGC number: %i\n", pgc->prev_pgc_nr);
    PUT(5, "GoUp PGC number: %i\n", pgc->goup_pgc_nr);
    
    PUT(5, "Still time: %i seconds (255=inf)\n", pgc->still_time);
    PUT(5, "PG Playback mode %02x\n", pgc->pg_playback_mode);
  } else {
    CHECK_ZERO(pgc->next_pgc_nr);
    CHECK_ZERO(pgc->prev_pgc_nr);
    CHECK_ZERO(pgc->goup_pgc_nr);
    
    CHECK_ZERO(pgc->still_time);
    CHECK_ZERO(pgc->pg_playback_mode);
  }
  
  {
    int i;
    for(i=0;i<16;i++)
      if(pgc->nr_of_cells != 0) {
	PUT(5, "Color %2i: %08x\n", i, pgc->palette[i]);
      } else {
	CHECK_ZERO(pgc->palette[i]); /* Migth no be true */
      }
  }
  
  /* Memmory offsets to div. tables. */
  ifoPrint_PGC_COMMAND_TBL(pgc->pgc_command_tbl);
  ifoPrint_PGC_PROGRAM_MAP(pgc->pgc_progam_map, pgc->nr_of_programs);
  ifoPrint_CELL_PLAYBACK_TBL(pgc->cell_playback_tbl, pgc->nr_of_cells);
  ifoPrint_CELL_POSITION_TBL(pgc->cell_position_tbl, pgc->nr_of_cells);
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
  assert(info_length >= vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t));
  if(fread(vmg_ptt_srpt->title_info, info_length, 1, ifo_file) != 1) {
    perror("VMG_PTT_SRPT");
    exit(1);
  }  
}

void ifoPrint_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt) {
  
  CHECK_ZERO(vmg_ptt_srpt->zero_1);
  PUT(5, "Number of PartofTitle search pointers: %i\n", 
      vmg_ptt_srpt->nr_of_srpts);
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
      PUT(5, "\tParental ID field: %04x\n", /* What does this mean? */
	  vmg_ptt_srpt->title_info[i].parental_id);
      PUT(5, "\tTitle set starting sector %08x\n", 
	  vmg_ptt_srpt->title_info[i].title_set_sector);
    }
  }
}


void ifoRead_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait, int sector) {
  int info_length;
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(ptl_mait, VMG_PTL_MAIT_SIZE, 1, ifo_file) != 1) {
    perror("VMG_PTL_MAIT");
    exit(1);
  }
  
  /* Change this to read and 'translate' the tables too. */
  
  //info_length = ptl_mait->nr_of_countries * VMG_PTL_MAIT_COUNTRY_SIZE;
  info_length = ptl_mait->last_byte + 1 -  VMG_PTL_MAIT_SIZE;
  ptl_mait->countries = malloc(info_length); 
  assert(info_length <= ptl_mait->last_byte + 1);
  if(fread(ptl_mait->countries, info_length, 1, ifo_file) != 1) {
    perror("VMG_PTL_MAIT info");
    exit(1);
  }
}

void ifoPrint_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait) {
  
  PUT(5, "Number of Countries: %i\n", ptl_mait->nr_of_countries);
  PUT(5, "Number of VTSs: %i\n", ptl_mait->nr_of_vtss);
  //PUT(5, "Last byte: %i\n", ptl_mait->last_byte);
  
  {
    int i, j;
    for(i=0;i<ptl_mait->nr_of_countries;i++) {
      CHECK_ZERO(ptl_mait->countries[i].zero_1);
      CHECK_ZERO(ptl_mait->countries[i].zero_2);
      PUT(5, "Country code: %c%c\n", 
	  ptl_mait->countries[i].country_code>>8,
	  ptl_mait->countries[i].country_code&0xff);
      /*
      PUT(5, "Start byte: %04x %i\n", 
	  ptl_mait->countries[i].start_byte_of_pf_ptl_mai, 
	  ptl_mait->countries[i].start_byte_of_pf_ptl_mai);
      */
      /* This seems to be pointing at a array with 8 2byte fields per VTS
         ? and one extra for the menue? always an odd number of VTSs on
	   all the dics I tested so it might be padding to even also.
	   If it is for the menu it probably the first entry.  */
      for(j=0;j<8;j++) {
	hexdump( (uint8_t *)ptl_mait->countries - 8 
		 + ptl_mait->countries[i].start_byte_of_pf_ptl_mai
		 + j*(ptl_mait->nr_of_vtss+1)*2, (ptl_mait->nr_of_vtss+1)*2);
	fprintf(stdout,"\n");
      }
    }
  }
}

void ifoRead_C_ADT(c_adt_t *c_adt, int sector) {
  int info_length;
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(c_adt, C_ADT_SIZE, 1, ifo_file) != 1) {
    perror("VMGM_C_ADT");
    exit(1);
  }
  
  info_length = c_adt->last_byte + 1 - C_ADT_SIZE;
  c_adt->cell_adr_table = malloc(info_length); 
  if(fread(c_adt->cell_adr_table, info_length, 1, ifo_file) != 1) {
    perror("VMGM_C_ADT info");
    exit(1);
  }
  assert((c_adt->last_byte + 1 - C_ADT_SIZE) % sizeof(c_adt_t) == 0);
}

void ifoPrint_C_ADT(c_adt_t *c_adt) {
  int i, entries;
  CHECK_ZERO(c_adt->zero_1);
  PUT(5, "Number of VOBs in this VOBS: %i\n", c_adt->nr_of_vobs);
  entries = (c_adt->last_byte + 1 - C_ADT_SIZE)/sizeof(c_adt_t);
  
  for(i=0;i<entries;i++) {
    CHECK_ZERO(c_adt->cell_adr_table[i].zero_1);
    PUT(5, "VOB ID: %3i, Cell ID: %3i   ", 
	c_adt->cell_adr_table[i].vob_id, c_adt->cell_adr_table[i].cell_id);
    PUT(5, "Sector (first): 0x%08x   (last): 0x%08x\n",
	c_adt->cell_adr_table[i].start_sector, 
	c_adt->cell_adr_table[i].last_sector);
    assert(c_adt->cell_adr_table[i].last_sector
	   > c_adt->cell_adr_table[i].start_sector);
  }
}


void ifoRead_VOBU_ADMAP(vobu_admap_t *vobu_admap, int sector) {
  int info_length;
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vobu_admap, VOBU_ADMAP_SIZE, 1, ifo_file) != 1) {
    perror("VMGM_VOBU_ADMAP");
    exit(1);
  }
  
  info_length = vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE;
  vobu_admap->vobu_start_sectors = malloc(info_length); 
  if(fread(vobu_admap->vobu_start_sectors, info_length, 1, ifo_file) != 1) {
    perror("VMGM_VOBU_ADMAP info");
    exit(1);
  }
}

void ifoPrint_VOBU_ADMAP(vobu_admap_t *vobu_admap) {
  int i, entries;
  entries = (vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE)/4;
  for(i=0;i<entries;i++) {
    PUT(5, "VOBU %5i  First sector: 0x%08x\n", i+1,
	vobu_admap->vobu_start_sectors[i]);
  }
}


void ifoRead_VMGM_PGCITI(vmgm_pgciti_t *vmgm_pgciti, int sector, int offset) {
  int i, info_length;
  uint8_t *data, *ptr;
  
  fseek(ifo_file, offset + sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vmgm_pgciti, VMGM_PGCITI_SIZE, 1, ifo_file) != 1) {
    perror("VMGM_PGCITI");
    exit(1);
  }
  
  info_length = vmgm_pgciti->nr_of_vmgm_pgci_srp * VMGM_PGCI_SRP_SIZE;
  data = malloc(info_length);
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("VMGM_PGCI_UT info");
    exit(1);
  }
  
  vmgm_pgciti->vmgm_pgci_srp 
    = malloc(vmgm_pgciti->nr_of_vmgm_pgci_srp * sizeof(vmgm_pgci_srp_t));
  ptr = data;
  for(i=0;i<vmgm_pgciti->nr_of_vmgm_pgci_srp;i++) {
    memcpy(&vmgm_pgciti->vmgm_pgci_srp[i], ptr, VMGM_PGCI_LU_SIZE);
    ptr += VMGM_PGCI_LU_SIZE;
    
    vmgm_pgciti->vmgm_pgci_srp[i].pgc = malloc(sizeof(pgc_t));
    ifoRead_PGC(vmgm_pgciti->vmgm_pgci_srp[i].pgc, 
		sector * DVD_BLOCK_LEN + offset
		+ vmgm_pgciti->vmgm_pgci_srp[i].start_byte_of_vmgm_pgc);
  }
  free(data);
}

void ifoRead_VMGM_PGCI_UT(vmgm_pgci_ut_t *vmgm_pgci_ut, int sector) {
  int i, info_length;
  uint8_t *data, *ptr;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vmgm_pgci_ut, VMGM_PGCI_UT_SIZE, 1, ifo_file) != 1) {
    perror("VMGM_PGCI_UT");
    exit(1);
  }
  info_length = vmgm_pgci_ut->nr_of_lang_units * VMGM_PGCI_LU_SIZE;
  data = malloc(info_length);
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("VMGM_PGCI_UT info");
    exit(1);
  }
  
  vmgm_pgci_ut->menu_lu 
    = malloc(vmgm_pgci_ut->nr_of_lang_units * sizeof(vmgm_pgci_lu_t));
  ptr = data;
  for(i=0;i<vmgm_pgci_ut->nr_of_lang_units;i++) {
    memcpy(&vmgm_pgci_ut->menu_lu[i], ptr, VMGM_PGCI_LU_SIZE);
    ptr += VMGM_PGCI_LU_SIZE;
    
    /* vmgm_pgci_ut->menu_lu[i].exists doesn't seem to indicate 
       the presens of the PGCITI but rather some thing else.
       Maybe only defined in v1.1? */
    vmgm_pgci_ut->menu_lu[i].menu_pgciti = malloc(sizeof(vmgm_pgciti_t));
    ifoRead_VMGM_PGCITI(vmgm_pgci_ut->menu_lu[i].menu_pgciti,
			sector,
			vmgm_pgci_ut->menu_lu[i].lang_start_byte);
  }
  free(data);
}


void ifoPrint_VMGM_PGCITI(vmgm_pgciti_t *vmgm_pgciti) {
  int i;
  CHECK_ZERO(vmgm_pgciti->zero_1);
  for(i=0;i<vmgm_pgciti->nr_of_vmgm_pgci_srp;i++) {
    PUT(5, "\nProgram (PGC): %3i\t", i);
    /* low 16 == ?Parental control mask? 
       highest byte menu type? 82 == root? */
    PUT(5, "VMGM PGC Category: 0x%08x\n",
	vmgm_pgciti->vmgm_pgci_srp[i].vmgm_pgc_category);
    ifoPrint_PGC(vmgm_pgciti->vmgm_pgci_srp[i].pgc);
  }
}

void ifoPrint_VMGM_PGCI_UT(vmgm_pgci_ut_t *vmgm_pgci_ut) {
  int i;
  CHECK_ZERO(vmgm_pgci_ut->zero_1);
  PUT(5, "Number of Menu Language Units (PGCI_LU): %3i\n", 
      vmgm_pgci_ut->nr_of_lang_units);
  for(i=0;i<vmgm_pgci_ut->nr_of_lang_units;i++) {
    CHECK_ZERO(vmgm_pgci_ut->menu_lu[i].zero_1);
    PUT(5, "Menu Language Code: %c%c\n",
	vmgm_pgci_ut->menu_lu[i].lang_code>>8,
	vmgm_pgci_ut->menu_lu[i].lang_code&0xff);
    PUT(5, "Menu Existence: %02x\n", vmgm_pgci_ut->menu_lu[i].exists);
    ifoPrint_VMGM_PGCITI(vmgm_pgci_ut->menu_lu[i].menu_pgciti);
  }
}

void ifoRead_VTS_ATRIBUTES(vts_atributes_t *vts_atributes, int offset) {
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(vts_atributes, VMG_VTS_ATRIBUTES_SIZE, 1, ifo_file) != 1) {
    perror("VMG_VTS_ATRIBUTES");
    exit(1);
  }
  //assert(vts_atributes->last_byte <= VMG_VTS_ATRIBUTES_SIZE);
}

void ifoRead_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt, int sector) {
  int i, info_length;
  uint8_t *data;
  uint32_t *ptr;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vts_atrt, VMG_VTS_ATRT_SIZE, 1, ifo_file) != 1) {
    perror("VMG_VTS_ATRT");
    exit(1);
  }
  info_length = vts_atrt->nr_of_vtss * 4; /* Fix? */
  data = malloc(info_length);
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("VMGM_VTS_ATRT info");
    exit(1);
  }
  
  info_length = vts_atrt->nr_of_vtss * VMG_VTS_ATRIBUTES_SIZE;
  vts_atrt->vts_atributes = malloc(info_length);
  ptr = (uint32_t *)data;
  for(i=0;i<vts_atrt->nr_of_vtss;i++) {
    int offset = *ptr++;
    assert(offset < vts_atrt->last_byte + 1);
    ifoRead_VTS_ATRIBUTES(&vts_atrt->vts_atributes[i], 
			  sector * DVD_BLOCK_LEN + offset);
    assert(offset + vts_atrt->vts_atributes[i].last_byte 
	   <= vts_atrt->last_byte + 1);
  }
  free(data);
}

void ifoPrint_VTS_ATRIBUTES(vts_atributes_t *vts_atributes) {
  int i, j;
  CHECK_ZERO(vts_atributes->zero_1);
  CHECK_ZERO(vts_atributes->zero_2);
  CHECK_ZERO(vts_atributes->zero_3);
  CHECK_ZERO(vts_atributes->zero_4);
  CHECK_ZERO(vts_atributes->zero_5);
  CHECK_ZERO(vts_atributes->zero_6);
  CHECK_ZERO(vts_atributes->zero_7);
  PUT(5, "VTS_CAT Application type: %08x\n", vts_atributes->vts_cat);
  
  PUT(5, "Attributes of VTSM_VOBS: %04x\n", 
      vts_atributes->vtsm_vobs_attributes);  
  PUT(5, "Number of Audio streams: %i\n", 
      vts_atributes->nr_of_vtsm_audio_streams);
  assert(vts_atributes->nr_of_vtsm_audio_streams <= 8);
  for(i=0;i<8;i++) {
    if(i<vts_atributes->nr_of_vtsm_audio_streams) {
      PUT(5, "\tAudio stream %i status: ", i+1);
      for(j=0;j<8;j++)  /* This should be a function (verbose) */
	PUT(5, "%02x ", vts_atributes->vtsm_audio_attributes[i][j]);
      PUT(5, "\n");
    } else
      for(j=0;j<6;j++)
	CHECK_ZERO(vts_atributes->vtsm_audio_attributes[i][j]);
  }      
  PUT(5, "Number of Subpicture streams: %i\n", 
      vts_atributes->nr_of_vtsm_subp_streams);
  assert(vts_atributes->nr_of_vtsm_subp_streams <= 28);
  for(i=0;i<28;i++) {
    if(i<vts_atributes->nr_of_vtsm_subp_streams) {
      PUT(5, "\tSub-picture stream %2i status: ", i+1);
      for(j=0;j<6;j++) /* This should be a function (verbose) */
	PUT(5, "%02x ", vts_atributes->vtsm_subp_attributes[i][j]);
      PUT(5, "\n");
    } else
      for(j=0;j<6;j++)
	CHECK_ZERO(vts_atributes->vtsm_subp_attributes[i][j]);      
  }
   
   
  PUT(5, "Attributes of VTSTT_VOBS: %04x\n", 
      vts_atributes->vtstt_vobs_video_attributes);  
  PUT(5, "Number of Audio streams: %i\n", 
      vts_atributes->nr_of_vtstt_audio_streams);
  assert(vts_atributes->nr_of_vtstt_audio_streams <= 8);
  for(i=0;i<8;i++) {
    if(i<vts_atributes->nr_of_vtstt_audio_streams) {
      PUT(5, "\tAudio stream %i status: ", i+1);
      for(j=0;j<8;j++) /* This should be a function (verbose) */
	PUT(5, "%02x ", vts_atributes->vtstt_audio_attributes[i][j]);
      PUT(5, "\n");
    } else
      for(j=0;j<6;j++)
	CHECK_ZERO(vts_atributes->vtstt_audio_attributes[i][j]);
  }
  
  /* I've had to cut down on the max number of streams here 
     because in 'Alien' it get garbage in the last two entries. 
     Should realy check last_byte for how many theres room for. */ 
  PUT(5, "Number of Subpicture streams: %i\n", 
      vts_atributes->nr_of_vtstt_subp_streams);
  assert(vts_atributes->nr_of_vtstt_subp_streams <= 26);
  for(i=0;i<26;i++) {
    if(i<vts_atributes->nr_of_vtstt_subp_streams) {
      PUT(5, "\tSub-picture stream %2i status: ", i+1);    
      for(j=0;j<6;j++) /* This should be a function (verbose) */
	PUT(5, "%02x ", vts_atributes->vtstt_subp_attributes[i][j]);
      PUT(5, "\n");
    } else
      for(j=0;j<6;j++)
	CHECK_ZERO(vts_atributes->vtstt_subp_attributes[i][j]);
   }
}

void ifoPrint_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt) {
  int i;
  CHECK_ZERO(vts_atrt->zero_1);
  PUT(5, "Number of Video Title Sets: %3i\n", 
     vts_atrt->nr_of_vtss);
  for(i=0;i<vts_atrt->nr_of_vtss;i++) {
    PUT(5, "\nVideo Title Set %i\n", i);
    ifoPrint_VTS_ATRIBUTES(&vts_atrt->vts_atributes[i]);
  }
}


