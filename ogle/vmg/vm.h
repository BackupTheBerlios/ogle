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
#define PGC_REG      registers.SPRM[6]
// Current Part of Title (PTT) number for (One_Sequential_PGC_Title)
#define PTT_REG      registers.SPRM[7]

// Highlighted Button Number (btn nr 1 == value 1024)
#define HL_BTNN_REG  registers.SPRM[8]

// Parental Level
#define PTL_REG      registers.SPRM[13]

#endif /* VM_H */
