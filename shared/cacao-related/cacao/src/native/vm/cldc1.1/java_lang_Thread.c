/* src/native/vm/cldc1.1/java_lang_Thread.c

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

#include "native/include/java_lang_Thread.h"

#include "native/vm/java_lang_Thread.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif

#include "toolbox/logging.h"

#include "vm/builtin.h"


/*
 * Class:     java/lang/Thread
 * Method:    currentThread
 * Signature: ()Ljava/lang/Thread;
 */
JNIEXPORT java_lang_Thread* JNICALL Java_java_lang_Thread_currentThread(JNIEnv *env, jclass clazz)
{
	return _Jv_java_lang_Thread_currentThread();
}


/*
 * Class:     java/lang/Thread
 * Method:    setPriority0
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_setPriority0(JNIEnv *env, java_lang_Thread *this, s4 oldPriority, s4 newPriority)
{
	_Jv_java_lang_Thread_setPriority(this, newPriority);
}


/*
 * Class:     java/lang/Thread
 * Method:    sleep
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_sleep(JNIEnv *env, jclass clazz, s8 millis)
{
	_Jv_java_lang_Thread_sleep(millis);
}


/*
 * Class:     java/lang/Thread
 * Method:    start0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_start0(JNIEnv *env, java_lang_Thread *this)
{
	_Jv_java_lang_Thread_start(this, 0);
}


/*
 * Class:     java/lang/Thread
 * Method:    isAlive
 * Signature: ()Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_Thread_isAlive(JNIEnv *env, java_lang_Thread *this)
{
	return _Jv_java_lang_Thread_isAlive(this);
}


#if 0
/*
 * Class:     java/lang/Thread
 * Method:    activeCount
 * Signature: ()I
 */
JNIEXPORT s4 JNICALL Java_java_lang_Thread_activeCount(JNIEnv *env, jclass clazz)
{
}


/*
 * Class:     java/lang/Thread
 * Method:    setPriority0
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_setPriority0(JNIEnv *env, struct java_lang_Thread* this, s4 par1, s4 par2)
{
}


/*
 * Class:     java/lang/Thread
 * Method:    interrupt0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_interrupt0(JNIEnv *env, struct java_lang_Thread* this)
{
}


/*
 * Class:     java/lang/Thread
 * Method:    internalExit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_internalExit(JNIEnv *env, struct java_lang_Thread* this)
{
}
#endif


/*
 * Class:     java/lang/Thread
 * Method:    yield
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_Thread_yield(JNIEnv *env, jclass clazz)
{
#if defined(ENABLE_THREADS)
	threads_yield();
#endif
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
