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

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include <ogle/dvdcontrol.h>

#include "menu.h"
#include "audio.h"
#include "subpicture.h"
#include "fileselector.h"

#include "xsniffer.h" //hack


extern ZoomMode_t zoom_mode;

extern DVDNav_t *nav;

static int isPaused = 0;


void
on_ptt_activate_pm                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(nav, DVD_MENU_Part);
}


void
on_angle_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(nav, DVD_MENU_Angle);
}


void
on_audio_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(nav, DVD_MENU_Audio);
}


void
on_subpicture_activate_pm              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(nav, DVD_MENU_Subpicture);
}


void
on_root_activate_pm                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(nav, DVD_MENU_Root);
}


void
on_title_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(nav, DVD_MENU_Title);
}


void
on_resume_activate_pm                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDResume(nav);
}


void
on_new_file_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_open_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  file_selector_new();
}

void 
on_opendvd_activate                  (GtkMenuItem     *menuitem,
				      gpointer         user_data) 
{
  DVDResult_t res;
#ifdef LINUX
  char *dev="/dev/dvd";
#else
#ifdef SOLARIS
  char *dev="/cdrom/cdrom0";
#else
#error "No default path."
#endif
#endif

  res = DVDSetDVDRoot(nav, dev);
  if(res != DVD_E_Ok) {
    DVDPerror("callbacks.on_opendvd_activate(): DVDSetDVDRoot", res);
    return;
  }
}

void
on_save_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

void
on_save_as_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_exit_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  extern DVDNav_t *nav;
  DVDResult_t res;
  res = DVDCloseNav(nav);
  if(res != DVD_E_Ok ) {
    DVDPerror("DVDCloseNav", res);
  }
  exit(0);
}


void
on_cut_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_copy_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_paste_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_clear_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_properties_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_preferences_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  fprintf(stderr, "Not implemented yet.\n");
}


void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(create_about());
}


void
on_stop_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDStop(nav);
}


void
on_pause_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  if(isPaused) {
    DVDPauseOff(nav);
    isPaused =0;
  } else {
    DVDPauseOn(nav);
    isPaused =1;
  }
}


void
on_rewind_button_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDBackwardScan(nav, 2.0);
}


void
on_skip_backwards_button_clicked       (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDPrevPGSearch(nav);
}


void
on_reverse_button_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_step_reverse_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_play_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDForwardScan(nav, 1.0);
  if(isPaused)
    DVDPauseOff(nav);
}


void
on_step_forwards_button_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_fastforward_button_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDForwardScan(nav, 2.0);
}


void
on_skip_forwards_button_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDNextPGSearch(nav);
}


void
on_fast_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_slow_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_cursor_up_button_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDUpperButtonSelect(nav);
}


void
on_cursor_left_button_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDLeftButtonSelect(nav);
}


void
on_cursor_right_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDRightButtonSelect(nav);
}


void
on_cursor_go_up_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDGoUp(nav);
}


void
on_cursor_down_button_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDLowerButtonSelect(nav);
}


void
on_cursor_enter_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDButtonActivate(nav);
}


void
on_menus_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  menu_show(button);
}


void
on_audio_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  audio_menu_show(button);
}


void
on_subpicture_button_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
  subpicture_menu_show(button);
}


void
on_angle_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  angle_menu_show(button);
}


void 
on_full_screen_activate                (GtkButton       *button,
					gpointer        user_data)
{
  DVDResult_t res;
  
  zoom_mode = (zoom_mode == ZoomModeResizeAllowed) 
    ? ZoomModeFullScreen : ZoomModeResizeAllowed;
  
  res = DVDSetZoomMode(nav, zoom_mode);
  if(res != DVD_E_Ok) {
    DVDPerror("callbacks.on_full_screen_activate: DVDSetZoomMode()",
	      res);
    return;
  }
}
