#ifndef SUBPICTURE_H
#define SUBPICTURE_H

void subpicture_menu_new(void);

void subpicture_menu_show(GtkWidget *button);

void subpicture_menu_remove(void);

void subpicture_menu_clear(void);

void subpicture_menu_update(void);

void subpicture_item_activate( GtkRadioMenuItem *item,
			       gpointer         user_data);

#endif /* SUBPICTURE_H */
