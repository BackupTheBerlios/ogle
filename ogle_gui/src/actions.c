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
  
  if(user != NULL && user->valid && (user->nr >= 0)) {
    DVDButtonSelectAndActivate(nav, user->nr);
  } else { 
    DVDButtonActivate(nav);
  }
  if(user != NULL) {
    user->valid = 0;
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

  { NULL, NULL }
};

action_mapping_t *get_action_mappings(void)
{

  return actions;
}

