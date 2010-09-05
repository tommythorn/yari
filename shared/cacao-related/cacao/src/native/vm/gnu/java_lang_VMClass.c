/* src/native/vm/gnu/java_lang_VMClass.c

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

   $Id: java_lang_VMClass.c 7246 2007-01-29 18:49:05Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "native/jni.h"
#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_ClassLoader.h"
#include "native/include/java_lang_Object.h"
#include "native/include/java_lang_Throwable.h"
#include "native/include/java_lang_VMClass.h"
#include "native/include/java_lang_reflect_Constructor.h"
#include "native/include/java_lang_reflect_Method.h"

#include "native/vm/java_lang_Class.h"


/*
 * Class:     java/lang/VMClass
 * Method:    isInstance
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isInstance(JNIEnv *env, jclass clazz, java_lang_Class *klass, java_lang_Object *o)
{
	return _Jv_java_lang_Class_isInstance(klass, o);
}


/*
 * Class:     java/lang/VMClass
 * Method:    isAssignableFrom
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isAssignableFrom(JNIEnv *env, jclass clazz, java_lang_Class *klass, java_lang_Class *c)
{
	return _Jv_java_lang_Class_isAssignableFrom(klass, c);
}


/*
 * Class:     java/lang/VMClass
 * Method:    isInterface
 * Signature: ()Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isInterface(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_isInterface(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    isPrimitive
 * Signature: ()Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isPrimitive(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return  _Jv_java_lang_Class_isPrimitive(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT java_lang_String* JNICALL Java_java_lang_VMClass_getName(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getName(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getSuperclass
 * Signature: ()Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_VMClass_getSuperclass(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getSuperclass(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getInterfaces
 * Signature: ()[Ljava/lang/Class;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMClass_getInterfaces(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getInterfaces(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getComponentType
 * Signature: ()Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_VMClass_getComponentType(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getComponentType(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getModifiers
 * Signature: (Z)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_getModifiers(JNIEnv *env, jclass clazz, java_lang_Class *klass, s4 ignoreInnerClassesAttrib)
{
	return _Jv_java_lang_Class_getModifiers(klass, ignoreInnerClassesAttrib);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getDeclaringClass
 * Signature: ()Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_VMClass_getDeclaringClass(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getDeclaringClass(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getDeclaredClasses
 * Signature: (Z)[Ljava/lang/Class;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMClass_getDeclaredClasses(JNIEnv *env, jclass clazz, java_lang_Class *klass, s4 publicOnly)
{
	return _Jv_java_lang_Class_getDeclaredClasses(klass, publicOnly);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getDeclaredFields
 * Signature: (Z)[Ljava/lang/reflect/Field;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMClass_getDeclaredFields(JNIEnv *env, jclass clazz, java_lang_Class *klass, s4 publicOnly)
{
	return _Jv_java_lang_Class_getDeclaredFields(klass, publicOnly);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getDeclaredMethods
 * Signature: (Z)[Ljava/lang/reflect/Method;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMClass_getDeclaredMethods(JNIEnv *env, jclass clazz, java_lang_Class *klass, s4 publicOnly)
{
	return _Jv_java_lang_Class_getDeclaredMethods(klass, publicOnly);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getDeclaredConstructors
 * Signature: (Z)[Ljava/lang/reflect/Constructor;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMClass_getDeclaredConstructors(JNIEnv *env, jclass clazz, java_lang_Class *klass, s4 publicOnly)
{
	return _Jv_java_lang_Class_getDeclaredConstructors(klass, publicOnly);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getClassLoader
 * Signature: ()Ljava/lang/ClassLoader;
 */
JNIEXPORT java_lang_ClassLoader* JNICALL Java_java_lang_VMClass_getClassLoader(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getClassLoader(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    forName
 * Signature: (Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_VMClass_forName(JNIEnv *env, jclass clazz, java_lang_String *name, s4 initialize, java_lang_ClassLoader *loader)
{
	return _Jv_java_lang_Class_forName(name, initialize, loader);
}


/*
 * Class:     java/lang/VMClass
 * Method:    isArray
 * Signature: ()Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isArray(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_isArray(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    throwException
 * Signature: (Ljava/lang/Throwable;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMClass_throwException(JNIEnv *env, jclass clazz, java_lang_Throwable *t)
{
	_Jv_java_lang_Class_throwException(t);
}


#if 0
/*
 * Class:     java/lang/VMClass
 * Method:    getDeclaredAnnotations
 * Signature: (Ljava/lang/Class;)[Ljava/lang/annotation/Annotation;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_VMClass_getDeclaredAnnotations(JNIEnv *env, jclass clazz, java_lang_Class* klass)
{
}
#endif


/*
 * Class:     java/lang/VMClass
 * Method:    getEnclosingClass
 * Signature: (Ljava/lang/Class;)Ljava/lang/Class;
 */
JNIEXPORT java_lang_Class* JNICALL Java_java_lang_VMClass_getEnclosingClass(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getEnclosingClass(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getEnclosingConstructor
 * Signature: (Ljava/lang/Class;)Ljava/lang/reflect/Constructor;
 */
JNIEXPORT java_lang_reflect_Constructor* JNICALL Java_java_lang_VMClass_getEnclosingConstructor(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getEnclosingConstructor(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getEnclosingMethod
 * Signature: (Ljava/lang/Class;)Ljava/lang/reflect/Method;
 */
JNIEXPORT java_lang_reflect_Method* JNICALL Java_java_lang_VMClass_getEnclosingMethod(JNIEnv *env, jclass clazz, java_lang_Class *klass)
{
	return _Jv_java_lang_Class_getEnclosingMethod(klass);
}


/*
 * Class:     java/lang/VMClass
 * Method:    getClassSignature
 * Signature: (Ljava/lang/Class;)Ljava/lang/String;
 */
JNIEXPORT java_lang_String* JNICALL Java_java_lang_VMClass_getClassSignature(JNIEnv *env, jclass clazz, java_lang_Class* klass)
{
	return _Jv_java_lang_Class_getClassSignature(klass);
}


#if 0
/*
 * Class:     java/lang/VMClass
 * Method:    isAnonymousClass
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isAnonymousClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);


/*
 * Class:     java/lang/VMClass
 * Method:    isLocalClass
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isLocalClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);


/*
 * Class:     java/lang/VMClass
 * Method:    isMemberClass
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMClass_isMemberClass(JNIEnv *env, jclass clazz, struct java_lang_Class* par1);
#endif


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
