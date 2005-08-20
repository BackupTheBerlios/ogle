#ifndef BINDINGS_H
#define BINDINGS_H

#include <X11/Xlib.h>

typedef enum {
  PointerButtonPress,
  PointerMotion
} PointerEventType;

typedef struct {
  PointerEventType type;
  unsigned int time;
  int x;
  int y;
  unsigned int modifier_mask; 
} pointer_event_t;

void do_keysym_action(KeySym keysym, KeySym keysym_base,
		      unsigned int keycode, unsigned int modifiers);
void add_keybinding(char *key, char **modifier_array, char *action);

#endif /* BINDINGS_H */
