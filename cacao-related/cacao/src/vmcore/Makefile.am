## src/vmcore/Makefile.am
##
## Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel,
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
## $Id: Makefile.am 6216 2006-12-18 18:21:37Z twisti $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR)

LIBS =

if ENABLE_JAVASE
ANNOTATION_SOURCES = \
	annotation.c \
	annotation.h

STACKMAP_SOURCES = \
	stackmap.c \
	stackmap.h
endif

if ENABLE_RT_TIMING
RT_TIMING_SOURCES = \
	rt-timing.c \
	rt-timing.h
endif

if ENABLE_STATISTICS
STATISTICS_SOURCES = \
	statistics.c \
	statistics.h
endif

if ENABLE_ZLIB
ZLIB_SOURCES = \
	zip.c \
	zip.h
endif

if ENABLE_EMBEDDED_CLASSES
EMBEDDED_CLASSES_SOURCES = \
	embedded_classes.c \
	embedded_classes.h
endif

noinst_LTLIBRARIES = \
	libvmcore.la

libvmcore_la_SOURCES = \
	$(ANNOTATION_SOURCES) \
	class.c \
	class.h \
	classcache.c \
	classcache.h \
	descriptor.c \
	descriptor.h \
	field.c \
	field.h \
	linker.c \
	linker.h \
	loader.c \
	loader.h \
	method.c \
	method.h \
	options.c \
	options.h \
	references.h \
	$(RT_TIMING_SOURCES) \
	$(STACKMAP_SOURCES) \
	$(STATISTICS_SOURCES) \
	suck.c \
	suck.h \
	utf8.c \
	utf8.h \
	$(ZLIB_SOURCES) \
	$(EMBEDDED_CLASSES_SOURCES)

## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
