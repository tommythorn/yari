/* src/threads/native/critical.h - restartable critical sections

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

   Authors: Stefan Ring
   			Edwin Steiner

   Changes: Christian Thalinger

   $Id: threads.h 4866 2006-05-01 21:40:38Z edwin $

*/


#ifndef _CRITICAL_H
#define _CRITICAL_H

#include "config.h"

#include <signal.h>   /* required on some older Darwin systems for ucontext.h */
#include <ucontext.h>


/* forward typedefs ***********************************************************/

typedef struct critical_section_node_t critical_section_node_t;


/* critical_section_node_t *****************************************************

   A node representing a restartable critical section.

   CAUTION: This order must not be changed, it is used in
            asm_criticalsections!

*******************************************************************************/

struct critical_section_node_t {
	u1 *start;
	u1 *end;
	u1 *restart;
};


/* functions ******************************************************************/

void  critical_init(void);
void  critical_section_register(critical_section_node_t *);
u1   *critical_find_restart_point(u1*);

/* this is a machine dependent function (see src/vm/jit/$(ARCH_DIR)/md.c) */
void  md_critical_section_restart(ucontext_t *_uc);

#endif /* _CRITICAL_H */


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
