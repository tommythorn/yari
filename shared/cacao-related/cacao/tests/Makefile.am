## tests/Makefile.am
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
## $Id: Makefile.am 7882 2007-05-07 14:23:32Z tbfg $

## Process this file with automake to produce Makefile.in

SUBDIRS = regression

EXTRA_DIST = \
	hello.java \
	prop.java \
	x.java \
	x.output \
	TestBase.java \
	TestArrayClasses.java \
	TestArrayClasses.output \
	BasicToStrings.java \
	BasicToStrings.output \
	GCBench.java \
	JavaPerformance.java \
	param_test.java \
	exception_restore_registers.java

JAVA = $(top_builddir)/src/cacao/cacao

checkall: x.tst TestArrayClasses.tst BasicToStrings.tst param_test.tst exception_restore_registers.tst

%.tst:
	$(JAVAC) $*.java
	sh Test.sh $(JAVA) $*
#	$(JAVA) $* > $*.thisoutput
#	diff --brief $*.output $*.thisoutput
	$(RM) $*.thisoutput
	$(RM) $*.this2output

%.tstrun:
	$(JAVAC) $*.java
	$(JAVA) $*


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
