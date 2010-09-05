/* src/native/vm/gnu/sun_misc_Unsafe.c - sun/misc/Unsafe

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

   $Id: java_lang_VMObject.c 5153 2006-07-18 08:19:24Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "native/jni.h"
#include "native/include/java_lang_Object.h"
#include "native/include/java_lang_reflect_Field.h"
#include "native/include/java_lang_Thread.h" /* required by sun_misc_Unsafe.h */
#include "native/include/sun_misc_Unsafe.h"


/*
 * Class:     sun/misc/Unsafe
 * Method:    objectFieldOffset
 * Signature: (Ljava/lang/reflect/Field;)J
 */
JNIEXPORT s8 JNICALL Java_sun_misc_Unsafe_objectFieldOffset(JNIEnv *env, sun_misc_Unsafe* this, java_lang_reflect_Field* field)
{
	classinfo *c;
	fieldinfo *f;

	c = (classinfo *) field->declaringClass;
	f = &c->fields[field->slot];

	return (s8) f->offset;
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    compareAndSwapInt
 * Signature: (Ljava/lang/Object;JII)Z
 */
JNIEXPORT s4 JNICALL Java_sun_misc_Unsafe_compareAndSwapInt(JNIEnv *env, sun_misc_Unsafe* this, java_lang_Object* obj, s8 offset, s4 expect, s4 update)
{
	s4 *p;
	s4  value;

	p = (s4 *) (((u1 *) obj) + offset);

	/* XXX this should be atomic */

	value = *p;

	if (value == expect) {
		*p = update;

		return true;
	}

	return false;
}


#if 0
/*
 * Class:     sun/misc/Unsafe
 * Method:    compareAndSwapLong
 * Signature: (Ljava/lang/Object;JJJ)Z
 */
JNIEXPORT s4 JNICALL Java_sun_misc_Unsafe_compareAndSwapLong(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, s8 par3, s8 par4)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    compareAndSwapObject
 * Signature: (Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z
 */
JNIEXPORT s4 JNICALL Java_sun_misc_Unsafe_compareAndSwapObject(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, struct java_lang_Object* par3, struct java_lang_Object* par4)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putOrderedInt
 * Signature: (Ljava/lang/Object;JI)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putOrderedInt(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, s4 par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putOrderedLong
 * Signature: (Ljava/lang/Object;JJ)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putOrderedLong(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, s8 par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putOrderedObject
 * Signature: (Ljava/lang/Object;JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putOrderedObject(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, struct java_lang_Object* par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putIntVolatile
 * Signature: (Ljava/lang/Object;JI)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putIntVolatile(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, s4 par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    getIntVolatile
 * Signature: (Ljava/lang/Object;J)I
 */
JNIEXPORT s4 JNICALL Java_sun_misc_Unsafe_getIntVolatile(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putLongVolatile
 * Signature: (Ljava/lang/Object;JJ)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putLongVolatile(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, s8 par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putLong
 * Signature: (Ljava/lang/Object;JJ)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putLong(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, s8 par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    getLongVolatile
 * Signature: (Ljava/lang/Object;J)J
 */
JNIEXPORT s8 JNICALL Java_sun_misc_Unsafe_getLongVolatile(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    getLong
 * Signature: (Ljava/lang/Object;J)J
 */
JNIEXPORT s8 JNICALL Java_sun_misc_Unsafe_getLong(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putObjectVolatile
 * Signature: (Ljava/lang/Object;JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putObjectVolatile(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, struct java_lang_Object* par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    putObject
 * Signature: (Ljava/lang/Object;JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_putObject(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2, struct java_lang_Object* par3)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    getObjectVolatile
 * Signature: (Ljava/lang/Object;J)Ljava/lang/Object;
 */
JNIEXPORT struct java_lang_Object* JNICALL Java_sun_misc_Unsafe_getObjectVolatile(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Object* par1, s8 par2)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    arrayBaseOffset
 * Signature: (Ljava/lang/Class;)I
 */
JNIEXPORT s4 JNICALL Java_sun_misc_Unsafe_arrayBaseOffset(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Class* par1)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    arrayIndexScale
 * Signature: (Ljava/lang/Class;)I
 */
JNIEXPORT s4 JNICALL Java_sun_misc_Unsafe_arrayIndexScale(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Class* par1)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    unpark
 * Signature: (Ljava/lang/Thread;)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_unpark(JNIEnv *env, struct sun_misc_Unsafe* this, struct java_lang_Thread* par1)
{
}


/*
 * Class:     sun/misc/Unsafe
 * Method:    park
 * Signature: (ZJ)V
 */
JNIEXPORT void JNICALL Java_sun_misc_Unsafe_park(JNIEnv *env, struct sun_misc_Unsafe* this, s4 par1, s8 par2)
{
}
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
 */
