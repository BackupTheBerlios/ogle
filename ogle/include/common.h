#ifndef COMMON_H
#define COMMON_H

#include <inttypes.h>
#include <semaphore.h>

#include "timemath.h"

/* "There's len bytes of data for you in your file at this offset." */
struct off_len_packet {
  uint32_t offset;
  uint32_t length;
};

/* "mmap this file now; off_len_packets from now on will be relative
 * this file." */
struct load_file_packet {
  uint32_t length;
  char     filename[200];   // HACK ALERT!
};


/* Imagetype. */

typedef struct {
  uint8_t *y; //[480][720];  //y-component image
  uint8_t *u; //[480/2][720/2]; //u-component
  uint8_t *v; //[480/2][720/2]; //v-component
  uint16_t start_x, start_y;
  uint16_t horizontal_size;
  uint16_t vertical_size;
  uint16_t padded_width, padded_height;
  uint16_t display_start_x;
  uint16_t display_start_y;
  uint16_t display_width;
  uint16_t display_height;
  float sar;
} yuv_image_t;


typedef struct {
  yuv_image_t picture;
  int is_ref;
  int displayed;
  clocktime_t pts_time;
  clocktime_t realtime_offset;
  uint64_t PTS;
  int scr_nr;
} picture_info_t;


typedef struct {
#if defined USE_POSIX_SEM
  sem_t pictures_ready_to_display;
  sem_t pictures_displayed;
#elif defined USE_SYSV_SEM
#define PICTURES_READY_TO_DISPLAY 0
#define PICTURES_DISPLAYED 1
  int semid_pics;
#else
#error No semaphore type set
#endif
  int nr_of_buffers;
  picture_info_t *picture_infos;
  int *dpy_q;
  long int frame_interval;
  int shmid;
  char *shmaddr;
} buf_ctrl_head_t;


typedef enum {
  MODE_STOP,
  MODE_PLAY,
  MODE_PAUSE
} playmode_t;

typedef enum {
  OFFSET_NOT_VALID,
  OFFSET_VALID
} offset_valid_t;


typedef struct {
  clocktime_t realtime_offset;
  offset_valid_t offset_valid;
} ctrl_time_t;


typedef struct {
  playmode_t mode;
} ctrl_data_t;

#endif /* COMMON_H */
