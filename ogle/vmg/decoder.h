#ifndef DECODER_H_INCLUDED
#define DECODER_H_INCLUDED

#include <inttypes.h>
#include "ifo.h" // vm_cmd_t

#ifndef bool
typedef int bool;
#endif

typedef enum {
  LinkNoLink  = 0,

  LinkTopC    = 1,
  LinkNextC   = 2,
  LinkPrevC   = 3,

  LinkTopPG   = 5,
  LinkNextPG  = 6,
  LinkPrevPG  = 7,

  LinkTopPGC  = 9,
  LinkNextPGC = 10,
  LinkPrevPGC = 11,
  LinkGoUpPGC = 12,
  LinkTailPGC = 13,

  LinkRSM     = 16,

  LinkPGCN,
  LinkPTTN,
  LinkPGN,
  LinkCN,

  Exit,

  JumpTT,
  JumpVTS_TT,
  JumpVTS_PTT,

  JumpSS_FP,
  JumpSS_VMGM_MENU,
  JumpSS_VTSM,
  JumpSS_VMGM_PGC,

  CallSS_FP,
  CallSS_VMGM_MENU,
  CallSS_VTSM,
  CallSS_VMGM_PGC,

  PlayThis
} link_cmd_t;

typedef struct {
  link_cmd_t command;
  uint16_t   data1;
  uint16_t   data2;
  uint16_t   data3;
} link_t;

typedef struct {
  uint16_t SPRM[24];
  uint16_t GPRM[16];
  /* Need to have some thing to indicate normal/counter mode for every GPRM */
  /* int GPRM_mode[16]; */
} registers_t;

bool
eval(vm_cmd_t commands[], int num_commands, 
     registers_t *registers, link_t *return_values);

#endif /* DECODER_H_INCLUDED */
