## src/native/vm/cldc1.1/Makefile.am
##
## Copyright (C) 2006, 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
## R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
## C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
## Institut f. Computersprachen - TU Wien
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
## $Id: Makefile.am 6213 2006-12-18 17:36:06Z twisti $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src

LIBS =

noinst_LTLIBRARIES = \
	libnativevmcore.la

if ENABLE_EMBEDDED_CLASSES
else
EXTRA_SOURCES = \
	com_sun_cldc_io_j2me_socket_Protocol.c
endif

libnativevmcore_la_SOURCES = \
	$(EXTRA_SOURCES) \
	com_sun_cldc_io_ResourceInputStream.c \
	com_sun_cldchi_io_ConsoleOutputStream.c \
	com_sun_cldchi_jvm_JVM.c \
	java_lang_Class.c \
	java_lang_Double.c \
	java_lang_Float.c \
	java_lang_Math.c \
	java_lang_Object.c \
	java_lang_Runtime.c \
	java_lang_String.c \
	java_lang_System.c \
	java_lang_Thread.c \
	java_lang_Throwable.c


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
