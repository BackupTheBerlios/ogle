#ifndef QUEUE_H
#define QUEUE_H

#include <ogle/msgevents.h>

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
  char filename[PATH_MAX+1]; // hack for mmap
  int flowcmd;
} data_elem_t;


typedef struct {
  int y_offset; //[480][720];  //y-component image
  int u_offset; //[480/2][720/2]; //u-component
  int v_offset; //[480/2][720/2]; //v-component
  uint16_t start_x, start_y;
  uint16_t horizontal_size;
  uint16_t vertical_size;
  uint16_t padded_width, padded_height;
  uint16_t display_start_x;
  uint16_t display_start_y;
  uint16_t display_width;
  uint16_t display_height;
  float sar;
} yuv_picture_t;

typedef struct {
  uint8_t *y; //[480][720];  //y-component image
  uint8_t *u; //[480/2][720/2]; //u-component
  uint8_t *v; //[480/2][720/2]; //v-component
  yuv_picture_t *info;
} yuv_image_t;

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
  yuv_picture_t picture;
  int picture_off;
  int picture_len;
  long int frame_interval;
  char *q_addr;
} picture_data_elem_t;

typedef struct {
  int shmid;
  int nr_of_dataelems;
  int write_nr;
  int read_nr;
  int buffer_start_offset;
  int buffer_size;
} data_buf_head_t;

typedef struct {
  int data_elem_index;
  int in_use;
} q_elem_t;


#define BUFS_FULL 0
#define BUFS_EMPTY 1
typedef struct {
  int qid;
  int data_buf_shmid;
  int nr_of_qelems;
  int write_nr;
  int read_nr;
  MsgEventClient_t writer;
  MsgEventClient_t reader;
  int writer_requests_notification; //writer sets/unsets this
  int reader_requests_notification; //reader sets/unsets this 
} q_head_t;

#endif
