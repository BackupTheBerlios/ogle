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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

static GnomeUIInfo menus_popup_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_PTT"),
    N_("Part of title"),
    on_ptt_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("A_ngle"),
    NULL,
    on_angle_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Audio"),
    NULL,
    on_audio_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Subpicture"),
    NULL,
    on_subpicture_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Root"),
    NULL,
    on_root_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Title"),
    NULL,
    on_title_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Resume"),
    NULL,
    on_resume_activate_pm, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  GNOMEUIINFO_END
};

GtkWidget*
create_menus_popup (void)
{
  GtkWidget *menus_popup;

  menus_popup = gtk_menu_new ();
  gtk_widget_set_name (menus_popup, "menus_popup");
  gtk_object_set_data (GTK_OBJECT (menus_popup), "menus_popup", menus_popup);
  gnome_app_fill_menu (GTK_MENU_SHELL (menus_popup), menus_popup_uiinfo,
                       NULL, FALSE, 0);

  gtk_widget_set_name (menus_popup_uiinfo[0].widget, "ptt_pm");
  gtk_widget_ref (menus_popup_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "ptt_pm",
                            menus_popup_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menus_popup_uiinfo[1].widget, "angle_pm");
  gtk_widget_ref (menus_popup_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "angle_pm",
                            menus_popup_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menus_popup_uiinfo[2].widget, "audio_pm");
  gtk_widget_ref (menus_popup_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "audio_pm",
                            menus_popup_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menus_popup_uiinfo[3].widget, "subpicture_pm");
  gtk_widget_ref (menus_popup_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "subpicture_pm",
                            menus_popup_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menus_popup_uiinfo[4].widget, "root_pm");
  gtk_widget_ref (menus_popup_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "root_pm",
                            menus_popup_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menus_popup_uiinfo[5].widget, "title_pm");
  gtk_widget_ref (menus_popup_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "title_pm",
                            menus_popup_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menus_popup_uiinfo[6].widget, "resume_pm");
  gtk_widget_ref (menus_popup_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (menus_popup), "resume_pm",
                            menus_popup_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  return menus_popup;
}


static GnomeUIInfo file_menu_uiinfo[] =
{
  GNOMEUIINFO_MENU_OPEN_ITEM (on_open_activate, NULL),
  GNOMEUIINFO_ITEM_STOCK     ("Open", "Opens the device directly.",
			      on_opendvd_activate, GNOME_STOCK_PIXMAP_CDROM),
  //GNOMEUIINFO_MENU_OPEN_ITEM (on_open_activate, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_EXIT_ITEM (on_exit_activate, NULL),
  GNOMEUIINFO_END
};

static char* file_menu_uiinfo_string[] = {
  "open", 
  "open_dvd", 
  "separator2", 
  "exit",
  ""
};

static GnomeUIInfo edit_menu_uiinfo[] =
{
  /*
  GNOMEUIINFO_MENU_CUT_ITEM (on_cut_activate, NULL),
  GNOMEUIINFO_MENU_COPY_ITEM (on_copy_activate, NULL),
  GNOMEUIINFO_MENU_PASTE_ITEM (on_paste_activate, NULL),
  GNOMEUIINFO_MENU_CLEAR_ITEM (on_clear_activate, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_PROPERTIES_ITEM (on_properties_activate, NULL),
  */
  GNOMEUIINFO_END
};

static GnomeUIInfo view_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Full screen"),
    NULL,
    on_full_screen_activate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_f, GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo settings_menu_uiinfo[] =
{
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (on_preferences_activate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo help_menu_uiinfo[] =
{
  GNOMEUIINFO_HELP ("ogle"),
  GNOMEUIINFO_MENU_ABOUT_ITEM (on_about_activate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_uiinfo[] =
{
  GNOMEUIINFO_MENU_FILE_TREE (file_menu_uiinfo),
  GNOMEUIINFO_MENU_EDIT_TREE (edit_menu_uiinfo),
  GNOMEUIINFO_MENU_VIEW_TREE (view_menu_uiinfo),
  GNOMEUIINFO_MENU_SETTINGS_TREE (settings_menu_uiinfo),
  GNOMEUIINFO_MENU_HELP_TREE (help_menu_uiinfo),
  GNOMEUIINFO_END
};

GtkWidget*
create_app (void)
{
  GtkWidget *app;
  GtkWidget *dock1;
  GtkWidget *vbox1;
  GtkWidget *hscale1;
  GtkWidget *hbox1;
  GtkWidget *table1;
  GtkWidget *stop_button;
  GtkWidget *stop_pixmap;
  GtkWidget *pause_button;
  GtkWidget *pause_pixmap;
  GtkWidget *rewind_button;
  GtkWidget *rewind_pixmap;
  GtkWidget *skip_backwards_button;
  GtkWidget *skip_backwards_pixmap;
  GtkWidget *reverse_button;
  GtkWidget *reverse_pixmap;
  GtkWidget *step_reverse_button;
  GtkWidget *step_reverse_pixmap;
  GtkWidget *play_button;
  GtkWidget *play_pixmap;
  GtkWidget *step_forwards_button;
  GtkWidget *step_forwards_pixmap;
  GtkWidget *fastforward_button;
  GtkWidget *fastforward_pixmap;
  GtkWidget *skip_forwards_button;
  GtkWidget *skip_forwards_pixmap;
  GtkWidget *table2;
  GtkWidget *fast_button;
  GtkWidget *fast_pixmap;
  GtkWidget *slow_button;
  GtkWidget *slow_pixmap;
  GtkWidget *table3;
  GtkWidget *cursor_up_button;
  GtkWidget *cursor_up_pixmap;
  GtkWidget *cursor_left_button;
  GtkWidget *cursor_left_pixmap;
  GtkWidget *cursor_right_button;
  GtkWidget *cursor_right_pixmap;
  GtkWidget *cursor_go_up_button;
  GtkWidget *cursor_go_up_pixmap;
  GtkWidget *cursor_down_button;
  GtkWidget *cursor_down_pixmap;
  GtkWidget *cursor_enter_button;
  GtkWidget *cursor_enter_pixmap;
  GtkWidget *table4;
  GtkWidget *menus_button;
  GtkWidget *menus_pixmap;
  GtkWidget *audio_button;
  GtkWidget *audio_pixmap;
  GtkWidget *subpicture_button;
  GtkWidget *subpicture_pixmap;
  GtkWidget *angle_button;
  GtkWidget *angle_pixmap;
  GtkWidget *appbar;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  app = gnome_app_new ("Ogle", _("Ogle"));
  gtk_widget_set_name (app, "app");
  gtk_object_set_data (GTK_OBJECT (app), "app", app);

  dock1 = GNOME_APP (app)->dock;
  gtk_widget_set_name (dock1, "dock1");
  gtk_widget_ref (dock1);
  gtk_object_set_data_full (GTK_OBJECT (app), "dock1", dock1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (dock1);

  gnome_app_create_menus (GNOME_APP (app), menubar_uiinfo);

  gtk_widget_set_name (menubar_uiinfo[0].widget, "file");
  gtk_widget_ref (menubar_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "file",
                            menubar_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  { 
    int  i=0;
    while(file_menu_uiinfo_string[i][0] != '\0') {
      gtk_widget_set_name (file_menu_uiinfo[0].widget, file_menu_uiinfo_string[i]);
      gtk_widget_ref (file_menu_uiinfo[0].widget);
      gtk_object_set_data_full (GTK_OBJECT (app), file_menu_uiinfo_string[i],
				file_menu_uiinfo[0].widget,
				(GtkDestroyNotify) gtk_widget_unref);
      i++;
    }
  }
  /*
  gtk_widget_set_name (menubar_uiinfo[1].widget, "edit");
  gtk_widget_ref (menubar_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "edit",
                            menubar_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (edit_menu_uiinfo[0].widget, "cut");
  gtk_widget_ref (edit_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "cut",
                            edit_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (edit_menu_uiinfo[1].widget, "copy");
  gtk_widget_ref (edit_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "copy",
                            edit_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (edit_menu_uiinfo[2].widget, "paste");
  gtk_widget_ref (edit_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "paste",
                            edit_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (edit_menu_uiinfo[3].widget, "clear");
  gtk_widget_ref (edit_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "clear",
                            edit_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (edit_menu_uiinfo[4].widget, "separator3");
  gtk_widget_ref (edit_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "separator3",
                            edit_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (edit_menu_uiinfo[5].widget, "properties");
  gtk_widget_ref (edit_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "properties",
                            edit_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  */
  gtk_widget_set_name (menubar_uiinfo[2].widget, "view");
  gtk_widget_ref (menubar_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "view",
                            menubar_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (view_menu_uiinfo[0].widget, "full_screen");
  gtk_widget_ref (view_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "full_screen",
                            view_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  
  gtk_widget_set_name (menubar_uiinfo[3].widget, "settings");
  gtk_widget_ref (menubar_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "settings",
                            menubar_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (settings_menu_uiinfo[0].widget, "preferences");
  gtk_widget_ref (settings_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "preferences",
                            settings_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (menubar_uiinfo[4].widget, "help");
  gtk_widget_ref (menubar_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "help",
                            menubar_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_set_name (help_menu_uiinfo[1].widget, "about");
  gtk_widget_ref (help_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (app), "about",
                            help_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox1, "vbox1");
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (app), "vbox1", vbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gnome_app_set_contents (GNOME_APP (app), vbox1);

  hscale1 = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 0, 0, 0)));
  gtk_widget_set_name (hscale1, "hscale1");
  gtk_widget_ref (hscale1);
  gtk_object_set_data_full (GTK_OBJECT (app), "hscale1", hscale1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hscale1);
  gtk_box_pack_start (GTK_BOX (vbox1), hscale1, FALSE, FALSE, 0);
  gtk_scale_set_draw_value (GTK_SCALE (hscale1), FALSE);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox1, "hbox1");
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (app), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 7);

  table1 = gtk_table_new (2, 5, FALSE);
  gtk_widget_set_name (table1, "table1");
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (app), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (hbox1), table1, TRUE, TRUE, 5);

  stop_button = gtk_button_new ();
  gtk_widget_set_name (stop_button, "stop_button");
  gtk_widget_ref (stop_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "stop_button", stop_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (stop_button);
  gtk_table_attach (GTK_TABLE (table1), stop_button, 2, 3, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, stop_button, _("Stop"), NULL);

  stop_pixmap = create_pixmap (app, "ogle_gui/stock_stop.xpm", FALSE);
  gtk_widget_set_name (stop_pixmap, "stop_pixmap");
  gtk_widget_ref (stop_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "stop_pixmap", stop_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (stop_pixmap);
  gtk_container_add (GTK_CONTAINER (stop_button), stop_pixmap);

  pause_button = gtk_button_new ();
  gtk_widget_set_name (pause_button, "pause_button");
  gtk_widget_ref (pause_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "pause_button", pause_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pause_button);
  gtk_table_attach (GTK_TABLE (table1), pause_button, 2, 3, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, pause_button, _("Pause"), NULL);

  pause_pixmap = create_pixmap (app, "ogle_gui/stock_pause.xpm", FALSE);
  gtk_widget_set_name (pause_pixmap, "pause_pixmap");
  gtk_widget_ref (pause_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "pause_pixmap", pause_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pause_pixmap);
  gtk_container_add (GTK_CONTAINER (pause_button), pause_pixmap);

  rewind_button = gtk_button_new ();
  gtk_widget_set_name (rewind_button, "rewind_button");
  gtk_widget_ref (rewind_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "rewind_button", rewind_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (rewind_button);
  gtk_table_attach (GTK_TABLE (table1), rewind_button, 0, 1, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, rewind_button, _("Rewind"), NULL);

  rewind_pixmap = create_pixmap (app, "ogle_gui/rewind.xpm", FALSE);
  gtk_widget_set_name (rewind_pixmap, "rewind_pixmap");
  gtk_widget_ref (rewind_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "rewind_pixmap", rewind_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (rewind_pixmap);
  gtk_container_add (GTK_CONTAINER (rewind_button), rewind_pixmap);

  skip_backwards_button = gtk_button_new ();
  gtk_widget_set_name (skip_backwards_button, "skip_backwards_button");
  gtk_widget_ref (skip_backwards_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "skip_backwards_button", skip_backwards_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (skip_backwards_button);
  gtk_table_attach (GTK_TABLE (table1), skip_backwards_button, 0, 1, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, skip_backwards_button, _("Skip backwards"), NULL);

  skip_backwards_pixmap = create_pixmap (app, "ogle_gui/skip_backwards.xpm", FALSE);
  gtk_widget_set_name (skip_backwards_pixmap, "skip_backwards_pixmap");
  gtk_widget_ref (skip_backwards_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "skip_backwards_pixmap", skip_backwards_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (skip_backwards_pixmap);
  gtk_container_add (GTK_CONTAINER (skip_backwards_button), skip_backwards_pixmap);

  reverse_button = gtk_button_new ();
  gtk_widget_set_name (reverse_button, "reverse_button");
  gtk_widget_ref (reverse_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "reverse_button", reverse_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (reverse_button);
  gtk_table_attach (GTK_TABLE (table1), reverse_button, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, reverse_button, _("Reverse"), NULL);

  reverse_pixmap = create_pixmap (app, "ogle_gui/stock_left_arrow.xpm", FALSE);
  gtk_widget_set_name (reverse_pixmap, "reverse_pixmap");
  gtk_widget_ref (reverse_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "reverse_pixmap", reverse_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (reverse_pixmap);
  gtk_container_add (GTK_CONTAINER (reverse_button), reverse_pixmap);

  step_reverse_button = gtk_button_new ();
  gtk_widget_set_name (step_reverse_button, "step_reverse_button");
  gtk_widget_ref (step_reverse_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "step_reverse_button", step_reverse_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (step_reverse_button);
  gtk_table_attach (GTK_TABLE (table1), step_reverse_button, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, step_reverse_button, _("Step reverse"), NULL);

  step_reverse_pixmap = create_pixmap (app, "ogle_gui/stock_first.xpm", FALSE);
  gtk_widget_set_name (step_reverse_pixmap, "step_reverse_pixmap");
  gtk_widget_ref (step_reverse_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "step_reverse_pixmap", step_reverse_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (step_reverse_pixmap);
  gtk_container_add (GTK_CONTAINER (step_reverse_button), step_reverse_pixmap);

  play_button = gtk_button_new ();
  gtk_widget_set_name (play_button, "play_button");
  gtk_widget_ref (play_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "play_button", play_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (play_button);
  gtk_table_attach (GTK_TABLE (table1), play_button, 3, 4, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, play_button, _("Play"), NULL);

  play_pixmap = create_pixmap (app, "ogle_gui/stock_right_arrow.xpm", FALSE);
  gtk_widget_set_name (play_pixmap, "play_pixmap");
  gtk_widget_ref (play_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "play_pixmap", play_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (play_pixmap);
  gtk_container_add (GTK_CONTAINER (play_button), play_pixmap);

  step_forwards_button = gtk_button_new ();
  gtk_widget_set_name (step_forwards_button, "step_forwards_button");
  gtk_widget_ref (step_forwards_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "step_forwards_button", step_forwards_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (step_forwards_button);
  gtk_table_attach (GTK_TABLE (table1), step_forwards_button, 3, 4, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, step_forwards_button, _("Step forwards"), NULL);

  step_forwards_pixmap = create_pixmap (app, "ogle_gui/stock_last.xpm", FALSE);
  gtk_widget_set_name (step_forwards_pixmap, "step_forwards_pixmap");
  gtk_widget_ref (step_forwards_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "step_forwards_pixmap", step_forwards_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (step_forwards_pixmap);
  gtk_container_add (GTK_CONTAINER (step_forwards_button), step_forwards_pixmap);

  fastforward_button = gtk_button_new ();
  gtk_widget_set_name (fastforward_button, "fastforward_button");
  gtk_widget_ref (fastforward_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "fastforward_button", fastforward_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fastforward_button);
  gtk_table_attach (GTK_TABLE (table1), fastforward_button, 4, 5, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, fastforward_button, _("Fast forward"), NULL);

  fastforward_pixmap = create_pixmap (app, "ogle_gui/fastforward.xpm", FALSE);
  gtk_widget_set_name (fastforward_pixmap, "fastforward_pixmap");
  gtk_widget_ref (fastforward_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "fastforward_pixmap", fastforward_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fastforward_pixmap);
  gtk_container_add (GTK_CONTAINER (fastforward_button), fastforward_pixmap);

  skip_forwards_button = gtk_button_new ();
  gtk_widget_set_name (skip_forwards_button, "skip_forwards_button");
  gtk_widget_ref (skip_forwards_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "skip_forwards_button", skip_forwards_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (skip_forwards_button);
  gtk_table_attach (GTK_TABLE (table1), skip_forwards_button, 4, 5, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, skip_forwards_button, _("Skip forwards"), NULL);

  skip_forwards_pixmap = create_pixmap (app, "ogle_gui/skip_forwards.xpm", FALSE);
  gtk_widget_set_name (skip_forwards_pixmap, "skip_forwards_pixmap");
  gtk_widget_ref (skip_forwards_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "skip_forwards_pixmap", skip_forwards_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (skip_forwards_pixmap);
  gtk_container_add (GTK_CONTAINER (skip_forwards_button), skip_forwards_pixmap);

  table2 = gtk_table_new (2, 1, FALSE);
  gtk_widget_set_name (table2, "table2");
  gtk_widget_ref (table2);
  gtk_object_set_data_full (GTK_OBJECT (app), "table2", table2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table2);
  gtk_box_pack_start (GTK_BOX (hbox1), table2, TRUE, TRUE, 0);

  fast_button = gtk_button_new ();
  gtk_widget_set_name (fast_button, "fast_button");
  gtk_widget_ref (fast_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "fast_button", fast_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fast_button);
  gtk_table_attach (GTK_TABLE (table2), fast_button, 0, 1, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, fast_button, _("Play fast"), NULL);

  fast_pixmap = create_pixmap (app, "ogle_gui/stock_timer.xpm", FALSE);
  gtk_widget_set_name (fast_pixmap, "fast_pixmap");
  gtk_widget_ref (fast_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "fast_pixmap", fast_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fast_pixmap);
  gtk_container_add (GTK_CONTAINER (fast_button), fast_pixmap);

  slow_button = gtk_button_new ();
  gtk_widget_set_name (slow_button, "slow_button");
  gtk_widget_ref (slow_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "slow_button", slow_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (slow_button);
  gtk_table_attach (GTK_TABLE (table2), slow_button, 0, 1, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, slow_button, _("Play slow"), NULL);

  slow_pixmap = create_pixmap (app, "ogle_gui/stock_timer_stopped.xpm", FALSE);
  gtk_widget_set_name (slow_pixmap, "slow_pixmap");
  gtk_widget_ref (slow_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "slow_pixmap", slow_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (slow_pixmap);
  gtk_container_add (GTK_CONTAINER (slow_button), slow_pixmap);

  table3 = gtk_table_new (2, 3, FALSE);
  gtk_widget_set_name (table3, "table3");
  gtk_widget_ref (table3);
  gtk_object_set_data_full (GTK_OBJECT (app), "table3", table3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table3);
  gtk_box_pack_start (GTK_BOX (hbox1), table3, TRUE, TRUE, 5);

  cursor_up_button = gtk_button_new ();
  gtk_widget_set_name (cursor_up_button, "cursor_up_button");
  gtk_widget_ref (cursor_up_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_up_button", cursor_up_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_up_button);
  gtk_table_attach (GTK_TABLE (table3), cursor_up_button, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, cursor_up_button, _("Move cursor up"), NULL);

  cursor_up_pixmap = create_pixmap (app, "ogle_gui/stock_up_arrow.xpm", FALSE);
  gtk_widget_set_name (cursor_up_pixmap, "cursor_up_pixmap");
  gtk_widget_ref (cursor_up_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_up_pixmap", cursor_up_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_up_pixmap);
  gtk_container_add (GTK_CONTAINER (cursor_up_button), cursor_up_pixmap);

  cursor_left_button = gtk_button_new ();
  gtk_widget_set_name (cursor_left_button, "cursor_left_button");
  gtk_widget_ref (cursor_left_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_left_button", cursor_left_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_left_button);
  gtk_table_attach (GTK_TABLE (table3), cursor_left_button, 0, 1, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, cursor_left_button, _("Move cursor left"), NULL);

  cursor_left_pixmap = create_pixmap (app, "ogle_gui/stock_left_arrow.xpm", FALSE);
  gtk_widget_set_name (cursor_left_pixmap, "cursor_left_pixmap");
  gtk_widget_ref (cursor_left_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_left_pixmap", cursor_left_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_left_pixmap);
  gtk_container_add (GTK_CONTAINER (cursor_left_button), cursor_left_pixmap);

  cursor_right_button = gtk_button_new ();
  gtk_widget_set_name (cursor_right_button, "cursor_right_button");
  gtk_widget_ref (cursor_right_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_right_button", cursor_right_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_right_button);
  gtk_table_attach (GTK_TABLE (table3), cursor_right_button, 2, 3, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, cursor_right_button, _("Move cursor right"), NULL);

  cursor_right_pixmap = create_pixmap (app, "ogle_gui/stock_right_arrow.xpm", FALSE);
  gtk_widget_set_name (cursor_right_pixmap, "cursor_right_pixmap");
  gtk_widget_ref (cursor_right_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_right_pixmap", cursor_right_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_right_pixmap);
  gtk_container_add (GTK_CONTAINER (cursor_right_button), cursor_right_pixmap);

  cursor_go_up_button = gtk_button_new ();
  gtk_widget_set_name (cursor_go_up_button, "cursor_go_up_button");
  gtk_widget_ref (cursor_go_up_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_go_up_button", cursor_go_up_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_go_up_button);
  gtk_table_attach (GTK_TABLE (table3), cursor_go_up_button, 0, 1, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, cursor_go_up_button, _("Go up"), NULL);

  cursor_go_up_pixmap = create_pixmap (app, "ogle_gui/go_up.xpm", FALSE);
  gtk_widget_set_name (cursor_go_up_pixmap, "cursor_go_up_pixmap");
  gtk_widget_ref (cursor_go_up_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_go_up_pixmap", cursor_go_up_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_go_up_pixmap);
  gtk_container_add (GTK_CONTAINER (cursor_go_up_button), cursor_go_up_pixmap);

  cursor_down_button = gtk_button_new ();
  gtk_widget_set_name (cursor_down_button, "cursor_down_button");
  gtk_widget_ref (cursor_down_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_down_button", cursor_down_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_down_button);
  gtk_table_attach (GTK_TABLE (table3), cursor_down_button, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, cursor_down_button, _("Move cursor down"), NULL);

  cursor_down_pixmap = create_pixmap (app, "ogle_gui/stock_down_arrow.xpm", FALSE);
  gtk_widget_set_name (cursor_down_pixmap, "cursor_down_pixmap");
  gtk_widget_ref (cursor_down_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_down_pixmap", cursor_down_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_down_pixmap);
  gtk_container_add (GTK_CONTAINER (cursor_down_button), cursor_down_pixmap);

  cursor_enter_button = gtk_button_new ();
  gtk_widget_set_name (cursor_enter_button, "cursor_enter_button");
  gtk_widget_ref (cursor_enter_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_enter_button", cursor_enter_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_enter_button);
  gtk_table_attach (GTK_TABLE (table3), cursor_enter_button, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_tooltips_set_tip (tooltips, cursor_enter_button, _("Enter"), NULL);

  cursor_enter_pixmap = create_pixmap (app, "ogle_gui/enter.xpm", FALSE);
  gtk_widget_set_name (cursor_enter_pixmap, "cursor_enter_pixmap");
  gtk_widget_ref (cursor_enter_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "cursor_enter_pixmap", cursor_enter_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cursor_enter_pixmap);
  gtk_container_add (GTK_CONTAINER (cursor_enter_button), cursor_enter_pixmap);

  table4 = gtk_table_new (2, 2, FALSE);
  gtk_widget_set_name (table4, "table4");
  gtk_widget_ref (table4);
  gtk_object_set_data_full (GTK_OBJECT (app), "table4", table4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table4);
  gtk_box_pack_start (GTK_BOX (hbox1), table4, TRUE, TRUE, 5);

  menus_button = gtk_button_new ();
  gtk_widget_set_name (menus_button, "menus_button");
  gtk_widget_ref (menus_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "menus_button", menus_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menus_button);
  gtk_table_attach (GTK_TABLE (table4), menus_button, 0, 1, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, menus_button, _("Select menu..."), NULL);

  menus_pixmap = create_pixmap (app, "ogle_gui/menus.xpm", FALSE);
  gtk_widget_set_name (menus_pixmap, "menus_pixmap");
  gtk_widget_ref (menus_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "menus_pixmap", menus_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menus_pixmap);
  gtk_container_add (GTK_CONTAINER (menus_button), menus_pixmap);

  audio_button = gtk_button_new ();
  gtk_widget_set_name (audio_button, "audio_button");
  gtk_widget_ref (audio_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "audio_button", audio_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (audio_button);
  gtk_table_attach (GTK_TABLE (table4), audio_button, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, audio_button, _("Audio select"), NULL);

  audio_pixmap = create_pixmap (app, "ogle_gui/stock_volume.xpm", FALSE);
  gtk_widget_set_name (audio_pixmap, "audio_pixmap");
  gtk_widget_ref (audio_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "audio_pixmap", audio_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (audio_pixmap);
  gtk_container_add (GTK_CONTAINER (audio_button), audio_pixmap);

  subpicture_button = gtk_button_new ();
  gtk_widget_set_name (subpicture_button, "subpicture_button");
  gtk_widget_ref (subpicture_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "subpicture_button", subpicture_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (subpicture_button);
  gtk_table_attach (GTK_TABLE (table4), subpicture_button, 0, 1, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, subpicture_button, _("Subpicture select"), NULL);

  subpicture_pixmap = create_pixmap (app, "ogle_gui/subpicture.xpm", FALSE);
  gtk_widget_set_name (subpicture_pixmap, "subpicture_pixmap");
  gtk_widget_ref (subpicture_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "subpicture_pixmap", subpicture_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (subpicture_pixmap);
  gtk_container_add (GTK_CONTAINER (subpicture_button), subpicture_pixmap);

  angle_button = gtk_button_new ();
  gtk_widget_set_name (angle_button, "angle_button");
  gtk_widget_ref (angle_button);
  gtk_object_set_data_full (GTK_OBJECT (app), "angle_button", angle_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (angle_button);
  gtk_table_attach (GTK_TABLE (table4), angle_button, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, angle_button, _("Angle select"), NULL);

  angle_pixmap = create_pixmap (app, "ogle_gui/angle.xpm", FALSE);
  gtk_widget_set_name (angle_pixmap, "angle_pixmap");
  gtk_widget_ref (angle_pixmap);
  gtk_object_set_data_full (GTK_OBJECT (app), "angle_pixmap", angle_pixmap,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (angle_pixmap);
  gtk_container_add (GTK_CONTAINER (angle_button), angle_pixmap);

  appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
  gtk_widget_set_name (appbar, "appbar");
  gtk_widget_ref (appbar);
  gtk_object_set_data_full (GTK_OBJECT (app), "appbar", appbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (appbar);
  gnome_app_set_statusbar (GNOME_APP (app), appbar);

  gnome_app_install_menu_hints (GNOME_APP (app), menubar_uiinfo);
  gtk_signal_connect (GTK_OBJECT (stop_button), "clicked",
                      GTK_SIGNAL_FUNC (on_stop_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (pause_button), "clicked",
                      GTK_SIGNAL_FUNC (on_pause_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (rewind_button), "clicked",
                      GTK_SIGNAL_FUNC (on_rewind_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (skip_backwards_button), "clicked",
                      GTK_SIGNAL_FUNC (on_skip_backwards_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (reverse_button), "clicked",
                      GTK_SIGNAL_FUNC (on_reverse_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (step_reverse_button), "clicked",
                      GTK_SIGNAL_FUNC (on_step_reverse_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (play_button), "clicked",
                      GTK_SIGNAL_FUNC (on_play_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (step_forwards_button), "clicked",
                      GTK_SIGNAL_FUNC (on_step_forwards_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (fastforward_button), "clicked",
                      GTK_SIGNAL_FUNC (on_fastforward_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (skip_forwards_button), "clicked",
                      GTK_SIGNAL_FUNC (on_skip_forwards_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (fast_button), "clicked",
                      GTK_SIGNAL_FUNC (on_fast_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (slow_button), "clicked",
                      GTK_SIGNAL_FUNC (on_slow_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cursor_up_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cursor_up_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cursor_left_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cursor_left_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cursor_right_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cursor_right_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cursor_go_up_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cursor_go_up_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cursor_down_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cursor_down_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cursor_enter_button), "clicked",
                      GTK_SIGNAL_FUNC (on_cursor_enter_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menus_button), "clicked",
                      GTK_SIGNAL_FUNC (on_menus_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (audio_button), "clicked",
                      GTK_SIGNAL_FUNC (on_audio_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (subpicture_button), "clicked",
                      GTK_SIGNAL_FUNC (on_subpicture_button_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (angle_button), "clicked",
                      GTK_SIGNAL_FUNC (on_angle_button_clicked),
                      NULL);

  gtk_object_set_data (GTK_OBJECT (app), "tooltips", tooltips);




  /* Beginning of disabled buttons */
  gtk_widget_set_sensitive(GTK_WIDGET(rewind_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(reverse_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(stop_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(play_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(fastforward_button), FALSE); 

  gtk_widget_set_sensitive(GTK_WIDGET(step_reverse_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(pause_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(step_forwards_button), FALSE);

  gtk_widget_set_sensitive(GTK_WIDGET(fast_button), FALSE);
  gtk_widget_set_sensitive(GTK_WIDGET(slow_button), FALSE);

  gtk_widget_set_sensitive(GTK_WIDGET(cursor_go_up_button), FALSE);
  /* End of disabled buttons */

  return app;
}

GtkWidget*
create_about (void)
{
  const gchar *authors[] = {
    "Björn Englund",
    "Håkan Hjort",
    "Vilhelm Bergman",
    "Martin Norbäck",
    "Björn Augustsson",
    NULL
  };
  GtkWidget *about;

  about = gnome_about_new ("Ogle", VERSION,
			   "Copyright 2000, 2001 Authors",
			   authors,
			   "http://www.dtek.chalmers.se/~dvd/",
			   NULL);
  gtk_widget_set_name (about, "about");
  gtk_object_set_data (GTK_OBJECT (about), "about", about);
  gtk_window_set_modal (GTK_WINDOW (about), TRUE);

  return about;
}

