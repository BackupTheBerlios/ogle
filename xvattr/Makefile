CC =	gcc
CFLAGS =	-g -Wall `gtk-config --cflags`
INCDIRS =	-I/usr/X11R6/include
LIBDIRS =	-L/usr/X11R6/lib
LIBS =	-lX11 -lXext -lXv 
OBJS =	xvattr.o

all: xvattr gxvattr

xvattr: xvattr.o
	$(CC) $(CFLAGS) $(INCDIRS) $(LIBDIRS)  -o $@ $< $(LIBS) 

gxvattr: gxvattr.o
	$(CC) $(CFLAGS) $(INCDIRS) $(LIBDIRS)  -o $@ $< $(LIBS) `gtk-config --libs`

clean:
	rm -f xvattr gxvattr *.o


.c.o:
	$(CC) $(INCDIRS) $(CFLAGS) -c $< -o $@
