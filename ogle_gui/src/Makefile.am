## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR) \
	-I$(DVD_INCLUDEDIR)

bin_PROGRAMS = ogle

ogle_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	xsniffer.c xsniffer.h \
	menu.c menu.h \
	audio.c audio.h 

ogle_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) $(DVD_LIBDIR)/libdvdcontrol.a $(DVD_LIBDIR)/libcommon.a
