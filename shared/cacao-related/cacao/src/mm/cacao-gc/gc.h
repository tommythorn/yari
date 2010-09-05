/* src/mm/cacao-gc/gc.h - main garbage collector header

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

   Authors: Michael Starzinger

   $Id$

*/


#ifdef GC_CONST
# error Why is the BoehmGC header included???
#endif


#ifndef _GC_H
#define _GC_H


/* Debugging ******************************************************************/

#define GC_DEBUGGING

#if !defined(NDEBUG) && defined(GC_DEBUGGING)
# include <assert.h>
# define GC_LOG(code) if (opt_verbosegc) { code; }
# define GC_ASSERT(assertion) assert(assertion)
#else
# define GC_LOG(code)
# define GC_ASSERT(assertion)
#endif


/* Helper Macros **************************************************************/

#define GC_SET_FLAGS(obj, flags)   ((obj)->hdrflags |=  (flags))
#define GC_CLEAR_FLAGS(obj, flags) ((obj)->hdrflags &= ~(flags))
#define GC_TEST_FLAGS(obj, flags)  ((obj)->hdrflags  &  (flags))

#define POINTS_INTO(ptr, ptr_start, ptr_end) \
	((void *) (ptr) > (ptr_start) && (void *) (ptr) < (ptr_end))


#endif /* _GC_H */

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
 * vim:noexpandtab:sw=4:ts=4:
 */
