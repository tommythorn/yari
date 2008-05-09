## src/vm/Makefile.am
##
## Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
## C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
## E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
## J. Wenninger, Institut f. Computersprachen - TU Wien
##
## This file is part of CACAO.
##
## This program is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as
## published by the Free Software Foundation; either version 2, or (at
## your option) any later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
## 02110-1301, USA.
##
## $Id: Makefile.am 7869 2007-05-05 08:08:49Z pm $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src -I$(top_srcdir)/contrib/vmlog -I$(top_srcdir)/src/native

LIBS =

SUBDIRS = jit

if ENABLE_CYCLES_STATS
CYCLES_STATS_SOURCES = \
	cycles-stats.c \
	cycles-stats.h
endif

noinst_HEADERS = \
	global.h \
	types.h

noinst_LTLIBRARIES = \
	libvm.la

libvm_la_SOURCES = \
	access.c \
	access.h \
	builtin.c \
	builtin.h \
	builtintable.inc \
	$(CYCLES_STATS_SOURCES) \
	exceptions.c \
	exceptions.h \
	finalizer.c \
	finalizer.h \
	initialize.c \
	initialize.h \
	jit_interface.h \
	properties.c \
	properties.h \
	resolve.c \
	resolve.h \
	signal.c \
	signallocal.h \
	string.c \
	stringlocal.h \
	vm.c \
	vm.h

libvm_la_LIBADD = \
	jit/libjit.la


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
