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
#include "subpicture.h"
#include "language.h"
#include <ogle/dvdcontrol.h>

extern DVDNav_t *nav;

static GtkWidget *menu;
static GnomeUIInfo *menu_items_uiinfo;
static GSList *labellist = NULL;
static GnomeUIInfo infoend = GNOMEUIINFO_END;

static const int OFF = 63;

void subpicture_menu_new(void) {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "subpicture_popup");
}

void subpicture_menu_show(GtkWidget *button) {
  subpicture_menu_clear();
  subpicture_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  button, 0);
}

void subpicture_menu_remove(void) {
  gtk_widget_destroy(menu);
}

void subpicture_menu_clear(void) {
  if(labellist==NULL) {
    return;
  }

  //Why, why, why
  subpicture_menu_remove();
  subpicture_menu_new();

  g_slist_foreach(labellist, (GFunc)free, NULL); 
  g_slist_free(labellist);
  labellist = NULL;
}

void subpicture_menu_update(void) {
  DVDResult_t res;
  int StreamsAvailable;
  DVDStream_t CurrentStream;
  DVDBool_t Shown;

  GtkWidget *selecteditem;
  int selectedpos=0;  

  int stream=0;
  int pos=0;
  
  res = DVDGetCurrentSubpicture(nav, &StreamsAvailable, &CurrentStream,
				&Shown);
  if(res != DVD_E_Ok) {
    DVDPerror("subpicture.subpicture_menu_update: DVDGetCurrentSubpicture",
	      res);
    return;
  }

  menu_items_uiinfo = malloc( (StreamsAvailable +2) * sizeof(GnomeUIInfo));
  if(menu_items_uiinfo==NULL) {
    perror("subpicture.subpicture_menu_update");
    return;
  }

  {
    GnomeUIInfo tmp_info =  {
      GNOME_APP_UI_ITEM, (gchar*) N_("None"),
      NULL,
      subpicture_item_activate, GINT_TO_POINTER(OFF), NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    };
    menu_items_uiinfo[pos++] = tmp_info;
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
      
      labellist = g_slist_append (labellist, label);      
      
      if( (stream == CurrentStream) && Shown) {
	selectedpos = pos;
      }
      
      {
	GnomeUIInfo tmp_info =  {
	  GNOME_APP_UI_ITEM, (gchar*)label,
	  NULL,
	  subpicture_item_activate, GINT_TO_POINTER(stream), NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  0, 0, NULL
	};
	menu_items_uiinfo[pos++] = tmp_info;
      }	
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
				   subpicture_item_activate, 
				   GINT_TO_POINTER( (Shown) ?
						    CurrentStream : OFF));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(selecteditem), TRUE);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(selecteditem), 
				     subpicture_item_activate,
				     GINT_TO_POINTER((Shown) ?
						     CurrentStream : OFF));
  

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



