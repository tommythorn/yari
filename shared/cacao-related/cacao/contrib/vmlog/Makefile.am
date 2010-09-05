
SUBDIRS = t

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/native \
	-I$(top_srcdir)/src/vm/jit/@ARCH_DIR@ \
	-I$(top_srcdir)/src/vm/jit/@ARCH_DIR@/@OS_DIR@ \
	-DVMLOG_HAVE_PTRINT

if ENABLE_VMLOG
noinst_PROGRAMS = vmlogdump vmlogindex vmlogdiff
endif

COMMON_SOURCES = \
	vmlog.c \
	vmlog.h

vmlogdump_SOURCES = \
	$(COMMON_SOURCES) \
	vmlogdump.c

vmlogindex_SOURCES = \
	$(COMMON_SOURCES) \
	vmlogdiff.c

vmlogdiff_SOURCES = \
	$(COMMON_SOURCES) \
	vmlogdiff.c

EXTRA_DIST = \
	COPYING \
	maintain.mk \
	README \
	vmlog_cacao.c \
	vmlog_cacao.h \
	vmlog_jamvm.c \
	vmlog_jamvm.h \
	vmlog_jamvm.patch

