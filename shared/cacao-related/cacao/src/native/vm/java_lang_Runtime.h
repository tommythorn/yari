/* src/native/vm/java_lang_Runtime.h

   Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel,
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

   $Id: java_lang_VMRuntime.c 7246 2007-01-29 18:49:05Z twisti $

*/


#ifndef _JV_JAVA_LANG_RUNTIME_H
#define _JV_JAVA_LANG_RUNTIME_H

#include "config.h"
#include "vm/types.h"

#include "native/jni.h"
#include "native/include/java_lang_String.h"


/* function prototypes *******************************************************/

void _Jv_java_lang_Runtime_exit(s4 status);
s8   _Jv_java_lang_Runtime_freeMemory(void);
s8   _Jv_java_lang_Runtime_totalMemory(void);
void _Jv_java_lang_Runtime_gc(void);

#if defined(ENABLE_JNI)
s4   _Jv_java_lang_Runtime_loadLibrary(JNIEnv *env, java_lang_String *libname, java_objectheader *cl);
#else
s4   _Jv_java_lang_Runtime_loadLibrary(java_lang_String *libname, java_objectheader *cl);
#endif

#if defined(ENABLE_JAVASE)
void _Jv_java_lang_Runtime_runFinalizersOnExit(s4 value);
#endif

#endif /* _JV_JAVA_LANG_RUNTIME_H */


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
