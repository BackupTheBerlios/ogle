## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(DVD_INCLUDEDIR) \
	$(GTK_CFLAGS) \
	@XML_CFLAGS@ \
	@LIBGLADE_CFLAGS@

# define LOCALEDIR, CONFIG_FILE, CONFIG_FILE_DTD, and PACKAGE_PIXMAPS_DIR
DEFS = \
  -DLOCALEDIR=\"$(datadir)/locale\" \
  -DCONFIG_FILE=\"$(datadir)/ogle/oglerc\" \
  -DCONFIG_FILE_DTD=\"$(datadir)/ogle/ogle_conf.dtd\" \
  -DPACKAGE_PIXMAPS_DIR=\"$(datadir)/ogle_gui\" \
  @DEFS@

# To get ogle_gui installed in the ogle pkglibdir
pkglibdir=$(libdir)/ogle
pkglib_PROGRAMS = ogle_gui

ogle_gui_SOURCES = \
	main.c myintl.h \
	callbacks.c callbacks.h \
	xsniffer.c xsniffer.h \
	menu.c menu.h \
	audio.c audio.h \
	angle.c angle.h \
	ptt.c ptt.h \
	subpicture.c subpicture.h \
	language.c language.h \
	fileselector.c fileselector.h \
	actions.c actions.h \
	bindings.c bindings.h \
	interpret_config.c interpret_config.h \
	my_glade.c my_glade.h \
	debug_print.h

ogle_gui_LDADD = $(DVDLIBS) $(GTK_LIBS) $(INTLLIBS) \
	@PTHREAD_LIB@ @XML_LIBS@

# Because the dynamically opened libglade needs the callback symbols
# from the executable
ogle_gui_LDFLAGS = -export-dynamic
