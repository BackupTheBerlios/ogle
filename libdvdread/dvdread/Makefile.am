
lib_LTLIBRARIES = libdvdread.la
libdvdread_la_SOURCES = dvd_reader.c dvd_udf.c ifo_read.c ifo_print.c \
	nav_read.c nav_print.c
libdvdread_la_LIBADD = @DL_LIBS@
libdvdread_la_LDFLAGS = -version-info 2:0:0

pkginclude_HEADERS = dvd_reader.h ifo_types.h ifo_read.h \
	ifo_print.h nav_types.h nav_read.h nav_print.h
