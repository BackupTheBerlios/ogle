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
  PUT(5, "VMG POS Code: %016x\n", vmgi_mat->vmg_pos_code);
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
