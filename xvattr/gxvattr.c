/* gxvattr - get/set Xv attributes
 * Copyright (C) 2001 Björn Englund, Martin Norbäck
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



#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>

#include <gtk/gtk.h>

struct property_info {
  XvPortID port;
  Atom     atom;
  int      standard;
};

static Display *dpy;

static void set_port_attribute (GtkAdjustment *adjustment, 
                               struct property_info *property)
{
  XvSetPortAttribute(dpy, property->port, property->atom, adjustment->value);
  XFlush(dpy);
}

static void set_color_attribute (GtkColorSelection *color_selection,
                                 struct property_info *property)
{
  gdouble color[4];
  int color_value;

  gtk_color_selection_get_color(GTK_COLOR_SELECTION(color_selection), color);
  color_value = (((int)(color[0]*255)) << 16) + 
                (((int)(color[1]*255)) << 8) + 
		(((int)(color[2]*255)));
  XvSetPortAttribute(dpy, property->port, property->atom, color_value);
  XFlush(dpy);
}

static void set_bool_attribute (GtkToggleButton *toggle, 
                                struct property_info *property)
{
  gboolean active;

  active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
  XvSetPortAttribute(dpy, property->port, property->atom, active?1:0);
  XFlush(dpy);
}

static void set_only_attribute (GtkWidget *widget, 
                                struct property_info *property)
{
  XvSetPortAttribute(dpy, property->port, property->atom, property->standard);
  XFlush(dpy);
}

int main(int argc, char **argv)
{
  int num_adaptors = 0;
  XvAdaptorInfo *adaptor_info;
  int n;
  unsigned int xv_version, xv_release;
  unsigned int xv_request_base, xv_event_base, xv_error_base;

  GtkWidget *win;
  GtkWidget *notebook;

  dpy = XOpenDisplay(NULL);
  if(dpy == NULL) {
    fprintf(stderr, "Couldn't open display\n");
    exit(1);
  }
  

  if(XvQueryExtension(dpy, &xv_version, &xv_release, &xv_request_base,
		      &xv_event_base, &xv_error_base) != Success) {
    fprintf(stderr, "Xv not found\n");
    exit(1);
  }

  XvQueryAdaptors(dpy, DefaultRootWindow(dpy),
		  &num_adaptors, &adaptor_info);  
  
  
  gtk_init(&argc, &argv);

  win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_signal_connect(GTK_OBJECT(win), "destroy", gtk_main_quit, NULL);
  notebook = gtk_notebook_new ();
  gtk_container_add(GTK_CONTAINER(win), notebook);

  for(n = 0; n < num_adaptors; n++) {
    int base_id;
    int num_ports;
    int m;

    GtkWidget *vbox;
    GtkWidget *adaptor_name;

    base_id = adaptor_info[n].base_id;
    num_ports = adaptor_info[n].num_ports;
    
    vbox = gtk_vbox_new(FALSE, 0);
    adaptor_name = gtk_label_new(adaptor_info[n].name);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, adaptor_name);

    for(m = base_id; m < base_id+num_ports; m++) {
      int num;
      int k;
      int cur_val = 0;
      Atom val_atom;
      XvAttribute *xvattr;

      // this should be used to get events, but gtk won't see them anyway
      // XvSelectPortNotify(dpy, m, True);

      xvattr = XvQueryPortAttributes(dpy, m, &num);
      for(k = 0; k < num; k++) {
        GtkWidget *hbox;
        GtkWidget *label;
        GtkWidget *manipulator;
        struct property_info *property;

        hbox = gtk_hbox_new(FALSE, 0);
        val_atom = XInternAtom(dpy, xvattr[k].name, False);
        if(xvattr[k].flags & XvGettable) {
          if(XvGetPortAttribute(dpy, m, val_atom, &cur_val) != Success) {
            fprintf(stderr, "Couldn't get attribute\n");
            exit(1);
          }
        }

	property = malloc(sizeof (struct property_info));
	property->port = m;
	property->atom = val_atom;
	property->standard = xvattr[k].min_value;

        label = gtk_label_new (xvattr[k].name);
        if (!strcmp(xvattr[k].name, "XV_COLORKEY")) {
          double colors[4] = {(cur_val >> 16)         / 255.0, 
                              ((cur_val >> 8) & 0xff) / 255.0,
                              (cur_val        & 0xff) / 255.0,
	                      0};
          manipulator = gtk_color_selection_new();
          gtk_color_selection_set_color(GTK_COLOR_SELECTION(manipulator),
                                        colors);
          if (xvattr[k].flags & XvSettable) {
	    gtk_signal_connect(GTK_OBJECT(manipulator), "color-changed",
		               GTK_SIGNAL_FUNC(set_color_attribute), property);
          } else {
            gtk_widget_set_sensitive(GTK_WIDGET(manipulator), FALSE);
          }
	} else if (xvattr[k].min_value == 0 && 
	           xvattr[k].max_value == 1) {
	  // boolean value, use check button
	  manipulator = gtk_check_button_new();
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(manipulator),cur_val);
          if (xvattr[k].flags & XvSettable) {
	    gtk_signal_connect(GTK_OBJECT(manipulator), "toggled",
		               GTK_SIGNAL_FUNC(set_bool_attribute), property);
          } else {
            gtk_widget_set_sensitive(GTK_WIDGET(manipulator), FALSE);
          }
	} else if (!(xvattr[k].flags & XvGettable) && 
	           xvattr[k].min_value == xvattr[k].max_value) {
	  // only settable, and one value
	  manipulator = gtk_button_new_with_label("Set");
          if (xvattr[k].flags & XvSettable) {
	    gtk_signal_connect(GTK_OBJECT(manipulator), "clicked",
		               GTK_SIGNAL_FUNC(set_only_attribute), property);
          } else {
            gtk_widget_set_sensitive(GTK_WIDGET(manipulator), FALSE);
	  }
        } else {
          GtkObject *adjustment;

          adjustment = gtk_adjustment_new (cur_val,
                                           xvattr[k].min_value,
                                           xvattr[k].max_value,
                                           1,
                                           10,
                                           0);
          manipulator = gtk_hscale_new (GTK_ADJUSTMENT(adjustment));
          gtk_scale_set_digits(GTK_SCALE(manipulator), 0);

          // enable the manipulator if settable, disable it otherwise
          if (xvattr[k].flags & XvSettable) {
            gtk_signal_connect (GTK_OBJECT(adjustment), "value-changed",
                                GTK_SIGNAL_FUNC(set_port_attribute), property);
          } else {
            gtk_widget_set_sensitive(GTK_WIDGET(manipulator), FALSE);
          }
        } 

        gtk_box_pack_start(GTK_BOX(hbox), 
                          label,
                          FALSE,
                          FALSE,
                          0);
        gtk_box_pack_start(GTK_BOX(hbox), 
                           manipulator,
                           TRUE,
                           TRUE,
                           0);

        gtk_container_add(GTK_CONTAINER(vbox), hbox);
      }
    }
  }

  gtk_widget_show_all(win);

  gtk_main();
  return 0;
}
