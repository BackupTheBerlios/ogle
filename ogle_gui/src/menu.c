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

#include <gtk/gtk.h>

#include <ogle/dvdcontrol.h>

#include "menu.h"
#include "callbacks.h"
#include "myintl.h"

extern DVDNav_t *nav;

static GtkWidget *menu;

struct item_s {
  GtkWidget *item;
  char name[20];
  int flag;
  void (*func) (GtkMenuItem *, gpointer);
};

struct item_s menuitem[] = {
  { NULL, N_("Chapter"),  UOP_FLAG_ChapterMenuCall, on_ptt_activate_pm       },
  { NULL, N_("Angle"),    UOP_FLAG_AngleMenuCall,   on_angle_activate_pm     },
  { NULL, N_("Audio"),    UOP_FLAG_AudioMenuCall,   on_audio_activate_pm     },
  { NULL, N_("Subtitle"), UOP_FLAG_SubPicMenuCall,  on_subpicture_activate_pm},
  { NULL, N_("Root"),     UOP_FLAG_RootMenuCall,    on_root_activate_pm      },
  { NULL, N_("Title"),    UOP_FLAG_TitleMenuCall,   on_title_activate_pm     },
  { NULL, N_("Resume"),   UOP_FLAG_Resume,          on_resume_activate_pm    },
  { NULL, "",             0,                        NULL                     }
};


void menu_new(GtkWidget *mainwin) {
  int i;
  
  if (menu != NULL)
    gtk_widget_destroy(menu);

  menu = gtk_menu_new();
  
  for(i=0; menuitem[i].flag != 0; i++) {
    menuitem[i].item = gtk_menu_item_new_with_label(_(menuitem[i].name));
    gtk_signal_connect(GTK_OBJECT(menuitem[i].item), "activate", 
                       menuitem[i].func, NULL);
    gtk_menu_append(GTK_MENU(menu), menuitem[i].item);
  }
  gtk_widget_show_all(menu);
}

void menu_show(GtkWidget *button) {
  menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 1, 0);
}

void menu_update(void) {
  // XXX Not implemented in player.
  DVDResult_t res;
  DVDUOP_t uop;
  int i=0;
  
  res = DVDGetCurrentUOPS(nav, &uop);
  if(res != DVD_E_Ok) {
    DVDPerror("menu.menu_update()", res);
    return;
  }
  
  for (i=0; menuitem[i].flag != 0; i++) {
    gboolean val = (uop & menuitem[i].flag) ? TRUE : FALSE;
    gtk_widget_set_sensitive(GTK_WIDGET(menuitem[i].item), val);
  }
}

