CC=gcc


 __sun__
DFLAGS= -g -Wall -O2  \
	-DHAVE_MADVISE -DHAVE_SHM_SHARE_MMU -DHAVE_CLOCK_GETTIME -DHAVE_MLIB \
	-DUSE_POSIX_SEM \
	-mcpu=ultrasparc -mvis -I/opt/SUNWmlib/include -I/usr/pd/include
LDFLAGS= -lrt 

# __linux__
#
#DFLAGS= -Wall -O2 -fomit-frame-pointer -funroll-loops -malign-double \
#	-DHAVE_MMX -DHAVE_XV\
#	-DUSE_SYSV_SEM \
#	-mcpu=pentiumpro -march=pentiumpro
#LDFLAGS= -lrt


CFLAGS= -DDCFLAGS='"$(DFLAGS)"' -DDLDFLAGS='"$(LDFLAGS)"' $(DFLAGS)

all: ac3dec_wrap

clean:
	rm -f ac3dec_wrap *.o
