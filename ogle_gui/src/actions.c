#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <gtk/gtk.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include "callbacks.h"
#include "bindings.h"
#include "actions.h"

#include "my_glade.h"

extern ZoomMode_t zoom_mode;
extern int isPaused;
extern double speed;
extern int dvdpipe[2];

int digit_timeout = 0;
int number_timeout = 0;
int default_skip_seconds = 15;
int skip_seconds = 15;
int prevpg_timeout = 1;

static DVDNav_t *nav;

struct action_number{
  int valid;
  int32_t nr;
  struct timeval timestamp;
};

static struct action_number user_nr = { 0, 0 };

void init_actions(DVDNav_t *new_nav)
{
  nav = new_nav;
}


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

void actionQuit(void *data)
{
  char buf2[1];

  buf2[0] = 'q';
  
  if(dvdpipe[1]) {
    write(dvdpipe[1], buf2, 1);
  }
}

void actionFullScreenToggle(void *data)
{
  GtkWidget *w;
  gboolean val;
  
  zoom_mode = (zoom_mode == ZoomModeResizeAllowed) 
    ? ZoomModeFullScreen : ZoomModeResizeAllowed;
  
  val = (zoom_mode == ZoomModeFullScreen) ? TRUE : FALSE;
  
  w = get_glade_widget("full_screen");
  if(w==NULL) {
    fprintf(stderr, "xsniffer: failed to lookup_widget();\n");
  }
  
  gdk_threads_enter(); // Toggle the menu checkbutton.
  gtk_signal_handler_block_by_func(GTK_OBJECT(w), 
				   on_full_screen_activate, NULL);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), val);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(w),
				     on_full_screen_activate, NULL);
  gdk_threads_leave();
  
  DVDSetZoomMode(nav, zoom_mode);
   
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

  if((speed >= 1.0) && (speed < 8.0)) {
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
  
  if((speed >= 1.0) && (speed < 8.0)) {
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
  
  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    fprintf(stderr, "NOTE[ogle_gui]: %s", "GetDiscID failed\n");
    return;
  }

  if(DVDGetState(nav, &state) == DVD_E_Ok) {
    if((bm = DVDBookmarkOpen(id, NULL, 1)) == NULL) {
      if(errno != ENOENT) {
	fprintf(stderr, "NOTE[ogle_gui]: %s", "BookmarkOpen failed:");
	perror("");
      }
      return;
    }
    if(DVDBookmarkAdd(bm, state, NULL, NULL, NULL) == -1) {
      fprintf(stderr, "NOTE[ogle_gui]: %s", "BookmarkAdd failed\n");
      DVDBookmarkClose(bm);
      return;
    }
    if(DVDBookmarkSave(bm, 0) == -1) {
      fprintf(stderr, "NOTE[ogle_gui]: %s", "BookmarkSave failed:");
      perror("");
    }
    DVDBookmarkClose(bm);
    //fprintf(stderr, "NOTE[ogle_gui]: %s", "Bookmark Saved\n");
  }
  free(state);
}

void actionBookmarkRemove(void *data)
{
  struct action_number *user = (struct action_number *)data;
  DVDBookmark_t *bm;
  unsigned char id[16];
  int n;
  
  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    fprintf(stderr, "NOTE[ogle_gui]: %s", "GetDiscID failed\n");
    return;
  }
  
  if((bm = DVDBookmarkOpen(id, NULL, 0)) == NULL) {
    if(errno != ENOENT) {
      fprintf(stderr, "NOTE[ogle_gui]: %s", "BookmarkOpen failed:");
      perror("");
    }
    return;
  }

  n = DVDBookmarkGetNr(bm);
  if(n == -1) {
    //fprintf(stderr, "NOTE[ogle_gui]: %s", "DVDBookmarkGetNr failed\n");
  } else if(n > 0) {
    if(user != NULL) {
      if((user->nr < n) && (user->nr > 0)) {
	n = user->nr;
      }
    }
    
    if(DVDBookmarkRemove(bm, n-1) != -1) {
      //fprintf(stderr, "NOTE[ogle_gui]: Bookmark %d removed\n", n-1);
      
      if(DVDBookmarkSave(bm, 0) == -1) {
	fprintf(stderr, "NOTE[ogle_gui]: %s", "BookmarkSave failed:");
	perror("");
      }
    } else {
      //fprintf(stderr, "NOTE[ogle_gui]: %s", "DVDBookmarkRemove failed\n");
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
    fprintf(stderr, "NOTE[ogle_gui]: %s", "GetDiscID failed\n");
    return;
  }
  
  if((bm = DVDBookmarkOpen(id, NULL, 0)) == NULL) {
    if(errno != ENOENT) {
      fprintf(stderr, "NOTE[ogle_gui]: %s", "BookmarkOpen failed:");
      perror("");
    }
    return;
  }

  n = DVDBookmarkGetNr(bm);

  if(n == -1) {
    //fprintf(stderr, "NOTE[ogle_gui]: %s", "DVDBookmarkGetNr failed\n");
  } else if(n > 0) {
    if(user != NULL) {
      if((user->nr < n) && (user->nr > 0)) {
	n = user->nr;
      }
    }

    DVDBookmarkGet(bm, n-1, &state, NULL, NULL, NULL);
    
    if(DVDSetState(nav, state) != DVD_E_Ok) {
      fprintf(stderr, "NOTE[ogle_gui]: %s", "DVDSetState failed\n");
    }
    free(state);
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

	res = DVDGetAudioAttributes(nav, new_stream, &au_attr);
	if(res != DVD_E_Ok) {
	  fprintf(stderr,"NOTE[ogle_gui]: DVDGetAudioAttributes: %s\n",
		  DVDStrerror(res));
	}
      }
    }
  } else {
    DVDPerror("DVDGetCurrentAudio: ", res);
  }
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

      res = DVDGetSubpictureAttributes(nav, new_stream, &sp_attr);
      if(res != DVD_E_Ok) {
	fprintf(stderr, "NOTE[ogle_gui]: DVDGetSubpictureAttributes: %s\n",
		DVDStrerror(res));
      }
    }   
  } else {
    DVDPerror("DVDGetCurrentSubpicture: ", res);
  }
}


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


static const uint8_t digits[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

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
  { "RightButton", do_action, actionRightButtonSelect},
  { "ButtonActivate", do_number_action, actionButtonActivate },
  { "TitleMenu", do_action, actionMenuCallTitle },
  { "RootMenu", do_action, actionMenuCallRoot },
  { "AudioMenu", do_action, actionMenuCallAudio },
  { "AngleMenu", do_action, actionMenuCallAngle },
  { "PTTMenu", do_action, actionMenuCallPTT },
  { "SubtitleMenu", do_action, actionMenuCallSubpicture },
  { "Resume", do_action, actionResume },
  { "FullScreenToggle", do_action, actionFullScreenToggle },
  { "SubtitleToggle", do_action, actionSubpictureToggle },
  { "SubtitleToggle", do_number_action, actionSubpictureToggle },
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

action_mapping_t *get_action_mappings(void)
{

  return actions;
}

