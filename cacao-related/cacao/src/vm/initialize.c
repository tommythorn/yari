/* src/vm/initialize.c - static class initializer functions

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

   $Id: initialize.c 7813 2007-04-25 19:20:13Z twisti $

*/


#include "config.h"

#include <string.h>

#include "vm/types.h"

#include "threads/lock-common.h"

#include "vm/global.h"
#include "vm/initialize.h"
#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"
#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif


/* private functions **********************************************************/

static bool initialize_class_intern(classinfo *c);


/* initialize_class ************************************************************

   In Java, every class can have a static initialization
   function. This function has to be called BEFORE calling other
   methods or accessing static variables.

*******************************************************************************/

bool initialize_class(classinfo *c)
{
	bool r;

	if (!makeinitializations)
		return true;

	LOCK_MONITOR_ENTER(c);

	/* maybe the class is already initalized or the current thread, which can
	   pass the monitor, is currently initalizing this class */

	if (CLASS_IS_OR_ALMOST_INITIALIZED(c)) {
		LOCK_MONITOR_EXIT(c);

		return true;
	}

	/* if <clinit> throw an Error before, the class was marked with an
       error and we have to throw a NoClassDefFoundError */

	if (c->state & CLASS_ERROR) {
		exceptions_throw_noclassdeffounderror(c->name);

		LOCK_MONITOR_EXIT(c);

		/* ...but return true, this is ok (mauve test) */

		return true;
	}

	/* this initalizing run begins NOW */

	c->state |= CLASS_INITIALIZING;

	/* call the internal function */

	r = initialize_class_intern(c);

	/* if return value is not NULL everything was ok and the class is
	   initialized */

	if (r)
		c->state |= CLASS_INITIALIZED;

	/* this initalizing run is done */

	c->state &= ~CLASS_INITIALIZING;

	LOCK_MONITOR_EXIT(c);

	return r;
}


/* initialize_class_intern *****************************************************

   This function MUST NOT be called directly, because of thread
   <clinit> race conditions.

*******************************************************************************/

static bool initialize_class_intern(classinfo *c)
{
	methodinfo        *m;
	java_objectheader *xptr;

	/* maybe the class is not already linked */

	if (!(c->state & CLASS_LINKED))
		if (!link_class(c))
			return false;

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		count_class_inits++;
#endif

	/* initialize super class */

	if (c->super.cls) {
		if (!(c->super.cls->state & CLASS_INITIALIZED)) {
#if !defined(NDEBUG)
			if (initverbose)
				log_message_class_message_class("Initialize super class ",
												c->super.cls,
												" from ",
												c);
#endif

			if (!initialize_class(c->super.cls))
				return false;
		}
	}

	/* interfaces implemented need not to be initialized (VM Spec 2.17.4) */

	m = class_findmethod(c, utf_clinit, utf_void__void);

	if (!m) {
#if !defined(NDEBUG)
		if (initverbose)
			log_message_class("Class has no static class initializer: ", c);
#endif

		return true;
	}

	/* Sun's and IBM's JVM don't care about the static flag */
/*  	if (!(m->flags & ACC_STATIC)) { */
/*  		log_text("Class initializer is not static!"); */

#if !defined(NDEBUG)
	if (initverbose)
		log_message_class("Starting static class initializer for class: ", c);
#endif

	/* now call the initializer */

	(void) vm_call_method(m, NULL);

	/* we have an exception or error */

	xptr = exceptions_get_exception();

	if (xptr != NULL) {
		/* class is NOT initialized and is marked with error */

		c->state |= CLASS_ERROR;

		/* is this an exception, than wrap it */

		if (builtin_instanceof(xptr, class_java_lang_Exception)) {
			/* clear exception, because we are calling jit code again */

			exceptions_clear_exception();

			/* wrap the exception */

			exceptions_throw_exceptionininitializererror(xptr);
		}

		return false;
	}

#if !defined(NDEBUG)
	if (initverbose)
		log_message_class("Finished static class initializer for class: ", c);
#endif

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
 */
