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
#include <unistd.h>
#include <strings.h>
#include <gtk/gtk.h>
#include <ogle/dvdcontrol.h>

#include "angle.h"
#include "language.h"
#include "myintl.h"

extern DVDNav_t *nav;

static GtkWidget *menu;

void angle_menu_new(void) {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "angle_popup");
}

void angle_menu_show(GtkWidget *button) {
  angle_menu_clear();
  angle_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
}

void angle_menu_remove(void) {
  gtk_widget_destroy(menu);
}

void angle_menu_clear(void) {
  //Why, why, why
  angle_menu_remove();
  angle_menu_new();
}

void angle_menu_update(void) {
  DVDResult_t res;
  int StreamsAvailable;
  DVDAngle_t CurrentStream;

  GtkWidget *selecteditem;
  GtkWidget *menu_item;
  GSList *menu_group = NULL;

  int stream=1;
  
  res = DVDGetCurrentAngle(nav, &StreamsAvailable, &CurrentStream);
  if(res != DVD_E_Ok) {
    DVDPerror("angle.angle_menu_update: DVDGetCurrentAngle",
	      res);
    return;
  }

  while(stream <= StreamsAvailable) {
    char *label;
    
    /* The name of the angle choice in the angle popup menu */
    label = g_strdup_printf(_("Angle %d"), stream);
    menu_item = gtk_radio_menu_item_new_with_label(menu_group, label);
    g_free(label);
    menu_group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));

    if(stream == CurrentStream) {
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
      selecteditem = menu_item;
    } 
    gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                       angle_item_activate, GINT_TO_POINTER(stream));
    gtk_menu_append(GTK_MENU(menu), menu_item);

    stream++;
  }
  gtk_widget_show_all(menu);
}

void angle_item_activate( GtkRadioMenuItem *item,
			  gpointer         user_data) {
  int stream = GPOINTER_TO_INT(user_data);
  
  if(GTK_CHECK_MENU_ITEM(item)->active) {
    DVDAngleChange(nav, stream); 
  }
}



