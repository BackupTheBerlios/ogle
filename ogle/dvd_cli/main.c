/* Ogle - A video player
 * Copyright (C) 2000, 2001 Håkan Hjort
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ogle/dvdcontrol.h>
#include "xsniffer.h"

//#include "menu.h"
//#include "audio.h"
//#include "subpicture.h"

DVDNav_t *nav;

int msgqid;
extern int win;

static char *program_name;
void usage()
{
  fprintf(stderr, "Usage: %s [-m <msgid>] path\n", 
          program_name);
}

int
main (int argc, char *argv[])
{
  char *dvd_path;
  int c;
  
  program_name = argv[0];

  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  if(argc - optind > 1){
    usage();
    exit(1);
  }
  
  if(argc - optind == 1){
    dvd_path = argv[optind];
  } else { 
    dvd_path = "/dev/dvd";
  }
  
  if(msgqid !=-1) { // ignore sending data.
    DVDResult_t res;
    sleep(1);
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen:", res);
      exit(1);
    }
  }
  
  {
    DVDResult_t res;
    res = DVDSetDVDRoot(nav, dvd_path);
    if(res != DVD_E_Ok) {
      DVDPerror("DVDSetDVDRoot:", res);
      exit(1);
    }
    
    // hack
    /*
    res = DVDGetXWindowID(nav, (void *)&windowid);
    if(res != DVD_E_Ok) {
      DVDPerror("DVDGetXWindowID:", res);
      exit(1);
    }
    win = windowid;
    */
    //    sleep(1);
    DVDRequestInput(nav,
		    INPUT_MASK_KeyPress | INPUT_MASK_ButtonPress |
		    INPUT_MASK_PointerMotion);
    // xsniff_init();
  }
  
  xsniff_mouse(NULL);
  exit(0);
}

