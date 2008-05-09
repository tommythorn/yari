/* src/native/vm/gnu/java_lang_VMObject.c - java/lang/VMObject

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

   Authors: Roman Obermaiser
            Joseph Wenninger
            Christian Thalinger

   $Id: java_lang_VMObject.c 6221 2006-12-21 15:56:38Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "native/jni.h"
#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_Cloneable.h"
#include "native/include/java_lang_Object.h"
#include "native/vm/java_lang_Object.h"


/*
 * Class:     java/lang/VMObject
 * Method:    getClass
 * Signature: (Ljava/lang/Object;)Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_VMObject_getClass(JNIEnv *env, jclass clazz, java_lang_Object *obj)
{
	return _Jv_java_lang_Object_getClass(obj);
}


/*
 * Class:     java/lang/VMObject
 * Method:    clone
 * Signature: (Ljava/lang/Cloneable;)Ljava/lang/Object;
 */
JNIEXPORT java_lang_Object* JNICALL Java_java_lang_VMObject_clone(JNIEnv *env, jclass clazz, java_lang_Cloneable *this)
{
	return _Jv_java_lang_Object_clone(this);
}


/*
 * Class:     java/lang/VMObject
 * Method:    notify
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMObject_notify(JNIEnv *env, jclass clazz, java_lang_Object *this)
{
	_Jv_java_lang_Object_notify(this);
}


/*
 * Class:     java/lang/VMObject
 * Method:    notifyAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMObject_notifyAll(JNIEnv *env, jclass clazz, java_lang_Object *this)
{
	_Jv_java_lang_Object_notifyAll(this);
}


/*
 * Class:     java/lang/VMObject
 * Method:    wait
 * Signature: (Ljava/lang/Object;JI)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMObject_wait(JNIEnv *env, jclass clazz, java_lang_Object *o, s8 ms, s4 ns)
{
	_Jv_java_lang_Object_wait(o, ms, ns);
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
