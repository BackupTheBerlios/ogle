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
#include "xsniffer.h"

extern DVDNav_t *nav;
Display *display;
int win = 0;
pthread_t at;


void xsniff_init() {
  int ret = 0;
  
  if(win==0) {
    fprintf(stderr, "Window-id == 0  Won't try to sniff events.\n");
    return;
  }

  fprintf(stderr, "sniff_init\n");
  display = XOpenDisplay(NULL);

  ret = XSelectInput(display, win, StructureNotifyMask | KeyPressMask 
		     | PointerMotionMask | ButtonPressMask | ExposureMask);
  
  fprintf(stderr, "Ret: %d\n", ret);
  pthread_create(&at, NULL, xsniff_mouse, NULL);

  fprintf(stderr, "sniff_init  out\n");
}

void* xsniff_mouse(void* args) {
  XEvent ev;

  fprintf(stderr, "xsniff_mouse\n");
  while(1) {
    XNextEvent(display, &ev);
    
    switch(ev.type) {

    case MotionNotify:
      {
	DVDResult_t res;
	int i = 5;
	Bool b;

	usleep(50000);  // wait for 0.05 s
	b = XCheckMaskEvent(display, PointerMotionMask, &ev);
	while(b == True && i >= 0) {
	  i--;
	  b = XCheckMaskEvent(display, PointerMotionMask, &ev);
	}

	{ 
	  int x, y;
	  XWindowAttributes xattr;
	  XGetWindowAttributes(display, win, &xattr);
	  // Represent the coordinate as a fixpoint numer btween 0..65536
	  x = ev.xbutton.x * 65536 / xattr.width;
	  y = ev.xbutton.y * 65536 / xattr.height;
	  res = DVDMouseSelect(nav, x, y);
	}
	if(res != DVD_E_Ok) {
	  fprintf(stderr, "DVDMouseSelect failed. Returned: %d\n", res);
	}
      }
      break;
    case ButtonPress:
      switch(ev.xbutton.button) {
      case 0x1:
	{ 
	  DVDResult_t res;
	  int x, y;
	  XWindowAttributes xattr;
	  XGetWindowAttributes(display, win, &xattr);
	  // Represent the coordinate as a fixpoint numer btween 0..65536
	  x = ev.xbutton.x * 65536 / xattr.width;
	  y = ev.xbutton.y * 65536 / xattr.height;
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


    case KeyPress:
      {
	char buff[2];
	//static int debug_change = 0;
	KeySym keysym;
	XLookupString(&(ev.xkey), buff, 2, &keysym, NULL);
	buff[1] = '\0';

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
	default:
	  break;
	}
	
#if 0	
	switch(buff[0]) {
	case 'n':
	  // step;
	  break;
	case 'r':
	  //  stop / start
	  break;
	case 'p':
	  // screenshot = 1;
	  // draw_win(&(windows[0]));
	  break;
	case 'q':
	  // display_exit();
	  break;
	case 's':
	  scalemode_change = 1;
	  break;
	case '1':
	case '2':
	case '3':
	case '4':
	  if(debug_change) 
	    break; /* Handled below the switch */
	  else if(scalemode_change) {
#ifdef HAVE_MLIB
	    switch(atoi(&buff[0])) {
	    case 1:
	      scalemode = MLIB_NEAREST;
	      break;
	    case 2:
	      scalemode = MLIB_BILINEAR;
	      break;
	    case 3:
	      scalemode = MLIB_BICUBIC;
	      break;
	    case 4:
	      scalemode = MLIB_BICUBIC2;
	      break;
	    default:
	      break;
	    }
#endif
	    scalemode_change = 0;	  
	  }
	  else { /* Scale size */
#if defined(HAVE_MLIB) || defined(HAVE_XV)
	    int x = atoi(&buff[0]);
	    display_change_size(windows[0].image->horizontal_size * x, 
				windows[0].image->vertical_size * x);
#endif
	  }
	  break;
	default:
	  break;
	} /* end case KeyPress */
#endif    
      }
    }
  }
}
  
