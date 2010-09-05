## src/native/vm/gnu/Makefile.am
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
## $Id: Makefile.am 6233 2006-12-26 23:13:16Z twisti $

## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR) -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR) -I$(top_builddir)/src

LIBS =

noinst_LTLIBRARIES = \
	libnativevmcore.la

if ENABLE_JVMTI
lib_LTLIBRARIES = \
	libjdwp.la
endif

libnativevmcore_la_SOURCES = \
	gnu_classpath_VMStackWalker.c \
	gnu_classpath_VMSystemProperties.c \
	gnu_java_lang_management_VMClassLoadingMXBeanImpl.c \
	gnu_java_lang_management_VMMemoryMXBeanImpl.c \
	gnu_java_lang_management_VMRuntimeMXBeanImpl.c \
	gnu_java_lang_management_VMThreadMXBeanImpl.c \
	java_lang_VMClass.c \
	java_lang_VMClassLoader.c \
	java_lang_VMObject.c \
	java_lang_VMRuntime.c \
	java_lang_VMString.c \
	java_lang_VMSystem.c \
	java_lang_VMThread.c \
	java_lang_VMThrowable.c \
	java_lang_management_VMManagementFactory.c \
	java_lang_reflect_Constructor.c \
	java_lang_reflect_Field.c \
	java_lang_reflect_Method.c \
	java_lang_reflect_VMProxy.c \
	java_security_VMAccessController.c \
	sun_misc_Unsafe.c

if ENABLE_JVMTI
libjdwp_la_SOURCES = \
	gnu_classpath_jdwp_VMFrame.c \
	gnu_classpath_jdwp_VMMethod.c \
	gnu_classpath_jdwp_VMVirtualMachine.c \
	VMjdwp.c \
	VMjdwp.h
endif


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
