#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <assert.h>

#include "ifo.h"

#ifdef WORDS_BIGENDIAN
#define B2N_16(x) (void)(x)
#define B2N_32(x) (void)(x)
#define B2N_64(x) (void)(x)
#else
#include <byteswap.h>
#define B2N_16(x) x = bswap_16((x))
#define B2N_32(x) x = bswap_32((x))
#define B2N_64(x) x = bswap_64((x))
#endif

#ifndef NDEBUG
#define CHECK_ZERO(arg) \
if(memcmp(my_friendly_zeros, &arg, sizeof(arg))) { \
 int i; \
 fprintf(stderr, "*** Zero check failed in %s:%i\n    for %s = 0x", \
	   __FILE__, __LINE__, # arg ); \
 for(i=0; i < sizeof(arg); i++) \
   fprintf(stderr, "%02x", *((uint8_t *)&arg + i)); \
 fprintf(stderr, "\n"); \
}
static const uint8_t my_friendly_zeros[2048];
#else
#define CHECK_ZERO(arg) (void)(arg)
#endif



FILE *ifo_file;

void ifoClose(void) {
  fclose(ifo_file);
}


int ifoOpen_VMG(vmgi_mat_t *vmgi_mat, char *filename) {
  int i, j;
  
  ifo_file = fopen(filename,"r");
  if(!ifo_file) {
    perror(filename);
    return -1;
  }
  
  if(fread(vmgi_mat, sizeof(vmgi_mat_t), 1, ifo_file) != 1) {
    fclose(ifo_file);
    ifo_file = NULL;
    return -1;
  }
  if(strncmp("DVDVIDEO-VMG", vmgi_mat->vmg_identifier, 12))
    return -1;
  
  B2N_32(vmgi_mat->vmg_last_sector);
  B2N_32(vmgi_mat->vmgi_last_sector);
  B2N_32(vmgi_mat->vmg_category);
  B2N_16(vmgi_mat->vmg_nr_of_volumes);
  B2N_16(vmgi_mat->vmg_this_volume_nr);
  B2N_16(vmgi_mat->vmg_nr_of_title_sets);
  B2N_64(vmgi_mat->vmg_pos_code);
  B2N_32(vmgi_mat->vmgi_last_byte);
  B2N_32(vmgi_mat->first_play_pgc);
  B2N_32(vmgi_mat->vmgm_vobs);
  B2N_32(vmgi_mat->vmg_ptt_srpt);
  B2N_32(vmgi_mat->vmgm_pgci_ut);
  B2N_32(vmgi_mat->vmg_ptl_mait);
  B2N_32(vmgi_mat->vmg_vts_atrt);
  B2N_32(vmgi_mat->vmg_txtdt_mgi);
  B2N_32(vmgi_mat->vmgm_c_adt);
  B2N_32(vmgi_mat->vmgm_vobu_admap);
  B2N_16(vmgi_mat->vmgm_video_attributes);
  
  
#ifndef NDEBUG
  CHECK_ZERO(vmgi_mat->zero_1);
  CHECK_ZERO(vmgi_mat->zero_2);
  CHECK_ZERO(vmgi_mat->zero_3);
  CHECK_ZERO(vmgi_mat->zero_4);
  CHECK_ZERO(vmgi_mat->zero_5);
  CHECK_ZERO(vmgi_mat->zero_6);
  CHECK_ZERO(vmgi_mat->zero_7);
  CHECK_ZERO(vmgi_mat->zero_8);
  assert(vmgi_mat->vmg_last_sector != 0);
  assert(vmgi_mat->vmgi_last_sector != 0);
  assert(vmgi_mat->vmgi_last_sector * 2 <= vmgi_mat->vmg_last_sector);
  assert(vmgi_mat->vmg_nr_of_volumes != 0);
  assert(vmgi_mat->vmg_this_volume_nr != 0);
  assert(vmgi_mat->vmg_this_volume_nr <= vmgi_mat->vmg_nr_of_volumes);
  assert(vmgi_mat->disc_side == 1 || vmgi_mat->disc_side == 2);
  assert(vmgi_mat->vmg_nr_of_title_sets != 0);
  assert(vmgi_mat->vmgi_last_byte >= 341);
  assert(vmgi_mat->vmgi_last_byte / DVD_BLOCK_LEN <= 
	 vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->first_play_pgc != 0 && 
	 vmgi_mat->first_play_pgc < vmgi_mat->vmgi_last_byte);
  assert(vmgi_mat->vmgm_vobs == 0 || 
	 (vmgi_mat->vmgm_vobs > vmgi_mat->vmgi_last_sector &&
	  vmgi_mat->vmgm_vobs < vmgi_mat->vmg_last_sector));
  assert(vmgi_mat->vmg_ptt_srpt <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmgm_pgci_ut <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmg_ptl_mait <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmg_vts_atrt <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmg_txtdt_mgi <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmgm_c_adt <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmgm_vobu_admap <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->nr_of_vmgm_audio_streams <= 8);
  for(i = vmgi_mat->nr_of_vmgm_audio_streams; i < 8; i++)
    for(j = 0; j < 8; j++)
      CHECK_ZERO(vmgi_mat->vmgm_audio_attributes[i][j]);
  assert(vmgi_mat->nr_of_vmgm_subp_streams <= 28);
  for(i = vmgi_mat->nr_of_vmgm_subp_streams; i < 28; i++)
    for(j = 0; j < 6; j++)
      CHECK_ZERO(vmgi_mat->vmgm_subp_attributes[i][j]);      
#endif
  
  return 0;
}


int ifoOpen_VTS(vtsi_mat_t *vtsi_mat, char *filename) {
  int i, j;
  
  ifo_file = fopen(filename,"r");
  if(!ifo_file) {
    perror(filename);
    return -1;
  }
  
  if(fread(vtsi_mat, sizeof(vtsi_mat_t), 1, ifo_file) != 1) {
    fclose(ifo_file);
    ifo_file = NULL;
    return -1;
  }
  if(strncmp("DVDVIDEO-VTS", vtsi_mat->vts_identifier, 12))
    return -1;
  
  B2N_32(vtsi_mat->vts_last_sector);
  B2N_32(vtsi_mat->vtsi_last_sector);
  B2N_32(vtsi_mat->vts_category);
  B2N_32(vtsi_mat->vtsi_last_byte);
  B2N_32(vtsi_mat->vtsm_vobs);
  B2N_32(vtsi_mat->vtstt_vobs);
  B2N_32(vtsi_mat->vts_ptt_srpt);
  B2N_32(vtsi_mat->vts_pgcit);
  B2N_32(vtsi_mat->vtsm_pgci_ut);
  B2N_32(vtsi_mat->vts_tmapt);
  B2N_32(vtsi_mat->vtsm_c_adt);
  B2N_32(vtsi_mat->vtsm_vobu_admap);
  B2N_32(vtsi_mat->vts_c_adt);
  B2N_32(vtsi_mat->vts_vobu_admap);
  B2N_16(vtsi_mat->vtsm_video_attributes);
  B2N_16(vtsi_mat->vts_video_attributes);

#ifndef NDEBUG
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
  assert(vtsi_mat->vtsi_last_sector*2 <= vtsi_mat->vts_last_sector);
  assert(vtsi_mat->vtsi_last_byte/DVD_BLOCK_LEN <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_vobs == 0 || 
	 (vtsi_mat->vtsm_vobs > vtsi_mat->vtsi_last_sector &&
	  vtsi_mat->vtsm_vobs < vtsi_mat->vts_last_sector));
  assert(vtsi_mat->vtstt_vobs == 0 || 
	 (vtsi_mat->vtstt_vobs > vtsi_mat->vtsi_last_sector &&
	  vtsi_mat->vtstt_vobs < vtsi_mat->vts_last_sector));
  assert(vtsi_mat->vts_ptt_srpt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_pgcit <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_pgci_ut <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_tmapt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_c_adt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_vobu_admap <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_c_adt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_vobu_admap <= vtsi_mat->vtsi_last_sector);
  
  assert(vtsi_mat->nr_of_vtsm_audio_streams <= 8);
  for(i = vtsi_mat->nr_of_vtsm_audio_streams; i < 8; i++)
    for(j = 0; j < 8; j++)
      CHECK_ZERO(vtsi_mat->vtsm_audio_attributes[i][j]);
  assert(vtsi_mat->nr_of_vtsm_subp_streams <= 28);
  for(i = vtsi_mat->nr_of_vtsm_subp_streams; i < 28; i++)
    for(j = 0; j < 6; j++)
      CHECK_ZERO(vtsi_mat->vtsm_subp_attributes[i][j]);      

  assert(vtsi_mat->nr_of_vts_audio_streams <= 8);
  for(i = vtsi_mat->nr_of_vts_audio_streams; i < 8; i++)
    for(j = 0; j < 8; j++)
      CHECK_ZERO(vtsi_mat->vts_audio_attributes[i][j]);
  assert(vtsi_mat->nr_of_vts_subp_streams <= 32);
  for(i = vtsi_mat->nr_of_vts_subp_streams; i < 32; i++)
    for(j = 0; j < 6; j++)
      CHECK_ZERO(vtsi_mat->vts_subp_attributes[i][j]);      
#endif  
  return 0;
}


void ifoRead_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl, int offset) {
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cmd_tbl, PGC_COMMAND_TBL_SIZE, 1, ifo_file) != 1) {
    perror("PGC - Command Table");
    exit(1);
  }
  B2N_16(cmd_tbl->nr_of_pre);
  B2N_16(cmd_tbl->nr_of_post);
  B2N_16(cmd_tbl->nr_of_cell);
  
  
  assert(cmd_tbl->nr_of_pre + cmd_tbl->nr_of_post + cmd_tbl->nr_of_cell<= 128);
  
  
  if(cmd_tbl->nr_of_pre != 0) {
    int pre_cmd_size  = cmd_tbl->nr_of_pre * COMMAND_DATA_SIZE;
    cmd_tbl->pre_commands = malloc(pre_cmd_size);
    if(fread(cmd_tbl->pre_commands, pre_cmd_size, 1, ifo_file) != 1) {
      perror("PGC - Command Table, Pre comands");
      exit(1);
    }
  }
  
  if(cmd_tbl->nr_of_post != 0) {
    int post_cmd_size = cmd_tbl->nr_of_post * COMMAND_DATA_SIZE;
    cmd_tbl->post_commands = malloc(post_cmd_size);
    if(fread(cmd_tbl->post_commands, post_cmd_size, 1, ifo_file) != 1) {
      perror("PGC - Command Table, Post commands");
      exit(1);
    }
  }
  
  if(cmd_tbl->nr_of_cell != 0) {
    int cell_cmd_size = cmd_tbl->nr_of_cell * COMMAND_DATA_SIZE;
    cmd_tbl->cell_commands = malloc(cell_cmd_size);
    if(fread(cmd_tbl->cell_commands, cell_cmd_size, 1, ifo_file) != 1) {
      perror("PGC - Command Table, Cell commands");
      exit(1);
    }
  }
  
  /* 
   * Make a run over all the commands and see that we can interpret them all. 
   */
}


void ifoRead_PGC_PROGRAM_MAP(pgc_program_map_t *program_map, int nr, int offset) {
  int size = nr * sizeof(pgc_program_map_t);
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(program_map, size, 1, ifo_file) != 1) {
    perror("PGC - Program Map");
    exit(1);
  }
}


void ifoRead_CELL_PLAYBACK_TBL(cell_playback_tbl_t *cell_playback, int nr, int offset) {
  int i;
  int size = nr * sizeof(cell_playback_tbl_t);
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cell_playback, size, 1, ifo_file) != 1) {
    perror("PGC - Cell Playback info");
    exit(1);
  }
  for(i = 0; i < nr; i++) {
    B2N_32(cell_playback[i].category);
    B2N_32(cell_playback[i].first_sector);
    B2N_32(cell_playback[i].first_ilvu_end_sector);
    B2N_32(cell_playback[i].last_vobu_start_sector);
    B2N_32(cell_playback[i].last_sector);
    
#ifndef NDEBUG
    // Changed < to <= because this was false in the movie 'Pi'.
    assert(cell_playback[i].last_vobu_start_sector <= 
	   cell_playback[i].last_sector);
    assert(cell_playback[i].first_sector <= 
	   cell_playback[i].last_vobu_start_sector);
#endif
  }
}


void ifoRead_CELL_POSITION_TBL(cell_position_tbl_t *cell_position, int nr, int offset) {
  int i;
  int size = nr * sizeof(cell_position_tbl_t);
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(cell_position, size, 1, ifo_file) != 1) {
    perror("PGC - Cell Position info");
    exit(1);
  }
  for(i = 0; i < nr; i++) {
    B2N_16(cell_position[i].vob_id_nr);
  }
  
#ifndef NDEBUG
  for(i = 0; i < nr; i++) {
    CHECK_ZERO(cell_position[i].zero_1);
  }
#endif
}


void ifoRead_PGC(pgc_t *pgc, int offset) {
  int i;
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(pgc, PGC_SIZE, 1, ifo_file) != 1) {
    perror("First Play PGC");
    exit(1);
  }
  B2N_32(pgc->prohibited_ops);
  B2N_16(pgc->next_pgc_nr);
  B2N_16(pgc->prev_pgc_nr);
  B2N_16(pgc->goup_pgc_nr);
  B2N_16(pgc->pgc_command_tbl_offset);
  B2N_16(pgc->pgc_program_map_offset);
  B2N_16(pgc->cell_playback_tbl_offset);
  B2N_16(pgc->cell_position_tbl_offset);
  for(i = 0; i < 8; i++)
    B2N_16(pgc->audio_status[i]);
  for(i = 0; i < 32; i++)
    B2N_32(pgc->subp_status[i]);
  for(i = 0; i < 16; i++)
    B2N_32(pgc->palette[i]);
  
#ifndef NDEBUG
  CHECK_ZERO(pgc->zero_1);
  assert(pgc->nr_of_programs <= pgc->nr_of_cells);
  // verify time (look at print_time)
  for(i = 0; i < 8; i++)
    if(!pgc->audio_status[i] & 0x8000) /* The 'is present' bit */
      CHECK_ZERO(pgc->audio_status[i]);
  for(i = 0; i < 32; i++)
    if(!pgc->subp_status[i] & 0x80000000) /* The 'is present' bit */
      CHECK_ZERO(pgc->subp_status[i]);
  
  /* Check that time is 0:0:0:0 also if nr_of_programs == 0 */
  if(pgc->nr_of_programs == 0) {
    CHECK_ZERO(pgc->still_time);
    CHECK_ZERO(pgc->pg_playback_mode); // ??
    assert(pgc->pgc_program_map_offset == 0);
    assert(pgc->cell_playback_tbl_offset == 0);
    assert(pgc->cell_position_tbl_offset == 0);
  } else {
    assert(pgc->pgc_program_map_offset != 0);
    assert(pgc->cell_playback_tbl_offset != 0);
    assert(pgc->cell_position_tbl_offset != 0);
  }
#endif
  
  if(pgc->pgc_command_tbl_offset != 0) {
    pgc->pgc_command_tbl = malloc(sizeof(pgc_command_tbl_t));
    ifoRead_PGC_COMMAND_TBL(pgc->pgc_command_tbl, 
			    offset + pgc->pgc_command_tbl_offset);
  } else {
    pgc->pgc_command_tbl = NULL;
  }
  
  if(pgc->pgc_program_map_offset != 0) {
    pgc->pgc_program_map = malloc(pgc->nr_of_programs * 
				 sizeof(pgc_program_map_t));
    ifoRead_PGC_PROGRAM_MAP(pgc->pgc_program_map, pgc->nr_of_programs, 
			    offset + pgc->pgc_program_map_offset);
  } else {
    pgc->pgc_program_map = NULL;
  }
  
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


void ifoRead_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt, int sector) {
  int i, info_length;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vmg_ptt_srpt, VMG_PTT_SRPT_SIZE, 1, ifo_file) != 1) {
    perror("VMG_PTT_SRPT");
    exit(1);
  }
  B2N_16(vmg_ptt_srpt->nr_of_srpts);
  B2N_32(vmg_ptt_srpt->last_byte);
  
  info_length = vmg_ptt_srpt->last_byte + 1 - VMG_PTT_SRPT_SIZE;
  
  vmg_ptt_srpt->title_info = malloc(info_length); 
  if(fread(vmg_ptt_srpt->title_info, info_length, 1, ifo_file) != 1) {
    perror("VMG_PTT_SRPT");
    exit(1);
  }
  for(i =  0; i < vmg_ptt_srpt->nr_of_srpts; i++) {
    B2N_16(vmg_ptt_srpt->title_info[i].nr_of_ptts);
    B2N_16(vmg_ptt_srpt->title_info[i].parental_id);
    B2N_32(vmg_ptt_srpt->title_info[i].title_set_sector);
  }
  
#ifndef NDEBUG
  CHECK_ZERO(vmg_ptt_srpt->zero_1);
  assert(vmg_ptt_srpt->nr_of_srpts != 0);
  assert(vmg_ptt_srpt->nr_of_srpts < 100); // ??
  assert(vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t) <= info_length);
  
  for(i = 0; i < vmg_ptt_srpt->nr_of_srpts; i++) {
    assert(vmg_ptt_srpt->title_info[i].nr_of_angles != 0);
    assert(vmg_ptt_srpt->title_info[i].nr_of_angles < 10);
    assert(vmg_ptt_srpt->title_info[i].nr_of_ptts != 0);
    assert(vmg_ptt_srpt->title_info[i].nr_of_ptts < 100); // ??
    assert(vmg_ptt_srpt->title_info[i].title_set_nr != 0);
    assert(vmg_ptt_srpt->title_info[i].title_set_nr < 100); // ??
    assert(vmg_ptt_srpt->title_info[i].vts_ttn != 0);
    assert(vmg_ptt_srpt->title_info[i].vts_ttn < 100); // ??
    assert(vmg_ptt_srpt->title_info[i].title_set_sector != 0);
  }
#endif
  
  // Make this a function
#if 0
  if(memcmp((uint8_t *)vmg_ptt_srpt->title_info + 
	    vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t), 
	    my_friendly_zeros, 
	    info_length - vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t))) {
    fprintf(stderr, "VMG_PTT_SRPT slack is != 0, ");
    hexdump((uint8_t *)vmg_ptt_srpt->title_info + 
	    vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t), 
	    info_length - vmg_ptt_srpt->nr_of_srpts * sizeof(title_info_t));
  }
#endif
}


void ifoRead_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt, int sector) {
  int info_length, i, j;
  uint32_t *data;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vts_ptt_srpt, VTS_PTT_SRPT_SIZE, 1, ifo_file) != 1) {
    perror("VTS_PTT_SRPT");
    exit(1);
  }
  B2N_16(vts_ptt_srpt->nr_of_srpts);
  B2N_32(vts_ptt_srpt->last_byte);
  
#ifndef NDEBUG
  CHECK_ZERO(vts_ptt_srpt->zero_1);
  assert(vts_ptt_srpt->nr_of_srpts != 0);
  assert(vts_ptt_srpt->nr_of_srpts < 100); // ??
#endif
  
  info_length = vts_ptt_srpt->last_byte + 1 - VTS_PTT_SRPT_SIZE;
  data = malloc(info_length); 
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("VTS_PTT_SRPT");
    exit(1);
  }
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++)
    B2N_32(data[i]);
  
#ifndef NDEBUG
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++)
    assert(data[i] + sizeof(ptt_info_t) <= vts_ptt_srpt->last_byte + 1);
#endif
  
  vts_ptt_srpt->title_info = malloc(vts_ptt_srpt->nr_of_srpts * sizeof(ttu_t));
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    int n;
    if(i < vts_ptt_srpt->nr_of_srpts - 1)
      n = (data[i+1] - data[i]);
    else
      n = (vts_ptt_srpt->last_byte + 1 - data[i]);
    assert(n > 0 && (n % 4) == 0);
    
    vts_ptt_srpt->title_info[i].nr_of_ptts = n / 4;
    vts_ptt_srpt->title_info[i].ptt_info = malloc(n * sizeof(ptt_info_t));
    for(j = 0; j < vts_ptt_srpt->title_info[i].nr_of_ptts; j++) {
      vts_ptt_srpt->title_info[i].ptt_info[j].pgcn 
	= *(uint16_t*)(((char *)data) + data[i] + 4*j - VTS_PTT_SRPT_SIZE);
      vts_ptt_srpt->title_info[i].ptt_info[j].pgn 
	= *(uint16_t*)(((char *)data) + data[i] + 4*j + 2 - VTS_PTT_SRPT_SIZE);
    }
  }
  free(data);
  
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    for(j = 0; j < vts_ptt_srpt->title_info[i].nr_of_ptts; j++) {
      B2N_16(vts_ptt_srpt->title_info[i].ptt_info[j].pgcn);
      B2N_16(vts_ptt_srpt->title_info[i].ptt_info[j].pgn);
    }
  }
  
#ifndef NDEBUG
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    assert(vts_ptt_srpt->title_info[i].nr_of_ptts < 1000); // ??
    for(j = 0; j < vts_ptt_srpt->title_info[i].nr_of_ptts; j++) {
      assert(vts_ptt_srpt->title_info[i].ptt_info[j].pgcn != 0 );
      assert(vts_ptt_srpt->title_info[i].ptt_info[j].pgcn < 100); // ??
      assert(vts_ptt_srpt->title_info[i].ptt_info[j].pgn != 0);
      assert(vts_ptt_srpt->title_info[i].ptt_info[j].pgn < 100); // ??
    }
  }
#endif
}


void ifoRead_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait, int sector) {
  int i, info_length;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(ptl_mait, VMG_PTL_MAIT_SIZE, 1, ifo_file) != 1) {
    perror("VMG_PTL_MAIT");
    exit(1);
  }
  B2N_16(ptl_mait->nr_of_countries);
  B2N_16(ptl_mait->nr_of_vtss);
  B2N_32(ptl_mait->last_byte);
  
  info_length = ptl_mait->last_byte + 1 -  VMG_PTL_MAIT_SIZE;
  
#ifndef NDEBUG
  assert(ptl_mait->nr_of_countries != 0);
  assert(ptl_mait->nr_of_countries < 100); // ??
  assert(ptl_mait->nr_of_vtss != 0);
  assert(ptl_mait->nr_of_vtss < 100); // ??  
  assert(ptl_mait->nr_of_countries * VMG_PTL_MAIT_COUNTRY_SIZE <= info_length);
#endif
  
  /* Change this to read and 'translate' the tables too. 
     I.e don't read so much here */
  ptl_mait->countries = malloc(info_length);
  if(fread(ptl_mait->countries, info_length, 1, ifo_file) != 1) {
    perror("VMG_PTL_MAIT info");
    exit(1);
  }
  for(i = 0; i < ptl_mait->nr_of_countries; i++) {
    B2N_16(ptl_mait->countries[i].pf_ptl_mai_start_byte);
  }
  
#ifndef NDEBUG
  for(i = 0; i < ptl_mait->nr_of_countries; i++) {
    CHECK_ZERO(ptl_mait->countries[i].zero_1);
    CHECK_ZERO(ptl_mait->countries[i].zero_2);    
    assert(ptl_mait->countries[i].pf_ptl_mai_start_byte + 
	   8 * (ptl_mait->nr_of_vtss+1) * 2 <= ptl_mait->last_byte + 1);
  }
#endif
}


void ifoRead_C_ADT(c_adt_t *c_adt, int sector) {
  int i, info_length;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(c_adt, C_ADT_SIZE, 1, ifo_file) != 1) {
    perror("VMGM_C_ADT");
    exit(1);
  }
  B2N_16(c_adt->nr_of_vobs);
  B2N_32(c_adt->last_byte);
  
  info_length = c_adt->last_byte + 1 - C_ADT_SIZE;
  
#ifndef NDEBUG
  CHECK_ZERO(c_adt->zero_1);
  assert(c_adt->nr_of_vobs > 0);  
  assert(info_length % sizeof(c_adt_t) == 0);
  assert(info_length/sizeof(c_adt_t) >= c_adt->nr_of_vobs); // Pointless test..
#endif
  
  c_adt->cell_adr_table = malloc(info_length); 
  if(fread(c_adt->cell_adr_table, info_length, 1, ifo_file) != 1) {
    perror("VMGM_C_ADT info");
    exit(1);
  }
  for(i = 0; i < info_length/sizeof(c_adt_t); i++) {
    B2N_16(c_adt->cell_adr_table[i].vob_id);
    B2N_32(c_adt->cell_adr_table[i].start_sector);
    B2N_32(c_adt->cell_adr_table[i].last_sector);
  }
  
#ifndef NDEBUG
  for(i = 0; i < info_length/sizeof(c_adt_t); i++) {
    CHECK_ZERO(c_adt->cell_adr_table[i].zero_1);
    assert(c_adt->cell_adr_table[i].vob_id > 0);
    assert(c_adt->cell_adr_table[i].vob_id <= c_adt->nr_of_vobs);
    assert(c_adt->cell_adr_table[i].cell_id > 0);
    assert(c_adt->cell_adr_table[i].start_sector < 
	   c_adt->cell_adr_table[i].last_sector);
  }
#endif
}


void ifoRead_VOBU_ADMAP(vobu_admap_t *vobu_admap, int sector) {
  int i, info_length;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vobu_admap, VOBU_ADMAP_SIZE, 1, ifo_file) != 1) {
    perror("VMGM_VOBU_ADMAP");
    exit(1);
  }
  B2N_32(vobu_admap->last_byte);
  
  info_length = vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE;
  assert(info_length > 0);
  assert(info_length % sizeof(int32_t) == 0);
  
  vobu_admap->vobu_start_sectors = malloc(info_length); 
  if(fread(vobu_admap->vobu_start_sectors, info_length, 1, ifo_file) != 1) {
    perror("VMGM_VOBU_ADMAP info");
    exit(1);
  }
  for(i = 0; i < info_length/sizeof(int32_t); i++) {
    B2N_32(vobu_admap->vobu_start_sectors[i]);
  }
}


void ifoRead_PGCIT(pgcit_t *pgcit, int offset) {
  int i, info_length;
  uint8_t *data, *ptr;
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(pgcit, PGCIT_SIZE, 1, ifo_file) != 1) {
    perror("PGCIT");
    exit(1);
  }
  B2N_16(pgcit->nr_of_pgci_srp);
  B2N_32(pgcit->last_byte);
  
#ifndef NDEBUG
  CHECK_ZERO(pgcit->zero_1);
  assert(pgcit->nr_of_pgci_srp != 0);
  assert(pgcit->nr_of_pgci_srp < 1000); // ?? 99
#endif
  
  info_length = pgcit->nr_of_pgci_srp * PGCI_SRP_SIZE;
  data = malloc(info_length);
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("PGCIT info");
    exit(1);
  }
  pgcit->pgci_srp = malloc(pgcit->nr_of_pgci_srp * sizeof(pgci_srp_t));
  ptr = data;
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    memcpy(&pgcit->pgci_srp[i], ptr, MENU_PGCI_LU_SIZE);
    ptr += MENU_PGCI_LU_SIZE;
    B2N_32(pgcit->pgci_srp[i].pgc_category);
    B2N_32(pgcit->pgci_srp[i].pgc_start_byte);
  }
  free(data);
  
#ifndef NDEBUG
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++)
    assert(pgcit->pgci_srp[i].pgc_start_byte + PGC_SIZE <= pgcit->last_byte+1);
#endif
  
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    pgcit->pgci_srp[i].pgc = malloc(sizeof(pgc_t));
    ifoRead_PGC(pgcit->pgci_srp[i].pgc, 
		offset + pgcit->pgci_srp[i].pgc_start_byte);
  }
}


void ifoRead_MENU_PGCI_UT(menu_pgci_ut_t *pgci_ut, int sector) {
  int i, info_length;
  uint8_t *data, *ptr;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(pgci_ut, MENU_PGCI_UT_SIZE, 1, ifo_file) != 1) {
    perror("MENU_PGCI_UT");
    exit(1);
  }
  B2N_16(pgci_ut->nr_of_lang_units);
  B2N_32(pgci_ut->last_byte);
  
#ifndef NDEBUG
  CHECK_ZERO(pgci_ut->zero_1);
  assert(pgci_ut->nr_of_lang_units != 0);
  assert(pgci_ut->nr_of_lang_units < 100); // ?? 3-4 ? 
  assert(pgci_ut->nr_of_lang_units * MENU_PGCI_LU_SIZE < pgci_ut->last_byte);
#endif
  
  info_length = pgci_ut->nr_of_lang_units * MENU_PGCI_LU_SIZE;
  data = malloc(info_length);
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("MENU_PGCI_UT info");
    exit(1);
  }
  pgci_ut->menu_lu 
    = malloc(pgci_ut->nr_of_lang_units * sizeof(menu_pgci_lu_t));
  ptr = data;
  for(i = 0; i < pgci_ut->nr_of_lang_units; i++) {
    memcpy(&pgci_ut->menu_lu[i], ptr, MENU_PGCI_LU_SIZE);
    ptr += MENU_PGCI_LU_SIZE;
    B2N_32(pgci_ut->menu_lu[i].lang_start_byte); 
  }
  free(data);
  
#ifndef NDEBUG
  for(i = 0; i < pgci_ut->nr_of_lang_units; i++) {
    CHECK_ZERO(pgci_ut->menu_lu[i].zero_1);
    // Maybe this is only defined for v1.1 and later titles.
    /* If the bits in 'menu_lu[i].exists' are enumerated abcd efgh then:
            VTS_x_yy.IFO	VIDEO_TS.IFO
       a == 0x83 "Root"		0x82 "Title (VTS menu)"
       b == 0x84 "Sub-Picture"
       c == 0x85 "Audio"
       d == 0x86 "Angle"
       e == 0x87 "PTT (Part of Title?)"
    */
    assert((pgci_ut->menu_lu[i].exists & 0x07) == 0);
  }
#endif
  
  for(i = 0; i < pgci_ut->nr_of_lang_units; i++) {
    pgci_ut->menu_lu[i].menu_pgcit = malloc(sizeof(pgcit_t));
    ifoRead_PGCIT(pgci_ut->menu_lu[i].menu_pgcit, sector * DVD_BLOCK_LEN +
		  pgci_ut->menu_lu[i].lang_start_byte);
    // Iterate and verify that all menus that should exists accordingly to 
    // pgci_ut->menu_lu[i].exists really do?
  }
}


void ifoRead_VTS_ATRIBUTES(vts_atributes_t *vts_atributes, int offset) {
  int i, j;
  
  fseek(ifo_file, offset, SEEK_SET);
  if(fread(vts_atributes, VMG_VTS_ATRIBUTES_SIZE, 1, ifo_file) != 1) {
    perror("VMG_VTS_ATRIBUTES");
    exit(1);
  }
  B2N_32(vts_atributes->last_byte);
  B2N_32(vts_atributes->vts_cat);
  B2N_16(vts_atributes->vtsm_vobs_attributes);
  B2N_16(vts_atributes->vtstt_vobs_video_attributes);
  
#ifndef NDEBUG
  CHECK_ZERO(vts_atributes->zero_1);
  CHECK_ZERO(vts_atributes->zero_2);
  CHECK_ZERO(vts_atributes->zero_3);
  CHECK_ZERO(vts_atributes->zero_4);
  CHECK_ZERO(vts_atributes->zero_5);
  CHECK_ZERO(vts_atributes->zero_6);
  CHECK_ZERO(vts_atributes->zero_7);
  assert(vts_atributes->nr_of_vtsm_audio_streams <= 8);
  for(i = vts_atributes->nr_of_vtsm_audio_streams; i < 8; i++)
    for(j = 0; j < 6; j++)
      CHECK_ZERO(vts_atributes->vtsm_audio_attributes[i][j]);
  assert(vts_atributes->nr_of_vtsm_subp_streams <= 28);
  for(i = vts_atributes->nr_of_vtsm_subp_streams; i < 28; i++)
    for(j = 0; j < 6; j++)
      CHECK_ZERO(vts_atributes->vtsm_subp_attributes[i][j]);      
  assert(vts_atributes->nr_of_vtstt_audio_streams <= 8);
  for(i = vts_atributes->nr_of_vtstt_audio_streams; i < 8; i++)
    for(j = 0; j < 6; j++)
      CHECK_ZERO(vts_atributes->vtstt_audio_attributes[i][j]);
  {
    int nr_coded;
    assert(vts_atributes->last_byte + 1 - VMG_VTS_ATRIBUTES_MIN_SIZE >= 0);  
    nr_coded = (vts_atributes->last_byte + 1 - VMG_VTS_ATRIBUTES_MIN_SIZE)/6;
    // This is often nr_coded = 70, how do you know how many there really are?
    if(nr_coded > 32) { // We haven't read more from disk/file anyway
      nr_coded = 32;
    }
    assert(vts_atributes->nr_of_vtstt_subp_streams <= nr_coded);
    for(i = vts_atributes->nr_of_vtstt_subp_streams; i < nr_coded; i++)
      for(j = 0; j < 6; j++)
	CHECK_ZERO(vts_atributes->vtstt_subp_attributes[i][j]);
  }
#endif
}


void ifoRead_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt, int sector) {
  int i, info_length;
  uint32_t *data;
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vts_atrt, VMG_VTS_ATRT_SIZE, 1, ifo_file) != 1) {
    perror("VMG_VTS_ATRT");
    exit(1);
  }
  B2N_16(vts_atrt->nr_of_vtss);
  B2N_32(vts_atrt->last_byte);
  
#ifndef NDEBUG
  CHECK_ZERO(vts_atrt->zero_1);
  assert(vts_atrt->nr_of_vtss != 0);
  assert(vts_atrt->nr_of_vtss < 100); //??
  assert(vts_atrt->nr_of_vtss * (4 + VMG_VTS_ATRIBUTES_MIN_SIZE) + 
	 VMG_VTS_ATRT_SIZE < vts_atrt->last_byte + 1);
#endif
  
  info_length = vts_atrt->nr_of_vtss * sizeof(uint32_t);
  data = malloc(info_length);
  if(fread(data, info_length, 1, ifo_file) != 1) {
    perror("VMG_VTS_ATRT offsets");
    exit(1);
  }
  for(i = 0; i < vts_atrt->nr_of_vtss; i++)
    B2N_32(data[i]);
  
#ifndef NDEBUG
  for(i = 0; i < vts_atrt->nr_of_vtss; i++)
    assert(data[i] + VMG_VTS_ATRIBUTES_MIN_SIZE < vts_atrt->last_byte + 1);
#endif
  
  info_length = vts_atrt->nr_of_vtss * VMG_VTS_ATRIBUTES_SIZE;
  vts_atrt->vts_atributes = malloc(info_length);
  for(i = 0; i < vts_atrt->nr_of_vtss; i++) {
    int offset = data[i];
    ifoRead_VTS_ATRIBUTES(&vts_atrt->vts_atributes[i], 
			  sector * DVD_BLOCK_LEN + offset);
#ifndef NDEBUG
    // This assert cant be in ifoRead_VTS_ATRIBUTES
    assert(offset + vts_atrt->vts_atributes[i].last_byte 
	   <= vts_atrt->last_byte + 1); // Check if this is correct
#endif
  }
  free(data);
}



void ifoRead_VMG_TXTDT_MGI(vmg_txtdt_mgi_t *vmg_txtdt_mgi, int sector) {
  
  fseek(ifo_file, sector * DVD_BLOCK_LEN, SEEK_SET);
  if(fread(vmg_txtdt_mgi, VMG_TXTDT_MGI_SIZE, 1, ifo_file) != 1) {
    perror("VMG_TXTDT_MGI");
    exit(1);
  }
  fprintf(stdout, "-- Not done yet --\n");
  
}
