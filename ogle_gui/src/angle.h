#ifndef ANGLE_H
#define ANGLE_H

void angle_menu_new(void);

void angle_menu_show(GtkWidget *button);

void angle_menu_remove(void);

void angle_menu_clear(void);

void angle_menu_update(void);

void angle_item_activate( GtkRadioMenuItem *item,
			  gpointer         user_data);

#endif /* ANGLE_H */
