#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <gnome.h>
#include <libgnomeui/gnome-init.h>

#include <ogle/dvdcontrol.h>
#include "interface.h"
#include "support.h"
#include "xsniffer.h"

#include "menu.h"
#include "audio.h"
#include "subpicture.h"

static char* window_string = NULL;

DVDNav_t *nav;

int msgqid;
extern int win;

poptContext ctx;
struct poptOption options[] = {
  { 
    "window",
    'w',
    POPT_ARG_STRING,
    &window_string,
    0,
    N_("Windows-id of player"),
    N_("winid")
  },
  { 
    "msgqid",
    'm',
    POPT_ARG_INT,
    &msgqid,
    0,
    N_("message queue id of player"),
    N_("msgqid")
  },
  {
    NULL,
    '\0',
    0,
    NULL,
    0,
    NULL,
     NULL
  }
};


void usage()
{
  fprintf(stderr, "Usage: [-m <msgid>] [-w <window_id>]\n");          
}






int
main (int argc, char *argv[])
{
  GtkWidget *app;

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif
  //gnome_init ("ogle", VERSION, argc, argv);
  gnome_init_with_popt_table("ogle", VERSION, argc, argv, options, 0, &ctx);

  if(window_string == NULL) {
    fprintf(stderr, "Inget win_id\n");
    window_string = "-1";
  }
  win = strtol(window_string, NULL, 0);
  
  fprintf(stderr, "Window-id: 0x%x\n", win);
  
  if(msgqid !=-1) { // ignore sending data.
    DVDResult_t res;
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen", res);
      exit(1);
    }
  }
  
  if(win != -1) {  // ignore sniffing
    xsniff_init();
  }

  
  audio_menu_new();
  subpicture_menu_new();
  
  app = create_app ();
  gtk_widget_show (app);

  menu_new(app);  

  gtk_main ();
  return 0;
}

