#! /bin/sh
set -x

aclocal
#aclocal -I <libtool m4 dir> -I <`xml2-config --prefix`/share/aclocal>
libtoolize --copy --automake
#autoheader
#add --include-deps if you want to bootstrap with any other compiler than gcc
#automake --copy --add-missing --include-deps
automake --copy --add-missing
autoconf
rm -f config.cache
