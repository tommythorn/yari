dnl m4/cacaoh.m4
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


dnl check which cacaoh to use

AC_DEFUN([AC_CHECK_WITH_CACAOH],[
AC_MSG_CHECKING(which cacaoh to use (for crosscompilation))
AC_ARG_WITH([cacaoh],
            [AS_HELP_STRING(--with-cacaoh,which cacaoh to use [[default=$(top_builddir)/src/cacaoh/cacaoh]])],
            [CACAOH="${withval}"
             ENABLE_CACAOH=no
            ],
            [CACAOH=["\$(top_builddir)/src/cacaoh/cacaoh"
             ENABLE_CACAOH=yes
            ]])
AC_MSG_RESULT(${CACAOH})
AC_SUBST(CACAOH)
AM_CONDITIONAL([ENABLE_CACAOH], test x"${ENABLE_CACAOH}" = "xyes")
])
