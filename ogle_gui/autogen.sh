#!/bin/sh
set -x

gettextize --copy --force < /dev/null

# patch for libglade strings
cat >> po/Makefile.in.in << "EOF"

# Added by autogen.sh
../po/glade-files.c: ../pixmaps/ogle_gui.glade
	libglade-xgettext -o $@ $<
POFILES: glade-files.c
EOF

aclocal
libtoolize --copy --automake
automake --copy --add-missing
autoconf
autoheader
rm -f config.cache
