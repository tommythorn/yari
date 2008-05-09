/* src/toolbox/worklist.h - worklist header

   Copyright (C) 2005, 2006 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
   Institut f. Computersprachen - TU Wien

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.

   Contact: cacao@complang.tuwien.ac.at

   Authors: Christian Ullrich

   $Id: worklist.h$

*/


#ifndef _WORKLIST_H
#define _WORKLIST_H

#include "toolbox/bitvector.h"

#if !defined(NDEBUG)
#include <assert.h>

/* define WL_DEBUG_CHECK to activate the bound checks */

/* #define WL_DEBUG_CHECK */

/* no debug messages implemented till now */

/* #define WL_DEBUG_VERBOSE */

#endif

#if defined(WL_DEBUG_CHECK)
#define _WL_CHECK_BOUNDS(i,l,h) assert( ((i) >= (l)) && ((i) < (h)));
#define _WL_ASSERT(a) assert((a));
#else
#define _WL_CHECK_BOUNDS(i,l,h);
#define _WL_ASSERT(a);
#endif

struct worklist {
	int *W_stack;
	int W_top;
	bitvector W_bv;
#ifdef WL_DEBUG_CHECK
	int size;
#endif
};

typedef struct worklist worklist;

/* function prototypes */
worklist *wl_new(int size);
void wl_add(worklist *w, int element);
int wl_get(worklist *w);
bool wl_is_empty(worklist *w);


#endif /* _BITVECTOR_H */


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
