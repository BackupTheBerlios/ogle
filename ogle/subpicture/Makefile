CC=gcc
CFLAGS=-g -Wall -O6 
all:	subt2xpm extract_subtitles xteddy
subt2xpm:
extract_subtitles:
xteddy:
	gcc -DHAVE_LIBXPM -Wall -Wconversion -g -o xteddy xteddy.c -I/usr/pd/X11/include/ -L/usr/pd/X11/lib/X11 -L/usr/X11/lib/ -lXpm -lX11 -lsocket -lnsl -lXext
clean:
	rm -f video_stream *.o

