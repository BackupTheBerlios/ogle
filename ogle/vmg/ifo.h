#ifndef __IFO_H__
#define __IFO_H__

/* 
 * Copyright (C) 2000 Björn Englund <d4bjorn@dtek.chalmers.se>, 
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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

typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int mpeg_version         : 2;
  unsigned int video_format         : 2;
  unsigned int display_aspect_ratio : 2;
  unsigned int permitted_df         : 2;
  
  unsigned int line21_CC_1          : 1;
  unsigned int line21_CC_2          : 1;
  unsigned int unknown1             : 2;
  
  unsigned int picture_size         : 2;
  unsigned int letterboxed          : 1;
  unsigned int film_mode            : 1;
#else
  unsigned int permitted_df         : 2;
  unsigned int display_aspect_ratio : 2;
  unsigned int video_format         : 2;
  unsigned int mpeg_version         : 2;
  
  unsigned int film_mode            : 1;
  unsigned int letterboxed          : 1;
  unsigned int picture_size         : 2;
  
  unsigned int unknown1             : 2;
  unsigned int line21_CC_2          : 1;
  unsigned int line21_CC_1          : 1;
#endif
} __attribute__ ((packed)) video_attr_t;

typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int audio_format           : 3;
  unsigned int multichannel_extension : 1;
  unsigned int lang_type              : 2;
  unsigned int application_mode       : 2;
  
  unsigned int quantization           : 2;
  unsigned int sample_frequency       : 2;
  unsigned int unknown1               : 1;
  unsigned int channels               : 3;
#else
  unsigned int application_mode       : 2;
  unsigned int lang_type              : 2;
  unsigned int multichannel_extension : 1;
  unsigned int audio_format           : 3;
  
  unsigned int channels               : 3;
  unsigned int unknown1               : 1;
  unsigned int sample_frequency       : 2;
  unsigned int quantization           : 2;
#endif
  unsigned char lang_code[2];
  uint8_t lang_extension; // ?? and if so use a [3] instead?
  uint8_t audio_type;
  uint16_t unknown2;
} __attribute__ ((packed)) audio_attr_t;

typedef struct {
  /*
   * type: 0 not specified
   *       1 language
   *       2 other
   */

  /*
   * coding mode: 0 run length
   *              1 extended
   *              2 other
   */
  
  /* 
   * language: indicates language if type == 1
   */
  
  /*
   * lang extension: if type == 1 contains the lang extension
   */
  uint8_t user_selectable; //?? 
  uint8_t zero1;
  unsigned char lang_code[2];
  uint8_t zero2; // lang ext?
  uint8_t lang_extension;
} __attribute__ ((packed)) subp_attr_t;



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
  
  video_attr_t vmgm_video_attributes;
  uint8_t  zero_7;
  uint8_t  nr_of_vmgm_audio_streams;
  audio_attr_t vmgm_audio_attributes[8]; //10?
  uint8_t  zero_8[16];
  uint8_t  zero_9;
  uint8_t  nr_of_vmgm_subp_streams;
  subp_attr_t vmgm_subp_attributes[28]; //32?
  //how much 'padding' here?
} __attribute__ ((packed)) vmgi_mat_t;

typedef struct {
  uint8_t bytes[8];
} __attribute__ ((packed)) vm_cmd_t;
#define COMMAND_DATA_SIZE 8
  
typedef struct { // PGC Command Table
  uint16_t nr_of_pre;
  uint16_t nr_of_post;
  uint16_t nr_of_cell;
  uint16_t zero_1;
  vm_cmd_t *pre_commands;
  vm_cmd_t *post_commands;
  vm_cmd_t *cell_commands;
} __attribute__ ((packed)) pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8

typedef uint8_t __attribute__ ((packed)) pgc_program_map_t; 

typedef struct { // Cell Playback Information
  /*
     the first one category of C_PBI has a lot of flags.
     9 flags, cell type cell still time and cell command number
    XX-- ----  Cel block mode
     00: not in a block
     01: the first cell in the block
     10: a cell in the block
     11: last cell in the block.
    --XX ---- block type 
     00: not part of a block
     01: angle block
     10: ??????????????
     11: ??????????????
     if block type 00b then Cel block mode shall also be 00b
     you select one cell of the angleblock based on the setting of the
     angle number system parameter.
     an angle block is a block of cells. each cell is for one angle.
     cells are interleaved to be able to do the seamless angle switching 
     on 1,5 x speed drives with el cheapo hardware
     e.x. cell 1 start sector 10 end sector 300, 
          cell 2 start sector 20 end 315
     You start play at either one depending on which angle you want
    ---- Y--- seamless playback flag. 
     0: cell not presented seamlessly
     1: presented seamlessly
    ---- -Y-- interleaved allocation flag: decribes wheter cell exists in 
              contiguous block or interleaved block. (interleaved is not 
              always angle block) 
     0: contiguous
     1: interleaved block
    ---- --Y- STC discontinuity flag.
     has something to do wich seamless playback..
    ---- ---Y seamless angle flag. 
     0: only non seamless angle change function shall be used
        (nsml_agli, in pci, should be used)
     1: only seamless angle change .... (sml_agli, in dsi, should be used)
     
    ---- ---- -X-- ----  -> Cell access restrion flag.
  */
  //uint32_t category;
#ifdef WORDS_BIGENDIAN
  unsigned int block_mode : 2;
  unsigned int block_type : 2;
  unsigned int seamless_play : 1;
  unsigned int interleaved : 1;
  unsigned int STC_discontinuty : 1;
  unsigned int seamless_angle : 1;
  
  unsigned int unknown1 : 1;
  unsigned int restricted : 1;
  unsigned int unknown2 : 6;
#else
  unsigned int seamless_angle : 1;
  unsigned int STC_discontinuty : 1;
  unsigned int interleaved : 1;
  unsigned int seamless_play : 1;
  unsigned int block_type : 2;
  unsigned int block_mode : 2;
  
  unsigned int unknown2 : 6;
  unsigned int restricted : 1;
  unsigned int unknown1 : 1;
#endif
  uint8_t still_time;
  uint8_t cell_cmd_nr;
  dvd_time_t playback_time;
  uint32_t first_sector;
  uint32_t first_ilvu_end_sector;
  uint32_t last_vobu_start_sector;
  uint32_t last_sector;
} __attribute__ ((packed)) cell_playback_t;

typedef struct { // Cell Position Information
  uint16_t vob_id_nr;
  uint8_t  zero_1;
  uint8_t  cell_nr;
} __attribute__ ((packed)) cell_position_t;

typedef struct { // Program Chain Information
  uint16_t zero_1;
  uint8_t  nr_of_programs;
  uint8_t  nr_of_cells;
  dvd_time_t playback_time;
  uint32_t prohibited_ops; // New type?
  //Y--- -XXX ---- ----     Y = availability, XXX = audio stream (sub)id
  //  for linear pcm, ac3, dts or sdds it's the sub_stream_id number
  //  for mpeg1/mpeg2 it's the stream_id number
  uint16_t audio_control[8]; // New type
  uint32_t subp_control[32]; // New type // 0x80,4:3,wide,letterbox,pan-scan
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
  cell_playback_t *cell_playback_tbl;
  cell_position_t *cell_position_tbl;
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
  unsigned char lang_code[2];
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
  unsigned char country_code[2];
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
  
  video_attr_t vtsm_vobs_attributes;
  uint8_t zero_1;
  uint8_t nr_of_vtsm_audio_streams;
  audio_attr_t vtsm_audio_attributes[8]; //10?
  uint8_t zero_2[16];
  uint8_t zero_3;
  uint8_t nr_of_vtsm_subp_streams;
  subp_attr_t vtsm_subp_attributes[28]; // Why not 32??
  
  uint8_t zero_4[2];
  
  video_attr_t vtstt_vobs_video_attributes;
  uint8_t zero_5;
  uint8_t nr_of_vtstt_audio_streams;
  audio_attr_t vtstt_audio_attributes[8]; //10?
  uint8_t zero_6[16];
  uint8_t zero_7;
  uint8_t nr_of_vtstt_subp_streams;
  subp_attr_t vtstt_subp_attributes[32]; // not allways room for 32...
} __attribute__ ((packed)) vts_attributes_t;
#define VMG_VTS_ATTRIBUTES_SIZE 546
#define VMG_VTS_ATTRIBUTES_MIN_SIZE 354

typedef struct { // Video Title Set Attribute Table
  uint16_t nr_of_vtss;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_attributes_t *vts_attributes;
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
  unsigned char lang_code[2];
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
  
  video_attr_t vtsm_video_attributes;
  uint8_t zero_14;
  uint8_t nr_of_vtsm_audio_streams;
  audio_attr_t vtsm_audio_attributes[8]; //10?
  uint8_t zero_15[16];
  uint8_t zero_16;
  uint8_t nr_of_vtsm_subp_streams;
  subp_attr_t vtsm_subp_attributes[28]; // Why not 32??
  
  uint8_t zero_17[2];
  
  video_attr_t vts_video_attributes;
  uint8_t zero_18;
  uint8_t nr_of_vts_audio_streams;
  audio_attr_t vts_audio_attributes[8]; //10?
  uint8_t zero_19[16];
  uint8_t zero_20;
  uint8_t nr_of_vts_subp_streams;
  subp_attr_t vts_subp_attributes[32];
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


#endif /* __IFO_H__ */
