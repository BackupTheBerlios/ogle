/* SKROMPF - A video player
 * Copyright (C) 2000 Bj�rn Englund, H�kan Hjort
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include "video_stream.h"
#include "video_types.h"

#ifdef HAVE_MLIB
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>
#else
#ifdef HAVE_MMX
#include "mmx.h"
#endif
#include "c_mlib.h"
#endif

#include "../include/common.h"

#include "c_getbits.h"
#include "video_tables.h"










extern yuv_image_t *dst_image;
extern yuv_image_t *fwd_ref_image;


extern void next_start_code(void);
extern void exit_program(int exitcode) __attribute__ ((noreturn));
extern void motion_comp();
extern void motion_comp_add_coeff(unsigned int i);
extern int get_vlc(const vlc_table_t *table, char *func);


/*
  Functions defined within this file:
  
  void mpeg2_slice(void);
  void block_intra(unsigned int);
  void block_non_intra(unsigned int);
  void macroblock(void);
  void macroblock_modes(void);
  void coded_block_pattern(void);
  void reset_dc_dct_pred(void);
  void reset_PMV();
  void reset_vectors();
  void motion_vectors(unsigned int s);
  void motion_vector(int r, int s);
*/







static
void reset_dc_dct_pred(void)
{
  DPRINTF(3, "Resetting dc_dct_pred\n");
  
  /* Table 7-2. Relation between intra_dc_precision and the predictor
     reset value */
  /*
  switch(pic.coding_ext.intra_dc_precision) {
  case 0:
    mb.dc_dct_pred[0] = 128;
    mb.dc_dct_pred[1] = 128;
    mb.dc_dct_pred[2] = 128;
    break;
  case 1:
    mb.dc_dct_pred[0] = 256;
    mb.dc_dct_pred[1] = 256;
    mb.dc_dct_pred[2] = 256;
    break;
  case 2:
    mb.dc_dct_pred[0] = 512;
    mb.dc_dct_pred[1] = 512;
    mb.dc_dct_pred[2] = 512;
    break;
  case 3:
    mb.dc_dct_pred[0] = 1024;
    mb.dc_dct_pred[1] = 1024;
    mb.dc_dct_pred[2] = 1024;
    break;
  default:
    fprintf(stderr, "*** reset_dc_dct_pred(), invalid intra_dc_precision\n");
    exit_program(-1);
    break;
  }
  */
  mb.dc_dct_pred[0] = 128 << pic.coding_ext.intra_dc_precision;
  mb.dc_dct_pred[1] = 128 << pic.coding_ext.intra_dc_precision;
  mb.dc_dct_pred[2] = 128 << pic.coding_ext.intra_dc_precision;
}

static
void reset_PMV()
{
  DPRINTF(3, "Resetting PMV\n");

  pic.PMV[0][0][0] = 0;
  pic.PMV[0][0][1] = 0;
  pic.PMV[0][1][0] = 0;
  pic.PMV[0][1][1] = 0;
  pic.PMV[1][0][0] = 0;
  pic.PMV[1][0][1] = 0;
  pic.PMV[1][1][0] = 0;
  pic.PMV[1][1][1] = 0;
}

static
void reset_vectors()
{
  DPRINTF(3, "Resetting motion vectors\n");
  
  mb.vector[0][0][0] = 0;
  mb.vector[0][0][1] = 0;
  mb.vector[1][0][0] = 0;
  mb.vector[1][0][1] = 0;
  mb.vector[0][1][0] = 0;
  mb.vector[0][1][1] = 0;
  mb.vector[1][1][0] = 0;
  mb.vector[1][1][1] = 0;
}




static inline
void inverse_quantisation_final(int sum)
{
  if((sum & 1) == 0) {
    if((mb.QFS[63]&1) != 0) {
      mb.QFS[63] = mb.QFS[63]-1;
    } else {
      mb.QFS[63] = mb.QFS[63]+1;
    }
  }
}


/* 6.2.6 Block */
static
void block_intra(unsigned int i)
{
  unsigned int n;
  int inverse_quantisation_sum;
  
  unsigned int left;
  unsigned int offset;
  uint32_t word;
  
  DPRINTF(3, "pattern_code(%d) set\n", i);
  
  { // Reset all coefficients to 0.
    int m;
    for(m=0; m<16; m++)
      memset( ((uint64_t *)mb.QFS) + m, 0, sizeof(uint64_t) );
  }
    
    
  /* DC - component */
  {
    unsigned int dct_dc_size;
    int dct_diff;
    
    if(i < 4) {
      dct_dc_size = get_vlc(table_b12, "dct_dc_size_luminance (b12)");
      DPRINTF(4, "luma_size: %d\n", dct_dc_size);
    } else {
      dct_dc_size = get_vlc(table_b13, "dct_dc_size_chrominance (b13)");
      DPRINTF(4, "chroma_size: %d\n", dct_dc_size);
    } 
    
    if(dct_dc_size != 0) {
      int half_range = 1<<(dct_dc_size-1);
      int dct_dc_differential = GETBITS(dct_dc_size, "dct_dc_differential");
      
      DPRINTF(4, "diff_val: %d, ", dct_dc_differential);
      
      if(dct_dc_differential >= half_range) {
	dct_diff = dct_dc_differential;
      } else {
	dct_diff = (dct_dc_differential+1)-(2*half_range);
      }
      DPRINTF(4, "%d\n", dct_diff);  
      
    } else {
      dct_diff = 0;
    }
      
    {
      // qfs is always between 0 and 2^(8+dct_dc_size)-1, i.e unsigned.
      unsigned int qfs;
      int cc;
      
      /* Table 7-1. Definition of cc, colour component index */ 
      if(i < 4)
	cc = 0;
      else
	cc = (i%2) + 1;
      
      qfs = mb.dc_dct_pred[cc] + dct_diff;
      mb.dc_dct_pred[cc] = qfs;
      DPRINTF(4, "QFS[0]: %d\n", qfs);
      
      /* inverse quantisation */
      {
	// mb.intra_dc_mult is 1, 2 , 4 or 8, i.e unsigned.
	unsigned int f = mb.intra_dc_mult * qfs;
#if 0
	if(f > 2047) {
	  fprintf(stderr, "Clipp (block_intra first)\n");
	  f = 2047;
	} 
#endif
	mb.QFS[0] = f;
	inverse_quantisation_sum = f;
      }
      n = 1;
    }
  }
  
  
  /* AC - components */
  
  left = bits_left;
  offset = offs;
  word = nextbits(24);
  
  while( 1 ) {
    //get_dct_intra(&runlevel,"dct_dc_subsequent");
    
    const DCTtab *tab;
    /*    const unsigned int code = nextbits(16); */
    const unsigned int code = (word >> (24-16));
    
    if(code>=16384) {
      if (pic.coding_ext.intra_vlc_format)
	tab = &DCTtab0a[(code >> 8) - 4];   // 15
      else
	tab = &DCTtabnext[(code >> 12) - 4];  // 14
      /* EOB here for both cases */
    }
    else if(code>=1024) {
      if (pic.coding_ext.intra_vlc_format)
	tab = &DCTtab0a[(code >> 8) - 4];   // 15
      else
	tab = &DCTtab0[(code >> 8) - 4];   // 14
      /* Escape here for both cases */
    }
    else if(code>=512)
      if (pic.coding_ext.intra_vlc_format)
	tab = &DCTtab1a[(code >> 6) - 8];  // 15
      else
	tab = &DCTtab1[(code >> 6) - 8];  // 14
    else if(code>=256)
      tab = &DCTtab2[(code >> 4) - 16];
    else if(code>=128)
      tab = &DCTtab3[(code >> 3) - 16];
    else if(code>=64)
      tab = &DCTtab4[(code >> 2) - 16];
    else if(code>=32)
      tab = &DCTtab5[(code >> 1) - 16];
    else if(code>=16)
      tab = &DCTtab6[code - 16];
    else {
      fprintf(stderr,
	      "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	      code);
      exit_program(1);
    }
    
#ifdef DEBUG
    if(tab->run != 64 /*VLC_END_OF_BLOCK*/ ) {
      DPRINTF(4, "coeff run: %d, level: %d\n",
	      tab->run, tab->level);
    }
#endif
    
    if(tab->run == 64 /*VLC_END_OF_BLOCK*/) { // end_of_block 
      /*dropbits(tab->len); // pic.coding_ext.intra_vlc_format ? 4 : 2 bits */
      left -= tab->len;
      break;
    } 
    else {
      unsigned int i, f, run, val, sgn;
      
      if(tab->run == 65) { /* escape */
	//    dropbits(tab->len); // always 6 bits.
	//    run = GETBITS(6, "(get_dct escape - run )");
	//    val = GETBITS(12, "(get_dct escape - level )");
	/* val = GETBITS(6 + 6 + 12, "(get_dct escape - run & level )"); */
	val = (word >> (24-(6+12+6))); left -= (6+12+6);
	run = (val >> 12) & 0x3f;
	val = val & 0xfff;
	
	if ((val & 2047) == 0) {
	  fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
	  exit_program(1);
	}
	
	sgn = (val >= 2048);
	if(val >= 2048)                // !!!! ?sgn? 
	  val = 4096 - val;
      } else {
	//    dropbits(tab->len);
	run = tab->run;
	val = tab->level; 
	/* sgn = 0x1 & GETBITS(tab->len + 1, "(get_dct sign )"); //sign bit */
	sgn = 0x1 & (word >> (24-(tab->len + 1))); left -= (tab->len + 1);
      }
      
      n += run;
      
      /* inverse quantisation */
      i = inverse_scan[pic.coding_ext.alternate_scan][n];
      f = (val 
	   * mb.quantiser_scale
	   * seq.header.intra_inverse_quantiser_matrix[i])/16;
      
#if 0	      
      if(!sgn) {
	if(f > 2047)
	  f = 2047;
      }
      else {
	if(f > 2048)
	  f = 2048;
      }
#endif
      inverse_quantisation_sum += f; // The last bit is the same in f and -f.
      mb.QFS[i] = sgn ? -f : f;
      
      n++;
    }
    
    /* Works like:
       dropbits(..);
       word = nextbits(24);
    */
    if(left <= 24) {
      if(offset < buf_size) {
	uint32_t new_word = GUINT32_FROM_BE(buf[offset++]);
	cur_word = (cur_word << 32) | new_word;
	left += 32;
      }
      else {
	bits_left = left;
	offs = offset;
	read_buf();
	left = bits_left;
	offset = offs;
      }
    }
    word = (cur_word << (64-left)) >> 40;
    
  }
  inverse_quantisation_final(inverse_quantisation_sum);
  
  // Clean-up
  bits_left = left;
  offs = offset;
  dropbits(0);
    
  DPRINTF(4, "nr of coeffs: %d\n", n);
}



/* 6.2.6 Block */
static
void block_non_intra(unsigned int b)
{
  unsigned int n = 0;
  int inverse_quantisation_sum = 0;
  
  /* Make a local 'getbits' implementation. */
  unsigned int left = bits_left;
  unsigned int offset = offs;
  uint64_t lcur_word = cur_word;
  uint32_t word = (lcur_word << (64-left)) >> 40; //nextbits(24);
  
  DPRINTF(3, "pattern_code(%d) set\n", b);
  
  // Reset all coefficients to 0.
  {
    int m;
    for(m = 0; m < 16; m++)
      memset((uint64_t *)mb.QFS + m, 0, sizeof(uint64_t));
  }
  
  while(1) {
    //      get_dct_non_intra(&runlevel, "dct_dc_subsequent");
    const DCTtab *tab;
    /*    const unsigned int code = nextbits(16);*/
    const unsigned int code = (word >> (24-16));
    
    if(code >= 16384)
      if(n == 0)
	tab = &DCTtabfirst[(code >> 12) - 4];
      else {
	tab = &DCTtabnext[(code >> 12) - 4];
	/* EOB only here */
      }
    else if(code >= 1024) {
      tab = &DCTtab0[(code >> 8) - 4];
      /* Escape only here */
    }
    else if(code >= 512)
      tab = &DCTtab1[(code >> 6) - 8];
    else if(code >= 256)
      tab = &DCTtab2[(code >> 4) - 16];
    else if(code >= 128)
      tab = &DCTtab3[(code >> 3) - 16];
    else if(code >= 64)
      tab = &DCTtab4[(code >> 2) - 16];
    else if(code >= 32)
      tab = &DCTtab5[(code >> 1) - 16];
    else if(code >= 16)
      tab = &DCTtab6[code - 16];
    else {
      //      fprintf(stderr, "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n", code);
      exit_program(1);
    }
    
#ifdef DEBUG
    if(tab->run != 64 /*VLC_END_OF_BLOCK*/) {
      DPRINTF(4, "coeff run: %d, level: %d\n",
	      tab->run, tab->level);
    }
#endif
   
    if(tab->run == 64 /*VLC_END_OF_BLOCK*/) { // end_of_block 
      //      dropbits( 2 ); // tab->len, end of block always = 2bits
      left -= 2;
      break;
    } 
    else {
      unsigned int i, f, run, val, sgn;

      if(tab->run == 65) { /* escape */
	//	  dropbits(tab->len); escape always = 6 bits
	//	  run = GETBITS(6, "(get_dct escape - run )");
	//	  val = GETBITS(12, "(get_dct escape - level )");
	/*	val = GETBITS(6 + 12 + 6, "(escape run + val)" );*/
	val = (word >> (24-(6+12+6))); left -= (6+12+6);
	run = (val >> 12) & 0x3f;
	val &= 0xfff;
	
	if((val & 2047) == 0) {
	  //	  fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
	  exit_program(1);
	}
	
	sgn = (val >= 2048);
	if(val >= 2048)                // !!!! ?sgn? 
	  val =  4096 - val;// - 4096;
      }
      else {
	//	  dropbits(tab->len);
	run = tab->run;
	val = tab->level; 
	/*sgn = 0x1 & GETBITS(tab->len + 1, "(get_dct sign )"); //sign bit*/
	sgn = 0x1 & (word >> (24-(tab->len + 1))); left -= (tab->len + 1);
      }
      
      n += run;
      
      /* inverse quantisation */
      i = inverse_scan[pic.coding_ext.alternate_scan][n];
      // flytta ut &inverse_scan[pic.coding_ext.alternate_scan] ??
      // flytta ut mb.quantiser_scale ??
      f = ( ((val*2)+1)
	    * mb.quantiser_scale
	    * seq.header.non_intra_inverse_quantiser_matrix[i])/32;

#if 0
      if(!sgn) {
	if(f > 2047)
	  f = 2047;
      }
      else {
	if(f > 2048)
	  f = 2048;
      }
#endif
      
      inverse_quantisation_sum += f; // The last bit is the same in f and -f.
      mb.QFS[i] = sgn ? -f : f;
      
      n++;      
    }
    
    /* Works like:
       dropbits(..);
       word = nextbits(24);
    */
    if(left <= 24) {
      if(offset < buf_size) {
	uint32_t new_word = GUINT32_FROM_BE(buf[offset++]);
	lcur_word = (lcur_word << 32) | new_word;
	left += 32;
      }
      else {
	bits_left = left;
	offs = offset;
	cur_word = lcur_word;
	read_buf();
	left = bits_left;
	offset = offs;
	lcur_word = cur_word;
      }
    }
    word = (lcur_word << (64-left)) >> 40;
  }
  inverse_quantisation_final(inverse_quantisation_sum);
  
  // Clean-up
  bits_left = left;
  offs = offset;
  cur_word = lcur_word;
  dropbits( 0 ); // eob-token ++
  
  DPRINTF(4, "nr of coeffs: %d\n", n);
}



/* 6.2.5.3 Coded block pattern */
static
void coded_block_pattern(void)
{
  //  uint16_t coded_block_pattern_420;
  uint8_t cbp = 0;
  
  DPRINTF(3, "coded_block_pattern\n");

  cbp = get_vlc(table_b9, "cbp (b9)");
  

  if((cbp == 0) && (seq.ext.chroma_format == 0x1)) {
    fprintf(stderr, "** shall not be used with 4:2:0 chrominance\n");
    exit_program(-1);
  }
  mb.cbp = cbp;
  
  DPRINTF(4, "cpb = %u\n", mb.cbp);

  if(seq.ext.chroma_format == 0x02) {
    mb.coded_block_pattern_1 = GETBITS(2, "coded_block_pattern_1");
  }
 
  if(seq.ext.chroma_format == 0x03) {
    mb.coded_block_pattern_2 = GETBITS(6, "coded_block_pattern_2");
  }
  
}


 
/* 6.2.5.2.1 Motion vector */
static
void motion_vector(int r, int s)
{
  int t;
  
  DPRINTF(2, "motion_vector(%d, %d)\n", r, s);

  for(t = 0; t < 2; t++) {   
    int delta;
    int prediction;
    int vector;
    
    int r_size = pic.coding_ext.f_code[s][t] - 1;
    
    { // Read and compute the motion vector delta
      int motion_code = get_vlc(table_b10, "motion_code[r][s][0] (b10)");
      
      if((pic.coding_ext.f_code[s][t] != 1) && (motion_code != 0)) {
	int motion_residual = GETBITS(r_size, "motion_residual[r][s][0]");
	
	delta = ((abs(motion_code) - 1) << r_size) + motion_residual + 1;
	if(motion_code < 0)
	  delta = - delta;
      }
      else {
	delta = motion_code;
      }      
    }
    
    if(mb.dmv == 1)
      mb.dmvector[t] = get_vlc(table_b11, "dmvector[0] (b11)");
  
    // Get the predictor
    prediction = pic.PMV[r][s][t];
    if((t==1) && (mb.mv_format == MV_FORMAT_FIELD) &&
       (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE))
      prediction = prediction >> 1; /* DIV */
    
    { // Compute the resulting motion vector
      int f = 1 << r_size;
      int high = (16 * f) - 1;
      int low = ((-16) * f);
      int range = (32 * f);
      
      vector = prediction + delta;
      if(vector < low)
	vector = vector + range;
      if(vector > high)
	vector = vector - range;
    }
    
    // Update predictors
    if((t==1) && (mb.mv_format == MV_FORMAT_FIELD) &&
       (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE))
      pic.PMV[r][s][t] = vector * 2;
    else
      pic.PMV[r][s][t] = vector;
    
#if MPEG1
    // Scale the vector so that it is always measured in half pels
    if(pic.header.full_pel_vector[s])
      mb.vector[r][s][t] = vector << 1;
    else
#endif
      mb.vector[r][s][t] = vector;
  }
}


/* 6.2.5.2 Motion vectors */
static
void motion_vectors(unsigned int s)
{
  DPRINTF(3, "motion_vectors(%u)\n", s);

  if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
    if(pic.coding_ext.frame_pred_frame_dct == 0) {
      
      /* Table 6-17 Meaning of frame_motion_type */
      switch(mb.modes.frame_motion_type) {
      case 0x0:
	fprintf(stderr, "*** reserved prediction type\n");
	exit_program(-1);
	break;
      case 0x1:
	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	mb.motion_vector_count = 2;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 0;
	break;
      case 0x2:
	/* spatial_temporal_weight_class always 0 for now */
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.dmv = 0;
	break;
      case 0x3:
	mb.prediction_type = PRED_TYPE_DUAL_PRIME;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 1;
	break;
      default:
	fprintf(stderr, "*** impossible prediction type\n");
	exit_program(-1);
	break;
      }
    }
    if(mb.modes.macroblock_intra &&
       pic.coding_ext.concealment_motion_vectors) {
      mb.prediction_type = PRED_TYPE_FRAME_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FRAME;
      mb.dmv = 0;
    }
      
    if(pic.coding_ext.frame_pred_frame_dct == 1) {
      mb.prediction_type = PRED_TYPE_FRAME_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FRAME;
      mb.dmv = 0;
    }
    
  } else {
    /* Table 6-18 Meaning of field_motion_type */
    switch(mb.modes.field_motion_type) {
    case 0x0:
      fprintf(stderr, "*** reserved field prediction type\n");
      exit_program(-1);
      break;
    case 0x1:
      mb.prediction_type = PRED_TYPE_FIELD_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 0;
      break;
    case 0x2:
      mb.prediction_type = PRED_TYPE_16x8_MC;
      mb.motion_vector_count = 2;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 0;
      break;
    case 0x3:
      mb.prediction_type = PRED_TYPE_DUAL_PRIME;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 1;
      break;
    default:
      fprintf(stderr, "*** impossible prediction type\n");
      exit_program(-1);
      break;
    }
  }


  if(mb.motion_vector_count == 1) {
    if((mb.mv_format == MV_FORMAT_FIELD) && (mb.dmv != 1)) {
      mb.motion_vertical_field_select[0][s] 
	= GETBITS(1, "motion_vertical_field_select[0][s]");
    }
    motion_vector(0, s);
  } else {
    mb.motion_vertical_field_select[0][s] 
      = GETBITS(1, "motion_vertical_field_select[0][s]");
    motion_vector(0, s);
    mb.motion_vertical_field_select[1][s] 
      = GETBITS(1, "motion_vertical_field_select[1][s]");
    motion_vector(1, s);
  }
}




/* 6.2.5.1 Macroblock modes */
static
void macroblock_modes(void)
{
  DPRINTF(3, "macroblock_modes\n");

  if(pic.header.picture_coding_type == PIC_CODING_TYPE_I) {
    /* I-picture */
    mb.modes.macroblock_type = get_vlc(table_b2, "macroblock_type (b2)");

  } else if(pic.header.picture_coding_type == PIC_CODING_TYPE_P) {
    /* P-picture */
    mb.modes.macroblock_type = get_vlc(table_b3, "macroblock_type (b3)");

  } else if(pic.header.picture_coding_type == PIC_CODING_TYPE_B) {
    /* B-picture */
    mb.modes.macroblock_type = get_vlc(table_b4, "macroblock_type (b4)");
    
  } else {
    fprintf(stderr, "*** Unsupported picture type %02x\n", 
	    pic.header.picture_coding_type);
    exit_program(-1);
  }
  
  mb.modes.macroblock_quant = mb.modes.macroblock_type & MACROBLOCK_QUANT;
  mb.modes.macroblock_motion_forward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD;
  mb.modes.macroblock_motion_backward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD;
  mb.modes.macroblock_pattern = mb.modes.macroblock_type & MACROBLOCK_PATTERN;
  mb.modes.macroblock_intra = mb.modes.macroblock_type & MACROBLOCK_INTRA;
  mb.modes.spatial_temporal_weight_code_flag =
    mb.modes.macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG;
  
  DPRINTF(5, "spatial_temporal_weight_code_flag: %01x\n", 
	  mb.modes.spatial_temporal_weight_code_flag);

  if((mb.modes.spatial_temporal_weight_code_flag == 1) &&
     (1 /*spatial_temporal_weight_code_table_index != 0*/)) {
    mb.modes.spatial_temporal_weight_code 
      = GETBITS(2, "spatial_temporal_weight_code");
  }

  if(mb.modes.macroblock_motion_forward ||
     mb.modes.macroblock_motion_backward) {
    if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
      if(pic.coding_ext.frame_pred_frame_dct == 0) {
	mb.modes.frame_motion_type = GETBITS(2, "frame_motion_type");
      } else {
	/* frame_motion_type omitted from the bitstream */
	mb.modes.frame_motion_type = 0x2;
      }
    } else {
      mb.modes.field_motion_type = GETBITS(2, "field_motion_type");
    }
  }

  /* if(decode_dct_type) */
  if((pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) &&
     (pic.coding_ext.frame_pred_frame_dct == 0) &&
     (mb.modes.macroblock_intra || mb.modes.macroblock_pattern)) {
    mb.modes.dct_type = GETBITS(1, "dct_type");
  } else {
    /* Table 6-19. Value of dct_type if dct_type is not in the bitstream.
       pic.coding_ext.frame_pred_frame_dct == 1 then mb.modes.dct_type = 0
       else dct_type is unused, either field picture or mb not coded */
    mb.modes.dct_type = 0;
  }
}



/* 6.2.5 Macroblock */
static
void macroblock(void)
{
  unsigned int block_count = 0;
  uint16_t inc_add = 0;
  
  DPRINTF(3, "macroblock()\n");

  while(nextbits(11) == 0x008) {
    GETBITS(11, "macroblock_escape");
    inc_add += 33;
  }

  mb.macroblock_address_increment 
    = get_vlc(table_b1, "macroblock_address_increment");

  mb.macroblock_address_increment += inc_add;
  
  seq.macroblock_address 
    = mb.macroblock_address_increment + seq.previous_macroblock_address;
  
  seq.previous_macroblock_address = seq.macroblock_address;


  /* In MPEG-2 a slice never span two or more rows. */
  seq.mb_column = seq.macroblock_address % seq.mb_width;
  
  DPRINTF(2, " Macroblock: %d, row: %d, col: %d\n",
	  seq.macroblock_address,
	  seq.mb_row,
	  seq.mb_column);
  
#ifdef DEBUG
  if(mb.macroblock_address_increment > 1) {
    DPRINTF(3, "Skipped %d macroblocks\n",
	    mb.macroblock_address_increment);
  }
#endif
  
  
  if(pic.header.picture_coding_type == PIC_CODING_TYPE_P) {
    /* In a P-picture when a macroblock is skipped */
    if(mb.macroblock_address_increment > 1) {
      reset_PMV();
      reset_vectors();
    }
  }
  
  
  
  if(mb.macroblock_address_increment > 1) {
    
    /* Skipped blocks never have any DCT coefficients (pattern). */
    
    switch(pic.header.picture_coding_type) {
    
    case PIC_CODING_TYPE_P:
      DPRINTF(3,"skipped in P-picture\n");
      /* mlib is broken!!! 
      {
	int x = (seq.mb_column-mb.macroblock_address_increment+1);
	int y = seq.mb_row;
	int offs_y = x * 16 + y * 16 * seq.mb_width * 16;//seq.horizontal_size;
	int offs_uv = x * 8 + y *  8 * (seq.mb_width * 16)/2;
	mlib_VideoCopyRef_U8_U8(&dst_image->y[offs_y],
				&fwd_ref_image->y[offs_y], 
				(mb.macroblock_address_increment-1)*16, 
				16, (seq.mb_width * 16));
	mlib_VideoCopyRef_U8_U8(&dst_image->u[offs_uv],
				&fwd_ref_image->u[offs_uv],
				(mb.macroblock_address_increment-1)*8,
				8, (seq.mb_width * 16)/2);
	mlib_VideoCopyRef_U8_U8(&dst_image->v[offs_uv],
				&fwd_ref_image->v[offs_uv],
				(mb.macroblock_address_increment-1)*8,
				8, (seq.mb_width * 16)/2);
      }
      */
      {
	int x,y;
	
	for(x = (seq.mb_column-mb.macroblock_address_increment+1)*16, 
	      y = seq.mb_row*16;
	    y < (seq.mb_row+1)*16; y++) {
	  memcpy(&dst_image->y[y*(seq.mb_width*16)+x], 
		 &fwd_ref_image->y[y*(seq.mb_width*16)+x], 
		 (mb.macroblock_address_increment-1)*16);
	}
	for(x = (seq.mb_column-mb.macroblock_address_increment+1)*8, 
	      y = seq.mb_row*8;
	    y < (seq.mb_row+1)*8; y++) {
	  memcpy(&dst_image->u[y*(seq.mb_width*16)/2+x], 
		 &fwd_ref_image->u[y*(seq.mb_width*16)/2+x], 
		 (mb.macroblock_address_increment-1)*8);
	}
	for(x = (seq.mb_column-mb.macroblock_address_increment+1)*8, 
	      y = seq.mb_row*8;
	    y < (seq.mb_row+1)*8; y++) {
	  memcpy(&dst_image->v[y*(seq.mb_width*16)/2+x], 
		 &fwd_ref_image->v[y*(seq.mb_width*16)/2+x], 
		 (mb.macroblock_address_increment-1)*8);
	}
      }
      break;
      
    case PIC_CODING_TYPE_B:
      DPRINTF(3,"skipped in B-frame\n");
      {
	int old_col = seq.mb_column;
	
	if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	  /* 7.6.6.4  B frame picture */
	  mb.prediction_type = PRED_TYPE_FRAME_BASED;
	  mb.motion_vector_count = 1;
	  mb.mv_format = MV_FORMAT_FRAME;
	  mb.dmv = 0;
	}
	else {
	  fprintf(stderr, "***ni Skipped in B-picture (no-frame)\n");
	  //exit_program(1);
	}
	
	/* Set mb_column so that motion_comp will use the right adress */
	for(seq.mb_column = seq.mb_column-mb.macroblock_address_increment+1;
	    seq.mb_column < old_col; seq.mb_column++) {
	  motion_comp();
	}
	seq.mb_column = old_col;
      }
      break;
    
    default:
      fprintf(stderr, "*** skipped blocks in I-picture\n");
      break;
    }
  }


  macroblock_modes();
  
  
  if(mb.modes.macroblock_intra == 0) {
    reset_dc_dct_pred();  
    DPRINTF(3, "non_intra macroblock\n");
  }

  if(mb.macroblock_address_increment > 1) {
    reset_dc_dct_pred();
    DPRINTF(3, "skipped block\n");
  }
    
   
  if(mb.modes.macroblock_quant) {
    mb.quantiser_scale = 
      q_scale[pic.coding_ext.q_scale_type][GETBITS(5, "quantiser_scale_code")];
  }
  
  if(mb.modes.macroblock_motion_forward ||
     (mb.modes.macroblock_intra &&
      pic.coding_ext.concealment_motion_vectors)) {
    motion_vectors(0);
  }
  if(mb.modes.macroblock_motion_backward) {
    motion_vectors(1);
  }
  if(mb.modes.macroblock_intra && pic.coding_ext.concealment_motion_vectors) {
    marker_bit();
  }

  
  /* All motion vectors for the block has been decoded. Update predictors */
  
  if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
    switch(mb.prediction_type) {
    case PRED_TYPE_FRAME_BASED:
      if(mb.modes.macroblock_intra) {
	if(pic.coding_ext.concealment_motion_vectors == 0) {
	  reset_PMV();
	  DPRINTF(4, "* 1\n");
	} else {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
      } else {
	if(mb.modes.macroblock_motion_forward) {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
	if(mb.modes.macroblock_motion_backward) {
	  pic.PMV[1][1][1] = pic.PMV[0][1][1];
	  pic.PMV[1][1][0] = pic.PMV[0][1][0];
	}
	if(pic.coding_ext.frame_pred_frame_dct != 0) {
	  if((mb.modes.macroblock_motion_forward == 0) &&
	     (mb.modes.macroblock_motion_backward == 0)) {
	    reset_PMV();
	    DPRINTF(4, "* 2\n");
	  }
	}
      }
      break;
    case PRED_TYPE_FIELD_BASED:
      break;
    case PRED_TYPE_DUAL_PRIME:
      if(mb.modes.macroblock_motion_forward) {
	pic.PMV[1][0][1] = pic.PMV[0][0][1];
	pic.PMV[1][0][0] = pic.PMV[0][0][0];
      }      
      break;
    default:
      fprintf(stderr, "*** invalid pred_type\n");
      exit_program(-1);
      break;
    }	
  } else { /* Fieldbased picture structure */
    switch(mb.prediction_type) {
    case PRED_TYPE_FIELD_BASED:
      if(mb.modes.macroblock_intra) {
	if(pic.coding_ext.concealment_motion_vectors == 0) {
	  DPRINTF(4, "* 3\n");
	  reset_PMV();
	} else {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
      } else {
	if(mb.modes.macroblock_motion_forward) {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
	if(mb.modes.macroblock_motion_backward) {
	  pic.PMV[1][1][1] = pic.PMV[0][1][1];
	  pic.PMV[1][1][0] = pic.PMV[0][1][0];
	}
	if(pic.coding_ext.frame_pred_frame_dct != 0) {
	  if((mb.modes.macroblock_motion_forward == 0) &&
	     (mb.modes.macroblock_motion_backward == 0)) {
	    reset_PMV();
	    DPRINTF(4, "* 4\n");
	  }
	}
      }
      break;
    case PRED_TYPE_16x8_MC:
      break;
    case PRED_TYPE_DUAL_PRIME:
      if(mb.modes.macroblock_motion_forward) {
	pic.PMV[1][0][1] = pic.PMV[0][0][1];
	pic.PMV[1][0][0] = pic.PMV[0][0][0];
      }      
      break;
    default:
      fprintf(stderr, "*** invalid pred_type\n");
      exit_program(-1);
      break;
    }
  }
  
  /* Whenever an intra macroblock is decoded which has
     no concealment motion vectors */
  if(mb.modes.macroblock_intra) {
    if(pic.coding_ext.concealment_motion_vectors == 0) {
      DPRINTF(4, "* 5\n");
      reset_PMV();
    }
  }
  

  if(pic.header.picture_coding_type == PIC_CODING_TYPE_P) {
    /* In a P-picture when a non-intra macroblock is decoded
       in which macroblock_motion_forward is zero */
    if(mb.modes.macroblock_intra == 0 && 
       mb.modes.macroblock_motion_forward == 0) {
      reset_PMV();
      DPRINTF(4, "* 6\n");
    }
  }
    

  /*** 7.6.3.5 Prediction in P-pictures ***/

  if(pic.header.picture_coding_type == PIC_CODING_TYPE_P) {
    /* P-picture */
    if((!mb.modes.macroblock_motion_forward) && (!mb.modes.macroblock_intra)) {
      DPRINTF(2, "prediction mode Frame-base, \nresetting motion vector predictor and motion vector\n");
      DPRINTF(2, "motion_type: %02x\n", mb.modes.frame_motion_type);
      if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.motion_vector_count = 1;
	reset_PMV();
      } else {

	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	mb.mv_format = MV_FORMAT_FIELD;
      }
      mb.modes.macroblock_motion_forward = 1;
      mb.vector[0][0][0] = 0;
      mb.vector[0][0][1] = 0;
      mb.vector[1][0][0] = 0;
      mb.vector[1][0][1] = 0;
       
    }

    /* never happens, see code further up
      if(mb.macroblock_address_increment > 1) {
      
      fprintf(stderr, "prediction mode shall be Frame-based\n");
      fprintf(stderr, "motion vector predictors shall be reset to zero\n");
      fprintf(stderr, "motion vector shall be zero\n");
      
      // *** TODO
      //mb.vector[0][0][0] = 0;
      //mb.vector[0][0][1] = 0;
      }
    */
    
  }
   

  /* Table 6-20 block_count as a function of chroma_format */
  if(seq.ext.chroma_format == 0x01) {
    block_count = 6;
  } else if(seq.ext.chroma_format == 0x02) {
    block_count = 8;
  } else if(seq.ext.chroma_format == 0x03) {
    block_count = 12;
  }
  
  
  if(mb.modes.macroblock_intra) {
    unsigned int i;
    
    /* Intra blocks always have pattern coded for all sub blocks and
       are writen directly to the output buffers by this code. */
    
    for(i = 0; i < block_count; i++) {  
      DPRINTF(4, "cbpindex: %d assumed\n", i);
      block_intra(i);
      
      /* Shortcut: write the IDCT data directly into the picture buffer */
      {
	const int x = seq.mb_column;
	const int y = seq.mb_row;
	const int width = seq.mb_width * 16; //seq.horizontal_size;
	int d, stride;
	uint8_t *dst;
	
	if (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) { 
	  if (mb.modes.dct_type) {
	    d = 1;
	    stride = width * 2;
	  } else {
	    d = 8;
	    stride = width;
	  }
	  if(i < 4) {
	    dst = &dst_image->y[x * 16 + y * 16 * width];
	    dst = (i & 1) ? dst + 8 : dst;
	    dst = (i >= 2) ? dst + width * d : dst;
	  }
	  else {
	    stride = width / 2;
	    if( i == 4 )
	      dst = &dst_image->u[x * 8 + y * 8 * stride];
	    else // i == 5
	      dst = &dst_image->v[x * 8 + y * 8 * stride];
	  }
	} else {
	    if(i < 4) {
	      stride = width * 2;
	      dst = &dst_image->y[x * 16 + y * 16 * stride];
	      dst = (i & 1) ? dst + 8 : dst;
	      dst = (i >= 2) ? dst + stride * 8 : dst;
	    }
	    else {
	      stride = width;
	      if( i == 4 )
		dst = &dst_image->u[x * 8 + y * 8 * stride];
	      else // i == 5
		dst = &dst_image->v[x * 8 + y * 8 * stride];
	    }
	    if (pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD)
	      dst += stride/2;
	}

	mlib_VideoIDCT8x8_U8_S16(dst, (int16_t *)mb.QFS, stride);
      }
    }
  } 
  else { /* Non-intra block */

    motion_comp(); // Only motion compensate don't add coefficients.

    if(mb.modes.macroblock_pattern) {
      unsigned int i;
      
      coded_block_pattern();
      
      for(i = 0; i < 6; i++) {
	if(mb.cbp & (1<<(5-i))) {
	  DPRINTF(4, "cbpindex: %d set\n", i);
	  block_non_intra(i);
	  mlib_VideoIDCT8x8_S16_S16((int16_t *)mb.QFS, (int16_t *)mb.QFS);
	  motion_comp_add_coeff(i);
	}
      }
      if(seq.ext.chroma_format == 0x02) {
	for(i = 6; i < 8; i++) {
	  if(mb.coded_block_pattern_1 & (1<<(7-i))) {
	    block_non_intra(i);
	    fprintf(stderr, "ni seq.ext.chroma_format == 0x02\n");
	    //exit_program(-1);
	  }
	}
      }
      if(seq.ext.chroma_format == 0x03) {
	for(i = 6; i < 12; i++) {
	  if(mb.coded_block_pattern_2 & (1<<(11-i))) {
	    block_non_intra(i);
	    fprintf(stderr, "ni seq.ext.chroma_format == 0x03\n");
	    //exit_program(-1);
	  }
	}
      }
    }
  }
  
}


/* 6.2.4 Slice */
void mpeg2_slice(void)
{
  uint32_t slice_start_code;
  
  DPRINTF(3, "slice\n");

  reset_dc_dct_pred();
  reset_PMV();

  DPRINTF(3, "start of slice\n");
  
  slice_start_code = GETBITS(32, "slice_start_code");
  slice_data.slice_vertical_position = slice_start_code & 0xff;
  
  if(seq.vertical_size > 2800) {
    slice_data.slice_vertical_position_extension 
      = GETBITS(3, "slice_vertical_position_extension");
    seq.mb_row = (slice_data.slice_vertical_position_extension << 7) +
      slice_data.slice_vertical_position - 1;
  } else {
      seq.mb_row = slice_data.slice_vertical_position - 1;
  }

  seq.previous_macroblock_address = (seq.mb_row * seq.mb_width) - 1;

  //TODO
  if(0) {//sequence_scalable_extension_present) {
    if(0) { //scalable_mode == DATA_PARTITIONING) {
      slice_data.priority_breakpoint = GETBITS(7, "priority_breakpoint");
    }
  }
  
  mb.quantiser_scale
    = q_scale[pic.coding_ext.q_scale_type][GETBITS(5, "quantiser_scale_code")];

  if(nextbits(1) == 1) {
    slice_data.intra_slice_flag = GETBITS(1, "intra_slice_flag");
    slice_data.intra_slice = GETBITS(1, "intra_slice");
    slice_data.reserved_bits = GETBITS(7, "reserved_bits");
    while(nextbits(1) == 1) {
      slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
      slice_data.extra_information_slice = GETBITS(8, "extra_information_slice");
    }
  }
  slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
  
  do {
    macroblock();
  } while(nextbits(23) != 0);
  
}
