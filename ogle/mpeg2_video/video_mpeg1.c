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










extern int show_stat;
extern yuv_image_t *dst_image;

extern macroblock_t *cur_mbs;
//extern macroblock_t *ref1_mbs;
extern macroblock_t *ref2_mbs;

extern void next_start_code(void);
extern void exit_program(int exitcode) __attribute__ ((noreturn));
extern void motion_comp();
extern void motion_comp_add_coeff(unsigned int i);
extern int get_vlc(const vlc_table_t *table, char *func);

#ifdef STATS
extern uint32_t stats_quantiser_scale_possible;
extern uint32_t stats_quantiser_scale_nr;

extern uint32_t stats_intra_quantiser_scale_possible;
extern uint32_t stats_intra_quantiser_scale_nr;

extern uint32_t stats_non_intra_quantiser_scale_possible;
extern uint32_t stats_non_intra_quantiser_scale_nr;

extern uint32_t stats_block_non_intra_nr;
extern uint32_t stats_f_non_intra_compute_first_nr;
extern uint32_t stats_f_non_intra_compute_subseq_nr;

extern uint32_t stats_block_intra_nr;
extern uint32_t stats_f_intra_compute_subseq_nr;

extern uint32_t stats_f_non_intra_subseq_escaped_run_nr;
extern uint32_t stats_f_non_intra_first_escaped_run_nr;

extern uint8_t new_scaled;
#endif


/*
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
  
  mb.dc_dct_pred[0] = 128;
  mb.dc_dct_pred[1] = 128;
  mb.dc_dct_pred[2] = 128;
}

static
void reset_PMV()
{
  DPRINTF(3, "Resetting PMV\n");
  
  pic.PMV[0][0][0] = 0;
  pic.PMV[0][0][1] = 0;
  pic.PMV[0][1][0] = 0;
  pic.PMV[0][1][1] = 0;
}

static
void reset_vectors()
{
  DPRINTF(3, "Resetting motion vectors\n");
  
  mb.vector[0][0][0] = 0;
  mb.vector[0][0][1] = 0;
  mb.vector[0][1][0] = 0;
  mb.vector[0][1][1] = 0;
}









#if 0
static inline
void get_dct_intra(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if(code>=16384)
    tab = &DCTtabnext[(code>>12)-4];  // 14 
  else if(code>=1024)
    tab = &DCTtab0[(code>>8)-4];      // 14
  else if(code>=512)
    tab = &DCTtab1[(code>>6)-8];      // 14
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit_program(1);
  }
  
  //  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    dropbits(2); // Always 2 bits.
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    //    dropbits(tab->len); // Always 6 bits.
    //    run = GETBITS(6, "(get_dct escape - run )");
    //    val = GETBITS(8, "(get_dct escape - level )");
    uint32_t tmp = GETBITS(6+6+8, "(get_dct escape - run & level)");
    val = (int8_t)(tmp & 0xff);
    run = (tmp >> 8) & 0x3f;
    if ((tmp&0x7f) == 0) {
      val = (val << 1) | GETBITS(8, "(get_dct escape - extended level)");
      if(abs(val) < 128) {
	fprintf(stderr, "invalid extended dct escape MPEG-1\n");
      }
    }
  } 
  else {
    run = tab->run;
    val = tab->level; 
    if(0x1 & GETBITS(tab->len+1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;
}


static inline
void get_dct_non_intra_first(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if (code>=16384)
    tab = &DCTtabfirst[(code>>12)-4]; // 14 
  else if(code>=1024)
    tab = &DCTtab0[(code>>8)-4];   // 14
  else if(code>=512)
    tab = &DCTtab1[(code>>6)-8];  // 14
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit_program(1);
  }
  
  //    dropbits(tab->len);
  
  if (tab->run==65) { /* escape */
    //    dropbits(tab->len); // 6
    //    run = GETBITS(6, "(get_dct escape - run )");
    //    val = GETBITS(8, "(get_dct escape - level )");
    uint32_t tmp = GETBITS(6+6+8, "(get_dct escape - run & level)");
#ifdef STATS
    stats_f_non_intra_first_escaped_run_nr++;
#endif
    val = (int8_t)(tmp & 0xff);
    run = (tmp >> 8) & 0x3f;
    if ((tmp&0x7f) == 0) {
      val = (val << 1) | GETBITS(8, "(get_dct escape - extended level)");
      if(abs(val) < 128) {
	fprintf(stderr, "invalid extended dct escape MPEG-1\n");
      }
    }      
  }
  else {
    //    dropbits(tab->len);
    run = tab->run;
    val = tab->level; 
    if( 0x1 & GETBITS(tab->len+1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;
}

static
void get_dct_non_intra_subseq(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if (code>=16384)
    tab = &DCTtabnext[(code>>12)-4];  // 14 
  else if(code>=1024)
    tab = &DCTtab0[(code>>8)-4];   // 14
  else if(code>=512)
    tab = &DCTtab1[(code>>6)-8];  // 14
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit_program(1);
  }
  
  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    run = GETBITS(6, "(get_dct escape - run )");
    val = (int8_t)GETBITS(8, "(get_dct escape - level)");
    
    if ((val&0x7f) == 0) {
      val = (val < 1) | GETBITS(8, "(get_dct escape - extended level)");
      if(val < 128) {
	fprintf(stderr, "invalid extended dct escape MPEG-1\n");
      }
    }
  }
  else {
    run = tab->run;
    val = tab->level; 
    if(GETBITS(1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;
}
#endif









/* 6.2.6 Block */
static
void block_intra(unsigned int i)
{
  unsigned int dct_dc_size;
  int dct_diff;
  
  int n;
  
  DPRINTF(3, "pattern_code(%d) set\n", i);
  
  { // Reset all coefficients to 0.
    int m;
    for(m=0; m<16; m++)
      memset( ((uint64_t *)mb.QFS) + m, 0, sizeof(uint64_t) );
  }
  
  /* DC - component */
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
      // mb.intra_dc_mult is 8 in MPEG-1, i.e unsigned.
      unsigned int f = 8 * qfs;
#if 0
      if(f > 2047) {
	fprintf(stderr, "Clipp (block_intra first)\n");
	f = 2047;
      }
#endif
      mb.QFS[0] = f;
    }
    n = 1;
  }
  
  
  /* AC - components */
  
  while( 1 ) {
    /* Manually inlined and optimized get_dct(..) */
    unsigned int code;
    const DCTtab *tab;
      
    code = nextbits(16);
    if(code >= 16384)
      tab = &DCTtabnext[(code >> 12) - 4];
    else if(code >= 1024)
      tab = &DCTtab0[(code >> 8) - 4];
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
      fprintf(stderr,
	      "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	      code);
      exit_program(1);
    }

#ifdef DEBUG
    if(tab->run != 64 /*VLC_END_OF_BLOCK*/) {
      DPRINTF(4, "coeff run: %d, level: %d\n",
	      runlevel.run, runlevel.level);
    }
#endif

    //  dropbits(tab->len);
    
    if (tab->run == 64 /*VLC_END_OF_BLOCK*/) { // end_of_block 
      //run = VLC_END_OF_BLOCK;
      //val = VLC_END_OF_BLOCK;
      dropbits(2); // Always 2 bits.
      break;
    } 
    else {
      unsigned int run, val, sgn, i, f;
	  
      if(tab->run == 65) { /* escape */
	// dropbits(tab->len); // tab->len, escape always = 6 bits	
	// run = GETBITS(6, "(get_dct escape - run )");
	// val = GETBITS(8, "(get_dct escape - level )");
	uint32_t tmp = GETBITS(6+6+8, "(get_dct escape - run & level)" );
	val = abs((int8_t)(tmp & 0xff));
	run = (tmp >> 8) & 0x3f;
	sgn = tmp & 0x80;
	
	if((tmp & 0x7f) == 0) { // ???
	  val = GETBITS(8, "(get_dct escape - extended level)");
	  val = sgn ? (0x100 - val) : val;
	  if(val < 128) {
	    fprintf(stderr, "invalid extended dct escape MPEG-1\n");
	  }
	}
      }
      else {
	// dropbits(tab->len); 
	run = tab->run;
	val = tab->level; 
	sgn = 0x1 & GETBITS(tab->len+1, "(get_dct sign )"); //sign bit
	// val = sgn ? -val : val;
      }
    
      n += run;
    
      /* inverse quantisation */
      i = inverse_scan[0][n];
      f = (2 * val
	   * mb.quantiser_scale
	   * seq.header.intra_inverse_quantiser_matrix[i])/16;
      
      f = f | 0x1; /* Oddification */
	
//#if 0
      if(f > 2047) {
	fprintf(stderr, "Clipp (block_intra subseq. +)\n");
	if(sgn)
	  f = 2048;
	else
	  f = 2047;
      }
//#endif
      
      mb.QFS[i] = sgn ? -f : f;
      n++;
    }
  }

  DPRINTF(4, "nr of coeffs: %d\n", n);
}





/* 6.2.6 Block */
static
void block_non_intra(unsigned int b)
{
  unsigned int n = 0;
    
  DPRINTF(3, "pattern_code(%d) set\n", b);
  
  /* Reset all coefficients to 0 */
  {
    int m;
    for(m=0; m<16; m+=4)
      memset( ((uint64_t *)mb.QFS) + m, 0, 4*sizeof(uint64_t) );
  }
  
  /* 7.2.2.4 Summary */
  while(1) {
    
    /* Manually inlined and optimized get_dct(..) */
    //      get_dct_non_intra(&runlevel, "dct_dc_subsequent");
    unsigned int code;
    const DCTtab *tab;
    
    code = nextbits(16);
    
    if (code>=16384)
      if(n == 0)
	tab = &DCTtabfirst[(code>>12)-4];
      else
	tab = &DCTtabnext[(code>>12)-4];
    else if(code>=1024)
      tab = &DCTtab0[(code>>8)-4];
    else if(code>=512)
      tab = &DCTtab1[(code>>6)-8];
    else if(code>=256)
      tab = &DCTtab2[(code>>4)-16];
    else if(code>=128)
      tab = &DCTtab3[(code>>3)-16];
    else if(code>=64)
      tab = &DCTtab4[(code>>2)-16];
    else if(code>=32)
      tab = &DCTtab5[(code>>1)-16];
    else if(code>=16)
      tab = &DCTtab6[code-16];
    else {
      fprintf(stderr,
	      "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	      code);
      exit_program(1);
    }
    
#ifdef DEBUG
    if(tab->run != 64 /*VLC_END_OF_BLOCK*/) {
      DPRINTF(4, "coeff run: %d, level: %d\n",
	      tab->run, tab->level);
    }
#endif
   
    if (tab->run == 64 /*VLC_END_OF_BLOCK*/) { // end_of_block 
      // run = VLC_END_OF_BLOCK;
      // val = VLC_END_OF_BLOCK;
      dropbits( 2 ); // tab->len, end of block always = 2bits
      break;
    } 
    else {
      unsigned int run, val, sgn, i, f;
      
      if (tab->run == 65) { /* escape */
	// dropbits(tab->len); // tab->len, escape always = 6 bits	
	// run = GETBITS(6, "(get_dct escape - run )");
	// val = GETBITS(8, "(get_dct escape - level )");
	uint32_t tmp = GETBITS(6+6+8, "(get_dct escape - run & level)" );
	val = abs((int8_t)(tmp & 0xff));
	run = (tmp >> 8) & 0x3f;
	sgn = tmp & 0x80;

#ifdef STATS
	stats_f_non_intra_subseq_escaped_run_nr++;
#endif
	
	if ((tmp&0x7f) == 0) { // ???
	  val = GETBITS(8, "(get_dct escape - extended level)");
	  val = sgn ? (0x100 - val) : val;
	  if(val < 128) {
	    fprintf(stderr, "invalid extended dct escape MPEG-1\n");
	  }
	}
      }
      else {
	// dropbits(tab->len); 
	run = tab->run;
	val = tab->level; 
	sgn = 0x1 & GETBITS(tab->len+1, "(get_dct sign )"); //sign bit
	// val = sgn ? -val : val;
      }
      
      n += run;
      
      /* inverse scan & quantisation */
      i = inverse_scan[0][n];
      // flytta ut &inverse_scan[pic.coding_ext.alternate_scan] ??
      
      f = ( ((val*2)+1)
	    * mb.quantiser_scale
	    * seq.header.non_intra_inverse_quantiser_matrix[i])/16;

      f = f | 0x1; /* Oddification */
      
//#if 0
      if(f > 2047) {
	fprintf(stderr, "Clipp (block_non_intra subseq. +)\n");
	if(sgn)
	  f = 2048;
	else
	  f = 2047;
      } 
//#endif
      
      mb.QFS[i] = sgn ? -f : f;
      n++;      
    }
  }
  
  DPRINTF(4, "nr of coeffs: %d\n", n);
}




/* 6.2.5.3 Coded block pattern */
static
void coded_block_pattern(void)
{
  uint8_t cbp = 0;
  
  DPRINTF(3, "coded_block_pattern\n");

  cbp = get_vlc(table_b9, "cbp (b9)");
  
  if(cbp == 0) {
    fprintf(stderr, "** shall not be used with 4:2:0 chrominance\n");
    exit_program(-1);
  }
  mb.cbp = cbp;
  
  DPRINTF(4, "cpb = %u\n", mb.cbp);
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
    if((mb.mv_format == MV_FORMAT_FIELD) && (t==1) &&
       (pic.coding_ext.picture_structure ==  0x3))
      prediction = (pic.PMV[r][s][t]) >> 1;         /* DIV */
    else
      prediction = pic.PMV[r][s][t];
    
    { // Compute the resulting motion vector
      int f = 1 << r_size;
      int high = (16 * f) - 1;
      int low = ((-16) * f);
      int range = (32 * f);
      
      vector = prediction + delta;
      if(vector < low) {
	vector = vector + range;
      }
      else if(vector > high) {
	vector = vector - range;
      }
    }
    
    // Update predictors
    if((mb.mv_format ==  MV_FORMAT_FIELD) && (t==1) &&
       (pic.coding_ext.picture_structure ==  0x3))
      pic.PMV[r][s][t] = vector * 2;
    else
      pic.PMV[r][s][t] = vector;
    
    // Scale the vector so that it is always measured in half pels
    if((s == 0 && pic.header.full_pel_forward_vector) || 
       (s == 1 && pic.header.full_pel_backward_vector))
      mb.vector[r][s][t] = vector << 1;
    else
      mb.vector[r][s][t] = vector;
  }
}


/* 6.2.5.2 Motion vectors */
static inline
void motion_vectors(unsigned int s)
{
  DPRINTF(3, "motion_vectors(%u)\n", s);
  
  /* This does not differ from MPEG-2 if:
     motion_vector_count == 1 && mv_format == MV_FORMAT_FRAME
     but this is shorter. */
  
  motion_vector(0, s);
}


/* 6.2.5.1 Macroblock modes */
static
void macroblock_modes(void)
{
  DPRINTF(3, "macroblock_modes\n");

  if(pic.header.picture_coding_type == 0x01) {
    /* I-picture */
    mb.modes.macroblock_type = get_vlc(table_b2, "macroblock_type (b2)");

  } else if(pic.header.picture_coding_type == 0x02) {
    /* P-picture */
    mb.modes.macroblock_type = get_vlc(table_b3, "macroblock_type (b3)");

  } else if(pic.header.picture_coding_type == 0x03) {
    /* B-picture */
    mb.modes.macroblock_type = get_vlc(table_b4, "macroblock_type (b4)");
    
  } else {
    fprintf(stderr, "*** Unsupported picture type %02x\n", pic.header.picture_coding_type);
    exit_program(-1);
  }
  
  mb.modes.macroblock_quant = mb.modes.macroblock_type & MACROBLOCK_QUANT;
  mb.modes.macroblock_motion_forward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD;
  mb.modes.macroblock_motion_backward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD;
  mb.modes.macroblock_pattern = mb.modes.macroblock_type & MACROBLOCK_PATTERN;
  mb.modes.macroblock_intra = mb.modes.macroblock_type & MACROBLOCK_INTRA;

#if 0
  mb.modes.spatial_temporal_weight_code_flag =
    mb.modes.macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG;
  
  DPRINTF(5, "spatial_temporal_weight_code_flag: %01x\n", mb.modes.spatial_temporal_weight_code_flag);

  if((mb.modes.spatial_temporal_weight_code_flag == 1) &&
     ( 1 /*spatial_temporal_weight_code_table_index != 0*/)) {
    mb.modes.spatial_temporal_weight_code = GETBITS(2, "spatial_temporal_weight_code");
  }

  if(mb.modes.macroblock_motion_forward ||
     mb.modes.macroblock_motion_backward) {
    if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
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
  if((pic.coding_ext.picture_structure == 0x03 /*frame*/) &&
     (pic.coding_ext.frame_pred_frame_dct == 0) &&
     (mb.modes.macroblock_intra || mb.modes.macroblock_pattern)) {
    
    mb.modes.dct_type = GETBITS(1, "dct_type");
  } else {
    /* Table 6-19. Value of dct_type if dct_type is not in the bitstream.*/
    if(pic.coding_ext.frame_pred_frame_dct == 1) {
      mb.modes.dct_type = 0;
    }
    /* else dct_type is unused, either field picture or mb not coded */
  }
#endif

}


/* 6.2.5 Macroblock */
static
void macroblock(void)
{
  uint16_t inc_add = 0;
  
  DPRINTF(3, "macroblock()\n");

  // MPEG-1 start   (this does not occur in an MPEG-2 stream)
  //                 (not a valid macroblock_escape code)
  while(nextbits(11) == 0x00F) {
    GETBITS(11, "macroblock_stuffing");
  }
  // MPEG-1 end


  while(nextbits(11) == 0x008) {
    GETBITS(11, "macroblock_escape");
    inc_add += 33;
  }

  mb.macroblock_address_increment
    = get_vlc(table_b1, "macroblock_address_increment");

  mb.macroblock_address_increment += inc_add;
  
  seq.macroblock_address
    =  mb.macroblock_address_increment + seq.previous_macroblock_address;
  
  seq.previous_macroblock_address = seq.macroblock_address;


  seq.mb_column = seq.macroblock_address % seq.mb_width;
  seq.mb_row = seq.macroblock_address / seq.mb_width;

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
  
  
  /* In a P-picture when a macroblock is skipped */
  if(pic.header.picture_coding_type == 0x2) {
    if(mb.macroblock_address_increment > 1) {
      reset_PMV();
    }
  }
  
  
  if(mb.macroblock_address_increment > 1) {
    
    int i;
    int new_address = seq.macroblock_address;
    
    mb.skipped = 1;
    
    if(show_stat) {
      i = mb.macroblock_address_increment;
      while( --i > 0 ) {
	seq.macroblock_address = new_address - i;
	
	switch(pic.header.picture_coding_type) {
	case 0x1:
	case 0x2:
	  memcpy(&ref2_mbs[seq.macroblock_address], &mb, sizeof(mb));
	  break;
	case 0x3:
	  memcpy(&cur_mbs[seq.macroblock_address], &mb, sizeof(mb));
	  break;
	}
      }
      seq.macroblock_address = new_address;
    }

    
    /* Skipped blocks never have any DCT-coefficients */
    // mb.cbp = 0;
    
    switch(pic.header.picture_coding_type) {
    
    case 0x2:
      DPRINTF(3,"skipped in P-picture\n");
      
      /* There is plenty of room to optimize this */
      
      /* Asume prediction is forward with a zero vector */
      mb.modes.macroblock_motion_forward = 1;
      reset_vectors();
      
      i = mb.macroblock_address_increment;
      while( --i > 0 ) {
	seq.mb_column = (seq.macroblock_address - i) % seq.mb_width;
	seq.mb_row    = (seq.macroblock_address - i) / seq.mb_width;
	motion_comp();
      }
      seq.mb_column = seq.macroblock_address % seq.mb_width;
      seq.mb_row    = seq.macroblock_address / seq.mb_width;
      break;
    
    case 0x3:
      DPRINTF(3,"skipped in B-frame\n");
      
      /* There is some room to optimize this */
      
      i = mb.macroblock_address_increment;
      while( --i > 0 ) {
	seq.mb_column = (seq.macroblock_address - i) % seq.mb_width;
	seq.mb_row    = (seq.macroblock_address - i) / seq.mb_width;
	motion_comp();
      }
      seq.mb_column = seq.macroblock_address % seq.mb_width;
      seq.mb_row    = seq.macroblock_address / seq.mb_width;
      break;
    
    default:
      fprintf(stderr, "*** skipped blocks in I-picture\n");
      break;
    }
  }

  mb.skipped = 0;
  

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
    mb.quantiser_scale = GETBITS(5, "quantiser_scale_code");
#ifdef STATS
    new_scaled = 1;
#endif
  }
  
#ifdef STATS
  stats_quantiser_scale_possible++;
  if(new_scaled) {
    stats_quantiser_scale_nr++;
  }
  if(mb.modes.macroblock_intra) {
    stats_intra_quantiser_scale_possible++;
    if(new_scaled) {
      stats_intra_quantiser_scale_nr++;
      new_scaled = 0;
    }
  } else {
    stats_non_intra_quantiser_scale_possible++;
    if(new_scaled) {
      stats_non_intra_quantiser_scale_nr++;
      new_scaled = 0;
    }
  }    
#endif
  
  if(mb.modes.macroblock_motion_forward)
    motion_vectors(0);
  if(mb.modes.macroblock_motion_backward)
    motion_vectors(1);
  
  
  /* All motion vectors for the block has been
     decoded. Update predictors. */
  
  // Is this correct?
  if ((mb.modes.macroblock_motion_forward == 0) &&
      (mb.modes.macroblock_motion_backward == 0)) {
    reset_PMV ();
  }
 
  switch (pic.header.picture_coding_type) {
  case 0x01: /* I-picture */
    break;
  case 0x02: /* P-picture */
    if(mb.modes.macroblock_intra) {
      reset_PMV();
      reset_vectors ();
    }
    if((!mb.modes.macroblock_intra) && (!mb.modes.macroblock_motion_forward)) {
      reset_PMV();
      /* Asume prediction is forward with a zero vector */
      mb.modes.macroblock_motion_forward = 1;
      reset_vectors ();
    }
    break;
  case 0x03: /* B-picture */
    if(mb.modes.macroblock_intra) {
      reset_PMV ();
      reset_vectors ();
    }
    break;
  }
  
  
  /* Pel reconstruction 
     - motion compensation (if any)
     - decode coefficents
     - inverse quantisation 
     - 'oddify' the coefficents
     - inverse transform
     - combine the data with motion compensated pels */
  
  if(mb.modes.macroblock_intra) {
    /* Intra blocks always have all sub block and are writen directly 
       to the output buffers by block() */
    int i;
    
    for(i = 0; i < 6; i++) {  
      DPRINTF(4, "cbpindex: %d assumed\n", i);
      block_intra(i);
      
      /* Shortcut: write the IDCT data directly into the picture buffer */
      {
	const int x = seq.mb_column;
	const int y = seq.mb_row;
	const int width = seq.horizontal_size;
	int d, stride;
	uint8_t *dst;
	
	if (mb.modes.dct_type) {
	  d = 1;
	  stride = width * 2;
	} else {
	  d = 8;
	  stride = width;
	}
	
	if(i < 4) {
	  dst = &dst_image->y[x * 16 + y * width * 16];
	  dst = (i & 1) ? dst + 8 : dst;
	  dst = (i >= 2) ? dst + width * d : dst;
	}
	else {
	  stride = width / 2; // HACK alert !!!
	  if( i == 4 )
	    dst = &dst_image->u[x * 8 + y * width/2 * 8];
	  else // i == 5
	    dst = &dst_image->v[x * 8 + y * width/2 * 8];
	}
	
	mlib_VideoIDCT8x8_U8_S16(dst, (int16_t *)mb.QFS, stride);
      }
    }
  } 
  else {
    
    motion_comp(); // Only motion compensate don't add coefficients   

    if(mb.modes.macroblock_pattern) {
      int i;

      coded_block_pattern();
      
      for(i = 0; i < 6; i++) {
	if(mb.cbp & (1<<(5-i))) {
	  DPRINTF(4, "cbpindex: %d set\n", i);
	  block_non_intra(i);
	  mlib_VideoIDCT8x8_S16_S16((int16_t *)mb.QFS, (int16_t *)mb.QFS);
	  motion_comp_add_coeff(i);
	}
      }
    }
  
  }
    

  if(show_stat) {
    switch(pic.header.picture_coding_type) {
    case 0x1:
    case 0x2:
      memcpy(&ref2_mbs[seq.macroblock_address], &mb, sizeof(mb));
      break;
    case 0x3:
      memcpy(&cur_mbs[seq.macroblock_address], &mb, sizeof(mb));
      break;
    } 
  }
  
}


/* 6.2.4 Slice */
void mpeg1_slice(void)
{
  uint32_t slice_start_code;
  
  DPRINTF(3, "slice\n");

  
  reset_dc_dct_pred();
  reset_PMV();
  reset_vectors();

  DPRINTF(3, "start of slice\n");
  
  slice_start_code = GETBITS(32, "slice_start_code");
  slice_data.slice_vertical_position = slice_start_code & 0xff;
  
  // Do we need to update seq.mb_col ???
  seq.mb_row = slice_data.slice_vertical_position - 1;
  seq.previous_macroblock_address = (seq.mb_row * seq.mb_width) - 1;

  mb.quantiser_scale = GETBITS(5, "quantiser_scale_code");
#ifdef STATS
  new_scaled = 1;
#endif

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
  
  next_start_code();
}

