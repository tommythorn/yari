/* src/vmcore/references.h - references to classes/fields/methods

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

   $Id: references.h 7246 2007-01-29 18:49:05Z twisti $

*/

#ifndef _REFERENCES_H_
#define _REFERENCES_H_

/* forward typedefs ***********************************************************/

typedef struct constant_classref constant_classref;
typedef struct constant_FMIref   constant_FMIref;


/* constant_classref **********************************************************/

struct constant_classref {
	void             *pseudo_vftbl; /* for distinguishing it from classinfo   */
	struct classinfo *referer;    /* class containing the reference           */
	struct utf       *name;       /* name of the class refered to             */
};


/* classref_or_classinfo ******************************************************/

typedef union classref_or_classinfo {
	constant_classref *ref;       /* a symbolic class reference               */
	struct classinfo  *cls;       /* an already loaded class                  */
	void              *any;       /* used for general access (x != NULL,...)  */
} classref_or_classinfo;


/* parseddesc *****************************************************************/

typedef union parseddesc {
	struct typedesc   *fd;        /* parsed field descriptor                  */
	struct methoddesc *md;        /* parsed method descriptor                 */
	void              *any;       /* used for simple test against NULL        */
} parseddesc;


#include "config.h"
#include "vm/types.h"

#include "vm/global.h"

#include "vmcore/class.h"
#include "vmcore/descriptor.h"
#include "vmcore/field.h"
#include "vmcore/method.h"
#include "vmcore/utf8.h"


/*----------------------------------------------------------------------------*/
/* References                                                                 */
/*                                                                            */
/* This header files defines the following types used for references to       */
/* classes/methods/fields and descriptors:                                    */
/*                                                                            */
/*     classinfo *                a loaded class                              */
/*     constant_classref          a symbolic reference                        */
/*     classref_or_classinfo      a loaded class or a symbolic reference      */
/*                                                                            */
/*     constant_FMIref            a symb. ref. to a field/method/intf.method  */
/*                                                                            */
/*     typedesc *                 describes a field type                      */
/*     methoddesc *               descrives a method type                     */
/*     parseddesc                 describes a field type or a method type     */
/*----------------------------------------------------------------------------*/

/* structs ********************************************************************/

/* constant_FMIref ************************************************************/

struct constant_FMIref{     /* Fieldref, Methodref and InterfaceMethodref     */
	union {
		s4                 index;     /* used only within the loader          */
		constant_classref *classref;  /* class having this field/meth./intfm. */
		fieldinfo         *field;     /* resolved field                       */
		methodinfo        *method;    /* resolved method                      */
	} p;
	utf       *name;        /* field/method/interfacemethod name              */
	utf       *descriptor;  /* field/method/intfmeth. type descriptor string  */
	parseddesc parseddesc;  /* parsed descriptor                              */
};


/* macros *********************************************************************/

/* a value that never occurrs in classinfo.header.vftbl                       */
#define CLASSREF_PSEUDO_VFTBL ((void *) 1)

/* macro for testing if a classref_or_classinfo is a classref                 */
/* `reforinfo` is only evaluated once                                         */
#define IS_CLASSREF(reforinfo)  \
	((reforinfo).ref->pseudo_vftbl == CLASSREF_PSEUDO_VFTBL)

/* macro for testing if a constant_FMIref has been resolved                   */
/* `fmiref` is only evaluated once                                            */
#define IS_FMIREF_RESOLVED(fmiref)  \
	((fmiref)->p.classref->pseudo_vftbl != CLASSREF_PSEUDO_VFTBL)

/* the same as IS_CLASSREF, but also check against NULL */
#define IS_XCLASSREF(reforinfo)  \
	((reforinfo).any && IS_CLASSREF(reforinfo))

/* macro for casting a classref/classinfo * to a classref_or_classinfo        */
#define CLASSREF_OR_CLASSINFO(value) \
	(*((classref_or_classinfo *)(&(value))))

/* macro for accessing the name of a classref/classinfo                       */
#define CLASSREF_OR_CLASSINFO_NAME(value) \
	(IS_CLASSREF(value) ? (value).ref->name : (value).cls->name)

/* macro for accessing the class name of a method reference                   */
#define METHODREF_CLASSNAME(fmiref) \
	(IS_FMIREF_RESOLVED(fmiref) ? (fmiref)->p.method->class->name \
	 							: (fmiref)->p.classref->name)

/* macro for accessing the class name of a method reference                   */
#define FIELDREF_CLASSNAME(fmiref) \
	(IS_FMIREF_RESOLVED(fmiref) ? (fmiref)->p.field->class->name \
	 							: (fmiref)->p.classref->name)

/* initialize a constant_classref with referer `ref` and name `classname`     */

#define CLASSREF_INIT(c,ref,classname) \
    do { \
        (c).pseudo_vftbl = CLASSREF_PSEUDO_VFTBL; \
        (c).referer = (ref); \
        (c).name = (classname); \
    } while (0)

#endif /* _REFERENCES_H_ */

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

