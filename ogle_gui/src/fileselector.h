#ifndef FILE_SELECTOR_H
#define FILE_SELECTOR_H

void file_selector_new(void);

void file_selector_remove(GtkButton *button, gpointer user_data);

void file_selector_change_root(GtkFileSelection *selector,
			      gpointer user_data);


#endif /* FILE_SELECTOR_H */
