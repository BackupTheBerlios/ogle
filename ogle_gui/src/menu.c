#include <stdio.h>
#include <stdlib.h>

#include <gnome.h>

#include <dvdcontrol.h>

#include "menu.h"
#include "interface.h"
#include "support.h"


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

void menu_update() {
  // XXX Not implemented in player.
  DVDResult_t res;
  DVDUOP_t uop;
  int i=0;
  
  res = DVDGetCurrentUOPS(&uop);
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




