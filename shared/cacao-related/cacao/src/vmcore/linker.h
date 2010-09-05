/* src/vmcore/linker.h - class linker header

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

   $Id: linker.h 7675 2007-04-05 14:23:04Z michi $
*/


#ifndef _LINKER_H
#define _LINKER_H

/* forward typedefs ***********************************************************/

typedef struct _vftbl vftbl_t;
typedef struct arraydescriptor arraydescriptor;
typedef struct primitivetypeinfo primitivetypeinfo;


#include "config.h"
#include "vm/types.h"

#include "vmcore/class.h"
#include "vmcore/references.h"


/* virtual function table ******************************************************

   The vtbl has a bidirectional layout with open ends at both sides.
   interfacetablelength gives the number of entries of the interface
   table at the start of the vftbl. The vftbl pointer points to
   &interfacetable[0].  vftbllength gives the number of entries of
   table at the end of the vftbl.

   runtime type check (checkcast):

   Different methods are used for runtime type check depending on the
   argument of checkcast/instanceof.
	
   A check against a class is implemented via relative numbering on
   the class hierachy tree. The tree is numbered in a depth first
   traversal setting the base field and the diff field. The diff field
   gets the result of (high - base) so that a range check can be
   implemented by an unsigned compare. A sub type test is done by
   checking the inclusion of base of the sub class in the range of the
   superclass.

   A check against an interface is implemented via the
   interfacevftbl. If the interfacevftbl contains a nonnull value a
   class is a subclass of this interface.

   interfacetable:

   Like standard virtual methods interface methods are called using
   virtual function tables. All interfaces are numbered sequentially
   (starting with zero). For each class there exist an interface table
   of virtual function tables for each implemented interface. The
   length of the interface table is determined by the highest number
   of an implemented interface.

   The following example assumes a class which implements interface 0 and 3:

   interfacetablelength = 4

                  | ...       |            +----------+
	              +-----------+            | method 2 |---> method z
	              | class     |            | method 1 |---> method y
	              +-----------+            | method 0 |---> method x
	              | ivftbl  0 |----------> +----------+
	vftblptr ---> +-----------+
                  | ivftbl -1 |--> NULL    +----------+
                  | ivftbl -2 |--> NULL    | method 1 |---> method x
                  | ivftbl -3 |-----+      | method 0 |---> method a
                  +-----------+     +----> +----------+
     
                              +---------------+
	                          | length 3 = 2  |
	                          | length 2 = 0  |
	                          | length 1 = 0  |
	                          | length 0 = 3  |
	interfacevftbllength ---> +---------------+

*******************************************************************************/

struct _vftbl {
	methodptr   *interfacetable[1];    /* interface table (access via macro)  */
	classinfo   *class;                /* class, the vtbl belongs to          */
	arraydescriptor *arraydesc;        /* for array classes, otherwise NULL   */
	s4           vftbllength;          /* virtual function table length       */
	s4           interfacetablelength; /* interface table length              */
	s4           baseval;              /* base for runtime type check         */
	                                   /* (-index for interfaces)             */
	s4           diffval;              /* high - base for runtime type check  */
	s4          *interfacevftbllength; /* length of interface vftbls          */
	methodptr    table[1];             /* class vftbl                         */
};


/* arraydescriptor *************************************************************

   For every array class an arraydescriptor is allocated which
   describes the array class. The arraydescriptor is referenced from
   the vftbl of the array class.

*******************************************************************************/

struct arraydescriptor {
	vftbl_t *componentvftbl; /* vftbl of the component type, NULL for primit. */
	vftbl_t *elementvftbl;   /* vftbl of the element type, NULL for primitive */
	s2       arraytype;      /* ARRAYTYPE_* constant                          */
	s2       dimension;      /* dimension of the array (always >= 1)          */
	s4       dataoffset;     /* offset of the array data from object pointer  */
	s4       componentsize;  /* size of a component in bytes                  */
	s2       elementtype;    /* ARRAYTYPE_* constant                          */
};


/* primitivetypeinfo **********************************************************/

struct primitivetypeinfo {
	classinfo *class_wrap;               /* class for wrapping primitive type */
	classinfo *class_primitive;          /* primitive class                   */
	char      *wrapname;                 /* name of class for wrapping        */
	char       typesig;                  /* one character type signature      */
	char      *name;                     /* name of primitive class           */
	char      *arrayname;                /* name of primitive array class     */
	classinfo *arrayclass;               /* primitive array class             */
	vftbl_t   *arrayvftbl;               /* vftbl of primitive array class    */
};


/* global variables ***********************************************************/

/* This array can be indexed by the PRIMITIVETYPE_ and ARRAYTYPE_ constants   */
/* (except ARRAYTYPE_OBJECT).                                                 */

extern primitivetypeinfo primitivetype_table[PRIMITIVETYPE_COUNT];

/* This lock must be taken while renumbering classes or while atomically      */
/* accessing classes.                                                         */

extern java_objectheader *linker_classrenumber_lock;


/* function prototypes ********************************************************/

/* initialize the linker subsystem */
bool linker_init(void);

/* link a class */
classinfo *link_class(classinfo *c);

#endif /* _LINKER_H */


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
