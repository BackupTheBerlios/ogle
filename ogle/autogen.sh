#! /bin/sh
set -x

aclocal
## Should we have this? 
libtoolize --copy --force --automake
##autoheader
automake --copy --add-missing
autoconf
rm -f config.cache