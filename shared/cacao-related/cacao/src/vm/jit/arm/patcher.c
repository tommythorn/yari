/* src/vm/jit/arm/patcher.c - ARM code patching functions

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

   $Id: patcher.c 7464 2007-03-06 00:26:31Z edwin $

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
#include "vm/jit/md.h"
#include "vm/jit/patcher.h"

#include "vmcore/field.h"
#include "vmcore/options.h"
#include "vmcore/references.h"
#include "vm/resolve.h"


#define gen_resolveload(inst,offset) \
	assert((offset) >= -0x0fff && (offset) <= 0x0fff); \
	assert(!((inst) & 0x0fff)); \
	if ((offset) <  0) { \
		(inst) = ((inst) & 0xff7ff000) | ((-(offset)) & 0x0fff); \
		/*(inst) &= ~(1 << 23);*/ \
	} else { \
		(inst) = ((inst) & 0xfffff000) | ((offset) & 0x0fff); \
		/*(inst) |= (1 << 23);*/ \
	}


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

	xpc = (u1 *)                *((ptrint *) (sp + 4 * 4));
	o   = (java_objectheader *) *((ptrint *) (sp + 3 * 4));
	f   = (functionptr)         *((ptrint *) (sp + 0 * 4));

	/* calculate and set the new return address */

	xpc = xpc - 1 * 4;

	*((ptrint *) (sp + 4 * 4)) = (ptrint) xpc;

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

	/* check for an error, get the exception and return it */

	if (result == false) {
		e = exceptions_get_and_clear_exception();

		PATCHER_MONITOREXIT;

		return e;
	}

	/* patch back original code */

	mcode = *((u4 *) (sp + 2 * 4));

	*((u4 *) xpc) = mcode;

	/* synchronize instruction cache */

	md_icacheflush(xpc, 1 * 4);

	PATCHER_MARK_PATCHED_MONITOREXIT;

	return NULL;
}


/* patcher_get_putstatic *******************************************************

   Machine code:

   <patched call position>
   e51c103c    ldr   r1, [ip, #-60]

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	s4                disp;
	unresolved_field *uf;
	u1               *pv;
	fieldinfo        *fi;

	/* get stuff from the stack */

	disp =                      *((s4 *)     (sp + 5 * 4));
	uf   = (unresolved_field *) *((ptrint *) (sp + 1 * 4));
	pv   = (u1 *)               *((ptrint *) (sp + 0 * 4));

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
   e58a8000    str   r8, [sl, #__]

*******************************************************************************/

bool patcher_get_putfield(u1 *sp)
{
	u1                *ra;
	u4                 mcode;
	unresolved_field  *uf;
	u1                *pv;
	fieldinfo         *fi;

	/* get stuff from the stack */
	ra    = (u1*)                 *((ptrint *) (sp + 4 * 4));
	mcode =                       *((u4 *)     (sp + 2 * 4));
	uf    = (unresolved_field*)   *((ptrint *) (sp + 1 * 4));
	pv    = (u1*)                 *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_showdisassemble) {
		ra = ra + 1 * 4;

		/* patch the field's offset into the instruction */

		switch(fi->type) {
		case TYPE_ADR:
		case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
#endif
			assert(fi->offset <= 0x0fff);
			*((u4 *) (ra + 0 * 4)) |= (fi->offset & 0x0fff);
			break;

		case TYPE_LNG:
#if defined(ENABLE_SOFTFLOAT)
		case TYPE_DBL:
#endif
			assert((fi->offset + 4) <= 0x0fff);
			*((u4 *) (ra + 0 * 4)) |= ((fi->offset + 0) & 0x0fff);
			*((u4 *) (ra + 1 * 4)) |= ((fi->offset + 4) & 0x0fff);
			break;

#if !defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
		case TYPE_DBL:
			assert(fi->offset <= 0x03ff);
			*((u4 *) (ra + 0 * 4)) |= ((fi->offset >> 2) & 0x00ff);
			break;
#endif
		}
	}
	else {
		/* patch the field's offset into the instruction stored on the
		   stack and the next instruction in the code */

		switch(fi->type) {
		case TYPE_ADR:
		case TYPE_INT:
#if defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
#endif
			assert(fi->offset <= 0x0fff);
			*((u4 *) (sp + 2 * 4)) |= (fi->offset & 0x0fff);
			break;

		case TYPE_LNG:
#if defined(ENABLE_SOFTFLOAT)
		case TYPE_DBL:
#endif
			assert((fi->offset + 4) <= 0x0fff);
			*((u4 *) (sp + 2 * 4)) |= ((fi->offset + 0) & 0x0fff);
			*((u4 *) (ra + 1 * 4)) |= ((fi->offset + 4) & 0x0fff);
			break;

#if !defined(ENABLE_SOFTFLOAT)
		case TYPE_FLT:
		case TYPE_DBL:
			assert(fi->offset <= 0x03ff);
			*((u4 *) (sp + 2 * 4)) |= ((fi->offset >> 2) & 0x00ff);
			break;
#endif
		}
	}

	/* synchronize instruction cache */

	md_icacheflush(ra, 2 * 4);

	return true;
}


/* patcher_aconst **************************************************************

   Machine code:

   <patched call postition>
   e51cc030    ldr   r0, [ip, #-48]

*******************************************************************************/

bool patcher_aconst(u1 *sp)
{
	s4                 disp;
	constant_classref *cr;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	disp =                       *((s4 *)     (sp + 5 * 4));
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	return true;
}


/* patcher_builtin_multianewarray **********************************************

   Machine code:

   <patched call position>
   e3a00002    mov   r0, #2  ; 0x2
   e51c1064    ldr   r1, [ip, #-100]
   e1a0200d    mov   r2, sp
   e1a0e00f    mov   lr, pc
   e51cf068    ldr   pc, [ip, #-104]

*******************************************************************************/

bool patcher_builtin_multianewarray(u1 *sp)
{
	s4                 disp;
	constant_classref *cr;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	disp =                       *((s4 *)     (sp + 5 * 4));
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	return true;
}


/* patcher_builtin_arraycheckcast **********************************************

   Machine code:

   <patched call position>
   e51c1120    ldr   r1, [ip, #-288]
   e1a0e00f    mov   lr, pc
   e51cf124    ldr   pc, [ip, #-292]

*******************************************************************************/

bool patcher_builtin_arraycheckcast(u1 *sp)
{
	s4                 disp;
	constant_classref *cr;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	disp =                       *((s4 *)     (sp + 5 * 4));
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	return true;
}


/* patcher_invokestatic_special ************************************************

   Machine code:

   <patched call position>
   e51cc02c    ldr   ip, [ip, #-44]
   e1a0e00f    mov   lr, pc
   e1a0f00c    mov   pc, ip

******************************************************************************/

bool patcher_invokestatic_special(u1 *sp)
{
	s4                 disp;
	unresolved_method *um;
	u1                *pv;
	methodinfo        *m;

	/* get stuff from the stack */

	disp  =                       *((s4 *)     (sp + 5 * 4));
	um    = (unresolved_method*)  *((ptrint *) (sp + 1 * 4));
	pv    = (u1*)                 *((ptrint *) (sp + 0 * 4));

	/* get the methodinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch stubroutine */

	*((ptrint *) (pv + disp)) = (ptrint) m->stubroutine;

	return true;
}


/* patcher_invokevirtual *******************************************************

   Machine code:

   <patched call position>
   e590b000    ldr   fp, [r0]
   e59bc000    ldr   ip, [fp, #__]
   e1a0e00f    mov   lr, pc
   e1a0f00c    mov   pc, ip

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 4 * 4));
	um = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the methodinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_showdisassemble)
		ra = ra + 1 * 4;

	/* patch vftbl index */

	gen_resolveload(*((s4 *) (ra + 1 * 4)), (s4) (OFFSET(vftbl_t, table[0]) + sizeof(methodptr) * m->vftblindex));

	/* synchronize instruction cache */

	md_icacheflush(ra + 1 * 4, 1 * 4);

	return true;
}


/* patcher_invokeinterface *****************************************************

   Machine code:

   <patched call position>
   e590b000    ldr   fp, [r0]
   e59bb000    ldr   fp, [fp, #__]
   e59bc000    ldr   ip, [fp, #__]
   e1a0e00f    mov   lr, pc
   e1a0f00c    mov   pc, ip


*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 4 * 4));
	um = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the methodinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_showdisassemble)
		ra = ra + 1 * 4;

	/* patch interfacetable index */

	gen_resolveload(*((s4 *) (ra + 1 * 4)), (s4) (OFFSET(vftbl_t, interfacetable[0]) - sizeof(methodptr*) * m->class->index));

	/* patch method offset */

	gen_resolveload(*((s4 *) (ra + 2 * 4)), (s4) (sizeof(methodptr) * (m - m->class->methods)));

	/* synchronize instruction cache */

	md_icacheflush(ra + 1 * 4, 2 * 4);

	return true;
}


/* patcher_checkcast_instanceof_flags ******************************************

   Machine code:

   <patched call position>

*******************************************************************************/

bool patcher_checkcast_instanceof_flags(u1 *sp)
{
	s4                 disp;
	constant_classref *cr;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	disp  =                       *((s4 *)     (sp + 5 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch class flags */

	*((s4 *) (pv + disp)) = (s4) c->flags;

	return true;
}


/* patcher_checkcast_instanceof_interface **************************************

   Machine code:

   <patched call position>
   e59ab000    ldr   fp, [sl]
   e59b9010    ldr   r9, [fp, #16]
   e3590000    cmp   r9, #0  ; 0x0
   da000000    ble   0x000000
   e59b9000    ldr   r9, [fp, #__]
   e1190009    tst   r9, r9
   0a000000    beq   0x000000

*******************************************************************************/

bool patcher_checkcast_instanceof_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 4 * 4));
	cr = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show disassembly, we have to skip the nop */

	if (opt_showdisassemble)
		ra = ra + 4;

	/* patch super class index */

	assert(*((s4 *) (ra + 2 * 4)) == 0xe3590000);
	assert(c->index <= 0xff);

	*((s4 *) (ra + 2 * 4)) |= (s4) (c->index & 0x000000ff);

	/* patch super class vftbl index */

	gen_resolveload(*((s4 *) (ra + 4 * 4)), (s4) (OFFSET(vftbl_t, interfacetable[0]) - sizeof(methodptr*) * c->index));

	/* synchronize instruction cache */

	md_icacheflush(ra + 2 * 4, 3 * 4);

	return true;
}


/* patcher_checkcast_instanceof_class ******************************************

   Machine code:

   <patched call position>

*******************************************************************************/

bool patcher_checkcast_instanceof_class(u1 *sp)
{
	s4                 disp;
	constant_classref *cr;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	disp =                       *((s4 *)     (sp + 5 * 4));
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch super class' vftbl */

	*((ptrint *) (pv + disp)) = (ptrint) c->vftbl;

	return true;
}


/* patcher_clinit **************************************************************

   XXX

*******************************************************************************/

bool patcher_clinit(u1 *sp)
{
	classinfo *c;

	/* get stuff from the stack */

	c = (classinfo *) *((ptrint *) (sp + 1 * 4));

	/* check if the class is initialized */

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	return true;
}


/* patcher_athrow_areturn ******************************************************

   Machine code:

   <patched call position>

*******************************************************************************/

#ifdef ENABLE_VERIFIER
bool patcher_athrow_areturn(u1 *sp)
{
	unresolved_class *uc;

	/* get stuff from the stack */

	uc = (unresolved_class *) *((ptrint *) (sp + 1 * 4));

	/* resolve the class and check subtype constraints */

	if (!resolve_class_eager_no_access_check(uc))
		return false;

	return true;
}
#endif /* ENABLE_VERIFIER */


/* patcher_resolve_native ******************************************************

   XXX

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH)
bool patcher_resolve_native(u1 *sp)
{
	s4           disp;
	methodinfo  *m;
	u1          *pv;
	functionptr  f;

	/* get stuff from the stack */

	disp =                *((s4 *)     (sp + 5 * 4));
	m    = (methodinfo *) *((ptrint *) (sp + 1 * 4));
	pv   = (u1 *)         *((ptrint *) (sp + 0 * 4));

	/* resolve native function */

	if (!(f = native_resolve_function(m)))
		return false;

	/* patch native function pointer */

	*((ptrint *) (pv + disp)) = (ptrint) f;

	return true;
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


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
