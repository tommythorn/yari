/* src/native/vm/gnu/java_lang_VMThrowable.c

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

   $Id: java_lang_VMThrowable.c 7720 2007-04-16 15:49:09Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "native/jni.h"
#include "native/native.h"
#include "native/include/gnu_classpath_Pointer.h"
#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_StackTraceElement.h"
#include "native/include/java_lang_Throwable.h"
#include "native/include/java_lang_VMThrowable.h"

#include "native/vm/java_lang_Class.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h"

#include "vm/jit/stacktrace.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"


/*
 * Class:     java/lang/VMThrowable
 * Method:    fillInStackTrace
 * Signature: (Ljava/lang/Throwable;)Ljava/lang/VMThrowable;
 */
JNIEXPORT java_lang_VMThrowable* JNICALL Java_java_lang_VMThrowable_fillInStackTrace(JNIEnv *env, jclass clazz, java_lang_Throwable *t)
{
	java_lang_VMThrowable *o;
	stacktracecontainer   *stc;

	o = (java_lang_VMThrowable *)
		native_new_and_init(class_java_lang_VMThrowable);

	if (o == NULL)
		return NULL;

	stc = stacktrace_fillInStackTrace();

	if (stc == NULL)
		return NULL;

	o->vmData = (gnu_classpath_Pointer *) stc;

	return o;
}


/*
 * Class:     java/lang/VMThrowable
 * Method:    getStackTrace
 * Signature: (Ljava/lang/Throwable;)[Ljava/lang/StackTraceElement;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMThrowable_getStackTrace(JNIEnv *env, java_lang_VMThrowable *this, java_lang_Throwable *t)
{
	stacktracecontainer         *stc;
	stacktracebuffer            *stb;
	stacktrace_entry            *ste;
	stacktrace_entry            *tmpste;
	s4                           size;
	s4                           i;
	classinfo                   *c;
	bool                         inexceptionclass;
	bool                         leftexceptionclass;

	methodinfo                  *m;
	java_objectarray            *oa;
	s4                           oalength;
	java_lang_StackTraceElement *o;
	java_lang_String            *filename;
	s4                           linenumber;
	java_lang_String            *declaringclass;

	/* get the stacktrace buffer from the VMThrowable object */

	stc = (stacktracecontainer *) this->vmData;
	stb = &(stc->stb);

	/* get the class of the Throwable object */

	c = t->header.vftbl->class;

	assert(stb != NULL);

	size = stb->used;

	assert(size >= 2);

	/* skip first 2 elements in stacktrace buffer:                            */
	/*   0: VMThrowable.fillInStackTrace                                      */
	/*   1: Throwable.fillInStackTrace                                        */

	ste = &(stb->entries[2]);
	size -= 2;

	if ((size > 0) && (ste->method != 0)) {
		/* not a builtin native wrapper*/

		if ((ste->method->class->name == utf_java_lang_Throwable) &&
			(ste->method->name == utf_init)) {
			/* We assume that we are within the initializer of the
			   exception object, the exception object itself should
			   not appear in the stack trace, so we skip till we reach
			   the first function, which is not an init function. */

			inexceptionclass = false;
			leftexceptionclass = false;

			while (size > 0) {
				/* check if we are in the exception class */

				if (ste->method->class == c)
					inexceptionclass = true;

				/* check if we left the exception class */

				if (inexceptionclass && (ste->method->class != c))
					leftexceptionclass = true;

				/* Found exception start point if we left the
				   initalizers or we left the exception class. */

				if ((ste->method->name != utf_init) || leftexceptionclass)
					break;

				/* go to next stacktrace element */

				ste++;
				size--;
			}
		}
	}


	/* now fill the stacktrace into java objects */

	m = class_findmethod(class_java_lang_StackTraceElement,
						 utf_init,
						 utf_new_char("(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;Z)V"));

	if (m == NULL)
		return NULL;

	/* count entries with a method name */

	for (oalength = 0, i = size, tmpste = ste; i > 0; i--, tmpste++)
		if (tmpste->method)
			oalength++;

	/* create the stacktrace element array */

	oa = builtin_anewarray(oalength, class_java_lang_StackTraceElement);

	if (oa == NULL)
		return NULL;

	for (i = 0; size > 0; size--, ste++, i++) {
		/* skip entries without a method name */

		if (ste->method == NULL) {
			i--;
			continue;
		}

		/* allocate a new stacktrace element */

		o = (java_lang_StackTraceElement *)
			builtin_new(class_java_lang_StackTraceElement);

		if (o == NULL)
			return NULL;

		/* get filename */

		if (!(ste->method->flags & ACC_NATIVE)) {
			if (ste->method->class->sourcefile)
				filename = (java_lang_String *) javastring_new(ste->method->class->sourcefile);
			else
				filename = NULL;
		}
		else
			filename = NULL;

		/* get line number */

		if (ste->method->flags & ACC_NATIVE)
			linenumber = -1;
		else
			linenumber = (ste->linenumber == 0) ? -1 : ste->linenumber;

		/* get declaring class name */

		declaringclass =
			_Jv_java_lang_Class_getName((java_lang_Class *) ste->method->class);

		/* fill the java.lang.StackTraceElement element */

		o->fileName       = filename;
		o->lineNumber     = linenumber;
		o->declaringClass = declaringclass;
		o->methodName     = (java_lang_String *) javastring_new(ste->method->name);
		o->isNative       = (ste->method->flags & ACC_NATIVE) ? 1 : 0;

		oa->data[i] = (java_objectheader *) o;
	}

	return oa;
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
