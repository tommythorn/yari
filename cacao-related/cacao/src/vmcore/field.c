/* src/vmcore/field.c - field functions

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

   $Id: field.c 7246 2007-01-29 18:49:05Z twisti $

*/


#include "config.h"

#include <stdio.h>

#include "vm/types.h"

#include "vmcore/field.h"
#include "vmcore/references.h"
#include "vmcore/utf8.h"


/* field_free ******************************************************************

   Frees a fields' resources.

*******************************************************************************/

void field_free(fieldinfo *f)
{
	/* empty */
}


/* field_printflags ************************************************************

   (debugging only)

*******************************************************************************/

#if !defined(NDEBUG)
void field_printflags(fieldinfo *f)
{
	if (f == NULL) {
		printf("NULL");
		return;
	}

	if (f->flags & ACC_PUBLIC)       printf(" PUBLIC");
	if (f->flags & ACC_PRIVATE)      printf(" PRIVATE");
	if (f->flags & ACC_PROTECTED)    printf(" PROTECTED");
	if (f->flags & ACC_STATIC)       printf(" STATIC");
	if (f->flags & ACC_FINAL)        printf(" FINAL");
	if (f->flags & ACC_SYNCHRONIZED) printf(" SYNCHRONIZED");
	if (f->flags & ACC_VOLATILE)     printf(" VOLATILE");
	if (f->flags & ACC_TRANSIENT)    printf(" TRANSIENT");
	if (f->flags & ACC_NATIVE)       printf(" NATIVE");
	if (f->flags & ACC_INTERFACE)    printf(" INTERFACE");
	if (f->flags & ACC_ABSTRACT)     printf(" ABSTRACT");
}
#endif


/* field_print *****************************************************************

   (debugging only)

*******************************************************************************/

#if !defined(NDEBUG)
void field_print(fieldinfo *f)
{
	if (f == NULL) {
		printf("(fieldinfo*)NULL");
		return;
	}

	utf_display_printable_ascii_classname(f->class->name);
	printf(".");
	utf_display_printable_ascii(f->name);
	printf(" ");
	utf_display_printable_ascii(f->descriptor);	

	field_printflags(f);
}
#endif


/* field_println ***************************************************************

   (debugging only)

*******************************************************************************/

#if !defined(NDEBUG)
void field_println(fieldinfo *f)
{
	field_print(f);
	printf("\n");
}
#endif

/* field_fieldref_print ********************************************************

   (debugging only)

*******************************************************************************/

#if !defined(NDEBUG)
void field_fieldref_print(constant_FMIref *fr)
{
	if (fr == NULL) {
		printf("(constant_FMIref *)NULL");
		return;
	}

	if (IS_FMIREF_RESOLVED(fr)) {
		printf("<field> ");
		field_print(fr->p.field);
	}
	else {
		printf("<fieldref> ");
		utf_display_printable_ascii_classname(fr->p.classref->name);
		printf(".");
		utf_display_printable_ascii(fr->name);
		printf(" ");
		utf_display_printable_ascii(fr->descriptor);
	}
}
#endif

/* field_fieldref_println ******************************************************

   (debugging only)

*******************************************************************************/

#if !defined(NDEBUG)
void field_fieldref_println(constant_FMIref *fr)
{
	field_fieldref_print(fr);
	printf("\n");
}
#endif

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
