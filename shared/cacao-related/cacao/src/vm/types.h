/* src/vm/types.h - type definitions for CACAO's internal types

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, Institut f. Computersprachen - TU Wien

   This file is part of CACAO.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Contact: cacao@cacaojvm.org

   Authors: Reinhard Grafl
            Andreas  Krall

   Changes: Christian Thalinger

   $Id: types.h 4357 2006-01-22 23:33:38Z twisti $

*/


#ifndef _CACAO_TYPES_H
#define _CACAO_TYPES_H

#include "config.h"


/* In this file we check for unknown pointersizes, so we don't have to
   do this somewhere else. */

/* Define the sizes of the integer types used internally by CACAO. ************/

typedef signed char             s1;
typedef unsigned char           u1;
 
typedef signed short int        s2;
typedef unsigned short int      u2;

typedef signed int              s4;
typedef unsigned int            u4;

#if SIZEOF_VOID_P == 8
typedef signed long int         s8;
typedef unsigned long int       u8;
#elif SIZEOF_VOID_P == 4
typedef signed long long int    s8;
typedef unsigned long long int  u8;
#else
#error unknown pointer size
#endif


/* Define the size of a function pointer used in function pointer casts. ******/

#if SIZEOF_VOID_P == 8
typedef u8                      ptrint;
#else
typedef u4                      ptrint;
#endif

#endif /* _CACAO_TYPES_H */


/*
 * These are local overrides for various environment variables in Emacs.
 * Please do not remove this and leave it at the end of the file, where
 * Emacs will automagically detect them.
 * ---------------------------------------------------------------------
 * Local variables:
 * mode: c
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 */
