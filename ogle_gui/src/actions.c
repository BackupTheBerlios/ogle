#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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


static DVDNav_t *nav;

struct action_number{
  int valid;
  int32_t nr;
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
  DVDButtonActivate(nav);
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
  DVDBool_t spu_shown;

  res = DVDGetCurrentSubpicture(nav, &spu_nr, &spu_this, &spu_shown);

  if(res != DVD_E_Ok) {
    return;
  }

  if(spu_shown == DVDTrue) {
    DVDSetSubpictureState(nav, DVDFalse);
  } else {
    DVDSetSubpictureState(nav, DVDTrue);
  }
}

void actionNextPG(void *data)
{
  DVDNextPGSearch(nav);
}

void actionPrevPG(void *data)
{
  DVDPrevPGSearch(nav);
}

void actionQuit(void *data)
{
  DVDResult_t res;
  res = DVDCloseNav(nav);
  if(res != DVD_E_Ok ) {
    DVDPerror("DVDCloseNav", res);
  }
  exit(0);
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
      if(user->valid && (user->nr < n) && (user->nr > 0)) {
       n = user->nr;
      }
      user->valid = 0;
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
      if(user->valid && (user->nr < n) && (user->nr > 0)) {
       n = user->nr;
      }
      user->valid = 0;
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
  if(!user_nr.valid) {
    user_nr.valid = 1;
    user_nr.nr = 0;
  }

  user_nr.nr = user_nr.nr * 10 + *(uint8_t *)data;
}

static int32_t default_skip_seconds = 10;
static int32_t skip_seconds = 10;

void actionSkipForward(void *data)
{
  struct action_number *user = (struct action_number *)data;
  if(user != NULL) {
    if(user->valid) {
      if((user->nr == 0) || (user->nr < 0)) {
       skip_seconds = default_skip_seconds;
      } else {
       skip_seconds = user->nr;
      }
      user->valid = 0;
    }     
  } 

  DVDTimeSkip(nav, skip_seconds);
}

void actionSkipBackward(void *data)
{
  struct action_number *user = (struct action_number *)data;
  if(user != NULL) {
    if(user->valid) {
      if((user->nr == 0) || (user->nr < 0)) {
       skip_seconds = default_skip_seconds;
      } else {
       skip_seconds = user->nr;
      }
      user->valid = 0;
    }     
  } 

  DVDTimeSkip(nav, -skip_seconds);
}

static const uint8_t digits[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

static action_mapping_t actions[] = {
  { "Play", actionPlay, NULL },
  { "PauseToggle", actionPauseToggle, NULL },
  { "Stop", NULL, NULL },
  { "FastForward", actionFastForward, NULL },
  { "SlowForward", actionSlowForward, NULL },
  { "Faster", actionFaster, NULL },
  { "Slower", actionSlower, NULL },
  { "NextPG", actionNextPG, NULL },
  { "PrevPG", actionPrevPG, NULL },
  { "UpperButton", actionUpperButtonSelect, NULL },
  { "LowerButton", actionLowerButtonSelect, NULL },
  { "LeftButton", actionLeftButtonSelect, NULL },
  { "RightButton", actionRightButtonSelect, NULL},
  { "ButtonActivate", actionButtonActivate, NULL },
  { "TitleMenu", actionMenuCallTitle, NULL },
  { "RootMenu", actionMenuCallRoot, NULL },
  { "AudioMenu", actionMenuCallAudio, NULL },
  { "AngleMenu", actionMenuCallAngle, NULL },
  { "PTTMenu", actionMenuCallPTT, NULL },
  { "SubtitleMenu", actionMenuCallSubpicture, NULL },
  { "Resume", actionResume, NULL },
  { "FullScreenToggle", actionFullScreenToggle, NULL },
  { "SubtitleToggle", actionSubpictureToggle, NULL },
  { "Quit", actionQuit, NULL },
  { "BookmarkAdd", actionBookmarkAdd, NULL },
  { "BookmarkRemove", actionBookmarkRemove, &user_nr },
  { "BookmarkRestore", actionBookmarkRestore, &user_nr },
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
  { "SkipForward", actionSkipForward, &user_nr },
  { "SkipBackward", actionSkipBackward, &user_nr },

  { NULL, NULL }
};

action_mapping_t *get_action_mappings(void)
{

  return actions;
}

