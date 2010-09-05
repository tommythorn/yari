/* src/vmcore/method.h - method functions header

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

   $Id: method.h 7575 2007-03-25 20:30:50Z twisti $
*/


#ifndef _METHOD_H
#define _METHOD_H

/* forward typedefs ***********************************************************/

typedef struct methodinfo          methodinfo; 
typedef struct raw_exception_entry raw_exception_entry;
typedef struct lineinfo            lineinfo; 
typedef struct method_assumption   method_assumption;
typedef struct method_worklist     method_worklist;
typedef struct codeinfo            codeinfo;

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"

#include "vmcore/descriptor.h"
#include "vmcore/references.h"
#include "vmcore/linker.h"

#if defined(ENABLE_JAVASE)
# include "vmcore/stackmap.h"
#endif

#include "vmcore/utf8.h"


#if defined(ENABLE_REPLACEMENT)
/* Initial value for the hit countdown field of each method. */
#define METHOD_INITIAL_HIT_COUNTDOWN  1000
#endif


/* methodinfo *****************************************************************/

struct methodinfo {                 /* method structure                       */
	java_objectheader header;       /* we need this in jit's monitorenter     */
	s4            flags;            /* ACC flags                              */
	utf          *name;             /* name of method                         */
	utf          *descriptor;       /* JavaVM descriptor string of method     */
#if defined(ENABLE_JAVASE)
	utf          *signature;        /* Signature attribute                    */
	stack_map_t  *stack_map;        /* StackMapTable attribute                */
#endif

	methoddesc   *parseddesc;       /* parsed descriptor                      */
			     
	classinfo    *class;            /* class, the method belongs to           */
	s4            vftblindex;       /* index of method in virtual function    */
	                                /* table (if it is a virtual method)      */
	s4            maxstack;         /* maximum stack depth of method          */
	s4            maxlocals;        /* maximum number of local variables      */
	s4            jcodelength;      /* length of JavaVM code                  */
	u1           *jcode;            /* pointer to JavaVM code                 */

	s4            rawexceptiontablelength;  /* exceptiontable length          */
	raw_exception_entry *rawexceptiontable; /* the exceptiontable             */

	u2            thrownexceptionscount; /* number of exceptions attribute    */
	classref_or_classinfo *thrownexceptions; /* except. a method may throw    */

	u2            linenumbercount;  /* number of linenumber attributes        */
	lineinfo     *linenumbers;      /* array of lineinfo items                */

	u1           *stubroutine;      /* stub for compiling or calling natives  */
	codeinfo     *code;             /* current code of this method            */

#if defined(ENABLE_LSRA)
	s4            maxlifetimes;     /* helper for lsra                        */
#endif

	methodinfo   *overwrites;       /* method that is directly overwritten    */
	method_assumption *assumptions; /* list of assumptions about this method  */

#if defined(ENABLE_REPLACEMENT)
	s4            hitcountdown;     /* decreased for each hit                 */
#endif
};


/* method_assumption ***********************************************************

   This struct is used for registering assumptions about methods.

*******************************************************************************/

struct method_assumption {
	method_assumption *next;
	methodinfo        *context;
};


/* method_worklist *************************************************************

   List node used for method worklists.

*******************************************************************************/

struct method_worklist {
	method_worklist *next;
	methodinfo      *m;
};


/* raw_exception_entry ********************************************************/

/* exception table entry read by the loader */

struct raw_exception_entry {    /* exceptiontable entry in a method           */
	classref_or_classinfo catchtype; /* catchtype of exc. (0 == catchall)     */
	u2              startpc;    /* start pc of guarded area (inclusive)       */
	u2              endpc;      /* end pc of guarded area (exklusive)         */
	u2              handlerpc;  /* pc of exception handler                    */
};


/* lineinfo *******************************************************************/

struct lineinfo {
	u2 start_pc;
	u2 line_number;
};


/* function prototypes ********************************************************/

void method_free(methodinfo *m);
bool method_canoverwrite(methodinfo *m, methodinfo *old);

methodinfo *method_vftbl_lookup(vftbl_t *vftbl, methodinfo* m);

java_objectarray *method_get_parametertypearray(methodinfo *m);
java_objectarray *method_get_exceptionarray(methodinfo *m);

void method_add_assumption_monomorphic(methodinfo *m, methodinfo *caller);
void method_break_assumption_monomorphic(methodinfo *m, method_worklist **wl);

s4   method_count_implementations(methodinfo *m, classinfo *c, methodinfo **found);

#if !defined(NDEBUG)
void method_printflags(methodinfo *m);
void method_print(methodinfo *m);
void method_println(methodinfo *m);
void method_methodref_print(constant_FMIref *mr);
void method_methodref_println(constant_FMIref *mr);
#endif

#endif /* _METHOD_H */


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
