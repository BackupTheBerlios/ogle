/** @file
 * DVD navigator interface.
 * This file contains the functions that form the interface to
 * the DVD navigator. These are the functions a user interface
 * should use to control DVD playback.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/msg.h>

#include <ogle/msgevents.h>
#include <ogle/dvdcontrol.h>


/**
 * Internally seen type for the handle to a DVD navigator
 */
struct DVDNav_s {
  MsgEventClient_t client;
  MsgEventQ_t *msgq;
};


/**
 * Get the id of the DVD navigator client.
 */
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


/** @defgroup dvdnav DVD navigator functions
 * These functions are used for controlling playback of a DVD.
 *
 * The functions are divided into two groups,
 * the @link dvdinfo DVD information functions @endlink and the
 * the @link dvdctrl DVD control functions @endlink. 
 *
 * Before using any of these, a connection to the DVD navigator
 * must be created by using the DVDOpenNav() function.
 * 
 * When the connection is not needed anymore the
 * DVDCloseNav() function should be used.
 *
 * The return values from these functions may be used as
 * an argument to DVDPerror() to generate
 * human readable error messages.
 * @{
 */


/**
 * Get a connection to the DVD navigator
 * @todo something
 */
DVDResult_t DVDOpenNav(DVDNav_t **nav, int msgqid) {
  
  *nav = (DVDNav_t *)malloc(sizeof(DVDNav_t));
  if(*nav == NULL) {
    return DVD_E_NOMEM;
  }
  
  if(((*nav)->msgq = MsgOpen(msgqid)) == NULL) {
    free(*nav);
    return DVD_E_Unspecified;
  }

  (*nav)->client = get_nav_client((*nav)->msgq);
  
  if((*nav)->client == CLIENT_NONE) {
    free(*nav);
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}

/**
 * Close the connection to the DVD navigator.
 * @todo something
 */
DVDResult_t DVDCloseNav(DVDNav_t *nav) {
  
  if(nav->msgq == NULL) {
    fprintf(stderr, "dvdcontrol: already closed\n");
    return DVD_E_Unspecified;
  }
  
  MsgClose(nav->msgq);
  
  nav = NULL;
  
  return DVD_E_Ok;
}

const static char DVD_E_Ok_STR[] = "OK";
const static char DVD_E_Unspecified_STR[] = "Unspecified";
const static char DVD_E_NotImplemented_STR[] = "Not Implemented";
const static char DVD_E_NoSuchError_STR[] = "No such error code";

/**
 * Print DVD error messages
 */
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


/** @defgroup dvdinfo DVD information functions
 * These functions are used for getting information from
 * the DVD navigator.
 *
 * These functions send queries to the DVD navigator which
 * then returns the requested information about the
 * state of the DVD navigator or attributes of the DVD.
 * The @link dvdctrl DVD control functions @endlink provides the functions
 * for controlling the DVD navigator.
 * @{
 */

/**
 * Get the contents of all General Parameters
 * @todo implement function
 */
DVDResult_t DVDGetAllGPRMs(DVDNav_t *nav, const DVDGPRMArray_t *Registers)
{
  return DVD_E_NotImplemented;
}

/**
 * Get the contents of all System Parameters
 * @todo implement function
 */
DVDResult_t DVDGetAllSPRMs(DVDNav_t *nav, const DVDSPRMArray_t *Registers)
{
  return DVD_E_NotImplemented;
}

/**
 * Get the current allowed user operations
 * @todo implement function
 */
DVDResult_t DVDGetCurrentUOPS(DVDNav_t *nav, const DVDUOP_t *uop)
{
  return DVD_E_NotImplemented;
}

/**
 * Get the attributes of the specified audio stream.
 * @todo Implement function.
 * 
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specifies the audio stream which attributes
 * will be retrieved.
 * @param Attr Points to where the attributes of the specified
 * audio stream will be written.
 *
 * @return If successful DVD_E_Ok is returned and the attributes
 * pointed to by Attr have been updated. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented. 
 */
DVDResult_t DVDGetAudioAttributes(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				  const DVDAudioAttributes_t *Attr)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGetAudioAttributes;
  MsgSendEvent(nav->msgq, nav->client, &ev);
  while(1) {
    MsgNextEvent(nav->msgq, &ev);
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlAudioAttributes)) {
      if(ev.dvdctrl.cmd.audioattributes.streamnr == StreamNr) {
	memcpy((void *)Attr, (void *)&(ev.dvdctrl.cmd.audioattributes.attr),
	       sizeof(DVDAudioAttributes_t));
	return DVD_E_Ok;
      }
    }
  } 
}

/** 
 * Get the language of an audio stream.
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specifies the audio stream which laguage code
 * will be retrieved.
 * @param Language Points to where the language code of the
 * specified audio stream will be written.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetAudioLanguage(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				const DVDLangID_t *Language)
{
  return DVD_E_NotImplemented;
}

/** 
 * Get the number of available audio streams and the current
 * @todo handle other events returned.
 * @param nav Specifies the connection to the DVD navigator.
 */
DVDResult_t DVDGetCurrentAudio(DVDNav_t *nav, int *const StreamsAvailable,
			       DVDAudioStream_t *const CurrentStream)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGetCurrentAudio;
  MsgSendEvent(nav->msgq, nav->client, &ev);
  while(1) {
    MsgNextEvent(nav->msgq, &ev);
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlCurrentAudio)) {
      *StreamsAvailable = ev.dvdctrl.cmd.currentaudio.nrofstreams;
      *CurrentStream = ev.dvdctrl.cmd.currentaudio.currentstream;
      return DVD_E_Ok;
    }
  } 
}

/**
 * Check if an audio stream is enabled
 * @todo Handle other return events
 * @param nav Specifies the connection to the DVD navigator.
 */
DVDResult_t DVDIsAudioStreamEnabled(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				    DVDBool_t *const Enabled)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlIsAudioStreamEnabled;
  ev.dvdctrl.cmd.audiostreamenabled.streamnr = StreamNr;
  MsgSendEvent(nav->msgq, nav->client, &ev);
  while(1) {
    MsgNextEvent(nav->msgq, &ev);
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlAudioStreamEnabled)) {
      if(ev.dvdctrl.cmd.audiostreamenabled.streamnr == StreamNr) {
	*Enabled = ev.dvdctrl.cmd.audiostreamenabled.enabled;
	return DVD_E_Ok;
      }
    }
  } 
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetDefaultAudioLanguage(DVDNav_t *nav,
				       const DVDLangID_t *Language,
				       const DVDAudioLangExt_t *AudioExtension)
{
  return DVD_E_NotImplemented;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetCurrentAngle(DVDNav_t *nav, const int *AnglesAvailable,
			       const DVDAngle_t *CurrentAngle)
{
  return DVD_E_NotImplemented;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetCurrentVideoAttributes(DVDNav_t *nav,
					 const DVDVideoAttributes_t *Attr)
{
  return DVD_E_NotImplemented;
}

/** @} end of dvdinfo */



/** @defgroup dvdctrl DVD control functions
 * These functions are used for controlling the
 * DVD navigator.
 *
 * These functions send commands and state information
 * to the DVD navigator. The DVD navigator performs the requested
 * actions if possible and updates the state.
 * The @link dvdinfo DVD information functions @endlink provides
 * the functions for getting information from the DVD navigator.
 * @{
 */


/** 
 * Selects the button to the left of the current one.
 * @todo add mode return values
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDLeftButtonSelect(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlLeftButtonSelect;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/**
 * Selects the button to the right of the current one.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDRightButtonSelect(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlRightButtonSelect;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Selects the button above the current one.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDUpperButtonSelect(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlUpperButtonSelect;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Selects the button below the current one.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDLowerButtonSelect(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlLowerButtonSelect;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Activates the button currently selected.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDButtonActivate(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlButtonActivate;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Selects the specified button.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Button Specifies the button to select.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDButtonSelect(DVDNav_t *nav, int Button)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlButtonSelect;
  ev.dvdctrl.cmd.button.nr = Button;
  
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Selects and activates the specified button.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Button Specifies the button to select and activate.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDButtonSelectAndActivate(DVDNav_t *nav, int Button)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlButtonSelectAndActivate;
  ev.dvdctrl.cmd.button.nr = Button;
  
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Selects the button at the specified position.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param x Specifies the x coordinate.
 * @param y Specifies the y coordinate.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDMouseSelect(DVDNav_t *nav, int x, int y)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlMouseSelect;
  ev.dvdctrl.cmd.mouse.x = x;
  ev.dvdctrl.cmd.mouse.y = y;
  
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Selects and activates the button at the specified position.
 * @todo add mode return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param x Specifies the x coordinate.
 * @param y Specifies the y coordinate.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDMouseActivate(DVDNav_t *nav, int x, int y)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlMouseActivate;
  ev.dvdctrl.cmd.mouse.x = x;
  ev.dvdctrl.cmd.mouse.y = y;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Jumps to the specified menu.
 * @todo add more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param MenuId Specifies the menu to display.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDMenuCall(DVDNav_t *nav, DVDMenuID_t MenuId)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlMenuCall;
  ev.dvdctrl.cmd.menucall.menuid = MenuId;
  
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Resumes playback.
 * @todo add more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDResume(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlResume;
  
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Go up one level
 * @todo better description.
 * @todo add more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGoUp(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGoUp;
  
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Play forward at specified speed.
 * @todo better description.
 * @todo add more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Speed Specifies the speed of play.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDForwardScan(DVDNav_t *nav, double Speed)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlForwardScan;
  ev.dvdctrl.cmd.scan.speed = Speed;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * Play backward at specified speed.
 * @todo better description.
 * @todo add more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Speed Specifies the speed of play.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDBackwardScan(DVDNav_t *nav, double Speed)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlBackwardScan;
  ev.dvdctrl.cmd.scan.speed = Speed;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDNextPGSearch(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlNextPGSearch;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDPrevPGSearch(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPrevPGSearch;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDTopPGSearch(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTopPGSearch;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDPTTSearch(DVDNav_t *nav, DVDPTT_t PTT)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPTTSearch;
  ev.dvdctrl.cmd.pttsearch.ptt = PTT;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDPTTPlay(DVDNav_t *nav, DVDTitle_t Title, DVDPTT_t PTT)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPTTPlay;
  ev.dvdctrl.cmd.pttplay.title = Title;
  ev.dvdctrl.cmd.pttplay.ptt = PTT;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDTitlePlay(DVDNav_t *nav, DVDTitle_t Title)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTitlePlay;
  ev.dvdctrl.cmd.titleplay.title = Title;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDTimeSearch(DVDNav_t *nav, DVDTimecode_t time)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTimeSearch;
  ev.dvdctrl.cmd.timesearch.time = time;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDTimePlay(DVDNav_t *nav, DVDTitle_t Title, DVDTimecode_t time)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlTimeSearch;
  ev.dvdctrl.cmd.timeplay.title = Title;
  ev.dvdctrl.cmd.timeplay.time = time;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDPauseOn(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPauseOn;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDPauseOff(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlPauseOff;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDStop(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlStop;

  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}



/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDDefaultMenuLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang)
{
  return DVD_E_NotImplemented;
}


/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDAudioStreamChange(DVDNav_t *nav, DVDAudioStream_t StreamNr)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlAudioStreamChange;
  ev.dvdctrl.cmd.audiostreamchange.streamnr = StreamNr;
  MsgSendEvent(nav->msgq, nav->client, &ev);
  
  return DVD_E_Ok;
}

/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDDefaultAudioLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang)
{
  return DVD_E_NotImplemented;
}

/** 
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDKaraokeAudioPresentationMode(DVDNav_t *nav, DVDKaraokeDownmixMask_t DownmixMode)
{
  return DVD_E_NotImplemented;
}



/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDAngleChange(DVDNav_t *nav, DVDAngle_t AngleNr)
{
return DVD_E_NotImplemented;
}


/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDStillOff(DVDNav_t *nav)
{
return DVD_E_NotImplemented;
}


/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDVideoPresentationModeChange(DVDNav_t *nav,
					   DVDDisplayMode_t mode)
{
  return DVD_E_NotImplemented;
}


/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDDefaultSubpictureLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang)
{
  return DVD_E_NotImplemented;
}


/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDSubpictureStreamChange(DVDNav_t *nav,
				      DVDSubpictureStream_t SubpictureNr,
				      DVDBool_t display)
{
  return DVD_E_NotImplemented;
}

/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDParentalCountrySelect(DVDNav_t *nav, DVDCountryID_t country)
{
  return DVD_E_NotImplemented;
}


/**
 * @todo Implement function
 *
 * @param nav Specifies the connection to the DVD navigator.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDParentalLevelSelect(DVDNav_t *nav, DVDParentalLevel_t level)
{
  return DVD_E_NotImplemented;
}

  
/** @} end of dvdctrl */

/** @} end of dvdnav */
