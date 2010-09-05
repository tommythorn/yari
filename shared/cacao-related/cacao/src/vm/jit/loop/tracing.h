/* vm/jit/loop/tracing.h - trace functions header

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

   Authors: Christian Thalinger

   $Id: tracing.h 4357 2006-01-22 23:33:38Z twisti $

*/


#ifndef _TRACING_H
#define _TRACING_H

#include "vm/jit/jit.h"


typedef struct Trace Trace;


/*	
   This struct is needed to record the source of operands of intermediate code
   instructions. The instructions are scanned backwards and the stack is 
   analyzed in order to determine the type of operand.
*/
struct Trace {
	int type;                  /* the type of the operand						*/
	int neg;                   /* set if negated								*/
	int var;                   /* variable reference	for IVAR				*/
			                   /* array reference		for AVAR/ARRAY			*/
	int nr;                    /* instruction number in the basic block, where */
			                   /* the trace is defined							*/
	int constant;              /* constant value		for ICONST				*/
				               /* modifiers			for IVAR				*/
};


/* function protoypes *********************************************************/

Trace* create_trace(int type, int var, int constant, int nr);
Trace* add(Trace* a, Trace* b);
Trace* negate(Trace* a);
Trace* sub(Trace* a, Trace* b);
Trace* array_length(Trace* a);
Trace* tracing(basicblock *block, int index, int temp);

#endif /* _TRACING_H */


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
