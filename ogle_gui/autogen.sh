#!/bin/sh
set -x

gettextize --copy --force < /dev/null
aclocal
libtoolize --copy --automake
automake --copy --add-missing
autoconf
autoheader
rm -f config.cache
