DIST_SUBDIRS = linux

SUBDIRS = $(OS_DIR)

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir) -I$(top_builddir)/src -I$(top_srcdir)/src/vm/jit/$(ARCH_DIR)/$(OS_DIR)
AM_CCASFLAGS = $(AM_CPPFLAGS)

LIBS =

#BUILT_SOURCES = offsets.h

CLEANFILES = offsets.h

noinst_HEADERS = \
	 arch.h \
	 machine-instr.h

noinst_LTLIBRARIES = libarch.la

if ENABLE_DISASSEMBLER
DISASS_SOURCES = \
	  disass.c
endif

libarch_la_SOURCES = \
	asmpart.S \
	codegen.c \
	codegen.h \
	$(DISASS_SOURCES) \
	emit.c \
	md.c \
	patcher.c
						       
libarch_la_LIBADD = \
	$(OS_DIR)/libmd.la
	$(srcdir)/asmpart.S: $(top_builddir)/config.h offsets.h

#offsets.h: $(top_builddir)/src/vm/jit/tools/genoffsets $(top_builddir)/config.h
#	$(top_builddir)/src/vm/jit/tools/genoffsets > offsets.h

