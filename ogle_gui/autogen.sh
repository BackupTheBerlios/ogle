#!/bin/sh
set -x

# Don't run gettextize automatically, you shouldn't really
#gettextize --copy --force < /dev/null
aclocal
libtoolize --copy --automake
automake --copy --add-missing
autoconf
autoheader
rm -f config.cache
