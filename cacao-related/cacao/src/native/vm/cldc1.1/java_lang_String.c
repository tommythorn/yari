/* src/native/vm/cldc1.1/java_lang_String.c

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
#include <string.h>

#include "vm/types.h"

#include "native/jni.h"
#include "native/native.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Object.h"
#include "vm/stringlocal.h"


/*
 * Class:     java/lang/String
 * Method:    hashCode
 * Signature: ()I
 */
JNIEXPORT s4 JNICALL Java_java_lang_String_hashCode(JNIEnv *env, java_lang_String *this)
{
	java_chararray *value;
	s4              offset;
	s4              count;
	s4              hash;
	s4              i;

	/* get values from Java object */

	offset = this->offset;
	count  = this->count;
	value  = this->value;

	hash = 0;

	for (i = 0; i < count; i++) {
		hash = (31 * hash) + value->data[offset + i];
	}

	return hash;
}


/*
 * Class:     java/lang/String
 * Method:    indexOf
 * Signature: (I)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_String_indexOf__I(JNIEnv *env, java_lang_String *this, s4 ch)
{
	java_chararray *value;
	s4              offset;
	s4              count;
	s4              i;

	/* get values from Java object */

	offset = this->offset;
	count  = this->count;
	value  = this->value;

	for (i = 0; i < count; i++) {
		if (value->data[offset + i] == ch) {
			return i;
		}
	}

	return -1;
}


/*
 * Class:     java/lang/String
 * Method:    indexOf
 * Signature: (II)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_String_indexOf__II(JNIEnv *env, java_lang_String *this, s4 ch, s4 fromIndex)
{
	java_chararray *value;
	s4              offset;
	s4              count;
	s4              i;

	/* get values from Java object */

	offset = this->offset;
	count  = this->count;
	value  = this->value;

	if (fromIndex < 0) {
		fromIndex = 0;
	}
	else if (fromIndex >= count) {
		/* Note: fromIndex might be near -1>>>1. */
		return -1;
	}

	for (i = fromIndex ; i < count ; i++) {
		if (value->data[offset + i] == ch) {
			return i;
		}
	}

	return -1;
}


/*
 * Class:     java/lang/String
 * Method:    lastIndexOf
 * Signature: (I)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_String_lastIndexOf__I(JNIEnv *env, java_lang_String *this, s4 ch)
{
	return Java_java_lang_String_lastIndexOf__II(env, this, ch, this->count - 1);
}


/*
 * Class:     java/lang/String
 * Method:    lastIndexOf
 * Signature: (II)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_String_lastIndexOf__II(JNIEnv *env, java_lang_String *this, s4 ch, s4 fromIndex)
{
	java_chararray *value;
	s4              offset;
	s4              count;
	s4              start;
	s4              i;

	/* get values from Java object */

	offset = this->offset;
	count  = this->count;
	value  = this->value;

	start = ((fromIndex >= count) ? count - 1 : fromIndex);

	for (i = start; i >= 0; i--) {
		if (value->data[offset + i] == ch) {
			return i;
		}
	}

	return -1;
}


#if 0
/*
 * Class:     java/lang/String
 * Method:    equals
 * Signature: (Ljava/lang/Object;)Z;
 */
JNIEXPORT s4 JNICALL Java_java_lang_String_equals(JNIEnv *env, java_lang_String* this, java_lang_Object *o)
{
	java_lang_String* s;

	/* TODO: is this the correct implementation for short-circuiting on object identity? */
	if ((java_lang_Object*)this == o)
		return 1;

	if (o->header.vftbl->class != class_java_lang_String) 
		return 0;

	s = (java_lang_String *) o;

	if (this->count != s->count)
		return 0;

	return ( 0 == memcmp((void*)(this->value->data + this->offset),
						 (void*)(s->value->data + s->offset),
						 this->count) );
}
#endif


/*
 * Class:     java/lang/String
 * Method:    intern
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT java_lang_String* JNICALL Java_java_lang_String_intern(JNIEnv *env, java_lang_String *this)
{
	java_objectheader *o;

	if (this == NULL)
		return NULL;

	/* search table so identical strings will get identical pointers */

	o = literalstring_u2(this->value, this->count, this->offset, true);

	return (java_lang_String *) o;
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
