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

#include "audio.h"
#include "language.h"
#include "myintl.h"

extern DVDNav_t *nav;
static GtkWidget *menu;

static const int OFF = 15;

char* guiDVDAudioFormat(DVDAudioFormat_t format) {
  switch(format) {
  case DVD_AUDIO_FORMAT_AC3:
    return "AC3";
  case DVD_AUDIO_FORMAT_MPEG1:
    return "MPEG1";
  case DVD_AUDIO_FORMAT_MPEG1_DRC:
    return "MPEG1 DRC";
  case DVD_AUDIO_FORMAT_MPEG2:
    return "MPEG2";
  case DVD_AUDIO_FORMAT_MPEG2_DRC:
    return "MPEG2 DRC";
  case DVD_AUDIO_FORMAT_LPCM:
    return "LPCM";
  case DVD_AUDIO_FORMAT_DTS:
    return "DTS";
  case DVD_AUDIO_FORMAT_SDDS:
    return "SDDS";
  case DVD_AUDIO_FORMAT_Other:
    return N_("Other");
  default:
    return N_("Unknown format");
  }
} 

void audio_menu_new(void) {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "audio_popup");
}

void audio_menu_show(GtkWidget *button) {
  audio_menu_clear();
  audio_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
}

void audio_menu_remove(void) {
  
  gtk_widget_destroy(menu);
}

void audio_menu_clear(void) {
  //Why, why, why
  audio_menu_remove();
  audio_menu_new();
}

void audio_menu_update(void) {
  DVDResult_t res;
  int StreamsAvailable;
  DVDStream_t CurrentStream;

  GtkWidget *selecteditem;
  GtkWidget *menu_item;

  int stream=0;

  char *strlangname;
  char *straudioformat;

  res = DVDGetCurrentAudio(nav, &StreamsAvailable, &CurrentStream);
  if(res != DVD_E_Ok) {
    DVDPerror("audio.audio_menu_create_new: DVDGetCurrentAudio", res);
    return;
  }

  selecteditem = NULL;

  while(stream < StreamsAvailable) {
    DVDBool_t Enabled;
    
    res = DVDIsAudioStreamEnabled(nav, (DVDStream_t)stream , &Enabled);
    if(res != DVD_E_Ok) {
      DVDPerror("audio.audio_menu_update: DVDIsAudioStreamEnabled", res);
      return;
    }

    if(Enabled) { 
      DVDAudioAttributes_t Attr;
      int stringlength;
      char *label;
      
      res = DVDGetAudioAttributes(nav, (DVDStream_t)stream , &Attr);
      if(res != DVD_E_Ok) {
	DVDPerror("audio.audio_menu_update: DVDGetAudioAttributes", res);
	return;
      }
      
      strlangname    = language_name(Attr.Language);
      straudioformat = _(guiDVDAudioFormat(Attr.AudioFormat));      
      stringlength = strlen(strlangname) + strlen(straudioformat) +10;

      label = (char*) malloc(stringlength  * sizeof(char));
      
      snprintf(label, stringlength-1, "%s (%s)",
	       strlangname, straudioformat);

      menu_item = gtk_check_menu_item_new_with_label(label);
      if(stream == CurrentStream) {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
        selecteditem = menu_item;
      } 
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                         audio_item_activate, GINT_TO_POINTER(stream));
      gtk_menu_append(GTK_MENU(menu), menu_item);

    }
    stream++;
  }
  
  menu_item = gtk_check_menu_item_new_with_label(_("None"));
  if (selecteditem == NULL) {
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
  }
  gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                     audio_item_activate, GINT_TO_POINTER(OFF));
  gtk_menu_prepend(GTK_MENU(menu), menu_item);

  gtk_widget_show_all(menu);
}


void audio_item_activate( GtkRadioMenuItem *item,
			  gpointer         user_data) {
  DVDResult_t res;
  int stream = GPOINTER_TO_INT(user_data);

  if(GTK_CHECK_MENU_ITEM(item)->active) {
    res = DVDAudioStreamChange(nav, stream);
    if(res != DVD_E_Ok) {
      DVDPerror("audio.audio_item_activate: DVDAudioChange", res);
      return;
    }
  }
}

