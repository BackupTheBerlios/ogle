# Generated automatically from Makefile.in by configure.
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
ifneq (,)
MLIB_OBJS=yuv2rgb_mlib.o
endif

ifneq (yes,)
MMX_OBJS=yuv2rgb_mmx.o mmx_idct.o mmx_mlib.o
endif

ifeq (@MLIB_OBJS@yes,)
OTHER_OBJS=c_mlib.o
endif

DFLAGS=-g -O2 -fomit-frame-pointer -funroll-loops -malign-double -march=i686  -DHAVE_XV=1 -DHAVE_XSHM=1 -DHAVE_MMX=1 -DUSE_SYSV_SEM=1   -I/usr/X11R6/include  -I/usr/lib/glib/include
LDFLAGS=-lrt  -ljpeg  -lSM -lICE  -L/usr/X11R6/lib -lX11  -lXext -lXv 
CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

OBJS=video_stream.o video_mpeg1.o video_mpeg2.o video_motion.o c_getbits.o video_output_x11.o video_output.o yuv2rgb.o video_tables.o screenshot.o $(MLIB_OBJS) $(MMX_OBJS) $(OTHER_OBJS)

video_stream: $(OBJS)

clean:
	rm -f video_stream *.o


