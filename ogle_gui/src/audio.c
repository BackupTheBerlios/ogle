#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <glib.h>
#include <gnome.h>
#include "audio.h"
#include <ogle/dvdcontrol.h>

extern DVDNav_t *nav;

static GtkWidget *menu;
static GtkWidget *button;
static GnomeUIInfo *menu_items_uiinfo;
static GSList *labellist = NULL;
static GnomeUIInfo infoend = GNOMEUIINFO_END;

char* language_name(DVDLangID_t lang) {
  return "bepa";
}


void audio_menu_new() {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "audio_popup");
}

void audio_menu_show(GtkWidget *button) {
  audio_menu_clear();
  audio_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  button, 0);
}

void audio_menu_remove() {
  
  gtk_widget_destroy(menu);
}

void audio_menu_clear() {
  if(labellist==NULL) {
    return;
  }

  //Why, why, why
  audio_menu_remove();
  audio_menu_new();

  g_slist_foreach(labellist, (GFunc)free, NULL); 
  g_slist_free(labellist);
  labellist = NULL;
}

void audio_menu_update() {
  DVDResult_t res;
  int StreamsAvailable;
  DVDStream_t CurrentStream;

  int stream=0;
  int pos=0;
  
  res = DVDGetCurrentAudio(nav, &StreamsAvailable, &CurrentStream);
  if(res != DVD_E_Ok) {
    DVDPerror("audio.audio_menu_create_new: DVDGetCurrentAudio", res);
    return;
  }

  menu_items_uiinfo = malloc( (StreamsAvailable +2) * sizeof(GnomeUIInfo));
  if(menu_items_uiinfo==NULL) {
    perror("audio.audio_menu_update");
    return;
  }

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
      
      stringlength = strlen(_(language_name(Attr.Language))) + 10;
      label = (char*) malloc(stringlength  * sizeof(char));
      
      // SV Svenska (AC3)
      //snprintf(label, stringlength-1, "%s, %s, (%s)", 
      //language_code(Attr.Language),
      //_(language_name(Attr.Language)),
      //DVDAudioFormat(Attr.AudioFormat) );
      
      snprintf(label, 9, "bepa %d", stream);
      labellist = g_slist_append (labellist, label);      

      {
	GnomeUIInfo tmp_info =  {
	  GNOME_APP_UI_ITEM, (gchar*)label,
	  NULL,
	  audio_item_activate, GINT_TO_POINTER(stream), NULL,
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

}
/*
	if(stream == CurrentStream) {
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM 
					(&menu_items_uiinfo[0].widget),
					TRUE);
      }

*/


void audio_item_activate( GtkButton       *button,
			  gpointer         user_data) {
  DVDResult_t res;
  int stream = GPOINTER_TO_INT(user_data);
  
  fprintf(stderr, "item %d\n", stream);
  
  res = DVDAudioStreamChange(nav, stream);
  if(res != DVD_E_Ok) {
    DVDPerror("audio.audio_item_activate: DVDAudioChange", res);
    return;
  }
  
}


