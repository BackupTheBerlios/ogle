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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <ogle/dvdcontrol.h>

#include "subpicture.h"
#include "language.h"
#include "myintl.h"

extern DVDNav_t *nav;

static GtkWidget *menu;

static const int OFF = 63;

void subpicture_menu_new(void) {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "subpicture_popup");
}

void subpicture_menu_show(GtkWidget *button) {
  subpicture_menu_clear();
  subpicture_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
}

void subpicture_menu_remove(void) {
  gtk_widget_destroy(menu);
}

void subpicture_menu_clear(void) {
  //Why, why, why
  subpicture_menu_remove();
  subpicture_menu_new();
}

void subpicture_menu_update(void) {
  DVDResult_t res;
  int StreamsAvailable;
  DVDStream_t CurrentStream;
  DVDBool_t Shown;

  GtkWidget *selecteditem;
  GtkWidget *menu_item;

  int stream=0;
  
  res = DVDGetCurrentSubpicture(nav, &StreamsAvailable, &CurrentStream,
				&Shown);
  if(res != DVD_E_Ok) {
    DVDPerror("subpicture.subpicture_menu_update: DVDGetCurrentSubpicture",
	      res);
    return;
  }

  while(stream < StreamsAvailable) {
    DVDBool_t Enabled;
    
    res = DVDIsSubpictureStreamEnabled(nav, (DVDStream_t)stream , &Enabled);
    if(res != DVD_E_Ok) {
      DVDPerror("subpicture.subpicture_menu_update: "
		"DVDIsSubpictureStreamEnabled", res);
      return;
    }

    if(Enabled) { 
      DVDSubpictureAttributes_t Attr;
      int stringlength;
      char *label;
      
      res = DVDGetSubpictureAttributes(nav, (DVDStream_t)stream , &Attr);
      if(res != DVD_E_Ok) {
	DVDPerror("subpicture.subpicture_menu_create_new: DVDGetSubpictureAttributes", res);
	return;
      }
      
      stringlength = strlen(language_name(Attr.Language)) + 10;
      label = (char*) malloc(stringlength  * sizeof(char));
      
      snprintf(label, stringlength-1, "%s", 
	       language_name(Attr.Language));
      //DVDSubpictureFormat(Attr.SubpictureFormat) );
      
      menu_item = gtk_check_menu_item_new_with_label(label);
      if((stream == CurrentStream) && Shown) {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
        selecteditem = menu_item;
      } 
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                         subpicture_item_activate, GINT_TO_POINTER(stream));
      gtk_menu_append(GTK_MENU(menu), menu_item);
    }
    stream++;
  }
  menu_item = gtk_check_menu_item_new_with_label(_("None"));
  if (selecteditem == NULL) {
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                     subpicture_item_activate, GINT_TO_POINTER(OFF));
  gtk_menu_prepend(GTK_MENU(menu), menu_item);

  gtk_widget_show_all(menu);
}

void subpicture_item_activate( GtkRadioMenuItem *item,
			       gpointer         user_data) {
  DVDResult_t res;
  int stream = GPOINTER_TO_INT(user_data);
  
  if(GTK_CHECK_MENU_ITEM(item)->active) {
    if(stream==OFF) {
      fprintf(stderr, "Subpicture: OFF\n");
      res = DVDSetSubpictureState(nav, DVDFalse);
      if(res != DVD_E_Ok) {
	DVDPerror("subpicture.subpicture_item_activate: DVDSubpictureState",
		  res);
      }
    } else {
      fprintf(stderr, "Subpicture: ON\n");
      res = DVDSetSubpictureState(nav, DVDTrue);
      if(res != DVD_E_Ok) {
	DVDPerror("subpicture.subpicture_item_activate: DVDSubpictureState",
		  res);
      }
      res = DVDSubpictureStreamChange(nav, stream);
      if(res != DVD_E_Ok) {
	DVDPerror("subpicture.subpicture_item_activate: DVDSubpictureChange",
		  res);
	return;
      }
    }
  }
}



