## src/native/vm/Makefile.am
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
## $Id: Makefile.am 5558 2006-09-28 19:33:24Z edwin $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src

LIBS =

DIST_SUBDIRS = \
	cldc1.1 \
	gnu

if WITH_CLASSPATH_CLDC1_1
SUBDIRS = \
	cldc1.1

NATIVEVM_LIB = \
	cldc1.1/libnativevmcore.la
endif

if WITH_CLASSPATH_GNU
SUBDIRS = \
	gnu

NATIVEVM_LIB = \
	gnu/libnativevmcore.la
endif

if ENABLE_JAVASE
CLASSLOADER_SOURCES = \
	java_lang_ClassLoader.c \
	java_lang_ClassLoader.h
endif

noinst_LTLIBRARIES = \
	libnativevm.la

libnativevm_la_SOURCES = \
	java_lang_Class.c \
	java_lang_Class.h \
	$(CLASSLOADER_SOURCES) \
	java_lang_Object.c \
	java_lang_Object.h \
	java_lang_Runtime.c \
	java_lang_Runtime.h \
	java_lang_Thread.c \
	java_lang_Thread.h

libnativevm_la_LIBADD = \
	$(NATIVEVM_LIB)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
