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
#include <gdk/gdkx.h>

// This is set when changing widget values from event handler, so that
// no new events are generated
static gboolean changing;

// Could be replaced by a more efficient data structure, but...come on...
// anyway, this is a list of all the different properties
static GList *properties;

struct property_info {
  XvPortID  port;
  Atom      atom;
  GtkObject *controller;
  void      (*set_widget)(GtkObject *, long);
  int       default_value;
};

static void set_adjustment_attribute (GtkAdjustment *adjustment, 
                               struct property_info *property)
{
  if(!changing) {
    XvSetPortAttribute(
        GDK_DISPLAY (), 
        property->port, 
        property->atom, 
        adjustment->value);
    XFlush(GDK_DISPLAY ());
  }
}

static void set_adjustment_widget (GtkObject *adjustment, long value)
{
  gtk_adjustment_set_value(GTK_ADJUSTMENT(adjustment), value);
}

static void set_color_attribute (GtkColorSelection *color_selection,
                                 struct property_info *property)
{
  gdouble color[4];
  int color_value;

  if(!changing) {
    gtk_color_selection_get_color(GTK_COLOR_SELECTION(color_selection), color);
    color_value = 
      (((int)(color[0]*255)) << 16) + 
      (((int)(color[1]*255)) << 8) + 
      (((int)(color[2]*255)));
    XvSetPortAttribute(
        GDK_DISPLAY (), 
        property->port, 
        property->atom, 
        color_value);
    XFlush(GDK_DISPLAY ());
  }
}

static void set_color_widget (GtkObject *color_selection, long value)
{
  double colors[4] = {
    (value >> 16)         / 255.0, 
    ((value >> 8) & 0xff) / 255.0,
    (value        & 0xff) / 255.0,
    255
  };
  gtk_color_selection_set_color(
      GTK_COLOR_SELECTION(color_selection),
      colors);
}


static void set_bool_attribute (GtkWidget *toggle, 
                                struct property_info *property)
{
  gboolean active;

  if(!changing) {
    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
    XvSetPortAttribute(
        GDK_DISPLAY (), 
        property->port, 
        property->atom, 
        active?1:0);
    XFlush(GDK_DISPLAY ());
  }
}

static void set_bool_widget (GtkObject *toggle, long value)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), value);
}

static void set_only_attribute (GtkWidget *widget, 
                                struct property_info *property)
{
  if(!changing) {
    XvSetPortAttribute(GDK_DISPLAY (), property->port, property->atom, property->default_value);
    XFlush(GDK_DISPLAY ());
  }
}

static GdkFilterReturn
xv_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer user_data)
{
  XEvent *xevent = (XEvent *) gdk_xevent;
  int xv_event_base = GPOINTER_TO_INT(user_data);

  
  // if this is a port notify event, call the right function to update widget
  if (xevent->type == xv_event_base + XvPortNotify) {
    XvPortNotifyEvent *ev = (XvPortNotifyEvent *) xevent;
    GList *i;

    changing = TRUE;

    // find the property to go with this event
    for(i = properties; i != NULL; i = g_list_next(i)) {
      struct property_info *p = (struct property_info *) (i->data);
      if (p->port == ev->port_id &&
          p->atom == ev->attribute) {
        if (p->set_widget != NULL) {
          p->set_widget(p->controller, ev->value);
        }
        break;
      }
    }

    changing = FALSE;

    return GDK_FILTER_REMOVE;
  }


  return GDK_FILTER_CONTINUE;
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

  changing = FALSE;

  gtk_init(&argc, &argv);

  if(XvQueryExtension(
        GDK_DISPLAY (), 
        &xv_version, 
        &xv_release, 
        &xv_request_base,
        &xv_event_base, 
        &xv_error_base
        ) != Success) {
    fprintf(stderr, "Xv not found\n");
    exit(1);
  }

  XvQueryAdaptors(
      GDK_DISPLAY (), 
      DefaultRootWindow(GDK_DISPLAY ()),
      &num_adaptors, 
      &adaptor_info);  
  
  gtk_init(&argc, &argv);

  gdk_window_add_filter (NULL, xv_filter, GINT_TO_POINTER(xv_event_base));
  
  win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  
  gtk_signal_connect(GTK_OBJECT(win), "destroy", gtk_main_quit, NULL);
  notebook = gtk_notebook_new ();
  gtk_container_add(GTK_CONTAINER(win), notebook);

  for(n = 0; n < num_adaptors; n++) {
    int base_id;
    int num_ports;
    int port;

    GtkWidget *vbox;
    GtkWidget *adaptor_name;

    base_id = adaptor_info[n].base_id;
    num_ports = adaptor_info[n].num_ports;
    
    vbox = gtk_vbox_new(FALSE, 0);
    adaptor_name = gtk_label_new(adaptor_info[n].name);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, adaptor_name);

    for(port = base_id; port < base_id+num_ports; port++) {
      int num;
      int k;
      int cur_val = 0;
      Atom val_atom;
      XvAttribute *xvattr;

      // Get events when this port changes
      XvSelectPortNotify(GDK_DISPLAY (), port, True);

      xvattr = XvQueryPortAttributes(GDK_DISPLAY (), port, &num);
      for(k = 0; k < num; k++) {
        GtkWidget *hbox;
        GtkWidget *label;
        GtkWidget *manipulator;
        struct property_info *property;

        hbox = gtk_hbox_new(FALSE, 0);
        val_atom = XInternAtom(GDK_DISPLAY (), xvattr[k].name, False);
        if(xvattr[k].flags & XvGettable) {
          XvGetPortAttribute(
              GDK_DISPLAY (), 
              port, 
              val_atom, 
              &cur_val);
        }

	property = malloc(sizeof (struct property_info));
	property->port = port;
	property->atom = val_atom;
	property->default_value = xvattr[k].min_value;

        label = gtk_label_new (xvattr[k].name);
        if (!strcmp(xvattr[k].name, "XV_COLORKEY")) {
          manipulator = gtk_color_selection_new();
          set_color_widget(manipulator, cur_val);
          property->controller = (GtkObject *) manipulator;
          property->set_widget = set_color_widget;
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
	  set_bool_widget(manipulator, cur_val);
          property->controller = (GtkObject *) manipulator;
          property->set_widget = set_bool_widget;
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
          property->set_widget = NULL;
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
          property->controller = adjustment;
          property->set_widget = set_adjustment_widget;

          manipulator = gtk_hscale_new (GTK_ADJUSTMENT(adjustment));
          gtk_scale_set_digits(GTK_SCALE(manipulator), 0);

          // enable the manipulator if settable, disable it otherwise
          if (xvattr[k].flags & XvSettable) {
            gtk_signal_connect (GTK_OBJECT(adjustment), "value-changed",
                                GTK_SIGNAL_FUNC(set_adjustment_attribute), property);
          } else {
            gtk_widget_set_sensitive(GTK_WIDGET(manipulator), FALSE);
          }
        } 

        // add to list of properties
        properties = g_list_append(properties, property);

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
