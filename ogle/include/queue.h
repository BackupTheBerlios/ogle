#ifndef QUEUE_H
#define QUEUE_H

#include "ip_sem.h"


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
  char *q_addr;
} data_elem_t;

typedef struct {
  uint64_t PTS;
  uint64_t DTS;
  uint64_t SCR_base;
  uint16_t SCR_ext;
  int displayed;
  int is_reference;
  uint8_t PTS_DTS_flags;
  uint8_t SCR_flags;
  int scr_nr;
  yuv_image_t picture;
  int picture_off;
  int picture_len;
  char *q_addr;
} picture_data_elem_t;

typedef struct {
  int shmid;
  int nr_of_dataelems;
  int write_nr;
  int read_nr;
} data_buf_head_t;

typedef struct {
  int data_elem_index;
} q_elem_t;


#define BUFS_FULL 0
#define BUFS_EMPTY 1
typedef struct {
  ip_sem_t queue;
  int data_buf_shmid;
  int nr_of_qelems;
  int write_nr;
  int read_nr;
} q_head_t;

#endif
