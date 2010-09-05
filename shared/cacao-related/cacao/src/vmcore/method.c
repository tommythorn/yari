/* src/vmcore/method.c - method functions

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

   $Id: method.c 7573 2007-03-25 18:55:02Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "vm/builtin.h"
#include "vm/global.h"
#include "vm/resolve.h"

#include "vm/jit/methodheader.h"

#include "vm/jit_interface.h"

#include "vmcore/class.h"
#include "vmcore/linker.h"
#include "vmcore/loader.h"
#include "vmcore/method.h"
#include "vmcore/options.h"


#if !defined(NDEBUG) && defined(ENABLE_INLINING)
#define INLINELOG(code)  do { if (opt_inline_debug_log) { code } } while (0)
#else
#define INLINELOG(code)
#endif


/* method_free *****************************************************************

   Frees all memory that was allocated for this method.

*******************************************************************************/

void method_free(methodinfo *m)
{
	if (m->jcode)
		MFREE(m->jcode, u1, m->jcodelength);

	if (m->rawexceptiontable)
		MFREE(m->rawexceptiontable, raw_exception_entry, m->rawexceptiontablelength);

	code_free_code_of_method(m);

	if (m->stubroutine) {
		if (m->flags & ACC_NATIVE) {
			removenativestub(m->stubroutine);

		} else {
			removecompilerstub(m->stubroutine);
		}
	}
}


/* method_canoverwrite *********************************************************

   Check if m and old are identical with respect to type and
   name. This means that old can be overwritten with m.
	
*******************************************************************************/

bool method_canoverwrite(methodinfo *m, methodinfo *old)
{
	if (m->name != old->name)
		return false;

	if (m->descriptor != old->descriptor)
		return false;

	if (m->flags & ACC_STATIC)
		return false;

	return true;
}


/* method_vftbl_lookup *********************************************************

   Does a method lookup in the passed virtual function table.  This
   function does exactly the same thing as JIT, but additionally
   relies on the fact, that the methodinfo pointer is at the first
   data segment slot (even for compiler stubs).

*******************************************************************************/

methodinfo *method_vftbl_lookup(vftbl_t *vftbl, methodinfo* m)
{
	methodptr   mptr;
	methodptr  *pmptr;
	methodinfo *resm;                   /* pointer to new resolved method     */

	/* If the method is not an instance method, just return it. */

	if (m->flags & ACC_STATIC)
		return m;

	assert(vftbl);

	/* Get the method from the virtual function table.  Is this an
	   interface method? */

	if (m->class->flags & ACC_INTERFACE) {
		pmptr = vftbl->interfacetable[-(m->class->index)];
		mptr  = pmptr[(m - m->class->methods)];
	}
	else {
		mptr = vftbl->table[m->vftblindex];
	}

	/* and now get the codeinfo pointer from the first data segment slot */

	resm = code_get_methodinfo_for_pv(mptr);

	return resm;
}


/* method_get_parametertypearray ***********************************************

   Use the descriptor of a method to generate a java.lang.Class array
   which contains the classes of the parametertypes of the method.

   This function is called by java.lang.reflect.{Constructor,Method}.

*******************************************************************************/

java_objectarray *method_get_parametertypearray(methodinfo *m)
{
	methoddesc       *md;
	typedesc         *paramtypes;
	s4                paramcount;
    java_objectarray *oa;
	s4                i;
	classinfo        *c;

	md = m->parseddesc;

	/* is the descriptor fully parsed? */

	if (m->parseddesc->params == NULL)
		if (!descriptor_params_from_paramtypes(md, m->flags))
			return NULL;

	paramtypes = md->paramtypes;
	paramcount = md->paramcount;

	/* skip `this' pointer */

	if (!(m->flags & ACC_STATIC)) {
		paramtypes++;
		paramcount--;
	}

	/* create class-array */

	oa = builtin_anewarray(paramcount, class_java_lang_Class);

	if (oa == NULL)
		return NULL;

    /* get classes */

	for (i = 0; i < paramcount; i++) {
		if (!resolve_class_from_typedesc(&paramtypes[i], true, false, &c))
			return NULL;

		oa->data[i] = (java_objectheader *) c;
	}

	return oa;
}


/* method_get_exceptionarray ***************************************************

   Get the exceptions which can be thrown by a method.

*******************************************************************************/

java_objectarray *method_get_exceptionarray(methodinfo *m)
{
	java_objectarray *oa;
	classinfo        *c;
	s4                i;

	/* create class-array */

	oa = builtin_anewarray(m->thrownexceptionscount, class_java_lang_Class);

	if (oa == NULL)
		return NULL;

	/* iterate over all exceptions and store the class in the array */

	for (i = 0; i < m->thrownexceptionscount; i++) {
		c = resolve_classref_or_classinfo_eager(m->thrownexceptions[i], true);

		if (c == NULL)
			return NULL;

		oa->data[i] = (java_objectheader *) c;
	}

	return oa;
}


/* method_count_implementations ************************************************

   Count the implementations of a method in a class cone (a class and all its
   subclasses.)

   IN:
       m................the method to count
	   c................class at which to start the counting (this class and
	                    all its subclasses will be searched)

   OUT:
       *found...........if found != NULL, *found receives the method
	                    implementation that was found. This value is only
						meaningful if the return value is 1.

   RETURN VALUE:
       the number of implementations found

*******************************************************************************/

s4 method_count_implementations(methodinfo *m, classinfo *c, methodinfo **found)
{
	s4          count;
	methodinfo *mp;
	methodinfo *mend;
	classinfo  *child;

	count = 0;

	mp = c->methods;
	mend = mp + c->methodscount;

	for (; mp < mend; ++mp) {
		if (method_canoverwrite(mp, m)) {
			if (found)
				*found = mp;
			count++;
			break;
		}
	}

	for (child = c->sub; child != NULL; child = child->nextsub) {
		count += method_count_implementations(m, child, found);
	}

	return count;
}


/* method_add_to_worklist ******************************************************

   Add the method to the given worklist. If the method already occurs in
   the worklist, the worklist remains unchanged.

*******************************************************************************/

static void method_add_to_worklist(methodinfo *m, method_worklist **wl)
{
	method_worklist *wi;

	for (wi = *wl; wi != NULL; wi = wi->next)
		if (wi->m == m)
			return;

	wi = NEW(method_worklist);
	wi->next = *wl;
	wi->m = m;

	*wl = wi;
}


/* method_add_assumption_monomorphic *******************************************

   Record the assumption that the method is monomorphic.

   IN:
      m.................the method
	  caller............the caller making the assumption

*******************************************************************************/

void method_add_assumption_monomorphic(methodinfo *m, methodinfo *caller)
{
	method_assumption *as;

	/* XXX LOCKING FOR THIS FUNCTION? */

	/* check if we already have registered this assumption */

	for (as = m->assumptions; as != NULL; as = as->next) {
		if (as->context == caller)
			return;
	}

	/* register the assumption */

	as = NEW(method_assumption);
	as->next = m->assumptions;
	as->context = caller;

	m->assumptions = as;
}


/* method_break_assumption_monomorphic *****************************************

   Break the assumption that this method is monomorphic. All callers that
   have registered this assumption are added to the worklist.

   IN:
      m.................the method
	  wl................worklist where to add invalidated callers

*******************************************************************************/

void method_break_assumption_monomorphic(methodinfo *m, method_worklist **wl)
{
	method_assumption *as;

	/* XXX LOCKING FOR THIS FUNCTION? */

	for (as = m->assumptions; as != NULL; as = as->next) {
		INLINELOG(
			printf("ASSUMPTION BROKEN (monomorphism): ");
			method_print(m);
			printf(" in ");
			method_println(as->context);
		);

		method_add_to_worklist(as->context, wl);
	}
}


/* method_printflags ***********************************************************

   Prints the flags of a method to stdout like.

*******************************************************************************/

#if !defined(NDEBUG)
void method_printflags(methodinfo *m)
{
	if (m == NULL) {
		printf("NULL");
		return;
	}

	if (m->flags & ACC_PUBLIC)       printf(" PUBLIC");
	if (m->flags & ACC_PRIVATE)      printf(" PRIVATE");
	if (m->flags & ACC_PROTECTED)    printf(" PROTECTED");
   	if (m->flags & ACC_STATIC)       printf(" STATIC");
   	if (m->flags & ACC_FINAL)        printf(" FINAL");
   	if (m->flags & ACC_SYNCHRONIZED) printf(" SYNCHRONIZED");
   	if (m->flags & ACC_VOLATILE)     printf(" VOLATILE");
   	if (m->flags & ACC_TRANSIENT)    printf(" TRANSIENT");
   	if (m->flags & ACC_NATIVE)       printf(" NATIVE");
   	if (m->flags & ACC_INTERFACE)    printf(" INTERFACE");
   	if (m->flags & ACC_ABSTRACT)     printf(" ABSTRACT");
   	if (m->flags & ACC_METHOD_MONOMORPHIC) printf(" (mono)");
   	if (m->flags & ACC_METHOD_IMPLEMENTED) printf(" (impl)");
}
#endif /* !defined(NDEBUG) */


/* method_print ****************************************************************

   Prints a method to stdout like:

   java.lang.Object.<init>()V

*******************************************************************************/

#if !defined(NDEBUG)
void method_print(methodinfo *m)
{
	if (m == NULL) {
		printf("NULL");
		return;
	}

	utf_display_printable_ascii_classname(m->class->name);
	printf(".");
	utf_display_printable_ascii(m->name);
	utf_display_printable_ascii(m->descriptor);

	method_printflags(m);
}
#endif /* !defined(NDEBUG) */


/* method_println **************************************************************

   Prints a method plus new line to stdout like:

   java.lang.Object.<init>()V

*******************************************************************************/

#if !defined(NDEBUG)
void method_println(methodinfo *m)
{
	if (opt_debugcolor) printf("\033[31m");	/* red */
	method_print(m);
	if (opt_debugcolor) printf("\033[m");	
	printf("\n");
}
#endif /* !defined(NDEBUG) */


/* method_methodref_print ******************************************************

   Prints a method reference to stdout.

*******************************************************************************/

#if !defined(NDEBUG)
void method_methodref_print(constant_FMIref *mr)
{
	if (!mr) {
		printf("(constant_FMIref *)NULL");
		return;
	}

	if (IS_FMIREF_RESOLVED(mr)) {
		printf("<method> ");
		method_print(mr->p.method);
	}
	else {
		printf("<methodref> ");
		utf_display_printable_ascii_classname(mr->p.classref->name);
		printf(".");
		utf_display_printable_ascii(mr->name);
		utf_display_printable_ascii(mr->descriptor);
	}
}
#endif /* !defined(NDEBUG) */


/* method_methodref_println ****************************************************

   Prints a method reference to stdout, followed by a newline.

*******************************************************************************/

#if !defined(NDEBUG)
void method_methodref_println(constant_FMIref *mr)
{
	method_methodref_print(mr);
	printf("\n");
}
#endif /* !defined(NDEBUG) */


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
