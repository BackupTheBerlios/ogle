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
  uint8_t frame_u; // The two high bits are the frame rate.
}  __attribute__ ((packed)) dvd_time_t;

/* -------------------------------- VMGM ----------------------------------- */

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
  uint32_t first_play_pgc;
  uint8_t  zero_5[56];
  uint32_t vmgm_vobs;		// sector
  uint32_t vmg_ptt_srpt;	// sector
  uint32_t vmgm_pgci_ut;	// sector
  uint32_t vmg_ptl_mait;	// sector
  uint32_t vmg_vts_atrt;	// sector
  uint32_t vmg_txtdt_mgi;	// sector
  uint32_t vmgm_c_adt;		// sector
  uint32_t vmgm_vobu_admap;	// sector
  uint8_t  zero_6[32];
  uint16_t vmgm_video_attributes; // new type video_attributes
  uint8_t  zero_7;
  uint8_t  nr_of_vmgm_audio_streams;
  uint8_t  vmgm_audio_attributes[8][8]; // new type?
  uint8_t  zero_8[17];
  uint8_t  nr_of_vmgm_subp_streams;
  uint8_t  vmgm_subp_attributes[28][6]; // new type?
  //how much 'padding' here?
} __attribute__ ((packed)) vmgi_mat_t;

typedef uint8_t __attribute__ ((packed)) command_data_t[8];
#define COMMAND_DATA_SIZE 8
  
typedef struct { // PGC Command Table
  uint16_t nr_of_pre;
  uint16_t nr_of_post;
  uint16_t nr_of_cell;
  uint16_t zero_1;
  command_data_t *pre_commands;
  command_data_t *post_commands;
  command_data_t *cell_commands;
} __attribute__ ((packed)) pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8

typedef uint8_t __attribute__ ((packed)) pgc_program_map_t; 

typedef struct { // Cell Playback Information
  uint32_t category;
  dvd_time_t playback_time;
  uint32_t first_sector;
  uint32_t first_ilvu_end_sector;
  uint32_t last_vobu_start_sector;
  uint32_t last_sector;
} __attribute__ ((packed)) cell_playback_tbl_t;

typedef struct { // Cell Position Information
  uint16_t vob_id_nr;
  uint8_t  zero_1;
  uint8_t  cell_nr;
} __attribute__ ((packed)) cell_position_tbl_t;

typedef struct { // Program Chain Information
  uint16_t zero_1;
  uint8_t  nr_of_programs;
  uint8_t  nr_of_cells;
  dvd_time_t playback_time;
  uint32_t prohibited_ops; // New type?
  uint16_t audio_status[8]; // New type
  uint32_t subp_status[32]; // New type
  uint16_t next_pgc_nr;
  uint16_t prev_pgc_nr;
  uint16_t goup_pgc_nr;
  uint8_t  still_time;
  uint8_t  pg_playback_mode;
  uint32_t palette[16]; // New type struct {zero_1, Y, Cr, Cb}
  uint16_t pgc_command_tbl_offset;
  uint16_t pgc_program_map_offset;
  uint16_t cell_playback_tbl_offset;
  uint16_t cell_position_tbl_offset;
  pgc_command_tbl_t *pgc_command_tbl;
  pgc_program_map_t  *pgc_program_map;
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

typedef struct { // PartOfTitle Search Pointer Table
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  title_info_t *title_info;
} __attribute__ ((packed)) vmg_ptt_srpt_t;
#define VMG_PTT_SRPT_SIZE 8


typedef struct { // Program Chain Information Search Pointer
  uint32_t pgc_category;
  uint32_t pgc_start_byte;
  pgc_t *pgc;
} __attribute__ ((packed)) pgci_srp_t;
#define PGCI_SRP_SIZE 8

typedef struct { // Program Chain Information Table
  uint16_t nr_of_pgci_srp;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_srp_t *pgci_srp;
} __attribute__ ((packed)) pgcit_t;
#define PGCIT_SIZE 8

typedef struct { // Menu PGCI Language Unit Table
  uint16_t lang_code;
  uint8_t  zero_1;
  uint8_t  exists;
  uint32_t lang_start_byte; // prt
  pgcit_t *menu_pgcit;
} __attribute__ ((packed)) menu_pgci_lu_t;
#define MENU_PGCI_LU_SIZE 8

typedef struct { // Menu PGCI Unit Table
  uint16_t nr_of_lang_units;
  uint16_t zero_1;
  uint32_t last_byte;
  menu_pgci_lu_t *menu_lu;
} __attribute__ ((packed)) menu_pgci_ut_t;
#define MENU_PGCI_UT_SIZE 8


typedef struct { // Parental Management Information Unit Table
  char country_code[2];
  uint16_t zero_1;
  uint16_t pf_ptl_mai_start_byte;
  uint16_t zero_2;
  //uint16_t *pf_ptl_mai // table of nr_of_vtss+1 x 8
} __attribute__ ((packed)) ptl_mait_country_t;
#define VMG_PTL_MAIT_COUNTRY_SIZE 8

typedef struct { // Parental Management Information Table
  uint16_t nr_of_countries;
  uint16_t nr_of_vtss;
  uint32_t last_byte;
  ptl_mait_country_t *countries;
} __attribute__ ((packed)) vmg_ptl_mait_t;
#define VMG_PTL_MAIT_SIZE 8


typedef struct { // Video Title Set Attribute
  uint32_t last_byte;
  uint32_t vts_cat;
  
  uint16_t vtsm_vobs_attributes; // new type video_attributes
  uint8_t  zero_1;
  uint8_t  nr_of_vtsm_audio_streams;
  uint8_t  vtsm_audio_attributes[8][8]; //? 10
  uint8_t  zero_2[16];
  uint8_t  zero_3;
  uint8_t  nr_of_vtsm_subp_streams;
  uint8_t  vtsm_subp_attributes[28][6]; // Why not all 32??
  uint8_t  zero_4[2];
  
  uint16_t vtstt_vobs_video_attributes;
  uint8_t  zero_5;
  uint8_t  nr_of_vtstt_audio_streams;
  uint8_t  vtstt_audio_attributes[8][8]; //? 10
  uint8_t  zero_6[16];
  uint8_t  zero_7;
  uint8_t  nr_of_vtstt_subp_streams;
  uint8_t  vtstt_subp_attributes[32][6]; // Are there room for 32 here? (no?)
  /* ? */
} __attribute__ ((packed)) vts_atributes_t;
#define VMG_VTS_ATRIBUTES_SIZE 546
#define VMG_VTS_ATRIBUTES_MIN_SIZE 354

typedef struct { // Video Title Set Attribute Table
  uint16_t nr_of_vtss;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_atributes_t *vts_atributes;
} __attribute__ ((packed)) vmg_vts_atrt_t;
#define VMG_VTS_ATRT_SIZE 8


typedef struct { // Text Data
  uint32_t last_byte;// offsets are relative here
  uint16_t offsets[100]; // == nr_of_srpts + 1 (first is disc title)
#if 0  
  uint16_t unknown; // 0x48 ?? 0x48 words (16bit) info following
  uint16_t zero_1;
  
  uint8_t type_of_info;//?? 01 == disc, 02 == Title, 04 == Title part 
  uint8_t unknown1;
  uint8_t unknown2;
  uint8_t unknown3;
  uint8_t unknown4;//?? allways 0x30 language?, text format?
  uint8_t unknown5;
  uint16_t offset; // from first 
  
  char text[12]; // ended by 0x09
#endif
}  __attribute__ ((packed)) txtdt_t;

  
typedef struct { // Text Data Language Unit 
  uint16_t lang_code;
  uint16_t unknown; // 0x0001, title 1? disc 1? side 1?
  uint32_t _start_byte; // prt, rel start of vmg_txtdt_mgi
  txtdt_t  *txtdt;
}  __attribute__ ((packed)) vmg_txtdt_lu_t;
#define VMG_TXTDT_LU_SIZE 8

typedef struct { // Text Data Manager Information
  char disc_name[14];//how many bytes??
  uint16_t nr_of_language_units;//32bit??
  uint32_t last_byte;
  vmg_txtdt_lu_t *txtdt_lu;
} __attribute__ ((packed)) vmg_txtdt_mgi_t;
#define VMG_TXTDT_MGI_SIZE 20



typedef struct { // Cell Adress information
  uint16_t vob_id;
  uint8_t  cell_id;
  uint8_t  zero_1;
  uint32_t start_sector;
  uint32_t last_sector;
} __attribute__ ((packed)) cell_adr_t;

typedef struct { // Cell Adress Table
  uint16_t nr_of_vobs; // VOBs 
  uint16_t zero_1;
  uint32_t last_byte;
  cell_adr_t *cell_adr_table;
} __attribute__ ((packed)) c_adt_t;
#define C_ADT_SIZE 8


typedef struct { // VOBU Address Map
  uint32_t last_byte;
  uint32_t *vobu_start_sectors;
} __attribute__ ((packed)) vobu_admap_t;
#define VOBU_ADMAP_SIZE 4




/* --------------------------------- VTS ----------------------------------- */

typedef struct { // Video Title Set Information Management Table
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
  uint32_t vtsm_vobu_admap;	// sector ptr
  uint32_t vts_c_adt;		// sector ptr
  uint32_t vts_vobu_admap;	// sector ptr
  uint8_t  zero_13[24];
  
  uint16_t vtsm_video_attributes;
  uint8_t  zero_14;
  uint8_t  nr_of_vtsm_audio_streams;
  uint8_t  vtsm_audio_attributes[8][8]; // new type
  uint8_t  zero_15[17];//??
  uint8_t  nr_of_vtsm_subp_streams;
  uint8_t  vtsm_subp_attributes[28][6]; // new type
  uint8_t  zero_16[2];//??
  
  uint16_t vts_video_attributes;
  uint8_t  zero_17;
  uint8_t  nr_of_vts_audio_streams;
  uint8_t  vts_audio_attributes[8][8]; // new type
  uint8_t  zero_18[17];//??
  uint8_t  nr_of_vts_subp_streams;
  uint8_t  vts_subp_attributes[32][6]; // new type
  //how much 'padding' here, if any?
} __attribute__ ((packed)) vtsi_mat_t;

typedef struct { // PartOfTitle Unit Information
  uint16_t pgcn;
  uint16_t pgn;
} ptt_info_t;

typedef struct { // PartOfTitle Information
  uint16_t nr_of_ptts;
  ptt_info_t *ptt_info;
} ttu_t;

typedef struct { // PartOfTitle Search Pointer Table
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  ttu_t  *title_info; // New type.
} __attribute__ ((packed)) vts_ptt_srpt_t;
#define VTS_PTT_SRPT_SIZE 8





/* -- ifo_read.c ----------------------------------------------------------- */

//void ifoOpen_DVD?(unsigned int *sector);
int ifoOpen_VMG(vmgi_mat_t *vmgi_mat, char *filename);
int ifoOpen_VTS(vtsi_mat_t *vtsi_mat, char *filename);
void ifoClose();

void ifoRead_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait, int sector);
void ifoRead_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt, int sector);
void ifoRead_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt, int sector);
void ifoRead_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt, int sector);
void ifoRead_PGC(pgc_t *pgc, int offset);
void ifoRead_PGCIT(pgcit_t *pgcit, int offset);
void ifoRead_MENU_PGCI_UT(menu_pgci_ut_t *menu_pgci_ut, int sector);
void ifoRead_C_ADT(c_adt_t *c_adt, int sector);
void ifoRead_VOBU_ADMAP(vobu_admap_t *vobu_admap, int sector);
void ifoRead_VMG_TXTDT_MGI(vmg_txtdt_mgi_t *vmg_txtdt_mgi, int sector);


/* -- ifo_print.c ---------------------------------------------------------- */

void ifoPrint_time(int level, dvd_time_t *time);
void ifoPrint_COMMAND(int row, uint8_t *command); // remove

void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat);
void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat);

void ifoPrint_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait);
void ifoPrint_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt);
void ifoPrint_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt);
void ifoPrint_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt);
void ifoPrint_PGC(pgc_t *pgc);
void ifoPrint_PGCIT(pgcit_t *pgcit);
void ifoPrint_MENU_PGCI_UT(menu_pgci_ut_t *menu_pgci_ut);
void ifoPrint_C_ADT(c_adt_t *c_adt);
void ifoPrint_VOBU_ADMAP(vobu_admap_t *vobu_admap);


#endif
