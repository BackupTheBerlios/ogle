/* Ogle - A video player
 * Copyright (C) 2000, 2001 Vilhelm Bergman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <gtk/gtk.h>

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <ogle/dvdcontrol.h>

#include "xsniffer.h"
#include "callbacks.h"
#include "bindings.h"
#include "actions.h"



static DVDNav_t *nav2;
pthread_t at;


void xsniff_init(char *msgq_str) {
  DVDResult_t res;
  res = DVDOpenNav(&nav2, msgq_str);
  if(res != DVD_E_Ok ) {
    DVDPerror("xsniffer: xsniff_init() DVDOpen", res);
    exit(1);
  }

  sleep(1);
  DVDRequestInput(nav2,
		  INPUT_MASK_KeyPress | INPUT_MASK_ButtonPress |
		  INPUT_MASK_PointerMotion);
  pthread_create(&at, NULL, xsniff_mouse, NULL);
  
}

void* xsniff_mouse(void* args) {
  DVDEvent_t ev;

  init_actions(nav2);
  
  while(1) {

    if (DVDNextEvent(nav2, &ev) != DVD_E_Ok)
      pthread_exit(NULL);
    
    switch(ev.type) {

    case DVDEventInputPointerMotion:
      {
	DVDResult_t res;
	int x, y;

	x = ev.input.x;
	y = ev.input.y;
	res = DVDMouseSelect(nav2, x, y);
	  
	if(res != DVD_E_Ok) {
	  fprintf(stderr, "DVDMouseSelect failed. Returned: %d\n", res);
	}
      }
      break;
    case DVDEventInputButtonPress:
      switch(ev.input.input) {
      case 0x1:
	{ 
	  DVDResult_t res;
	  int x, y;
	  x = ev.input.x;
	  y = ev.input.y;
	  res = DVDMouseActivate(nav2, x, y);
	  if(res != DVD_E_Ok) {
	    fprintf(stderr, "DVDMouseActivate failed. Returned: %d\n", res);
	  }
	}
	break;
      case 0x2:
	break;
      case 0x3:
	break;
      }
      break;

    case DVDEventInputKeyPress:
      {
	KeySym keysym;
	keysym = ev.input.input;

	do_keysym_action(keysym);
      }
      break;
    default:
      break;
    }
  }
}
  
