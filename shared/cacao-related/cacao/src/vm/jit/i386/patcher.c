/* src/vm/jit/i386/patcher.c - i386 code patching functions

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
#include "vm/types.h"

#include "vm/jit/i386/codegen.h"

#include "mm/memory.h"

#include "native/native.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/initialize.h"

#include "vm/jit/patcher.h"
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
	functionptr        f;
	bool               result;
	java_objectheader *e;

	/* define the patcher function */

	bool (*patcher_function)(u1 *);

	/* get stuff from the stack */

	xpc = (u1 *)                *((ptrint *) (sp + 6 * 4));
	o   = (java_objectheader *) *((ptrint *) (sp + 4 * 4));
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


/* patcher_get_putstatic *******************************************************

   Machine code:

   <patched call position>
   b8 00 00 00 00             mov    $0x00000000,%eax

*******************************************************************************/

bool patcher_get_putstatic(u1 *sp)
{
	u1               *ra;
	u8                mcode;
	unresolved_field *uf;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 6 * 4));
	mcode =                      *((u8 *)     (sp + 2 * 4));
	uf    = (unresolved_field *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* check if the field's class is initialized */

	if (!(fi->class->state & CLASS_INITIALIZED))
		if (!initialize_class(fi->class))
			return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the field value's address */

	*((ptrint *) (ra + 1)) = (ptrint) &(fi->value);

	return true;
}


/* patcher_getfield ************************************************************

   Machine code:

   <patched call position>
   8b 88 00 00 00 00          mov    0x00000000(%eax),%ecx

*******************************************************************************/

bool patcher_getfield(u1 *sp)
{
	u1               *ra;
	u8                mcode;
	unresolved_field *uf;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 6 * 4));
	mcode =                      *((u8 *)     (sp + 2 * 4));
	uf    = (unresolved_field *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the field's offset */

	*((u4 *) (ra + 2)) = (u4) (fi->offset);

	/* if the field has type long, we need to patch the second move too */

	if (fi->type == TYPE_LNG)
		*((u4 *) (ra + 6 + 2)) = (u4) (fi->offset + 4);

	return true;
}


/* patcher_putfield ************************************************************

   Machine code:

   <patched call position>
   8b 88 00 00 00 00          mov    0x00000000(%eax),%ecx

*******************************************************************************/

bool patcher_putfield(u1 *sp)
{
	u1               *ra;
	u8                mcode;
	unresolved_field *uf;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 6 * 4));
	mcode =                      *((u8 *)     (sp + 2 * 4));
	uf    = (unresolved_field *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the field's offset */

	if (fi->type != TYPE_LNG) {
		*((u4 *) (ra + 2)) = (u4) (fi->offset);
	}
	else {
		/* The long code is special:
		 *
		 * 89 8d 00 00 00 00          mov    %ecx,0x00000000(%ebp)
		 * 89 95 00 00 00 00          mov    %edx,0x00000000(%ebp)
		 */

		*((u4 *) (ra + 2))     = (u4) (fi->offset);
		*((u4 *) (ra + 6 + 2)) = (u4) (fi->offset + 4);
	}

	return true;
}


/* patcher_putfieldconst *******************************************************

   Machine code:

   <patched call position>
   c7 85 00 00 00 00 7b 00 00 00    movl   $0x7b,0x0(%ebp)

*******************************************************************************/

bool patcher_putfieldconst(u1 *sp)
{
	u1               *ra;
	u8                mcode;
	unresolved_field *uf;
	fieldinfo        *fi;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 6 * 4));
	mcode =                      *((u8 *)     (sp + 2 * 4));
	uf    = (unresolved_field *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(fi = resolve_field_eager(uf)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the field's offset */

	if (!IS_2_WORD_TYPE(fi->type)) {
		*((u4 *) (ra + 2)) = (u4) (fi->offset);
	}
	else {
		/* long/double code is different:
		 *
		 * c7 80 00 00 00 00 c8 01 00 00    movl   $0x1c8,0x0(%eax)
		 * c7 80 04 00 00 00 00 00 00 00    movl   $0x0,0x4(%eax)
		 */

		*((u4 *) (ra + 2))      = (u4) (fi->offset);
		*((u4 *) (ra + 10 + 2)) = (u4) (fi->offset + 4);
	}

	return true;
}


/* patcher_aconst **************************************************************

   Machine code:

   <patched call postition>
   c7 04 24 00 00 00 00       movl   $0x0000000,(%esp)
   b8 00 00 00 00             mov    $0x0000000,%eax

*******************************************************************************/

bool patcher_aconst(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the classinfo pointer */

	*((ptrint *) (ra + 1)) = (ptrint) c;

	return true;
}


/* patcher_builtin_multianewarray **********************************************

   Machine code:

   <patched call position>
   c7 04 24 02 00 00 00       movl   $0x2,(%esp)
   c7 44 24 04 00 00 00 00    movl   $0x00000000,0x4(%esp)
   89 e0                      mov    %esp,%eax
   83 c0 0c                   add    $0xc,%eax
   89 44 24 08                mov    %eax,0x8(%esp)
   b8 00 00 00 00             mov    $0x00000000,%eax
   ff d0                      call   *%eax

*******************************************************************************/

bool patcher_builtin_multianewarray(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the classinfo pointer */

	*((ptrint *) (ra + 7 + 4)) = (ptrint) c;

	/* patch new function address */

	*((ptrint *) (ra + 7 + 8 + 2 + 3 + 4 + 1)) =
		(ptrint) BUILTIN_multianewarray;

	return true;
}


/* patcher_builtin_arraycheckcast **********************************************

   Machine code:

   <patched call position>
   c7 44 24 04 00 00 00 00    movl   $0x00000000,0x4(%esp)
   ba 00 00 00 00             mov    $0x00000000,%edx
   ff d2                      call   *%edx

*******************************************************************************/

bool patcher_builtin_arraycheckcast(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the classinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch the classinfo pointer */

	*((ptrint *) (ra + 4)) = (ptrint) c;

	/* patch new function address */

	*((ptrint *) (ra + 8 + 1)) = (ptrint) BUILTIN_arraycheckcast;

	return true;
}


/* patcher_invokestatic_special ************************************************

   Machine code:

   <patched call position>
   b9 00 00 00 00             mov    $0x00000000,%ecx
   ff d1                      call   *%ecx

*******************************************************************************/

bool patcher_invokestatic_special(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	um    = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch stubroutine */

	*((ptrint *) (ra + 1)) = (ptrint) m->stubroutine;

	return true;
}


/* patcher_invokevirtual *******************************************************

   Machine code:

   <patched call position>
   8b 08                      mov    (%eax),%ecx
   8b 81 00 00 00 00          mov    0x00000000(%ecx),%eax
   ff d0                      call   *%eax

*******************************************************************************/

bool patcher_invokevirtual(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	um    = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch vftbl index */

	*((s4 *) (ra + 2 + 2)) = (s4) (OFFSET(vftbl_t, table[0]) +
								   sizeof(methodptr) * m->vftblindex);

	return true;
}


/* patcher_invokeinterface *****************************************************

   Machine code:

   <patched call position>
   8b 00                      mov    (%eax),%eax
   8b 88 00 00 00 00          mov    0x00000000(%eax),%ecx
   8b 81 00 00 00 00          mov    0x00000000(%ecx),%eax
   ff d0                      call   *%eax

*******************************************************************************/

bool patcher_invokeinterface(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	unresolved_method *um;
	methodinfo        *m;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	um    = (unresolved_method *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(m = resolve_method_eager(um)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch interfacetable index */

	*((s4 *) (ra + 2 + 2)) = (s4) (OFFSET(vftbl_t, interfacetable[0]) -
								   sizeof(methodptr) * m->class->index);

	/* patch method offset */

	*((s4 *) (ra + 2 + 6 + 2)) =
		(s4) (sizeof(methodptr) * (m - m->class->methods));

	return true;
}


/* patcher_checkcast_instanceof_flags ******************************************

   Machine code:

   <patched call position>
   b9 00 00 00 00             mov    $0x00000000,%ecx

*******************************************************************************/

bool patcher_checkcast_instanceof_flags(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch class flags */

	*((s4 *) (ra + 1)) = (s4) c->flags;

	return true;
}


/* patcher_checkcast_interface *************************************************

   Machine code:

   <patched call position>
   8b 91 00 00 00 00          mov    0x00000000(%ecx),%edx
   81 ea 00 00 00 00          sub    $0x00000000,%edx
   85 d2                      test   %edx,%edx
   0f 8f 06 00 00 00          jg     0x00000000
   8b 35 03 00 00 00          mov    0x3,%esi
   8b 91 00 00 00 00          mov    0x00000000(%ecx),%edx

*******************************************************************************/

bool patcher_checkcast_interface(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch super class index */

	*((s4 *) (ra + 6 + 2)) = (s4) c->index;

	*((s4 *) (ra + 6 + 6 + 2 + 6 + 6 + 2)) =
		(s4) (OFFSET(vftbl_t, interfacetable[0]) -
			  c->index * sizeof(methodptr*));

	return true;
}


/* patcher_instanceof_interface ************************************************

   Machine code:

   <patched call position>
   8b 91 00 00 00 00          mov    0x00000000(%ecx),%edx
   81 ea 00 00 00 00          sub    $0x00000000,%edx
   85 d2                      test   %edx,%edx
   0f 8e 13 00 00 00          jle    0x00000000
   8b 91 00 00 00 00          mov    0x00000000(%ecx),%edx

*******************************************************************************/

bool patcher_instanceof_interface(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch super class index */

	*((s4 *) (ra + 6 + 2)) = (s4) c->index;

	*((s4 *) (ra + 6 + 6 + 2 + 6 + 2)) =
		(s4) (OFFSET(vftbl_t, interfacetable[0]) -
			  c->index * sizeof(methodptr*));

	return true;
}


/* patcher_checkcast_class *****************************************************

   Machine code:

   <patched call position>
   ba 00 00 00 00             mov    $0x00000000,%edx
   8b 89 00 00 00 00          mov    0x00000000(%ecx),%ecx
   8b 92 00 00 00 00          mov    0x00000000(%edx),%edx
   29 d1                      sub    %edx,%ecx
   ba 00 00 00 00             mov    $0x00000000,%edx

*******************************************************************************/

bool patcher_checkcast_class(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch super class' vftbl */

	*((ptrint *) (ra + 1)) = (ptrint) c->vftbl;
	*((ptrint *) (ra + 5 + 6 + 6 + 2 + 1)) = (ptrint) c->vftbl;

	return true;
}


/* patcher_instanceof_class ****************************************************

   Machine code:

   <patched call position>
   b9 00 00 00 00             mov    $0x0,%ecx
   8b 40 14                   mov    0x14(%eax),%eax
   8b 51 18                   mov    0x18(%ecx),%edx
   8b 49 14                   mov    0x14(%ecx),%ecx

*******************************************************************************/

bool patcher_instanceof_class(u1 *sp)
{
	u1                *ra;
	u8                 mcode;
	constant_classref *cr;
	classinfo         *c;

	/* get stuff from the stack */

	ra    = (u1 *)                *((ptrint *) (sp + 6 * 4));
	mcode =                       *((u8 *)     (sp + 2 * 4));
	cr    = (constant_classref *) *((ptrint *) (sp + 1 * 4));

	/* get the fieldinfo */

	if (!(c = resolve_classref_eager(cr)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch super class' vftbl */

	*((ptrint *) (ra + 1)) = (ptrint) c->vftbl;

	return true;
}


/* patcher_clinit **************************************************************

   Is used int PUT/GETSTATIC and native stub.

   Machine code:

   <patched call position>

*******************************************************************************/

bool patcher_clinit(u1 *sp)
{
	u1        *ra;
	u8         mcode;
	classinfo *c;

	/* get stuff from the stack */

	ra    = (u1 *)        *((ptrint *) (sp + 6 * 4));
	mcode =               *((u8 *)     (sp + 2 * 4));
	c     = (classinfo *) *((ptrint *) (sp + 1 * 4));

	/* check if the class is initialized */

	if (!(c->state & CLASS_INITIALIZED))
		if (!initialize_class(c))
			return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	return true;
}


/* patcher_athrow_areturn ******************************************************

   Machine code:

   <patched call position>

*******************************************************************************/

#ifdef ENABLE_VERIFIER
bool patcher_athrow_areturn(u1 *sp)
{
	u1               *ra;
	u8                mcode;
	unresolved_class *uc;

	/* get stuff from the stack */

	ra    = (u1 *)               *((ptrint *) (sp + 6 * 4));
	mcode =                      *((u8 *)     (sp + 2 * 4));
	uc    = (unresolved_class *) *((ptrint *) (sp + 1 * 4));

	/* resolve the class and check subtype constraints */

	if (!resolve_class_eager_no_access_check(uc))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	return true;
}
#endif /* ENABLE_VERIFIER */


/* patcher_resolve_native ******************************************************

   Is used in native stub.

   Machine code:

   <patched call position>
   c7 44 24 04 28 90 01 40    movl   $0x40019028,0x4(%esp)

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH)
bool patcher_resolve_native(u1 *sp)
{
	u1          *ra;
	u8           mcode;
	methodinfo  *m;
	functionptr  f;

	/* get stuff from the stack */

	ra    = (u1 *)         *((ptrint *) (sp + 6 * 4));
	mcode =                *((u8 *)     (sp + 2 * 4));
	m     = (methodinfo *) *((ptrint *) (sp + 1 * 4));

	/* resolve native function */

	if (!(f = native_resolve_function(m)))
		return false;

	/* patch back original code */

	*((u4 *) (ra + 0)) = (u4) mcode;
	*((u1 *) (ra + 4)) = (u1) (mcode >> 32);

	/* if we show disassembly, we have to skip the nop's */

	if (opt_shownops)
		ra = ra + 5;

	/* patch native function pointer */

	*((ptrint *) (ra + 4)) = (ptrint) f;

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
