# SKROMPF - A video player
# Copyright (C) 2000 Björn Englund, Håkan Hjort, Martin Norbäck
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

CC=gcc


# __sun__
DFLAGS= -g -Wall -O2 `glib-config --cflags` \
	-DHAVE_MADVISE -DHAVE_SHARE_MMU -DHAVE_CLOCK_GETTIME -DHAVE_MLIB \
	-DUSE_POSIX_SEM \
	-mcpu=ultrasparc -mvis -I/opt/SUNWmlib/include -I/usr/pd/include
LDFLAGS= -L/usr/X11R6/lib -L/usr/pd/lib -ljpeg -lX11 -lXext -lsocket -lnsl -lposix4 

# __linux__

#DFLAGS= -Wall -O2 -fomit-frame-pointer -funroll-loops -malign-double \
#	`glib-config --cflags` \
#	-DHAVE_MMX -DHAVE_XV\
#	-DUSE_SYSV_SEM \
#	-mcpu=pentiumpro -march=pentiumpro -I/usr/pd/include
#LDFLAGS= -L/usr/X11R6/lib -L/usr/pd/lib  -lX11 -lXext -lnsl -lrt  -lXv


CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)


all:	programstream play_pes

programstream:

play_pes:

clean:
	rm -f programstream play_pes *.o
