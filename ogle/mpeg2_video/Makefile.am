# SKROMPF - A video player
# Copyright (C) 2000 Bj�rn Englund, H�kan Hjort, Martin Norb�ck
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

if MLIB_LIBS
MLIBSOURCES=yuv2rgb_mlib.c yuv2rgb_mlib.h
else
if USE_MMX
MMXSOURCES=yuv2rgb_mmx.c mmx_idct.S mmx_mlib.c mmx.h
else
CSOURCES=c_mlib.c c_mlib.h
endif
endif

EXTRA_DIST = \
	yuv2rgb_mlib.c yuv2rgb_mlib.h \
	yuv2rgb_mmx.c yuv2rgb_mmx.h \
	mmx_idct.S \
	mmx_mlib.c mmx.h \
	c_mlib.c c_mlib.h

bin_PROGRAMS = video_stream video_output

INCLUDES = @X_CFLAGS@ @MLIB_CFLAGS@ @GLIB_CFLAGS@

video_stream_SOURCES = \
	video_stream.c video_stream.h \
	video_mpeg1.c video_mpeg2.c video_motion.c \
	c_getbits.c c_getbits.h \
	video_tables.c video_tables.h \
	video_types.h \
	$(MLIBSOURCES) $(MMXSOURCES) $(CSOURCES)

video_output_SOURCES = \
	video_output_x11.c video_output.c \
	yuv2rgb.c yuv2rgb.h \
	screenshot.c screenshot.h \
	spu_mixer.c spu_mixer.h \
	$(MLIBSOURCES) $(MMXSOURCES) $(CSOURCES)

video_stream_LDADD = -ljpeg @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@ @XEXT_LIB@ @MLIB_LIBS@ $(top_builddir)/common/libcommon.a

video_output_LDADD = -ljpeg @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@ @XEXT_LIB@ @MLIB_LIBS@ $(top_builddir)/common/libcommon.a