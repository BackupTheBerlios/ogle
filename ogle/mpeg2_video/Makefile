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

# To build a clean software/only C, do nothing

# To build using MMX add -DHAVE_MMX to DFLAGS and uncomment the the MMX target
# To bulid using MLIB add -DHAVE_MLIB -I/opt/SUNWmlib/include to DFLAGS and
# uncoment the MLIB traget

# __sun__
DFLAGS= -g -Wall -O2 `glib-config --cflags` \
	-DHAVE_MADVISE -DHAVE_SHARE_MMU -DHAVE_CLOCK_GETTIME -DHAVE_MLIB \
	-DUSE_POSIX_SEM \
	-mcpu=ultrasparc -mvis -I/opt/SUNWmlib/include -I/usr/pd/include
LDFLAGS= -L/usr/X11R6/lib -L/usr/pd/lib -ljpeg -lX11 -lXext -lsocket -lnsl -lposix4 

# __lnux__

#DFLAGS= -Wall -O2 -fomit-frame-pointer -funroll-loops -malign-double `glib-config --cflags` \
#	-DHAVE_MMX -DHAVE_XV\
#	-DUSE_SYSV_SEM \
#	-mcpu=pentiumpro -march=pentiumpro -I/usr/pd/include
#LDFLAGS= -L/usr/X11R6/lib -L/usr/pd/lib -ljpeg -lX11 -lXext -lnsl -lrt  -lXv


CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

# Uncoment this to build an unaccelerated version
#video_stream: video_mpeg1.o video_mpeg2.o video_motion.o c_mlib.o c_getbits.o screenshot.o video_output_x11.o video_output.o yuv2rgb.o video_tables.o

# Uncoment this to build for MMX
#video_stream: video_mpeg1.o video_mpeg2.o video_motion.o yuv2rgb_mmx.o mmx_idct.o mmx_mlib.o c_getbits.o screenshot.o video_output_x11.o video_output.o yuv2rgb.o  video_tables.o

# Uncoment this to build for MLIB 
video_stream: video_stream.o video_mpeg1.o video_mpeg2.o video_motion.o yuv2rgb_mlib.o c_getbits.o video_output_x11.o video_output.o yuv2rgb.o video_tables.o screenshot.o /opt/SUNWmlib/lib/libmlib.a


# Uncoment this to build for MLIB DGA
#video_stream: video_stream.o video_mpeg1.o video_mpeg2.o video_motion.o yuv2rgb_mlib.o c_getbits.o video_output_ffb.o video_output.o yuv2rgb.o video_tables.o screenshot.o /opt/SUNWmlib/lib/libmlib.a

clean:
	rm -f video_stream *.o


