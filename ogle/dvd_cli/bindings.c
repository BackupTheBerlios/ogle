#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "debug_print.h"

#include "bindings.h"


extern DVDNav_t *nav;
extern int bookmarks_autosave;

int prevpg_timeout = 1;

int digit_timeout = 0;
int number_timeout = 0;

int default_skip_seconds = 15;
int skip_seconds = 15;

static int fs = 0;
static int isPaused = 0;
static double speed = 1.0;

struct action_number{
  int valid;
  int32_t nr;
  struct timeval timestamp;
};

static struct action_number user_nr = { 0, 0 };


void actionUpperButtonSelect(void *data)
{
  DVDUpperButtonSelect(nav);	  
}

void actionLowerButtonSelect(void *data)
{
  DVDLowerButtonSelect(nav);
}

void actionLeftButtonSelect(void *data)
{
  DVDLeftButtonSelect(nav);
}

void actionRightButtonSelect(void *data)
{
  DVDRightButtonSelect(nav);
}

void actionButtonActivate(void *data)
{
  struct action_number *user = (struct action_number *)data;
  if(user != NULL && (user->nr >= 0)) {
    DVDButtonSelectAndActivate(nav, user->nr);
  } else { 
    DVDButtonActivate(nav);
  }
}

void actionMenuCallTitle(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Title);
}

void actionMenuCallRoot(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Root);
}

void actionMenuCallSubpicture(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Subpicture);
}

void actionMenuCallAudio(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Audio);
}

void actionMenuCallAngle(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Angle);
}

void actionMenuCallPTT(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Part);
}

void actionResume(void *data)
{
  DVDResume(nav);
}

void actionPauseToggle(void *data)
{
  
  if(isPaused) {
    DVDPauseOff(nav);
    isPaused = 0;
  } else {
    DVDPauseOn(nav);
    isPaused = 1;
  }
  
}

void actionPauseOn(void *data)
{
    DVDPauseOn(nav);
    isPaused = 1;
}

void actionPauseOff(void *data)
{
    DVDPauseOff(nav);
    isPaused = 0;
}

void actionSubpictureToggle(void *data)
{
  DVDResult_t res;
  int spu_nr;
  DVDStream_t spu_this;
  DVDSubpictureState_t spu_state;
  static DVDSubpictureState_t off_state = DVD_SUBPICTURE_STATE_OFF;

  struct action_number *user = (struct action_number *)data;

  if(user && user->nr >= 0 && user->nr < 4) {

    switch(user->nr) {
    case 0:
      spu_state = DVD_SUBPICTURE_STATE_OFF;
      off_state = spu_state;
      break;
    case 1:
      spu_state = DVD_SUBPICTURE_STATE_ON;
      break;
    case 2:
      spu_state = DVD_SUBPICTURE_STATE_FORCEDOFF;
      off_state = spu_state;
      break;
    case 3:
      spu_state = DVD_SUBPICTURE_STATE_DISABLED;
      off_state = spu_state;
      break;
    default:
      break;
    }

    DVDSetSubpictureState(nav, spu_state);

  } else {
    res = DVDGetCurrentSubpicture(nav, &spu_nr, &spu_this, &spu_state);
    
    if(res != DVD_E_Ok) {
      return;
    }
    
    if(spu_state != DVD_SUBPICTURE_STATE_ON) {
      DVDSetSubpictureState(nav, DVD_SUBPICTURE_STATE_ON);
    } else {
      DVDSetSubpictureState(nav, off_state);
    }
  }
}


void actionVideoToggle(void *data)
{
  DVDResult_t res;
  static DVDVideoState_t video_state = DVD_VIDEO_STATE_ON;

  if(video_state == DVD_VIDEO_STATE_ON) {
    video_state = DVD_VIDEO_STATE_OFF;
  } else {
    video_state = DVD_VIDEO_STATE_ON;
  }

  DVDSetVideoState(nav, video_state);
}



struct timeval pg_timestamp = {0, 0};

void actionNextPG(void *data)
{
  struct timeval curtime;

  gettimeofday(&curtime, NULL);
  pg_timestamp = curtime;

  DVDNextPGSearch(nav);
}

void actionPrevPG(void *data)
{
  struct timeval curtime;

  long diff;
  
  gettimeofday(&curtime, NULL);
  diff = curtime.tv_sec - pg_timestamp.tv_sec;
  pg_timestamp = curtime;  

  if((prevpg_timeout && (diff > prevpg_timeout))) {
    DVDTopPGSearch(nav);
  } else {
    DVDPrevPGSearch(nav);
  }
}

void autosave_bookmark(void)
{
  DVDBookmark_t *bm;
  unsigned char id[16];
  char volid[33];
  int volid_type;
  char *state = NULL;
  int n;
  
  if(bookmarks_autosave) {
    if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
      NOTE("%s", "GetDiscID failed\n");
      return;
    }
    
    if(DVDGetVolumeIdentifiers(nav, 0, &volid_type, volid, NULL) != DVD_E_Ok) {
      DNOTE("%s", "GetVolumeIdentifiers failed\n");
      volid_type = 0;
    }
    
    if(DVDGetState(nav, &state) == DVD_E_Ok) {
      
      if((bm = DVDBookmarkOpen(id, NULL, 1)) == NULL) {
	if(errno != ENOENT) {
	  NOTE("%s", "BookmarkOpen failed: ");
	  perror("");
	}
	free(state);
	return;
      }
    
      n = DVDBookmarkGetNr(bm);
      
      if(n == -1) {
	NOTE("%s", "DVDBookmarkGetNr failed\n");
      } else if(n > 0) {
	for(n--; n >= 0; n--) {
	  char *appinfo;
	  if(DVDBookmarkGet(bm, n, NULL, NULL,
			    "common", &appinfo) != -1) {
	    if(appinfo) {
	      if(!strcmp(appinfo, "autobookmark")) {
		if(DVDBookmarkRemove(bm, n) == -1) {
		  NOTE("%s", "DVDBookmarkRemove failed\n");
		}
	      }
	      free(appinfo);
	    }
	  } else {
	    NOTE("%s", "DVDBookmarkGet failed\n");
	  }
	}
      }
      
      
      
      if(DVDBookmarkAdd(bm, state, NULL, "common", "autobookmark") == -1) {
	DNOTE("%s", "BookmarkAdd failed\n");
	DVDBookmarkClose(bm);
	free(state);
	return;
      }
      free(state);
      
      if(volid_type != 0) {
	char *disccomment = NULL;
	if(DVDBookmarkGetDiscComment(bm, &disccomment) != -1) {
	  if((disccomment == NULL) || (disccomment[0] == '\0')) {
	    if(DVDBookmarkSetDiscComment(bm, volid) == -1) {
	      DNOTE("%s", "SetDiscComment failed\n");
	    }
	  }
	  if(disccomment) {
	    free(disccomment);
	  }
	}
      }
      if(DVDBookmarkSave(bm, 0) == -1) {
	NOTE("%s", "BookmarkSave failed\n");
      }
      DVDBookmarkClose(bm);
    }
  }
}

void actionQuit(void *data)
{
  DVDResult_t res;
  
  autosave_bookmark();

  res = DVDCloseNav(nav);
  if(res != DVD_E_Ok ) {
    DVDPerror("DVDCloseNav", res);
  }
  exit(0);
}

void actionFullScreenToggle(void *data)
{
  fs = !fs;
  if(fs) {
    DVDSetZoomMode(nav, ZoomModeFullScreen);
  } else {
    DVDSetZoomMode(nav, ZoomModeResizeAllowed);
  }
}

void actionForwardScan(void *data)
{
  DVDForwardScan(nav, 1.0);
}


void actionPlay(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }
  speed = 1.0;
  DVDForwardScan(nav, speed);
}


void actionFastForward(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if((speed >= 1.0) && (speed < 32.0)) {
    speed +=0.5;
  } else if(speed < 1.0) {
    speed = 1.5;
  }
  DVDForwardScan(nav, speed);
}

void actionSlowForward(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if(speed > 1.0) {
    speed = 0.5;
  } else if((speed > 0.1) && (speed <= 1.0)) {
    speed /= 2.0;
  }
  DVDForwardScan(nav, speed);
}


void actionFaster(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }
  
  if((speed >= 1.0) && (speed < 32.0)) {
    speed += 0.5;
  } else if(speed < 1.0) {
    speed *= 2.0;
  }
  DVDForwardScan(nav, speed);
}

void actionSlower(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if(speed > 1.0) {
    speed -= 0.5;
  } else if((speed > 0.1) && (speed <= 1.0)) {
    speed /= 2.0;
  }
  DVDForwardScan(nav, speed);
}


void actionBookmarkAdd(void *data)
{
  DVDBookmark_t *bm;
  unsigned char id[16];
  char *state = NULL;
  char volid[33];
  int volid_type;
  char *disccomment = NULL;
  
  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    NOTE("%s", "GetDiscID failed\n");
    return;
  }
  
  if(DVDGetVolumeIdentifiers(nav, 0, &volid_type, volid, NULL) != DVD_E_Ok) {
    DNOTE("%s", "GetVolumeIdentifiers failed\n");
    volid_type = 0;
  }
  
  if(DVDGetState(nav, &state) == DVD_E_Ok) {
    if((bm = DVDBookmarkOpen(id, NULL, 1)) == NULL) {
      if(errno != ENOENT) {
	NOTE("%s", "BookmarkOpen failed: ");
	perror("");
      }
      free(state);
      return;
    }
    if(DVDBookmarkAdd(bm, state, NULL, NULL, NULL) == -1) {
      DNOTE("%s", "BookmarkAdd failed\n");
      DVDBookmarkClose(bm);
      free(state);
      return;
    }
    free(state);
    if(volid_type != 0) {
      if(DVDBookmarkGetDiscComment(bm, &disccomment) != -1) {
	if((disccomment == NULL) || (disccomment[0] == '\0')) {
	  if(DVDBookmarkSetDiscComment(bm, volid) == -1) {
	    DNOTE("%s", "SetDiscComment failed\n");
	  }
	}
	if(disccomment) {
	  free(disccomment);
	}
      }
    }
    if(DVDBookmarkSave(bm, 0) == -1) {
      NOTE("%s", "BookmarkSave failed\n");
    }
    DVDBookmarkClose(bm);
  }
}


void actionBookmarkRemove(void *data)
{
  struct action_number *user = (struct action_number *)data;
  DVDBookmark_t *bm;
  unsigned char id[16];
  int n;
  
  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    NOTE("%s", "GetDiscID failed\n");
    return;
  }
  
  if((bm = DVDBookmarkOpen(id, NULL, 0)) == NULL) {
    if(errno != ENOENT) {
      NOTE("%s", "BookmarkOpen failed: ");
      perror("");
    }
    return;
  }

  n = DVDBookmarkGetNr(bm);

  if(n == -1) {
    NOTE("%s", "DVDBookmarkGetNr failed\n");
  } else if(n > 0) {
    if(user != NULL) {
      if((user->nr < n) && (user->nr > 0)) {
	n = user->nr;
      }
    }

    if(DVDBookmarkRemove(bm, n-1) != -1) {
      if(DVDBookmarkSave(bm, 0) == -1) {
	NOTE("%s", "BookmarkSave failed\n");
      }
    } else {
      NOTE("%s", "DVDBookmarkRemove failed\n");
    }
  }
  
  DVDBookmarkClose(bm);
}

void actionBookmarkRestore(void *data)
{
  struct action_number *user = (struct action_number *)data;
  DVDBookmark_t *bm;
  unsigned char id[16];
  char *state = NULL;
  int n;


  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    NOTE("%s", "GetDiscID failed\n");
    return;
  }
  
  if((bm = DVDBookmarkOpen(id, NULL, 0)) == NULL) {
    if(errno != ENOENT) {
      NOTE("%s", "BookmarkOpen failed: ");
      perror("");
    }
    return;
  }

  n = DVDBookmarkGetNr(bm);

  if(n == -1) {
    NOTE("%s", "DVDBookmarkGetNr failed\n");
  } else if(n > 0) {
    if(user != NULL) {
      if((user->nr < n) && (user->nr > 0)) {
	n = user->nr;
      }
    }
    if(DVDBookmarkGet(bm, n-1, &state, NULL, NULL, NULL) != -1) {
      if(state) {
	if(DVDSetState(nav, state) != DVD_E_Ok) {
	  NOTE("%s", "DVDSetState failed\n");
	}
	free(state);
      }
    } else {
      NOTE("%s", "BookmarkGet failed\n");
    }
  }
  DVDBookmarkClose(bm);
}


void actionDigit(void *data)
{
  struct timeval curtime;
  long diff;
  
  gettimeofday(&curtime, NULL);
  diff = curtime.tv_sec - user_nr.timestamp.tv_sec;
  
  if(!user_nr.valid || (digit_timeout && (diff > digit_timeout))) {
    user_nr.nr = 0;
  }
  
  user_nr.timestamp = curtime;
  user_nr.nr = user_nr.nr * 10 + *(uint8_t *)data;
  user_nr.valid = 1;
}


void actionSkipForward(void *data)
{
  struct action_number *user = (struct action_number *)data;

  if(user != NULL) {
    if((user->nr == 0) || (user->nr < 0)) {
      skip_seconds = default_skip_seconds;
    } else {
      skip_seconds = user->nr;
    }
  } 
  
  DVDTimeSkip(nav, skip_seconds);
}

void actionSkipBackward(void *data)
{
  struct action_number *user = (struct action_number *)data;
  if(user != NULL) {
    if((user->nr == 0) || (user->nr < 0)) {
      skip_seconds = default_skip_seconds;
    } else {
      skip_seconds = user->nr;
    }
  } 
  
  DVDTimeSkip(nav, -skip_seconds);
}


void actionSaveScreenshot(void *data)
{
  DVDSaveScreenshot(nav, ScreenshotModeWithoutSPU, NULL);
}


void actionSaveScreenshotWithSPU(void *data)
{
  DVDSaveScreenshot(nav, ScreenshotModeWithSPU, NULL);
}


static void printAudioAttributes(DVDAudioAttributes_t *attr)
{
  char *t_str;
  switch(attr->AudioFormat) {
  case DVD_AUDIO_FORMAT_AC3:
    t_str = "AC-3";
    break;
  case DVD_AUDIO_FORMAT_MPEG1:    
    t_str = "MPEG-1"; //MPEG-1 (or MPEG-2 without extension stream)
    break;
  case DVD_AUDIO_FORMAT_MPEG1_DRC:
    t_str = "MPEG-1 DRC";
    break;
  case DVD_AUDIO_FORMAT_MPEG2:
    t_str = "MPEG-2"; // MPEG-2 with extension stream
    break;
  case DVD_AUDIO_FORMAT_MPEG2_DRC:
    t_str = "MPEG-2 DRC";
    break;
  case DVD_AUDIO_FORMAT_LPCM:
    t_str = "LPCM";
    break;
  case DVD_AUDIO_FORMAT_DTS:
    t_str = "DTS";
    break;
  case DVD_AUDIO_FORMAT_SDDS:
    t_str = "SDDS";
    break;
  case DVD_AUDIO_FORMAT_Other:
  default:
    t_str = "Other";
    break;
  }
  DNOTE("%s", t_str);
  
  if(attr->AudioFormat == DVD_AUDIO_FORMAT_LPCM) {
    DNOTEC(" %d bits %d Hz", 
	   attr->SampleQuantization,
	   attr->SampleFrequency);
  }
  DNOTEC(" %dch", attr->NumberOfChannels);
  
  if(attr->AudioType == DVD_AUDIO_TYPE_Language) {
    DNOTEC(" %c%c", attr->Language >> 8, attr->Language & 0xff);
  }
  
  switch(attr->LanguageExtension) {
  case DVD_AUDIO_LANG_EXT_NotSpecified:
    t_str = "";
    break;
  case DVD_AUDIO_LANG_EXT_NormalCaptions:
    t_str = "Normal Captions";
    break;
  case DVD_AUDIO_LANG_EXT_VisuallyImpaired:
    t_str = "Captions for visually impaired";
    break;
  case DVD_AUDIO_LANG_EXT_DirectorsComments1:
    t_str = "Director's comments 1";
    break;
  case DVD_AUDIO_LANG_EXT_DirectorsComments2:
    t_str = "Director's comments 2";
    break;
  }
  DNOTEC(" %s", t_str);
  
  switch(attr->AppMode) {
  case DVD_AUDIO_APP_MODE_None:
    t_str = "";
    break;
  case DVD_AUDIO_APP_MODE_Karaoke:
    t_str = "Karaoke Mode";
    break;
  case DVD_AUDIO_APP_MODE_Surround:
    t_str = "Surround Mode";
    break;
  case DVD_AUDIO_APP_MODE_Other:
    t_str = "Other Mode";
    break;
  }
  DNOTEC(" %s\n", t_str);
}


void actionAudioStreamChange(void *data)
{
  int res;
  int streams_avail;
  DVDAudioStream_t cur_stream;
  DVDAudioStream_t new_stream;
  DVDAudioAttributes_t au_attr;
  DVDBool_t enabled;
  int next_track = 0;
  
  struct action_number *user = (struct action_number *)data;
  
  res = DVDGetCurrentAudio(nav, &streams_avail, &cur_stream);
  if(res == DVD_E_Ok) {
    if(streams_avail > 0) {
      if(user != NULL && user->nr >= 0 && user->nr < streams_avail) {
	new_stream = user->nr;
      } else {
	new_stream = cur_stream+1;
	next_track = 8;
      }

      do {
	if(new_stream >= streams_avail) {
	  new_stream = 0;
	}
	res = DVDIsAudioStreamEnabled(nav, new_stream, &enabled);
	if(res == DVD_E_Ok) {
	  if(enabled) {
	    break;
	  }
	  new_stream++;
	}
	next_track--;
      } while(next_track > 0);
      
      if(enabled) {
	res = DVDAudioStreamChange(nav, new_stream);
	if(res != DVD_E_Ok) {
	  DVDPerror("DVDAudioStreamChange: ", res);
	}
	DNOTE("get audioattr snr %d\n", new_stream);
	res = DVDGetAudioAttributes(nav, new_stream, &au_attr);
	if(res != DVD_E_Ok) {
	  ERROR("DVDGetAudioAttributes: %s\n", DVDStrerror(res));
	} else {
	  printAudioAttributes(&au_attr);
	}
      } else {
	if(next_track == -1) {
	  DNOTE("Audio stream %d not enabled\n", new_stream-1);
	} else {
	  DNOTE("%s", "No enabled audio streams\n");
	}
      }
    }
  } else {
    DVDPerror("DVDGetCurrentAudio: ", res);
  }
}


static void printSubpictureAttributes(DVDSubpictureAttributes_t *attr)
{
  char *t_str;
  switch(attr->Type) {
  case   DVD_SUBPICTURE_TYPE_NotSpecified:
    t_str = "Not Specified";
    break;
  case   DVD_SUBPICTURE_TYPE_Language:
    t_str = "Language";
    break;
  case   DVD_SUBPICTURE_TYPE_Other:
    t_str = "Other";
    break;
  }
  DNOTE("sp_attr: type: %s", t_str);
  
  switch(attr->CodingMode) {
  case DVD_SUBPICTURE_CODING_RunLength:
    t_str = "RLE";
    break;
  case DVD_SUBPICTURE_CODING_Extended:
    t_str = "Extended";
    break;
  case DVD_SUBPICTURE_CODING_Other:
    t_str = "Other";
    break;
  }	
  DNOTEC(" codemode: %s", t_str);
  
  if(attr->Type == DVD_SUBPICTURE_TYPE_Language) {
    DNOTEC(" lang: %c%c", 
	   attr->Language >> 8,
	   attr->Language & 0xff);
    
    switch(attr->LanguageExtension) {
    case DVD_SUBPICTURE_LANG_EXT_NotSpecified:
      t_str = "Not Specified";
      break;
    case DVD_SUBPICTURE_LANG_EXT_NormalCaptions:
      t_str = "Normal Captions";
      break;
    case DVD_SUBPICTURE_LANG_EXT_BigCaptions:
      t_str = "Big Captions";
      break;
    case DVD_SUBPICTURE_LANG_EXT_ChildrensCaptions:
      t_str = "Childrend Captions";
      break;
    case DVD_SUBPICTURE_LANG_EXT_NormalCC:
      t_str = "Normal CC";
      break;
    case DVD_SUBPICTURE_LANG_EXT_BigCC:
      t_str = "Big CC";
      break;
    case DVD_SUBPICTURE_LANG_EXT_ChildrensCC:
      t_str = "Childrens CC";
      break;
    case DVD_SUBPICTURE_LANG_EXT_Forced:
      t_str = "Forced";
      break;
    case DVD_SUBPICTURE_LANG_EXT_NormalDirectorsComments:
      t_str = "Normal Directors Comments";
      break;
    case DVD_SUBPICTURE_LANG_EXT_BigDirectorsComments:
      t_str = "Big Directors Comments";
      break;
    case DVD_SUBPICTURE_LANG_EXT_ChildrensDirectorsComments:
      t_str = "Childrens Directors Comments";
      break;
    }	    
    DNOTEC(" lang_ext: %s", t_str);
  }
  DNOTEC("%s", "\n");
}


void actionSubtitleStreamChange(void *data)
{
  int res;
  int streams_avail;
  DVDSubpictureStream_t cur_stream;
  DVDSubpictureStream_t new_stream;
  DVDSubpictureState_t spu_state;
  DVDSubpictureAttributes_t sp_attr;

  struct action_number *user = (struct action_number *)data;
  
  
  
  res = DVDGetCurrentSubpicture(nav, &streams_avail,
				&cur_stream, &spu_state);
  if(res == DVD_E_Ok) {
    if(streams_avail > 0) {
      if(spu_state == DVD_SUBPICTURE_STATE_ON) {
	if(user != NULL && user->nr >=0 && user->nr < streams_avail) {
	  new_stream = user->nr;
	} else {
	  new_stream = cur_stream+1;
	}
	
	if(new_stream >= streams_avail) {
	  new_stream = 0;
	  // if we are at the last stream, turn off subtitles
	  res = DVDSetSubpictureState(nav, DVD_SUBPICTURE_STATE_OFF);
	  if(res != DVD_E_Ok) {
	    DVDPerror("DVDSetSubpictureState: ", res);
	  }
	}
	res = DVDSubpictureStreamChange(nav, new_stream);
	if(res != DVD_E_Ok) {
	  DVDPerror("DVDSubpictureStreamChange: ", res);
	}

      } else {
	// if subtitles are off, turn them on and
	// change stream if requested
	if(user != NULL && user->nr >=0 && user->nr < streams_avail) {
	  new_stream = user->nr;
	  res = DVDSubpictureStreamChange(nav, new_stream);
	  if(res != DVD_E_Ok) {
	    DVDPerror("DVDSubpictureStreamChange: ", res);
	  }
	} else {
	  new_stream = cur_stream;
	}

	res = DVDSetSubpictureState(nav, DVD_SUBPICTURE_STATE_ON);
	if(res != DVD_E_Ok) {
	  DVDPerror("DVDSetSubpictureState: ", res);
	}
      }
      DNOTE("get subpattr snr %d\n", new_stream);
      res = DVDGetSubpictureAttributes(nav, new_stream, &sp_attr);
      if(res != DVD_E_Ok) {
	ERROR("DVDGetSubpictureAttributes: %s\n", DVDStrerror(res));
	//DVDPerror("DVDGetSubpictureAttributes: ", res);
      } else {
	printSubpictureAttributes(&sp_attr);
      }
    }   
  } else {
    DVDPerror("DVDGetCurrentSubpicture: ", res);
  }

  
}

/* Calls vfun() with the data parameter set to a pointer
 *  to the user number if valid, otherwise the data parameter is NULL
 */
void do_number_action(void *vfun)
{
  void (*number_action)(void *) = vfun;
  struct timeval curtime;
  long diff;
  struct action_number *number = NULL;
  
  gettimeofday(&curtime, NULL);
  diff = curtime.tv_sec - user_nr.timestamp.tv_sec;
  
  if(number_timeout && (diff > number_timeout)) {
    user_nr.valid = 0;
  }
  if(user_nr.valid) {
    number = &user_nr;
  }
  number_action(number);
  user_nr.valid = 0;  
}

void do_action(void *fun)
{
  void (*action)(void *) = fun;

  user_nr.valid = 0;  
  action(NULL);
}

typedef struct {
  PointerEventType event_type;
  unsigned int button;
  unsigned int modifier_mask;
  void (*fun)(void *);
} pointer_mapping_t;

static unsigned int pointer_mappings_index = 0;
//static unsigned int nr_pointer_mappings = 0;

static pointer_mapping_t *pointer_mappings;

typedef struct {
  KeySym keysym;
  void (*fun)(void *);
  void *arg;
  unsigned int modifiers;
} ks_map_t;

static unsigned int ks_maps_index = 0;
static unsigned int nr_ks_maps = 0;

static ks_map_t *ks_maps;

static const uint8_t digits[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

typedef struct {
  char *str;
  void (*fun)(void *);
  void *ptr;
} action_mapping_t;

static action_mapping_t actions[] = {
  { "Play", do_action, actionPlay },
  { "PauseToggle", do_action, actionPauseToggle },
  { "Stop", NULL, NULL },
  { "FastForward", do_action, actionFastForward },
  { "SlowForward", do_action, actionSlowForward },
  { "Faster", do_action, actionFaster },
  { "Slower", do_action, actionSlower },
  { "NextPG", do_action, actionNextPG },
  { "PrevPG", do_action, actionPrevPG },
  { "UpperButton", do_action, actionUpperButtonSelect },
  { "LowerButton", do_action, actionLowerButtonSelect },
  { "LeftButton", do_action, actionLeftButtonSelect },
  { "RightButton", do_action, actionRightButtonSelect },
  { "ButtonActivate", do_number_action, actionButtonActivate },
  { "TitleMenu", do_action, actionMenuCallTitle },
  { "RootMenu", do_action, actionMenuCallRoot },
  { "AudioMenu", do_action, actionMenuCallAudio },
  { "AngleMenu", do_action, actionMenuCallAngle },
  { "PTTMenu", do_action, actionMenuCallPTT },
  { "SubtitleMenu", do_action, actionMenuCallSubpicture },
  { "Resume", do_action, actionResume },
  { "FullScreenToggle", do_action, actionFullScreenToggle },
  { "SubtitleToggle", do_number_action, actionSubpictureToggle },
  { "VideoToggle", do_number_action, actionVideoToggle },
  { "Quit", do_action, actionQuit },
  { "BookmarkAdd", do_action, actionBookmarkAdd },
  { "BookmarkRemove", do_number_action, actionBookmarkRemove },
  { "BookmarkRestore", do_number_action, actionBookmarkRestore },
  { "DigitZero", actionDigit, (void *)&digits[0] },
  { "DigitOne",  actionDigit, (void *)&digits[1] },
  { "DigitTwo",  actionDigit, (void *)&digits[2] },
  { "DigitThree",actionDigit, (void *)&digits[3] },
  { "DigitFour", actionDigit, (void *)&digits[4] },
  { "DigitFive", actionDigit, (void *)&digits[5] },
  { "DigitSix",  actionDigit, (void *)&digits[6] },
  { "DigitSeven",actionDigit, (void *)&digits[7] },
  { "DigitEight",actionDigit, (void *)&digits[8] },
  { "DigitNine", actionDigit, (void *)&digits[9] },
  { "SkipForward", do_number_action, actionSkipForward },
  { "SkipBackward", do_number_action, actionSkipBackward },
  { "SaveScreenshot", do_action, actionSaveScreenshot },
  { "SaveScreenshotWithSPU", do_action, actionSaveScreenshotWithSPU },
  { "AudioStreamChange", do_number_action, actionAudioStreamChange },
  { "SubtitleStreamChange", do_number_action, actionSubtitleStreamChange },
  { NULL, NULL }
};


void do_pointer_action(pointer_event_t *p_ev)
{
  int n;
  
  for(n = 0; n < pointer_mappings_index; n++) {
    if((pointer_mappings[n].event_type == p_ev->type) &&
       (pointer_mappings[n].modifier_mask == p_ev->modifier_mask)) {
      if(pointer_mappings[n].fun != NULL) {
	pointer_mappings[n].fun(p_ev);
      }
      return;
    }
  }
  
  return;
}


void do_keysym_action(KeySym keysym, KeySym keysym_base,
		      unsigned int keycode, unsigned int modifiers)
{
  int n;
  unsigned int dont_care_mods = LockMask | Mod1Mask; //get_dont_care_mods();

  modifiers &= (ShiftMask | LockMask | ControlMask |
    Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask);

  //check bindings with modifiers first
  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].modifiers && (ks_maps[n].keysym == keysym_base)) {
      if((ks_maps[n].modifiers == modifiers) || 
	 (ks_maps[n].modifiers == (modifiers & ~dont_care_mods))) {
	if(ks_maps[n].fun != NULL) {
	  ks_maps[n].fun(ks_maps[n].arg);
	}
	return;
      }
    }
  }
  //then without modifiers
  for(n = 0; n < ks_maps_index; n++) {
    if((!ks_maps[n].modifiers) && (ks_maps[n].keysym == keysym)) {
      if(ks_maps[n].fun != NULL) {
	ks_maps[n].fun(ks_maps[n].arg);
      }
      return;
    }
  }
  return;
}


void remove_keysym_binding(KeySym keysym, unsigned int modifiers)
{
  int n;
  
  for(n = 0; n < ks_maps_index; n++) {
    if((ks_maps[n].keysym == keysym) &&
       (ks_maps[n].modifiers == modifiers)) {
      ks_maps[n].keysym = NoSymbol;
      ks_maps[n].modifiers = 0;
      ks_maps[n].fun = NULL;
      ks_maps[n].arg = NULL;
      return;
    }
  }
}

void add_keysym_binding(KeySym keysym, unsigned int modifiers, 
			void(*fun)(void *), void *arg)
{
  int n;
  
  for(n = 0; n < ks_maps_index; n++) {
    if((ks_maps[n].keysym == keysym) &&
       (ks_maps[n].modifiers == modifiers)) {
      ks_maps[n].fun = fun;
      ks_maps[n].arg = arg;
      return;
    }
  }

  if(ks_maps_index >= nr_ks_maps) {
    nr_ks_maps+=32;
    ks_maps = (ks_map_t *)realloc(ks_maps, sizeof(ks_map_t)*nr_ks_maps);
  }
  
  ks_maps[ks_maps_index].keysym = keysym;
  ks_maps[ks_maps_index].modifiers = modifiers;
  ks_maps[ks_maps_index].fun = fun;
  ks_maps[ks_maps_index].arg = arg;
  
  ks_maps_index++;
  

  return;
}
  
void add_keybinding(char *key, char **modifier_array, char *action)
{
  KeySym keysym;
  int n = 0;
  unsigned int modifiers = 0;
  
  keysym = XStringToKeysym(key);
  
  if(keysym == NoSymbol) {
    WARNING("add_keybinding(): '%s' not a valid keysym\n", key);
    return;
  }
  
  if(modifier_array) {
    char **m;
    for(m = modifier_array; *m != NULL; m++) {
      if(!strcasecmp(*m, "Shift")) {
	modifiers |= ShiftMask;
      } else if(!strcasecmp(*m, "Lock")) {
	modifiers |= LockMask;
      } else if(!strcasecmp(*m, "Control")) {
	modifiers |= ControlMask;
      } else if(!strcasecmp(*m, "Mod1")) {
	modifiers |= Mod1Mask;
      } else if(!strcasecmp(*m, "Mod2")) {
	modifiers |= Mod2Mask;
      } else if(!strcasecmp(*m, "Mod3")) {
	modifiers |= Mod3Mask;
      } else if(!strcasecmp(*m, "Mod4")) {
	modifiers |= Mod4Mask;
      } else if(!strcasecmp(*m, "Mod5")) {
	modifiers |= Mod5Mask;
      } else {
	WARNING("add_keybinding(): No such modifier: '%s'\n", *m);
      }
      
    }
  }

  if(!strcmp("NoAction", action)) {
    remove_keysym_binding(keysym, modifiers);
    return;
  }
    
  for(n = 0; actions[n].str != NULL; n++) {
    if(!strcmp(actions[n].str, action)) {
      if(actions[n].fun != NULL) {
	add_keysym_binding(keysym, modifiers, actions[n].fun, actions[n].ptr);
      }
      return;
    }
  }
  
  WARNING("add_keybinding(): No such action: '%s'\n", action);
  
  return;
}

/*
void add_pointerbinding(char *, char *action)
{
  KeySym keysym;
  int n = 0;
  
  keysym = XStringToKeysym(key);
  
  if(keysym == NoSymbol) {
  WARNING("add_keybinding(): '%s' not a valid keysym\n", key);
  return;
  }
  
  if(!strcmp("NoAction", action)) {
    remove_keysym_binding(keysym);
    return;
  }
    
  for(n = 0; actions[n].str != NULL; n++) {
    if(!strcmp(actions[n].str, action)) {
      if(actions[n].fun != NULL) {
	add_keysym_binding(keysym, actions[n].fun);
      }
      return;
    }
  }
  
  WARNING("add_keybinding(): No such action: '%s'\n", action);
  
  return;
}
*/
