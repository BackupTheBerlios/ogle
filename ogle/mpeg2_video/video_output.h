#ifndef OGLE_VIDEO_OUTPUT_H
#define OGLE_VIDEO_OUTPUT_H

#include "common.h"

typedef struct _data_q_t {
  int in_use;
  int eoq;
  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  picture_data_elem_t *data_elems;
  yuv_image_t *image_bufs;
#ifdef HAVE_XV
  yuv_image_t *reserv_image;
#endif
  struct _data_q_t *next;
} data_q_t;



#endif /* OGLE_VIDEO_OUTPUT_H */
