## src/native/Makefile.am
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
## $Id: Makefile.am 7384M 2007-05-07 11:35:04Z (local) $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src

LIBS =

SUBDIRS = \
	include \
	vm
if ENABLE_EMBEDDED_CLASSES
MY_NATIVETABLE_SOURCES = \
	my_nativetable.c

CLEANFILES = \
	nativetable.inc \
	my_nativetable.c \
	my_nativetable.tmp
else
CLEANFILES = \
	nativetable.inc
endif

if ENABLE_JVMTI
SUBDIRS += jvmti
JVMTI_LIB = jvmti/libjvmti.la
endif

if WITH_STATIC_CLASSPATH
SUBDIRS += tools

GENNATIVETABLE=$(top_builddir)/src/native/tools/gennativetable

CLASSPATH = $(top_builddir)/src/lib/classes/:$(CLASSPATH_GLIBJ_ZIP)

$(srcdir)/native.c: nativetable.inc

nativetable.inc: $(CACAO_VM_ZIP) $(CLASSPATH_GLIBJ_ZIP) $(top_builddir)/src/native/tools/gennativetable
	echo	$(GENNATIVETABLE) -bootclasspath $(CLASSPATH) > $(top_builddir)/src/native/nativetable.inc
	if `$(GENNATIVETABLE) -bootclasspath $(CLASSPATH) > $(top_builddir)/src/native/nativetable.inc` ; then true; else rm -f $(top_builddir)/src/native/nativetable.inc; exit 1; fi
endif

noinst_LTLIBRARIES = libnative.la

if ENABLE_JNI
JNI_SOURCES = \
	jni.c \
	jni.h
endif

if ENABLE_EMBEDDED_CLASSES
my_nativetable.tmp: vm/libnativevm.la
	nm --defined-only vm/.libs/libnativevm.a | grep Java | cut -d ' ' -f 3 > $@

my_nativetable.c: my_nativetable.tmp
	echo "char *native_names[] = {" > $@
	cat $^ | sort | sed -e s/^/\"/ -e s/$$/\",/ >> $@
	echo "};" >> $@
	cat $^ | sort | sed -e s/^/extern\ void\ / -e s/$$/\(\)\;/ >> $@
	echo "typedef void (*funcptr)();" >> $@
	echo "funcptr native_symbols[] = {" >> $@
	cat $^ | sort | sed -e s/^/\&/ -e s/$$/,/ >> $@
	echo "};" >> $@
	echo -n "int num_natives = " >> $@
	cat $^ | wc -l >> $@
	echo ";" >> $@
endif

libnative_la_SOURCES = \
	$(JNI_SOURCES) \
    $(MY_NATIVETABLE_SOURCES) \
	native.c \
	native.h

libnative_la_LIBADD = \
	vm/libnativevm.la \
	$(JVMTI_LIB)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
