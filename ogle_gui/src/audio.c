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
static GnomeUIInfo *menu_items_uiinfo;
static GSList *labellist = NULL;
static GnomeUIInfo infoend = GNOMEUIINFO_END;

char* language_name(DVDLangID_t lang) {
  return "bepa";
}


void audio_menu_new(void) {
  menu = gtk_menu_new ();
  gtk_widget_set_name (menu, "audio_popup");
}

void audio_menu_show(GtkWidget *button) {
  audio_menu_clear();
  audio_menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  button, 0);
}

void audio_menu_remove(void) {
  
  gtk_widget_destroy(menu);
}

void audio_menu_clear(void) {
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

void audio_menu_update(void) {
  DVDResult_t res;
  int StreamsAvailable;
  DVDStream_t CurrentStream;

  GtkWidget *selecteditem;
  int selectedpos=0;  

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

  {
    GnomeUIInfo tmp_info =  {
      GNOME_APP_UI_ITEM, (gchar*) N_("None"),
      NULL,
      audio_item_activate, GINT_TO_POINTER(15), NULL,
      GNOME_APP_PIXMAP_NONE, NULL,
      0, 0, NULL
    };
    menu_items_uiinfo[pos++] = tmp_info;
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

      if(stream == CurrentStream) {
	selectedpos = pos;
      }

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
    
    gnome_app_fill_menu (GTK_MENU_SHELL (menu), menu_uiinfo,
			 NULL, FALSE, 0);
  }
  
  selecteditem = g_list_nth_data(GTK_MENU_SHELL(menu)->children, selectedpos);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(selecteditem), TRUE);
}


void audio_item_activate( GtkRadioMenuItem *item,
			  gpointer         user_data) {
  DVDResult_t res;
  int stream = GPOINTER_TO_INT(user_data);

  if(GTK_CHECK_MENU_ITEM(item)->active) {
    fprintf(stderr, "item %d\n", stream);
    
    res = DVDAudioStreamChange(nav, stream);
    if(res != DVD_E_Ok) {
      DVDPerror("audio.audio_item_activate: DVDAudioChange", res);
      return;
    }
  }
}


