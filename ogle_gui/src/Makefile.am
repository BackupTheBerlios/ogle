## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(DVD_INCLUDEDIR) \
	$(GTK_CFLAGS) \
	$(LIBGLADE_CFLAGS) \
	@XML_CFLAGS@

# To get ogle_gui installed in the ogle pkglibdir
pkglibdir=$(libdir)/ogle
pkglib_PROGRAMS = ogle_gui

noinst_LIBRARIES = libinterpret.a

libinterpret_a_SOURCES = \
	interpret_config.c interpret_config.h \
	debug_print.h

libinterpret_a_LIBADD = @XML_LIBS@

libinterpret_a_LDFLAGS = -static

ogle_gui_SOURCES = \
	main.c myintl.h \
	callbacks.c callbacks.h \
	xsniffer.c xsniffer.h \
	menu.c menu.h \
	audio.c audio.h \
	angle.c angle.h \
	subpicture.c subpicture.h \
	language.c language.h \
	fileselector.c fileselector.h \
	actions.c actions.h \
	bindings.c bindings.h \
	debug_print.h

ogle_gui_LDADD = .libs/libinterpret.a \
	$(DVDLIBS) $(GTK_LIBS) $(LIBGLADE_LIBS) $(INTLLIBS) \
	@PTHREAD_LIB@ 
#@XML_LIBS@
