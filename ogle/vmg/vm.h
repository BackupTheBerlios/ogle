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
  int pgcN;
  int pgN;
  int cellN;
  int rsm_pgcN;
  int rsm_pgN;
  int rsm_cellN;
} dvd_state_t;

#endif /* VM_H */
