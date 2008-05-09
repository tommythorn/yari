/* src/native/vm/java_lang_Thread.h - java/lang/Thread functions

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

   $Id: java_lang_VMThread.c 6213 2006-12-18 17:36:06Z twisti $

*/


#ifndef _JV_JAVA_LANG_THREAD_H
#define _JV_JAVA_LANG_THREAD_H

#include "config.h"
#include "vm/types.h"

#include "native/jni.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Object.h"            /* java_lang_Thread.h */
#include "native/include/java_lang_Throwable.h"         /* java_lang_Thread.h */
#include "native/include/java_lang_Thread.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_ThreadGroup.h"
#endif

#if defined(WITH_CLASSPATH_GNU)
# include "native/include/java_lang_VMThread.h"
#endif


/* function prototypes ********************************************************/

s4                _Jv_java_lang_Thread_countStackFrames(java_lang_Thread *this);
void              _Jv_java_lang_Thread_sleep(s8 millis);
void              _Jv_java_lang_Thread_start(java_lang_Thread *this, s8 stacksize);
void              _Jv_java_lang_Thread_interrupt(java_lang_Thread *this);
s4                _Jv_java_lang_Thread_isAlive(java_lang_Thread *this);
s4                _Jv_java_lang_Thread_isInterrupted(java_lang_Thread *this);
void              _Jv_java_lang_Thread_suspend(java_lang_Thread *this);
void              _Jv_java_lang_Thread_resume(java_lang_Thread *this);
void              _Jv_java_lang_Thread_setPriority(java_lang_Thread *this, s4 priority);
void              _Jv_java_lang_Thread_stop(java_lang_Thread *this, java_lang_Throwable *t);
java_lang_Thread *_Jv_java_lang_Thread_currentThread(void);
void              _Jv_java_lang_Thread_yield(void);
s4                _Jv_java_lang_Thread_interrupted(void);
s4                _Jv_java_lang_Thread_holdsLock(java_lang_Object* o);
java_lang_String *_Jv_java_lang_Thread_getState(java_lang_Thread *this);

#endif /* _JV_JAVA_LANG_THREAD_H */


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
