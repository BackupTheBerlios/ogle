#ifndef BINDINGS_H
#define BINDINGS_H

#include <X11/Xlib.h>

void do_keysym_action(KeySym keysym);
void add_keybinding(char *key, char *action);

#endif /* BINDINGS_H */
