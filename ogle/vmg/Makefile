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


all: vmg_dump ifo_dump dump vm vmg

dump: print_ifo_vm.o vobdump.o vmcmd.o decoder.o
ifo_dump: ifo_dump.o ifo_read.o ifo_print.o vmcmd.o
vm: vm.o ifo_read.o ifo_print.o decoder.o nav_read.o nav_print.o nav.o vmcmd.o 
vmg_dump: vmg_dump.o nav_read.o nav_print.o nav.o vmcmd.o

clean:
	rm -f vmg vmg_dump ifo_dump dump vm *.o
