/* src/vm/jit/optimizing/dominators.h - Dominators and Dominance Frontier header

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

   $Id: dominators.h$

*/


#ifndef _DOMINATORS_H
#define _DOMINATORS_H

#include "vm/jit/optimizing/graph.h"

#if !defined(NDEBUG)
# include <assert.h>
/* # define DOM_DEBUG_CHECK */
# define DOM_DEBUG_VERBOSE
#endif

#ifdef DOM_DEBUG_CHECK
# define _DOM_CHECK_BOUNDS(i,l,h) assert( ((i) >= (l)) && ((i) < (h)));
# define _DOM_ASSERT(a) assert((a));
#else
# define _DOM_CHECK_BOUNDS(i,l,h)
# define _DOM_ASSERT(a)
#endif

struct dominatordata {
	int *dfnum;           /* [0..ls->basicblockcount[ */
	int *vertex;          /* [0..ls->basicblockcount[ */
	int *parent;          /* [0..ls->basicblockcount[ */
	int *semi;            /* [0..ls->basicblockcount[ */
	int *ancestor;        /* [0..ls->basicblockcount[ */
	int *idom;            /* [0..ls->basicblockcount[ */
	int *samedom;         /* [0..ls->basicblockcount[ */
	int **bucket;         /* [0..ls->basicblockcount[[0..ls->bbc[ */
	int *num_bucket;      /* [0..ls->basicblockcount[ */
	int *best;            /* [0..ls->basicblockcount[ */
	int **DF;             /* [0..ls->basicblockcount[[0..ls->bbc[ */
	int *num_DF;          /* [0..ls->basicblockcount[ */
};	

typedef struct dominatordata dominatordata;

/* function prototypes */
dominatordata *compute_Dominators(graphdata *gd, int basicblockcount);
void computeDF(graphdata *gd, dominatordata *dd, int basicblockcount, int n);

#endif /* _DOMINATORS_H */

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
