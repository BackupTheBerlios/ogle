/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/msg.h>
#include <string.h>
#include <ogle/msgevents.h>
#include <ogle/dvdcontrol.h>

/** @file
 * DVD navigator interface.
 * This file contains the functions that form the interface to
 * the DVD navigator. These are the functions a user interface
 * should use to control DVD playback.
 */


/**
 * Internally seen type for the handle to a DVD navigator
 */
struct DVDNav_s {
  MsgEventClient_t client;
  MsgEventClient_t voclient;
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
  
  if(MsgSendEvent(msq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
    fprintf(stderr, "dvdcontrol: get_nav_client\n");
    return -1;
  }

  while(ev.type != MsgEventQGntCapability) {
    if(MsgNextEvent(msq, &ev) == -1) {
      fprintf(stderr, "dvdcontrol: get_nav_client\n");
      return -1;
    }
  }
  
  return ev.gntcapability.capclient;
}

/**
 * Get the id of the VideoOutput client.
 */
static MsgEventClient_t get_vo_client(MsgEventQ_t *msq)
{
  MsgEvent_t ev;
  ev.type = MsgEventQReqCapability;
  ev.reqcapability.capability = VIDEO_OUTPUT;
  
  if(MsgSendEvent(msq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
    fprintf(stderr, "dvdcontrol: get_vo_client\n");
    return -1;
  }

  while(ev.type != MsgEventQGntCapability) {
    if(MsgNextEvent(msq, &ev) == -1) {
      fprintf(stderr, "dvdcontrol: get_vo_client\n");
      return -1;
    }
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
  MsgEvent_t ev;

  *nav = (DVDNav_t *)malloc(sizeof(DVDNav_t));
  if(*nav == NULL) {
    return DVD_E_NOMEM;
  }
  
  if(((*nav)->msgq = MsgOpen(msgqid)) == NULL) {
    free(*nav);
    return DVD_E_Unspecified;
  }
  
  ev.type = MsgEventQRegister;
  ev.registercaps.capabilities = UI_DVD_GUI;
  if(MsgSendEvent((*nav)->msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
    free(*nav);
    return DVD_E_Unspecified;    
  }
  
  (*nav)->client = get_nav_client((*nav)->msgq);
  
  switch((*nav)->client) {
  case -1:
    free(*nav);
    return DVD_E_Unspecified;
  case CLIENT_NONE:
    free(*nav);
    return DVD_E_Unspecified;
  default:
    break;
  }
  
  (*nav)->voclient = CLIENT_NONE;
  
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
 * @todo handle other return events.
 * @todo add more return values.
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
  ev.dvdctrl.cmd.audioattributes.streamnr = StreamNr;
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
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
 * @todo handle more return events.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamsAvailable Points to where the number of available 
 * streams will be written.
 * @param CurrentStream Points to where the current stream will be written.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetCurrentAudio(DVDNav_t *nav, int *const StreamsAvailable,
			       DVDAudioStream_t *const CurrentStream)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGetCurrentAudio;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }

  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
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
 * @todo more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specify the audio stream to retrieve information about.
 * @param Enabled Specifies if the audio stream is enabled or disabled.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDIsAudioStreamEnabled(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				    DVDBool_t *const Enabled)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlIsAudioStreamEnabled;
  ev.dvdctrl.cmd.audiostreamenabled.streamnr = StreamNr;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
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
DVDResult_t DVDGetCurrentAngle(DVDNav_t *nav, int *const AnglesAvailable,
			       DVDAngle_t *const CurrentAngle)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGetCurrentAngle;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }

  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlCurrentAngle)) {
      *AnglesAvailable = ev.dvdctrl.cmd.currentangle.anglesavailable;
      *CurrentAngle = ev.dvdctrl.cmd.currentangle.anglenr;
      return DVD_E_Ok;
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
DVDResult_t DVDGetCurrentVideoAttributes(DVDNav_t *nav,
					 const DVDVideoAttributes_t *Attr)
{
  return DVD_E_NotImplemented;
}




/**
 * Get the number of available subpicture streams and the current one.
 * @todo handle more return events.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamsAvailable Points to where the number of available 
 * streams will be written.
 * @param CurrentStream Points to where the current stream will be written.
 * @param Display Specifies if the subpicture display is on or off
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetCurrentSubpicture(DVDNav_t *nav,
				    int *const StreamsAvailable,
				    DVDSubpictureStream_t *const CurrentStream,
				    DVDBool_t *const Display)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGetCurrentSubpicture;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
    
  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlCurrentSubpicture)) {
      *StreamsAvailable = ev.dvdctrl.cmd.currentsubpicture.nrofstreams;
      *CurrentStream = ev.dvdctrl.cmd.currentsubpicture.currentstream;
      *Display = ev.dvdctrl.cmd.currentsubpicture.display;
      return DVD_E_Ok;
    }
  } 
}


/**
 * Checks if the specified subpicture stream is available.
 * @todo handle other return events
 * @todo more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specifies the subpicture stream to retrieve information
 * for.
 * @param Enabled Specifies if the subpicture stream is enabled or disabled.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDIsSubpictureStreamEnabled(DVDNav_t *nav,
					 DVDSubpictureStream_t StreamNr,
					 DVDBool_t *const Enabled)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlIsSubpictureStreamEnabled;
  ev.dvdctrl.cmd.subpicturestreamenabled.streamnr = StreamNr;
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlSubpictureStreamEnabled)) {
      if(ev.dvdctrl.cmd.subpicturestreamenabled.streamnr == StreamNr) {
	*Enabled = ev.dvdctrl.cmd.subpicturestreamenabled.enabled;
	return DVD_E_Ok;
      }
    }
  } 
}

/**
 * Get the attributes of the specified subpicture stream.
 * @todo handle other return events.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specifies which subpicture stream attributes
 * will be retrieved for.
 * @param Attr Points to where the attributes of the specified
 * subpicture stream will be written.
 *
 * @return If successful DVD_E_Ok is returned and the attributes
 * pointed to by Attr have been updated. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented. 
 */
DVDResult_t DVDGetSubpictureAttributes(DVDNav_t *nav,
				       DVDSubpictureStream_t StreamNr,
				       const DVDSubpictureAttributes_t *Attr)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlGetSubpictureAttributes;
  ev.dvdctrl.cmd.subpictureattributes.streamnr = StreamNr;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  while(1) {
    if(MsgNextEvent(nav->msgq, &ev) == -1) {
      return DVD_E_Unspecified;
    }
    if((ev.type == MsgEventQDVDCtrl) &&
       (ev.dvdctrl.cmd.type == DVDCtrlSubpictureAttributes)) {
      if(ev.dvdctrl.cmd.subpictureattributes.streamnr == StreamNr) {
	memcpy((void *)Attr,
	       (void *)&(ev.dvdctrl.cmd.subpictureattributes.attr),
	       sizeof(DVDSubpictureAttributes_t));
	return DVD_E_Ok;
      }
    }
  } 
}


/** 
 * Get the language of an subpicture stream.
 * @todo Implement function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specifies the subpicture stream which laguage code
 * will be retrieved.
 * @param Language Points to where the language code of the
 * specified subpicture stream will be written.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDGetSubpictureLanguage(DVDNav_t *nav,
				     DVDSubpictureStream_t StreamNr,
				     const DVDLangID_t *Language)
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
DVDResult_t DVDGetDefaultSubpictureLanguage(DVDNav_t *nav,
					    const DVDLangID_t *Language,
					    const DVDSubpictureLangExt_t *SubpictureExtension)
{
  return DVD_E_NotImplemented;
}

/** @} end of dvdinfo */

/** 
 * @todo Remove this function.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param ev is the returned message incase DVD_E_Ok is returned.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDNextEvent(DVDNav_t *nav, MsgEvent_t *ev)
{
  
  if(MsgNextEvent(nav->msgq, ev) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
  
} 

DVDResult_t DVDRequestInput(DVDNav_t *nav, InputMask_t mask)
{
  MsgEvent_t ev;

  ev.type = MsgEventQReqInput;
  ev.reqinput.mask = mask;
  
  if((nav->voclient == CLIENT_NONE) ||
     (nav->voclient == -1)) {
    nav->voclient = get_vo_client(nav->msgq);
  }
  
  switch(nav->voclient) {
  case -1:
  case CLIENT_NONE:
    fprintf(stderr, "dvdctrl: voclient error\n");
    return DVD_E_Unspecified;
    break;
  default:
    break;
  }
  
  if(MsgSendEvent(nav->msgq, nav->voclient, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
  
}

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





DVDResult_t DVDSetAspectModeSrc(DVDNav_t *nav, AspectModeSrc_t mode_src)
{
  MsgEvent_t ev;
  ev.type = MsgEventQSetAspectModeSrc;
  ev.setaspectmodesrc.mode_src = mode_src;

  if((nav->voclient == CLIENT_NONE) ||
     (nav->voclient == -1)) {
    nav->voclient = get_vo_client(nav->msgq);
  }
  switch(nav->voclient) {
  case -1:
  case CLIENT_NONE:
    fprintf(stderr, "dvdctrl: voclient error\n");
    return DVD_E_Unspecified;
    break;
  default:
    break;
  }
  
  if(MsgSendEvent(nav->msgq, nav->voclient, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}

DVDResult_t DVDSetSrcAspect(DVDNav_t *nav, AspectModeSrc_t mode_sender,
			    uint16_t aspect_frac_n, uint16_t aspect_frac_d)
{
  MsgEvent_t ev;
  ev.type = MsgEventQSetSrcAspect;
  ev.setsrcaspect.mode_src = mode_sender;
  ev.setsrcaspect.aspect_frac_n = aspect_frac_n;
  ev.setsrcaspect.aspect_frac_d = aspect_frac_d;
  
  if((nav->voclient == CLIENT_NONE) ||
     (nav->voclient == -1)) {
    nav->voclient = get_vo_client(nav->msgq);
  }
  switch(nav->voclient) {
  case -1:
  case CLIENT_NONE:
    fprintf(stderr, "dvdctrl: voclient error\n");
    return DVD_E_Unspecified;
    break;
  default:
    break;
  }
  
  if(MsgSendEvent(nav->msgq, nav->voclient, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}

DVDResult_t DVDSetZoomMode(DVDNav_t *nav, ZoomMode_t zoom_mode)
{
  MsgEvent_t ev;
  ev.type = MsgEventQSetZoomMode;
  ev.setzoommode.mode = zoom_mode;
  
  if((nav->voclient == CLIENT_NONE) ||
     (nav->voclient == -1)) {
    nav->voclient = get_vo_client(nav->msgq);
  }
  switch(nav->voclient) {
  case -1:
  case CLIENT_NONE:
    fprintf(stderr, "dvdctrl: voclient error\n");
    return DVD_E_Unspecified;
    break;
  default:
    break;
  }
  
  if(MsgSendEvent(nav->msgq, nav->voclient, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Selects the directory where the dvd files are *.VOB *.IFO.
 * @todo implement
 * @param nav Specifies the connection to the DVD navigator.
 * @param 
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDSetDVDRoot(DVDNav_t *nav, char *Path)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrlLong;
  ev.dvdctrllong.cmd.type = DVDCtrlLongSetDVDRoot;
  strncpy(ev.dvdctrllong.cmd.dvdroot.path, Path,
	  sizeof(ev.dvdctrllong.cmd.dvdroot.path));
  ev.dvdctrllong.cmd.dvdroot.path[sizeof(ev.dvdctrllong.cmd.dvdroot.path)-1]
    = '\0';
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}

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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) { 
    return DVD_E_Unspecified;
  }
 
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
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }

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
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Jump to the beginning of the next program
 * @todo more return values.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Jump to the beginning of the previous program
 * @todo more return values.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Jump to the beginning of the current PG.
 * @todo more return values.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Jump to beginning of the specified ptt in the current title.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param PTT Specifies the PTT
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Jump to the beginning of the specified ptt in the specified title.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Title Specifies the Title.
 * @param PTT Specifies the PTT.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) { 
    return DVD_E_Unspecified;
  }
 
  return DVD_E_Ok;
}


/** 
 * @todo more return values.
 * @todo better description.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * @todo more return values.
 * @todo better description.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * @todo more return values.
 * @todo better description.
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
  ev.dvdctrl.cmd.type = DVDCtrlTimePlay;
  ev.dvdctrl.cmd.timeplay.title = Title;
  ev.dvdctrl.cmd.timeplay.time = time;
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Pause playback.
 * @todo more return values.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Resume playback if in pause mode.
 * @todo more return values.
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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
 * @retval DVD_E_Unspecified.
 */
DVDResult_t DVDStop(DVDNav_t *nav)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlStop;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}



/**
 * Select the default menu language
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Lang Specifies the language.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_Unspecified.
 */
DVDResult_t DVDDefaultMenuLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlDefaultMenuLanguageSelect;
  ev.dvdctrl.cmd.defaultmenulanguageselect.langid = Lang;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/** 
 * Change the audio stream
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param StreamNr Specifies which audio stream to decode
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

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}

/** 
 * Selects the default audio language.
 * @todo more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Lang Specifies the lanugage id.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_Unspecified.
 */
DVDResult_t DVDDefaultAudioLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlDefaultAudioLanguageSelect;
  ev.dvdctrl.cmd.defaultaudiolanguageselect.langid = Lang;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
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
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlAngleChange;
  ev.dvdctrl.cmd.anglechange.anglenr = AngleNr;
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
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
 * Selects the default subpicture language.
 * @todo more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Lang Specifies the language id.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_Unspecified.
 */
DVDResult_t DVDDefaultSubpictureLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlDefaultSubpictureLanguageSelect;
  ev.dvdctrl.cmd.defaultsubpicturelanguageselect.langid = Lang;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/**
 * Change the subpicture stream.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param SubpictureNr Specifies which subpicture stream to change to.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDSubpictureStreamChange(DVDNav_t *nav,
				      DVDSubpictureStream_t SubpictureNr)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlSubpictureStreamChange;
  ev.dvdctrl.cmd.subpicturestreamchange.streamnr = SubpictureNr;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/**
 * Set the display of subpictures on or off.
 * @todo more return values.
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param Display Specifies wheter to disaply or hide subpictures.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_NotImplemented The function is not implemented.
 */
DVDResult_t DVDSetSubpictureState(DVDNav_t *nav, DVDBool_t Display)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlSetSubpictureState;
  ev.dvdctrl.cmd.subpicturestate.display = Display;

  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/**
 * Select the country for parental check.
 * @todo more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param country Specifies the country id.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_Unspecified.
 */
DVDResult_t DVDParentalCountrySelect(DVDNav_t *nav, DVDCountryID_t country)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlParentalCountrySelect;
  ev.dvdctrl.cmd.parentalcountryselect.countryid = country;
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}


/**
 * Selects the parental level.
 * @todo more return values
 *
 * @param nav Specifies the connection to the DVD navigator.
 * @param level Specifies the parental level.
 *
 * @return If successful DVD_E_Ok is returned. Otherwise an error code
 * is returned.
 *
 * @retval DVD_E_Ok Success.
 * @retval DVD_E_Unspecified.
 */
DVDResult_t DVDParentalLevelSelect(DVDNav_t *nav, DVDParentalLevel_t level)
{
  MsgEvent_t ev;
  ev.type = MsgEventQDVDCtrl;
  ev.dvdctrl.cmd.type = DVDCtrlParentalLevelSelect;
  ev.dvdctrl.cmd.parentallevelselect.level = level;
  
  if(MsgSendEvent(nav->msgq, nav->client, &ev, 0) == -1) {
    return DVD_E_Unspecified;
  }
  
  return DVD_E_Ok;
}

  
/** @} end of dvdctrl */

/** @} end of dvdnav */
