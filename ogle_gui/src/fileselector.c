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

#include <stdio.h>
#include <stdlib.h>
#include <gnome.h>

#include <ogle/dvdcontrol.h>

#include "fileselector.h"

#include "xsniffer.h" //hack

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
