#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <dlfcn.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "my_glade.h"

// my own versions of the glade calls
static void (*my_glade_init)(void);
static void (*my_glade_xml_signal_autoconnect)(GladeXML *);
static GtkWidget * (*my_glade_xml_get_widget)(GladeXML *, const char *);

// the glade file
static GladeXML *xml;

// to be called first
void my_glade_setup (const char *filename)
{
  void *glade_lib;

  glade_lib = dlopen (LIBGLADE_LIB, RTLD_NOW);

  my_glade_init = dlsym(glade_lib, "glade_init");
  my_glade_xml_signal_autoconnect = 
    dlsym(glade_lib, "glade_xml_signal_autoconnect");
  my_glade_xml_get_widget =
    dlsym(glade_lib, "glade_xml_get_widget");

  (*my_glade_init)();
  xml = (*glade_xml_new)(filename, NULL);
  (*glade_xml_signal_autoconnect)(xml);
}

GtkWidget *get_glade_widget(const char *name)
{
  return (*my_glade_xml_get_widget)(xml, name);
}
