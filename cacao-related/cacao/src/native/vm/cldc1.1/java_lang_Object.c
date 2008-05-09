/* src/native/vm/cldc1.1/java_lang_Object.c

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

#include "native/jni.h"

#include "native/include/java_lang_String.h" /* required by java_lang_Class.h */
#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_Object.h"

#include "native/vm/java_lang_Object.h"


/*
 * Class:     java/lang/VMObject
 * Method:    getClass
 * Signature: (Ljava/lang/Object;)Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_Object_getClass(JNIEnv *env, java_lang_Object *obj)
{
	java_objectheader *o;
	classinfo         *c;

	o = (java_objectheader *) obj;

	if (o == NULL)
		return NULL;

	c = o->vftbl->class;

	return (java_lang_Class *) c;
}


/*
 * Class:     java/lang/Object
 * Method:    hashCode
 * Signature: ()I
 */
JNIEXPORT s4 JNICALL Java_java_lang_Object_hashCode(JNIEnv *env, java_lang_Object *this)
{
	return (s4) ((ptrint) this);
}


/*
 * Class:     java/lang/Object
 * Method:    notify
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_Object_notify(JNIEnv *env, java_lang_Object *this)
{
	_Jv_java_lang_Object_notify(this);
}


/*
 * Class:     java/lang/Object
 * Method:    notifyAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_Object_notifyAll(JNIEnv *env, java_lang_Object *this)
{
	_Jv_java_lang_Object_notifyAll(this);
}


/*
 * Class:     java/lang/Object
 * Method:    wait
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_java_lang_Object_wait(JNIEnv *env, java_lang_Object *this, s8 timeout)
{
	_Jv_java_lang_Object_wait(this, timeout, 0);
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
