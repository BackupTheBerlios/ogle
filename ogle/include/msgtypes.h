#ifndef MSGTYPES_H
#define MSGTYPE_H

#include <limits.h>
#include <inttypes.h>

typedef enum {
  MTYPE_CTL = 1,
  MTYPE_DEMUX = 2,
  MTYPE_VIDEO_DECODE_MPEG = 3,
  MTYPE_VIDEO_OUT = 4,
  MTYPE_AUDIO_DECODE_MPEG = 5,
  MTYPE_AUDIO_DECODE_AC3 = 6,
  MTYPE_AUDIO_OUT = 7,
  MTYPE_DECODE_MPEG_PRIVATE_STREAM_2 = 8,
  MTYPE_CTRL_INPUT = 9,
  MTYPE_SPU_DECODE = 10,
  MTYPE_DVD_NAV = 11
} mq_mtype_t;


typedef enum { 
  CMD_NONE = 0,
  CMD_ALL = 1,
  CMD_FILE_OPEN = 2,
  CMD_DEMUX_INFO = 3,
  CMD_DEMUX_REQ_BUFFER = 4,
  CMD_DEMUX_GNT_BUFFER = 5,
  CMD_DEMUX_NEW_STREAM = 6,
  CMD_DEMUX_STREAM_BUFFER = 7,
  CMD_DECODE_STREAM_BUFFER = 8,
  CMD_CTRL_DATA = 9,
  CMD_CTRL_CMD = 10,
  CMD_DECODE_NEW_OUTPUT = 11,
  CMD_DECODE_OUTPUT_BUFFER = 12,
  CMD_OUTPUT_BUFFER = 13,
  CMD_SPU_SET_PALETTE = 14,
  CMD_SPU_SET_HIGHLIGHT = 15,
  CMD_DVDCTRL_CMD = 16
} mq_cmdtype_t;

typedef enum {
  DVDCTRL_CMD_UP_BUTTON,
  DVDCTRL_CMD_DOWN_BUTTON,
  DVDCTRL_CMD_LEFT_BUTTON,
  DVDCTRL_CMD_RIGHT_BUTTON,
  DVDCTRL_CMD_ACTIVATE_BUTTON,
  DVDCTRL_CMD_SELECT_BUTTON_NR,
  DVDCTRL_CMD_SELECT_ACTIVATE_BUTTON_NR,
  DVDCTRL_CMD_CHECK_MOUSE_SELECT,
  DVDCTRL_CMD_CHECK_MOUSE_ACTIVATE
} mq_dvdctrl_cmd_t;

typedef struct {
  mq_dvdctrl_cmd_t cmd;
  unsigned int mouse_x;
  unsigned int mouse_y;
  int button_nr;
} mq_cmd_dvdctrl_cmd_t;

typedef enum {
  CTRLCMD_NONE = 0,
  CTRLCMD_STOP = 1,
  CTRLCMD_PLAY = 2,
  CTRLCMD_PLAY_FROM = 3,
  CTRLCMD_PLAY_TO = 4,
  CTRLCMD_PLAY_FROM_TO = 5
} mq_ctrlcmd_t;

typedef enum {
  OUTPUT_NONE = 0,
  OUTPUT_VIDEO_YUV = 1,
  OUTPUT_SPU = 2
} mq_output_t;

typedef struct {
  int x_start;
  int y_start;
  int x_end;
  int y_end;
  uint8_t color[4];
  uint8_t contrast[4];
} mq_cmd_spu_highlight_t;

typedef struct {
  uint32_t colors[16];
} mq_cmd_spu_palette_t;

typedef struct {
  char file[PATH_MAX+1];
} mq_cmd_file_open_t;

typedef struct {
  int size;
} mq_cmd_demux_info_t;

typedef struct {
  int size;
} mq_cmd_req_buffer_t;

typedef struct {
  int shmid;
  int size;
} mq_cmd_gnt_buffer_t;

typedef struct {
  uint8_t stream_id;  // 0xe0-ef mpeg_video, 0xc0-df mpeg_audio, ...
  int subtype;        // in case of private stream...
  int data_buf_shmid; // shmid of databuffer corresponding to offsets
  int nr_of_elems;    // nr of elements in q
} mq_cmd_new_stream_t;

typedef struct {
  mq_output_t type;  
  int data_buf_size;
  int nr_of_elems;    // nr of elements in q
} mq_cmd_new_output_t;

typedef struct {
  uint8_t stream_id;
  uint8_t subtype;
  int q_shmid;
} mq_cmd_stream_buffer_t;

typedef struct {
  int data_shmid;
  int data_size;
  int q_shmid;
} mq_cmd_output_buffer_t;


typedef struct {
  int shmid;
} mq_cmd_ctrl_data_t;



typedef struct {
  mq_ctrlcmd_t ctrlcmd;
  int off_from;
  int off_to;
} mq_cmd_ctrl_cmd_t;

typedef struct {
  mq_cmdtype_t cmdtype;
  union {
    mq_cmd_file_open_t file_open;
    mq_cmd_demux_info_t demux_info;
    mq_cmd_req_buffer_t req_buffer;
    mq_cmd_gnt_buffer_t gnt_buffer;
    mq_cmd_new_stream_t new_stream;
    mq_cmd_stream_buffer_t stream_buffer;
    mq_cmd_ctrl_data_t ctrl_data;
    mq_cmd_ctrl_cmd_t ctrl_cmd;
    mq_cmd_new_output_t new_output;
    mq_cmd_output_buffer_t output_buffer;
    mq_cmd_spu_palette_t spu_palette;
    mq_cmd_spu_highlight_t spu_highlight;
    mq_cmd_dvdctrl_cmd_t dvdctrl_cmd;
  } cmd;
} mq_cmd_t;


typedef struct {
  long mtype;
  char mtext[sizeof(mq_cmd_t)];
} mq_msg_t;

typedef struct {
  int shmid;
  int size;
} shm_bufinfo_t;

#endif
