#ifndef MSGEVENTS_H
#define MSGEVENTS_H

#include <limits.h>
#include <inttypes.h>

#if 0 /* One message queue for every listener */

typedef struct {
  int msqid;
  long int mtype;
} MsgEventClient_t;

typedef int MsgEventQ_t;

#else /* One global message queue */

typedef long int MsgEventClient_t;

typedef struct {
  int msqid;
  long int mtype;
} MsgEventQ_t;


#endif

typedef enum {
  PlayCtrlCmdPlay,
  PlayCtrlCmdStop,
  PlayCtrlCmdPlayFrom,
  PlayCtrlCmdPlayTo,
  PlayCtrlCmdPlayFromTo,
  PlayCtrlCmdPause
} MsgQPlayCtrlCmd_t;

typedef enum {
  InputCmdButtonUp,
  InputCmdButtonDown,
  InputCmdButtonLeft,
  InputCmdButtonRight,
  InputCmdButtonActivate,
  InputCmdButtonActivateNr,
  InputCmdButtonSelectNr,
  InputCmdMouseActivate,
  InputCmdMouseMove //???
} MsgQInputCmd_t;

typedef enum {
  MsgEventQNone = 0,
  MsgEventQInitReq = 2,
  MsgEventQInitGnt,  
  MsgEventQRegister,
  MsgEventQNotify,
  MsgEventQReqCapability,
  MsgEventQGntCapability,
  MsgEventQPlayCtrl,
  MsgEventQChangeFile,
  MsgEventQReqStreamBuf,
  MsgEventQGntStreamBuf,
  MsgEventQDecodeStreamBuf,
  MsgEventQReqBuf,
  MsgEventQGntBuf,
  MsgEventQCtrlData,
  MsgEventQReqPicBuf,
  MsgEventQGntPicBuf,
  MsgEventQAttachQ,
  MsgEventQSPUPalette,
  MsgEventQSPUHighlight,
  MsgEventQUserInput,
  MsgEventQSpeed,
  MsgEventQDVDMenuCall
} MsgEventType_t;


/* DVD control */
typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  DVDMenuID_t menuid;
} MsgQDVDMenuCallEvent_t;

/* end DVD control */

/* DVD info */
/* end DVD info */

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  double speed;
} MsgQSpeedEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  MsgQInputCmd_t cmd;
  int button_nr;
  int mouse_x;
  int mouse_y;
} MsgQUserInputEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int x_start;
  int y_start;
  int x_end;
  int y_end;
  uint8_t color[4];
  uint8_t contrast[4];
} MsgQSPUHighlightEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint32_t colors[16];
} MsgQSPUPaletteEvent_t;


typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int shmid;
} MsgQCtrlDataEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int size;
} MsgQReqBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int shmid;
  int size;
} MsgQGntBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int data_buf_shmid;
  int nr_of_elems;
} MsgQReqPicBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int q_shmid;
} MsgQGntPicBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint8_t stream_id;
  int subtype;
  int data_buf_shmid;
  int nr_of_elems;
} MsgQReqStreamBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint8_t stream_id;
  int subtype;
  int q_shmid;
} MsgQGntStreamBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint8_t stream_id;
  int subtype;
  int q_shmid;
} MsgQDecodeStreamBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int q_shmid;
} MsgQAttachQEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  MsgQPlayCtrlCmd_t cmd;
  int from;
  int to;
  int extra_cmd;
} MsgQPlayCtrlEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  char filename[PATH_MAX+1];
} MsgQChangefileEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
} MsgQInitReqEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client; 
  MsgEventClient_t newclientid; //new client id to use
} MsgQInitGntEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int capabilities;
} MsgQRegisterCapsEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int capability;  
} MsgQReqCapabilityEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int capability;  
  MsgEventClient_t capclient;
} MsgQGntCapabilityEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int qid;             /* id of the q this notification belongs to */
  int action;           /* DataReleased, DataAvailable, ... ?*/
} MsgQNotifyEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
} MsgQAnyEvent_t;

typedef union {
  MsgEventType_t type;
  MsgQAnyEvent_t any;
  MsgQInitReqEvent_t initreq;
  MsgQInitGntEvent_t initgnt;
  MsgQRegisterCapsEvent_t registercaps;
  MsgQNotifyEvent_t notify;
  MsgQReqCapabilityEvent_t reqcapability;
  MsgQGntCapabilityEvent_t gntcapability;
  MsgQPlayCtrlEvent_t playctrl;
  MsgQChangefileEvent_t changefile;
  MsgQReqStreamBufEvent_t reqstreambuf;
  MsgQGntStreamBufEvent_t gntstreambuf;
  MsgQDecodeStreamBufEvent_t decodestreambuf;
  MsgQReqBufEvent_t reqbuf;
  MsgQGntBufEvent_t gntbuf;
  MsgQCtrlDataEvent_t ctrldata;
  MsgQReqPicBufEvent_t reqpicbuf;
  MsgQGntPicBufEvent_t gntpicbuf;
  MsgQAttachQEvent_t attachq;
  MsgQSPUPaletteEvent_t spupalette;
  MsgQSPUHighlightEvent_t spuhighlight;
  MsgQUserInputEvent_t userinput;
  MsgQSpeedEvent_t speed;
  MsgQDVDMenuCallEvent_t menucall;
} MsgEvent_t;

typedef struct {
  long int mtype;
  char event_data[sizeof(MsgEvent_t)];
} msg_t;

MsgEventQ_t *MsgOpen(int msqid);

void MsgClose(MsgEventQ_t *q);

int MsgNextEvent(MsgEventQ_t *q, MsgEvent_t *event_return);

int MsgCheckEvent(MsgEventQ_t *q, MsgEvent_t *event_return);

int MsgSendEvent(MsgEventQ_t *q, MsgEventClient_t client,
		 MsgEvent_t *event_send);

/* client types */
#define CLIENT_NONE               0x00000000L
#define CLIENT_RESOURCE_MANAGER   0x00000001L
#define CLIENT_UNINITIALIZED      0x00000002L

/* capabilities */
#define DEMUX_MPEG1        0x0001
#define DEMUX_MPEG2_PS     0x0002
#define DEMUX_MPEG2_TS     0x0004
#define DECODE_MPEG1_VIDEO 0x0008
#define DECODE_MPEG2_VIDEO 0x0010
#define DECODE_MPEG1_AUDIO 0x0020
#define DECODE_MPEG2_AUDIO 0x0040
#define DECODE_AC3_AUDIO   0x0080
#define DECODE_DTS_AUDIO   0x0100
#define DECODE_SDDS_AUDIO  0x0200
#define DECODE_DVD_SPU     0x0400
#define DECODE_DVD_NAV     0x0800
#define UI_MPEG_CLI        0x1000
#define UI_MPEG_GUI        0x2000
#define UI_DVD_CLI         0x4000
#define UI_DVD_GUI         0x8000
#define VIDEO_OUTPUT       0x10000

#endif /* MSGEVENTS_H */
