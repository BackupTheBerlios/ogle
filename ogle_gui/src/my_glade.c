#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "my_glade.h"

// my own versions of the glade calls
static void (*my_glade_init)(void);
static GladeXML * (*my_glade_xml_new)(const char *, const char *);
static void (*my_glade_xml_signal_autoconnect)(GladeXML *);
static GtkWidget * (*my_glade_xml_get_widget)(GladeXML *, const char *);

// the glade file
static GladeXML *xml;

// to be called first
void my_glade_setup (const char *filename)
{
  void *glade_lib;

  fprintf(stderr, "calling dlopen(" LIBGLADE_LIB ")\n");

  glade_lib = dlopen (LIBGLADE_LIB, RTLD_NOW);

  fprintf(stderr, "glade_lib = %lx\n", glade_lib);

  my_glade_init = dlsym(glade_lib, "glade_init");
  my_glade_xml_new = dlsym(glade_lib, "glade_xml_new");
  my_glade_xml_signal_autoconnect = 
    dlsym(glade_lib, "glade_xml_signal_autoconnect");
  my_glade_xml_get_widget =
    dlsym(glade_lib, "glade_xml_get_widget");

  my_glade_init();
  xml = my_glade_xml_new(filename, NULL);
  my_glade_xml_signal_autoconnect(xml);
}

GtkWidget *get_glade_widget(const char *name)
{
  return my_glade_xml_get_widget(xml, name);
}
