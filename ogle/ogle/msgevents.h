#ifndef MSGEVENTS_H_INCLUDED
#define MSGEVENTS_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2005 Björn Englund, Håkan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if 1
#define SOCKIPC
#else
#define HAVE_SYSV_MSG
#endif

#include <limits.h>
#include <inttypes.h>

#include "dvd.h"
#include "dvdevents.h"

#include <sys/param.h>

#ifdef SOCKIPC
#include <sys/socket.h>
#include <sys/un.h>
#endif


#if 0 /* One message queue for every listener */

typedef struct {
  int msqid;
  long int mtype;
} MsgEventClient_t;

typedef int MsgEventQ_t;

#else /* One global message queue */

#ifndef SOCKIPC
typedef long int MsgEventClient_t;

typedef struct {
  int msqid;
  long int mtype;
} MsgEventQ_t;

#else

/* more general version */

typedef long int MsgEventClient_t;
#if 0
typedef union {
  struct {
    long int mtype;
  } msgq;
  
  struct {
    long int type;
    int d;
  } socket;

  struct {
    long int type;
    int d;
  } pipe;
} MsgEventClient_t;
#endif

typedef enum {
#ifdef HAVE_SYSV_MSG
  MsgEventQType_msgq,
#endif
  MsgEventQType_socket,
  MsgEventQType_pipe
} MsgEventQType_t;

typedef union {
 
  MsgEventQType_t type;
#ifdef HAVE_SYSV_MSG  
  struct {
    MsgEventQType_t type;
    int msqid;
    long int mtype;
  } msgq;
#endif  
  struct {
    MsgEventQType_t type;
    long int clientid;
    struct sockaddr_un server_addr;
    int sd; /* local socket */
    struct sockaddr_un client_addr;
  } socket;
  
  struct {
    MsgEventQType_t type;
    int d;
  } pipe;
  
} MsgEventQ_t;

#endif

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
  MsgEventQNone = 0,
  MsgEventQInitReq = 2,
  MsgEventQInitGnt,  
  MsgEventQRegister,
  MsgEventQNotify,
  MsgEventQReqCapability,
  MsgEventQGntCapability,
  MsgEventQPlayCtrl,
  MsgEventQChangeFile,
  MsgEventQReqStreamBuf,       /* 10 */
  MsgEventQGntStreamBuf,
  MsgEventQDecodeStreamBuf,
  MsgEventQReqBuf,
  MsgEventQGntBuf,
  MsgEventQCtrlData,
  MsgEventQReqPicQ,
  MsgEventQGntPicQ,
  MsgEventQAttachQ,
  MsgEventQSPUPalette,
  MsgEventQSPUHighlight,       /* 20 */
  MsgEventQSpeed,
  MsgEventQDVDCtrl,
  MsgEventQFlow,
  MsgEventQFlushData,
  MsgEventQDemuxStream,
  MsgEventQDemuxStreamChange,
  MsgEventQDemuxDefault,
  MsgEventQDVDCtrlLong,
  MsgEventQDemuxDVD,
  MsgEventQDemuxDVDRoot,       /* 30 */
  MsgEventQSetAspectModeSrc,
  MsgEventQSetSrcAspect,
  MsgEventQSetZoomMode,
  MsgEventQReqInput,
  MsgEventQInputButtonPress,
  MsgEventQInputButtonRelease,
  MsgEventQInputPointerMotion,
  MsgEventQInputKeyPress,
  MsgEventQInputKeyRelease,
  MsgEventQDestroyBuf,         /* 40 */
  MsgEventQAppendQ,
  MsgEventQDetachQ,
  MsgEventQQDetached,
  MsgEventQDestroyQ,
  MsgEventQDemuxStreamChange2,
  MsgEventQSaveScreenshot,
  MsgEventQSPUState,
  MsgEventQReqPicBuf,
  MsgEventQGntPicBuf,
  MsgEventQDestroyPicBuf,      /* 50 */
  MsgEventQDemuxStreamEnable,
  MsgEventQStop
} MsgEventType_t;




typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  InputMask_t mask;
} MsgQReqInputEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int x;
  int y;
  int x_root;
  int y_root;
  unsigned long mod_mask; /* X modifiers (keys/buttons)*/
  unsigned long input;
  unsigned long time; /* milliseconds */
  unsigned long input_base;
  unsigned int input_keycode;
} MsgQInputEvent_t;



typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  ZoomMode_t mode;
  uint16_t zoom_frac_n;
  uint16_t zoom_frac_d;  
} MsgQSetZoomModeEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  AspectModeSrc_t mode_src;
} MsgQSetAspectModeSrcEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  AspectModeSrc_t mode_src;
  uint16_t aspect_frac_n;
  uint16_t aspect_frac_d;  
} MsgQSetSrcAspectEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  DVDCtrlEvent_t cmd;
} MsgQDVDCtrlEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  DVDCtrlLongEvent_t cmd;
} MsgQDVDCtrlLongEvent_t;


typedef enum {
  QCmdFlushTo
} QCmd_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  QCmd_t cmd;
  int reference;
} MsgQFlowEvent_t;

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
  uint32_t state;
} MsgQSPUStateEvent_t;


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
} MsgQDestroyBufEvent_t;

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
  int size;
} MsgQReqPicBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int shmid;
} MsgQDestroyPicBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int shmid;
  int size;
} MsgQGntPicBufEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int data_buf_shmid;
  int nr_of_elems;
} MsgQReqPicQEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int q_shmid;
} MsgQGntPicQEvent_t;

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
  uint8_t stream_id;
  int subtype;
} MsgQDemuxStreamChangeEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint8_t old_stream_id;
  int old_subtype;
  uint8_t new_stream_id;
  int new_subtype;
} MsgQDemuxStreamChange2Event_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint8_t stream_id;
  int subtype;
  int state; /* 0 - diabled, 1 - enabled */
} MsgQDemuxStreamEnableEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint8_t stream_id;
  int subtype;
} MsgQDemuxStreamEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int state; /* 0 - not registered, 1 - discard */
} MsgQDemuxDefaultEvent_t;

typedef enum {
  FlowCtrlNone = 0,
  FlowCtrlCompleteVideoUnit = 1,
  FlowCtrlFlush = 2 
} FlowCtrl_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int titlenum;        /* titlenum as described in libdvdread */
  int domain;          /* dvd_domain_t as in libdvdread */
  int block_offset;    /* blocks of 2048 bytes from start of title/file */
  int block_count;     /* nr of blocks to demux */
  FlowCtrl_t flowcmd;
} MsgQDemuxDVDEvent_t;

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
  int q_shmid;
} MsgQDetachQEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  MsgQPlayCtrlCmd_t cmd;
  int from;
  int to;
  FlowCtrl_t flowcmd;
} MsgQPlayCtrlEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  int to_scrid;
} MsgQFlushDataEvent_t;

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
  char path[PATH_MAX+1];
} MsgQDemuxDVDRootEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
} MsgQInitReqEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client; 
  MsgEventClient_t newclientid; /* new client id to use */
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
  ScreenshotMode_t mode;
  char formatstr[PATH_MAX+1];
} MsgQSaveScreenshotEvent_t;

typedef struct {
  MsgEventType_t type;
  MsgEventQ_t *q;
  MsgEventClient_t client;
  uint32_t state;
} MsgQStopEvent_t;

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
  MsgQDestroyBufEvent_t destroybuf;
  MsgQReqPicBufEvent_t reqpicbuf;
  MsgQGntPicBufEvent_t gntpicbuf;
  MsgQDestroyPicBufEvent_t destroypicbuf;
  MsgQCtrlDataEvent_t ctrldata;
  MsgQReqPicQEvent_t reqpicq;
  MsgQGntPicQEvent_t gntpicq;
  MsgQAttachQEvent_t attachq;
  MsgQDetachQEvent_t detachq;
  MsgQSPUPaletteEvent_t spupalette;
  MsgQSPUHighlightEvent_t spuhighlight;
  MsgQSpeedEvent_t speed;
  MsgQFlowEvent_t flow;
  MsgQDVDCtrlEvent_t dvdctrl;
  MsgQFlushDataEvent_t flushdata;
  MsgQDemuxStreamEvent_t demuxstream;
  MsgQDemuxStreamChangeEvent_t demuxstreamchange;
  MsgQDemuxStreamChange2Event_t demuxstreamchange2;
  MsgQDemuxStreamEnableEvent_t demuxstreamenable;
  MsgQDemuxDefaultEvent_t demuxdefault;
  MsgQDVDCtrlLongEvent_t dvdctrllong;
  MsgQDemuxDVDEvent_t demuxdvd;
  MsgQDemuxDVDRootEvent_t demuxdvdroot;
  MsgQSetAspectModeSrcEvent_t setaspectmodesrc;
  MsgQSetSrcAspectEvent_t setsrcaspect;
  MsgQSetZoomModeEvent_t setzoommode;
  MsgQReqInputEvent_t reqinput;
  MsgQInputEvent_t input;
  MsgQSaveScreenshotEvent_t savescreenshot;
  MsgQSPUStateEvent_t spustate;
  MsgQStopEvent_t stop;
} MsgEvent_t;

#ifdef SOCKIPC
typedef union {
#ifdef HAVE_SYSV_MSG
  struct {
    long int mtype;
    char event_data[sizeof(MsgEvent_t)];
  } msgq;
#endif  
  struct{
    char event_data[sizeof(MsgEvent_t)];
  } socket;

  struct {
    char event_data[sizeof(MsgEvent_t)];
  } pipe;
} msg_t;
#else
typedef struct {
  long int mtype;
  char event_data[sizeof(MsgEvent_t)];
} msg_t;
#endif

#ifdef SOCKIPC
MsgEventQ_t *MsgOpen(MsgEventQType_t type, char *name, int namelen);
#else
MsgEventQ_t *MsgOpen(int msqid);
#endif

void MsgClose(MsgEventQ_t *q);

int MsgNextEvent(MsgEventQ_t *q, MsgEvent_t *event_return);
int MsgNextEventInterruptible(MsgEventQ_t *q, MsgEvent_t *event_return);

#ifndef SOCKIPC
#if (defined(BSD) && (BSD >= 199306))
int MsgNextEventNonBlocking(MsgEventQ_t *q, MsgEvent_t *event_return);
#endif
#endif

int MsgCheckEvent(MsgEventQ_t *q, MsgEvent_t *event_return);

int MsgSendEvent(MsgEventQ_t *q, MsgEventClient_t client,
		 MsgEvent_t *event_send, int msgflg);

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
#define USER_INPUT         0x20000
#define DECODE_LPCM_AUDIO  0x40000

#ifdef SOCKIPC
int get_msgqtype(char *msgq_str, MsgEventQType_t *type, char **id);
#endif
#endif /* MSGEVENTS_H_INCLUDED */
