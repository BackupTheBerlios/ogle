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

#define BLOCK_SIZE 2048
typedef struct
{
  uint32_t bit_position;
  uint8_t bytes [BLOCK_SIZE];
} buffer_t;


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


typedef struct {
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
  uint8_t unknown[148];
} __attribute__ ((packed)) sml_pbi_t;

typedef struct { /* AnGLe Information for seamless playback */
  uint8_t sml_agl_dsta[9][6]; // Address and size of destination ILVU in AGL_X
} __attribute__ ((packed)) sml_agli_t;

typedef struct { /* VOBUnit SeaRch Information */
  uint32_t unknown1;     // Next (nv_pck_lbn+this value -> next vobu lbn)
  uint32_t unknown2[20]; // Forwards, time
  uint32_t unknown3[20]; // Backwars, time
  uint32_t unknown4;     // Previous (nv_pck_lbn-this value -> prev. vobu lbn)
} __attribute__ ((packed)) vobu_sri_t;

typedef struct { /* SYNChronous Information */ 
  uint16_t offset;      //  Highbit == signbit
  uint8_t unknown[142];
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
