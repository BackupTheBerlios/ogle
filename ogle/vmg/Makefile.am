# SKROMPF - A video player
# Copyright (C) 2000 Björn Englund, Håkan Hjort, Martin Norbäck
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

bin_PROGRAMS = vmg dump ifo_dump vm vmg_dump

vmg_LDADD = $(top_builddir)/common/libcommon.a
vmg_SOURCES = vmg.c com.c

dump_SOURCES = \
	dump.c print_ifo_vm.c vobdump.c \
	vmcmd.c vmcmd.h \
	decoder.c decoder.h \
	ifo.h

ifo_dump_SOURCES = ifo_dump.c \
	ifo_read.c ifo_read.h \
	ifo_print.c ifo_print.h \
	vmcmd.c ifo.h

vm_LDADD = $(top_builddir)/common/libcommon.a
vm_SOURCES = \
	vm.c vm.h \
	ifo_read.c ifo_read.h \
	ifo_print.c ifo_print.h \
	decoder.c decoder.h \
	nav_read.c nav_read.h \
	nav_print.c nav_print.h \
	nav.c nav.h \
	vmcmd.c vmcmd.h \
	com.c ifo.h

vmg_dump_SOURCES = vmg_dump.c \
	nav_read.c nav_read.h \
	nav_print.c nav_print.h \
	nav.c nav.h \
	vmcmd.c com.c 
	vm.h
vmg_dump_LDADD = $(top_builddir)/common/libcommon.a
