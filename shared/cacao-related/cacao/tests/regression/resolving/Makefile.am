## tests/regression/resolving/Makefile.am
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
## $Id: Makefile.am 7433 2007-03-02 19:42:13Z edwin $

## Process this file with automake to produce Makefile.in

SUBDIRS = \
	classes1 \
	classes2 \
	classes3

HARNESS_SOURCE_FILES = \
	$(srcdir)/TestController.java \
	$(srcdir)/TestLoader.java

HARNESS_CLASS_FILES = \
	TestController.class \
	TestLoader.class

TEST_SOURCE_FILES = \
	$(srcdir)/test_instance_subtype_violated.java \
	$(srcdir)/test_param_loading_constraint_violated_derived.java \
	$(srcdir)/test_param_loading_constraint_violated.java \
	$(srcdir)/test_param_subtype_violated.java \
	$(srcdir)/test_retval_loading_constraint_violated.java \
	$(srcdir)/test_return_subtype_ok.java \
	$(srcdir)/test_return_subtype_violated.java \
	$(srcdir)/test_simple_lazy_load.java

TEST_NAMES = \
	test_instance_subtype_violated \
	test_param_loading_constraint_violated_derived \
	test_param_loading_constraint_violated \
	test_param_subtype_violated \
	test_retval_loading_constraint_violated \
	test_return_subtype_ok \
	test_return_subtype_violated \
	test_simple_lazy_load

EXTRA_DIST = $(HARNESS_SOURCE_FILES) $(TEST_SOURCE_FILES)

CLEANFILES = \
	*.class

JAVA      = $(top_builddir)/src/cacao/cacao

if WITH_CLASSPATH_GNU
JAVAFLAGS = -Xbootclasspath:$(top_builddir)/src/lib/classes/:$(CLASSPATH_CLASSES)
else
JAVAFLAGS = -Xbootclasspath:$(CLASSPATH_CLASSES)
endif

check: $(HARNESS_CLASS_FILES)
	@for t in $(TEST_NAMES) ; do echo "TEST $$t" ; { $(JAVAC) -d . -classpath . $(srcdir)/$$t.java && LD_LIBRARY_PATH=$(top_builddir)/src/cacao/.libs $(JAVA) $(JAVAFLAGS) $$t ; } || exit 1 ; done

$(HARNESS_CLASS_FILES): $(HARNESS_SOURCE_FILES)
	$(JAVAC) -d . $(HARNESS_SOURCE_FILES)

