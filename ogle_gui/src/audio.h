#ifndef _AUDIO_H
#define _AUDIO_H

void audio_menu_new(void);

void audio_menu_show(GtkWidget *button);

void audio_menu_remove(void);

void audio_menu_clear(void);

void audio_menu_update(void);

void audio_item_activate( GtkRadioMenuItem *item,
			  gpointer         user_data);

#endif /* _AUDIO_H */


