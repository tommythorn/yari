## tests/regression/resolving/classes1/Makefile.am
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

SOURCE_FILES = \
	$(srcdir)/BarPassFoo.java \
	$(srcdir)/BarUseFoo.java \
	$(srcdir)/Foo.java

CLASS_FILES = \
	BarPassFoo.class \
	BarUseFoo.class \
	Foo.class

EXTRA_DIST = $(SOURCE_FILES)

CLEANFILES = \
	*.class

check: $(CLASS_FILES)

$(CLASS_FILES): $(SOURCE_FILES)
	$(JAVAC) -d . $(SOURCE_FILES)

