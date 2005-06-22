#!/bin/sh
set -x

# Don't run gettextize automatically, you shouldn't really
# run gettextize when upgradeing to new version of gettext
# and follow the instructions, and commit changes to cvs
#gettextize --copy --force --intl
aclocal -I m4
libtoolize --copy --automake
autoheader
automake --copy --add-missing
autoconf
rm -f config.cache
