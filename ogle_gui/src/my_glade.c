#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "debug_print.h"
#include "my_glade.h"

// the glade file
static GladeXML *xml;

// If LIBGLADE_LIBDIR is defined, then we should dynamic linking of
// libglade, since otherwise libxml will clash with libxml2
// This also means we use glade1 instead of glade2
#ifdef LIBGLADE_LIBDIR
// my own versions of the glade calls
static void (*my_glade_init)(void);
static GladeXML * (*my_glade_xml_new)(const char *, const char *);
static void (*my_glade_xml_signal_autoconnect)(GladeXML *);
static GtkWidget * (*my_glade_xml_get_widget)(GladeXML *, const char *);

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

#define GLADE_INIT my_glade_init
#define GLADE_XML_NEW(x) my_glade_xml_new(x, NULL)
#define GLADE_XML_SIGNAL_AUTOCONNECT my_glade_xml_signal_autoconnect
#define GLADE_XML_GET_WIDGET my_glade_xml_get_widget
#define GLADE_EXT "glade"

#else /* not defined(GLADE_XML_LIB) */

#define GLADE_INIT glade_init 
// yes, different number of arguments, strange incompatibility
#define GLADE_XML_NEW(x) glade_xml_new(x, NULL, NULL) 
#define GLADE_XML_SIGNAL_AUTOCONNECT glade_xml_signal_autoconnect
#define GLADE_XML_GET_WIDGET glade_xml_get_widget

#define GLADE_EXT "glade2"

#endif

// location of the standard ogle_gui.glade file
#define OGLE_GLADE_FILE PACKAGE_PIXMAPS_DIR "/ogle_gui." GLADE_EXT

// to be called first
void my_glade_setup ()
{
  char *home;
#ifdef LIBGLADE_LIBDIR
  void *glade_lib;

  // First try to open libglade.so in the path given by libglade-config
  glade_lib = dlopen (LIBGLADE_LIBDIR "/" LIBGLADE_LIB, RTLD_NOW);
  if (glade_lib == NULL) {
    // next, try without path
    glade_lib = dlopen (LIBGLADE_LIB, RTLD_NOW);
  }
  if (glade_lib == NULL) {
    fprintf(stderr, "Error during dlopen: %s\n", dlerror());
    gtk_exit(1);
  }

  // connect symbols in dynamic lib
  my_glade_init = my_dlsym(glade_lib, "glade_init");
  my_glade_xml_new = my_dlsym(glade_lib, "glade_xml_new");
  my_glade_xml_signal_autoconnect = 
    my_dlsym(glade_lib, "glade_xml_signal_autoconnect");
  my_glade_xml_get_widget =
    my_dlsym(glade_lib, "glade_xml_get_widget");
#endif

  GLADE_INIT();

  // first, try the user's home directory
  home = getenv("HOME");
  if (home != NULL) {
    char *filename;
    filename = g_strdup_printf("%s/.ogle_gui/ogle_gui." GLADE_EXT, home);
    if(!access(filename, R_OK)) {
      xml = GLADE_XML_NEW(filename);
    }
    g_free(filename);
  }
  if (xml == NULL) {
    xml = GLADE_XML_NEW(OGLE_GLADE_FILE);
  }
  if (xml == NULL) {
    fprintf(stderr, "Couldn't find $HOME/.ogle_gui/ogle_gui.glade or %s\n", 
	    OGLE_GLADE_FILE);
    gtk_exit(1);
  }
  GLADE_XML_SIGNAL_AUTOCONNECT(xml);
}

GtkWidget *get_glade_widget(const char *name)
{
  return GLADE_XML_GET_WIDGET(xml, name);
}
