#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "debug_print.h"
#include "my_glade.h"

// location of the standard ogle_gui.glade file
#define OGLE_GLADE_FILE PACKAGE_PIXMAPS_DIR "ogle_gui.glade"

// my own versions of the glade calls
static void (*my_glade_init)(void);
static GladeXML * (*my_glade_xml_new)(const char *, const char *);
static void (*my_glade_xml_signal_autoconnect)(GladeXML *);
static GtkWidget * (*my_glade_xml_get_widget)(GladeXML *, const char *);

// the glade file
static GladeXML *xml;

// my version of dlsym, which checks for errors
static void *my_dlsym(void *handle, char *symbol) {
  void *fun;
  char *error;
  char *new_symbol;

// Use _ before dlopened names to get them right on OpenBSD
#if defined(__OpenBSD__) && ! defined(__ELF__)
  new_symbol = g_strdup_printf("_%s", symbol);
#else
  new_symbol = g_strdup(symbol);
#endif

  fun = dlsym(handle, new_symbol);
  error = dlerror();
  if(error != NULL) {
    fprintf(stderr, "Error during dlsym of %s: %s\n", new_symbol, error);
    gtk_exit(1);
  }
  g_free(new_symbol);

  return fun;
}

// to be called first
void my_glade_setup ()
{
  void *glade_lib;

  glade_lib = dlopen (LIBGLADE_LIB, RTLD_NOW);
  if (glade_lib == NULL) {
    fprintf(stderr, "Error during dlopen: %s\n", dlerror());
    gtk_exit(1);
  }

  my_glade_init = my_dlsym(glade_lib, "glade_init");
  my_glade_xml_new = my_dlsym(glade_lib, "glade_xml_new");
  my_glade_xml_signal_autoconnect = 
    my_dlsym(glade_lib, "glade_xml_signal_autoconnect");
  my_glade_xml_get_widget =
    my_dlsym(glade_lib, "glade_xml_get_widget");

  my_glade_init();
  xml = my_glade_xml_new(OGLE_GLADE_FILE, NULL);
  my_glade_xml_signal_autoconnect(xml);
}

GtkWidget *get_glade_widget(const char *name)
{
  return my_glade_xml_get_widget(xml, name);
}
