/* src/vm/jit_interface.h - prototypes of jit functions used in vm/ code

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

   $Id: jit_interface.h 7692 2007-04-12 14:47:24Z twisti $

*/


#ifndef _JIT_INTERFACE
#define _JIT_INTERFACE

#include "config.h"
#include "vm/types.h"


/* These functions are used from vm/ but defined in vm/jit/ */

void code_free_code_of_method(methodinfo *m);

methodinfo *code_get_methodinfo_for_pv(u1 *pv);

u1       *codegen_generate_stub_compiler(methodinfo *m);
codeinfo *codegen_generate_stub_native(methodinfo *m, functionptr f);

#if defined(ENABLE_INTRP)
u1 *intrp_createcompilerstub(methodinfo *m);
#endif

void removecompilerstub(u1 *stub);
void removenativestub(u1 *stub);

void jit_invalidate_code(methodinfo *m);

void md_param_alloc(methoddesc *md);

/* stub for throwing AbstractMethodError's */
#if defined(ENABLE_JIT)
void asm_abstractmethoderror(void);
#endif

#if defined(ENABLE_INTRP)
void intrp_asm_abstractmethoderror(void);
#endif

#endif /* _JIT_INTERFACE */


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
