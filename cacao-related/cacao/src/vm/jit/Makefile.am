## src/vm/jit/Makefile.am
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
## $Id: Makefile.am 7786 2007-04-21 12:37:47Z edwin $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src -I$(top_srcdir)/contrib/vmlog

LIBS =

DIST_SUBDIRS = \
	allocator \
	inline \
	loop \
	optimizing \
	schedule \
	tools \
	verify \
	\
	intrp \
	alpha \
	arm \
	i386 \
	m68k \
	mips \
	parisc \
	powerpc \
	powerpc64 \
	s390 \
	sparc64 \
	x86_64

SUBDIRS = \
	optimizing \
	tools \
	$(ARCH_DIR)

ARCH_LIB = \
	$(ARCH_DIR)/libarch.la

if ENABLE_JIT
SUBDIRS += \
	allocator

OPTIMIZING_LIB = \
	optimizing/liboptimizing.la
endif

if ENABLE_INTRP
SUBDIRS += \
	intrp

INTRP_LIB = \
	intrp/libintrp.la
endif

if ENABLE_INLINING
SUBDIRS += \
	inline

INLINE_LIB = \
	inline/libinline.la
endif

if ENABLE_LOOP
SUBDIRS += \
	loop

LOOP_LIB = \
	loop/libloop.la
endif

if USE_SCHEDULER
SUBDIRS += \
	schedule

SCHEDULE_LIB = \
	schedule/libschedule.la
endif

REPLACE_SOURCES =

if ENABLE_JIT
REG_SOURCES = \
	reg.c \
	reg.h

REPLACE_SOURCES += \
	replace.h

STACK_SOURCES = \
	stack.c \
	stack.h
endif

if ENABLE_REPLACEMENT
REPLACE_SOURCES += \
	replace.c
endif

if ENABLE_VERIFIER
SUBDIRS += \
	verify

VERIFIER_LIB = \
	verify/libverify.la
endif

if WITH_BINUTILS_DISASSEMBLER
DISASS_SOURCES = disass-common.c
endif

noinst_HEADERS = \
	abi.h \
	abi-asm.h \
	asmpart.h \
	md.h \
	methodheader.h \
	patcher.h 

noinst_LTLIBRARIES = \
	libjit.la

libjit_la_SOURCES = \
	code.c \
	code.h \
	codegen-common.c \
	codegen-common.h \
	disass.h \
	$(DISASS_SOURCES) \
	dseg.c \
	dseg.h \
	emit-common.c \
	emit-common.h \
	icmdtable.inc \
	jit.c \
	jit.h \
	parse.c \
	parse.h \
	$(RECOMPILE_SOURCES) \
	$(REG_SOURCES) \
	$(REPLACE_SOURCES) \
	show.c \
	show.h \
	$(STACK_SOURCES) \
	stacktrace.c \
	stacktrace.h

libjit_la_SOURCES += \
	cfg.c \
	cfg.h

libjit_la_LIBADD = \
	allocator/liballocator.la \
	$(ALLOCATOR_LIB) \
	$(INLINE_LIB) \
	$(LOOP_LIB) \
	$(OPTIMIZING_LIB) \
	$(PROFILE_LIB) \
	$(SCHEDULE_LIB) \
	$(VERIFIER_LIB) \
	$(INTRP_LIB) \
	$(ARCH_LIB)


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
