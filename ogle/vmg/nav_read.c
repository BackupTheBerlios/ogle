/* 
 * Copyright (C) 2000 Håkan Hjort <d95hjort@dtek.chalmers.se>
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
#include <inttypes.h>
#include <assert.h>

#include "nav.h"
#include "nav_read.h"


/* FIXME: Autoconf this */
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


void read_pci_packet(pci_t *pci, buffer_t *buffer) {
  int i, j, k;
  
  assert(sizeof(pci_t) == PCI_BYTES - 1); // -1 for substream id
  
  memcpy(pci, &(buffer->bytes [buffer->bit_position>>3]), sizeof(pci_t));
  buffer->bit_position += 8 * (PCI_BYTES - 1);
  
  /* -- endian conversions ------------------------------------------------- */
  
  /* pci pci_gi */
  B2N_32(pci->pci_gi.nv_pck_lbn);
  B2N_16(pci->pci_gi.vobu_cat);
  B2N_32(pci->pci_gi.vobu_uop_ctl);
  B2N_32(pci->pci_gi.vobu_s_ptm);
  B2N_32(pci->pci_gi.vobu_e_ptm);
  B2N_32(pci->pci_gi.vobu_se_e_ptm);
  B2N_32(pci->pci_gi.e_eltm);
  
  /* pci nsml_agli */
  for(i = 0; i < 9; i++)
    B2N_32(pci->nsml_agli.nsml_agl_dsta[i]);
  
  /* pci hli hli_gi */
  B2N_16(pci->hli.hl_gi.hli_ss);
  B2N_32(pci->hli.hl_gi.hli_s_ptm);
  B2N_32(pci->hli.hl_gi.hli_e_ptm);
  B2N_32(pci->hli.hl_gi.btn_se_e_ptm);
  
  /* pci hli btn_colit */
  for(i = 0; i < 3; i++)
    for(j = 0; j < 2; j++)
      B2N_32(pci->hli.btn_colit.btn_coli[i][j]);
  
#ifndef WORDS_BIGENDIAN
  /* pci hli btni */
  for(i = 0; i < 36; i++) {
    char tmp[6], swap;
    memcpy(tmp, &(pci->hli.btnit[i]), 6);
    swap = tmp[0]; tmp[0] = tmp[2]; tmp[2] = swap;
    swap = tmp[3]; tmp[3] = tmp[5]; tmp[5] = swap;
    memcpy(&(pci->hli.btnit[i]), tmp, 6);
  }
#endif
  
  
  /* -- asserts ------------------------------------------------------------ */
  
#ifndef NDEBUG
  /* pci pci gi */ 
  assert(pci->pci_gi.zero1 == 0);
  
  /* pci hli hli_gi */
  assert(pci->hli.hl_gi.zero1 == 0);
  assert(pci->hli.hl_gi.zero2 == 0);
  assert(pci->hli.hl_gi.zero3 == 0);
  assert(pci->hli.hl_gi.zero4 == 0);
  assert(pci->hli.hl_gi.zero5 == 0);
  
  /* Is there a menu defined here? */
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0) {
    assert(pci->hli.hl_gi.btn_ns != 0); 
    assert(pci->hli.hl_gi.btngr_ns != 0); 
  } else {
    assert((pci->hli.hl_gi.btn_ns != 0 && pci->hli.hl_gi.btngr_ns != 0) 
	   || (pci->hli.hl_gi.btn_ns == 0 && pci->hli.hl_gi.btngr_ns == 0));
  }
  
  /* pci hli btnit */
  for(i = 0; i < pci->hli.hl_gi.btngr_ns; i++) {
    for(j = 0; j < (36 / pci->hli.hl_gi.btngr_ns); j++) {
      int n = (36 / pci->hli.hl_gi.btngr_ns) * i + j;
      assert(pci->hli.btnit[n].zero1 == 0);
      assert(pci->hli.btnit[n].zero2 == 0);
      assert(pci->hli.btnit[n].zero3 == 0);
      assert(pci->hli.btnit[n].zero4 == 0);
      assert(pci->hli.btnit[n].zero5 == 0);
      assert(pci->hli.btnit[n].zero6 == 0);
      
      if (j < pci->hli.hl_gi.btn_ns) {	
	assert(pci->hli.btnit[n].x_start < pci->hli.btnit[n].x_end);
	assert(pci->hli.btnit[n].y_start < pci->hli.btnit[n].y_end);
	assert(pci->hli.btnit[n].up <= pci->hli.hl_gi.btn_ns);
	assert(pci->hli.btnit[n].down <= pci->hli.hl_gi.btn_ns);
	assert(pci->hli.btnit[n].left <= pci->hli.hl_gi.btn_ns);
	assert(pci->hli.btnit[n].right <= pci->hli.hl_gi.btn_ns);
	//vmcmd_verify(pci->hli.btnit[n].cmd);
      } else {
	assert(pci->hli.btnit[n].btn_coln == 0);
	assert(pci->hli.btnit[n].auto_action_mode == 0);
	assert(pci->hli.btnit[n].x_start == 0);
	assert(pci->hli.btnit[n].y_start == 0);
	assert(pci->hli.btnit[n].x_end == 0);
	assert(pci->hli.btnit[n].y_end == 0);
	assert(pci->hli.btnit[n].up == 0);
	assert(pci->hli.btnit[n].down == 0);
	assert(pci->hli.btnit[n].left == 0);
	assert(pci->hli.btnit[n].right == 0);
	for (k = 0; k < 8; k++)
	  assert(pci->hli.btnit[n].cmd.bytes[k] == 0); //CHECK_ZERO?
      }
    }
  }
#endif
}

void read_dsi_packet(dsi_t *dsi, buffer_t *buffer) {
  
  assert(sizeof(dsi_t) == DSI_BYTES - 1); // -1 for substream id
  
  memcpy(dsi, &buffer->bytes [buffer->bit_position>>3], sizeof(dsi_t));
  buffer->bit_position += 8 * (DSI_BYTES - 1);
  
  
  /* FIXME: Endian conversions & asserts */
  
  /* -- endian conversions ------------------------------------------------- */
  
  /* dsi dsi gi */
  B2N_32(dsi->dsi_gi.nv_pck_scr);
  B2N_32(dsi->dsi_gi.nv_pck_lbn);
  B2N_32(dsi->dsi_gi.vobu_ea);
  B2N_32(dsi->dsi_gi.vobu_1stref_ea);
  B2N_32(dsi->dsi_gi.vobu_2ndref_ea);
  B2N_32(dsi->dsi_gi.vobu_3rdref_ea);
  B2N_16(dsi->dsi_gi.vobu_vob_idn);
  B2N_32(dsi->dsi_gi.c_eltm);
  
  
  
  /* -- asserts ------------------------------------------------------------ */
  
#ifndef NDEBUG
  /* dsi dsi gi */
  assert(dsi->dsi_gi.zero1 == 0);
#endif
}

