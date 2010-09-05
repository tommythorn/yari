/* mm/cacao-gc/mark.c - GC module for marking heap objects

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


#include "config.h"

#include "heap.h"
#include "toolbox/logging.h"
#include "vm/global.h"
#include "vm/linker.h"
#include "vm/options.h"


/* Debugging ******************************************************************/

int mark_depth;
int mark_depth_max;
#define MARK_DEPTH_INIT { mark_depth = 0; mark_depth_max = 0; }
#define MARK_DEPTH_INC { mark_depth++; if (mark_depth>mark_depth_max) mark_depth_max=mark_depth; }
#define MARK_DEPTH_DEC { mark_depth--; GC_ASSERT(mark_depth >= 0); }
void mark_println_stats()
{
	printf("Maximal marking depth: %d\n", mark_depth_max);
}


/* mark_recursice **************************************************************

   Recursively mark all objects (including this) which are referenced.

   IN:
	  o.....heap-object to be marked (either OBJECT or ARRAY)

*******************************************************************************/

void mark_recursive(java_objectheader *o)
{
	vftbl_t           *t;
	classinfo         *c;
	fieldinfo         *f;
	java_objectarray  *oa;
	arraydescriptor   *desc;
	java_objectheader *ref;
	int i;

	/* uncollectable objects should never get marked this way */
	GC_ASSERT(o);
	GC_ASSERT(!GC_TEST_FLAGS(o, GC_FLAG_UNCOLLECTABLE));

	/* mark this object */
	GC_SET_MARKED(o);

	/* get the class of this object */
	/* TODO: maybe we do not need this yet, look to move down! */
	t = o->vftbl;
	GC_ASSERT(t);
	c = t->class;
	GC_ASSERT(c);

	/* TODO: should we mark the class of the object as well? */
	/*GC_ASSERT(GC_IS_MARKED((java_objectheader *) c));*/

	/* does this object has pointers? */
	/* TODO: check how often this happens, maybe remove this check! */
	/*if (!GC_IS_REFERENCING(o))
		return;*/

	/* check if we are marking an array */
	if ((desc = t->arraydesc) != NULL) {
		/* this is an ARRAY */

		/* check if the array contains references */
		if (desc->arraytype != ARRAYTYPE_OBJECT)
			return;

		/* for object-arrays we need to check every entry */
		oa = (java_objectarray *) o;
		for (i = 0; i < oa->header.size; i++) {

			/* load the reference value */
			ref = (java_objectheader *) (oa->data[i]);

			/* check for outside or null pointers */
			if (!POINTS_INTO(ref, heap_base, heap_ptr))
				continue;

			GC_LOG( printf("Found (%p) from Array\n", (void *) ref); );

			/* do the recursive marking */
			if (!GC_IS_MARKED(ref)) {
				MARK_DEPTH_INC;
				mark_recursive(ref);
				MARK_DEPTH_DEC;
			}

		}

	} else {
		/* this is an OBJECT */

		/* for objects we need to check all (non-static) fields */
		for (i = 0; i < c->fieldscount; i++) {
			f = &(c->fields[i]);

			/* check if this field contains a non-static reference */
			if (!IS_ADR_TYPE(f->type) || (f->flags & ACC_STATIC))
				continue;

			/* load the reference value */
			ref = *( (java_objectheader **) ((s1 *) o + f->offset) );

			/* check for outside or null pointers */
			if (!POINTS_INTO(ref, heap_base, heap_ptr))
				continue;

			GC_LOG( printf("Found (%p) from Field ", (void *) ref);
					field_print(f); printf("\n"); );

			/* do the recursive marking */
			if (!GC_IS_MARKED(ref)) {
				MARK_DEPTH_INC;
				mark_recursive(ref);
				MARK_DEPTH_DEC;
			}

		}

	}

}


/* mark ************************************************************************

   Marks all Heap Objects which are reachable from a given root-set.

*******************************************************************************/

/* rootset is passed as array of pointers, which point to the location of
   the reference */
/* TODO: this definitely has to change!!! */
typedef java_objectheader** rootset_t;

void mark(rootset_t *rootset, int rootset_size)
{
	java_objectheader *ref;
	int i;

	/* recursively mark all references of the rootset */
	MARK_DEPTH_INIT;
	MARK_DEPTH_INC;
	for (i = 0; i < rootset_size; i++) {

		ref = *( rootset[i] );
		mark_recursive(ref);

	}
	MARK_DEPTH_DEC;
	GC_ASSERT(mark_depth == 0);
	GC_ASSERT(mark_depth_max > 0);

	GC_LOG( mark_println_stats(); );
}


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
