/*
 * Copyright (C) 2000 Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * Much of the contents in this file is based on VOBDUMP.
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

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include "nav.h"
#include "nav_print.h"
#include "vmcmd.h"


/* FIXME: Move this function to a common file. */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; // The two high bits of this byte represent the frame rate.
}  __attribute__ ((packed)) dvd_time_t;

void print_time(FILE *out, dvd_time_t *time) {
  char *rate = NULL;
  
  fprintf(out, "%02x:%02x:%02x.%02x", 
	  time->hour, time->minute, time->second, time->frame_u & 0x3f);
  switch((time->frame_u >> 6) & 0x03) {
    case 0: 
      rate = "(please send a bug report)";
      break;
    case 1:
      rate = "25.00";
      break;
    case 2:
      rate = "(please send a bug report)";
      break;
    case 3:
      rate = "29.97";
      break;
  }
  fprintf(out, " @ %s fps", rate);
}



void print_pci_gi(FILE *out, pci_gi_t *pci_gi) {
  int i;

  fprintf(out, "pci_gi:\n");
  fprintf(out, "nv_pck_lbn    0x%08x\n", pci_gi->nv_pck_lbn);
  fprintf(out, "vobu_cat      0x%04x\n", pci_gi->vobu_cat);
  fprintf(out, "vobu_uop_ctl  0x%08x\n", pci_gi->vobu_uop_ctl);
  fprintf(out, "vobu_s_ptm    0x%08x\n", pci_gi->vobu_s_ptm);
  fprintf(out, "vobu_e_ptm    0x%08x\n", pci_gi->vobu_e_ptm);
  fprintf(out, "vobu_se_e_ptm 0x%08x\n", pci_gi->vobu_se_e_ptm);
  fprintf(out, "e_eltm        ");
  print_time(out, (dvd_time_t *)&pci_gi->e_eltm);
  fprintf(out, "\n");
  
  fprintf(out, "vobu_isrc     \"");
  for(i = 0; i < 32; i++) {
    char c = pci_gi->vobu_isrc[i];
    if((c >= ' ') && (c <= '~'))
      fprintf(out, "%c", c);
    else
      fprintf(out, ".");
  }
  fprintf(out, "\"\n");
}

void print_nsml_agli(FILE *out, nsml_agli_t *nsml_agli) {
  int i, j = 0;
  
  for(i = 0; i < 9; i++)
    j |= nsml_agli->nsml_agl_dsta[i];
  if(j == 0)
    return;
  
  fprintf(out, "nsml_agli:\n");
  for(i = 0; i < 9; i++)
    if(nsml_agli->nsml_agl_dsta[i])
      fprintf(out, "nsml_agl_c%d_dsta  0x%08x\n", i + 1, 
	      nsml_agli->nsml_agl_dsta[i]);
}

void print_hl_gi(FILE *out, hl_gi_t *hl_gi, int *btngr_ns, int *btn_ns) {
  
  if((hl_gi->hli_ss & 0x03) == 0)
    return;
  
  fprintf(out, "hl_gi:\n");
  
  fprintf(out, "hli_ss        0x%01x\n", hl_gi->hli_ss & 0x03);
  fprintf(out, "hli_s_ptm     0x%08x\n", hl_gi->hli_s_ptm);
  fprintf(out, "hli_e_ptm     0x%08x\n", hl_gi->hli_e_ptm);
  fprintf(out, "btn_se_e_ptm  0x%08x\n", hl_gi->btn_se_e_ptm);

#if 0
  fprintf(out, "btn_md        0x%04x\n", hl_gi->btn_md);
#else
  *btngr_ns = hl_gi->btngr_ns;
  fprintf(out, "btngr_ns      %d\n",  hl_gi->btngr_ns);
  fprintf(out, "btngr%d_dsp_ty    0x%02x\n", 1, hl_gi->btngr1_dsp_ty);
  fprintf(out, "btngr%d_dsp_ty    0x%02x\n", 2, hl_gi->btngr2_dsp_ty);
  fprintf(out, "btngr%d_dsp_ty    0x%02x\n", 3, hl_gi->btngr3_dsp_ty);
#endif
  
  fprintf(out, "btn_ofn       %d\n", hl_gi->btn_ofn);
  *btn_ns = hl_gi->btn_ns;
  fprintf(out, "btn_ns        %d\n", hl_gi->btn_ns);
  fprintf(out, "nsl_btn_ns    %d\n", hl_gi->nsl_btn_ns);
  fprintf(out, "fosl_btnn     %d\n", hl_gi->fosl_btnn);
  fprintf(out, "foac_btnn     %d\n", hl_gi->foac_btnn);
}

void print_btn_colit(FILE *out, btn_colit_t *btn_colit) {
  int i, j;
  
  j = 0;
  for(i = 0; i < 6; i++)
    j |= btn_colit->btn_coli[i/2][i&1];
  if(j == 0)
    return;
  
  fprintf(out, "btn_colit:\n");
  for(i = 0; i < 3; i++)
    for(j = 0; j < 2; j++)
      fprintf(out, "btn_cqoli %d  %s_coli:  %08x\n",
	      i, (j == 0) ? "sl" : "ac",
	      btn_colit->btn_coli[i][j]);
}

void print_btnit(FILE *out, btni_t *btni_table, int btngr_ns, int btn_ns) {
  int i, j;
  
  fprintf(out, "btnit:\n");
  fprintf(out, "btngr_ns: %i\n", btngr_ns);
  fprintf(out, "btn_ns: %i\n", btn_ns);

  if(btngr_ns == 0)
    return;
  
  for(i = 0; i < btngr_ns; i++)
    for(j = 0; j < (36 / btngr_ns); j++)
      if(j < btn_ns) {
	btni_t *btni = &btni_table[(36 / btngr_ns) * i + j];
	
	fprintf(out, "group %d btni %d:  ", i+1, j+1);
	fprintf(out, "btn_coln %d, auto_action_mode %d\n",
		btni->btn_coln, btni->auto_action_mode);
	fprintf(out, "coords   (%d, %d) .. (%d, %d)\n",
		btni->x_start, btni->y_start, btni->x_end, btni->y_end);

	fprintf(out, "up %d, ", btni->up);
	fprintf(out, "down %d, ", btni->down);
	fprintf(out, "left %d, ", btni->left);
	fprintf(out, "right %d\n", btni->right);
	
	vmcmd(btni->cmd);
	fprintf(out, "\n");
      }
}

void print_hli(FILE *out, hli_t *hli) {
  int btngr_ns = 0, btn_ns = 0;
  
  fprintf(out, "hli:\n");
  print_hl_gi(out, &hli->hl_gi, & btngr_ns, & btn_ns);
  print_btn_colit(out, &hli->btn_colit);
  print_btnit(out, hli->btnit, btngr_ns, btn_ns);
}

void print_pci_packet(FILE *out, pci_t *pci) {
  fprintf(out, "pci packet:\n");
  print_pci_gi(out, &pci->pci_gi);
  print_nsml_agli(out, &pci->nsml_agli);
  print_hli(out, &pci->hli);
}

void print_dsi_gi(FILE *out, dsi_gi_t *dsi_gi) {
  
  fprintf(out, "dsi_gi:\n");
  fprintf(out, "nv_pck_scr     0x%08x\n", dsi_gi->nv_pck_scr);
  fprintf(out, "nv_pck_lbn     0x%08x\n", dsi_gi->nv_pck_lbn );
  fprintf(out, "vobu_ea        0x%08x\n", dsi_gi->vobu_ea);
  fprintf(out, "vobu_1stref_ea 0x%08x\n", dsi_gi->vobu_1stref_ea);
  fprintf(out, "vobu_2ndref_ea 0x%08x\n", dsi_gi->vobu_2ndref_ea);
  fprintf(out, "vobu_3rdref_ea 0x%08x\n", dsi_gi->vobu_3rdref_ea);
  fprintf(out, "vobu_vob_idn   0x%04x\n", dsi_gi->vobu_vob_idn);
  fprintf(out, "vobu_c_idn     0x%02x\n", dsi_gi->vobu_c_idn);
  fprintf(out, "c_eltm         ");
  print_time (out, (dvd_time_t *)&dsi_gi->c_eltm);
  fprintf(out, "\n");
}

void print_sml_pbi(FILE *out, sml_pbi_t *sml_pbi) {
  /* $$$ more code needed here */
#if 1
  int i;
  for(i = 0; i < 148; i++) {
    fprintf(stdout, "%02x ", sml_pbi->unknown[i]);
    if(i % 20 == 19)
      fprintf(stdout, "\n");
  }
  if(i % 20 != 19)
    fprintf(stdout, "\n");
#endif
}

void print_sml_agli(FILE *out, sml_agli_t *sml_agli) {
  /* $$$ more code needed here */
#if 1
  int i, j;
  for(i = 0; i < 9; i++) {
    for(j = 0; j < 6; j++)
      fprintf(stdout, "%02x ", sml_agli->unknown[i][j]);
    fprintf(stdout, "\n");
  }
#endif
}

void print_vobu_sri(FILE *out, vobu_sri_t *vobu_sri) {
  /* $$$ more code needed here */
#if 1
  int i;
  fprintf(stdout, "%08x\n", vobu_sri->unknown1);
  for(i = 0; i < 20; i++) {
    fprintf(stdout, "%08x ", vobu_sri->unknown2[i]);
    if(i % 5 == 4)
      fprintf(stdout, "\n");
  }
  fprintf(stdout, "--\n");
  for(i = 0; i < 20; i++) {
    fprintf(stdout, "%08x ", vobu_sri->unknown3[i]);
    if(i % 5 == 4)
      fprintf(stdout, "\n");
  }
  fprintf(stdout, "%08x\n", vobu_sri->unknown4);
#endif
}

void print_synci(FILE *out, synci_t *synci) {
  /* $$$ more code needed here */
#if 1
  int i;
  fprintf(stdout, "%04x\n", synci->offset);
  for(i = 0; i < 142; i++) {
    fprintf(stdout, "%02x ", synci->unknown[i]);
    if(i % 20 == 19)
      fprintf(stdout, "\n");
  }
  if(i % 20 != 19)
    fprintf(stdout, "\n");
#endif
}

void print_dsi_packet(FILE *out, dsi_t *dsi) {
  fprintf(out, "dsi packet:\n");
  print_dsi_gi(out, &dsi->dsi_gi);
  print_sml_pbi(out, &dsi->sml_pbi);
  print_sml_agli(out, &dsi->sml_agli);
  print_vobu_sri(out, &dsi->vobu_sri);
  print_synci(out, &dsi->synci);
}


