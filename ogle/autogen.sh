#! /bin/sh
set -x

aclocal
## Should we have this? 
libtoolize --force
##autoheader
automake --add-missing
autoconf
rm -f config.cache