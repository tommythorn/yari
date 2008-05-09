/* src/native/vm/gnu/gnu_java_lang_management_VMThreadMXBeanImpl.c

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

   $Id: VMFrame.c 4996 2006-05-31 13:53:16Z motse $

*/


#include "config.h"
#include "vm/types.h"

#include "mm/gc-common.h"

#include "native/jni.h"
#include "native/include/java_lang_Throwable.h"
#include "native/include/java_lang_management_ThreadInfo.h"

#include "toolbox/logging.h"

#include "vm/vm.h"

#include "vmcore/classcache.h"


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    findMonitorDeadlockedThreads
 * Signature: ()[J
 */
JNIEXPORT java_longarray* JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_findMonitorDeadlockedThreads(JNIEnv *env, jclass clazz)
{
	log_println("Java_gnu_java_lang_management_VMThreadMXBeanImpl_findMonitorDeadlockedThreads: IMPLEMENT ME!");

	return NULL;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getCurrentThreadCpuTime
 * Signature: ()J
 */
JNIEXPORT s8 JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getCurrentThreadCpuTime(JNIEnv *env, jclass clazz)
{
	log_println("Java_gnu_java_lang_management_VMThreadMXBeanImpl_getCurrentThreadCpuTime: IMPLEMENT ME!");

	return 0;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getCurrentThreadUserTime
 * Signature: ()J
 */
JNIEXPORT s8 JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getCurrentThreadUserTime(JNIEnv *env, jclass clazz)
{
	log_println("Java_gnu_java_lang_management_VMThreadMXBeanImpl_getCurrentThreadUserTime: IMPLEMENT ME!");

	return 0;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getPeakThreadCount
 * Signature: ()I
 */
JNIEXPORT s4 JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getPeakThreadCount(JNIEnv *env, jclass clazz)
{
	return _Jv_jvm->java_lang_management_ThreadMXBean_PeakThreadCount;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getThreadCpuTime
 * Signature: (J)J
 */
JNIEXPORT s8 JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getThreadCpuTime(JNIEnv *env, jclass clazz, s8 id)
{
	log_println("Java_gnu_java_lang_management_VMThreadMXBeanImpl_getThreadCpuTime: IMPLEMENT ME!");

	return 0;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getThreadInfoForId
 * Signature: (JI)Ljava/lang/management/ThreadInfo;
 */
JNIEXPORT java_lang_management_ThreadInfo* JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getThreadInfoForId(JNIEnv *env, jclass clazz, s8 id, s4 maxDepth)
{
	log_println("Java_gnu_java_lang_management_VMThreadMXBeanImpl_getThreadInfoForId: IMPLEMENT ME!");

	return NULL;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getThreadUserTime
 * Signature: (J)J
 */
JNIEXPORT s8 JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getThreadUserTime(JNIEnv *env, jclass clazz, s8 par1)
{
	log_println("Java_gnu_java_lang_management_VMThreadMXBeanImpl_getThreadUserTime: IMPLEMENT ME!");

	return 0;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    getTotalStartedThreadCount
 * Signature: ()J
 */
JNIEXPORT s8 JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_getTotalStartedThreadCount(JNIEnv *env, jclass clazz)
{
	return _Jv_jvm->java_lang_management_ThreadMXBean_TotalStartedThreadCount;
}


/*
 * Class:     gnu/java/lang/management/VMThreadMXBeanImpl
 * Method:    resetPeakThreadCount
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_gnu_java_lang_management_VMThreadMXBeanImpl_resetPeakThreadCount(JNIEnv *env, jclass clazz)
{
	_Jv_jvm->java_lang_management_ThreadMXBean_PeakThreadCount =
		_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount;
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
