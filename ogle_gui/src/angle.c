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
#include <gnome.h>
#include "angle.h"
#include "language.h"
#include <ogle/dvdcontrol.h>

extern DVDNav_t *nav;

static GtkWidget *menu;
static GnomeUIInfo *menu_items_uiinfo;
static GSList *labellist = NULL;
static GnomeUIInfo infoend = GNOMEUIINFO_END;

void angle_menu_new(void) {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "angle_popup");
}

void angle_menu_show(GtkWidget *button) {
  angle_menu_clear();
  angle_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  button, 0);
}

void angle_menu_remove(void) {
  gtk_widget_destroy(menu);
}

void angle_menu_clear(void) {
  //Why, why, why
  angle_menu_remove();
  angle_menu_new();
  if(labellist!=NULL) {
    g_slist_foreach(labellist, (GFunc)free, NULL); 
    g_slist_free(labellist);
    labellist = NULL;
  }
}

void angle_menu_update(void) {
  DVDResult_t res;
  int StreamsAvailable;
  DVDAngle_t CurrentStream;

  GtkWidget *selecteditem;
  int selectedpos=0;  

  int stream=1;
  int pos=0;
  
  res = DVDGetCurrentAngle(nav, &StreamsAvailable, &CurrentStream);
  if(res != DVD_E_Ok) {
    DVDPerror("angle.angle_menu_update: DVDGetCurrentAngle",
	      res);
    return;
  }

  menu_items_uiinfo = malloc( (StreamsAvailable +2) * sizeof(GnomeUIInfo));
  if(menu_items_uiinfo==NULL) {
    perror("angle.angle_menu_update");
    return;
  }

  while(stream <= StreamsAvailable) {
    int stringlength;
    char *label;
    
    stringlength = strlen(_("angle"))+5;
    label = (char*) malloc(stringlength  * sizeof(char));
    
    snprintf(label, stringlength-1, "%s %d", _("angle"), (int)stream);
    
    labellist = g_slist_append (labellist, label);      
    
    if( stream == CurrentStream) {
      selectedpos = pos;
    }
    
    {
      GnomeUIInfo tmp_info =  {
	GNOME_APP_UI_ITEM, (gchar*)label,
	NULL,
	angle_item_activate, GINT_TO_POINTER(stream), NULL,
	GNOME_APP_PIXMAP_NONE, NULL,
	0, 0, NULL
      };
      menu_items_uiinfo[pos++] = tmp_info;
    }	
    stream++;
  }



  menu_items_uiinfo[pos++] = infoend;
  
  {
    GnomeUIInfo menu_uiinfo[] =
    {
      {
	GNOME_APP_UI_RADIOITEMS, NULL, NULL, menu_items_uiinfo,
	NULL, NULL, 0, NULL, 0, 0, NULL
      },
      GNOMEUIINFO_END
    };
    
    // Insert
    gnome_app_fill_menu (GTK_MENU_SHELL (menu), menu_uiinfo,
			 NULL, FALSE, 0);
  }

  selecteditem = g_list_nth_data(GTK_MENU_SHELL(menu)->children, selectedpos);
  gtk_signal_handler_block_by_func(GTK_OBJECT(selecteditem),
				   angle_item_activate, 
				   GINT_TO_POINTER( (CurrentStream)));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(selecteditem), TRUE);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(selecteditem), 
				     angle_item_activate,
				     GINT_TO_POINTER((CurrentStream)));
  

}

void angle_item_activate( GtkRadioMenuItem *item,
			  gpointer         user_data) {
  DVDResult_t res;
  int stream = GPOINTER_TO_INT(user_data);
  
  if(GTK_CHECK_MENU_ITEM(item)->active) {
    DVDAngleChange(nav, stream); 
  }
}



