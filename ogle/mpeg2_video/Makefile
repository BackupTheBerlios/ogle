CC=gcc
CFLAGS= -g -Wall -O6 -mcpu=ultrasparc -mvis -I/opt/SUNWmlib/include
#LDFLAGS=-L/opt/SUNWmlib/lib -R/opt/SUNWmlib/lib -lmlib -lX11 -lXext -lsocket -lnsl
LDFLAGS=-lX11 -lXext -lsocket -lnsl 

video_stream: /opt/SUNWmlib/lib/libmlib.a
clean:
	rm -f video_stream *.o
