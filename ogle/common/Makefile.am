## Process this file with automake to produce Makefile.in

builddir = `pwd`

noinst_LIBRARIES = libcommon.a  
pkglib_LTLIBRARIES = libmsgcom.la libdvdcontrol.la
pkginclude_HEADERS = dvd.h dvdcontrol.h msgevents.h dvdevents.h
libcommon_a_SOURCES = ip_sem.c sync.c timemath.c
libmsgcom_la_SOURCES = msgevents.c
libdvdcontrol_la_SOURCES = dvdcontrol.c
libdvdcontrol_la_LIBADD = libmsgcom.la
##libdvdcontrol_la_LIBADD = -lmsgcom
##libdvdcontrol_la_LDFLAGS = -L$(builddir)/.libs
