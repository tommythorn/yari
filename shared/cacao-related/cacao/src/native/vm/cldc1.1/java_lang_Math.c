/* src/native/vm/cldc1.1/java_lang_Math.c

   Copyright (C) 2006 R. Grafl, A. Krall, C. Kruegel, C. Oates,
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

   Contact: cacao@cacaojvm.org

   Authors: Phil Tomsich
            Christian Thalinger

   $Id: java_lang_VMRuntime.c 5900 2006-11-04 17:30:44Z michi $

*/


#include "config.h"
#include "vm/types.h"

#include "fdlibm/fdlibm.h"
#include "native/jni.h"


/*
 * Class:     java/lang/Math
 * Method:    ceil
 * Signature: (D)D
 */
JNIEXPORT double JNICALL Java_java_lang_Math_ceil(JNIEnv *env, jclass clazz, double a)
{
	return ceil(a);
}


/*
 * Class:     java/lang/Math
 * Method:    cos
 * Signature: (D)D
 */
JNIEXPORT double JNICALL Java_java_lang_Math_cos(JNIEnv *env, jclass clazz, double a)
{
	return cos(a);
}


/*
 * Class:     java/lang/Math
 * Method:    floor
 * Signature: (D)D
 */
JNIEXPORT double JNICALL Java_java_lang_Math_floor(JNIEnv *env, jclass clazz, double a)
{
	return floor(a);
}


/*
 * Class:     java/lang/Math
 * Method:    sin
 * Signature: (D)D
 */
JNIEXPORT double JNICALL Java_java_lang_Math_sin(JNIEnv *env, jclass clazz, double a)
{
	return sin(a);
}


/*
 * Class:     java/lang/Math
 * Method:    sqrt
 * Signature: (D)D
 */
JNIEXPORT double JNICALL Java_java_lang_Math_sqrt(JNIEnv *env, jclass clazz, double a)
{
	return sqrt(a);
}


/*
 * Class:     java/lang/Math
 * Method:    tan
 * Signature: (D)D
 */
JNIEXPORT double JNICALL Java_java_lang_Math_tan(JNIEnv *env, jclass clazz, double a)
{
	return tan(a);
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
