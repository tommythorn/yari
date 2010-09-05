/* src/vm/jit/optimizing/ssa.h - static single assignment form header

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

   $Id: ssa.h$

*/


#ifndef _SSA_H
#define _SSA_H

#include "vm/jit/optimizing/graph.h"

#if !defined(NDEBUG)
# include <assert.h>
# define SSA_DEBUG_CHECK
# define SSA_DEBUG_VERBOSE
#endif

#ifdef SSA_DEBUG_CHECK
# define _SSA_CHECK_BOUNDS(i,l,h) assert( ((i) >= (l)) && ((i) < (h)));
# define _SSA_ASSERT(a) assert((a));
#else
# define _SSA_CHECK_BOUNDS(i,l,h)
# define _SSA_ASSERT(a)
#endif

/* #define JOIN_PHI_LT */ /* build intereference graph for lt used in phi */
                          /* funcs and try to merge them */

/* function prototypes */
void ssa_init(jitdata *);
void ssa_set_local_def(lsradata *, int , int , int);
void ssa_set_use(lsradata *, int , int , int );
void ssa_set_interface(codegendata *, lsradata *, basicblock *);
void ssa(jitdata *, graphdata *);

#endif /* _SSA_H */

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
