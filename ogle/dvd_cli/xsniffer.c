/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2005 Vilhelm Bergman, Björn Englund
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <ogle/dvdcontrol.h>

#include "xsniffer.h"
#include "bindings.h"

extern DVDNav_t *nav;



void* xsniff_mouse(void* args) {
  DVDEvent_t ev;

  while(1) {
    
    if(DVDNextEvent(nav, &ev) != DVD_E_Ok)
      continue;
    
    switch(ev.type) {
      
      
      //case MotionNotify:
    case DVDEventInputPointerMotion:
      
      {
	DVDResult_t res;
	int x, y;
	
	x = ev.input.x;
	y = ev.input.y;
	
	res = DVDMouseSelect(nav, x, y);
	
	if(res != DVD_E_Ok) {
	  fprintf(stderr, "DVDMouseSelect failed. Returned: %d\n", res);
	}
      }
      /*
      {
	pointer_mapping_t p_ev;
	p_ev.type = PointerMotion;
	p_ev.time = mev.input.time;
	p_ev.x = mev.input.x;
	p_ev.y = mev.input.y;
	p_ev.modifier_mask = mev.mod_mask;

	do_pointer_action(&p_ev);
      }
      */
      break;
    case DVDEventInputButtonPress:
      switch(ev.input.input) {
      case 0x1:
	{ 
	  DVDResult_t res;
	  int x, y;

	  x = ev.input.x;
	  y = ev.input.y;
	  
	  res = DVDMouseActivate(nav, x, y);
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
	/*
	fprintf(stderr, "keysym: %ld, modifier: %ld\n",
		mev.input.input,
		mev.input.mod_mask);
	*/
	do_keysym_action(keysym, ev.input.input_base,
			 ev.input.input_keycode, ev.input.mod_mask);
      }
      break;
    default:
      break;
    }
  }
}
  





 





