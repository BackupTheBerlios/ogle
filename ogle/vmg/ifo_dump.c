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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ifo.h"
#include "ifo_read.h"
#include "ifo_print.h"



#define PUT(level, text...) \
do { \
  if(level < debug) { \
    fprintf(stdout, ## text); \
  } \
} while(0)

void ifoPrint(char *filename);

int debug = 6;

static char *program_name;

void usage()
{
  fprintf(stderr, "Usage: %s  [-d <debug_level>]\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int c;
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:h?")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }

  if(argc - optind != 1){
    usage();
    return 1;
  }

  ifoPrint(argv[optind]);
  
  exit(0);
}


void ifoPrint(char *filename) {
  vmgi_mat_t vmgi_mat;
  vtsi_mat_t vtsi_mat;
  
  if(ifoOpen_VMG(&vmgi_mat, filename) != NULL) {
    vmg_ptt_srpt_t vmg_ptt_srpt;
    pgc_t pgc;    
    menu_pgci_ut_t vmgm_pgci_ut;
    vmg_ptl_mait_t vmg_ptl_mait;
    vmg_vts_atrt_t vmg_vts_atrt;
    vmg_txtdt_mgi_t vmg_txtdt_mgi;
    c_adt_t vmgm_c_adt;
    vobu_admap_t vmgm_vobu_admap;
     
    
    PUT(1, "VMG top level\n-------------\n");
    ifoPrint_VMGI_MAT(&vmgi_mat);
      
    PUT(1, "\nFirst Play PGC\n--------------\n");
    ifoRead_PGC(&pgc, vmgi_mat.first_play_pgc);
    ifoPrint_PGC(&pgc);
      
    PUT(1, "\nPart of Title (and Chapter) Search pointer table\n");
    PUT(1,   "------------------------------------------------\n");
    /* This needs to be in a IFO? */
    if(vmgi_mat.vmg_ptt_srpt != 0) {
      ifoRead_VMG_PTT_SRPT(&vmg_ptt_srpt, vmgi_mat.vmg_ptt_srpt);
      ifoPrint_VMG_PTT_SRPT(&vmg_ptt_srpt);
    } else
      PUT(5, "No PartOfTitle Searchpointer information present\n");
      
    PUT(1, "\nMenu PGCI Unit table\n");
    PUT(1,   "--------------------\n");
    if(vmgi_mat.vmgm_pgci_ut != 0) {
      ifoRead_MENU_PGCI_UT(&vmgm_pgci_ut, vmgi_mat.vmgm_pgci_ut);
      ifoPrint_MENU_PGCI_UT(&vmgm_pgci_ut);
    } else
      PUT(5, "No PGCI Unit table present\n");
       
      
    PUT(1, "\nParental Manegment Information table\n");
    PUT(1,   "------------------------------------\n");
    if(vmgi_mat.vmg_ptl_mait != 0) {
      ifoRead_VMG_PTL_MAIT(&vmg_ptl_mait, vmgi_mat.vmg_ptl_mait);
      ifoPrint_VMG_PTL_MAIT(&vmg_ptl_mait);
    } else
      PUT(5, "No Parental Management Information present\n");
      
    PUT(1, "\nVideo Title Set Attribute Table\n");
    PUT(1,   "-------------------------------\n");
    if(vmgi_mat.vmg_vts_atrt != 0) {
      ifoRead_VMG_VTS_ATRT(&vmg_vts_atrt, vmgi_mat.vmg_vts_atrt);
      ifoPrint_VMG_VTS_ATRT(&vmg_vts_atrt);
    } else
      PUT(5, "No Video Title Set Attribute Table present\n");
      
    PUT(1, "\nText Data Manager Information\n");
    PUT(1,   "-----------------------------\n");
    if(vmgi_mat.vmg_txtdt_mgi != 0) {
      ifoRead_VMG_TXTDT_MGI(&vmg_txtdt_mgi, vmgi_mat.vmg_txtdt_mgi);
      //ifoPrint_VMG_TXTDT_MGI(&vmg_txtdt_mgi);
    } else
      PUT(5, "No Text Data Manager Information present\n");
      
      
    PUT(1, "\nCell Adress table\n");
    PUT(1,   "-----------------\n");
    if(vmgi_mat.vmgm_c_adt != 0) {
      ifoRead_C_ADT(&vmgm_c_adt, vmgi_mat.vmgm_c_adt);
      ifoPrint_C_ADT(&vmgm_c_adt);
    } else
      PUT(5, "No Cell Adress table present\n");
      
    PUT(1, "\nVideo Title set Menu VOBU address map\n");
    PUT(1,   "-----------------\n");
    if(vmgi_mat.vmgm_vobu_admap != 0) {
      ifoRead_VOBU_ADMAP(&vmgm_vobu_admap, vmgi_mat.vmgm_vobu_admap);
      ifoPrint_VOBU_ADMAP(&vmgm_vobu_admap);
    } else
      PUT(5, "No Menu VOBU address map present\n");   
    ifoClose();
  }

  if(ifoOpen_VTS(&vtsi_mat, filename) != NULL) {
    vts_ptt_srpt_t vts_ptt_srpt;
    pgcit_t vts_pgcit;
    menu_pgci_ut_t vtsm_pgci_ut;
    c_adt_t vtsm_c_adt;
    vobu_admap_t vtsm_vobu_admap;
    c_adt_t vts_c_adt;
    vobu_admap_t vts_vobu_admap;
      
    PUT(1, "VTS top level\n-------------\n");
    ifoPrint_VTSI_MAT(&vtsi_mat);
      
    PUT(1, "\nPart of title search pointer table information\n");
    PUT(1,   "----------------------------------------------\n");
    /* This needs to be in a IFO? */
    if(vtsi_mat.vts_ptt_srpt != 0) {
      ifoRead_VTS_PTT_SRPT(&vts_ptt_srpt, vtsi_mat.vts_ptt_srpt);
      ifoPrint_VTS_PTT_SRPT(&vts_ptt_srpt);
    } else
      PUT(5, "No PGCI Unit table present\n");
       
    PUT(1, "\nPGCI Unit table\n");
    PUT(1,   "--------------------\n");
    if(vtsi_mat.vts_pgcit != 0) {
      ifoRead_PGCIT(&vts_pgcit, vtsi_mat.vts_pgcit * DVD_BLOCK_LEN);
      ifoPrint_PGCIT(&vts_pgcit);
    } else
      PUT(5, "No PGCI Unit table present\n");
      
    PUT(1, "\nMenu PGCI Unit table\n");
    PUT(1,   "--------------------\n");
    if(vtsi_mat.vtsm_pgci_ut != 0) {
      ifoRead_MENU_PGCI_UT(&vtsm_pgci_ut, vtsi_mat.vtsm_pgci_ut);
      ifoPrint_MENU_PGCI_UT(&vtsm_pgci_ut);
    } else
      PUT(5, "No Menu PGCI Unit table present\n");
      
      
    PUT(1, "\nMenu Cell Adress table\n");
    PUT(1,   "-----------------\n");
    if(vtsi_mat.vtsm_c_adt != 0) {
      ifoRead_C_ADT(&vtsm_c_adt, vtsi_mat.vtsm_c_adt);
      ifoPrint_C_ADT(&vtsm_c_adt);
    } else
      PUT(5, "No Cell Adress table present\n");
      
    PUT(1, "\nVideo Title Set Menu VOBU address map\n");
    PUT(1,   "-----------------\n");
    if(vtsi_mat.vtsm_vobu_admap != 0) {
      ifoRead_VOBU_ADMAP(&vtsm_vobu_admap, vtsi_mat.vtsm_vobu_admap);
      ifoPrint_VOBU_ADMAP(&vtsm_vobu_admap);
    } else
      PUT(5, "No Menu VOBU address map present\n");
      
    PUT(1, "\nCell Adress table\n");
    PUT(1,   "-----------------\n");
    if(vtsi_mat.vts_c_adt != 0) {
      ifoRead_C_ADT(&vts_c_adt, vtsi_mat.vts_c_adt);
      ifoPrint_C_ADT(&vts_c_adt);
    } else
      PUT(5, "No Cell Adress table present\n");
      
    PUT(1, "\nVideo Title Set VOBU address map\n");
    PUT(1,   "-----------------\n");
    if(vtsi_mat.vts_vobu_admap != 0) {
      ifoRead_VOBU_ADMAP(&vts_vobu_admap, vtsi_mat.vts_vobu_admap);
      ifoPrint_VOBU_ADMAP(&vts_vobu_admap);
    } else
      PUT(5, "No Menu VOBU address map present\n");  
    ifoClose();
  } 


  /* Vob */
  
}




