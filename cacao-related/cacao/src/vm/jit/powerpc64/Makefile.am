## src/vm/jit/powerpc64/Makefile.am
##
## Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
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
## Contact: cacao@cacaojvm.org
##
## Authors: Christian Thalinger
##
## Changes:
##
## $Id: Makefile.am 5162 2006-07-19 13:07:00Z tbfg $

## Process this file with automake to produce Makefile.in

DIST_SUBDIRS = \
	linux 

SUBDIRS = $(OS_DIR)

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir) -I$(top_builddir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR)
AM_CCASFLAGS = $(AM_CPPFLAGS)

LIBS =

BUILT_SOURCES = offsets.h

CLEANFILES = offsets.h

noinst_HEADERS = \
	arch.h \
	machine-instr.h

noinst_LTLIBRARIES = libarch.la

if ENABLE_DISASSEMBLER
DISASS_SOURCES = \
	disass.c
endif

libarch_la_SOURCES = \
	asmpart.S \
	codegen.c \
	codegen.h \
	$(DISASS_SOURCES) \
	emit.c \
	md.c \
	patcher.c

libarch_la_LIBADD = \
	$(OS_DIR)/libmd.la

$(srcdir)/asmpart.S: $(top_builddir)/config.h offsets.h

offsets.h: $(top_builddir)/src/vm/jit/tools/genoffsets $(top_builddir)/config.h
	$(top_builddir)/src/vm/jit/tools/genoffsets > offsets.h


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
