/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort
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

#ifndef HAVE_VIDEO_TYPES_H
#define HAVE_VIDEO_TYPES_H

/*
#define MPEG2_VS_START_CODE_PREFIX 0x000001

#define MPEG2_VS_PICTURE_START_CODE   0x00
#define MPEG2_VS_USER_DATA_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_HEADER_CODE 0xB2
#define MPEG2_VS_SEQUENCE_ERROR_CODE  0xB2
#define MPEG2_VS_EXTENSION_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_END_CODE    0xB2
#define MPEG2_VS_GROUP_START_CODE     0xB2
*/


/* Table 6-1  Start code values */
#define MPEG2_VS_PICTURE_START_CODE       0x00000100
#define MPEG2_VS_SLICE_START_CODE_LOWEST  0x00000101
#define MPEG2_VS_SLICE_START_CODE_HIGHEST 0x000001AF
#define MPEG2_VS_USER_DATA_START_CODE     0x000001B2
#define MPEG2_VS_SEQUENCE_HEADER_CODE     0x000001B3
#define MPEG2_VS_SEQUENCE_ERROR_CODE      0x000001B4
#define MPEG2_VS_EXTENSION_START_CODE     0x000001B5
#define MPEG2_VS_SEQUENCE_END_CODE        0x000001B7
#define MPEG2_VS_GROUP_START_CODE         0x000001B8

/* Table 6-2. extension_start_code_identifier codes. */
#define MPEG2_VS_SEQUENCE_EXTENSION_ID                  0x1
#define MPEG2_VS_SEQUENCE_DISPLAY_EXTENSION_ID          0x2
#define MPEG2_VS_QUANT_MATRIX_EXTENSION_ID              0x3
#define MPEG2_VS_SEQUENCE_SCALABLE_EXTENSION_ID         0x5
#define MPEG2_VS_PICTURE_DISPLAY_EXTENSION_ID           0x7
#define MPEG2_VS_PICTURE_CODING_EXTENSION_ID            0x8
#define MPEG2_VS_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID  0x9
#define MPEG2_VS_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID 0xA

/* Table 6-3. aspect_ratio_information TODO */
/* Table 6-4 --- frame_rate_value TODO */
/* Table 6-5. Meaning of chroma_format TODO */
/* Table 6-10. Definition of scalable_mode TODO */


#define MV_FORMAT_FIELD 0
#define MV_FORMAT_FRAME 1

/* Table 6-17 Meaning of frame_motion_type */
/* Table 6-18 Meaning of field_motion_type */
#define PRED_TYPE_FIELD_BASED 0
#define PRED_TYPE_FRAME_BASED 1
#define PRED_TYPE_DUAL_PRIME 2
#define PRED_TYPE_16x8_MC 3


/* Table 6-14 Meaning of picture_structure */
#define PIC_STRUCT_RESERVED 0
#define PIC_STRUCT_TOP_FIELD 1
#define PIC_STRUCT_BOTTOM_FIELD 2
#define PIC_STRUCT_FRAME_PICTURE 3


/* Table 6-12 Meaning of picture_coding_type */
#define PIC_CODING_TYPE_FORBIDDEN 0
#define PIC_CODING_TYPE_I 1
#define PIC_CODING_TYPE_P 2
#define PIC_CODING_TYPE_B 3
#define PIC_CODING_TYPE_D 4
#define PIC_CODING_TYPE_RESERVED1 5
#define PIC_CODING_TYPE_RESERVED2 6
#define PIC_CODING_TYPE_RESERVED3 7


#ifdef DEBUG
extern unsigned int debug;
extern void exit_program(int exitcode) __attribute__ ((noreturn));
#endif

#ifdef DEBUG

int debug_indent_level;
#define DINDENT(spaces) \
{ \
  debug_indent_level += spaces; \
  if(debug_indent_level < 0) { \
    debug_indent_level = 0; \
  } \
} 

#define DPRINTFI(level, text...) \
if(debug >= level) \
{ \
  fprintf(stderr, "%*s", debug_indent_level, ""); \
  fprintf(stderr, ## text); \
}

#define DPRINTF(level, text...) \
if(debug >= level) \
{ \
  fprintf(stderr, ## text); \
}
#else
#define DINDENT(spaces)
#define DPRINTFI(level, text...)
#define DPRINTF(level, text...)
#endif

#ifdef DEBUG
#define DPRINTBITS(level, bits, value) \
{ \
  int n; \
  for(n = 0; n < bits; n++) { \
    DPRINTF(level, "%u", (value>>(bits-n-1)) & 0x1); \
  } \
}
#else
#define DPRINTBITS(level, bits, value)
#endif













typedef struct { 
	int numberofbits;
	int vlc;
	int value;
} vlc_table_t;

typedef struct { 
	int numberofbits;
	int vlc;
	int run;
	int level;
} vlc_rl_table;


typedef struct {
  uint8_t run;
  int16_t level;
} runlevel_t;

typedef struct {
  uint8_t x;
  uint8_t y;
} pair_uint8_t;



typedef struct {
  uint8_t run, level, len;
} DCTtab;





typedef struct {
  uint16_t macroblock_type; // 6 bits
  uint8_t spatial_temporal_weight_code; // 2 bits
  uint8_t frame_motion_type; // 2 bits (This and field_motion are ..
  uint8_t field_motion_type; // 2 bits ( .. mutually exclusive.         )
  uint8_t dct_type; // 1 bit

  uint8_t macroblock_quant; // 1 bit  -- These are all in macroblock_type
  uint8_t macroblock_motion_forward; // 1 bit 
  uint8_t macroblock_motion_backward; // 1 bit
  uint8_t macroblock_pattern; // 1 bit
  uint8_t macroblock_intra;  // 1 bit
  uint8_t spatial_temporal_weight_code_flag; // 1 bit
  
} macroblock_modes_t;



typedef struct {
  uint16_t dummy1; //macroblock_escape
  uint16_t macroblock_address_increment;
  macroblock_modes_t modes; 
  
  uint8_t dummy2[12]; //pattern_code[12];
  uint8_t cbp;
  uint8_t coded_block_pattern_1;
  uint8_t coded_block_pattern_2;
  uint16_t dc_dct_pred[3];

  int16_t dmv; // 1 bit
  int16_t mv_format; // 1 bit
  int16_t prediction_type; // 2 bits
  
  int16_t dmvector[2];
  int16_t dummy3[2][2][2]; //motion_code[2][2][2];
  int16_t dummy4[2][2][2]; //motion_residual[2][2][2];
  int16_t vector[2][2][2];

  int8_t motion_vector_count; // ? 2 bits
  int8_t motion_vertical_field_select[2][2]; // 4 bits

  int16_t dummy5[2][2][2]; //delta[2][2][2];

  int8_t skipped;

  uint8_t quantiser_scale;
  int intra_dc_mult;

  int16_t QFS[64] __attribute__ ((aligned (8)));

} macroblock_t;


typedef struct {
  uint8_t slice_vertical_position;
  uint8_t slice_vertical_position_extension;
  uint8_t priority_breakpoint;
  uint8_t quantiser_scale_code;
  uint8_t intra_slice_flag;
  uint8_t intra_slice;
  uint8_t reserved_bits;
  uint8_t extra_bit_slice;
  uint8_t extra_information_slice;

} slice_t;


typedef struct {
  uint8_t extension_start_code_identifier;
  uint8_t f_code[2][2];
  uint8_t intra_dc_precision;
  uint8_t picture_structure;
  uint8_t top_field_first;
  uint8_t frame_pred_frame_dct;
  uint8_t concealment_motion_vectors;
  uint8_t q_scale_type;
  uint8_t intra_vlc_format;
  uint8_t alternate_scan;
  uint8_t repeat_first_field;
  uint8_t chroma_420_type;
  uint8_t progressive_frame;
  uint8_t composite_display_flag;
  uint8_t v_axis;
  uint8_t field_sequence;
  uint8_t sub_carrier;
  uint8_t burst_amplitude;
  uint8_t sub_carrier_phase;

} picture_coding_extension_t;


typedef struct {
  uint16_t temporal_reference;
  uint8_t picture_coding_type;
  uint16_t vbv_delay;
  uint8_t full_pel_vector[2];
  uint8_t forward_f_code;
  uint8_t backward_f_code;
  uint8_t extra_bit_picture;
  uint8_t extra_information_picture;  

} picture_header_t;


typedef struct {
  picture_header_t header;
  picture_coding_extension_t coding_ext;

  int16_t PMV[2][2][2];

} picture_t;


typedef struct {
  uint16_t horizontal_size_value;
  uint16_t vertical_size_value;
  uint8_t aspect_ratio_information;
  uint8_t frame_rate_code;
  uint32_t bit_rate_value;
  uint16_t vbv_buffer_size_value;
  uint8_t constrained_parameters_flag;
  uint8_t load_intra_quantiser_matrix;
  uint8_t intra_quantiser_matrix[64];
  uint8_t load_non_intra_quantiser_matrix;
  uint8_t non_intra_quantiser_matrix[64];
  
  /***/
  uint8_t intra_inverse_quantiser_matrix[64];
  uint8_t non_intra_inverse_quantiser_matrix[64];

  /***/
  int16_t scaled_intra_inverse_quantiser_matrix[8][8];

} sequence_header_t;


typedef struct {
  uint8_t extension_start_code_identifier;
  uint8_t profile_and_level_indication;
  uint8_t progressive_sequence;
  uint8_t chroma_format;
  uint8_t horizontal_size_extension;
  uint8_t vertical_size_extension;
  uint16_t bit_rate_extension;
  uint8_t vbv_buffer_size_extension;
  uint8_t low_delay;
  uint8_t frame_rate_extension_n;
  uint8_t frame_rate_extension_d;
  
} sequence_extension_t;


typedef struct {
  uint8_t extension_start_code_identifier;

  uint8_t video_format;
 
  uint8_t colour_description;
  uint8_t colour_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coefficients;
  
  uint16_t display_horizontal_size;
  uint16_t display_vertical_size;

} sequence_display_extension_t;


typedef struct {
  sequence_header_t header;
  sequence_extension_t ext;
  sequence_display_extension_t dpy_ext;
  /***/
  int16_t horizontal_size;
  int16_t vertical_size;

  int16_t mb_width;
  int16_t mb_height;
  int16_t mb_row;
  int16_t mb_column;
  int16_t macroblock_address;
  
} sequence_t;






extern sequence_t seq;
extern picture_t pic;
extern slice_t slice_data;
extern macroblock_t mb;






#endif
