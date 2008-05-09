## tests/regression/codepatching/Makefile.am
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
## $Id: Makefile.am 6257 2006-12-28 13:43:06Z twisti $

## Process this file with automake to produce Makefile.in

JAVA      = $(top_builddir)/src/cacao/cacao

if WITH_CLASSPATH_GNU
JAVAFLAGS = -Xbootclasspath:$(top_builddir)/src/lib/classes/:$(CLASSPATH_CLASSES)
else
JAVAFLAGS = -Xbootclasspath:$(CLASSPATH_CLASSES)
endif

SOURCE_FILES = \
	$(srcdir)/aastoreconstClass.java \
	$(srcdir)/checkcastC.java \
	$(srcdir)/checkcastI.java \
	$(srcdir)/getfieldD.java \
	$(srcdir)/getfieldF.java \
	$(srcdir)/getfieldI.java \
	$(srcdir)/getfieldJ.java \
	$(srcdir)/getfieldL.java \
	$(srcdir)/getstaticD.java \
	$(srcdir)/getstaticF.java \
	$(srcdir)/getstaticI.java \
	$(srcdir)/getstaticJ.java \
	$(srcdir)/getstaticL.java \
	$(srcdir)/instanceofC.java \
	$(srcdir)/instanceofI.java \
	$(srcdir)/invokespecial.java \
	$(srcdir)/invokestatic.java \
	$(srcdir)/multianewarray.java \
	$(srcdir)/newarray.java \
	$(srcdir)/putfieldD.java \
	$(srcdir)/putfieldF.java \
	$(srcdir)/putfieldI.java \
	$(srcdir)/putfieldJ.java \
	$(srcdir)/putfieldL.java \
	$(srcdir)/putfieldconstC.java \
	$(srcdir)/putfieldconstD.java \
	$(srcdir)/putfieldconstF.java \
	$(srcdir)/putfieldconstI.java \
	$(srcdir)/putfieldconstJ.java \
	$(srcdir)/putfieldconstL.java \
	$(srcdir)/putstaticD.java \
	$(srcdir)/putstaticF.java \
	$(srcdir)/putstaticI.java \
	$(srcdir)/putstaticJ.java \
	$(srcdir)/putstaticL.java \
	$(srcdir)/putstaticconstC.java \
	$(srcdir)/putstaticconstD.java \
	$(srcdir)/putstaticconstF.java \
	$(srcdir)/putstaticconstI.java \
	$(srcdir)/putstaticconstJ.java \
	$(srcdir)/putstaticconstL.java \
	$(srcdir)/test.java

EXTRA_DIST = \
	$(SOURCE_FILES)

MAINCLASS = \
	test.class

CLASSES = \
	aastoreconstClass.class \
	checkcastC.class \
	checkcastI.class \
	getfieldD.class \
	getfieldF.class \
	getfieldI.class \
	getfieldJ.class \
	getfieldL.class \
	getstaticD.class \
	getstaticF.class \
	getstaticI.class \
	getstaticJ.class \
	getstaticL.class \
	instanceofC.class \
	instanceofI.class \
	invokespecial.class \
	invokestatic.class \
	multianewarray.class \
	newarray.class \
	putfieldD.class \
	putfieldF.class \
	putfieldI.class \
	putfieldJ.class \
	putfieldL.class \
	putfieldconstC.class \
	putfieldconstD.class \
	putfieldconstF.class \
	putfieldconstI.class \
	putfieldconstJ.class \
	putfieldconstL.class \
	putstaticD.class \
	putstaticF.class \
	putstaticI.class \
	putstaticJ.class \
	putstaticL.class \
	putstaticconstC.class \
	putstaticconstD.class \
	putstaticconstF.class \
	putstaticconstI.class \
	putstaticconstJ.class \
	putstaticconstL.class

CLEANFILES = \
	*.class

check: build
	@LD_LIBRARY_PATH=$(top_builddir)/src/cacao/.libs $(JAVA) $(JAVAFLAGS) test

build:
	@$(JAVAC) -nowarn -d . $(SOURCE_FILES)

remove:
	$(RM) $(CLASSES)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
