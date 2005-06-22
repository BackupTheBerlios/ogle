#!/bin/sh
set -x

# Don't run gettextize automatically, you shouldn't really
#gettextize --copy --force < /dev/null
aclocal -I m4
libtoolize --copy --automake
autoheader
automake --copy --add-missing
autoconf
rm -f config.cache
