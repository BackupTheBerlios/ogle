CC=gcc
DFLAGS= -DDEBUG -g -Wall -O6 `glib-config --cflags` -mcpu=ultrasparc -mvis \
	-DHAVE_MLIB -DHAVE_MADVICE -I/opt/SUNWmlib/include
#LDFLAGS=-L/opt/SUNWmlib/lib -R/opt/SUNWmlib/lib -lmlib -lX11 -lXext -lsocket -lnsl -lposix4 
LDFLAGS= -L/usr/X11R6/lib -lX11 -lXext -lsocket -lnsl -lposix4 
CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

#video_stream: video_output.o yuv2rgb.o yuv2rgb_mmx.o mmx_idct.o mmx_mlib.o 
video_stream: video_mpeg1.o video_mpeg2.o yuv2rgb_mlib.o /opt/SUNWmlib/lib/libmlib.a c_getbits.o video_output.o yuv2rgb.o video_tables.o 
clean:
	rm -f video_stream *.o
