#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "debug_print.h"

#include "bindings.h"


extern DVDNav_t *nav;

static int fs = 0;
static int isPaused = 0;
static double speed = 1.0;

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
    NOTE("%s", "GetDiscID failed\n");
    return;
  }

  if(DVDGetState(nav, &state) == DVD_E_Ok) {
    if((bm = DVDBookmarkOpen(id, NULL, 1)) == NULL) {
      NOTE("%s", "BookmarkOpen failed\n");
      return;
    }
    if(DVDBookmarkAdd(bm, state, NULL, NULL, NULL) == -1) {
      NOTE("%s", "BookmarkAdd failed\n");
      DVDBookmarkClose(bm);
      return;
    }
    if(DVDBookmarkSave(bm, 0) == -1) {
      NOTE("%s", "BookmarkSave failed\n");
    }
    DVDBookmarkClose(bm);
    NOTE("%s", "Bookmark Saved\n");
  }
  free(state);
}

void actionBookmarkRestore(void *data)
{
  DVDBookmark_t *bm;
  unsigned char id[16];
  char *state = NULL;
  int n;

  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    NOTE("%s", "GetDiscID failed\n");
    return;
  }
  
  if((bm = DVDBookmarkOpen(id, NULL, 0)) == NULL) {
    NOTE("%s", "BookmarkOpen failed\n");
    return;
  }
  for(n = -1;;n++) {
    if(DVDBookmarkGet(bm, n+1, NULL, NULL, NULL, NULL) == -1) {
      break;
    }
  }
  if(n == -1) {
    NOTE("%s", "DVDBookmarkGet failed\n");
  } else {
    DVDBookmarkGet(bm, n, &state, NULL, NULL, NULL);
    
    if(DVDSetState(nav, state) != DVD_E_Ok) {
      NOTE("%s", "DVDSetState failed\n");
    }
    free(state);
  }
  DVDBookmarkClose(bm);
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
} ks_map_t;

static unsigned int ks_maps_index = 0;
static unsigned int nr_ks_maps = 0;

static ks_map_t *ks_maps;

typedef struct {
  char *str;
  void (*fun)(void *);
} action_mapping_t;

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
  { "BookmarkAdd", actionBookmarkAdd },
  { "BookmarkRestore", actionBookmarkRestore },
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


void do_keysym_action(KeySym keysym)
{
  int n;

  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      if(ks_maps[n].fun != NULL) {
	ks_maps[n].fun(NULL);
      }
      return;
    }
  }
  return;
}


void remove_keysym_binding(KeySym keysym)
{
  int n;
  
  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      ks_maps[n].keysym = NoSymbol;
      ks_maps[n].fun = NULL;
      return;
    }
  }
}

void add_keysym_binding(KeySym keysym, void(*fun)(void *))
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
    fprintf(stderr,
	    "WARNING[dvd_cli]: add_keybinding(): '%s' not a valid keysym\n",
	    key);
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
  
  fprintf(stderr,
	  "WARNING[dvd_cli]: add_keybinding(): No such action: '%s'\n",
	  action);
  
  return;
}

/*
void add_pointerbinding(char *, char *action)
{
  KeySym keysym;
  int n = 0;
  
  keysym = XStringToKeysym(key);
  
  if(keysym == NoSymbol) {
    fprintf(stderr,
	    "WARNING[dvd_cli]: add_keybinding(): '%s' not a valid keysym\n",
	    key);
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
  
  fprintf(stderr,
	  "WARNING[dvd_cli]: add_keybinding(): No such action: '%s'\n",
	  action);
  
  return;
}
*/
