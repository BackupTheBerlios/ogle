#ifndef __IFO_H__
#define __IFO_H__

#include <inttypes.h>

#ifndef DVD_BLOCK_LEN
#define DVD_BLOCK_LEN 2048
#endif


typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; // The two high bits are the fps.
} dvd_time_t;

typedef struct { // Video Manager Information Management Table
  char     vmg_identifier[12];
  uint32_t vmg_last_sector;
  uint8_t  zero_1[12];
  uint32_t vmgi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vmg_category;
  uint16_t vmg_nr_of_volumes;
  uint16_t vmg_this_volume_nr;
  uint8_t  disc_side;
  uint8_t  zero_3[19];
  uint16_t vmg_nr_of_title_sets; // Number of VTSs.
  char     provider_identifier[32];
  uint64_t vmg_pos_code;
  uint8_t  zero_4[24];
  uint32_t vmgi_last_byte;
  uint32_t first_play_pgc; // ptr
  uint8_t  zero_5[56];
  uint32_t vmgm_vobs;		// sector ptr
  uint32_t vmg_ptt_srpt;	// sector ptr
  uint32_t vmgm_pgci_ut;	// sector ptr
  uint32_t vmg_plt_mait;	// sector ptr
  uint32_t vmg_vts_atrt;	// sector ptr
  uint32_t vmg_txtdt_mg;	// sector ptr
  uint32_t vmgm_c_adt;		// sector ptr
  uint32_t vmgm_voubu_admap; // sector ptr
  uint8_t  zero_6[32];
  uint16_t vmgm_video_attributes;
  uint8_t  zero_7;
  uint8_t  nr_of_vmgm_audio_streams;
  uint8_t  audio_attributes[8]; // new type
  uint8_t  zero_8[73];
  uint8_t  nr_of_vmgm_subp_streams;
  uint8_t  subp_attributes[6]; // new type
  //how much 'padding' here?
} __attribute__ ((packed)) vmgi_mat_t;

typedef uint8_t command_data_t[8];
#define COMMAND_DATA_SIZE 8
  
typedef struct {
  uint16_t nr_of_pre;
  uint16_t nr_of_post;
  uint16_t nr_of_cell;
  uint16_t zero_1;
  command_data_t *pre_commands; // New type
  command_data_t *post_commands; // New type
  command_data_t *cell_commands; // New type
} __attribute__ ((packed)) pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8

typedef uint8_t pgc_progam_map_t; 

typedef struct {
  uint32_t category;
  dvd_time_t playback_time;
  uint32_t first_sector;
  uint32_t first_ilvu_end_sector;
  uint32_t last_vobu_start_sector;
  uint32_t last_sector;
} __attribute__ ((packed)) cell_playback_tbl_t;

typedef struct {
  uint16_t vob_id_nr;
  uint8_t  zero_1;
  uint8_t  cell_nr;
} __attribute__ ((packed)) cell_position_tbl_t;

typedef struct {
  uint16_t zero_1;
  uint8_t  nr_of_programs;
  uint8_t  nr_of_cells;
  dvd_time_t playback_time;
  uint32_t prohibited_ops; // New type
  uint16_t audio_status[8]; // New type
  uint32_t subp_status[32]; // New type
  uint16_t next_pgc_nr;
  uint16_t prev_pgc_nr;
  uint16_t goup_pgc_nr;
  uint8_t  still_time;
  uint8_t  pg_playback_mode;
  uint32_t palette[16]; // New type {zero_1, Y, Cr, Cb}
  uint16_t pgc_command_tbl_offset;
  uint16_t pgc_program_map_offset;
  uint16_t cell_playback_tbl_offset;
  uint16_t cell_position_tbl_offset;
  pgc_command_tbl_t *pgc_command_tbl;
  pgc_progam_map_t  *pgc_progam_map;
  cell_playback_tbl_t *cell_playback_tbl;
  cell_position_tbl_t *cell_position_tbl;
} __attribute__ ((packed)) pgc_t;
#define PGC_SIZE 236

typedef struct {
  uint8_t  playback_type;
  uint8_t  nr_of_angles;
  uint16_t nr_of_ptts;
  uint16_t parental_id;
  uint8_t  title_set_nr;
  uint8_t  vts_ttn;
  uint32_t title_set_sector; // sector ptr
} __attribute__ ((packed)) title_info_t;

typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  title_info_t  *title_info; // New type.
} __attribute__ ((packed)) vmg_ptt_srpt_t;
#define VMG_PTT_SRPT_SIZE 8


typedef struct {
  uint16_t lang_code;
  uint8_t  zero_1;
  uint8_t  exists;
  uint32_t lang_start_byte; // prt
} __attribute__ ((packed)) vmgm_pgci_lu_t;

typedef struct {
  uint16_t nr_of_lang_units;
  uint16_t zero_1;
  uint32_t last_byte;
  vmgm_pgci_lu_t *menu_lu;
} __attribute__ ((packed)) vmgm_pgci_ut_t;


typedef struct {
  uint16_t country_code;
  uint16_t zero_1;
  uint16_t start_byte_of_pf_ptl_mai;
  uint16_t zero_2;
} __attribute__ ((packed)) ptl_mait_country_t;

typedef struct {
  uint8_t  nr_of_countries;
  uint8_t  nr_of_vtss;
  uint32_t last_byte;
  ptl_mait_country_t *countries;
} __attribute__ ((packed)) vmg_ptl_mait_t;



typedef struct {
  uint32_t last_byte;
  uint32_t vts_cat;
  uint16_t vtsm_vobs_attributes; // maybe video_attributes ?? 
  uint8_t  zero_1;
  uint8_t  nr_of_vtsm_audio_strams;
  uint8_t  vtsm_audio_attributes[8];
  uint8_t  zero_2[72];
  uint8_t  zero_3;
  uint8_t  nr_of_vtsm_subp_streams;
  uint8_t  vtsm_subp_attributes[32];
  uint8_t  zero_4[138];
  
  uint16_t vtstt_vobs_video_attributes;
  uint8_t  zero_5;
  uint8_t  nr_of_vtstt_audio_streams;
  uint8_t  vtstt_audio_attributes[8];
  uint8_t  zero_6[72];
  uint8_t  zero_7;
  uint8_t  nr_of_vtstt_subp_streams;
  uint8_t  vtstt_subp_attributes[32];
} __attribute__ ((packed)) vts_atributes_t;

typedef struct {
  uint16_t nr_of_vtss;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_atributes_t *vts_atributes;
} __attribute__ ((packed)) vmg_vts_atrt_t;



typedef struct {
  uint16_t vob_id;
  uint8_t  cell_id;
  uint8_t  zero_1;
  uint32_t start_sector;
  uint32_t last_sector;
} __attribute__ ((packed)) cell_adr_t;

typedef struct {
  uint16_t nr_of_vobs; // VOBs 
  uint16_t zero_1;
  uint32_t last_byte;
  cell_adr_t *cell_adr_table;
} __attribute__ ((packed)) vmgm_c_adt_t;



typedef struct {
  uint32_t last_byte;
  uint32_t *vobu_start_sectors;
} __attribute__ ((packed)) vmgm_vobu_admap_t;




/* --------------------------------- VTS ----------------------------------- */
   

typedef struct { // Video Manager Information Management Table
  char vts_identifier[12];
  uint32_t vts_last_sector;
  uint8_t  zero_1[12];
  uint32_t vtsi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vts_category;
  uint16_t zero_3;
  uint16_t zero_4;
  uint8_t  zero_5;
  uint8_t  zero_6[19];
  uint16_t zero_7;
  char zero_8[32];
  uint64_t zero_9;
  uint8_t  zero_10[24];
  uint32_t vtsi_last_byte;
  uint32_t zero_11;
  uint8_t  zero_12[56];
  uint32_t vtsm_vobs;		// sector ptr
  uint32_t vtstt_vobs;		// sector ptr
  uint32_t vts_ptt_srpt;	// sector ptr
  uint32_t vts_pgcit;		// sector ptr
  uint32_t vtsm_pgci_ut;	// sector ptr
  uint32_t vts_tmapt;		// sector ptr
  uint32_t vtsm_c_adt;		// sector ptr
  uint32_t vtsm_voubu_admap;	// sector ptr
  uint32_t vts_c_adt;		// sector ptr
  uint32_t vts_voubu_admap;	// sector ptr
  uint8_t  zero_13[24];
  uint16_t vtsm_video_attributes;
  uint8_t  zero_14;
  uint8_t  nr_of_vtsm_audio_streams;
  uint8_t  vtsm_audio_attributes[8]; // new type
  uint8_t  zero_15[73];
  uint8_t  nr_of_vtsm_subp_streams;
  uint8_t  vtsm_subp_attributes[6]; // new type
  uint8_t  zero_16[164];//??
  
  uint16_t vts_video_attributes;
  uint8_t  zero_17;
  uint8_t  nr_of_vts_audio_streams;
  uint8_t  vts_audio_attributes[8][8]; // new type ??
  uint8_t  zero_18[17];
  uint8_t  nr_of_vts_subp_streams;
  uint8_t  vts_subp_attributes[6][32]; // new type
  //how much 'padding' here?
} __attribute__ ((packed)) vtsi_mat_t;






#endif
