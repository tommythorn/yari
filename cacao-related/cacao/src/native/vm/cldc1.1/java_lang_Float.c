/* src/native/vm/cldc1.1/java_lang_Float.c

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

#include "native/jni.h"

#include "native/include/java_lang_Float.h"

#include "vm/builtin.h"


/*
 * Class:     java/lang/Float
 * Method:    floatToIntBits
 * Signature: (F)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_Float_floatToIntBits(JNIEnv *env, jclass clazz, float value)
{
	imm_union val;
	int e, f;

	val.f = value;

	e = val.i & 0x7f800000;
	f = val.i & 0x007fffff;

	if (e == FLT_POSINF && f != 0)
		return FLT_NAN;

	return val.i;
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
