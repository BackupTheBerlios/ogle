#ifndef QUEUE_H
#define QUEUE_H


#include <semaphore.h>




typedef struct {
  uint64_t PTS;
  uint64_t DTS;
  uint64_t SCR_base;
  uint16_t SCR_ext;
  int in_use;
  uint8_t PTS_DTS_flags;
  uint8_t SCR_flags;
  int scr_nr;
  int off;
  int len;
} q_elem_t;


typedef struct {
  sem_t bufs_full;
  sem_t bufs_empty;
  int buf_shmid;
  int nr_of_qelems;
  int write_nr;
  int read_nr;
} q_head_t;

#endif
