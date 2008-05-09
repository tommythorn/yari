/* src/vm/jit/m68k/patcher.c - m68k patcher functions

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

   $Id: arch.h 5330 2006-09-05 18:43:12Z edwin $

*/

#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "native/native.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/initialize.h"
#include "vm/resolve.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/patcher.h"
#include "vm/jit/md.h"
#include "vm/jit/methodheader.h"
#include "vm/jit/stacktrace.h"

#include "vmcore/class.h"
#include "vmcore/field.h"
#include "vmcore/options.h"
#include "vmcore/references.h"

#include "codegen.h"

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
	functionptr        f;
	bool               result;
	java_objectheader *e;

	/* define the patcher function */

	bool (*patcher_function)(u1 *);

	/* get stuff from the stack */

	xpc = (u1 *)                *((ptrint *) (sp + 6 * 4));
	/* REG_ITMP3 				  sp + 5 * 4 */
	o   = (java_objectheader *) *((ptrint *) (sp + 4 * 4));
	/*mcode = 				    *((u4*)      (sp + 3 * 4));*/
	/*xmcode = 		   			*((u4*)      (sp + 2 * 4));*/
	/* unresolved file                        sp + 1 * 4 */
	f   = (functionptr)         *((ptrint *) (sp + 0 * 4));


	/* calculate and set the new return address */
	xpc = xpc - PATCHER_CALL_SIZE;
	*((ptrint *) (sp + 6 * 4)) = (ptrint) xpc;


	/* cast the passed function to a patcher function */
	patcher_function = (bool (*)(u1 *)) (ptrint) f;

	/* enter a monitor on the patching position */
	PATCHER_MONITORENTER;

	/* create the stackframeinfo */

	/* RA is passed as NULL, but the XPC is correct and can be used in
	   stacktrace_create_extern_stackframeinfo for
	   md_codegen_get_pv_from_pc. */

	/*
	fprintf(stderr, "EXT STACKFRAME: sfi=%x pv=%x, sp=%x, xpc=%x\n", &sfi, pv, sp+7*4, xpc);
	*/
	stacktrace_create_extern_stackframeinfo(&sfi, pv, sp + 7 * 4, xpc, xpc);

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
}

/*	Helper
 */
static void patcher_patch_back(u1 *sp)	
{
	u1* xpc    = (u1 *)      *((ptrint *) (sp + 6 * 4));
	u4 mcode  = *((u4*)      (sp + 3 * 4));
	u4 xmcode = *((u4*)      (sp + 2 * 4));

	*((u4*)(xpc)) 	= mcode;
	*((u4*)(xpc+4)) = xmcode;
	md_icacheflush(xpc, 8);
}

/* patcher_initialize_class ****************************************************

   Initalizes a given classinfo pointer.  This function does not patch
   any data.

*******************************************************************************/

bool patcher_initialize_class(u1 *sp)
{
	classinfo *c;
	u4		   xpc, mcode, xmcode;

	/* get stuff from the stack */
	c = (classinfo *) *((ptrint *) (sp + 1 * 4));

	/* check if the class is initialized */
	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	/* patch back original code */
	patcher_patch_back(sp);

	return true;
}

/* patcher_invokevirtual *******************************************************

   Machine code:
0x4029bc46:   61ff 0000 00ba    bsrl 0x4029bd02
0x4029bc4c:   246f 0000         moveal %sp@(0),%a2
0x4029bc50:   266a 0000         moveal %a2@(0),%a3
0x4029bc54:   246b 0000         moveal %a3@(0),%a2 	<-- patch this (0) offset
0x4029bc58:   4e92              jsr %a2@

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;
	s2                 disp;

	/* get stuff from the stack */
	ra = (u1 *)                *((ptrint *) (sp + 6 * 4));
	um = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */
	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* if we show NOPs, we have to skip them */
	if (opt_shownops) ra += PATCHER_CALL_SIZE;

	assert( *((u2*)(ra+8)) == 0x286b);

	/* patch vftbl index */
	disp = (OFFSET(vftbl_t, table[0]) + sizeof(methodptr) * m->vftblindex);
	*((s2 *) (ra + 10)) = disp;

	/* synchronize instruction cache */
	md_icacheflush(ra + 10, 2);

	return true;
}

/* patcher_invokestatic_special ************************************************

   Machine code:

   INVOKESPECIAL
0x402902bc:   61ff 0000 0076    bsrl 0x40290334
0x402902c2:   247c 0000 0000    moveal #0,%a2		<-- this #0
0x402902c8:   4e92              jsr %a2@

******************************************************************************/

bool patcher_invokestatic_special(u1 *sp)
{
	unresolved_method *um;
	s4                 disp;
	methodinfo        *m;

	/* get stuff from the stack */
	disp =                       *((s4 *)     (sp + 6 * 4));
	um   = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */
	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* patch stubroutine */
	if (opt_shownops) disp += PATCHER_CALL_SIZE;

	*((ptrint *) (disp+2)) = (ptrint) m->stubroutine;

	/* synchronize inst cache */

	md_icacheflush(disp+2, SIZEOF_VOID_P);

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
	classinfo        *c;
	s4				disp;

	/* get stuff from the stack */
	uc = (unresolved_class *) *((ptrint *) (sp + 1 * 4));
	disp =                    *((s4 *)     (sp + 6 * 4));

	/* resolve the class */
	if (!resolve_class(uc, resolveEager, false, &c))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	return true;
}
#endif /* ENABLE_VERIFIER */

/* patcher_resolve_classref_to_classinfo ***************************************
  ACONST:
  	0x4028f2ca:   2479 0000 0000    moveal 0x00000000,%a2
*******************************************************************************/
bool patcher_resolve_classref_to_classinfo(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	u1                *pv;
	classinfo         *c;

	/* get stuff from the stack */
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	disp =                       *((s4 *)     (sp + 6 * 4));

	/* get the classinfo */
	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* patch the classinfo pointer */
	if (opt_shownops) disp += PATCHER_CALL_SIZE;
	*((ptrint *) (disp+2)) = (ptrint) c;

	/* synchronize inst cache */
	md_icacheflush(disp+2, SIZEOF_VOID_P);

	return true;
}

/* patcher_get_putstatic *******************************************************

   Machine code:

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	u1               *ra;
	unresolved_field *uf;
	s4                disp;
	u1               *pv;
	fieldinfo        *fi;

	/* get stuff from the stack */
	uf    = (unresolved_field *)  *((ptrint *) (sp + 1 * 4));
	disp  =                       *((s4 *)     (sp + 6 * 4));

	/* get the fieldinfo */
	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* check if the field's class is initialized */
	if (!(fi->class->state & CLASS_INITIALIZED))
		if (!initialize_class(fi->class))
			return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* patch the field value's address */
	if (opt_shownops) disp += PATCHER_CALL_SIZE;
	assert(*((uint16_t*)(disp)) == 0x247c);
	*((ptrint *) (disp+2)) = (ptrint) &(fi->value);

	/* synchronize inst cache */
	md_icacheflush(disp+2, SIZEOF_VOID_P);

	return true;
}

/* patcher_get_putfield ********************************************************

   Machine code:

   <patched call position>

*******************************************************************************/

bool patcher_get_putfield(u1 *sp)
{
	u1               *ra;
	unresolved_field *uf;
	fieldinfo        *fi;
	s2                disp;

	ra = (u1 *)               *((ptrint *) (sp + 6 * 4));
	uf = (unresolved_field *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */
	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* if we show NOPs, we have to skip them */
	if (opt_shownops) ra += PATCHER_CALL_SIZE;

	/* patch the field's offset */
	if (IS_LNG_TYPE(fi->type)) {
		/* If the field has type long, we have to patch two
		   instructions.  But we have to check which instruction
		   is first.  We do that with the offset of the first
		   instruction. */
		assert(0);
		disp = *((u4 *) (ra + 1 * 4));

		if (disp == 4) {
			*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
			*((u4 *) (ra + 2 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
		}
		else {
			*((u4 *) (ra + 1 * 4)) |= (s2) ((fi->offset + 0) & 0x0000ffff);
			*((u4 *) (ra + 2 * 4)) |= (s2) ((fi->offset + 4) & 0x0000ffff);
		}
	} else	{
		/*
		 *	0x40adb3f6:     0x254d0000 	movel %a5,%a2@(0)
		 *	                      ^^^^                     ^
		 *	                      to be patched
		 */
		assert( (*((uint32_t*)ra) & 0xffff0000) == *((uint32_t*)ra) );
		assert( (fi->offset & 0x0000ffff) == fi->offset );
		*((uint32_t*)ra) |= fi->offset;

		/* synchronize instruction cache */
		md_icacheflush(ra, 1 * 4);
	}

	return true;
}
/* patcher_resolve_classref_to_flags *******************************************

   CHECKCAST/INSTANCEOF:


CHECKCAST:
0x4029b056:   61ff 0000 013e    bsrl 0x4029b196
0x4029b05c:   263c 0000 0000    movel #0,%d3		<-- patch this #0
0x4029b062:   0283 0000 0200    andil #512,%d3

INSTANCEOF:
0x402a4aa8:   61ff 0000 05c4    bsrl 0x402a506e
0x402a4aae:   283c 0000 0000    movel #0,%d4		<-- same here
0x402a4ab4:   0284 0000 0200    andil #512,%d4


*******************************************************************************/

bool patcher_resolve_classref_to_flags(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	classinfo         *c;

	/* get stuff from the stack */
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	disp =                       *((s4 *)     (sp + 6 * 4));

	/* get the fieldinfo */
	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* patch class flags */
	if (opt_shownops) disp += PATCHER_CALL_SIZE;
	assert( (*((u2*)(disp)) == 0x263c) || (*((u2*)(disp)) == 0x283c) );
	*((s4 *) (disp + 2)) = (s4) c->flags;

	/* synchronize insn cache */
	md_icacheflush(disp + 2, SIZEOF_VOID_P);

	return true;
}

/* patcher_resolve_classref_to_vftbl *******************************************

   CHECKCAST (class):
0x4029b094:   61ff 0000 00b4    bsrl 0x4029b14a
0x4029b09a:   287c 0000 0000    moveal #0,%a4		<-- patch this #0
0x4029b0a0:   2668 0000         moveal %a0@(0),%a3

   INSTANCEOF (class):
0x402a9300:   61ff 0000 0574    bsrl 0x402a9876
0x402a9306:   267c 0000 0000    moveal #0,%a3
0x402a930c:   246a 0000         moveal %a2@(0),%a2


*******************************************************************************/

bool patcher_resolve_classref_to_vftbl(u1 *sp)
{
	constant_classref *cr;
	s4                 disp;
	classinfo         *c;

	/* get stuff from the stack */
	cr   = (constant_classref *) *((ptrint *) (sp + 1 * 4));
	disp =                       *((s4 *)     (sp + 6 * 4));

	/* get the fieldinfo */
	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* patch super class' vftbl */
	if (opt_shownops) disp += PATCHER_CALL_SIZE;
	assert( (*((u2*)disp) == 0x287c) || (*((u2*)disp)== 0x267c) );

	*((s4 *) (disp+2)) = (s4) c->vftbl;

	/* synchronize insin cache */
	md_icacheflush(disp+2, SIZEOF_VOID_P);

	return true;
}

/* patcher_instanceof_interface ************************************************

   Machine code:

0x402a92da:   61ff 0000 05c0    bsrl 0x402a989c
0x402a92e0:   246a 0000         moveal %a2@(0),%a2
0x402a92e4:   282a 0010         movel %a2@(16),%d4
0x402a92e8:   d8bc 0000 0000    addl #0,%d4		<-- this const
0x402a92ee:   4a84              tstl %d4
0x402a92f0:   6e0a              bles 0x402a92fc
0x402a92f2:   246a 0000         moveal %a2@(0),%a2	<-- this offset

*******************************************************************************/

bool patcher_instanceof_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;
	s4                 disp;

	/* get stuff from the stack */

	ra = (u1 *)                *((ptrint *) (sp + 6 * 4));
	cr = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */
	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* if we show NOPs, we have to skip them */
	if (opt_shownops) ra += PATCHER_CALL_SIZE;
		
	/* patch super class index */
	disp = -(c->index);
	assert( *((u2*)(ra + 8)) == 0xd8bc );
	*((s4 *) (ra + 10 )) = disp;

	disp = OFFSET(vftbl_t, interfacetable[0]) - c->index * sizeof(methodptr*);

	assert( (s2)disp  == disp);
	assert ( *((s2*)(ra+18)) == 0x246a );

	*((s2 *) (ra + 20)) = disp;

	/* synchronize instruction cache */
	md_icacheflush(ra + 10, 12);

	return true;
}

/* patcher_checkcast_interface *************************************************

0x402a9400:   61ff 0000 03b6    bsrl 0x402a97b8
0x402a9406:   266a 0000         moveal %a2@(0),%a3
0x402a940a:   282b 0010         movel %a3@(16),%d4
0x402a940e:   d8bc 0000 0000    addl #0,%d4		<-- this 0
0x402a9414:   4a84              tstl %d4
0x402a9416:   6e02              bgts 0x402a941a
	      1234		tstb %d0
0x402a9418:   4afc              illegal
0x402a941a:   286b 0000         moveal %a3@(0),%a4	<-- and this 0 offset

*******************************************************************************/

bool patcher_checkcast_interface(u1 *sp)
{
	u1                *ra;
	constant_classref *cr;
	classinfo         *c;
	s4                 disp;

	/* get stuff from the stack */
	ra = (u1 *)                *((ptrint *) (sp + 6 * 4));
	cr = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */
	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* if we show NOPs, we have to skip them */
	if (opt_shownops) ra += PATCHER_CALL_SIZE;

	/* patch super class index */
	disp = -(c->index);
	assert ( *((u2 *)(ra + 8)) == 0xd8bc );
	*((s4 *) (ra + 10)) = disp;

	disp = OFFSET(vftbl_t, interfacetable[0]) - c->index * sizeof(methodptr*);
	assert( *((u2 *)(ra + 22)) == 0x286b );
	assert( (s2)disp == disp);
	*((s2 *) (ra + 24)) = disp;
	
	/* synchronize instruction cache */
	md_icacheflush(ra + 10, 16);

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
	functionptr  f;

	/* get stuff from the stack */
	m    = (methodinfo *) *((ptrint *) (sp + 1 * 4));
	disp =                *((s4 *)     (sp + 6 * 4));

	/* resolve native function */
	if (!(f = native_resolve_function(m)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* patch native function pointer */
	if (opt_shownops) disp += PATCHER_CALL_SIZE;
	*((ptrint *) (disp + 2)) = (ptrint) f;

	/* synchronize data cache */
	md_icacheflush(disp + 2, SIZEOF_VOID_P);

	return true;
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


/* patcher_invokeinterface *****************************************************

   Machine code:
0x40adb03e:     moveal %a2@(0),%a3		0x266a0000		<-- no patching
0x40adb042:     moveal %a3@(0),%a3		0x266b0000		<-- patch this 0000
0x40adb046:     moveal %a3@(0),%a4		0xxxxx0000		<-- patch this 0000
0x40adb04a:     jsr %a4@				0xxxxx			


*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	unresolved_method *um;
	methodinfo        *m;
	s4                 disp;

	/* get stuff from the stack */
	ra = (u1 *)                *((ptrint *) (sp + 6 * 4));
	um = (unresolved_method *) *((ptrint *) (sp + 1 * 4));


	/* get the fieldinfo */
	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */
	patcher_patch_back(sp);

	/* if we show NOPs, we have to skip them */
	if (opt_shownops) ra += PATCHER_CALL_SIZE;
	assert( *((uint32_t*)ra) == 0x246f0000 );

	/* patch interfacetable index (first #0) */
	disp = OFFSET(vftbl_t, interfacetable[0]) - sizeof(methodptr*) * m->class->index;
	/* XXX this disp is negative, check! 
	 * assert( (disp & 0x0000ffff) == disp);*/
	*((uint16_t *) (ra + 5 * 2)) = disp;

	/* patch method offset (second #0) */
	disp = sizeof(methodptr) * (m - m->class->methods);
	assert( (disp & 0x0000ffff) == disp);
	*((uint16_t *) (ra + 7 * 2)) = disp;

	/* synchronize instruction cache */
	md_icacheflush(ra + 5 * 2, 2 * 2);

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
