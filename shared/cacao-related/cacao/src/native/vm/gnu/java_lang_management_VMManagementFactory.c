/* src/native/vm/gnu/java_lang_management_VMManagementFactory.c

   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: VMFrame.c 4996 2006-05-31 13:53:16Z motse $

*/


#include "config.h"

#include <stdlib.h>

#include "vm/types.h"

#include "native/jni.h"

#include "toolbox/logging.h"

#include "vm/builtin.h"

#include "vmcore/class.h"


/*
 * Class:     java/lang/management/VMManagementFactory
 * Method:    getMemoryPoolNames
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_management_VMManagementFactory_getMemoryPoolNames(JNIEnv *env, jclass clazz)
{
	java_objectarray *oa;

	log_println("Java_java_lang_management_VMManagementFactory_getMemoryPoolNames: IMPLEMENT ME!");

	oa = builtin_anewarray(0, class_java_lang_String);

	return oa;
}


/*
 * Class:     java/lang/management/VMManagementFactory
 * Method:    getMemoryManagerNames
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_management_VMManagementFactory_getMemoryManagerNames(JNIEnv *env, jclass clazz)
{
	java_objectarray *oa;

	log_println("Java_java_lang_management_VMManagementFactory_getMemoryManagerNames: IMPLEMENT ME!");

	oa = builtin_anewarray(0, class_java_lang_String);

	return oa;
}


/*
 * Class:     java/lang/management/VMManagementFactory
 * Method:    getGarbageCollectorNames
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT java_objectarray* JNICALL Java_java_lang_management_VMManagementFactory_getGarbageCollectorNames(JNIEnv *env, jclass clazz)
{
	java_objectarray *oa;

	log_println("Java_java_lang_management_VMManagementFactory_getGarbageCollectorNames: IMPLEMENT ME!");

	oa = builtin_anewarray(0, class_java_lang_String);

	return oa;
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
