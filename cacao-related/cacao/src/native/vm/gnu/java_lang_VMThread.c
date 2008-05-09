/* src/native/vm/gnu/java_lang_VMThread.c

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

   $Id: java_lang_VMThread.c 7246 2007-01-29 18:49:05Z twisti $

*/


#include "config.h"
#include "vm/types.h"

#include "native/jni.h"
#include "native/native.h"
#include "native/include/java_lang_ThreadGroup.h"
#include "native/include/java_lang_Object.h"            /* java_lang_Thread.h */
#include "native/include/java_lang_Throwable.h"         /* java_lang_Thread.h */
#include "native/include/java_lang_VMThread.h"
#include "native/include/java_lang_Thread.h"
#include "native/vm/java_lang_Thread.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif


/*
 * Class:     java/lang/VMThread
 * Method:    countStackFrames
 * Signature: ()I
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMThread_countStackFrames(JNIEnv *env, java_lang_VMThread *this)
{
    return _Jv_java_lang_Thread_countStackFrames(this->thread);
}


/*
 * Class:     java/lang/VMThread
 * Method:    start
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_start(JNIEnv *env, java_lang_VMThread *this, s8 stacksize)
{
	_Jv_java_lang_Thread_start(this->thread, stacksize);
}


/*
 * Class:     java/lang/VMThread
 * Method:    interrupt
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_interrupt(JNIEnv *env, java_lang_VMThread *this)
{
	_Jv_java_lang_Thread_interrupt(this->thread);
}


/*
 * Class:     java/lang/VMThread
 * Method:    isInterrupted
 * Signature: ()Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMThread_isInterrupted(JNIEnv *env, java_lang_VMThread *this)
{
	return _Jv_java_lang_Thread_isInterrupted(this->thread);
}


/*
 * Class:     java/lang/VMThread
 * Method:    suspend
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_suspend(JNIEnv *env, java_lang_VMThread *this)
{
	_Jv_java_lang_Thread_suspend(this->thread);
}


/*
 * Class:     java/lang/VMThread
 * Method:    resume
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_resume(JNIEnv *env, java_lang_VMThread *this)
{
	_Jv_java_lang_Thread_resume(this->thread);
}


/*
 * Class:     java/lang/VMThread
 * Method:    nativeSetPriority
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_nativeSetPriority(JNIEnv *env, java_lang_VMThread *this, s4 priority)
{
	_Jv_java_lang_Thread_setPriority(this->thread, priority);
}


/*
 * Class:     java/lang/VMThread
 * Method:    nativeStop
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_nativeStop(JNIEnv *env, java_lang_VMThread *this, java_lang_Throwable *t)
{
	_Jv_java_lang_Thread_stop(this->thread, t);
}


/*
 * Class:     java/lang/VMThread
 * Method:    currentThread
 * Signature: ()Ljava/lang/Thread;
 */
JNIEXPORT java_lang_Thread* JNICALL Java_java_lang_VMThread_currentThread(JNIEnv *env, jclass clazz)
{
	return _Jv_java_lang_Thread_currentThread();
}


/*
 * Class:     java/lang/VMThread
 * Method:    yield
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMThread_yield(JNIEnv *env, jclass clazz)
{
	_Jv_java_lang_Thread_yield();
}


/*
 * Class:     java/lang/VMThread
 * Method:    interrupted
 * Signature: ()Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMThread_interrupted(JNIEnv *env, jclass clazz)
{
	return _Jv_java_lang_Thread_interrupted();
}


/*
 * Class:     java/lang/VMThread
 * Method:    holdsLock
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMThread_holdsLock(JNIEnv *env, jclass clazz, java_lang_Object* o)
{
	return _Jv_java_lang_Thread_holdsLock(o);
}


/*
 * Class:     java/lang/VMThread
 * Method:    getState
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT java_lang_String* JNICALL Java_java_lang_VMThread_getState(JNIEnv *env, java_lang_VMThread *this)
{
	return _Jv_java_lang_Thread_getState(this->thread);
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
