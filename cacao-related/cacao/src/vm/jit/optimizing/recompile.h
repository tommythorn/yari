/* src/vm/jit/optimizing/recompile.h - recompilation system

   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, J. Wenninger, Institut f. Computersprachen - TU Wien

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

   $Id: cacao.c 4357 2006-01-22 23:33:38Z twisti $

*/


#ifndef _RECOMPILE_H
#define _RECOMPILE_H

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"


/* list_method_entry **********************************************************/

typedef struct list_method_entry list_method_entry;

struct list_method_entry {
	methodinfo *m;
	listnode_t  linkage;
};


/* function prototypes ********************************************************/

bool recompile_init(void);
bool recompile_start_thread(void);

void recompile_queue_method(methodinfo *m);

#endif /* _PROFILE_H */


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
