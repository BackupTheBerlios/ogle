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
  //pthread_create(&at, NULL, xsniff_mouse, NULL);

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

	res = DVDMouseSelect(nav, ev.xbutton.x, ev.xbutton.y);
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

	  res = DVDMouseActivate(nav, ev.xbutton.x, ev.xbutton.y);
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
	char buff[2] = {0};
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
	  break;
	default:
	  break;
	}
	
	switch(buff[0]) {
	case 't':
	  DVDMenuCall(nav, DVD_MENU_Title);
	  break;
	case 'r':
	  DVDMenuCall(nav, DVD_MENU_Root);
	  break;
	case 's':
	  DVDMenuCall(nav, DVD_MENU_Subpicture);
	  break;
	case 'a':
	  DVDMenuCall(nav, DVD_MENU_Audio);
	  break;
	case 'g':
	  DVDMenuCall(nav, DVD_MENU_Angle);
	  break;
	case 'p':
	  DVDMenuCall(nav, DVD_MENU_Part);
	  break;
	case 'c':
	  DVDResume(nav, DVD_MENU_Part);
	  break;
	case '>':
	  // next;
	  DVDNextPGSearch(nav);
	  break;
	case '<':
	  // prev;
	  DVDPrevPGSearch(nav);
	  break;
	case 'q':
	  {
	    DVDResult_t res;
	    res = DVDCloseNav(nav);
	    if(res != DVD_E_Ok ) {
	      DVDPerror("DVDCloseNav", res);
	    }
	    exit(0);
	  }
	  break;
	case 's':

	  break;
	default:
	  break;
	} /* end case KeyPress */
#endif    
      }
    }
  }
}
  
