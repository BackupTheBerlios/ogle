#ifndef CALLBACKS_H
#define CALLSBACKS_H

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

#include <gtk/gtk.h>


void
on_ptt_activate_pm                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_angle_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_audio_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_subpicture_activate_pm              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_root_activate_pm                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_title_activate_pm                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_resume_activate_pm                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_new_file_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_open_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void 
on_opendvd_activate                  (GtkMenuItem     *menuitem,
				      gpointer         user_data);

void
on_save_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_as_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_exit_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_cut_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_copy_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_paste_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_clear_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_properties_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_preferences_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_stop_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_pause_button_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_rewind_button_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_skip_backwards_button_clicked       (GtkButton       *button,
                                        gpointer         user_data);

void
on_reverse_button_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_step_reverse_button_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_play_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_step_forwards_button_clicked        (GtkButton       *button,
                                        gpointer         user_data);

void
on_fastforward_button_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_skip_forwards_button_clicked        (GtkButton       *button,
                                        gpointer         user_data);

void
on_fast_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_slow_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_cursor_up_button_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_cursor_left_button_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_cursor_right_button_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_cursor_go_up_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_cursor_down_button_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_cursor_enter_button_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_menus_button_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_audio_button_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_subpicture_button_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_angle_button_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_ptt_button_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void 
on_full_screen_activate                (GtkButton       *button,
					gpointer        user_data);

void
on_keypad_activate                     (GtkButton       *button,
					gpointer        user_data);

void
on_keypadbutton_clicked                (GtkButton       *button,
					gpointer        user_data);

void on_keypad_nof_digits_clicked      (GtkButton       *button,
					gpointer        user_data);

void
on_jump_to_ptt_activate                (GtkWidget       *widget,
                                        gpointer        pos);

#endif /* CALLBACKS_H */

