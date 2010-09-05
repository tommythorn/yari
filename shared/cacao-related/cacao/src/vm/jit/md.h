/* src/vm/jit/md.h - machine dependent functions

   Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: jit.h 6265 2007-01-02 20:40:57Z edwin $

*/


#ifndef _MD_H
#define _MD_H

#include "config.h"
#include "vm/types.h"

#include "vm/jit/stacktrace.h"


/* function prototypes ********************************************************/

#if defined(ENABLE_JIT)
void md_init(void);

u1  *md_get_method_patch_address(u1 *ra, stackframeinfo *sfi, u1 *mptr);

void md_cacheflush(u1 *addr, s4 nbytes);
void md_icacheflush(u1 *addr, s4 nbytes);
void md_dcacheflush(u1 *addr, s4 nbytes);
#endif

#if defined(ENABLE_INTRP)
void intrp_md_init(void);
#endif

#endif /* _MD_H */


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
