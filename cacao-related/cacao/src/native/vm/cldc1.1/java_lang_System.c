/* src/native/vm/cldc1.1/java_lang_System.c

   Copyright (C) 2006, 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
   R. Obermaisser, M. Platter, M. Probst, S. Ring, E. Steiner,
   C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich, J. Wenninger,
   Institut f. Computersprachen - TU Wien

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

   $Id: java_lang_VMRuntime.c 5900 2006-11-04 17:30:44Z michi $

*/


#include "config.h"

#include <stdlib.h>

#include "vm/types.h"

#include "mm/memory.h"
#include "native/jni.h"
#include "native/native.h"
#include "native/include/java_lang_Object.h"
#include "native/include/java_lang_String.h"

#include "vm/builtin.h"
#include "vm/properties.h"
#include "vm/stringlocal.h"


/*
 * Class:     java/lang/System
 * Method:    arraycopy
 * Signature: (Ljava/lang/Object;ILjava/lang/Object;II)V
 */
JNIEXPORT void JNICALL Java_java_lang_System_arraycopy(JNIEnv *env, jclass clazz, java_lang_Object *src, s4 srcStart, java_lang_Object *dest, s4 destStart, s4 len)
{
	(void) builtin_arraycopy((java_arrayheader *) src, srcStart,
							 (java_arrayheader *) dest, destStart, len);
}


/*
 * Class:     java/lang/System
 * Method:    getProperty0
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */

JNIEXPORT java_lang_String* JNICALL Java_java_lang_System_getProperty0(JNIEnv *env, jclass clazz, java_lang_String *s)
{
	java_objectheader *so;
	char*              key;
	char*              value;
	java_objectheader *result;

	so = (java_objectheader *) s;

	/* build an ASCII string out of the java/lang/String passed */

	key = javastring_tochar(so);

	/* get the property from the internal table */

	value = properties_get(key);

	/* release the memory allocated in javastring_tochar */

	MFREE(key, char, 0);

	if (value == NULL)
		return NULL;

	result = javastring_new_from_ascii(value);

	return (java_lang_String *) result;
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
