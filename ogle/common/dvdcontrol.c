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


DVDResult_t DVDLeftButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonLeft;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDRightButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonRight;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDUpperButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonUp;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDLowerButtonSelect(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonDown;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDButtonActivate(void)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonActivate;

  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDButtonSelect(int Button)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonSelectNr;
  ev.userinput.button_nr = Button;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDButtonSelectAndActivate(int Button)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdButtonActivateNr;
  ev.userinput.button_nr = Button;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDMouseSelect(int x, int y)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdMouseMove;
  ev.userinput.mouse_x = x;
  ev.userinput.mouse_y = y;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDMouseActivate(int x, int y)
{
  MsgEvent_t ev;
  ev.type = MsgEventQUserInput;
  ev.userinput.cmd = InputCmdMouseActivate;
  ev.userinput.mouse_x = x;
  ev.userinput.mouse_y = y;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDMenuCall(DVDMenuID_t MenuId)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDMenuCall;
  ev.menucall.menuid = MenuId;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

DVDResult_t DVDResume(void)
{
  return DVD_E_NotImplemented;
}

DVDResult_t DVDGoUP(void)
{
  return DVD_E_NotImplemented;
}

DVDResult_t DVDDefaultMenuLanguageSelect(DVDLangID_t Lang)
{
  return DVD_E_NotImplemented;
}

DVDResult_t DVDAudioStreamChange(DVDAudioStream_t StreamNr)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDAudioStreamChange;
  ev.menucall.streamnr = StreamNr;
  
  MsgSendEvent(msgq, nav_client, &ev);
  
  return DVD_E_Ok;
}

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
