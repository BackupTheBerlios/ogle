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
#include <string.h>

#include <gtk/gtk.h>

#include <ogle/dvdcontrol.h>
#include "xsniffer.h"

#include "debug_print.h"
#include "menu.h"
#include "audio.h"
#include "subpicture.h"
#include "bindings.h"
#include "interpret_config.h"
#include "my_glade.h"

char *program_name;
DVDNav_t *nav;
char *dvd_path;
int msgqid;

GtkWidget *app;

void set_dvd_path(char *new_path)
{
  if(dvd_path != NULL) {
    free(dvd_path);
    dvd_path = NULL;
  }
  if(new_path != NULL) {
    dvd_path = strdup(new_path);
  }
}

int
main (int argc, char *argv[])
{
  program_name = argv[0];
#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif
  if(argc==1) {
    fprintf(stderr, "Error: Do not start ogle_gui directly. Start ogle\n");
    exit(1);
  }
  msgqid = atoi(argv[2]);

  DNOTE("call init_interpret_config\n");
  init_interpret_config(program_name,
			add_keybinding,
			set_dvd_path);
  DNOTE("return init_interpret_config\n");

  DNOTE("call interpret_config\n");
  interpret_config();
  DNOTE("return interpret_config\n");
  
  gtk_init(&argc, &argv);

  // Initialize glade, and read in the glade file
  my_glade_setup();

  if(msgqid !=-1) { // ignore sending data.
    DVDResult_t res;
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen", res);
      exit(1);
    }
  }
  
  xsniff_init();
  
  audio_menu_new();
  subpicture_menu_new();
  
  app = get_glade_widget("app");
  gtk_widget_show(app);

  menu_new(app);  

  gtk_main ();
  return 0;
}

