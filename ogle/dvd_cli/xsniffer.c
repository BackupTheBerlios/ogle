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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include "xsniffer.h"

extern DVDNav_t *nav;


void xsniff_init() {
  
  fprintf(stderr, "sniff_init\n");


  fprintf(stderr, "sniff_init  out\n");
}

void* xsniff_mouse(void* args) {
  MsgEvent_t mev;
  
  while(1) {
    

    DVDNextEvent(nav, &mev);

    switch(mev.type) {
    
      
      //case MotionNotify:
    case MsgEventQInputPointerMotion:
      {
	DVDResult_t res;
	int x, y;
	
	x = mev.input.x;
	y = mev.input.y;
	
	res = DVDMouseSelect(nav, x, y);
	
	if(res != DVD_E_Ok) {
	  fprintf(stderr, "DVDMouseSelect failed. Returned: %d\n", res);
	}
      }
      break;
    case MsgEventQInputButtonPress:
      switch(mev.input.input) {
      case 0x1:
	{ 
	  DVDResult_t res;
	  int x, y;

	  x = mev.input.x;
	  y = mev.input.y;
	  
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
    case MsgEventQInputKeyPress:
      {
	KeySym keysym;
	keysym = mev.input.input;

	switch(keysym) {
	case XK_Up:
	  DVDUpperButtonSelect(nav);	  
	  break;
	case XK_Down:
	  DVDLowerButtonSelect(nav);
	  break;
	case XK_Left:
	  DVDLeftButtonSelect(nav);
	  break;
	case XK_Right:
	  DVDRightButtonSelect(nav);
	  break;
	case XK_Return:
	case XK_KP_Enter:
	  DVDButtonActivate(nav);
	  break;
	case XK_t:
	  DVDMenuCall(nav, DVD_MENU_Title);
	  break;
	case XK_r:
	  DVDMenuCall(nav, DVD_MENU_Root);
	  break;
	case XK_s:
	  DVDMenuCall(nav, DVD_MENU_Subpicture);
	  break;
	case XK_a:
	  DVDMenuCall(nav, DVD_MENU_Audio);
	  break;
	case XK_g:
	  DVDMenuCall(nav, DVD_MENU_Angle);
	  break;
	case XK_p:
	  DVDMenuCall(nav, DVD_MENU_Part);
	  break;
	case XK_c:
	  DVDResume(nav);
	  break;
	case XK_greater:
	  // next;
	  DVDNextPGSearch(nav);
	  break;
	case XK_less:
	  // prev;
	  DVDPrevPGSearch(nav);
	  break;
	case XK_q:
	  {
	    DVDResult_t res;
	    res = DVDCloseNav(nav);
	    if(res != DVD_E_Ok ) {
	      DVDPerror("DVDCloseNav", res);
	    }
	    exit(0);
	  }
	  break;
	case XK_f:
	  // fullscreen
	  {
	    static int fs = 0;
	    fs = !fs;
	    if(fs) {
	      DVDSetZoomMode(nav, ZoomModeFullScreen);
	    } else {
	      DVDSetZoomMode(nav, ZoomModeResizeAllowed);
	    }
	  }
	  break;
	  
	default:
	  break;
	}
      default:
	break;
      }
    }
  }
}
  
