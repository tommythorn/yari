## src/Makefile.am
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
## $Id: Makefile.am 7596 2007-03-28 21:05:53Z twisti $

## Process this file with automake to produce Makefile.in

DIST_SUBDIRS = \
	cacao \
	cacaoh \
	fdlibm \
	lib \
	mm \
	native \
	scripts \
	threads \
	toolbox \
	vm \
	vmcore

if WITH_CLASSPATH_GNU
VM_DIR = \
	lib
endif

if ENABLE_THREADS
THREADS_DIR = \
	threads
endif

if ENABLE_CACAOH
CACAOH_DIR = \
	cacaoh 
endif

# DON'T CHANGE THIS ORDER!!!

SUBDIRS = \
	toolbox \
	vmcore \
	$(CACAOH_DIR) \
	$(VM_DIR) \
	native \
	fdlibm \
	mm \
	$(THREADS_DIR) \
	vm \
	cacao \
	scripts


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
