/* src/vm/jit/powerpc/patcher.c - PowerPC code patching functions

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

   $Id: patcher.c 7596 2007-03-28 21:05:53Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "native/native.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/initialize.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/patcher.h"
#include "vm/jit/md.h"
#include "vm/jit/methodheader.h"
#include "vm/jit/stacktrace.h"

#include "vmcore/class.h"
#include "vmcore/field.h"
#include "vmcore/options.h"
#include "vm/resolve.h"
#include "vmcore/references.h"


/* patcher_wrapper *************************************************************

   Wrapper for all patchers.  It also creates the stackframe info
   structure.

   If the return value of the patcher function is false, it gets the
   exception object, clears the exception pointer and returns the
   exception.

*******************************************************************************/

java_objectheader *patcher_wrapper(u1 *sp, u1 *pv, u1 *ra)
{
	stackframeinfo     sfi;
	u1                *xpc;
	java_objectheader *o;
	u4                 mcode;
	functionptr        f;
	bool               result;
	java_objectheader *e;

	/* define the patcher function */

	bool (*patcher_function)(u1 *);

	assert(pv != NULL);

	/* get stuff from the stack */

	xpc = (u1 *)                *((ptrint *) (sp + 5 * 4));
	o   = (java_objectheader *) *((ptrint *) (sp + 4 * 4));
	f   = (functionptr)         *((ptrint *) (sp + 0 * 4));

	/* Correct RA is calculated in codegen.c and stored in the patcher
	   stub stack.  There's no need to adjust xpc. */

	/* store PV into the patcher function position */

	*((ptrint *) (sp + 0 * 4)) = (ptrint) pv;

	/* cast the passed function to a patcher function */

	patcher_function = (bool (*)(u1 *)) (ptrint) f;

	/* enter a monitor on the patching position */

	PATCHER_MONITORENTER;

	/* create the stackframeinfo */

	stacktrace_create_extern_stackframeinfo(&sfi, pv, sp + 8 * 4, ra, xpc);

	/* call the proper patcher function */

	result = (patcher_function)(sp);

	/* remove the stackframeinfo */

	stacktrace_remove_stackframeinfo(&sfi);

	/* check for return value and exit accordingly */

	if (result == false) {
		e = exceptions_get_and_clear_exception();

		PATCHER_MONITOREXIT;

		return e;
	}

	/* patch back original code */

	mcode = *((u4 *) (sp + 3 * 4));

	*((u4 *) xpc) = mcode;

	/* synchronize instruction cache */

	md_icacheflush(xpc, 4);

	PATCHER_MARK_PATCHED_MONITOREXIT;

	return NULL;
}


/* patcher_initialize_class ****************************************************

   Initalizes a given classinfo pointer.  This function does not patch
   any data.

*******************************************************************************/

bool patcher_initialize_class(u1 *sp)
{
	classinfo *c;

	/* get stuff from the stack */

	c = (classinfo *) *((ptrint *) (sp + 2 * 4));

	/* check if the class is initialized */

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	return true;
}


/* patcher_resolve_class *******************************************************

   Resolves a given unresolved_class pointer.  This function does not
   patch any data.

*******************************************************************************/

#ifdef ENABLE_VERIFIER
bool patcher_resolve_class(u1 *sp)
{
	unresolved_class *uc;

	/* get stuff from the stack */

	uc = (unresolved_class *) *((ptrint *) (sp + 2 * 4));

	/* resolve the class and check subtype constraints */

	if (!resolve_class_eager_no_access_check(uc))
		return false;

	return true;
}
#endif /* ENABLE_VERIFIER */


/* patcher_resolve_classref_to_classinfo ***************************************

   ACONST:

   <patched call postition>
   806dffc4    lwz   r3,-60(r13)
   81adffc0    lwz   r13,-64(r13)
   7da903a6    mtctr r13
   4e800421    bctrl


   MULTIANEWARRAY:

   <patched call position>
   808dffc0    lwz   r4,-64(r13)
   38a10038    addi  r5,r1,56
   81adffbc    lwz   r13,-68(r13)
   7da903a6    mtctr r13
   4e800421    bctrl


   ARRAYCHECKCAST:

   <patched call position>
   808dffd8    lwz   r4,-40(r13)
   81adffd4    lwz   r13,-44(r13)
   7da903a6    mtctr r13
   4e800421    bctrl

*******************************************************************************/

bool patcher_resolve_classref_to_classinfo(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 4));
	disp =                       *((s4 *)     (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_resolve_classref_to_vftbl *******************************************

   CHECKCAST (class):

   <patched call position>
   81870000    lwz   r12,0(r7)
   800c0014    lwz   r0,20(r12)
   818dff78    lwz   r12,-136(r13)


   INSTANCEOF (class):

   <patched call position>
   817d0000    lwz   r11,0(r29)
   818dff8c    lwz   r12,-116(r13)

*******************************************************************************/

bool patcher_resolve_classref_to_vftbl(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 4));
	disp =                       *((s4 *)     (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch super class' vftbl */

	*((ptrint *) (pv + disp)) = (ptrint) c->vftbl;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_resolve_classref_to_flags *******************************************

   CHECKCAST/INSTANCEOF:

   <patched call position>
   818dff7c    lwz   r12,-132(r13)

*******************************************************************************/

bool patcher_resolve_classref_to_flags(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 4));
	disp =                       *((s4 *)     (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch class flags */

	*((s4 *) (pv + disp)) = (s4) c->flags;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_resolve_native_function *********************************************

   XXX

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH)
bool patcher_resolve_native_function(u1 *sp)
{
	methodinfo  *m;
	s4           disp;
	u1          *pv;
	functionptr  f;

	/* get stuff from the stack */

	m    = (methodinfo *) *((ptrint *) (sp + 2 * 4));
	disp =                *((s4 *)     (sp + 1 * 4));
	pv   = (u1 *)         *((ptrint *) (sp + 0 * 4));

	/* resolve native function */

	if (!(f = native_resolve_function(m)))
		return false;

	/* patch native function pointer */

	*((ptrint *) (pv + disp)) = (ptrint) f;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


/* patcher_get_putstatic *******************************************************

   Machine code:

   <patched call position>
   816dffc8    lwz   r11,-56(r13)
   80ab0000    lwz   r5,0(r11)

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	u1               *ra;
	u4                mcode;
	unresolved_field *uf;
	s4                disp;
	u1               *pv;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	uf    = (unresolved_field *)  *((ptrint *) (sp + 2 * 4));
	disp  =                       *((s4 *)     (sp + 1 * 4));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* check if the field's class is initialized */

	if (!(fi->class->state & CLASS_INITIALIZED))
		if (!initialize_class(fi->class))
			return false;

	/* patch the field value's address */

	*((ptrint *) (pv + disp)) = (ptrint) &(fi->value);

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_get_putfield ********************************************************

   Machine code:

   <patched call position>
   811f0014    lwz   r8,20(r31)

*******************************************************************************/

bool patcher_get_putfield(u1 *sp)
{
	u1               *ra;
	unresolved_field *uf;
	u1               *pv;
	fieldinfo        *fi;
	s2                disp;

	ra = (u1 *)               *((ptrint *) (sp + 5 * 4));
	uf = (unresolved_field *) *((ptrint *) (sp + 2 * 4));
	pv = (u1 *)               *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* if we show NOPs, we have to skip them */

	if (opt_shownops) {
		/* patch the field's offset */

		if (IS_LNG_TYPE(fi->type)) {
			/* If the field has type long, we have to patch two
			   instructions.  But we have to check which instruction
			   is first.  We do that with the offset of the first
			   instruction. */

			disp = *((u4 *) (ra + 1 * 4));

			if (disp == 4) {
				*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
				*((u4 *) (ra + 2 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
			}
			else {
				*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
				*((u4 *) (ra + 2 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
			}
		}
		else
			*((u4 *) (ra + 1 * 4)) |= (s2) (fi->offset & 0x0000ffff);
	}
	else {
		if (IS_LNG_TYPE(fi->type)) {

			disp = *((u4 *) (sp + 3 * 4));

			/* We patch the first instruction in the patcher stub
			   stack and the second in the code.  The first
			   instruction is patched back later in
			   patcher_wrapper. */

			if (disp == 4) {
				*((u4 *) (sp + 3 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
				*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
			}
			else {
				*((u4 *) (sp + 3 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
				*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
			}
		}
		else
			*((u4 *) (sp + 3 * 4)) |= (s2) (fi->offset & 0x0000ffff);
	}

	/* synchronize instruction cache */

	md_icacheflush(ra + 1 * 4, 2 * 4);

	return true;
}


/* patcher_invokestatic_special ************************************************

   Machine code:

   <patched call position>
   81adffd8    lwz   r13,-40(r13)
   7da903a6    mtctr r13
   4e800421    bctrl

******************************************************************************/

bool patcher_invokestatic_special(u1 *sp)
{
	unresolved_method *um;
	s4                 disp;
	u1                *pv;
	methodinfo        *m;

	/* get stuff from the stack */

	um   = (unresolved_method *) *((ptrint *) (sp + 2 * 4));
	disp =                       *((s4 *)     (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch stubroutine */

	*((ptrint *) (pv + disp)) = (ptrint) m->stubroutine;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_invokevirtual *******************************************************

   Machine code:

   <patched call position>
   81830000    lwz   r12,0(r3)
   81ac0088    lwz   r13,136(r12)
   7da903a6    mtctr r13
   4e800421    bctrl

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;
	s4                 disp;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 5 * 4));
	um = (unresolved_method *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show NOPs, we have to skip them */

	if (opt_shownops)
		ra = ra + 1 * 4;

	/* patch vftbl index */

	disp = (OFFSET(vftbl_t, table[0]) + sizeof(methodptr) * m->vftblindex);

	*((s4 *) (ra + 1 * 4)) |= (disp & 0x0000ffff);

	/* synchronize instruction cache */

	md_icacheflush(ra + 1 * 4, 1 * 4);

	return true;
}


/* patcher_invokeinterface *****************************************************

   Machine code:

   <patched call position>
   81830000    lwz   r12,0(r3)
   818cffd0    lwz   r12,-48(r12)
   81ac000c    lwz   r13,12(r12)
   7da903a6    mtctr r13
   4e800421    bctrl

*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;
	s4                 disp;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 5 * 4));
	um = (unresolved_method *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show NOPs, we have to skip them */

	if (opt_shownops)
		ra = ra + 1 * 4;

	/* patch interfacetable index */

	disp = OFFSET(vftbl_t, interfacetable[0]) -
		sizeof(methodptr*) * m->class->index;

	/* XXX TWISTI: check displacement */

	*((s4 *) (ra + 1 * 4)) |= (disp & 0x0000ffff);

	/* patch method offset */

	disp = sizeof(methodptr) * (m - m->class->methods);

	/* XXX TWISTI: check displacement */

	*((s4 *) (ra + 2 * 4)) |= (disp & 0x0000ffff);

	/* synchronize instruction cache */

	md_icacheflush(ra + 1 * 4, 2 * 4);

	return true;
}


/* patcher_checkcast_interface *************************************************

   Machine code:

   <patched call position>
   81870000    lwz     r12,0(r7)
   800c0010    lwz     r0,16(r12)
   34000000    addic.  r0,r0,0
   41810008    bgt-    0x014135d8
   83c00003    lwz     r30,3(0)
   800c0000    lwz     r0,0(r12)

*******************************************************************************/

bool patcher_checkcast_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;
	s4                 disp;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 5 * 4));
	cr = (constant_classref *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show NOPs, we have to skip them */

	if (opt_shownops)
		ra = ra + 1 * 4;

	/* patch super class index */

	disp = -(c->index);

	*((s4 *) (ra + 2 * 4)) |= (disp & 0x0000ffff);

	disp = OFFSET(vftbl_t, interfacetable[0]) - c->index * sizeof(methodptr*);

	*((s4 *) (ra + 5 * 4)) |= (disp & 0x0000ffff);

	/* synchronize instruction cache */

	md_icacheflush(ra + 2 * 4, 4 * 4);

	return true;
}


/* patcher_instanceof_interface ************************************************

   Machine code:

   <patched call position>
   81870000    lwz     r12,0(r7)
   800c0010    lwz     r0,16(r12)
   34000000    addic.  r0,r0,0
   41810008    bgt-    0x014135d8
   83c00003    lwz     r30,3(0)
   800c0000    lwz     r0,0(r12)

*******************************************************************************/

bool patcher_instanceof_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;
	s4                 disp;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 5 * 4));
	cr = (constant_classref *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show NOPs, we have to skip them */

	if (opt_shownops)
		ra = ra + 1 * 4;

	/* patch super class index */

	disp = -(c->index);

	*((s4 *) (ra + 2 * 4)) |= (disp & 0x0000ffff);

	disp = OFFSET(vftbl_t, interfacetable[0]) - c->index * sizeof(methodptr*);

	*((s4 *) (ra + 4 * 4)) |= (disp & 0x0000ffff);

	/* synchronize instruction cache */

	md_icacheflush(ra + 2 * 4, 3 * 4);

	return true;
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
