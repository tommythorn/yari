/* src/vm/jit/x86_64/patcher.c - x86_64 code patching functions

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

   Authors: Christian Thalinger

   Changes:

   $Id: patcher.c 7839 2007-04-29 22:46:56Z pm $

*/


#include "config.h"
#include "vm/types.h"

#include "vm/jit/s390/codegen.h"
#include "vm/jit/s390/md-abi.h"

#include "mm/memory.h"
#include "native/native.h"
#include "vm/builtin.h"
#include "vmcore/class.h"
#include "vm/exceptions.h"
#include "vmcore/field.h"
#include "vm/initialize.h"
#include "vmcore/options.h"
#include "vmcore/references.h"
#include "vm/resolve.h"
#include "vm/jit/patcher.h"
#include "vm/jit/stacktrace.h"

#include <assert.h>
#define OOPS() assert(0);
#define __PORTED__

/* patcher_wrapper *************************************************************

   Wrapper for all patchers.  It also creates the stackframe info
   structure.

   If the return value of the patcher function is false, it gets the
   exception object, clears the exception pointer and returns the
   exception.

*******************************************************************************/

java_objectheader *patcher_wrapper(u1 *sp, u1 *pv, u1 *ra)
{
#if 1
	stackframeinfo     sfi;
	u1                *xpc;
	java_objectheader *o;
	functionptr        f;
	bool               result;
	java_objectheader *e;

	/* define the patcher function */

	bool (*patcher_function)(u1 *);

	/* get stuff from the stack */

	xpc = (u1 *)                *((ptrint *) (sp + 5 * 4));
	o   = (java_objectheader *) *((ptrint *) (sp + 4 * 4));
	f   = (functionptr)         *((ptrint *) (sp + 0 * 4));
	
  	xpc = xpc - 4; /* the patch position is 4 bytes before the RA */

	*((ptrint *) (sp + 5 * 4)) = (ptrint) xpc;

	/* store PV into the patcher function position */

	*((ptrint *) (sp + 0 * 4)) = (ptrint) pv;

	/* cast the passed function to a patcher function */

	patcher_function = (bool (*)(u1 *)) (ptrint) f;

	/* enter a monitor on the patching position */

	PATCHER_MONITORENTER;

	/* create the stackframeinfo */

	/* RA is passed as NULL, but the XPC is correct and can be used in
	   stacktrace_create_extern_stackframeinfo for
	   md_codegen_get_pv_from_pc. */

	stacktrace_create_extern_stackframeinfo(&sfi, pv, sp + (6 * 4), ra, xpc);

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

	PATCHER_MARK_PATCHED_MONITOREXIT;

	return NULL;
#else

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

	PATCHER_MARK_PATCHED_MONITOREXIT;

	return NULL;
#endif
}


/* patcher_get_putstatic *******************************************************

   Machine code:

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	u1               *ra;
	u4                mcode;
	unresolved_field *uf;
	s4                disp;
	fieldinfo        *fi;
	u1               *pv;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 5 * 4));
	mcode =                      *((u4 *)     (sp + 3 * 4));
	uf    = (unresolved_field *) *((ptrint *) (sp + 2 * 4));
	disp  =                      *((s4 *)     (sp + 1 * 4));
	pv    = (u1 *)               *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* check if the field's class is initialized */

	if (!(fi->class->state & CLASS_INITIALIZED))
		if (!initialize_class(fi->class))
			return false;

	*((ptrint *) (pv + disp)) = (ptrint) &(fi->value);

	/* patch back original code */

	*((u4 *) ra) = mcode;

	return true;
}


/* patcher_get_putfield ********************************************************

   Machine code:

*******************************************************************************/

bool patcher_get_putfield(u1 *sp)
{
	u1               *ra;
	u4                mcode;
	unresolved_field *uf;
	fieldinfo        *fi;
	u1                byte;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 5 * 4));
	mcode =                      *((u4 *)     (sp + 3 * 4));
	uf    = (unresolved_field *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	/* If NOPs are generated, skip them */

	if (opt_shownops)
		ra += PATCHER_NOPS_SKIP;

	/* patch correct offset */

	if (fi->type == TYPE_LNG) {
		assert(N_VALID_DISP(fi->offset + 4));
		/* 2 RX operations, for 2 words; each already contains a 0 or 4 offset. */
		*((u4 *) ra ) |= (fi->offset + (*((u4 *) ra) & 0xF));
		ra += 4;
		*((u4 *) ra ) |= (fi->offset + (*((u4 *) ra) & 0xF));
	} else {
		assert(N_VALID_DISP(fi->offset));
		/* 1 RX operation */
		*((u4 *) ra) |= fi->offset;
	}

	return true;
}


/* patcher_putfieldconst *******************************************************

   Machine code:

   <patched call position>
   41 c7 85 00 00 00 00 7b 00 00 00    movl   $0x7b,0x0(%r13)

*******************************************************************************/

bool patcher_putfieldconst(u1 *sp)
{
	OOPS();
	u1               *ra;
	u8                mcode;
	unresolved_field *uf;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 5 * 8));
	mcode =                      *((u8 *)     (sp + 3 * 8));
	uf    = (unresolved_field *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* patch back original code */

	*((u8 *) ra) = mcode;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the field's offset */

	if (IS_2_WORD_TYPE(fi->type) || IS_ADR_TYPE(fi->type)) {
		/* handle special case when the base register is %r12 */

		if (*(ra + 2) == 0x84) {
			*((u4 *) (ra + 4))      = (u4) (fi->offset);
			*((u4 *) (ra + 12 + 4)) = (u4) (fi->offset + 4);
		}
		else {
			*((u4 *) (ra + 3))      = (u4) (fi->offset);
			*((u4 *) (ra + 11 + 3)) = (u4) (fi->offset + 4);
		}
	}
	else {
		/* handle special case when the base register is %r12 */

		if (*(ra + 2) == 0x84)
			*((u4 *) (ra + 4)) = (u4) (fi->offset);
		else
			*((u4 *) (ra + 3)) = (u4) (fi->offset);
	}

	return true;
}


/* patcher_aconst **************************************************************

   Machine code:

   <patched call position>
   48 bf a0 f0 92 00 00 00 00 00    mov    $0x92f0a0,%rdi

*******************************************************************************/

bool patcher_aconst(u1 *sp)
{
	OOPS();
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	mcode =                       *((u8 *)     (sp + 3 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u8 *) ra) = mcode;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the classinfo pointer */

	*((ptrint *) (ra + 2)) = (ptrint) c;

	return true;
}


/* patcher_builtin_multianewarray **********************************************

   Machine code:

   <patched call position>
   48 bf 02 00 00 00 00 00 00 00    mov    $0x2,%rdi
   48 be 30 40 b2 00 00 00 00 00    mov    $0xb24030,%rsi
   48 89 e2                         mov    %rsp,%rdx
   48 b8 7c 96 4b 00 00 00 00 00    mov    $0x4b967c,%rax
   48 ff d0                         callq  *%rax

*******************************************************************************/

bool patcher_builtin_multianewarray(u1 *sp)
{
	OOPS();
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	mcode =                       *((u8 *)     (sp + 3 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u8 *) ra) = mcode;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the classinfo pointer */

	*((ptrint *) (ra + 10 + 2)) = (ptrint) c;

	return true;
}


/* patcher_builtin_arraycheckcast **********************************************

   Machine code:

   <patched call position>
   48 be b8 3f b2 00 00 00 00 00    mov    $0xb23fb8,%rsi
   48 b8 00 00 00 00 00 00 00 00    mov    $0x0,%rax
   48 ff d0                         callq  *%rax

*******************************************************************************/

bool patcher_builtin_arraycheckcast(u1 *sp)
{
	OOPS();
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	mcode =                       *((u8 *)     (sp + 3 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u8 *) ra) = mcode;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the classinfo pointer */

	*((ptrint *) (ra + 2)) = (ptrint) c;

	return true;
}


/* patcher_invokestatic_special ************************************************

   Machine code:

*******************************************************************************/

__PORTED__ bool patcher_invokestatic_special(u1 *sp)
{
	u1                *ra;
	u4                 mcode;
	unresolved_method *um;
	s4                 disp;
	u1                *pv;
	methodinfo        *m;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	um    = (unresolved_method *) *((ptrint *) (sp + 2 * 4));
	disp  =                       *((s4 *)     (sp + 1 * 4));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	*((ptrint *) (pv + disp)) = (ptrint) m->stubroutine;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	/* patch stubroutine */

	return true;
}


/* patcher_invokevirtual *******************************************************

   Machine code:

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	u4                 mcode;
	unresolved_method *um;
	methodinfo        *m;
	s4                off;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	um    = (unresolved_method *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	/* If NOPs are generated, skip them */

	if (opt_shownops)
		ra += PATCHER_NOPS_SKIP;

	/* patch vftbl index */


	off = (s4) (OFFSET(vftbl_t, table[0]) +
								   sizeof(methodptr) * m->vftblindex);

	assert(N_VALID_DISP(off));

	*((s4 *)(ra + 4)) |= off;

	return true;
}


/* patcher_invokeinterface *****************************************************

   Machine code:

*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	u4                 mcode;
	unresolved_method *um;
	methodinfo        *m;
	s4                 idx, off;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	um    = (unresolved_method *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	/* If NOPs are generated, skip them */

	if (opt_shownops)
		ra += PATCHER_NOPS_SKIP;

	/* get interfacetable index */

	idx = (s4) (OFFSET(vftbl_t, interfacetable[0]) -
		sizeof(methodptr) * m->class->index) + 
		N_DISP_MAX;

	ASSERT_VALID_DISP(idx);

	/* get method offset */

	off =
		(s4) (sizeof(methodptr) * (m - m->class->methods));
	ASSERT_VALID_DISP(off);

	/* patch them */

	*((s4 *)(ra + 4)) |= idx;
	*((s4 *)(ra + 4 + 4)) |= off;

	return true;
}


/* patcher_resolve_classref_to_flags *******************************************

   CHECKCAST/INSTANCEOF:

   <patched call position>

*******************************************************************************/

__PORTED__ bool patcher_resolve_classref_to_flags(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;
	u4                 mcode;
	u1                *ra;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 4));
	disp  =                       *((s4 *)     (sp + 1 * 4));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch class flags */

	*((s4 *) (pv + disp)) = (s4) c->flags;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	return true;
}

/* patcher_resolve_classref_to_classinfo ***************************************

   ACONST:
   MULTIANEWARRAY:
   ARRAYCHECKCAST:

*******************************************************************************/

__PORTED__ bool patcher_resolve_classref_to_classinfo(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;
	u4                 mcode;
	u1                *ra;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 4));
	disp  =                       *((s4 *)     (sp + 1 * 4));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	return true;
}

/* patcher_resolve_classref_to_vftbl *******************************************

   CHECKCAST (class):
   INSTANCEOF (class):

*******************************************************************************/

bool patcher_resolve_classref_to_vftbl(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;
	u4                 mcode;
	u1                *ra;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 4));
	disp  =                       *((s4 *)     (sp + 1 * 4));
	pv    = (u1 *)                *((ptrint *) (sp + 0 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch super class' vftbl */

	*((ptrint *) (pv + disp)) = (ptrint) c->vftbl;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	return true;
}

/* patcher_checkcast_instanceof_interface **************************************

   Machine code:

*******************************************************************************/

bool patcher_checkcast_instanceof_interface(u1 *sp)
{
	u1                *ra;
	u4                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 4));
	mcode =                       *((u4 *)     (sp + 3 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	/* If NOPs are generated, skip them */

	if (opt_shownops)
		ra += PATCHER_NOPS_SKIP;

	/* patch super class index */

	/* From here, split your editor and open codegen.c */

	switch (*(ra + 1) >> 4) {
		case REG_ITMP1: 
			/* First M_ALD is into ITMP1 */
			/* INSTANCEOF code */

			*(u4 *)(ra + SZ_L + SZ_L) |= (u2)(s2)(- c->index);
			*(u4 *)(ra + SZ_L + SZ_L + SZ_AHI + SZ_BRC) |=
				(u2)(s2)(OFFSET(vftbl_t, interfacetable[0]) -
					c->index * sizeof(methodptr*));

			break;

		case REG_ITMP2:
			/* First M_ALD is into ITMP2 */
			/* CHECKCAST code */

			*(u4 *)(ra + SZ_L + SZ_L) |= (u2)(s2)(- c->index);
			*(u4 *)(ra + SZ_L + SZ_L + SZ_AHI + SZ_BRC + SZ_ILL) |=
				(u2)(s2)(OFFSET(vftbl_t, interfacetable[0]) -
					c->index * sizeof(methodptr*));

			break;

		default:
			assert(0);
			break;
	}

	return true;
}


/* patcher_checkcast_class *****************************************************

   Machine code:

   <patched call position>
   49 bb 00 00 00 00 00 00 00 00    mov    $0x0,%r11
   45 8b 92 20 00 00 00             mov    0x20(%r10),%r10d
   45 8b 9b 20 00 00 00             mov    0x20(%r11),%r11d
   4d 29 da                         sub    %r11,%r10
   49 bb 00 00 00 00 00 00 00 00    mov    $0x0,%r11

*******************************************************************************/

bool patcher_checkcast_class(u1 *sp)
{
	OOPS();
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	mcode =                       *((u8 *)     (sp + 3 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u8 *) ra) = mcode;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch super class' vftbl */

	*((ptrint *) (ra + 2)) = (ptrint) c->vftbl;
	*((ptrint *) (ra + 10 + 7 + 7 + 3 + 2)) = (ptrint) c->vftbl;

	return true;
}


/* patcher_instanceof_class ****************************************************

   Machine code:

   <patched call position>
   49 ba 00 00 00 00 00 00 00 00    mov    $0x0,%r10

*******************************************************************************/

bool patcher_instanceof_class(u1 *sp)
{
	OOPS();
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 5 * 8));
	mcode =                       *((u8 *)     (sp + 3 * 8));
	cr    = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u8 *) ra) = mcode;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch super class' vftbl */

	*((ptrint *) (ra + 2)) = (ptrint) c->vftbl;

	return true;
}


/* patcher_clinit **************************************************************

   May be used for GET/PUTSTATIC and in native stub.

   Machine code:

*******************************************************************************/

__PORTED__ bool patcher_clinit(u1 *sp)
{
	u1        *ra;
	u4         mcode;
	classinfo *c;

	/* get stuff from the stack */

	ra    = (u1 *)        *((ptrint *) (sp + 5 * 4));
	mcode =               *((u4 *)     (sp + 3 * 4));
	c     = (classinfo *) *((ptrint *) (sp + 2 * 4));

	/* check if the class is initialized */

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	return true;
}


/* patcher_athrow_areturn ******************************************************

   Machine code:

   <patched call position>

*******************************************************************************/

#ifdef ENABLE_VERIFIER
__PORTED__ bool patcher_athrow_areturn(u1 *sp)
{
	u1               *ra;
	u4                mcode;
	unresolved_class *uc;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 5 * 4));
	mcode =                      *((u4 *)     (sp + 3 * 4));
	uc    = (unresolved_class *) *((ptrint *) (sp + 2 * 4));

	/* resolve the class and check subtype constraints */

	if (!resolve_class_eager_no_access_check(uc))
		return false;

	/* patch back original code */

	*((u4 *) ra) = mcode;

	return true;
}
#endif /* ENABLE_VERIFIER */


/* patcher_resolve_native ******************************************************

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH)
__PORTED__ bool patcher_resolve_native(u1 *sp)
{
	u1          *ra;
	u4           mcode;
	methodinfo  *m;
	functionptr  f;
	s4           disp;
	u1          *pv;

	/* get stuff from the stack */

	ra    = (u1 *)         *((ptrint *) (sp + 5 * 4));
	mcode =                *((u4 *)     (sp + 3 * 4));
	disp  =                *((s4 *)     (sp + 1 * 4));
	m     = (methodinfo *) *((ptrint *) (sp + 2 * 4));
	pv    = (u1 *)         *((ptrint *) (sp + 0 * 4));

	/* resolve native function */

	if (!(f = native_resolve_function(m)))
		return false;

	/* patch native function pointer */

	*((ptrint *) (pv + disp)) = (ptrint) f;

	/* patch back original code */

	*((u4 *) ra) = mcode;

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
