#!/bin/sh
set -x

gettextize
aclocal
libtoolize --copy --automake
automake --copy --add-missing
autoconf
autoheader
rm -f config.cache
