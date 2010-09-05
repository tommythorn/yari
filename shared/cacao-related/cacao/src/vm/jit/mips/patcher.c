/* src/vm/jit/mips/patcher.c - MIPS code patching functions

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

#include "vm/jit/mips/codegen.h"

#include "mm/memory.h"

#include "native/native.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/initialize.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/md.h"
#include "vm/jit/patcher.h"

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
	u4                 mcode[5];
	functionptr        f;
	bool               result;
	java_objectheader *e;

	/* define the patcher function */

	bool (*patcher_function)(u1 *);

	assert(pv != NULL);

	/* get stuff from the stack */

	xpc = (u1 *)                *((ptrint *) (sp + 7 * 8));
	o   = (java_objectheader *) *((ptrint *) (sp + 6 * 8));
	f   = (functionptr)         *((ptrint *) (sp + 0 * 8));

	/* store PV into the patcher function position */

	*((ptrint *) (sp + 0 * 8)) = (ptrint) pv;

	/* cast the passed function to a patcher function */

	patcher_function = (bool (*)(u1 *)) (ptrint) f;

	/* enter a monitor on the patching position */

	PATCHER_MONITORENTER;

	/* create the stackframeinfo */

	stacktrace_create_extern_stackframeinfo(&sfi, pv, sp + 8 * 8, ra, xpc);

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

	mcode[0] = *((u4 *) (sp + 3 * 8 + 0));
	mcode[1] = *((u4 *) (sp + 3 * 8 + 4));

	mcode[2] = *((u4 *) (sp + 4 * 8 + 0));
	mcode[3] = *((u4 *) (sp + 4 * 8 + 4));
	mcode[4] = *((u4 *) (sp + 5 * 8 + 0));

	*((u4 *) (xpc + 0 * 4)) = mcode[0];
	*((u4 *) (xpc + 1 * 4)) = mcode[1];

	*((u4 *) (xpc + 2 * 4)) = mcode[2];
	*((u4 *) (xpc + 3 * 4)) = mcode[3];
	*((u4 *) (xpc + 4 * 4)) = mcode[4];

	/* synchronize instruction cache */

	md_icacheflush(xpc, PATCHER_CALL_SIZE);

	PATCHER_MARK_PATCHED_MONITOREXIT;

	return NULL;
}


/* patcher_get_putstatic *******************************************************

   Machine code:

   <patched call position>
   dfc1ffb8    ld       at,-72(s8)
   fc250000    sd       a1,0(at)

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	unresolved_field *uf;
	s4                disp;
	u1               *pv;
	fieldinfo        *fi;

	/* get stuff from the stack */

	uf   = (unresolved_field *) *((ptrint *) (sp + 2 * 8));
	disp =                      *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)               *((ptrint *) (sp + 0 * 8));

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
   8ee90020    lw       a5,32(s7)

*******************************************************************************/

bool patcher_get_putfield(u1 *sp)
{
	u1               *ra;
	unresolved_field *uf;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra = (u1 *)               *((ptrint *) (sp + 7 * 8));
	uf = (unresolved_field *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops) {
		ra = ra + PATCHER_CALL_SIZE;

#if SIZEOF_VOID_P == 4
		if (IS_LNG_TYPE(fi->type)) {
# if WORDS_BIGENDIAN == 1
			/* ATTENTION: order of these instructions depend on M_LLD_INTERN */
			*((u4 *) (ra + 0 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
			*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
# else
			/* ATTENTION: order of these instructions depend on M_LLD_INTERN */
			*((u4 *) (ra + 0 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
			*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
# endif
		}
		else
#endif
			*((u4 *) (ra + 0 * 4)) |= (s2) (fi->offset & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra, 2 * 4);
	}
	else {
#if SIZEOF_VOID_P == 4
		if (IS_LNG_TYPE(fi->type)) {
# if WORDS_BIGENDIAN == 1
			/* ATTENTION: order of these instructions depend on M_LLD_INTERN */
			*((u4 *) (sp + 3 * 8 + 0)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
			*((u4 *) (sp + 3 * 8 + 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
# else
			/* ATTENTION: order of these instructions depend on M_LLD_INTERN */
			*((u4 *) (sp + 3 * 8 + 0)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
			*((u4 *) (sp + 3 * 8 + 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
# endif
		}
		else
#endif
			*((u4 *) (sp + 3 * 8 + 0)) |= (s2) (fi->offset & 0x0000ffff);
	}

	return true;
}


/* patcher_aconst **************************************************************

   Machine code:

   <patched call postition>
   dfc4ff98    ld       a0,-104(s8)

*******************************************************************************/

bool patcher_aconst(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp =                       *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_builtin_multianewarray **********************************************

   Machine code:

   <patched call position>
   dfc5ff90    ld       a1,-112(s8)
   03a03025    move     a2,sp
   dfd9ff88    ld       t9,-120(s8)
   0320f809    jalr     t9
   00000000    nop

*******************************************************************************/

bool patcher_builtin_multianewarray(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp =                       *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_builtin_arraycheckcast **********************************************

   Machine code:

   <patched call position>
   dfc5ffc0    ld       a1,-64(s8)
   dfd9ffb8    ld       t9,-72(s8)
   0320f809    jalr     t9
   00000000    nop

*******************************************************************************/

bool patcher_builtin_arraycheckcast(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp =                       *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch the classinfo pointer */

	*((ptrint *) (pv + disp)) = (ptrint) c;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_invokestatic_special ************************************************

   Machine code:

   <patched call position>
   dfdeffc0    ld       s8,-64(s8)
   03c0f809    jalr     s8
   00000000    nop

******************************************************************************/

bool patcher_invokestatic_special(u1 *sp)
{
	unresolved_method *um;
	s4                 disp;
	u1                *pv;
	methodinfo        *m;

	/* get stuff from the stack */

	um   = (unresolved_method *) *((ptrint *) (sp + 2 * 8));
	disp =                       *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 8));

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
   dc990000    ld       t9,0(a0)
   df3e0040    ld       s8,64(t9)
   03c0f809    jalr     s8
   00000000    nop

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 7 * 8));
	um = (unresolved_method *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops) {
		ra = ra + PATCHER_CALL_SIZE;

		/* patch vftbl index */

		*((s4 *) (ra + 1 * 4)) |=
			(s4) ((OFFSET(vftbl_t, table[0]) +
				   sizeof(methodptr) * m->vftblindex) & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra + 1 * 4, 1 * 4);
	}
	else {
		/* patch vftbl index */

		*((s4 *) (sp + 3 * 8 + 4)) |=
			(s4) ((OFFSET(vftbl_t, table[0]) +
				   sizeof(methodptr) * m->vftblindex) & 0x0000ffff);
	}

	return true;
}


/* patcher_invokeinterface *****************************************************

   Machine code:

   <patched call position>
   dc990000    ld       t9,0(a0)
   df39ffa0    ld       t9,-96(t9)
   df3e0018    ld       s8,24(t9)
   03c0f809    jalr     s8
   00000000    nop

*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 7 * 8));
	um = (unresolved_method *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops) {
		ra = ra + PATCHER_CALL_SIZE;

		/* patch interfacetable index */

		*((s4 *) (ra + 1 * 4)) |=
			(s4) ((OFFSET(vftbl_t, interfacetable[0]) -
				   sizeof(methodptr*) * m->class->index) & 0x0000ffff);

		/* patch method offset */

		*((s4 *) (ra + 2 * 4)) |=
			(s4) ((sizeof(methodptr) * (m - m->class->methods)) & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra + 1 * 4, 2 * 4);
	}
	else {
		/* patch interfacetable index */

		*((s4 *) (sp + 3 * 8 + 4)) |=
			(s4) ((OFFSET(vftbl_t, interfacetable[0]) -
				   sizeof(methodptr*) * m->class->index) & 0x0000ffff);

		/* patch method offset */

		*((s4 *) (sp + 4 * 8 + 0)) |=
			(s4) ((sizeof(methodptr) * (m - m->class->methods)) & 0x0000ffff);
	}

	return true;
}


/* patcher_checkcast_instanceof_flags ******************************************

   Machine code:

   <patched call position>
   8fc3ff24    lw       v1,-220(s8)
   30630200    andi     v1,v1,512
   1060000d    beq      v1,zero,0x000000001051824c
   00000000    nop

*******************************************************************************/

bool patcher_checkcast_instanceof_flags(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp =                       *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch class flags */

	*((s4 *) (pv + disp)) = (s4) c->flags;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, sizeof(s4));

	return true;
}


/* patcher_checkcast_interface *************************************************

   Machine code:

   <patched call position>
   dd030000    ld       v1,0(a4)
   8c79001c    lw       t9,28(v1)
   27390000    addiu    t9,t9,0
   1b200082    blez     t9,zero,0x000000001051843c
   00000000    nop
   dc790000    ld       t9,0(v1)

*******************************************************************************/

bool patcher_checkcast_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 7 * 8));
	cr = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops) {
		ra = ra + PATCHER_CALL_SIZE;

		/* patch super class index */

		*((s4 *) (ra + 2 * 4)) |= (s4) (-(c->index) & 0x0000ffff);
		/* 	*((s4 *) (ra + 5 * 4)) |= (s4) ((OFFSET(vftbl_t, interfacetable[0]) - */
		/* 									 c->index * sizeof(methodptr*)) & 0x0000ffff); */
		*((s4 *) (ra + 6 * 4)) |=
			(s4) ((OFFSET(vftbl_t, interfacetable[0]) -
				   c->index * sizeof(methodptr*)) & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra + 2 * 4, 5 * 4);
	}
	else {
		/* patch super class index */

		*((s4 *) (sp + 2 * 4)) |= (s4) (-(c->index) & 0x0000ffff);
		/* 	*((s4 *) (ra + 5 * 4)) |= (s4) ((OFFSET(vftbl_t, interfacetable[0]) - */
		/* 									 c->index * sizeof(methodptr*)) & 0x0000ffff); */
		*((s4 *) (ra + 6 * 4)) |=
			(s4) ((OFFSET(vftbl_t, interfacetable[0]) -
				   c->index * sizeof(methodptr*)) & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra + 6 * 4, 1 * 4);
	}

	return true;
}


/* patcher_instanceof_interface ************************************************

   Machine code:

   <patched call position>
   dd030000    ld       v1,0(a4)
   8c79001c    lw       t9,28(v1)
   27390000    addiu    t9,t9,0
   1b200082    blez     t9,zero,0x000000001051843c
   00000000    nop
   dc790000    ld       t9,0(v1)

*******************************************************************************/

bool patcher_instanceof_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 7 * 8));
	cr = (constant_classref *) *((ptrint *) (sp + 2 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops) {
		ra = ra + PATCHER_CALL_SIZE;

		/* patch super class index */

		*((s4 *) (ra + 2 * 4)) |= (s4) (-(c->index) & 0x0000ffff);
		*((s4 *) (ra + 5 * 4)) |=
			(s4) ((OFFSET(vftbl_t, interfacetable[0]) -
				   c->index * sizeof(methodptr*)) & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra + 2 * 4, 4 * 4);
	}
	else {
		/* patch super class index */

		*((s4 *) (sp + 4 * 8 + 0)) |= (s4) (-(c->index) & 0x0000ffff);
		*((s4 *) (ra + 5 * 4)) |=
			(s4) ((OFFSET(vftbl_t, interfacetable[0]) -
				   c->index * sizeof(methodptr*)) & 0x0000ffff);

		/* synchronize instruction cache */

		md_icacheflush(ra + 5 * 4, 1 * 4);
	}

	return true;
}


/* patcher_checkcast_instanceof_class ******************************************

   Machine code:

   <patched call position>
   dd030000    ld       v1,0(a4)
   dfd9ff18    ld       t9,-232(s8)

*******************************************************************************/

bool patcher_checkcast_instanceof_class(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */

	cr   = (constant_classref *) *((ptrint *) (sp + 2 * 8));
	disp =                       *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)                *((ptrint *) (sp + 0 * 8));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch super class' vftbl */

	*((ptrint *) (pv + disp)) = (ptrint) c->vftbl;

	/* synchronize data cache */

	md_dcacheflush(pv + disp, SIZEOF_VOID_P);

	return true;
}


/* patcher_clinit **************************************************************

   No special machine code.

*******************************************************************************/

bool patcher_clinit(u1 *sp)
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


/* patcher_athrow_areturn ******************************************************

   Machine code:

   <patched call position>

*******************************************************************************/

#ifdef ENABLE_VERIFIER
bool patcher_athrow_areturn(u1 *sp)
{
	unresolved_class *uc;

	/* get stuff from the stack */

	uc = (unresolved_class *) *((ptrint *) (sp + 2 * 8));

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
	methodinfo  *m;
	s4           disp;
	u1          *pv;
	functionptr  f;

	/* get stuff from the stack */

	m    = (methodinfo *) *((ptrint *) (sp + 2 * 8));
	disp =                *((s4 *)     (sp + 1 * 8));
	pv   = (u1 *)         *((ptrint *) (sp + 0 * 8));

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
