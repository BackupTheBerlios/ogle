#ifndef __DECODER_H__
#define __DECODER_H__

#include <inttypes.h>

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
  CallSS_VMGM_PGC
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
} state_t;

bool
eval(uint8_t commands[][8], int num_commands, 
     state_t *registers, link_t *return_values);

#endif /* __DECODER_H__ */
