#ifndef __IFO_READ_H__
#define __IFO_READ_H__

#include "ifo.h"

//void ifoOpen_DVD?(unsigned int *sector);
int ifoOpen_VMG(vmgi_mat_t *vmgi_mat, char *filename);
int ifoOpen_VTS(vtsi_mat_t *vtsi_mat, char *filename);
void ifoClose();

void ifoRead_VMG_PTL_MAIT(vmg_ptl_mait_t *ptl_mait, int sector);
void ifoRead_VMG_VTS_ATRT(vmg_vts_atrt_t *vts_atrt, int sector);
void ifoRead_VMG_PTT_SRPT(vmg_ptt_srpt_t *vmg_ptt_srpt, int sector);
void ifoRead_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt, int sector);
void ifoRead_PGC(pgc_t *pgc, int offset);
void ifoRead_PGCIT(pgcit_t *pgcit, int offset);
void ifoRead_MENU_PGCI_UT(menu_pgci_ut_t *menu_pgci_ut, int sector);
void ifoRead_C_ADT(c_adt_t *c_adt, int sector);
void ifoRead_VOBU_ADMAP(vobu_admap_t *vobu_admap, int sector);
void ifoRead_VMG_TXTDT_MGI(vmg_txtdt_mgi_t *vmg_txtdt_mgi, int sector);

#endif /* __IFO_READ_H__ */
