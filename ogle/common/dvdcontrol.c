#include <unistd.h>
#include <stdio.h>
#include <sys/msg.h>

#include "msgevents.h"
#include "dvdcontrol.h"


//TODO alloc handles for each pid?
static MsgEventQ_t *msgq;
static MsgEventClient_t nav_client;

static MsgEventClient_t get_nav_client(MsgEventQ_t *msq)
{
  MsgEvent_t ev;
  ev.type = MsgEventQReqCapability;
  ev.reqcapability.capability = DECODE_DVD_NAV;
  
  if(MsgSendEvent(msq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
    fprintf(stderr, "dvdcontrol: get_nav_client\n");
    return 0;
  }

  while(ev.type != MsgEventQGntCapability) {
    MsgNextEvent(msq, &ev);
  }
  
  return ev.gntcapability.capclient;
}

DVDResult_t DVDOpen(int msgqid) {
  
  if(msgq != NULL) {
    fprintf(stderr, "dvdcontrol: allready open\n");
    return DVD_E_Unspecified;
  }
  
  if((msgq = MsgOpen(msgqid)) == NULL) {
    return DVD_E_Unspecified;
  }

  nav_client = get_nav_client(msgq);
  
  if(nav_client == CLIENT_NONE) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/*** info commands ***/

DVDResult_t DVDGetAllGPRMs(const DVDGPRMArray_t *Registers)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetAllSPRMs(const DVDSPRMArray_t *Registers)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetCurrentUOPS(const DVDUOP_t *uop)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetAudioAttributes(DVDAudioStream_t StreamNr,
				  const DVDAudioAttributes_t *Attr)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetAudioLanguage(DVDAudioStream_t StreamNr,
				const DVDLangID_t *Language)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetCurrentAudio(const int *StreamsAvailable,
			       const DVDAudioStream_t *CurrentStream)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDIsAudioStreamEnabled(DVDAudioStream_t StreamNr,
				    const DVDBool_t *Enabled)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetDefaultAudioLanguage(const DVDLangID_t *Language,
				       const DVDAudioLangExt_t *AudioExtension)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetCurrentAngle(const int *AnglesAvailable,
			       const DVDAngle_t *CurrentAngle)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDGetCurrentVideoAttributes(const DVDVideoAttributes_t *Attr)
{
  return DVD_E_NotImplemented;
}


/*** end info commands ***/



/*** control commands ***/

DVDResult_t DVDLeftButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlLeftButtonSelect;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDRightButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlRightButtonSelect;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDUpperButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlUpperButtonSelect;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDLowerButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlLowerButtonSelect;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDButtonActivate(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlButtonActivate;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDButtonSelect(int Button)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlButtonSelect;
  ev.dvdctrl.cmd.buttonselect.nr = Button;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDButtonSelectAndActivate(int Button)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlButtonSelectAndActivate;
  ev.dvdctrl.cmd.buttonselectandactivate.nr = Button;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDMouseSelect(int x, int y)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlMouseSelect;
  ev.dvdctrl.cmd.mouseselect.x = x;
  ev.dvdctrl.cmd.mouseselect.y = y;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDMouseActivate(int x, int y)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlMouseActivate;
  ev.dvdctrl.cmd.mouseactivate.x = x;
  ev.dvdctrl.cmd.mouseactivate.y = y;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDMenuCall(DVDMenuID_t MenuId)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlMenuCall;
  ev.dvdctrl.cmd.menucall.menuid = MenuId;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDResume(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlResume;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDGoUp(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGoUp;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDForwardScan(double Speed)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlForwardScan;
  ev.dvdctrl.cmd.forwardscan.speed = Speed;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDBackwardScan(double Speed)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlBackwardScan;
  ev.dvdctrl.cmd.backwardscan.speed = Speed;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDNextPGSearch(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlNextPGSearch;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDPrevPGSearch(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPrevPGSearch;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDTopPGSearch(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTopPGSearch;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDPTTSearch(DVDPTT_t PTT)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPTTSearch;
  ev.dvdctrl.cmd.pttsearch.ptt = PTT;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDPTTPlay(DVDTitle_t Title, DVDPTT_t PTT)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPTTPlay;
  ev.dvdctrl.cmd.pttsearch.title = Title;
  ev.dvdctrl.cmd.pttsearch.ptt = PTT;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDTitlePlay(DVDTitle_t Title)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTitlePlay;
  ev.dvdctrl.cmd.titleplay.title = Title;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDTimeSearch(DVDTimecode_t time)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTimeSearch;
  ev.dvdctrl.cmd.timesearch.time = time;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDTimePlay(DVDTitle_t Title, DVDTimecode_t time)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTimeSearch;
  ev.dvdctrl.cmd.timesearch.title = Title;
  ev.dvdctrl.cmd.timesearch.time = time;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDPauseOn(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPauseOn;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDPauseOff(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPauseOff;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}


DVDResult_t DVDStop(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlStop;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}



DVDResult_t DVDDefaultMenuLanguageSelect(DVDLangID_t Lang)
{
  return DVD_E_NotImplemented;
}


DVDResult_t DVDAudioStreamChange(DVDAudioStream_t StreamNr)
{
  return DVD_E_NotImplemented;
}

/*** end control commands ***/


const static char DVD_E_Ok_STR[] = "OK";
const static char DVD_E_Unspecified_STR[] = "Unspecified";
const static char DVD_E_NotImplemented_STR[] = "Not Implemented";
const static char DVD_E_NoSuchError_STR[] = "No such error code";

void DVDPerror(const char *str, DVDResult_t ErrCode)
{
  const char *errstr;

  switch(ErrCode) {
  case DVD_E_Ok:
    errstr = DVD_E_Ok_STR;
    break;
  case DVD_E_Unspecified:
    errstr = DVD_E_Unspecified_STR;
    break;
  case DVD_E_NotImplemented:
    errstr = DVD_E_NotImplemented_STR;
    break;
  default:
    errstr = DVD_E_NoSuchError_STR;
    break;
  }

  fprintf(stderr, "%s%s %s\n",
	  (str == NULL ? "" : str),
	  (str == NULL ? "" : ":"),
	  errstr);

}
