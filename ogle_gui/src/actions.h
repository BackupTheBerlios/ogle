#ifndef ACTIONS_H
#define ACTIONS_H

#include <ogle/dvdcontrol.h>

typedef struct {
  char *str;
  void (*fun)(void *);
} action_mapping_t;


void init_actions(DVDNav_t *new_nav);
action_mapping_t *get_action_mappings(void);

#endif /* ACTIONS_H */
