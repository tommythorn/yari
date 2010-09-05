/* src/vm/jit/patcher.h - code patching functions

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

   $Id: patcher.h 7813 2007-04-25 19:20:13Z twisti $

*/


#ifndef _PATCHER_H
#define _PATCHER_H

#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "threads/lock-common.h"

#include "vm/global.h"


/* patcher macros *************************************************************/

#define PATCHER_FLAG_PATCHED    (vftbl_t *) 0xdeadbeef


#if defined(ENABLE_THREADS)

#define PATCHER_MONITORENTER \
	/* enter a monitor on the patching position */       \
	                                                     \
	lock_monitor_enter(o);                               \
	                                                     \
	/* check if the position has already been patched */ \
	                                                     \
	if (o->vftbl != NULL) {                              \
        assert(o->vftbl == PATCHER_FLAG_PATCHED);        \
                                                         \
		lock_monitor_exit(o);                            \
	                                                     \
		return NULL;                                     \
	}                                                    \


#define PATCHER_MONITOREXIT \
	/* leave the monitor on the patching position */     \
	                                                     \
	lock_monitor_exit(o);


#define PATCHER_MARK_PATCHED_MONITOREXIT \
	/* mark position as patched */                       \
	                                                     \
	o->vftbl = PATCHER_FLAG_PATCHED;                     \
	                                                     \
	PATCHER_MONITOREXIT

#else

#define PATCHER_MONITORENTER                 /* nop */
#define PATCHER_MONITOREXIT                  /* nop */
#define PATCHER_MARK_PATCHED_MONITOREXIT     /* nop */

#endif /* defined(ENABLE_THREADS) */


/* function prototypes ********************************************************/

java_objectheader *patcher_wrapper(u1 *sp, u1 *pv, u1 *ra);
#define PATCHER_wrapper (functionptr) patcher_wrapper

bool patcher_resolve_class(u1 *sp);
#define PATCHER_resolve_class (functionptr) patcher_resolve_class

bool patcher_initialize_class(u1 *sp);
#define PATCHER_initialize_class (functionptr) patcher_initialize_class

bool patcher_resolve_classref_to_classinfo(u1 *sp);
#define PATCHER_resolve_classref_to_classinfo (functionptr) patcher_resolve_classref_to_classinfo

bool patcher_resolve_classref_to_vftbl(u1 *sp);
#define PATCHER_resolve_classref_to_vftbl (functionptr) patcher_resolve_classref_to_vftbl

bool patcher_resolve_classref_to_flags(u1 *sp);
#define PATCHER_resolve_classref_to_flags (functionptr) patcher_resolve_classref_to_flags

#if !defined(WITH_STATIC_CLASSPATH)
bool patcher_resolve_native_function(u1 *sp);
#define PATCHER_resolve_native_function (functionptr) patcher_resolve_native_function
#endif


bool patcher_get_putstatic(u1 *sp);
#define PATCHER_get_putstatic (functionptr) patcher_get_putstatic

#if defined(__I386__)

bool patcher_getfield(u1 *sp);
#define PATCHER_getfield (functionptr) patcher_getfield

bool patcher_putfield(u1 *sp);
#define PATCHER_putfield (functionptr) patcher_putfield

#else

bool patcher_get_putfield(u1 *sp);
#define PATCHER_get_putfield (functionptr) patcher_get_putfield

#endif /* defined(__I386__) */

#if defined(__I386__) || defined(__X86_64__)

bool patcher_putfieldconst(u1 *sp);
#define PATCHER_putfieldconst (functionptr) patcher_putfieldconst

#endif /* defined(__I386__) || defined(__X86_64__) */

bool patcher_aconst(u1 *sp);
#define PATCHER_aconst (functionptr) patcher_aconst

bool patcher_builtin_multianewarray(u1 *sp);
#define PATCHER_builtin_multianewarray (functionptr) patcher_builtin_multianewarray

bool patcher_builtin_arraycheckcast(u1 *sp);
#define PATCHER_builtin_arraycheckcast (functionptr) patcher_builtin_arraycheckcast

bool patcher_invokestatic_special(u1 *sp);
#define PATCHER_invokestatic_special (functionptr) patcher_invokestatic_special

bool patcher_invokevirtual(u1 *sp);
#define PATCHER_invokevirtual (functionptr) patcher_invokevirtual

bool patcher_invokeinterface(u1 *sp);
#define PATCHER_invokeinterface (functionptr) patcher_invokeinterface


/* only for interpreter */
bool patcher_checkcast_instanceof(u1 *sp);
#define PATCHER_checkcast_instanceof (functionptr) patcher_checkcast_instanceof


bool patcher_checkcast_instanceof_flags(u1 *sp);
#define PATCHER_checkcast_instanceof_flags (functionptr) patcher_checkcast_instanceof_flags

bool patcher_checkcast_instanceof_interface(u1 *sp);
#define PATCHER_checkcast_instanceof_interface (functionptr) patcher_checkcast_instanceof_interface

bool patcher_checkcast_interface(u1 *sp);
#define PATCHER_checkcast_interface (functionptr) patcher_checkcast_interface

bool patcher_instanceof_interface(u1 *sp);
#define PATCHER_instanceof_interface (functionptr) patcher_instanceof_interface

#if defined(__I386__) || defined(__X86_64__) || defined(__POWERPC__) || defined(__POWERPC64__)

bool patcher_checkcast_class(u1 *sp);
#define PATCHER_checkcast_class (functionptr) patcher_checkcast_class

bool patcher_instanceof_class(u1 *sp);
#define PATCHER_instanceof_class (functionptr) patcher_instanceof_class

#else /* defined(__I386__) || defined(__X86_64__) || defined(__POWERPC__) || defined(__POWERPC64__)*/

bool patcher_checkcast_instanceof_class(u1 *sp);
#define PATCHER_checkcast_instanceof_class (functionptr) patcher_checkcast_instanceof_class

#endif /* defined(__I386__) || defined(__X86_64__) || defined(__POWERPC__) || defined(__POWERPC64__)*/

bool patcher_clinit(u1 *sp);
#define PATCHER_clinit (functionptr) patcher_clinit

bool patcher_athrow_areturn(u1 *sp);
#define PATCHER_athrow_areturn (functionptr) patcher_athrow_areturn

#if !defined(WITH_STATIC_CLASSPATH)
bool patcher_resolve_native(u1 *sp);
#define PATCHER_resolve_native (functionptr) patcher_resolve_native
#endif


/* stuff for the interpreter **************************************************/

#if defined(ENABLE_INTRP)
bool intrp_patcher_get_putstatic(u1 *sp);
bool intrp_patcher_get_putstatic_clinit(u1 *sp);
bool intrp_patcher_get_putfield(u1 *sp);
bool intrp_patcher_aconst(u1 *sp);
bool intrp_patcher_builtin_multianewarray(u1 *sp);
bool intrp_patcher_builtin_arraycheckcast(u1 *sp);
bool intrp_patcher_invokestatic_special(u1 *sp);
bool intrp_patcher_invokevirtual(u1 *sp);
bool intrp_patcher_invokeinterface(u1 *sp);
bool intrp_patcher_checkcast_instanceof(u1 *sp);
bool intrp_patcher_resolve_native(u1 *sp);
#endif /* defined(ENABLE_INTRP) */

#endif /* _PATCHER_H */


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

