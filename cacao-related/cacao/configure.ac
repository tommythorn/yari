dnl configure.ac
dnl
dnl Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
dnl C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
dnl E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
dnl J. Wenninger, Institut f. Computersprachen - TU Wien
dnl 
dnl This file is part of CACAO.
dnl 
dnl This program is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU General Public License as
dnl published by the Free Software Foundation; either version 2, or (at
dnl your option) any later version.
dnl 
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
dnl 02110-1301, USA.
dnl 
dnl $Id: configure.ac 7887M 2007-05-08 12:45:38Z (local) $

dnl Process this file with autoconf to produce a configure script.


AC_INIT(cacao, 0.98rc, cacao@cacaojvm.org)
AC_CONFIG_SRCDIR(src/cacao/cacao.c)
AC_CANONICAL_HOST
AC_PREREQ(2.59)
AM_INIT_AUTOMAKE([1.9.0])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER([config.h])

AC_PREFIX_DEFAULT(/usr/local/cacao)


dnl set optimization and debugging for all architectures and systems
if test x"$CFLAGS" = "x"; then
    OPT_CFLAGS="-O0 -g3"
else
    OPT_CFLAGS=$CFLAGS
fi

dnl system type
case "$host_cpu" in
alpha | alphaev56 | alphapca56 )
    ARCH_DIR="alpha"
    ARCH_CFLAGS="-mieee -D__ALPHA__"
    ;;

arm | armv4 | armv4tl | armv5b | armv5l )
    ARCH_DIR="arm"
    ARCH_CFLAGS="-D__ARM__"
    ;;

hppa2.0 )
    ARCH_DIR="parisc"
    ARCH_CFLAGS="-D__PARISC__"
    ;;

i386 | i486 | i586 | i686 )
    ARCH_DIR="i386"
    ARCH_CFLAGS="-D__I386__"
    ;;

m68k )
    ARCH_DIR="m68k"
    ARCH_CFLAGS="-D__M68K__"
    ;;

mips | mipsel )
    ARCH_DIR="mips"
    ARCH_CFLAGS="-D__MIPS__"
    ;;

powerpc )
    ARCH_DIR="powerpc"
    ARCH_CFLAGS="-D__POWERPC__"
    ;;

powerpc64 )
    ARCH_DIR="powerpc64"
    ARCH_CFLAGS="-D__POWERPC64__"
    ;;

s390 )
    ARCH_DIR="s390"
    ARCH_CFLAGS="-D__S390__"
    ;;

sparc | sparc64 )
    ARCH_DIR="sparc64"
    ARCH_CFLAGS="-mcpu=v9 -m64 -D__SPARC_64__"
    ;;

x86_64 )
    ARCH_DIR="x86_64"
    ARCH_CFLAGS="-D__X86_64__"
    ;;

xdspcore )
    ARCH_DIR="xdspcore"
    ENABLE_STATICVM="yes"
    ARCH_CFLAGS="-D__XDSPCORE__"
    USE_SCHEDULER="1"
    ;;

* )
    AC_MSG_ERROR($host_cpu systems are not supported at this time)
    ;;
esac

dnl host type
case "$host_os" in
*cygwin* )
    OS_DIR="cygwin"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__WINDOWS__ -D__CYGWIN__ -Wall -Wno-long-long"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__WINDOWS__ -D__CYGWIN__ -ansi -pedantic -Wall -Wno-long-long"
    ;;

*darwin* )
    OS_DIR="darwin"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__DARWIN__ -Wall -Wno-long-long"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__DARWIN__ -ansi -pedantic -Wall -Wno-long-long"
    ;;

*freebsd* )
    OS_DIR="freebsd"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__FREEBSD__ -Wall -Wno-long-long -D_XOPEN_SOURCE_EXTENDED"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__FREEBSD__ -ansi -pedantic -Wall -Wno-long-long -D_XOPEN_SOURCE_EXTENDED"
    ;;

*irix* )
    OS_DIR="irix"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__IRIX__"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__IRIX__"
    ;;

*kfreebsd*-gnu)
    OS_DIR="freebsd"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__FREEBSD__ -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__FREEBSD__ -ansi -pedantic -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ;;

dnl must be before *linux*
*uclinux | *elf )
    OS_DIR="uclinux"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__LINUX__ -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__LINUX__ -ansi -pedantic -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ;;

*linux* | *Linux* )
    OS_DIR="linux"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__LINUX__ -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__LINUX__ -ansi -pedantic -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ;;

*netbsd* )
    OS_DIR="netbsd"
    INTRP_CFLAGS="$ARCH_CFLAGS -D__NETBSD__ -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__NETBSD__ -ansi -pedantic -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_BSD_SOURCE"
    ;;

*solaris* )
    OS_DIR="solaris"
	dnl on solaris the architecture defines 'sparc','sun','unix' are not set when cpp is called with -ansi
	dnl therefore we add them here
    INTRP_CFLAGS="$ARCH_CFLAGS -D__SOLARIS__ -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D__EXTENSIONS__ -D_BSD_SOURCE -Dsparc -Dsun -Dunix"
    ARCH_CFLAGS="$ARCH_CFLAGS -D__SOLARIS__ -ansi -pedantic -Wall -Wno-long-long -D_POSIX_C_SOURCE=199506L -D_XOPEN_SOURCE=500 -D__EXTENSIONS__ -D_BSD_SOURCE -Dsparc -Dsun -Dunix"
    ;;

* )
    ;;
esac


dnl temporary set the CFLAGS for configure tests (e.g. inline keyword)
dnl we set it properly at the end of this file
CFLAGS="$ARCH_CFLAGS $OPT_CFLAGS"

dnl set interpreter flags
AC_SUBST(INTRP_CFLAGS)

dnl define some stuff required for --fullversion
AC_DEFINE_UNQUOTED(VERSION_CONFIGURE_ARGS, "$ac_configure_args", [configure arguments])
AC_DEFINE_UNQUOTED(VERSION_CC, "$CC", [CC used])
AC_DEFINE_UNQUOTED(VERSION_CFLAGS, "$OPT_CFLAGS $ARCH_CFLAGS", [CFLAGS used])


dnl define and substitute some architecture specific variables
AC_DEFINE_UNQUOTED([ARCH_DIR], "${ARCH_DIR}", [architecture directory])
AC_SUBST(ARCH_DIR)
AC_SUBST(OS_DIR)
AC_SUBST(USE_SCHEDULER)


dnl Checks for programs.
AC_PROG_CC
AM_PROG_AS
AC_PROG_LD
AC_PROG_LD_GNU
AC_PROG_RANLIB
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_MKDIR_P

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC

dnl keep them alpha-sorted!
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([sys/mman.h])
AC_CHECK_HEADERS([sys/resource.h])
AC_CHECK_HEADERS([sys/time.h])

dnl this is for fdlibm
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([sys/config.h])
AC_CHECK_HEADERS([sys/types.h])
AC_EGREP_HEADER(uint32_t, stdint.h, AC_DEFINE(HAVE_INT32_DEFINED, 1, [Define to 1 if you have uint32_t]))
AC_EGREP_HEADER(uint32_t, inttypes.h, AC_DEFINE(HAVE_INT32_DEFINED, 1, [Define to 1 if you have uint32_t]))
AC_EGREP_HEADER(u_int32_t, sys/types.h, AC_DEFINE(HAVE_BSD_INT32_DEFINED, 1, [Define to 1 if you have BSD u_int32_t]))
AC_EGREP_HEADER(u_int32_t, sys/config.h, AC_DEFINE(HAVE_BSD_INT32_DEFINED, 1, [Define to 1 if you have BSD u_int32_t]))

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_ATTRIBUTE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_MEMCMP
AC_FUNC_MMAP

dnl keep them alpha-sorted!
AC_CHECK_FUNCS([calloc])
AC_CHECK_FUNCS([getpagesize])
AC_CHECK_FUNCS([free])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([getrusage])
AC_CHECK_FUNCS([isnan])
AC_CHECK_FUNCS([mmap])
AC_CHECK_FUNCS([mprotect])
AC_CHECK_FUNCS([scandir])
AC_CHECK_FUNCS([strdup])


dnl Checks for libraries.

dnl Checking sizeof void *
AC_CHECK_SIZEOF(void *)
AC_C_BIGENDIAN


dnl define install prefix
if test "x$prefix" = "xNONE"; then
    CACAO_PREFIX=$ac_default_prefix
else
    CACAO_PREFIX=$prefix
fi
AC_DEFINE_UNQUOTED([CACAO_PREFIX], "${CACAO_PREFIX}", [installation prefix])
AC_SUBST(CACAO_PREFIX)

dnl define libdir prefix
if test "x$libdir" = "xNONE"; then
    CACAO_LIBDIR=$ac_default_libdir
else
    CACAO_LIBDIR=$libdir
fi
dnl expand CACAO_LIBDIR to something that is usable in C code
AS_AC_EXPAND([CACAO_LIBDIR], ${CACAO_LIBDIR})
AC_DEFINE_UNQUOTED([CACAO_LIBDIR], "${CACAO_LIBDIR}", [library installation prefix])
AC_SUBST(CACAO_LIBDIR)


dnl Features

AC_CHECK_ENABLE_JAVA
AC_CHECK_ENABLE_JIT

dnl with-options
AC_CHECK_WITH_CACAOH

dnl check for stuff that should be done in software
AC_CHECK_SOFTFLOAT
AC_CHECK_SOFT_FLOAT_CMP
AC_CHECK_SOFT_DOUBLE_CMP


dnl check for interpreter
AC_ARG_ENABLE([intrp], [AS_HELP_STRING(--enable-intrp,enable interpreter [[default=no]])])

AC_MSG_CHECKING(whether interpreter should be compiled)
if test x"$enable_intrp" = "xyes"; then
    AC_MSG_RESULT(yes)
    AC_DEFINE([ENABLE_INTRP], 1, [enable interpreter])
    AM_CONDITIONAL([ENABLE_INTRP], [true])
    ENABLE_INTRP=yes

    AC_ASM_SKIP16

    dnl check for libffi
    AC_ARG_WITH([ffi],
                [AS_HELP_STRING(--with-ffi,use libffi to call native functions)],
                [WITH_FFI=yes
                 AC_CHECK_HEADERS([ffi.h],, [AC_MSG_ERROR(cannot find ffi.h)])
                 AC_CHECK_LIB(ffi, ffi_call,, [AC_MSG_ERROR(cannot find libffi)])
                 AC_DEFINE([WITH_FFI], 1, [use libffi])],
                [WITH_FFI=no])

    dnl check for libffcall
    AC_ARG_WITH([ffcall],
                [AS_HELP_STRING(--with-ffcall,use libffcall to call native functions)],
                [WITH_FFCALL=yes
                 AC_CHECK_HEADERS([avcall.h],, [AC_MSG_ERROR(cannot find avcall.h)])
                 AC_CHECK_LIB(avcall, __builtin_avcall,, [AC_MSG_ERROR(cannot find libavcall (from libffcall))])
                 AC_DEFINE([WITH_FFCALL], 1, [use libffcall])],
                [WITH_FFCALL=no])

    dnl check for libffi or libffcall
    if test x"${WITH_FFI}" = "xyes" -a x"${WITH_FFCALL}" = "xyes"; then
        AC_ERROR(Enable only libffi or libffcall.)
    fi
    if test x"${WITH_FFI}" = "xno" -a x"${WITH_FFCALL}" = "xno"; then
        AC_ERROR(Enable one of libffi or libffcall.)
    fi

else
    AC_MSG_RESULT(no)
    AM_CONDITIONAL([ENABLE_INTRP], [false])
    ENABLE_INTRP="no"
fi

if test x"${ENABLE_JIT}" = "xno" -a x"${ENABLE_INTRP}" = "xno"; then
    AC_ERROR(You have to enable at least the JIT or the interpreter.)
fi


dnl check for garbage collector usage
AC_ARG_ENABLE([gc], [AS_HELP_STRING(--enable-gc,enable garbage collector support (none,boehm,cacao) [[default=boehm]])])
AC_MSG_CHECKING(whether GC should be enabled)
enable_gc=${enable_gc:-boehm}
case "$enable_gc" in
no | none)
    AC_DEFINE([DISABLE_GC], 1, [disable garbage collector])
    ENABLE_GC=none
    AC_MSG_RESULT(no)
    ;;

boehm)
    AC_DEFINE([ENABLE_GC_BOEHM], 1, [enable conservative boehm-gc])
    ENABLE_GC=boehm
    AC_MSG_RESULT(yes, boehm-gc)
    ;;

cacao)
    AC_DEFINE([ENABLE_GC_CACAO], 1, [enable exact cacao-gc])
    ENABLE_GC=cacao
    AC_MSG_RESULT(yes, cacao-gc)
    ;;

*)
    AC_MSG_ERROR($enable_gc is an unknown garbage collector package)
    ;;
esac
AM_CONDITIONAL([DISABLE_GC], test x"${ENABLE_GC}" = "xnone")
AM_CONDITIONAL([ENABLE_GC_BOEHM], test x"${ENABLE_GC}" = "xboehm")
AM_CONDITIONAL([ENABLE_GC_CACAO], test x"${ENABLE_GC}" = "xcacao")



dnl check for dump memory usage
AC_MSG_CHECKING(whether dump memory should be disabled)
AC_ARG_ENABLE([dump],
              [AS_HELP_STRING(--disable-dump,disable dump memory (for debugging only!) [[default=yes]])],
              [case "${enableval}" in
                   no)
                       DISABLE_DUMP=yes
                       AC_DEFINE([DISABLE_DUMP], 1, [disable dump memory])
                       ;;
                   *) DISABLE_DUMP=no;;
               esac],
               [DISABLE_DUMP=no])
AC_MSG_RESULT(${DISABLE_DUMP})


dnl check for debug
AC_MSG_CHECKING(whether debug code generation should be enabled)
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING(--disable-debug,disable debug code generation [[default=yes]])],
              [case "${enableval}" in
                   no) NDEBUG=yes
                       AC_DEFINE([NDEBUG], 1, [disable debug code])
                       ;;
                   *) NDEBUG=no;;
               esac],
              [NDEBUG=no])

if test x"${NDEBUG}" = "xno"; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([NDEBUG], test x"${NDEBUG}" = "xyes")


dnl check for memcheck support
AC_MSG_CHECKING(whether debugging memory checks should be enabled)
AC_ARG_ENABLE([memcheck],
              [AS_HELP_STRING(--enable-memcheck,enable debugging memory checks [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_MEMCHECK=yes;;
                   *) ENABLE_MEMCHECK=no;;
               esac],
              [ENABLE_MEMCHECK=no])
AC_MSG_RESULT(${ENABLE_MEMCHECK})
AM_CONDITIONAL([ENABLE_MEMCHECK], test x"${ENABLE_MEMCHECK}" = "xyes")

if test x"${ENABLE_MEMCHECK}" = "xyes"; then
    AC_DEFINE([ENABLE_MEMCHECK], 1, [perform debugging memory checks])
fi


dnl check for vmlog support
AC_MSG_CHECKING(whether vmlog tracing should be enabled)
AC_ARG_ENABLE([vmlog],
              [AS_HELP_STRING(--enable-vmlog,enable vmlog tracing [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_VMLOG=yes;;
                   *) ENABLE_VMLOG=no;;
               esac],
              [ENABLE_VMLOG=no])
AC_MSG_RESULT(${ENABLE_VMLOG})
AM_CONDITIONAL([ENABLE_VMLOG], test x"${ENABLE_VMLOG}" = "xyes")

if test x"${ENABLE_VMLOG}" = "xyes"; then
    AC_DEFINE([ENABLE_VMLOG], 1, [enable vmlog tracing code])
fi


dnl check for disassembler support
AC_MSG_CHECKING(whether disassembler should be enabled)
AC_ARG_ENABLE([disassembler],
              [AS_HELP_STRING(--enable-disassembler,enable disassembler [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_DISASSEMBLER=yes;;
                   *) ENABLE_DISASSEMBLER=no;;
               esac],
              [ENABLE_DISASSEMBLER=no])
AC_MSG_RESULT(${ENABLE_DISASSEMBLER})
AM_CONDITIONAL([ENABLE_DISASSEMBLER], test x"${ENABLE_DISASSEMBLER}" = "xyes")

if test x"${ENABLE_DISASSEMBLER}" = "xyes"; then
    AC_DEFINE([ENABLE_DISASSEMBLER], 1, [enable disassembler])

    dnl check for binutils headers and libraries on some architectures for the
    dnl disassembler
    case "${ARCH_DIR}" in
        arm | i386 | powerpc | x86_64 | sparc64 | powerpc64 | s390 | m68k)
            AC_CHECK_HEADER([ansidecl.h],, [AC_MSG_ERROR(cannot find ansidecl.h)])
            AC_CHECK_HEADER([symcat.h],, [AC_MSG_ERROR(cannot find symcat.h)])
            AC_CHECK_HEADER([bfd.h],, [AC_MSG_ERROR(cannot find bfd.h)])
            AC_CHECK_HEADER([dis-asm.h],, [AC_MSG_ERROR(cannot find dis-asm.h)])

            case "${OS_DIR}" in
                cygwin | darwin | netbsd | solaris)
                     AC_CHECK_LIB(intl, dcgettext,, [AC_MSG_ERROR(cannot find libintl (from binutils))])
                     ;;
            esac

            AC_CHECK_LIB(iberty, xstrerror,, [AC_MSG_ERROR(cannot find libiberty (from binutils))])
            AC_CHECK_LIB(bfd, bfd_get_arch,, [AC_MSG_ERROR(cannot find libbfd (from binutils))])
            AC_CHECK_LIB(opcodes, disassembler,, [AC_MSG_ERROR(cannot find libopcodes (from binutils))])
            AC_DEFINE([WITH_BINUTILS_DISASSEMBLER], 1, [use binutils disassembler])
            AM_CONDITIONAL([WITH_BINUTILS_DISASSEMBLER], [true])
            ;;
        * )
            AM_CONDITIONAL([WITH_BINUTILS_DISASSEMBLER], [false])
            ;;
    esac
else
    AM_CONDITIONAL([WITH_BINUTILS_DISASSEMBLER], [false])
fi


dnl check for statistics
AC_MSG_CHECKING(whether statistics generation should be enabled)
AC_ARG_ENABLE([statistics],
              [AS_HELP_STRING(--enable-statistics,enable statistics generation [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_STATISTICS=yes;;
                   *) ENABLE_STATISTICS=no;;
               esac],
              [ENABLE_STATISTICS=no])
AC_MSG_RESULT(${ENABLE_STATISTICS})
AM_CONDITIONAL([ENABLE_STATISTICS], test x"${ENABLE_STATISTICS}" = "xyes")

if test x"${ENABLE_STATISTICS}" = "xyes"; then
    AC_DEFINE([ENABLE_STATISTICS], 1, [enable statistics])
fi


dnl check for verifier
AC_MSG_CHECKING(whether classfile verification should be enabled)
AC_ARG_ENABLE([verifier],
              [AS_HELP_STRING(--disable-verifier,disable classfile verification [[default=yes]])],
              [case "${enableval}" in
                   no) ENABLE_VERIFIER=no;;
                   *) ENABLE_VERIFIER=yes;;
               esac],
              [ENABLE_VERIFIER=yes])
AC_MSG_RESULT(${ENABLE_VERIFIER})
AM_CONDITIONAL([ENABLE_VERIFIER], test x"${ENABLE_VERIFIER}" = "xyes")

if test x"${ENABLE_VERIFIER}" = "xyes"; then
    AC_DEFINE([ENABLE_VERIFIER], 1, [enable classfile verification])
fi


dnl check for compiler timing
AC_MSG_CHECKING(whether real-time timing should be enabled)
AC_ARG_ENABLE([rt-timing],
              [AS_HELP_STRING(--enable-rt-timing,enable real-time timing [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_RT_TIMING=yes;;
                   *) ENABLE_RT_TIMING=no;;
               esac],
              [ENABLE_RT_TIMING=no])
AC_MSG_RESULT(${ENABLE_RT_TIMING})
AM_CONDITIONAL([ENABLE_RT_TIMING], test x"${ENABLE_RT_TIMING}" = "xyes")

if test x"${ENABLE_RT_TIMING}" = "xyes"; then
    AC_DEFINE([ENABLE_RT_TIMING], 1, [enable real-time timing])
fi


dnl check for cycle count statistics
AC_MSG_CHECKING(whether cycle count statistics should be enabled)
AC_ARG_ENABLE([cycles-stats],
              [AS_HELP_STRING(--enable-cycles-stats,enable cycle count statistics [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_CYCLES_STATS=yes;;
                   *) ENABLE_CYCLES_STATS=no;;
               esac],
              [ENABLE_CYCLES_STATS=no])
AC_MSG_RESULT(${ENABLE_CYCLES_STATS})
AM_CONDITIONAL([ENABLE_CYCLES_STATS], test x"${ENABLE_CYCLES_STATS}" = "xyes")

if test x"${ENABLE_CYCLES_STATS}" = "xyes"; then
    AC_DEFINE([ENABLE_CYCLES_STATS], 1, [enable cycle count statistics])
fi


dnl check for JVMTI
AC_MSG_CHECKING(whether to compile JVMTI support)
AC_ARG_ENABLE([jvmti],
              [AS_HELP_STRING(--enable-jvmti,enable JVMTI [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_JVMTI=yes;;
                   *) ENABLE_JVMTI=no;;
               esac],
              [ENABLE_JVMTI=no])
AC_MSG_RESULT(${ENABLE_JVMTI})
AM_CONDITIONAL([ENABLE_JVMTI], test x"${ENABLE_JVMTI}" = "xyes")

if test x"${ENABLE_JVMTI}" = "xyes"; then
    AC_DEFINE([ENABLE_JVMTI], 1, [use JVMTI])
fi


AC_CHECK_ENABLE_THREADS


dnl check if if-conversion should be supported
AC_MSG_CHECKING(whether if-conversion should be supported)
AC_ARG_ENABLE([ifconv],
              [AS_HELP_STRING(--disable-ifconv,disable if-conversion [[default=yes]])],
              [case "${enableval}" in
                   no) ENABLE_IFCONV=no;;
                   *) ENABLE_IFCONV=yes;;
               esac],
              [ENABLE_IFCONV=yes])
AC_MSG_RESULT(${ENABLE_IFCONV})
AM_CONDITIONAL([ENABLE_IFCONV], test x"${ENABLE_IFCONV}" = "xyes")
   
if test x"${ENABLE_IFCONV}" = "xyes"; then
    AC_DEFINE([ENABLE_IFCONV], 1, [enable if-conversion])
fi


dnl check for inlining
AC_MSG_CHECKING(whether method inlining should be supported)
AC_ARG_ENABLE([inlining],
              [AS_HELP_STRING(--enable-inlining,enable method inlining [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_INLINING=yes;;
                   *) ENABLE_INLINING=no;;
               esac],
              [ENABLE_INLINING=no])
AC_MSG_RESULT(${ENABLE_INLINING})
AM_CONDITIONAL([ENABLE_INLINING], test x"${ENABLE_INLINING}" = "xyes")

if test x"${ENABLE_INLINING}" = "xyes"; then
    AC_DEFINE([ENABLE_INLINING], 1, [use method inlining])
fi


dnl check for inlining debug options
AC_MSG_CHECKING(whether method inlining debug options should be enabled)
AC_ARG_ENABLE([inlining-debug],
              [AS_HELP_STRING(--enable-inlining-debug,enable method inlining debug options [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_INLINING_DEBUG=yes;;
                   *) ENABLE_INLINING_DEBUG=no;;
               esac],
              [ENABLE_INLINING_DEBUG=no])
AC_MSG_RESULT(${ENABLE_INLINING_DEBUG})
AM_CONDITIONAL([ENABLE_INLINING_DEBUG], test x"${ENABLE_INLINING_DEBUG}" = "xyes")

if test x"${ENABLE_INLINING_DEBUG}" = "xyes"; then
    AC_DEFINE([ENABLE_INLINING_DEBUG], 1, [enable method inlining debug options])
fi


dnl check for replacement
AC_MSG_CHECKING(whether on-stack replacement should be supported)
AC_ARG_ENABLE([replacement],
              [AS_HELP_STRING(--enable-replacement,enable on-stack replacement [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_REPLACEMENT=yes;;
                   *) ENABLE_REPLACEMENT=no;;
               esac],
              [ENABLE_REPLACEMENT=no])
AC_MSG_RESULT(${ENABLE_REPLACEMENT})
AM_CONDITIONAL([ENABLE_REPLACEMENT], test x"${ENABLE_REPLACEMENT}" = "xyes")

if test x"${ENABLE_REPLACEMENT}" = "xyes"; then
    AC_DEFINE([ENABLE_REPLACEMENT], 1, [use on-stack replacement])
fi


dnl check for loop optimization
AC_MSG_CHECKING(whether loop optimization should be supported)
AC_ARG_ENABLE([loop],
              [AS_HELP_STRING(--enable-loop,enable loop optimization [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_LOOP=yes;;
                   *) ENABLE_LOOP=no;;
               esac],
              [ENABLE_LOOP=no])
AC_MSG_RESULT(${ENABLE_LOOP})
AM_CONDITIONAL([ENABLE_LOOP], test x"${ENABLE_LOOP}" = "xyes")

if test x"${ENABLE_LOOP}" = "xyes"; then
    AC_DEFINE([ENABLE_LOOP], 1, [use loop optimization])
fi


dnl check if linear scan register allocator(lsra) with SSA should be used
AC_MSG_CHECKING(whether lsra with ssa should be supported)
AC_ARG_ENABLE([ssa],
              [AS_HELP_STRING(--disable-ssa,disable ssa [[default=no]])],
              [case "${enableval}" in
                   no) ENABLE_SSA=no;;
                   *) ENABLE_SSA=yes;;
               esac],
              [ENABLE_SSA=no])
AC_MSG_RESULT(${ENABLE_SSA})
AM_CONDITIONAL([ENABLE_SSA], test x"${ENABLE_SSA}" = "xyes")

if test x"${ENABLE_SSA}" = "xyes"; then
    AC_DEFINE([ENABLE_SSA], 1, [enable lsra with ssa])
    ENABLE_LSRA="no"
fi


dnl check if linear scan register allocator(lsra) should be used
AC_MSG_CHECKING(whether lsra should be supported)
AC_ARG_ENABLE([lsra],
              [AS_HELP_STRING(--enable-lsra,enable linear scan register allocator [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_LSRA=yes;;
                   *) ENABLE_LSRA=no;;
               esac],
              [ENABLE_LSRA=no])
   
if test x"${ENABLE_LSRA}" = "xyes"; then
    if test x"${ENABLE_SSA}" = "xyes"; then
        ENABLE_LSRA="no"
    fi
fi
AC_MSG_RESULT(${ENABLE_LSRA})
AM_CONDITIONAL([ENABLE_LSRA], test x"${ENABLE_LSRA}" = "xyes")
 
if test x"${ENABLE_LSRA}" = "xyes"; then
    AC_DEFINE([ENABLE_LSRA], 1, [enable lsra])
fi


dnl check if profiling should be supported
AC_MSG_CHECKING(whether profiling should be supported)
AC_ARG_ENABLE([profiling],
              [AS_HELP_STRING(--enable-profiling,enable profiling [[default=no]])],
              [case "${enableval}" in
                   yes) ENABLE_PROFILING=yes;;
                   *) ENABLE_PROFILING=no;;
               esac],
              [ENABLE_PROFILING=no])
AC_MSG_RESULT(${ENABLE_PROFILING})
AM_CONDITIONAL([ENABLE_PROFILING], test x"${ENABLE_PROFILING}" = "xyes")
   
if test x"${ENABLE_PROFILING}" = "xyes"; then
    AC_DEFINE([ENABLE_PROFILING], 1, [enable profiling])
fi


dnl check for instruction scheduler
AC_ARG_ENABLE([scheduler], [AS_HELP_STRING(--enable-scheduler,enable instruction scheduler [[default=no]])])

AC_MSG_CHECKING(whether instruction scheduler should be supported)
if test x"${USE_SCHEDULER}" = "x1" -o x"$enable_scheduler" = "xyes"; then
	case "${ARCH_DIR}" in
		xdspcore )
			;;
		* )
			AC_MSG_ERROR(instruction scheduler is not available for ${ARCH_DIR} code generator)
			;;
	esac

	AC_MSG_RESULT(yes)
	AC_DEFINE([USE_SCHEDULER], 1, [use scheduler])
	AM_CONDITIONAL([USE_SCHEDULER], [true])
else
	AC_MSG_RESULT(no)
	AM_CONDITIONAL([USE_SCHEDULER], [false])
fi


dnl check if zlib should be used
AC_MSG_CHECKING(whether ZIP/JAR archives should be supported)
AC_ARG_ENABLE([zlib],
              [AS_HELP_STRING(--disable-zlib,disable ZIP/JAR archive support (needs zlib) [[default=yes]])],
              [case "${enableval}" in
                  no) ENABLE_ZLIB=no;;
                  *) ENABLE_ZLIB=yes;;
               esac],
              [ENABLE_ZLIB=yes])
AC_MSG_RESULT(${ENABLE_ZLIB})

if test x"${ENABLE_ZLIB}" = "xyes"; then
    AC_CHECK_HEADERS([zconf.h],, [AC_MSG_ERROR(cannot find zconf.h)])
    AC_CHECK_HEADERS([zlib.h],, [AC_MSG_ERROR(cannot find zlib.h)])
    AC_CHECK_LIB(z, inflate,, [AC_MSG_ERROR(cannot find libz)])
    AC_DEFINE([ENABLE_ZLIB], 1, [use zlib])
fi
AM_CONDITIONAL([ENABLE_ZLIB], test x"${ENABLE_ZLIB}" = "xyes")


dnl check for stuff to be built
AC_CHECK_ENABLE_JNI


dnl check if a libjvm.so should be built
AC_MSG_CHECKING(whether to build a libjvm.so)
AC_ARG_ENABLE([libjvm],
              [AS_HELP_STRING(--disable-libjvm,build a libjvm.so [[default=yes]])],
              [case "${enableval}" in
                  no) ENABLE_LIBJVM=no;;
                  *) ENABLE_LIBJVM=yes;;
               esac],
              [ENABLE_LIBJVM=yes])
AC_MSG_RESULT(${ENABLE_LIBJVM})
AM_CONDITIONAL([ENABLE_LIBJVM], test x"${ENABLE_LIBJVM}" = "xyes")
AC_SUBST(ENABLE_LIBJVM)

if test x"${ENABLE_LIBJVM}" = "xyes"; then
    AC_DEFINE([ENABLE_LIBJVM], 1, [enable libjvm.so])

    dnl set AC_ENABLE_SHARED and AC_DISABLE_STATIC properly
    enable_shared=yes
    enable_static=no
else
    enable_shared=no
    enable_static=yes
fi

dnl libtool stuff
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL


dnl check if CACAO should be linked statically
AC_MSG_CHECKING(whether to link CACAO statically)
AC_ARG_ENABLE([staticvm],
              [AS_HELP_STRING(--enable-staticvm,link CACAO statically [[default=no]])],
              [case "${enableval}" in
                  yes) ENABLE_STATICVM=yes
                       AC_DEFINE([ENABLE_STATICVM], 1, [link CACAO statically])
                       ;;
                  *) ENABLE_STATICVM=no;;
               esac],
              [ENABLE_STATICVM=no])
AC_MSG_RESULT(${ENABLE_STATICVM})
AM_CONDITIONAL([ENABLE_STATICVM], test x"${ENABLE_STATICVM}" = "xyes")
AC_SUBST(ENABLE_STATICVM)


dnl check for embedded classes
AC_MSG_CHECKING(whether to embedd classes)
AC_ARG_ENABLE([embedded-classes],
            [AS_HELP_STRING(--enable-embedded-classes, statically embedded class files) [[default=no]])],
			[case "${enableval}" in
				yes) ENABLE_EMBEDDED_CLASSES=yes
					AC_DEFINE([ENABLE_EMBEDDED_CLASSES], 1, [statically embedd class files])
					;;
				*) ENABLE_EMBEDDED_CLASSES=no;;
			esac],
			[ENABLE_EMBEDDED_CLASSES=no])
AC_MSG_RESULT(${ENABLE_EMBEDDED_CLASSES})
AM_CONDITIONAL([ENABLE_EMBEDDED_CLASSES], test x"${ENABLE_EMBEDDED_CLASSES}" == "xyes")
AC_SUBST(ENABLE_EMBEDDED_CLASSES)

AC_MSG_CHECKING(whether to link CACAO statically)
AC_ARG_ENABLE([staticvm],
              [AS_HELP_STRING(--enable-staticvm,link CACAO statically [[default=no]])],
              [case "${enableval}" in
                  yes) ENABLE_STATICVM=yes
                       AC_DEFINE([ENABLE_STATICVM], 1, [link CACAO statically])
                       ;;
                  *) ENABLE_STATICVM=no;;
               esac],
              [ENABLE_STATICVM=no])
AC_MSG_RESULT(${ENABLE_STATICVM})
AM_CONDITIONAL([ENABLE_STATICVM], test x"${ENABLE_STATICVM}" = "xyes")
AC_SUBST(ENABLE_STATICVM)


dnl where is CACAO's vm.zip
AC_MSG_CHECKING(where CACAO's vm.zip is installed)
AC_ARG_WITH([vm-zip],
            [AS_HELP_STRING(--with-vm-zip=<path>,path to CACAO's vm.zip (includes the name of the file and may be flat) [[default=/usr/local/cacao/share/cacao/vm.zip]])],
            [CACAO_VM_ZIP=${withval}],
            [CACAO_VM_ZIP=${CACAO_PREFIX}/share/cacao/vm.zip])
AC_MSG_RESULT(${CACAO_VM_ZIP})
AC_DEFINE_UNQUOTED([CACAO_VM_ZIP], "${CACAO_VM_ZIP}", [CACAO's vm.zip])
AC_SUBST(CACAO_VM_ZIP)

AC_CHECK_WITH_CLASSPATH
AC_CHECK_WITH_CLASSPATH_PREFIX
AC_CHECK_WITH_CLASSPATH_CLASSES
AC_CHECK_WITH_CLASSPATH_LIBDIR
AC_CHECK_WITH_CLASSPATH_INCLUDEDIR


dnl if we compile for a JRE-style directory layout
AC_MSG_CHECKING(if we compile for a JRE-style directory layout)
AC_ARG_WITH([jre-layout],
            [AS_HELP_STRING(--with-jre-layout,compile for JRE-style directory layout [[default=no]])],
            [case "${enableval}" in
                yes) WITH_JRE_LAYOUT=yes
                     AC_DEFINE([WITH_JRE_LAYOUT], 1, [with JRE layout])
                     ;;
                *) WITH_JRE_LAYOUT=no;;
             esac],
            [WITH_JRE_LAYOUT=no])
AC_MSG_RESULT(${WITH_JRE_LAYOUT})


dnl check if GNU Classpath should be linked statically into CACAO
AC_MSG_CHECKING(whether to link GNU Classpath native libraries statically into CACAO)
AC_ARG_WITH([static-classpath],
            [AS_HELP_STRING(--with-static-classpath,link GNU classpath native libraries statically into CACAO [[default=no]])],
            [case "${enableval}" in
                yes) WITH_STATIC_CLASSPATH=yes;;
                *) WITH_STATIC_CLASSPATH=no;;
             esac],
            [WITH_STATIC_CLASSPATH=no])
AC_MSG_RESULT(${WITH_STATIC_CLASSPATH})

if test x"${WITH_STATIC_CLASSPATH}" = "xyes"; then
    AC_DEFINE([WITH_STATIC_CLASSPATH], 1, [link GNU classpath native libraries statically into CACAO])

    dnl check for some classpath related stuff when we link statically
    AM_ICONV_LINK
else
	if test x"${ENABLE_EMBEDDED_CLASSES}" != "xyes"; then
		dnl we need this check for --enable-staticvm, otherwise ltdl can't find dlopen
		if test x"${ENABLE_STATICVM}" = "xyes"; then
			AC_CHECK_LIB(dl, dlopen,, [AC_MSG_ERROR(cannot find libdl)])
		fi

		AC_CHECK_HEADERS([ltdl.h],, [AC_MSG_ERROR(cannot find ltdl.h)])
		AC_CHECK_LIB(ltdl, lt_dlopen,, [AC_MSG_ERROR(cannot find libltdl)])
    fi
fi
AM_CONDITIONAL([WITH_STATIC_CLASSPATH], test x"${WITH_STATIC_CLASSPATH}" = "xyes")
AC_SUBST(WITH_STATIC_CLASSPATH)


dnl check for some programs we need
AC_PROG_JAVAC
AC_PROG_JAR


dnl finally pass CFLAGS to Makefiles via AM_CFLAGS
CFLAGS=$OPT_CFLAGS
AM_CFLAGS=$ARCH_CFLAGS
AC_SUBST(AM_CFLAGS)


AC_CONFIG_FILES([Makefile]
		[contrib/Makefile]
		[contrib/setenvinstalled]
		[contrib/setenvsource]
		[contrib/vmlog/Makefile]
		[contrib/vmlog/t/Makefile]
		[doc/Makefile]
		[doc/handbook/Makefile]
		[man/Makefile]
		[src/Makefile]
		[src/cacao/Makefile]
		[src/cacaoh/Makefile]
		[src/fdlibm/Makefile]
		[src/lib/Makefile]
		[src/mm/Makefile]
		[src/mm/cacao-gc/Makefile]
		[src/native/Makefile]
		[src/native/include/Makefile]
		[src/native/jvmti/Makefile]
		[src/native/tools/Makefile]
		[src/native/vm/Makefile]
		[src/native/vm/cldc1.1/Makefile]
		[src/native/vm/gnu/Makefile]
		[src/scripts/Makefile]
		[src/scripts/java]
		[src/threads/Makefile]
		[src/threads/native/Makefile]
		[src/threads/none/Makefile]
		[src/toolbox/Makefile]
		[src/vm/Makefile]
		[src/vm/jit/Makefile]
		[src/vm/jit/allocator/Makefile]
		[src/vm/jit/alpha/Makefile]
		[src/vm/jit/alpha/freebsd/Makefile]
		[src/vm/jit/alpha/linux/Makefile]
		[src/vm/jit/arm/Makefile]
		[src/vm/jit/arm/linux/Makefile]
		[src/vm/jit/i386/Makefile]
		[src/vm/jit/i386/cygwin/Makefile]
		[src/vm/jit/i386/darwin/Makefile]
		[src/vm/jit/i386/freebsd/Makefile]
		[src/vm/jit/i386/linux/Makefile]
		[src/vm/jit/intrp/Makefile]
		[src/vm/jit/inline/Makefile]
		[src/vm/jit/loop/Makefile]
		[src/vm/jit/m68k/Makefile]
		[src/vm/jit/m68k/linux/Makefile]
		[src/vm/jit/mips/Makefile]
		[src/vm/jit/mips/irix/Makefile]
		[src/vm/jit/mips/linux/Makefile]
		[src/vm/jit/mips/uclinux/Makefile]
		[src/vm/jit/optimizing/Makefile]
		[src/vm/jit/parisc/Makefile]
		[src/vm/jit/parisc/linux/Makefile]
		[src/vm/jit/powerpc/Makefile]
		[src/vm/jit/powerpc/darwin/Makefile]
		[src/vm/jit/powerpc/linux/Makefile]
		[src/vm/jit/powerpc/netbsd/Makefile]
		[src/vm/jit/powerpc64/Makefile]
		[src/vm/jit/powerpc64/linux/Makefile]
		[src/vm/jit/s390/Makefile]
		[src/vm/jit/schedule/Makefile]
		[src/vm/jit/sparc64/Makefile]
		[src/vm/jit/sparc64/linux/Makefile]
		[src/vm/jit/sparc64/solaris/Makefile]
		[src/vm/jit/tools/Makefile]
		[src/vm/jit/verify/Makefile]
		[src/vm/jit/x86_64/Makefile]
		[src/vm/jit/x86_64/freebsd/Makefile]
		[src/vm/jit/x86_64/linux/Makefile]
		[src/vmcore/Makefile]
		[tests/Makefile]
		[tests/regression/Makefile]
		[tests/regression/codepatching/Makefile]
		[tests/regression/jasmin/Makefile]
		[tests/regression/native/Makefile]
		[tests/regression/resolving/Makefile]
		[tests/regression/resolving/classes1/Makefile]
		[tests/regression/resolving/classes2/Makefile]
		[tests/regression/resolving/classes3/Makefile]
		)


dnl now configure subpackages with OPT_CFLAGS and ARCH_CFLAGS
export OPT_CFLAGS
export ARCH_CFLAGS
AC_CONFIG_SUBDIRS(src/mm/boehm-gc)


AC_OUTPUT


## Local variables:
## mode: m4
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
