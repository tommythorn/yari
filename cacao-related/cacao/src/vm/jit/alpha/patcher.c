/* src/vm/jit/alpha/patcher.c - Alpha code patching functions

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
#include "vmcore/references.h"
#include "vm/resolve.h"


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

	xpc   = (u1 *)                *((ptrint *) (sp + 5 * 8));
	o     = (java_objectheader *) *((ptrint *) (sp + 4 * 8));
	f     = (functionptr)         *((ptrint *) (sp + 0 * 8));

	/* calculate and set the new return address */

	xpc = xpc - 1 * 4;

	*((ptrint *) (sp + 5 * 8)) = (ptrint) xpc;

	/* store PV into the patcher function position */

	*((ptrint *) (sp + 0 * 8)) = (ptrint) pv;

	/* cast the passed function to a patcher function */

	patcher_function = (bool (*)(u1 *)) (ptrint) f;

	/* enter a monitor on the patching position */

	PATCHER_MONITORENTER;

	/* create the stackframeinfo */

	stacktrace_create_extern_stackframeinfo(&sfi, pv, sp + 6 * 8, ra, xpc);

	/* call the proper patcher function */

	result = (patcher_function)(sp);

	/* remove the stackframeinfo */

	stacktrace_remove_stackframeinfo(&sfi);

	/* check for an error, get the exception and return it */

	if (result == false) {
		e = exceptions_get_and_clear_exception();

		PATCHER_MONITOREXIT;

		return e;
	}

	/* patch back original code */

	mcode = *((u4 *) (sp + 3 * 8));

	*((u4 *) xpc) = mcode;

	/* synchronize instruction cache */

	md_icacheflush(NULL, 0);

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

	c = (classinfo *) *((ptrint *) (sp + 2 * 8));

	/* check if the class is initialized */

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	return true;
}

/* patcher_resolve_class *****************************************************

   Initalizes a given classinfo pointer.  This function does not patch
   any data.

*******************************************************************************/

#ifdef ENABLE_VERIFIER
bool patcher_resolve_class(u1 *sp)
{
	unresolved_class *uc;

	/* get stuff from the stack */

	uc = (unresolved_class *)  *((ptrint *) (sp + 2 * 8));

	/* resolve the class and check subtype constraints */

	if (!resolve_class_eager_no_access_check(uc))
		return false;

	return true;
}
#endif /* ENABLE_VERIFIER */


/* patcher_resolve_classref_to_classinfo ***************************************

   ACONST:

   <patched call postition>
   a61bff80    ldq     a0,-128(pv)

   MULTIANEWARRAY:

   <patched call position>
   a63bff80    ldq     a1,-128(pv)
   47de0412    mov     sp,a2
   a77bff78    ldq     pv,-136(pv)
   6b5b4000    jsr     (pv)

   ARRAYCHECKCAST:

   <patched call position>
   a63bfe60    ldq     a1,-416(pv)
   a77bfe58    ldq     pv,-424(pv)
   6b5b4000    jsr     (pv)

*******************************************************************************/

bool patcher_resolve_classref_to_classinfo(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp  =                       *((s4 *)     (sp + 1 * 8));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	return true;
}


/* patcher_resolve_classref_to_vftbl *******************************************

   CHECKCAST (class):
   INSTANCEOF (class):

   <patched call position>
   a7940000    ldq     at,0(a4)
   a7bbff28    ldq     gp,-216(pv)

*******************************************************************************/

bool patcher_resolve_classref_to_vftbl(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp  =                       *((s4 *)     (sp + 1 * 8));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch super class' vftbl */

	*((ptrint *) (pv + disp)) = (ptrint) c->vftbl;

	return true;
}


/* patcher_resolve_classref_to_flags *******************************************

   CHECKCAST/INSTANCEOF:

   <patched call position>

*******************************************************************************/

bool patcher_resolve_classref_to_flags(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp  =                       *((s4 *)     (sp + 1 * 8));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch class flags */

	*((s4 *) (pv + disp)) = (s4) c->flags;

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

	m     = (methodinfo *) *((ptrint *) (sp + 2 * 8));
	disp  =                *((s4 *)     (sp + 1 * 8));
	pv    = (u1 *)         *((ptrint *) (sp + 0 * 8));

	/* resolve native function */

	if (!(f = native_resolve_function(m)))
		return false;

	/* patch native function pointer */

	*((ptrint *) (pv + disp)) = (ptrint) f;

	return true;
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


/* patcher_get_putstatic *******************************************************

   Machine code:

   <patched call position>
   a73bff98    ldq     t11,-104(pv)
   a2590000    ldl     a2,0(t11)

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	unresolved_field *uf;
	s4                disp;
	u1               *pv;
	fieldinfo        *fi;

	/* get stuff from the stack */

	uf    = (unresolved_field *) *((ptrint *) (sp + 2 * 8));
	disp  =                      *((s4 *)     (sp + 1 * 8));
	pv    = (u1 *)               *((ptrint *) (sp + 0 * 8));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* check if the field's class is initialized */

	if (!(fi->class->state & CLASS_INITIALIZED))
		if (!initialize_class(fi->class))
			return false;

	/* patch the field value's address */

	*((ptrint *) (pv + disp)) = (ptrint) &(fi->value);

	return true;
}


/* patcher_get_putfield ********************************************************

   Machine code:

   <patched call position>
   a2af0020    ldl     a5,32(s6)

*******************************************************************************/

bool patcher_get_putfield(u1 *sp)
{
	u1               *ra;
	unresolved_field *uf;
	fieldinfo        *fi;

	ra    = (u1 *)               *((ptrint *) (sp + 5 * 8));
	uf    = (unresolved_field *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_shownops) {
		/* patch the field's offset into the instruction */

		*((u4 *) (ra + 4)) |= (s2) (fi->offset & 0x0000ffff);
	}
	else {
		/* otherwise store the patched instruction on the stack */

		*((u4 *) (sp + 3 * 8)) |= (s2) (fi->offset & 0x0000ffff);
	}

	return true;
}


/* patcher_invokestatic_special ************************************************

   Machine code:

   <patched call position>
   a77bffa8    ldq     pv,-88(pv)
   6b5b4000    jsr     (pv)

******************************************************************************/

bool patcher_invokestatic_special(u1 *sp)
{
	unresolved_method *um;
	s4                 disp;
	u1                *pv;
	methodinfo        *m;

	/* get stuff from the stack */

	um    = (unresolved_method *) *((ptrint *) (sp + 2 * 8));
	disp  =                       *((s4 *)     (sp + 1 * 8));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch stubroutine */

	*((ptrint *) (pv + disp)) = (ptrint) m->stubroutine;

	return true;
}


/* patcher_invokevirtual *******************************************************

   Machine code:

   <patched call position>
   a7900000    ldq     at,0(a0)
   a77c0100    ldq     pv,256(at)
   6b5b4000    jsr     (pv)

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	um    = (unresolved_method *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_shownops)
		ra = ra + 4;

	/* patch vftbl index */

	*((s4 *) (ra + 4)) |= (s4) ((OFFSET(vftbl_t, table[0]) +
								 sizeof(methodptr) * m->vftblindex) & 0x0000ffff);

	return true;
}


/* patcher_invokeinterface *****************************************************

   Machine code:

   <patched call position>
   a7900000    ldq     at,0(a0)
   a79cffa0    ldq     at,-96(at)
   a77c0018    ldq     pv,24(at)
   6b5b4000    jsr     (pv)

*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	um    = (unresolved_method *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_shownops)
		ra = ra + 4;

	/* patch interfacetable index */

	*((s4 *) (ra + 4)) |= (s4) ((OFFSET(vftbl_t, interfacetable[0]) -
								 sizeof(methodptr*) * m->class->index) & 0x0000ffff);

	/* patch method offset */

	*((s4 *) (ra + 4 + 4)) |=
		(s4) ((sizeof(methodptr) * (m - m->class->methods)) & 0x0000ffff);

	return true;
}


/* patcher_checkcast_interface *************************************************

   Machine code:

   <patched call position>
   a78e0000    ldq     at,0(s5)
   a3bc001c    ldl     gp,28(at)
   23bdfffd    lda     gp,-3(gp)
   efa0002e    ble     gp,0x00000200002bf6b0
   a7bcffe8    ldq     gp,-24(at)

*******************************************************************************/

bool patcher_checkcast_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_shownops)
		ra = ra + 4;

	/* patch super class index */

	*((s4 *) (ra + 2 * 4)) |= (s4) (-(c->index) & 0x0000ffff);

	*((s4 *) (ra + 5 * 4)) |= (s4) ((OFFSET(vftbl_t, interfacetable[0]) -
									 c->index * sizeof(methodptr*)) & 0x0000ffff);

	return true;
}


/* patcher_instanceof_interface ************************************************

   Machine code:

   <patched call position>
   a78e0000    ldq     at,0(s5)
   a3bc001c    ldl     gp,28(at)
   23bdfffd    lda     gp,-3(gp)
   efa0002e    ble     gp,0x00000200002bf6b0
   a7bcffe8    ldq     gp,-24(at)

*******************************************************************************/

bool patcher_instanceof_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_shownops)
		ra = ra + 4;

	/* patch super class index */

	*((s4 *) (ra + 2 * 4)) |= (s4) (-(c->index) & 0x0000ffff);

	*((s4 *) (ra + 4 * 4)) |= (s4) ((OFFSET(vftbl_t, interfacetable[0]) -
									 c->index * sizeof(methodptr*)) & 0x0000ffff);

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
