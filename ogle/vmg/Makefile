CC=gcc
CFLAGS=-g -O2 -Wall -DWORDS_BIGENDIAN
LDFLAGS=-lposix4

all: vmg_dump ifo_dump dump vm vmg

dump: print_ifo_vm.o vobdump.o vmcmd.o decoder.o
ifo_dump: ifo_dump.o ifo_read.o ifo_print.o vmcmd.o
vm: vm.o ifo_read.o ifo_print.o decoder.o nav_read.o nav_print.o nav.o vmcmd.o 
vmg_dump: vmg_dump.o nav_read.o nav_print.o nav.o vmcmd.o

clean:
	rm -f vmg vmg_dump ifo_dump dump vm *.o
