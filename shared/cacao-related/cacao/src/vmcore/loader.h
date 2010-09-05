/* src/vmcore/loader.h - class loader header

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

   $Id: loader.h 7246 2007-01-29 18:49:05Z twisti $
*/


#ifndef _LOADER_H
#define _LOADER_H

/* forward typedefs ***********************************************************/

typedef struct classbuffer classbuffer;


#include "config.h"

#include <stdio.h>

#include "vm/types.h"

#include "vm/global.h"

#include "vmcore/descriptor.h"
#include "vmcore/class.h"
#include "vmcore/method.h"
#include "vmcore/references.h"
#include "vmcore/utf8.h"


/* constant pool entries *******************************************************

	All constant pool entries need a data structure which contain the entrys
	value. In some cases this structure exist already, in the remaining cases
	this structure must be generated:

		kind                      structure                     generated?
	----------------------------------------------------------------------
    CONSTANT_Class               constant_classref                  yes
    CONSTANT_Fieldref            constant_FMIref                    yes
    CONSTANT_Methodref           constant_FMIref                    yes
    CONSTANT_InterfaceMethodref  constant_FMIref                    yes
    CONSTANT_String              unicode                             no
    CONSTANT_Integer             constant_integer                   yes
    CONSTANT_Float               constant_float                     yes
    CONSTANT_Long                constant_long                      yes
    CONSTANT_Double              constant_double                    yes
    CONSTANT_NameAndType         constant_nameandtype               yes
    CONSTANT_Utf8                unicode                             no
    CONSTANT_UNUSED              -

*******************************************************************************/

typedef struct {            /* Integer                                        */
	s4 value;
} constant_integer;

	
typedef struct {            /* Float                                          */
	float value;
} constant_float;


typedef struct {            /* Long                                           */
	s8 value;
} constant_long;
	

typedef struct {            /* Double                                         */
	double value;
} constant_double;


typedef struct {            /* NameAndType (Field or Method)                  */
	utf *name;              /* field/method name                              */
	utf *descriptor;        /* field/method type descriptor string            */
} constant_nameandtype;


/* classbuffer ****************************************************************/

struct classbuffer {
	classinfo *class;                   /* pointer to classinfo structure     */
	u1        *data;                    /* pointer to byte code               */
	s4         size;                    /* size of the byte code              */
	u1        *pos;                     /* current read position              */
	char      *path;                    /* path to file (for debugging)       */
};


/* function prototypes ********************************************************/

/* initialize loader, load important systemclasses */
bool loader_init(void);

void loader_load_all_classes(void);

bool loader_skip_attribute_body(classbuffer *cb);

#if defined(ENABLE_JAVASE)
bool loader_load_attribute_signature(classbuffer *cb, utf **signature);
#endif

/* free resources */
void loader_close(void);

/* class loading functions */
classinfo *load_class_from_sysloader(utf *name);
classinfo *load_class_from_classloader(utf *name, java_objectheader *cl);
classinfo *load_class_bootstrap(utf *name);

/* (don't use the following directly) */
classinfo *load_class_from_classbuffer(classbuffer *cb);
classinfo *load_newly_created_array(classinfo *c,java_objectheader *loader);

#endif /* _LOADER_H */

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
