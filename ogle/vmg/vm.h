#ifndef VM_H
#define VM_H

#include "decoder.h"


typedef enum {
  FP_DOMAIN = 1,
  VTS_DOMAIN = 2,
  VMGM_DOMAIN = 4,
  VTSM_DOMAIN = 8
} domain_t;  

/*
  State: SPRM, GPRM, Domain, pgc, pgN, cellN, ?
*/
typedef struct {
  registers_t registers;
  domain_t domain;
  int pgN;
  int cellN;
  int rsm_vtsN;
  int rsm_pgcN;
  int rsm_cellN;
  int rsm_blockN;
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
// Parental Level
#define PTL_REG      registers.SPRM[13]

#endif /* VM_H */
