## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR) \
	$(DVD_INCLUDEDIR)

# To get ogle_gui installed in the ogle pkglibdir
pkglibdir=$(datadir)/ogle
pkglib_PROGRAMS = ogle_gui

ogle_gui_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	xsniffer.c xsniffer.h \
	menu.c menu.h \
	audio.c audio.h \
	angle.c angle.h \
	subpicture.c subpicture.h \
	language.c language.h \
	fileselector.c fileselector.h


ogle_gui_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) $(DVDLIBS) \
	$(PTHREAD_LIB)
