#include <stdlib.h>
#include <stdio.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include "bindings.h"


extern DVDNav_t *nav;

static int fs = 0;
static int isPaused = 0;
static double speed = 1.0;

void actionUpperButtonSelect(void)
{
  DVDUpperButtonSelect(nav);	  
}

void actionLowerButtonSelect(void)
{
  DVDLowerButtonSelect(nav);
}

void actionLeftButtonSelect(void)
{
  DVDLeftButtonSelect(nav);
}

void actionRightButtonSelect(void)
{
  DVDRightButtonSelect(nav);
}

void actionButtonActivate(void)
{
  DVDButtonActivate(nav);
}

void actionMenuCallTitle(void)
{
  DVDMenuCall(nav, DVD_MENU_Title);
}

void actionMenuCallRoot(void)
{
  DVDMenuCall(nav, DVD_MENU_Root);
}

void actionMenuCallSubpicture(void)
{
  DVDMenuCall(nav, DVD_MENU_Subpicture);
}

void actionMenuCallAudio(void)
{
  DVDMenuCall(nav, DVD_MENU_Audio);
}

void actionMenuCallAngle(void)
{
  DVDMenuCall(nav, DVD_MENU_Angle);
}

void actionMenuCallPTT(void)
{
  DVDMenuCall(nav, DVD_MENU_Part);
}

void actionResume(void)
{
  DVDResume(nav);
}

void actionPauseToggle(void)
{
  
  if(isPaused) {
    DVDPauseOff(nav);
    isPaused = 0;
  } else {
    DVDPauseOn(nav);
    isPaused = 1;
  }
  
}

void actionPauseOn(void)
{
    DVDPauseOn(nav);
    isPaused = 1;
}

void actionPauseOff(void)
{
    DVDPauseOff(nav);
    isPaused = 0;
}

void actionSubpictureToggle(void)
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

void actionNextPG(void)
{
  DVDNextPGSearch(nav);
}

void actionPrevPG(void)
{
  DVDPrevPGSearch(nav);
}

void actionQuit(void)
{
  DVDResult_t res;
  res = DVDCloseNav(nav);
  if(res != DVD_E_Ok ) {
    DVDPerror("DVDCloseNav", res);
  }
  exit(0);
}

void actionFullScreenToggle(void)
{
  fs = !fs;
  if(fs) {
    DVDSetZoomMode(nav, ZoomModeFullScreen);
  } else {
    DVDSetZoomMode(nav, ZoomModeResizeAllowed);
  }
}

void actionForwardScan(void)
{
  DVDForwardScan(nav, 1.0);
}


void actionPlay(void)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }
  speed = 1.0;
  DVDForwardScan(nav, speed);
}


void actionFastForward(void)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if((speed >= 1.0) && (speed < 8.0)) {
    speed +=0.5;
  } else {
    speed = 1.5;
  }
  DVDForwardScan(nav, speed);
}

void actionSlowMotion(void)
{
  DVDForwardScan(nav, 0.5);
}






typedef struct {
  KeySym keysym;
  void (*fun)(void);
  void *arg;
} ks_map_t;

unsigned int ks_maps_index = 0;
unsigned int nr_ks_maps = 0;

ks_map_t *ks_maps;

typedef struct {
  char *str;
  void (*fun)(void);
} action_mapping_t;

action_mapping_t actions[] = {
  { "Play", actionPlay },
  { "Pause", actionPauseToggle },
  { "Stop", NULL },
  { "FastForward", NULL },
  { "SlowMotion", NULL },
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
  { "FullScreen", actionFullScreenToggle },
  { "SubtitleToggle", actionSubpictureToggle },
  { "Quit", actionQuit },

  { NULL, NULL }
};


void do_keysym_action(KeySym keysym)
{
  int n;

  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      ks_maps[n].fun();
      return;
    }
  }
  return;
}

void add_keysym_binding(KeySym keysym, void(*fun)(void))
{
  int n;
  
  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      ks_maps[n].fun = fun;
      return;
    }
  }

  if(ks_maps_index >= nr_ks_maps) {
    nr_ks_maps+=32;
    ks_maps = (ks_map_t *)realloc(ks_maps, sizeof(ks_map_t)*nr_ks_maps);
  }
  
  ks_maps[ks_maps_index].keysym = keysym;
  ks_maps[ks_maps_index].fun = fun;
  
  ks_maps_index++;
  

  return;
}
  
void add_keybinding(char *key, char *action)
{
  KeySym keysym;
  int n = 0;
  
  keysym = XStringToKeysym(key);
  
  if(keysym == NoSymbol) {
    fprintf(stderr, "WARNING: add_keybinding(): key '%s' not a valid keysym\n",
	    key);
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
  
  fprintf(stderr, "WARNING: add_keybinding(): no such action '%s'\n", action);
  
  return;
}

