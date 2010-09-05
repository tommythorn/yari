/* src/vmcore/annotation.h - class annotations

   Copyright (C) 2006, 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   $Id: utf8.h 5920 2006-11-05 21:23:09Z twisti $

*/


#ifndef _ANNOTATION_H
#define _ANNOTATION_H

/* forward typedefs ***********************************************************/

typedef struct annotation_t    annotation_t;
typedef struct element_value_t element_value_t;

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"

#include "vmcore/loader.h"
#include "vmcore/utf8.h"


/* annotation *****************************************************************/

struct annotation_t {
	utf             *type;
	s4               element_valuescount;
	element_value_t *element_values;
};


/* element_value **************************************************************/

struct element_value_t {
	utf *name;
	u1   tag;
};


/* function prototypes ********************************************************/

bool annotation_load_attribute_runtimevisibleannotations(classbuffer *cb);

#endif /* _ANNOTATION_H */


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
