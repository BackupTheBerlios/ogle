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

#include <gnome.h>

#include <ogle/dvdcontrol.h>

#include "menu.h"
#include "interface.h"
#include "support.h"

extern DVDNav_t *nav;

static GtkWidget *menu;
static GtkWidget *button;

struct item_s {
  GtkWidget *item;
  char name[20];
  int flag;
};

struct item_s menuitem[] = {
  { NULL, "ptt_pm",         UOP_FLAG_ChapterMenuCall  },
  { NULL, "angle_pm",       UOP_FLAG_AngleMenuCall    },
  { NULL, "audio_pm",       UOP_FLAG_AudioMenuCall    },
  { NULL, "subpicture_pm",  UOP_FLAG_SubPicMenuCall   },
  { NULL, "root_pm",        UOP_FLAG_RootMenuCall     },
  { NULL, "title_pm",       UOP_FLAG_TitleMenuCall    },
  { NULL, "resume_pm",      UOP_FLAG_Resume           },
  { NULL, "",               0                         },
};


void menu_new(GtkWidget *mainwin) {
  int i;
  
  menu = create_menus_popup();
  
  i=0;
  while(menuitem[i].flag != 0) {
    menuitem[i].item = lookup_widget(menu, menuitem[i].name);
    i++;
  }
  
}

void menu_show(GtkWidget *button) {
  menu_update();
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  button, 0);
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
  
  i=0;
  while(menuitem[i].flag != 0) {
    gboolean val = (uop & menuitem[i].flag) ? TRUE : FALSE;
    //gboolean val = TRUE;
    gtk_widget_set_sensitive(GTK_WIDGET(menuitem[i].item), val);
    i++;
  }
}




