CC=gcc

# To build a clean software/only C, do nothing

# To build using MMX add -DHAVE_MMX to DFLAGS and uncomment the the MMX target
# To bulid using MLIB add -DHAVE_MLIB -I/opt/SUNWmlib/include to DFLAGS and
# uncoment the MLIB traget


DFLAGS= -g -Wall -O6 `glib-config --cflags` \
	-DHAVE_MADVICE -DDEBUG \
	-mcpu=ultrasparc -mvis -DHAVE_MLIB -I/opt/SUNWmlib/include
LDFLAGS= -L/usr/X11R6/lib -lX11 -lXext -lsocket -lnsl -lposix4 
CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

# Uncoment this to build an unaccelerated version
#video_stream: video_mpeg1.o video_mpeg2.o c_mlib.o c_getbits.o video_output.o yuv2rgb.o video_tables.o

# Uncoment this to build for MMX
#video_stream: video_mpeg1.o video_mpeg2.o yuv2rgb_mmx.o mmx_idct.o mmx_mlib.o c_getbits.o video_output.o yuv2rgb.o  video_tables.o

# Uncoment this to build for MLIB 
video_stream: video_mpeg1.o video_mpeg2.o yuv2rgb_mlib.o /opt/SUNWmlib/lib/libmlib.a c_getbits.o video_output.o yuv2rgb.o video_tables.o 

clean:
	rm -f video_stream *.o
