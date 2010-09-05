## src/tests/regression/native/Makefile.am
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
## $Id: Makefile.am 7596 2007-03-28 21:05:53Z twisti $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)

JAVA      = $(top_builddir)/src/cacao/cacao

if WITH_CLASSPATH_GNU
JAVAFLAGS = -Xbootclasspath:$(top_builddir)/src/lib/classes/:$(CLASSPATH_CLASSES)
else
JAVAFLAGS = -Xbootclasspath:$(CLASSPATH_CLASSES)
endif

JAVAH     = @CACAOH@

SOURCE_FILES = \
	checkjni.java \
	testarguments.java

EXTRA_DIST = \
	$(SOURCE_FILES) \
	checkjni.c \
	testarguments.c \
	\
	checkjni.output \
	testarguments.output

CLEANFILES = \
	*.class \
	*.h \
	*.so \
	*.thisoutput \
	*.this2output

NOTESTNAMES = \
	test

TESTNAMES = \
	checkjni \
	testarguments

check: $(TESTNAMES)

$(TESTNAMES) $(NOTESTNAMES):
	@$(JAVAC) -nowarn -d . $(srcdir)/$@.java
	@$(JAVAH) $@
	@$(CC) -shared $(AM_CPPFLAGS) $(CFLAGS) $(srcdir)/$@.c -o lib$@.so -fPIC
	@LD_LIBRARY_PATH=$(top_builddir)/src/cacao/.libs:. $(SHELL) $(srcdir)/../Test.sh "$(JAVA) $(JAVAFLAGS)" $@ $(srcdir)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
