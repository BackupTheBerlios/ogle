#ifndef DVDEVENTS_H
#define DVDEVENTS_H

#include "dvd.h"

typedef enum {
  DVDCtrlButtonMask = 0x01000000,
  DVDCtrlMouseMask  = 0x02000000
} DVDCtrlEventMask_t;

typedef enum {
  DVDCtrlLeftButtonSelect        = 0x01000001,
  DVDCtrlRightButtonSelect       = 0x01000002,
  DVDCtrlUpperButtonSelect       = 0x01000003,
  DVDCtrlLowerButtonSelect       = 0x01000004,

  DVDCtrlButtonActivate          = 0x01000005,
  DVDCtrlButtonSelect            = 0x01000006,
  DVDCtrlButtonSelectAndActivate = 0x01000007,

  DVDCtrlMouseSelect             = 0x02000001,
  DVDCtrlMouseActivate           = 0x02000002,

  DVDCtrlMenuCall = 1,
  DVDCtrlResume,
  DVDCtrlGoUp,

  DVDCtrlForwardScan,
  DVDCtrlBackwardScan,

  DVDCtrlNextPGSearch,
  DVDCtrlPrevPGSearch,
  DVDCtrlTopPGSearch,

  DVDCtrlPTTSearch,
  DVDCtrlPTTPlay,

  DVDCtrlTitlePlay,

  DVDCtrlTimeSearch,
  DVDCtrlTimePlay,

  DVDCtrlPauseOn,
  DVDCtrlPauseOff,

  DVDCtrlStop,

  DVDCtrlEventAudioStreamChange
} DVDCtrlEventType_t;

/*
typedef struct {
} DVDCtrlLeftButtonSelectEvent_t;

typedef struct {
} DVDCtrlRightButtonSelectEvent_t;

typedef struct {
} DVDCtrlUpperButtonSelectEvent_t;

typedef struct {
} DVDCtrlLowerButtonSelectEvent_t;

typedef struct {
} DVDCtrlButtonActivateEvent_t;


typedef struct {
  int nr;
} DVDCtrlButtonSelectEvent_t;

typedef struct {
  int nr;
} DVDCtrlButtonSelectAndActivateEvent_t;
*/

typedef struct {
  DVDCtrlEventType_t type;
  int nr;
} DVDCtrlButtonEvent_t;

/*
typedef struct {
  int x;
  int y;
} DVDCtrlMouseSelectEvent_t;

typedef struct {
  int x;
  int y;
} DVDCtrlMouseActivateEvent_t;
*/

typedef struct {
  DVDCtrlEventType_t type;
  int x;
  int y;
} DVDCtrlMouseEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  DVDMenuID_t menuid;
} DVDCtrlMenuCallEvent_t;

/*
typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlResumeEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlGoUpEvent_t;
*/

/*
typedef struct {
  DVDCtrlEventType_t type;
  double speed;
} DVDCtrlForwardScanEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  double speed;
} DVDCtrlBackwardScanEvent_t;
*/

typedef struct {
  DVDCtrlEventType_t type;
  double speed;
} DVDCtrlScanEvent_t;

/*
typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlNextPGSearchEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlPrevPGSearchEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlTopPGSearchEvent_t;
*/

typedef struct {
  DVDCtrlEventType_t type;
  DVDPTT_t ptt;
} DVDCtrlPTTSearchEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  DVDTitle_t title;
  DVDPTT_t ptt;
} DVDCtrlPTTPlayEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  DVDTitle_t title;
} DVDCtrlTitlePlayEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  DVDTimecode_t time;
} DVDCtrlTimeSearchEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  DVDTitle_t title;
  DVDTimecode_t time;
} DVDCtrlTimePlayEvent_t;

/*
typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlPauseOnEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlPauseOffEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
} DVDCtrlStopEvent_t;
*/

typedef struct {
  DVDCtrlEventType_t type;
  DVDAudioStream_t streamnr;
} DVDCtrlAudioStreamChangeEvent_t;

typedef union {
  DVDCtrlEventType_t type;
  /*
  DVDCtrlLeftButtonSelectEvent_t leftbuttonselect;
  DVDCtrlLeftButtonSelectEvent_t rightbuttonselect;
  DVDCtrlLeftButtonSelectEvent_t upperbuttonselect;
  DVDCtrlLeftButtonSelectEvent_t lowerbuttonselect;
  DVDCtrlButtonActivateEvent_t buttonactivate;
  DVDCtrlButtonSelectEvent_t buttonselect;
  DVDCtrlButtonSelectAndActivateEvent_t buttonselectandactivate;
  */

  DVDCtrlButtonEvent_t button;

  /*
  DVDCtrlMouseSelectEvent_t mouseselect;
  DVDCtrlMouseActivateEvent_t mouseactivate;
  */

  DVDCtrlMouseEvent_t mouse;

  DVDCtrlMenuCallEvent_t menucall;

  /*
  DVDCtrlResumeEvent_t resume;
  DVDCtrlGoUpEvent_t goup;
  */

  /*
  DVDCtrlForwardScanEvent_t forwardscan;
  DVDCtrlBackwardScanEvent_t backwardscan;
  */

  DVDCtrlScanEvent_t scan;

  /*
  DVDCtrlNextPGSearchEvent_t nextpgsearch;
  DVDCtrlPrevPGSearchEvent_t prevpgsearch;
  DVDCtrlTopPGSearchEvent_t toppgsearch;
  */

  DVDCtrlPTTSearchEvent_t pttsearch;
  DVDCtrlPTTPlayEvent_t pttplay;
  DVDCtrlTitlePlayEvent_t titleplay;
  DVDCtrlTimeSearchEvent_t timesearch;
  DVDCtrlTimePlayEvent_t timeplay;

  /*
  DVDCtrlPauseOnEvent_t pauseon;
  DVDCtrlPauseOffEvent_t pauseoff;
  DVDCtrlStopEvent_t stop;
  */

  DVDCtrlAudioStreamChangeEvent_t audiostreamchange;
} DVDCtrlEvent_t;


#endif /* DVDEVENTS_H */
