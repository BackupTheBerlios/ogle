#ifndef __IFO_PRINT_H__
#define __IFO_PRINT_H__

#include "ifo.h"

void ifoPrint_time(int level, dvd_time_t *time);
void ifoPrint_COMMAND(int row, uint8_t *command); // remove

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
