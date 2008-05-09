/* src/threads/none/threads.h - fake threads header

   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: threads.h 4405 2006-02-03 12:46:22Z twisti $

*/


#ifndef _THREADS_H
#define _THREADS_H

#include "config.h"
#include "vm/types.h"
#include "vm/global.h"


/* define some stuff we need to no-ops ****************************************/

#define THREADSPECIFIC
#define THREADOBJECT      NULL
#define THREADINFO        NULL

#define threadobject      void


/* exception pointer **********************************************************/

extern java_objectheader    *_no_threads_exceptionptr;

#define exceptionptr        (&_no_threads_exceptionptr)


/* stackframeinfo *************************************************************/

struct stackframeinfo;

extern struct stackframeinfo       *_no_threads_stackframeinfo;

#define STACKFRAMEINFO      (_no_threads_stackframeinfo)

#endif /* _THREADS_H */


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
