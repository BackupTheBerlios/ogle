#ifndef MSGTYPES_H
#define MSGTYPE_H

#include <limits.h>

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
} mtype_t;


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
  CMD_NAV_CMD = 16
} cmdtype_t;

typedef enum {
  NAV_CMD_UP_BUTTON,
  NAV_CMD_DOWN_BUTTON,
  NAV_CMD_LEFT_BUTTON,
  NAV_CMD_RIGHT_BUTTON,
  NAV_CMD_ACTIVATE_BUTTON,
  NAV_CMD_SELECT_BUTTON_NR,
  NAV_CMD_SELECT_ACTIVATE_BUTTON_NR
} nav_cmd_t;

typedef struct {
  nav_cmd_t cmd;
  int button_nr;
} cmd_nav_cmd_t;

typedef enum {
  CTRLCMD_NONE = 0,
  CTRLCMD_STOP = 1,
  CTRLCMD_PLAY = 2,
  CTRLCMD_PLAY_FROM = 3,
  CTRLCMD_PLAY_TO = 4,
  CTRLCMD_PLAY_FROM_TO = 5
} ctrlcmd_t;

typedef enum {
  OUTPUT_NONE = 0,
  OUTPUT_VIDEO_YUV = 1,
  OUTPUT_SPU = 2
} output_t;

typedef struct {
  int x_start;
  int y_start;
  int x_end;
  int y_end;
  uint8_t color[4];
  uint8_t contrast[4];
} cmd_spu_highlight_t;

typedef struct {
  uint32_t colors[16];
} cmd_spu_palette_t;

typedef struct {
  char file[PATH_MAX+1];
} cmd_file_open_t;

typedef struct {
  int size;
} cmd_demux_info_t;

typedef struct {
  int size;
} cmd_req_buffer_t;

typedef struct {
  int shmid;
  int size;
} cmd_gnt_buffer_t;

typedef struct {
  uint8_t stream_id;  // 0xe0-ef mpeg_video, 0xc0-df mpeg_audio, ...
  int subtype;        // in case of private stream...
  int data_buf_shmid; // shmid of databuffer corresponding to offsets
  int nr_of_elems;    // nr of elements in q
} cmd_new_stream_t;

typedef struct {
  output_t type;  
  int data_buf_size;
  int nr_of_elems;    // nr of elements in q
} cmd_new_output_t;

typedef struct {
  uint8_t stream_id;
  uint8_t subtype;
  int q_shmid;
} cmd_stream_buffer_t;

typedef struct {
  int data_shmid;
  int data_size;
  int q_shmid;
} cmd_output_buffer_t;


typedef struct {
  int shmid;
} cmd_ctrl_data_t;



typedef struct {
  ctrlcmd_t ctrlcmd;
  int off_from;
  int off_to;
} cmd_ctrl_cmd_t;

typedef struct {
  cmdtype_t cmdtype;
  union {
    cmd_file_open_t file_open;
    cmd_demux_info_t demux_info;
    cmd_req_buffer_t req_buffer;
    cmd_gnt_buffer_t gnt_buffer;
    cmd_new_stream_t new_stream;
    cmd_stream_buffer_t stream_buffer;
    cmd_ctrl_data_t ctrl_data;
    cmd_ctrl_cmd_t ctrl_cmd;
    cmd_new_output_t new_output;
    cmd_output_buffer_t output_buffer;
    cmd_spu_palette_t spu_palette;
    cmd_spu_highlight_t spu_highlight;
    cmd_nav_cmd_t nav_cmd;
  } cmd;
} cmd_t;


typedef struct {
  long mtype;
  char mtext[sizeof(cmd_t)];
} msg_t;

typedef struct {
  int shmid;
  int size;
} shm_bufinfo_t;

#endif
