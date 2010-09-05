/* src/native/vm/java_lang_Class.h - java/lang/Class functions

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

   $Id: java_lang_VMClass.c 6131 2006-12-06 22:15:57Z twisti $

*/


#ifndef _JV_JAVA_LANG_CLASS_H
#define _JV_JAVA_LANG_CLASS_H

#include "config.h"
#include "vm/types.h"

#include "native/jni.h"

#include "native/include/java_lang_String.h" /* required by java_lang_Class.h */
#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_Object.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_ClassLoader.h"
# include "native/include/java_lang_Throwable.h"
# include "native/include/java_lang_reflect_Constructor.h"
# include "native/include/java_lang_reflect_Method.h"
#endif


/* function prototypes ********************************************************/

java_lang_String              *_Jv_java_lang_Class_getName(java_lang_Class *klass);

#if defined(ENABLE_JAVASE)
java_lang_Class               *_Jv_java_lang_Class_forName(java_lang_String *name, s4 initialize, java_lang_ClassLoader *loader);
#elif defined(ENABLE_JAVAME_CLDC1_1)
java_lang_Class               *_Jv_java_lang_Class_forName(java_lang_String *name);
#endif

s4                             _Jv_java_lang_Class_isInstance(java_lang_Class *klass, java_lang_Object *o);
s4                             _Jv_java_lang_Class_isAssignableFrom(java_lang_Class *klass, java_lang_Class *c);
s4                             _Jv_java_lang_Class_isInterface(java_lang_Class *klass);

#if defined(ENABLE_JAVASE)
s4                             _Jv_java_lang_Class_isPrimitive(java_lang_Class *klass);
java_lang_Class               *_Jv_java_lang_Class_getSuperclass(java_lang_Class *klass);
java_objectarray              *_Jv_java_lang_Class_getInterfaces(java_lang_Class *klass);
java_lang_Class               *_Jv_java_lang_Class_getComponentType(java_lang_Class *klass);
s4                             _Jv_java_lang_Class_getModifiers(java_lang_Class *klass, s4 ignoreInnerClassesAttrib);
java_lang_Class               *_Jv_java_lang_Class_getDeclaringClass(java_lang_Class *klass);
java_objectarray              *_Jv_java_lang_Class_getDeclaredClasses(java_lang_Class *klass, s4 publicOnly);
java_objectarray              *_Jv_java_lang_Class_getDeclaredFields(java_lang_Class *klass, s4 publicOnly);
java_objectarray              *_Jv_java_lang_Class_getDeclaredMethods(java_lang_Class *klass, s4 publicOnly);
java_objectarray              *_Jv_java_lang_Class_getDeclaredConstructors(java_lang_Class *klass, s4 publicOnly);
java_lang_ClassLoader         *_Jv_java_lang_Class_getClassLoader(java_lang_Class *klass);
#endif

s4                             _Jv_java_lang_Class_isArray(java_lang_Class *klass);

#if defined(ENABLE_JAVASE)
void                           _Jv_java_lang_Class_throwException(java_lang_Throwable *t);

#if 0
java_objectarray              *_Jv_java_lang_Class_getDeclaredAnnotations(java_lang_Class* klass);
#endif

java_lang_Class               *_Jv_java_lang_Class_getEnclosingClass(java_lang_Class *klass);
java_lang_reflect_Constructor *_Jv_java_lang_Class_getEnclosingConstructor(java_lang_Class *klass);
java_lang_reflect_Method      *_Jv_java_lang_Class_getEnclosingMethod(java_lang_Class *klass);

java_lang_String              *_Jv_java_lang_Class_getClassSignature(java_lang_Class* klass);
#endif

#if 0
s4                             _Jv_java_lang_Class_isAnonymousClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isLocalClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isMemberClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);
#endif

#endif /* _JV_JAVA_LANG_CLASS_H */


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
