#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

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
  
  //  fprintf(stderr, "argc: %d optind: %d argv[optind]: %s\n", argc, optind, argv[optind]);
  
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
    unsigned long windowid;
    DVDResult_t res;
    res = DVDSetDVDRoot(nav, dvd_path);
    if(res != DVD_E_Ok) {
      DVDPerror("DVDSetDVDRoot:", res);
      exit(1);
    }
    
    // hack
    res = DVDGetXWindowID(nav, (void *)&windowid);
    if(res != DVD_E_Ok) {
      DVDPerror("DVDGetXWindowID:", res);
      exit(1);
    }
    win = windowid;
    xsniff_init();
  }
  
  xsniff_mouse(NULL);
  exit(0);
}

