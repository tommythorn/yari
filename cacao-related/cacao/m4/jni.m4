dnl m4/jni.m4
dnl
dnl Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel,
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
dnl $Id: configure.ac 7228 2007-01-19 01:13:48Z edwin $


dnl check if JNI should be enabled

AC_DEFUN([AC_CHECK_ENABLE_JNI],[
AC_MSG_CHECKING(whether JNI should be enabled)
AC_ARG_ENABLE([jni],
              [AS_HELP_STRING(--enable-jni,enable JNI [[default=yes]])],
              [case "${enableval}" in
                  yes)
                      ENABLE_JNI=yes
                      ;;
                  no)
                      ENABLE_JNI=no
                      ;;
                  *)
                      AC_CHECK_ENABLE_JNI_DEFAULT
                      ;;
               esac],
              [AC_CHECK_ENABLE_JNI_DEFAULT])
AC_MSG_RESULT(${ENABLE_JNI})
AM_CONDITIONAL([ENABLE_JNI], test x"${ENABLE_JNI}" = "xyes")

if test x"${ENABLE_JNI}" = "xyes"; then
    AC_DEFINE([ENABLE_JNI], 1, [enable JNI])
fi
])


dnl check for the default value of --enable-jni

AC_DEFUN([AC_CHECK_ENABLE_JNI_DEFAULT],[
if test x"${ENABLE_JAVAME_CLDC1_1}" = "xyes"; then
    ENABLE_JNI=no
else
    ENABLE_JNI=yes
fi
])
