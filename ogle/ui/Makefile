CC=gcc

# __sun__
DFLAGS= -g -Wall -O2  -DWORDS_BIGENDIAN\
	-DHAVE_MADVISE -DHAVE_SHARE_MMU -DHAVE_CLOCK_GETTIME -DHAVE_MLIB \
	-DUSE_POSIX_SEM \
	-mcpu=ultrasparc -mvis -I/opt/SUNWmlib/include -I/usr/pd/include
LDFLAGS= -lrt 

# __linux__

#DFLAGS= -g -Wall -O2 -fomit-frame-pointer -funroll-loops -malign-double \
#	-DHAVE_MMX -DHAVE_XV\
#	-DUSE_SYSV_SEM \
#	-mcpu=pentiumpro -march=pentiumpro
#LDFLAGS= -lrt

CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

all: ui	

clean:
	rm -f ui *.o

