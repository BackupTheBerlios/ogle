#! /bin/sh
set -x

aclocal
libtoolize --copy --automake
##autoheader
automake --copy --add-missing
autoconf
rm -f config.cache