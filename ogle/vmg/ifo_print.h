#ifndef __IFO_PRINT_H__
#define __IFO_PRINT_H__

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

#include "ifo.h"

void ifoPrint_time(int level, dvd_time_t *time);
void ifoPrint_COMMAND(int row, vm_cmd_t *command); // remove

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

#endif /* __IFO_PRINT_H__ */
