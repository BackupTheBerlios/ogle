CC=gcc
CFLAGS=-g
LDFLAGS=-lposix4

all: vmg ifo dump

dump: print_ifo_vm.o vobdump.o vmcmd.o decoder.o
ifo: vmcmd.o
