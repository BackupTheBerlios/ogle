#ifndef COMMON_H
#define COMMON_H

#include "timemath.h"
#include "queue.h"


#define PICTURES_READY_TO_DISPLAY 0
#define PICTURES_DISPLAYED 1

typedef enum {
  MODE_STOP,
  MODE_PLAY,
  MODE_PAUSE,
  MODE_SPEED
} playmode_t;

typedef enum {
  OFFSET_NOT_VALID,
  OFFSET_VALID
} offset_valid_t;


typedef struct {
  clocktime_t realtime_offset;
  offset_valid_t offset_valid;
  int sync_master;
} ctrl_time_t;

#define SYNC_NONE 0
#define SYNC_VIDEO 1
#define SYNC_AUDIO 2

typedef struct {
  playmode_t mode;
  int slow;
  int fast;
  double speed;
  clocktime_t vt_offset;
  clocktime_t rt_start;
  int sync_master;
} ctrl_data_t;

typedef struct {
  int shmid;
  int size;
} shm_bufinfo_t;

#endif /* COMMON_H */
