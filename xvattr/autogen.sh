#! /bin/sh
set -x

aclocal
automake --copy --add-missing
autoconf
rm -f config.cache
