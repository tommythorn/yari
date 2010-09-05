/* src/toolbox/hashtable.h - functions for internal hashtables

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
            Christian Thalinger

   $Id: hashtable.h 7246 2007-01-29 18:49:05Z twisti $

*/


#ifndef _HASHTABLE_H
#define _HASHTABLE_H

/* forward typedefs ***********************************************************/

typedef struct hashtable hashtable;


#include "config.h"
#include "vm/types.h"

#include "vm/global.h"
#include "vmcore/utf8.h"


/* data structures for hashtables ********************************************

   All utf-symbols, javastrings and classes are stored in global
   hashtables, so every symbol exists only once. Equal symbols have
   identical pointers.  The functions for adding hashtable elements
   search the table for the element with the specified name/text and
   return it on success. Otherwise a new hashtable element is created.

   The hashtables use external linking for handling collisions. The
   hashtable structure contains a pointer <ptr> to the array of
   hashtable slots. The number of hashtable slots and therefore the
   size of this array is specified by the element <size> of hashtable
   structure. <entries> contains the number of all hashtable elements
   stored in the table, including those in the external chains.  The
   hashtable element structures (utf, literalstring, classinfo)
   contain both a pointer to the next hashtable element as a link for
   the external hash chain and the key of the element. The key is
   computed from the text of the string or the classname by using up
   to 8 characters.
	
   If the number of entries in the hashtable exceeds twice the size of
   the hashtableslot-array it is supposed that the average length of
   the external chains has reached a value beyond 2. Therefore the
   functions for adding hashtable elements (utf_new, class_new,
   literalstring_new) double the hashtableslot-array. In this
   restructuring process all elements have to be inserted into the new
   hashtable and new external chains must be built.

   Example for the layout of a hashtable:

hashtable.ptr-->+-------------------+
                |                   |
                         ...
                |                   |
                +-------------------+   +-------------------+   +-------------------+
                | hashtable element |-->| hashtable element |-->| hashtable element |-->NULL
                +-------------------+   +-------------------+   +-------------------+
                | hashtable element |
                +-------------------+   +-------------------+   
                | hashtable element |-->| hashtable element |-->NULL
                +-------------------+   +-------------------+   
                | hashtable element |-->NULL
                +-------------------+
                |                   |
                         ...
                |                   |
                +-------------------+

*/


/* hashtable ******************************************************************/

struct hashtable {            
#if defined(ENABLE_THREADS)
	java_objectheader  *header;         /* required for locking               */
#endif
	u4                  size;           /* current size of the hashtable      */
	u4                  entries;        /* number of entries in the table     */
	void              **ptr;            /* pointer to hashtable               */
};


/* function prototypes ********************************************************/

/* create hashtable */
void hashtable_create(hashtable *hash, u4 size);

/* creates and resizes a hashtable */
hashtable *hashtable_resize(hashtable *hash, u4 size);

/* frees a hashtable */
void hashtable_free(hashtable *hash);

#endif /* _HASHTABLE_H */


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
