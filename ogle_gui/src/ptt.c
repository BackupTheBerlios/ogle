/* Ogle - A video player
 * Copyright (C) 2002 Martin Norbäck
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

#include "ptt.h"
#include "myintl.h"
#include "callbacks.h"

extern DVDNav_t *nav;

static GtkWidget *menu;

void ptt_menu_new(void) {
  menu = gtk_menu_new ();
}

void ptt_menu_show(GtkWidget *button) {
  ptt_menu_clear();
  ptt_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
}

void ptt_menu_remove(void) {
  if (menu != NULL) {
    gtk_widget_destroy(menu);
  }
}

void ptt_menu_clear(void) {
  ptt_menu_remove();
  ptt_menu_new();
}

void ptt_menu_update(void) {
  DVDResult_t res;
  int i;
  int num_titles;
  int num_ptts;

  menu = gtk_menu_new();

  res = DVDGetTitles(nav, &num_titles);
  if(res != DVD_E_Ok) {
    DVDPerror("ptt_menu_update: DVDGettPTTsForTitle(1)", res);
    return;
  } 
  for (i=1; i <= num_titles; i++) {
    int j;
    GtkWidget *menu_item;
    GtkWidget *submenu;
    GtkWidget *submenu_item;
    char* label;

    res = DVDGetPTTsForTitle(nav, i, &num_ptts);
    if(res != DVD_E_Ok) {
      DVDPerror("ptt_menu_update: DVDGettPTTsForTitle(1)", res);
      return;
    } 

    submenu = gtk_menu_new ();

    label = g_strdup_printf(_("Title %i"), i);
    menu_item = gtk_menu_item_new_with_label(label);
    g_free(label);
  
    for (j=1; j<=num_ptts; j++) {

      label = g_strdup_printf(_("Chapter %i"), j);
      submenu_item = gtk_menu_item_new_with_label(label);
      g_free(label);

      gtk_signal_connect(GTK_OBJECT(submenu_item), "activate",
                         on_jump_to_ptt_activate, GINT_TO_POINTER(i*256+j));
      gtk_menu_append(GTK_MENU(submenu), submenu_item);
    }
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    gtk_menu_append(GTK_MENU(menu), menu_item);
  }

  gtk_widget_show_all(menu);
}
