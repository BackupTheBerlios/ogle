#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "bindings.h"
#include "actions.h"


typedef struct {
  PointerEventType event_type;
  unsigned int button;
  unsigned int modifier_mask;
  void (*fun)(void *);
} pointer_mapping_t;

static unsigned int pointer_mappings_index = 0;
static unsigned int nr_pointer_mappings = 0;

static pointer_mapping_t *pointer_mappings;

typedef struct {
  KeySym keysym;
  void (*fun)(void *);
  void *arg;
} ks_map_t;

static unsigned int ks_maps_index = 0;
static unsigned int nr_ks_maps = 0;

static ks_map_t *ks_maps;



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
  action_mapping_t *actions = get_action_mappings();
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
