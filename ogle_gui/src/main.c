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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

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
//extern int win;

poptContext ctx;
struct poptOption options[] = {
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


int
main (int argc, char *argv[])
{
  GtkWidget *app;

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif
  //gnome_init ("ogle", VERSION, argc, argv);
  gnome_init_with_popt_table("ogle", VERSION, argc, argv, options, 0, &ctx);

  if(argc==1) {
    fprintf(stderr, "Error: Do not start ogle_gui directly. Start ogle\n");
    exit(1);
  }

  if(msgqid !=-1) { // ignore sending data.
    DVDResult_t res;
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen", res);
      exit(1);
    }
  }
  
  //gnome_window_icon_set_default_from_file ("");
    
  audio_menu_new();
  subpicture_menu_new();
  
  app = create_app ();
  gtk_widget_show (app);

  menu_new(app);  

  gtk_main ();
  return 0;
}

