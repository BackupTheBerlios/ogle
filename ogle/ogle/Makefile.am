## Process this file with automake to produce Makefile.in


pkglib_LTLIBRARIES = libmsgevents.la libdvdcontrol.la
pkginclude_HEADERS = dvd.h dvdcontrol.h msgevents.h dvdevents.h
libmsgevents_la_SOURCES = msgevents.c
libmsgevents_la_LDFLAGS = -version-info 1:0:0
libdvdcontrol_la_SOURCES = dvdcontrol.c
libdvdcontrol_la_LIBADD = libmsgevents.la
libdvdcontrol_la_LDFLAGS = -version-info 1:0:0
