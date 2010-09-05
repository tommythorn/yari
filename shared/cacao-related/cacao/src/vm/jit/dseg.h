/* src/vm/jit/dseg.c - data segment handling stuff

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

   $Id: dseg.h 7596 2007-03-28 21:05:53Z twisti $

*/


#ifndef _DSEG_H
#define _DSEG_H

/* forward typedefs ***********************************************************/

typedef struct dsegentry             dsegentry;
typedef struct linenumbertable_entry linenumbertable_entry;
typedef struct dseg_exception_entry  dseg_exception_entry;


#include "config.h"
#include "vm/types.h"

#include "toolbox/list.h"

#include "vm/jit/jit.h"
#include "vm/jit/codegen-common.h"

#include "vmcore/references.h"


/* convenience macros *********************************************************/

#define dseg_add_functionptr(cd,value) \
    dseg_add_address((cd), (void *) (ptrint) (value))


/* dataentry ******************************************************************/

#define DSEG_FLAG_UNIQUE      0x0001
#define DSEG_FLAG_READONLY    0x0002

struct dsegentry {
	u2         type;
	u2         flags;
	s4         disp;
	imm_union  val;
	dsegentry *next;
};


/* linenumbertable_entry ******************************************************/

/* Keep the type of line the same as the pointer type, otherwise we
   run into alignment troubles (like on MIPS64). */

struct linenumbertable_entry {
	ptrint  line;               /* NOTE: see doc/inlining_stacktrace.txt for  */
	u1     *pc;                 /*       special meanings of line and pc.     */
};


/* dseg_exception_entry ********************************************************

   Datastructure which represents an exception entry in the exception
   table residing in the data segment.

*******************************************************************************/

struct dseg_exception_entry {
	classref_or_classinfo  catchtype;
	u1                    *handlerpc;
	u1                    *endpc;
	u1                    *startpc;
};


/* function prototypes ********************************************************/

void dseg_finish(jitdata *jd);

s4 dseg_add_unique_s4(codegendata *cd, s4 value);
s4 dseg_add_unique_s8(codegendata *cd, s8 value);
s4 dseg_add_unique_float(codegendata *cd, float value);
s4 dseg_add_unique_double(codegendata *cd, double value);
s4 dseg_add_unique_address(codegendata *cd, void *value);

s4 dseg_add_s4(codegendata *cd, s4 value);
s4 dseg_add_s8(codegendata *cd, s8 value);
s4 dseg_add_float(codegendata *cd, float value);
s4 dseg_add_double(codegendata *cd, double value);
s4 dseg_add_address(codegendata *cd, void *value);

void dseg_add_unique_target(codegendata *cd, basicblock *target);
void dseg_add_target(codegendata *cd, basicblock *target);

void dseg_addlinenumbertablesize(codegendata *cd);
void dseg_addlinenumber(codegendata *cd, u2 linenumber);
void dseg_addlinenumber_inline_start(codegendata *cd, instruction *iptr);
void dseg_addlinenumber_inline_end(codegendata *cd, instruction *iptr);

void dseg_createlinenumbertable(codegendata *cd);

s4 dseg_get_linenumber_from_pc(methodinfo **pm, u1 *pv, u1 *pc);

#if defined(__I386__) || defined(__X86_64__) || defined(__XDSPCORE__) || defined(__M68K___) || defined(ENABLE_INTRP)
void dseg_adddata(codegendata *cd);
void dseg_resolve_datareferences(jitdata *jd);
#endif

#if !defined(NDEBUG)
void dseg_display(jitdata *jd);
#endif

#endif /* _DSEG_H */


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
