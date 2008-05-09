/* src/vm/exceptions.c - exception related functions

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

   $Id: exceptions.c 7785 2007-04-21 10:55:30Z edwin $

*/


#include "config.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "vm/types.h"

#include "md-abi.h"

#include "mm/memory.h"

#include "native/jni.h"
#include "native/native.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Throwable.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#else
# include "threads/none/threads.h"
#endif

#include "toolbox/logging.h"
#include "toolbox/util.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/disass.h"
#include "vm/jit/jit.h"
#include "vm/jit/methodheader.h"
#include "vm/jit/stacktrace.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"
#include "vmcore/options.h"

#if defined(ENABLE_VMLOG)
#include <vmlog_cacao.h>
#endif


/* for raising exceptions from native methods *********************************/

#if !defined(ENABLE_THREADS)
java_objectheader *_no_threads_exceptionptr = NULL;
#endif


/* init_system_exceptions ******************************************************

   Load and link exceptions used in the system.

*******************************************************************************/

bool exceptions_init(void)
{
	/* java/lang/Throwable */

	if (!(class_java_lang_Throwable =
		  load_class_bootstrap(utf_java_lang_Throwable)) ||
		!link_class(class_java_lang_Throwable))
		return false;

	/* java/lang/Error */

	if (!(class_java_lang_Error = load_class_bootstrap(utf_java_lang_Error)) ||
		!link_class(class_java_lang_Error))
		return false;

#if defined(ENABLE_JAVASE)
	/* java/lang/LinkageError */

	if (!(class_java_lang_LinkageError =
		  load_class_bootstrap(utf_java_lang_LinkageError)) ||
		!link_class(class_java_lang_LinkageError))
		return false;
#endif

	/* java/lang/NoClassDefFoundError */

	if (!(class_java_lang_NoClassDefFoundError =
		  load_class_bootstrap(utf_java_lang_NoClassDefFoundError)) ||
		!link_class(class_java_lang_NoClassDefFoundError))
		return false;

	/* java/lang/OutOfMemoryError */

	if (!(class_java_lang_OutOfMemoryError =
		  load_class_bootstrap(utf_java_lang_OutOfMemoryError)) ||
		!link_class(class_java_lang_OutOfMemoryError))
		return false;

	/* java/lang/VirtualMachineError */

	if (!(class_java_lang_VirtualMachineError =
		  load_class_bootstrap(utf_java_lang_VirtualMachineError)) ||
		!link_class(class_java_lang_VirtualMachineError))
		return false;


	/* java/lang/Exception */

	if (!(class_java_lang_Exception =
		  load_class_bootstrap(utf_java_lang_Exception)) ||
		!link_class(class_java_lang_Exception))
		return false;

	/* java/lang/ClassCastException */

	if (!(class_java_lang_ClassCastException =
		  load_class_bootstrap(utf_java_lang_ClassCastException)) ||
		!link_class(class_java_lang_ClassCastException))
		return false;

	/* java/lang/ClassNotFoundException */

	if (!(class_java_lang_ClassNotFoundException =
		  load_class_bootstrap(utf_java_lang_ClassNotFoundException)) ||
		!link_class(class_java_lang_ClassNotFoundException))
		return false;

	/* java/lang/NullPointerException */

	if (!(class_java_lang_NullPointerException =
		  load_class_bootstrap(utf_java_lang_NullPointerException)) ||
		!link_class(class_java_lang_NullPointerException))
		return false;


#if defined(WITH_CLASSPATH_GNU)
	/* java/lang/VMThrowable */

	if (!(class_java_lang_VMThrowable =
		  load_class_bootstrap(utf_java_lang_VMThrowable)) ||
		!link_class(class_java_lang_VMThrowable))
		return false;
#endif

	return true;
}


/* exceptions_new_class ********************************************************

   Creates an exception object from the given class and initalizes it.

   IN:
      class....class pointer

*******************************************************************************/

static java_objectheader *exceptions_new_class(classinfo *c)
{
	java_objectheader *o;

	o = native_new_and_init(c);

	if (o == NULL)
		return *exceptionptr;

	return o;
}


/* exceptions_new_utf **********************************************************

   Creates an exception object with the given name and initalizes it.

   IN:
      classname....class name in UTF-8

*******************************************************************************/

static java_objectheader *exceptions_new_utf(utf *classname)
{
	classinfo         *c;
	java_objectheader *o;

	c = load_class_bootstrap(classname);

	if (c == NULL)
		return *exceptionptr;

	o = exceptions_new_class(c);

	return o;
}


/* exceptions_throw_class ******************************************************

   Creates an exception object from the given class, initalizes and
   throws it.

   IN:
      class....class pointer

*******************************************************************************/

static void exceptions_throw_class(classinfo *c)
{
	java_objectheader *o;

	o = exceptions_new_class(c);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_throw_utf ********************************************************

   Creates an exception object with the given name, initalizes and
   throws it.

   IN:
      classname....class name in UTF-8

*******************************************************************************/

static void exceptions_throw_utf(utf *classname)
{
	classinfo *c;

	c = load_class_bootstrap(classname);

	if (c == NULL)
		return;

	exceptions_throw_class(c);
}


/* exceptions_throw_utf_throwable **********************************************

   Creates an exception object with the given name and initalizes it
   with the given java/lang/Throwable exception.

   IN:
      classname....class name in UTF-8
	  cause........the given Throwable

*******************************************************************************/

static void exceptions_throw_utf_throwable(utf *classname,
										   java_objectheader *cause)
{
	java_objectheader *o;
	classinfo         *c;
   
	c = load_class_bootstrap(classname);

	if (c == NULL)
		return;

	o = native_new_and_init_throwable(c, cause);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_new_utf_javastring ***********************************************

   Creates an exception object with the given name and initalizes it
   with the given java/lang/String message.

   IN:
      classname....class name in UTF-8
	  message......the message as a java.lang.String

   RETURN VALUE:
      an exception pointer (in any case -- either it is the newly created
	  exception, or an exception thrown while trying to create it).

*******************************************************************************/

static java_objectheader *exceptions_new_utf_javastring(utf *classname,
														java_objectheader *message)
{
	java_objectheader *o;
	classinfo         *c;
   
	c = load_class_bootstrap(classname);

	if (c == NULL)
		return *exceptionptr;

	o = native_new_and_init_string(c, message);

	if (o == NULL)
		return *exceptionptr;

	return o;
}


/* exceptions_new_class_utf ****************************************************

   Creates an exception object of the given class and initalizes it.

   IN:
      c..........class pointer
      message....the message as UTF-8 string

*******************************************************************************/

static java_objectheader *exceptions_new_class_utf(classinfo *c, utf *message)
{
	java_objectheader *o;
	java_objectheader *s;

	s = javastring_new(message);

	if (s == NULL)
		return *exceptionptr;

	o = native_new_and_init_string(c, s);

	if (o == NULL)
		return *exceptionptr;

	return o;
}


/* exceptions_new_utf_utf ******************************************************

   Creates an exception object with the given name and initalizes it
   with the given utf message.

   IN:
      classname....class name in UTF-8
	  message......the message as an utf *

   RETURN VALUE:
      an exception pointer (in any case -- either it is the newly created
	  exception, or an exception thrown while trying to create it).

*******************************************************************************/

static java_objectheader *exceptions_new_utf_utf(utf *classname, utf *message)
{
	classinfo         *c;
	java_objectheader *o;

	c = load_class_bootstrap(classname);

	if (c == NULL)
		return *exceptionptr;

	o = exceptions_new_class_utf(c, message);

	return o;
}


/* new_exception_message *******************************************************

   Creates an exception object with the given name and initalizes it
   with the given char message.

   IN:
      classname....class name in UTF-8
	  message......message in UTF-8

   RETURN VALUE:
      an exception pointer (in any case -- either it is the newly created
	  exception, or an exception thrown while trying to create it).

*******************************************************************************/

static java_objectheader *new_exception_message(const char *classname,
												const char *message)
{
	java_objectheader *o;
	java_objectheader *s;

	s = javastring_new_from_utf_string(message);

	if (s == NULL)
		return *exceptionptr;

	o = exceptions_new_utf_javastring(classname, s);

	return o;
}


/* exceptions_throw_class_utf **************************************************

   Creates an exception object of the given class, initalizes and
   throws it with the given utf message.

   IN:
      c..........class pointer
	  message....the message as an UTF-8

*******************************************************************************/

static void exceptions_throw_class_utf(classinfo *c, utf *message)
{
	*exceptionptr = exceptions_new_class_utf(c, message);
}


/* exceptions_throw_utf_utf ****************************************************

   Creates an exception object with the given name, initalizes and
   throws it with the given utf message.

   IN:
      classname....class name in UTF-8
	  message......the message as an utf *

*******************************************************************************/

static void exceptions_throw_utf_utf(utf *classname, utf *message)
{
	*exceptionptr = exceptions_new_utf_utf(classname, message);
}


/* new_exception_int ***********************************************************

   Creates an exception object with the given name and initalizes it
   with the given int value.

   IN:
      classname....class name in UTF-8
	  i............the integer

   RETURN VALUE:
      an exception pointer (in any case -- either it is the newly created
	  exception, or an exception thrown while trying to create it).

*******************************************************************************/

java_objectheader *new_exception_int(const char *classname, s4 i)
{
	java_objectheader *o;
	classinfo         *c;
   
	if (!(c = load_class_bootstrap(utf_new_char(classname))))
		return *exceptionptr;

	o = native_new_and_init_int(c, i);

	if (!o)
		return *exceptionptr;

	return o;
}


/* exceptions_new_abstractmethoderror ****************************************

   Generates a java.lang.AbstractMethodError for the VM.

*******************************************************************************/

java_objectheader *exceptions_new_abstractmethoderror(void)
{
	java_objectheader *o;

	o = exceptions_new_utf(utf_java_lang_AbstractMethodError);

	return o;
}


/* exceptions_new_error ********************************************************

   Generates a java.lang.Error for the VM.

*******************************************************************************/

#if defined(ENABLE_JAVAME_CLDC1_1)
static java_objectheader *exceptions_new_error(utf *message)
{
	java_objectheader *o;

	o = exceptions_new_class_utf(class_java_lang_Error, message);

	return o;
}
#endif


/* exceptions_asm_new_abstractmethoderror **************************************

   Generates a java.lang.AbstractMethodError for
   asm_abstractmethoderror.

*******************************************************************************/

java_objectheader *exceptions_asm_new_abstractmethoderror(u1 *sp, u1 *ra)
{
	stackframeinfo     sfi;
	java_objectheader *e;

	/* create the stackframeinfo (XPC is equal to RA) */

	stacktrace_create_extern_stackframeinfo(&sfi, NULL, sp, ra, ra);

	/* create the exception */

#if defined(ENABLE_JAVASE)
	e = exceptions_new_abstractmethoderror();
#else
	e = exceptions_new_error(utf_java_lang_AbstractMethodError);
#endif

	/* remove the stackframeinfo */

	stacktrace_remove_stackframeinfo(&sfi);

	return e;
}


/* exceptions_new_arraystoreexception ******************************************

   Generates a java.lang.ArrayStoreException for the VM.

*******************************************************************************/

java_objectheader *exceptions_new_arraystoreexception(void)
{
	java_objectheader *o;

	o = exceptions_new_utf(utf_java_lang_ArrayStoreException);

	return o;
}


/* exceptions_throw_abstractmethoderror ****************************************

   Generates and throws a java.lang.AbstractMethodError for the VM.

*******************************************************************************/

void exceptions_throw_abstractmethoderror(void)
{
	exceptions_throw_utf(utf_java_lang_AbstractMethodError);
}


/* exceptions_throw_classcircularityerror **************************************

   Generates and throws a java.lang.ClassCircularityError for the
   classloader.

   IN:
      c............the class in which the error was found

*******************************************************************************/

void exceptions_throw_classcircularityerror(classinfo *c)
{
	java_objectheader *o;
	char              *msg;
	s4                 msglen;

	/* calculate message length */

	msglen = utf_bytes(c->name) + strlen("0");

	/* allocate a buffer */

	msg = MNEW(char, msglen);

	/* print message into allocated buffer */

	utf_copy_classname(msg, c->name);

	o = new_exception_message(utf_java_lang_ClassCircularityError, msg);

	MFREE(msg, char, msglen);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_throw_classformaterror *******************************************

   Generates and throws a java.lang.ClassFormatError for the VM.

   IN:
      c............the class in which the error was found
	  message......UTF-8 format string

*******************************************************************************/

void exceptions_throw_classformaterror(classinfo *c, const char *message, ...)
{
	java_objectheader *o;
	char              *msg;
	s4                 msglen;
	va_list            ap;

	/* calculate message length */

	msglen = 0;

	if (c != NULL)
		msglen += utf_bytes(c->name) + strlen(" (");

	va_start(ap, message);
	msglen += get_variable_message_length(message, ap);
	va_end(ap);

	if (c != NULL)
		msglen += strlen(")");

	msglen += strlen("0");

	/* allocate a buffer */

	msg = MNEW(char, msglen);

	/* print message into allocated buffer */

	if (c != NULL) {
		utf_copy_classname(msg, c->name);
		strcat(msg, " (");
	}

	va_start(ap, message);
	vsprintf(msg + strlen(msg), message, ap);
	va_end(ap);

	if (c != NULL)
		strcat(msg, ")");

	o = new_exception_message(utf_java_lang_ClassFormatError, msg);

	MFREE(msg, char, msglen);

	*exceptionptr = o;
}


/* exceptions_throw_classnotfoundexception *************************************

   Generates and throws a java.lang.ClassNotFoundException for the
   VM.

   IN:
      name.........name of the class not found as a utf *

*******************************************************************************/

void exceptions_throw_classnotfoundexception(utf *name)
{
	/* we use class here, as this one is rather frequent */

	exceptions_throw_class_utf(class_java_lang_ClassNotFoundException, name);
}


/* exceptions_throw_noclassdeffounderror ***************************************

   Generates and throws a java.lang.NoClassDefFoundError.

   IN:
      name.........name of the class not found as a utf *

*******************************************************************************/

void exceptions_throw_noclassdeffounderror(utf *name)
{
	if (vm_initializing)
		vm_abort("java.lang.NoClassDefFoundError: %s", name->text);

	exceptions_throw_class_utf(class_java_lang_NoClassDefFoundError, name);
}


/* exceptions_throw_noclassdeffounderror_wrong_name ****************************

   Generates and throws a java.lang.NoClassDefFoundError with a
   specific message:



   IN:
      name.........name of the class not found as a utf *

*******************************************************************************/

void exceptions_throw_noclassdeffounderror_wrong_name(classinfo *c, utf *name)
{
	char *msg;
	s4    msglen;
	utf  *u;

	msglen = utf_bytes(c->name) + strlen(" (wrong name: ") +
		utf_bytes(name) + strlen(")") + strlen("0");

	msg = MNEW(char, msglen);

	utf_copy_classname(msg, c->name);
	strcat(msg, " (wrong name: ");
	utf_cat_classname(msg, name);
	strcat(msg, ")");

	u = utf_new_char(msg);

	MFREE(msg, char, msglen);

	exceptions_throw_noclassdeffounderror(u);
}


/* classnotfoundexception_to_noclassdeffounderror ******************************

   Check the *exceptionptr for a ClassNotFoundException. If it is one,
   convert it to a NoClassDefFoundError.

*******************************************************************************/

void classnotfoundexception_to_noclassdeffounderror(void)
{
	java_objectheader   *xptr;
	java_objectheader   *cause;
	java_lang_Throwable *t;
	java_lang_String    *s;

	/* get the cause */

	cause = *exceptionptr;

	/* convert ClassNotFoundException's to NoClassDefFoundError's */

	if (builtin_instanceof(cause, class_java_lang_ClassNotFoundException)) {
		/* clear exception, because we are calling jit code again */

		*exceptionptr = NULL;

		/* create new error */

		t = (java_lang_Throwable *) cause;
		s = t->detailMessage;

		xptr = exceptions_new_utf_javastring(utf_java_lang_NoClassDefFoundError, s);

		/* we had an exception while creating the error */

		if (*exceptionptr)
			return;

		/* set new exception */

		*exceptionptr = xptr;
	}
}


/* exceptions_throw_exceptionininitializererror ********************************

   Generates and throws a java.lang.ExceptionInInitializerError for
   the VM.

   IN:
      cause......cause exception object

*******************************************************************************/

void exceptions_throw_exceptionininitializererror(java_objectheader *cause)
{
	exceptions_throw_utf_throwable(utf_java_lang_ExceptionInInitializerError,
								   cause);
}


/* exceptions_throw_incompatibleclasschangeerror *******************************

   Generates and throws a java.lang.IncompatibleClassChangeError for
   the VM.

   IN:
      message......UTF-8 message format string

*******************************************************************************/

void exceptions_throw_incompatibleclasschangeerror(classinfo *c, const char *message)
{
	java_objectheader *o;
	char              *msg;
	s4                 msglen;

	/* calculate exception message length */

	msglen = utf_bytes(c->name) + strlen(message) + strlen("0");

	/* allocate memory */

	msg = MNEW(char, msglen);

	utf_copy_classname(msg, c->name);
	strcat(msg, message);

	o = native_new_and_init_string(utf_java_lang_IncompatibleClassChangeError,
								   javastring_new_from_utf_string(msg));

	/* free memory */

	MFREE(msg, char, msglen);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_throw_instantiationerror *****************************************

   Generates and throws a java.lang.InstantiationError for the VM.

*******************************************************************************/

void exceptions_throw_instantiationerror(classinfo *c)
{
	exceptions_throw_utf_utf(utf_java_lang_InstantiationError, c->name);
}


/* exceptions_throw_internalerror **********************************************

   Generates and throws a java.lang.InternalError for the VM.

   IN:
      message......UTF-8 message format string

*******************************************************************************/

void exceptions_throw_internalerror(const char *message, ...)
{
	java_objectheader *o;
	va_list            ap;
	char              *msg;
	s4                 msglen;

	/* calculate exception message length */

	va_start(ap, message);
	msglen = get_variable_message_length(message, ap);
	va_end(ap);

	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	va_start(ap, message);
	vsprintf(msg, message, ap);
	va_end(ap);

	/* create exception object */

	o = new_exception_message(utf_java_lang_InternalError, msg);

	/* free memory */

	MFREE(msg, char, msglen);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_throw_linkageerror ***********************************************

   Generates and throws java.lang.LinkageError with an error message.

   IN:
      message......UTF-8 message
	  c............class related to the error. If this is != NULL
	               the name of c is appended to the error message.

*******************************************************************************/

void exceptions_throw_linkageerror(const char *message, classinfo *c)
{
	java_objectheader *o;
	char              *msg;
	s4                 msglen;

	/* calculate exception message length */

	msglen = strlen(message) + 1;

	if (c != NULL)
		msglen += utf_bytes(c->name);
		
	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	strcpy(msg,message);

	if (c != NULL)
		utf_cat_classname(msg, c->name);

	o = native_new_and_init_string(class_java_lang_LinkageError,
								   javastring_new_from_utf_string(msg));

	/* free memory */

	MFREE(msg, char, msglen);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_throw_nosuchfielderror *******************************************

   Generates and throws a java.lang.NoSuchFieldError with an error
   message.

   IN:
      c............class in which the field was not found
	  name.........name of the field

*******************************************************************************/

void exceptions_throw_nosuchfielderror(classinfo *c, utf *name)
{
	char *msg;
	s4    msglen;
	utf  *u;

	/* calculate exception message length */

	msglen = utf_bytes(c->name) + strlen(".") + utf_bytes(name) + strlen("0");

	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	utf_copy_classname(msg, c->name);
	strcat(msg, ".");
	utf_cat(msg, name);

	u = utf_new_char(msg);

	/* free memory */

	MFREE(msg, char, msglen);

	exceptions_throw_utf_utf(utf_java_lang_NoSuchFieldError, u);
}


/* exceptions_throw_nosuchmethoderror ******************************************

   Generates and throws a java.lang.NoSuchMethodError with an error
   message.

   IN:
      c............class in which the method was not found
	  name.........name of the method
	  desc.........descriptor of the method

*******************************************************************************/

void exceptions_throw_nosuchmethoderror(classinfo *c, utf *name, utf *desc)
{
	char *msg;
	s4    msglen;
	utf  *u;

	/* calculate exception message length */

	msglen = utf_bytes(c->name) + strlen(".") + utf_bytes(name) +
		utf_bytes(desc) + strlen("0");

	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	utf_copy_classname(msg, c->name);
	strcat(msg, ".");
	utf_cat(msg, name);
	utf_cat(msg, desc);

	u = utf_new_char(msg);

	/* free memory */

	MFREE(msg, char, msglen);

#if defined(ENABLE_JAVASE)
	exceptions_throw_utf_utf(utf_java_lang_NoSuchMethodError, u);
#else
	exceptions_throw_class_utf(class_java_lang_Error, u);
#endif
}


/* exceptions_throw_outofmemoryerror *******************************************

   Generates and throws an java.lang.OutOfMemoryError for the VM.

*******************************************************************************/

void exceptions_throw_outofmemoryerror(void)
{
	exceptions_throw_class(class_java_lang_OutOfMemoryError);
}


/* exceptions_throw_unsatisfiedlinkerror ***************************************

   Generates and throws a java.lang.UnsatisfiedLinkError for the
   classloader.

   IN:
	  name......UTF-8 name string

*******************************************************************************/

void exceptions_throw_unsatisfiedlinkerror(utf *name)
{
#if defined(ENABLE_JAVASE)
	exceptions_throw_utf_utf(utf_java_lang_UnsatisfiedLinkError, name);
#else
	exceptions_throw_class_utf(class_java_lang_Error, name);
#endif
}


/* exceptions_throw_unsupportedclassversionerror *******************************

   Generates and throws a java.lang.UnsupportedClassVersionError for
   the classloader.

   IN:
      c............class in which the method was not found
	  message......UTF-8 format string

*******************************************************************************/

void exceptions_throw_unsupportedclassversionerror(classinfo *c, u4 ma, u4 mi)
{
	java_objectheader *o;
	char              *msg;
    s4                 msglen;

	/* calculate exception message length */

	msglen =
		utf_bytes(c->name) +
		strlen(" (Unsupported major.minor version 00.0)") +
		strlen("0");

	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	utf_copy_classname(msg, c->name);
	sprintf(msg + strlen(msg), " (Unsupported major.minor version %d.%d)",
			ma, mi);

	/* create exception object */

	o = new_exception_message(utf_java_lang_UnsupportedClassVersionError, msg);

	/* free memory */

	MFREE(msg, char, msglen);

	if (o == NULL)
		return;

	*exceptionptr = o;
}


/* exceptions_throw_verifyerror ************************************************

   Generates and throws a java.lang.VerifyError for the JIT compiler.

   IN:
      m............method in which the error was found
	  message......UTF-8 format string

*******************************************************************************/

void exceptions_throw_verifyerror(methodinfo *m, const char *message, ...)
{
	java_objectheader *o;
	va_list            ap;
	char              *msg;
	s4                 msglen;

	/* calculate exception message length */

	msglen = 0;

	if (m != NULL)
		msglen =
			strlen("(class: ") + utf_bytes(m->class->name) +
			strlen(", method: ") + utf_bytes(m->name) +
			strlen(" signature: ") + utf_bytes(m->descriptor) +
			strlen(") ") + strlen("0");

	va_start(ap, message);
	msglen += get_variable_message_length(message, ap);
	va_end(ap);

	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	if (m != NULL) {
		strcpy(msg, "(class: ");
		utf_cat_classname(msg, m->class->name);
		strcat(msg, ", method: ");
		utf_cat(msg, m->name);
		strcat(msg, " signature: ");
		utf_cat(msg, m->descriptor);
		strcat(msg, ") ");
	}

	va_start(ap, message);
	vsprintf(msg + strlen(msg), message, ap);
	va_end(ap);

	/* create exception object */

	o = new_exception_message(utf_java_lang_VerifyError, msg);

	/* free memory */

	MFREE(msg, char, msglen);

	*exceptionptr = o;
}


/* exceptions_throw_verifyerror_for_stack **************************************

   throws a java.lang.VerifyError for an invalid stack slot type

   IN:
      m............method in which the error was found
	  type.........the expected type

   RETURN VALUE:
      an exception pointer (in any case -- either it is the newly created
	  exception, or an exception thrown while trying to create it).

*******************************************************************************/

void exceptions_throw_verifyerror_for_stack(methodinfo *m,int type)
{
	java_objectheader *o;
	char              *msg;
	s4                 msglen;
	char              *typename;

	/* calculate exception message length */

	msglen = 0;

	if (m)
		msglen = strlen("(class: ") + utf_bytes(m->class->name) +
			strlen(", method: ") + utf_bytes(m->name) +
			strlen(" signature: ") + utf_bytes(m->descriptor) +
			strlen(") Expecting to find longest-------typename on stack") 
			+ strlen("0");

	/* allocate memory */

	msg = MNEW(char, msglen);

	/* generate message */

	if (m) {
		strcpy(msg, "(class: ");
		utf_cat_classname(msg, m->class->name);
		strcat(msg, ", method: ");
		utf_cat(msg, m->name);
		strcat(msg, " signature: ");
		utf_cat(msg, m->descriptor);
		strcat(msg, ") ");
	}
	else {
		msg[0] = 0;
	}

	strcat(msg,"Expecting to find ");
	switch (type) {
		case TYPE_INT: typename = "integer"; break;
		case TYPE_LNG: typename = "long"; break;
		case TYPE_FLT: typename = "float"; break;
		case TYPE_DBL: typename = "double"; break;
		case TYPE_ADR: typename = "object/array"; break;
		case TYPE_RET: typename = "returnAddress"; break;
		default:       typename = "<INVALID>"; assert(0); break;
	}
	strcat(msg, typename);
	strcat(msg, " on stack");

	/* create exception object */

	o = new_exception_message(utf_java_lang_VerifyError, msg);

	/* free memory */

	MFREE(msg, char, msglen);

	*exceptionptr = o;
}


/* exceptions_new_arithmeticexception ******************************************

   Generates a java.lang.ArithmeticException for the JIT compiler.

*******************************************************************************/

java_objectheader *exceptions_new_arithmeticexception(void)
{
	java_objectheader *o;

	o = new_exception_message(utf_java_lang_ArithmeticException, "/ by zero");

	if (o == NULL)
		return *exceptionptr;

	return o;
}


/* exceptions_new_arrayindexoutofboundsexception *******************************

   Generates a java.lang.ArrayIndexOutOfBoundsException for the VM
   system.

*******************************************************************************/

java_objectheader *exceptions_new_arrayindexoutofboundsexception(s4 index)
{
	java_objectheader *o;
	methodinfo        *m;
	java_objectheader *s;

	/* convert the index into a String, like Sun does */

	m = class_resolveclassmethod(class_java_lang_String,
								 utf_new_char("valueOf"),
								 utf_new_char("(I)Ljava/lang/String;"),
								 class_java_lang_Object,
								 true);

	if (m == NULL)
		return *exceptionptr;

	s = vm_call_method(m, NULL, index);

	if (s == NULL)
		return *exceptionptr;

	o = exceptions_new_utf_javastring(utf_java_lang_ArrayIndexOutOfBoundsException,
									  s);

	if (o == NULL)
		return *exceptionptr;

	return o;
}


/* exceptions_throw_arrayindexoutofboundsexception *****************************

   Generates and throws a java.lang.ArrayIndexOutOfBoundsException for
   the VM.

*******************************************************************************/

void exceptions_throw_arrayindexoutofboundsexception(void)
{
	exceptions_throw_utf(utf_java_lang_ArrayIndexOutOfBoundsException);
}


/* exceptions_throw_arraystoreexception ****************************************

   Generates and throws a java.lang.ArrayStoreException for the VM.

*******************************************************************************/

void exceptions_throw_arraystoreexception(void)
{
	exceptions_throw_utf(utf_java_lang_ArrayStoreException);
/*  	e = native_new_and_init(class_java_lang_ArrayStoreException); */
}


/* exceptions_new_classcastexception *******************************************

   Generates a java.lang.ClassCastException for the JIT compiler.

*******************************************************************************/

java_objectheader *exceptions_new_classcastexception(java_objectheader *o)
{
	java_objectheader *e;
	utf               *classname;
	java_lang_String  *s;

	classname = o->vftbl->class->name;

	s = javastring_new(classname);

	e = native_new_and_init_string(class_java_lang_ClassCastException, s);

	if (e == NULL)
		return *exceptionptr;

	return e;
}


/* exceptions_throw_clonenotsupportedexception *********************************

   Generates and throws a java.lang.CloneNotSupportedException for the
   VM.

*******************************************************************************/

void exceptions_throw_clonenotsupportedexception(void)
{
	exceptions_throw_utf(utf_java_lang_CloneNotSupportedException);
}


/* exceptions_throw_illegalaccessexception *************************************

   Generates and throws a java.lang.IllegalAccessException for the VM.

*******************************************************************************/

void exceptions_throw_illegalaccessexception(classinfo *c)
{
	/* XXX handle argument */

	exceptions_throw_utf(utf_java_lang_IllegalAccessException);
}


/* exceptions_throw_illegalargumentexception ***********************************

   Generates and throws a java.lang.IllegalArgumentException for the
   VM.

*******************************************************************************/

void exceptions_throw_illegalargumentexception(void)
{
	exceptions_throw_utf(utf_java_lang_IllegalArgumentException);
}


/* exceptions_throw_illegalmonitorstateexception *******************************

   Generates and throws a java.lang.IllegalMonitorStateException for
   the VM.

*******************************************************************************/

void exceptions_throw_illegalmonitorstateexception(void)
{
	exceptions_throw_utf(utf_java_lang_IllegalMonitorStateException);
}


/* exceptions_throw_instantiationexception *************************************

   Generates and throws a java.lang.InstantiationException for the VM.

*******************************************************************************/

void exceptions_throw_instantiationexception(classinfo *c)
{
	exceptions_throw_utf_utf(utf_java_lang_InstantiationException, c->name);
}


/* exceptions_throw_interruptedexception ***************************************

   Generates and throws a java.lang.InterruptedException for the VM.

*******************************************************************************/

void exceptions_throw_interruptedexception(void)
{
	exceptions_throw_utf(utf_java_lang_InterruptedException);
}


/* exceptions_throw_invocationtargetexception **********************************

   Generates and throws a java.lang.reflect.InvocationTargetException
   for the VM.

   IN:
      cause......cause exception object

*******************************************************************************/

void exceptions_throw_invocationtargetexception(java_objectheader *cause)
{
	exceptions_throw_utf_throwable(utf_java_lang_reflect_InvocationTargetException,
								   cause);
}


/* exceptions_throw_negativearraysizeexception *********************************

   Generates and throws a java.lang.NegativeArraySizeException for the
   VM.

*******************************************************************************/

void exceptions_throw_negativearraysizeexception(void)
{
	exceptions_throw_utf(utf_java_lang_NegativeArraySizeException);
}


/* exceptions_new_nullpointerexception *****************************************

   Generates a java.lang.NullPointerException for the VM system.

*******************************************************************************/

java_objectheader *exceptions_new_nullpointerexception(void)
{
	java_objectheader *o;

	o = exceptions_new_class(class_java_lang_NullPointerException);

	return o;
}


/* exceptions_throw_nullpointerexception ***************************************

   Generates a java.lang.NullPointerException for the VM system and
   throw it in the VM system.

*******************************************************************************/

void exceptions_throw_nullpointerexception(void)
{
	exceptions_throw_class(class_java_lang_NullPointerException);
}


/* exceptions_throw_stringindexoutofboundsexception ****************************

   Generates and throws a java.lang.StringIndexOutOfBoundsException
   for the VM.

*******************************************************************************/

void exceptions_throw_stringindexoutofboundsexception(void)
{
	exceptions_throw_utf(utf_java_lang_StringIndexOutOfBoundsException);
}


/* exceptions_get_exception ****************************************************

   Returns the current exception pointer of the current thread.

*******************************************************************************/

java_objectheader *exceptions_get_exception(void)
{
	/* return the exception */

	return *exceptionptr;
}


/* exceptions_set_exception ****************************************************

   Sets the exception pointer of the current thread.

*******************************************************************************/

void exceptions_set_exception(java_objectheader *o)
{
	/* set the exception */

	*exceptionptr = o;
}


/* exceptions_clear_exception **************************************************

   Clears the current exception pointer of the current thread.

*******************************************************************************/

void exceptions_clear_exception(void)
{
	/* and clear the exception */

	*exceptionptr = NULL;
}


/* exceptions_fillinstacktrace *************************************************

   Calls the fillInStackTrace-method of the currently thrown
   exception.

*******************************************************************************/

java_objectheader *exceptions_fillinstacktrace(void)
{
	java_objectheader *o;
	methodinfo        *m;

	/* get exception */

	o = *exceptionptr;
	assert(o);

	/* clear exception */

	*exceptionptr = NULL;

	/* resolve methodinfo pointer from exception object */

#if defined(ENABLE_JAVASE)
	m = class_resolvemethod(o->vftbl->class,
							utf_fillInStackTrace,
							utf_void__java_lang_Throwable);
#elif defined(ENABLE_JAVAME_CLDC1_1)
	m = class_resolvemethod(o->vftbl->class,
							utf_fillInStackTrace,
							utf_void__void);
#else
#error IMPLEMENT ME!
#endif

	/* call function */

	(void) vm_call_method(m, o);

	/* return exception object */

	return o;
}


/* exceptions_get_and_clear_exception ******************************************

   Gets the exception pointer of the current thread and clears it.
   This function may return NULL.

*******************************************************************************/

java_objectheader *exceptions_get_and_clear_exception(void)
{
	java_objectheader **p;
	java_objectheader  *e;

	/* get the pointer of the exception pointer */

	p = exceptionptr;

	/* get the exception */

	e = *p;

	/* and clear the exception */

	*p = NULL;

	/* return the exception */

	return e;
}


/* exceptions_new_hardware_exception *******************************************

   Creates the correct exception for a hardware-exception thrown and
   caught by a signal handler.

*******************************************************************************/

java_objectheader *exceptions_new_hardware_exception(u1 *pv, u1 *sp, u1 *ra, u1 *xpc, s4 type, ptrint val)
{
	stackframeinfo     sfi;
	java_objectheader *e;
	java_objectheader *o;
	s4                 index;

	/* create stackframeinfo */

	stacktrace_create_extern_stackframeinfo(&sfi, pv, sp, ra, xpc);

	switch (type) {
	case EXCEPTION_HARDWARE_NULLPOINTER:
		e = exceptions_new_nullpointerexception();
		break;

	case EXCEPTION_HARDWARE_ARITHMETIC:
		e = exceptions_new_arithmeticexception();
		break;

	case EXCEPTION_HARDWARE_ARRAYINDEXOUTOFBOUNDS:
		index = (s4) val;
		e = exceptions_new_arrayindexoutofboundsexception(index);
		break;

	case EXCEPTION_HARDWARE_CLASSCAST:
		o = (java_objectheader *) val;
		e = exceptions_new_classcastexception(o);
		break;

	case EXCEPTION_HARDWARE_EXCEPTION:
		e = exceptions_fillinstacktrace();
		break;

	default:
		/* let's try to get a backtrace */

		codegen_get_pv_from_pc(xpc);

		/* if that does not work, print more debug info */

		log_println("exceptions_new_hardware_exception: unknown exception type %d", type);

#if SIZEOF_VOID_P == 8
		log_println("PC=0x%016lx", xpc);
#else
		log_println("PC=0x%08x", xpc);
#endif

#if defined(ENABLE_DISASSEMBLER)
		log_println("machine instruction at PC:");
		disassinstr(xpc);
#endif

		vm_abort("Exiting...");
	}

	/* remove stackframeinfo */

	stacktrace_remove_stackframeinfo(&sfi);

	/* return the exception object */

	return e;
}


/* exceptions_handle_exception *************************************************

   Try to find an exception handler for the given exception and return it.
   If no handler is found, exit the monitor of the method (if any)
   and return NULL.

   IN:
      xptr.........the exception object
	  xpc..........PC of where the exception was thrown
	  pv...........Procedure Value of the current method
	  sp...........current stack pointer

   RETURN VALUE:
      the address of the first matching exception handler, or
	  NULL if no handler was found

*******************************************************************************/

#if defined(ENABLE_JIT)
u1 *exceptions_handle_exception(java_objectheader *xptr, u1 *xpc, u1 *pv, u1 *sp)
{
	methodinfo            *m;
	codeinfo              *code;
	s4                     issync;
	dseg_exception_entry  *ex;
	s4                     exceptiontablelength;
	s4                     i;
	classref_or_classinfo  cr;
	classinfo             *c;
#if defined(ENABLE_THREADS)
	java_objectheader     *o;
#endif

#ifdef __S390__
	/* Addresses are 31 bit integers */
#	define ADDR_MASK(x) (u1 *)((u4)(x) & 0x7FFFFFFF)
#else
#	define ADDR_MASK(x) (x)
#endif

	xpc = ADDR_MASK(xpc);

	/* get info from the method header */

	code                 = *((codeinfo **)            (pv + CodeinfoPointer));
	issync               = *((s4 *)                   (pv + IsSync));
	ex                   =   (dseg_exception_entry *) (pv + ExTableStart);
	exceptiontablelength = *((s4 *)                   (pv + ExTableSize));

	/* Get the methodinfo pointer from the codeinfo pointer. For
	   asm_vm_call_method the codeinfo pointer is NULL. */

	m = (code == NULL) ? NULL : code->m;

#if !defined(NDEBUG)
	/* print exception trace */

	if (opt_verbose || opt_verbosecall || opt_verboseexception)
		builtin_trace_exception(xptr, m, xpc, 1);
#endif

#if defined(ENABLE_VMLOG)
	vmlog_cacao_throw(xptr);
#endif

	for (i = 0; i < exceptiontablelength; i++) {
		/* ATTENTION: keep this here, as we need to decrement the
           pointer before the loop executes! */

		ex--;

		/* If the start and end PC is NULL, this means we have the
		   special case of asm_vm_call_method.  So, just return the
		   proper exception handler. */

		if ((ex->startpc == NULL) && (ex->endpc == NULL))
			return (u1 *) (ptrint) &asm_vm_call_method_exception_handler;

		/* is the xpc is the current catch range */

		if ((ADDR_MASK(ex->startpc) <= xpc) && (xpc < ADDR_MASK(ex->endpc))) {
			cr = ex->catchtype;

			/* NULL catches everything */

			if (cr.any == NULL) {
#if !defined(NDEBUG)
				/* Print stacktrace of exception when caught. */

#if defined(ENABLE_VMLOG)
				vmlog_cacao_catch(xptr);
#endif

				if (opt_verboseexception) {
					exceptions_print_exception(xptr);
					stacktrace_print_trace(xptr);
				}
#endif

				return ex->handlerpc;
			}

			/* resolve or load/link the exception class */

			if (IS_CLASSREF(cr)) {
				/* The exception class reference is unresolved. */
				/* We have to do _eager_ resolving here. While the class of */
				/* the exception object is guaranteed to be loaded, it may  */
				/* well have been loaded by a different loader than the     */
				/* defining loader of m's class, which is the one we must   */
				/* use to resolve the catch class. Thus lazy resolving      */
				/* might fail, even if the result of the resolution would   */
				/* be an already loaded class.                              */

				c = resolve_classref_eager(cr.ref);

				if (c == NULL) {
					/* Exception resolving the exception class, argh! */
					return NULL;
				}

				/* Ok, we resolved it. Enter it in the table, so we don't */
				/* have to do this again.                                 */
				/* XXX this write should be atomic. Is it?                */

				ex->catchtype.cls = c;
			} else {
				c = cr.cls;

				/* XXX I don't think this case can ever happen. -Edwin */
				if (!(c->state & CLASS_LOADED))
					/* use the methods' classloader */
					if (!load_class_from_classloader(c->name,
													 m->class->classloader))
						return NULL;

				/* XXX I think, if it is not linked, we can be sure that     */
				/* the exception object is no (indirect) instance of it, no? */
				/* -Edwin                                                    */
				if (!(c->state & CLASS_LINKED))
					if (!link_class(c))
						return NULL;
			}

			/* is the thrown exception an instance of the catch class? */

			if (builtin_instanceof(xptr, c)) {
#if !defined(NDEBUG)
				/* Print stacktrace of exception when caught. */

#if defined(ENABLE_VMLOG)
				vmlog_cacao_catch(xptr);
#endif

				if (opt_verboseexception) {
					exceptions_print_exception(xptr);
					stacktrace_print_trace(xptr);
				}
#endif

				return ex->handlerpc;
			}
		}
	}

#if defined(ENABLE_THREADS)
	/* is this method synchronized? */

	if (issync) {
		/* get synchronization object */

# if defined(__MIPS__) && (SIZEOF_VOID_P == 4)
		/* XXX change this if we ever want to use 4-byte stackslots */
		o = *((java_objectheader **) (sp + issync - 8));
# else
		o = *((java_objectheader **) (sp + issync - SIZEOF_VOID_P));
#endif

		assert(o != NULL);

		lock_monitor_exit(o);
	}
#endif

	/* none of the exceptions catch this one */

#if defined(ENABLE_VMLOG)
	vmlog_cacao_unwnd_method(m);
#endif

	return NULL;
}
#endif /* defined(ENABLE_JIT) */


/* exceptions_print_exception **************************************************

   Prints an exception, the detail message and the cause, if
   available, with CACAO internal functions to stdout.

*******************************************************************************/

void exceptions_print_exception(java_objectheader *xptr)
{
	java_lang_Throwable   *t;
#if defined(ENABLE_JAVASE)
	java_lang_Throwable   *cause;
#endif
	utf                   *u;

	t = (java_lang_Throwable *) xptr;

	if (t == NULL) {
		puts("NULL\n");
		return;
	}

#if defined(ENABLE_JAVASE)
	cause = t->cause;
#endif

	/* print the root exception */

	utf_display_printable_ascii_classname(t->header.vftbl->class->name);

	if (t->detailMessage != NULL) {
		u = javastring_toutf(t->detailMessage, false);

		printf(": ");
		utf_display_printable_ascii(u);
	}

	putc('\n', stdout);

#if defined(ENABLE_JAVASE)
	/* print the cause if available */

	if ((cause != NULL) && (cause != t)) {
		printf("Caused by: ");
		utf_display_printable_ascii_classname(cause->header.vftbl->class->name);

		if (cause->detailMessage) {
			u = javastring_toutf(cause->detailMessage, false);

			printf(": ");
			utf_display_printable_ascii(u);
		}

		putc('\n', stdout);
	}
#endif
}


/* exceptions_print_current_exception ******************************************

   Prints the current pending exception, the detail message and the
   cause, if available, with CACAO internal functions to stdout.

*******************************************************************************/

void exceptions_print_current_exception(void)
{
	java_objectheader *xptr;

	xptr = *exceptionptr;

	exceptions_print_exception(xptr);
}


/* exceptions_print_stacktrace *************************************************

   Prints a pending exception with Throwable.printStackTrace().  If
   there happens an exception during printStackTrace(), we print the
   thrown exception and the original one.

   NOTE: This function calls Java code.

*******************************************************************************/

void exceptions_print_stacktrace(void)
{
	java_objectheader *oxptr;
	java_objectheader *xptr;
	classinfo         *c;
	methodinfo        *m;

	/* get original exception */

	oxptr = *exceptionptr;

	if (oxptr == NULL)
		vm_abort("exceptions_print_stacktrace: no exception thrown");

	/* clear exception, because we are calling jit code again */

	*exceptionptr = NULL;

	c = oxptr->vftbl->class;

	/* find the printStackTrace() method */

	m = class_resolveclassmethod(c,
								 utf_printStackTrace,
								 utf_void__void,
								 class_java_lang_Object,
								 false);

	if (m == NULL)
		vm_abort("exceptions_print_stacktrace: printStackTrace()V not found");

	/* print compatibility message */

	fprintf(stderr, "Exception in thread \"main\" ");

	/* print the stacktrace */

	(void) vm_call_method(m, oxptr);

	/* This normally means, we are EXTREMLY out of memory or
	   have a serious problem while printStackTrace. But may
	   be another exception, so print it. */

	xptr = *exceptionptr;

	if (xptr != NULL) {
		fprintf(stderr, "Exception while printStackTrace(): ");

		/* now print original exception */

		exceptions_print_exception(xptr);
		stacktrace_print_trace(xptr);

		/* now print original exception */

		fprintf(stderr, "Original exception was: ");
		exceptions_print_exception(oxptr);
		stacktrace_print_trace(oxptr);
	}

	fflush(stderr);
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
 * vim:noexpandtab:sw=4:ts=4:
 */
