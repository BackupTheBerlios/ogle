#ifndef __NAV_H__
#define __NAV_H__

/* 
 * Copyright (C) 2000 Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * The data structures in this file should represent the layout of the 
 * pci and dsi packets as they are stored in the stream. 
 * Information found by reading the source to VOBDUMP is the base for 
 * the structure and names of these data types.
 *
 * VOBDUMP: a program for examining DVD .VOB filse
 *
 * Copyright 1998, 1999 Eric Smith <eric@brouhaha.com>
 *
 * VOBDUMP is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  Note that I am not
 * granting permission to redistribute or modify VOBDUMP under the
 * terms of any later version of the General Public License.
 *
 * This program is distributed in the hope that it will be useful (or
 * at least amusing), but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <inttypes.h>
#include "ifo.h" // vm_cmd_t


#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define PS2_PCI_SUBSTREAM_ID 0x00
#define PS2_DSI_SUBSTREAM_ID 0x01


typedef struct {
  uint32_t nv_pck_lbn;
  uint16_t vobu_cat;
  uint16_t zero1;
  uint32_t vobu_uop_ctl;
  uint32_t vobu_s_ptm;
  uint32_t vobu_e_ptm;
  uint32_t vobu_se_e_ptm;
  uint32_t e_eltm;
  char vobu_isrc[32];
} __attribute__ ((packed)) pci_gi_t;


typedef struct { /* non seamless angle information */
  uint32_t nsml_agl_dsta[9]; 
} __attribute__ ((packed)) nsml_agli_t;


typedef struct {
  uint16_t hli_ss; // only low 2 bits
  uint32_t hli_s_ptm;
  uint32_t hli_e_ptm;
  uint32_t btn_se_e_ptm;
#ifdef WORDS_BIGENDIAN
  unsigned int zero1 : 2;
  unsigned int btngr_ns : 2;
  unsigned int zero2 : 1;
  unsigned int btngr1_dsp_ty : 3;
  unsigned int zero3 : 1;
  unsigned int btngr2_dsp_ty : 3;
  unsigned int zero4 : 1;
  unsigned int btngr3_dsp_ty : 3;
#else
  unsigned int btngr1_dsp_ty : 3;
  unsigned int zero2 : 1;
  unsigned int btngr_ns : 2;
  unsigned int zero1 : 2;
  unsigned int btngr3_dsp_ty : 3;
  unsigned int zero4 : 1;
  unsigned int btngr2_dsp_ty : 3;
  unsigned int zero3 : 1;
#endif
  uint8_t btn_ofn;
  uint8_t btn_ns;     // only low 6 bits
  uint8_t nsl_btn_ns; // only low 6 bits
  uint8_t zero5;
  uint8_t fosl_btnn;  // only low 6 bits
  uint8_t foac_btnn;  // only low 6 bits
} __attribute__ ((packed)) hl_gi_t;

typedef struct {
  uint32_t btn_coli[3][2];
} __attribute__ ((packed)) btn_colit_t;

typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int btn_coln : 2;
  unsigned int x_start : 10;
  unsigned int zero1 : 2;
  unsigned int x_end : 10;
  unsigned int auto_action_mode : 2;
  unsigned int y_start : 10;
  unsigned int zero2 : 2;
  unsigned int y_end : 10;

  unsigned int zero3 : 2;
  unsigned int up : 6;
  unsigned int zero4 : 2;
  unsigned int down : 6;
  unsigned int zero5 : 2;
  unsigned int left : 6;
  unsigned int zero6 : 2;
  unsigned int right : 6;
#else
  unsigned int x_end : 10;
  unsigned int zero1 : 2;
  unsigned int x_start : 10;
  unsigned int btn_coln : 2;
  unsigned int y_end : 10;
  unsigned int zero2 : 2;
  unsigned int y_start : 10;
  unsigned int auto_action_mode : 2;

  unsigned int up : 6;
  unsigned int zero3 : 2;
  unsigned int down : 6;
  unsigned int zero4 : 2;
  unsigned int left : 6;
  unsigned int zero5 : 2;
  unsigned int right : 6;
  unsigned int zero6 : 2;
#endif
  vm_cmd_t cmd;
} __attribute__ ((packed)) btni_t;

typedef struct {
  hl_gi_t     hl_gi;
  btn_colit_t btn_colit;
  btni_t      btnit[36];
} __attribute__ ((packed)) hli_t;

typedef struct {
  pci_gi_t    pci_gi;
  nsml_agli_t nsml_agli;
  hli_t       hli;
  uint8_t     zero1[189];
} __attribute__ ((packed)) pci_t;






typedef struct { /* DSI General Information */
  uint32_t nv_pck_scr;
  uint32_t nv_pck_lbn;
  uint32_t vobu_ea;
  uint32_t vobu_1stref_ea;
  uint32_t vobu_2ndref_ea;
  uint32_t vobu_3rdref_ea;
  uint16_t vobu_vob_idn;
  uint8_t  zero1;
  uint8_t  vobu_c_idn;
  uint32_t c_eltm;
} __attribute__ ((packed)) dsi_gi_t;

typedef struct { /* Seamless PlayBack Information */ 
  uint16_t category; // category of seamless VOBU
  /* Xyyy yyyy PREU flag        1b: VOBU is in preunit 
   *                            0b: VOBU is not in preunit
   * yXyy yyyy ILVU flag        1b: VOBU is in ILVU
   *                            0b: VOBU is not in ILVU described
   * yyXy yyyy Unit Start flag  1b: VOBU at the beginning of ILVU described
   *                            0b: VOBU not at the beginning
   * yyyX yyyy Unit End flag    1b: VOBU at end of PREU of ILVU described
   *                            0b: not at the end
   */
  uint32_t ilvu_ea;  // end address of interleaved Unit (sectors)
  uint32_t ilvu_sa;  // start address of next interleaved unit (sectors)
  uint16_t size;     // size of next interleaved unit (sectors)
  uint32_t unknown_s_ptm; // ?? first pts in cell/ pts of first object 
  uint32_t unknown_e_ptm; // ?? last pts in cell/ pts of last object
 
  uint8_t unknown[128];
} __attribute__ ((packed)) sml_pbi_t;

typedef struct { /* AnGLe Information for seamless playback */
  // Address and size of destination ILVU in AGL_X
  struct {
    uint32_t address; // Sector offset to next ILVU, high bit is before/after
    uint16_t size;    // Byte size of the ILVU pointed to by address.
  } __attribute__ ((packed)) dsta[9];
} __attribute__ ((packed)) sml_agli_t;

typedef struct { /* VOBUnit SeaRch Information */
  /*
    bit 0: V_FWD_Exist1  0b: *Video data* does not exist in the address
                         1b: *video data* exists in the VOBU on the address
    bit 1: V_FWD_Exist2  indicates whether theres *video data* between 
			 current vobu and last vobu.
    if address = 3fff ffff -> vobu does not exist
  */
  
  /* 1     FWDI Video     VOBU start address with video data
     n = 0.5 seconds after presentationtime of current VOBU
     2 n=240
     3 120
     4 60
     5 20
     6 15
     7 14
     ..
     20 1
     21 Next VOBU start addres
     22 previous VOBU start address
     23 - end backwards
     23   n = 1
     24 n = 2
     ..
     37 n = 15
     38  20
     39 60
     40 120
     41 240
     42 .....
  */
  uint32_t next_video;   // Next (nv_pck_lbn+this value -> next vobu lbn)
  uint32_t FWDA[19]; // Forwards, time
  uint32_t next;
  uint32_t prev;
  uint32_t BWDA[19]; // Backwars, time
  uint32_t prev_video; // Previous (nv_pck_lbn-this value -> prev. vobu lbn)
} __attribute__ ((packed)) vobu_sri_t;

typedef struct { /* SYNChronous Information */ 
  uint16_t unknown_offset;      //  Highbit == signbit
  uint8_t unknown1[14];
  uint32_t start_of_cell_offset1; //?? 7f ff ff ff == at start
  uint32_t start_of_cell_offset2; //??  
  uint8_t unknown2[120];
} __attribute__ ((packed)) synci_t;

typedef struct {
  dsi_gi_t   dsi_gi;
  sml_pbi_t  sml_pbi;
  sml_agli_t sml_agli;
  vobu_sri_t vobu_sri;
  synci_t    synci;
  uint8_t    zero1[471];
} __attribute__ ((packed)) dsi_t;


#endif /* __NAV_H__ */
