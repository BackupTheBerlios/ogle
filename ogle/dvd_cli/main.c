/* Ogle - A video player
 * Copyright (C) 2000, 2001 H�kan Hjort
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
#include <string.h>


#include <X11/Xlib.h>

#include <ogle/dvdcontrol.h>

#include "xsniffer.h"
#include "interpret_config.h"
#include "bindings.h"


DVDNav_t *nav;
char *dvd_path;

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
  DVDResult_t res;

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
  
  
  if(msgqid !=-1) { // ignore sending data.
    sleep(1);
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen:", res);
      exit(1);
    }
  }

  interpret_config();

  if(argc - optind == 1){
    dvd_path = argv[optind];
  }
  
  res = DVDSetDVDRoot(nav, dvd_path);
  if(res != DVD_E_Ok) {
    DVDPerror("DVDSetDVDRoot:", res);
    exit(1);
  }
  
  DVDRequestInput(nav,
		  INPUT_MASK_KeyPress | INPUT_MASK_ButtonPress |
		  INPUT_MASK_PointerMotion);
  
  xsniff_mouse(NULL);
  exit(0);
}


