#include <stdio.h>
#include <stdlib.h>
#include <gnome.h>

#include <ogle/dvdcontrol.h>

#include "fileselector.h"

extern DVDNav_t *nav;

static GtkWidget *file_selector = NULL;
static gchar *last_visited = NULL;

void file_selector_new(void) {
  if(file_selector != NULL)
    return;
  
  /* Create the selector */
  file_selector =
    gtk_file_selection_new(_("Please select a file or directory."));
  
  gtk_signal_connect (GTK_OBJECT
		      (GTK_FILE_SELECTION (file_selector)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC (file_selector_change_root),
		      NULL);
  
  /* Ensure that the dialog box is destroyed when the user clicks a button. */
  gtk_signal_connect_object (GTK_OBJECT
			     (GTK_FILE_SELECTION(file_selector)->ok_button),
			     "clicked", GTK_SIGNAL_FUNC (file_selector_remove),
			     (gpointer) file_selector);
  
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION
					 (file_selector)->cancel_button),
			     "clicked", GTK_SIGNAL_FUNC (file_selector_remove),
			     (gpointer) file_selector);

  if(last_visited!=NULL) {  
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector),
				    last_visited);
  }

  /* Display that dialog */
  gtk_widget_show (file_selector);
}

void file_selector_remove(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(GTK_WIDGET(file_selector));
  file_selector=NULL;
}

void file_selector_change_root(GtkFileSelection *selector,
			       gpointer user_data) {
  gchar *selected_filename;
  DVDResult_t res;

  if(last_visited!=NULL) {
    g_free(last_visited);
  }

  selected_filename =
    gtk_file_selection_get_filename (GTK_FILE_SELECTION(file_selector));
    
  last_visited = g_strdup(selected_filename);
  
  res = DVDSetDVDRoot(nav, (char*)selected_filename);
  if(res != DVD_E_Ok) {
    DVDPerror("file_selector_change_root: DVDSetDVDRoot", res);
    return;
  }
}
