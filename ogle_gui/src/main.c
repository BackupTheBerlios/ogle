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

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <ogle/dvdcontrol.h>
#include "xsniffer.h"

#include "menu.h"
#include "audio.h"
#include "subpicture.h"

#define OGLE_GLADE_FILE PACKAGE_PIXMAPS_DIR "ogle_gui.glade"

DVDNav_t *nav;

int msgqid;
GladeXML *xml;

ZoomMode_t zoom_mode = ZoomModeResizeAllowed;
GtkWidget *app;

int
main (int argc, char *argv[])
{
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

  gtk_init(&argc, &argv);
  glade_init();

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
  
  xml = glade_xml_new(OGLE_GLADE_FILE, NULL);
  glade_xml_signal_autoconnect(xml);
  app = glade_xml_get_widget(xml, "app");
  gtk_widget_show(app);

  menu_new(app);  

  gtk_main ();
  return 0;
}

