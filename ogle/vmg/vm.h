#ifndef VM_H
#define VM_H

#include "decoder.h"


typedef enum {
  FP_DOMAIN = 1,
  VTS_DOMAIN = 2,
  VMGM_DOMAIN = 4,
  VTSM_DOMAIN = 8
} domain_t;  

/**
 * State: SPRM, GPRM, Domain, pgc, pgN, cellN, ?
 */
typedef struct {
  registers_t registers;
  
  pgc_t *pgc; // either this or *pgc is enough?
  
  domain_t domain;
  int vtsN; // 0 is vmgm?
  //  int pgcN; // either this or *pgc is enough. Which to use?
  int pgN;  // is this needed? can allways fid pgN from cellN?
  int cellN;
  int blockN;
  
  /* Resume info */
  int rsm_vtsN;
  int rsm_blockN; /* of nav_packet */
  uint16_t rsm_regs[5]; /* system registers 4-8 */
  int rsm_pgcN;
  int rsm_cellN;
} dvd_state_t;


// Audio stream number
#define AST_REG      registers.SPRM[1]
// Subpicture stream number
#define SPST_REG     registers.SPRM[2]
// Angle number
#define AGL_REG      registers.SPRM[3]
// Title Track Number
#define TTN_REG      registers.SPRM[4]
// VTS Title Track Number
#define VTS_TTN_REG  registers.SPRM[5]
#define TT_PGCN_REG  registers.SPRM[6]
// Current Part of Title (PTT) number for (One_Sequential_PGC_Title)
#define PTTN_REG     registers.SPRM[7]

// Highlighted Button Number (btn nr 1 == value 1024)
#define HL_BTNN_REG  registers.SPRM[8]

// Parental Level
#define PTL_REG      registers.SPRM[13]

#endif /* VM_H */


// vm.c
dvd_state_t state;
int vm_reset(char *dvdroot);
int vm_start(void);
int vm_eval_cmd(vm_cmd_t *cmd);
int vm_get_next_cell();
int vm_menu_call(DVDMenuID_t menuid, int block);
int vm_resume(void);
int vm_top_pg(void);
int vm_next_pg(void);
int vm_prev_pg(void);
int vm_get_audio_stream(int audioN);
int vm_get_subp_stream(int subpN);
int vm_get_subp_active_stream(void);
void vm_get_angle_info(int *num_avail, int *current);
void vm_get_audio_info(int *num_avail, int *current);
void vm_get_subp_info(int *num_avail, int *current);
subp_attr_t vm_get_subp_attr(int streamN);
audio_attr_t vm_get_audio_attr(int streamN);
