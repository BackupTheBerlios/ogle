#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include "callbacks.h"
#include "bindings.h"
#include "actions.h"


extern GladeXML *xml;

extern ZoomMode_t zoom_mode;
extern int isPaused;
extern double speed;


static DVDNav_t *nav;


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
  
  w = glade_xml_get_widget(xml, "full_screen");
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




static action_mapping_t actions[] = {
  { "Play", actionPlay },
  { "PauseToggle", actionPauseToggle },
  { "Stop", NULL },
  { "FastForward", actionFastForward },
  { "SlowForward", actionSlowForward },
  { "Faster", actionFaster },
  { "Slower", actionSlower },
  { "NextPG", actionNextPG },
  { "PrevPG", actionPrevPG },
  { "UpperButton", actionUpperButtonSelect },
  { "LowerButton", actionLowerButtonSelect },
  { "LeftButton", actionLeftButtonSelect },
  { "RightButton", actionRightButtonSelect},
  { "ButtonActivate", actionButtonActivate },
  { "TitleMenu", actionMenuCallTitle },
  { "RootMenu", actionMenuCallRoot },
  { "AudioMenu", actionMenuCallAudio },
  { "AngleMenu", actionMenuCallAngle },
  { "PTTMenu", actionMenuCallPTT },
  { "SubtitleMenu", actionMenuCallSubpicture },
  { "Resume", actionResume },
  { "FullScreenToggle", actionFullScreenToggle },
  { "SubtitleToggle", actionSubpictureToggle },
  { "Quit", actionQuit },

  { NULL, NULL }
};

action_mapping_t *get_action_mappings(void)
{

  return actions;
}

