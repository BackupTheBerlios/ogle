#include <inttypes.h>
#include <semaphore.h>
/* Packettypes. */

enum {
      PACK_TYPE_OFF_LEN, 
      PACK_TYPE_LOAD_FILE
     };

/* "There's len bytes of data for you in your file at this offset." */
struct off_len_packet {
//	               uint32_t type;
                       uint32_t offset;
                       uint32_t length;
                      };

/* "mmap this file now; off_len_packets from now on will be relative
 * this file." */
struct load_file_packet {
//                        uint32_t type;
                         uint32_t length;
                         char     filename[200];   // HACK ALERT!
                        };


/* Imgaretype. */

typedef struct {
  uint8_t *y; //[480][720];  //y-component image
  uint8_t *u; //[480/2][720/2]; //u-component
  uint8_t *v; //[480/2][720/2]; //v-component
  uint16_t start_x, start_y;
  uint16_t horizontal_size;
  uint16_t vertical_size;
  uint16_t padded_width, padded_height;
} yuv_image_t;

typedef struct {
  yuv_image_t picture;
  int in_use;
  int displayed;
  struct timespec pts_time;
  struct timespec realtime_offset;
  uint64_t PTS;
  int scr_nr;
} picture_info_t;

typedef struct {
  sem_t pictures_ready_to_display;
  sem_t pictures_displayed;
  int nr_of_buffers;
  picture_info_t *picture_infos;
  int *dpy_q;
  long int frame_interval;
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
  struct timespec realtime_offset;
  offset_valid_t offset_valid;
} ctrl_time_t;

typedef struct {
  playmode_t mode;
} ctrl_data_t;

