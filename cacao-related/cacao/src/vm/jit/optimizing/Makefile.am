## src/vm/jit/optimizing/Makefile.am
##
## Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
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
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.
##
## $Id: Makefile.am,v 1.8 2004/12/07 14:33:21 twisti Exp $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src

LIBS =

if ENABLE_IFCONV
IFCONV_SOURCES = \
	ifconv.c \
	ifconv.h
endif

if ENABLE_PROFILING
PROFILE_SOURCES = \
	profile.c \
	profile.h

REORDER_SOURCES = \
	reorder.c \
	reorder.h
endif

if ENABLE_THREADS
RECOMPILE_SOURCES = \
	recompile.c \
	recompile.h
endif

if ENABLE_SSA
SSA_SOURCES = \
	lsra.c \
	lsra.h \
	ssa.c \
	ssa.h \
	graph.c \
	graph.h \
	dominators.c \
	dominators.h \
	lifetimes.c \
	lifetimes.h
endif

noinst_LTLIBRARIES = \
	liboptimizing.la

liboptimizing_la_SOURCES = \
	$(IFCONV_SOURCES) \
	$(PROFILE_SOURCES) \
	$(RECOMPILE_SOURCES) \
	$(REORDER_SOURCES) \
	$(SSA_SOURCES)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
