#ifndef _AUDIO_H
#define _AUDIO_H

void audio_menu_new();

void audio_menu_show(GtkWidget *button);

void audio_menu_remove();

void audio_menu_clear();

void audio_menu_update();

void audio_item_activate( GtkButton       *button,
			  gpointer         user_data);

#endif /* _AUDIO_H */


