/* src/native/vm/cldc1.1/java_lang_Double.c

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
#include "vm/types.h"

#include "native/jni.h"

#include "native/include/java_lang_Double.h"

#include "vm/builtin.h"


/*
 * Class:     java/lang/Double
 * Method:    doubleToLongBits
 * Signature: (D)J
 */
JNIEXPORT s8 JNICALL Java_java_lang_Double_doubleToLongBits(JNIEnv *env, jclass clazz, double doubleValue)
{
	jvalue val;
	s8  e, f;
	val.d = doubleValue;

#if defined(__IEEE_BYTES_LITTLE_ENDIAN)
	/* On little endian ARM processors when using FPA, word order of
	   doubles is still big endian. So take that into account here. When
	   using VFP, word order of doubles follows byte order. */

#define SWAP_DOUBLE(a)    (((a) << 32) | (((a) >> 32) & 0x00000000ffffffff))

	val.j = SWAP_DOUBLE(val.j);
#endif

	e = val.j & 0x7ff0000000000000LL;
	f = val.j & 0x000fffffffffffffLL;

	if (e == DBL_POSINF && f != 0L)
		val.j = DBL_NAN;

	return val.j;
}


/*
 * Class:     java/lang/Double
 * Method:    longBitsToDouble
 * Signature: (J)D
 */
JNIEXPORT double JNICALL Java_java_lang_Double_longBitsToDouble(JNIEnv *env, jclass clazz, s8 longValue)
{
	jvalue val;
	val.j = longValue;

#if defined(__IEEE_BYTES_LITTLE_ENDIAN)
	val.j = SWAP_DOUBLE(val.j);
#endif

	return val.d;
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
