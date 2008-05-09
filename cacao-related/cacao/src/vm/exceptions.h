/* src/vm/exceptions.h - exception related functions prototypes

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

   $Id: exceptions.h 7773 2007-04-19 19:44:54Z twisti $

*/


#ifndef _EXCEPTIONS_H
#define _EXCEPTIONS_H

/* forward typedefs ***********************************************************/

#include "config.h"
#include "vm/types.h"

#include "vm/global.h"

#include "vmcore/references.h"
#include "vmcore/method.h"


/* hardware-exception defines **************************************************

   These defines define an internal number for the various hardware
   exceptions.

   ATTENTION: These values are also used as load-displacements on some
   architectures. Thus, these values must NOT be aligned to 4 or
   8-byte boundaries, since normal loads could have such offsets with
   a base of NULL which should result in a NullPointerException.

   NOTE: In signal_init() we have an assert checking that the offset
   of java_objectheader.data[0] is greater than the largest
   displacement defined below.  Otherwise normal array loads/stores
   could trigger an exception.

*******************************************************************************/

#define EXCEPTION_HARDWARE_NULLPOINTER              0
#define EXCEPTION_HARDWARE_ARITHMETIC               1
#define EXCEPTION_HARDWARE_ARRAYINDEXOUTOFBOUNDS    2
#define EXCEPTION_HARDWARE_CLASSCAST                3

#define EXCEPTION_HARDWARE_EXCEPTION                5

#define EXCEPTION_HARDWARE_PATCHER                  6


/* function prototypes ********************************************************/

/* load and link exceptions used in the system */
bool exceptions_init(void);

/* initialize new exceptions */

java_objectheader *new_exception_utfmessage(const char *classname,
											utf *message);

java_objectheader *new_exception_int(const char *classname, s4 i);


/* functions to generate compiler exceptions */

java_objectheader *exceptions_new_abstractmethoderror(void);
java_objectheader *exceptions_asm_new_abstractmethoderror(u1 *sp, u1 *ra);
java_objectheader *exceptions_new_arraystoreexception(void);

void exceptions_throw_abstractmethoderror(void);
void exceptions_throw_classcircularityerror(classinfo *c);
void exceptions_throw_classformaterror(classinfo *c, const char *message, ...);
void exceptions_throw_classnotfoundexception(utf *name);
void exceptions_throw_noclassdeffounderror(utf *name);
void exceptions_throw_noclassdeffounderror_wrong_name(classinfo *c, utf *name);
void exceptions_throw_linkageerror(const char *message, classinfo *c);
void exceptions_throw_nosuchfielderror(classinfo *c, utf *name);
void exceptions_throw_nosuchmethoderror(classinfo *c, utf *name, utf *desc);
void exceptions_throw_exceptionininitializererror(java_objectheader *cause);
void exceptions_throw_incompatibleclasschangeerror(classinfo *c,
												   const char *message);
void exceptions_throw_instantiationerror(classinfo *c);
void exceptions_throw_internalerror(const char *message, ...);
void exceptions_throw_outofmemoryerror(void);
void exceptions_throw_verifyerror(methodinfo *m, const char *message, ...);
void exceptions_throw_verifyerror_for_stack(methodinfo *m, int type);
void exceptions_throw_unsatisfiedlinkerror(utf *name);
void exceptions_throw_unsupportedclassversionerror(classinfo *c, u4 ma, u4 mi);

java_objectheader *exceptions_new_arithmeticexception(void);

java_objectheader *exceptions_new_arrayindexoutofboundsexception(s4 index);
void exceptions_throw_arrayindexoutofboundsexception(void);
void exceptions_throw_arraystoreexception(void);

java_objectheader *exceptions_new_classcastexception(java_objectheader *o);

void exceptions_throw_clonenotsupportedexception(void);
void exceptions_throw_illegalaccessexception(classinfo *c);
void exceptions_throw_illegalargumentexception(void);
void exceptions_throw_illegalmonitorstateexception(void);
void exceptions_throw_interruptedexception(void);
void exceptions_throw_instantiationexception(classinfo *c);
void exceptions_throw_invocationtargetexception(java_objectheader *cause);
void exceptions_throw_negativearraysizeexception(void);

java_objectheader *exceptions_new_nullpointerexception(void);
void exceptions_throw_nullpointerexception(void);
void exceptions_throw_stringindexoutofboundsexception(void);

java_objectheader *exceptions_fillinstacktrace(void);

void classnotfoundexception_to_noclassdeffounderror(void);

java_objectheader *exceptions_get_exception(void);
void               exceptions_set_exception(java_objectheader *o);
void               exceptions_clear_exception(void);
java_objectheader *exceptions_get_and_clear_exception(void);

java_objectheader *exceptions_new_hardware_exception(u1 *pv, u1 *sp, u1 *ra, u1 *xpc, s4 type, ptrint val);

void exceptions_print_exception(java_objectheader *xptr);
void exceptions_print_current_exception(void);
void exceptions_print_stacktrace(void);

#endif /* _EXCEPTIONS_H */


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
