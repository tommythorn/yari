## src/cacao/Makefile.am
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
## Contact: cacao@cacaojvm.org
##
## $Id: Makefile.am 7338 2007-02-13 00:17:22Z twisti $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR)

if ENABLE_THREADS
THREAD_LIB = \
	$(top_builddir)/src/threads/libthreads.la
endif

if ENABLE_STATICVM
cacao_LDFLAGS = \
	-all-static
else
cacao_LDFLAGS =
endif

if ENABLE_LIBJVM
lib_LTLIBRARIES = \
	libjvm.la

libjvm_la_LDFLAGS = \
	-release $(PACKAGE_VERSION)

cacao_LDFLAGS += \
	-export-dynamic

CACAO_LIBS =
else
noinst_LTLIBRARIES = \
	libjvm.la

cacao_LDFLAGS += \
	-export-dynamic

CACAO_LIBS = \
	libjvm.la
endif

if WITH_STATIC_CLASSPATH
cacao_LDFLAGS += \
	$(LTLIBICONV)

CLASSPATH_LIBS = \
	$(CLASSPATH_LIBDIR)/classpath/libjavaio.a \
	$(CLASSPATH_LIBDIR)/classpath/libjavanio.a \
	$(CLASSPATH_LIBDIR)/classpath/libjavanet.a \
	$(CLASSPATH_LIBDIR)/classpath/libjavautil.a \
	$(CLASSPATH_LIBDIR)/classpath/libjavalang.a \
	$(CLASSPATH_LIBDIR)/classpath/libjavalangreflect.a

CACAO_LIBS += \
	$(CLASSPATH_LIBS)

#if USE_GTK_PEER
#cacao_CFLAGS = \
#	$(GTK_CFLAGS) $(CAIRO_CFLAGS) $(PANGOFT2_CFLAGS) \
#	$(XML_CFLAGS) $(XSLT_CFLAGS)
#
#cacao_LDFLAGS += \
#	$(GTK_LIBS) $(CAIRO_LIBS) $(PANGOFT2_LIBS) $(X_LIBS) -lXtst \
#	$(XML_LIBS) $(XSLT_LIBS)
#
#CLASSPATH_LIBS += \
#	$(CLASSPATH_LIBDIR)/classpath/libgtkpeer.a
#endif
endif

if ENABLE_RT_TIMING
cacao_LDFLAGS += -lrt
endif

libjvm_la_SOURCES =

libjvm_la_LIBADD = \
	$(top_builddir)/src/fdlibm/libfdlibm.la \
	$(top_builddir)/src/mm/libmm.la \
	$(top_builddir)/src/native/libnative.la \
	$(top_builddir)/src/toolbox/libtoolbox.la \
	$(top_builddir)/src/vm/libvm.la \
	$(top_builddir)/src/vmcore/libvmcore.la \
	$(GC_LIB) \
	$(THREAD_LIB)

bin_PROGRAMS = \
	cacao

cacao_SOURCES = \
	cacao.c

cacao_LDADD = \
	$(CACAO_LIBS)

cacao_DEPENDENCIES = \
	$(cacao_LDADD)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
