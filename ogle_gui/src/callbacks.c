#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include <ogle/dvdcontrol.h>

#include "menu.h"

void
on_ptt_activate_pm                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(DVD_MENU_Part);
}


void
on_angle_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(DVD_MENU_Angle);
}


void
on_audio_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(DVD_MENU_Audio);
}


void
on_subpicture_activate_pm              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(DVD_MENU_Subpicture);
}


void
on_root_activate_pm                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(DVD_MENU_Subpicture);
}


void
on_title_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDMenuCall(DVD_MENU_Title);
}


void
on_resume_activate_pm                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  DVDResume();
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

}


void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_stop_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDStop();
}


void
on_pause_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  static int isPaused = 0;
  if(isPaused) {
    DVDPauseOff();
    isPaused =1;
  } else {
    DVDPauseOn();
    isPaused =0;
  }
}


void
on_rewind_button_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDBackwardScan(2.0);
}


void
on_skip_backwards_button_clicked       (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDPrevPGSearch();
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
  DVDForwardScan(2.0);
}


void
on_skip_forwards_button_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDNextPGSearch();
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
  DVDUpperButtonSelect();
}


void
on_cursor_left_button_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDLeftButtonSelect();
}


void
on_cursor_right_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDRightButtonSelect();
}


void
on_cursor_go_up_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDGoUp();
}


void
on_cursor_down_button_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDLowerButtonSelect();
}


void
on_cursor_enter_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  DVDButtonActivate();
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

}


void
on_angle_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

}


