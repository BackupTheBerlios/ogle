CC=gcc
DFLAGS= -g -Wall -O6 `glib-config --cflags` -mcpu=ultrasparc -mvis \
	-DHAVE_MLIB -DHAVE_MADVICE -I/opt/SUNWmlib/include
#LDFLAGS=-L/opt/SUNWmlib/lib -R/opt/SUNWmlib/lib -lmlib -lX11 -lXext -lsocket -lnsl
LDFLAGS= -L/usr/X11R6/lib -lX11 -lXext -lsocket -lnsl
CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

#video_stream: video_output.o yuv2rgb.o yuv2rgb_mmx.o mmx_idct.o mmx_mlib.o 
video_stream: video_output.o yuv2rgb.o yuv2rgb_mlib.o /opt/SUNWmlib/lib/libmlib.a
clean:
	rm -f video_stream *.o
