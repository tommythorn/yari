/* src/native/vm/java_lang_ClassLoader.h - java/lang/ClassLoader functions

   Copyright (C) 2007 R. Grafl, A. Krall, C. Kruegel, C. Oates,
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

   $Id: java_lang_VMClass.c 6131 2006-12-06 22:15:57Z twisti $

*/


#ifndef _JV_JAVA_LANG_CLASSLOADER_H
#define _JV_JAVA_LANG_CLASSLOADER_H

#include "config.h"
#include "vm/types.h"

#include "native/jni.h"

#include "native/include/java_lang_Object.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_String.h"/* required by java_lang_Class.h */
# include "native/include/java_lang_Class.h"
# include "native/include/java_lang_ClassLoader.h"
# include "native/include/java_security_ProtectionDomain.h"
#endif

#include "vm/global.h"


/* function prototypes ********************************************************/

#if defined(ENABLE_JAVASE)
java_lang_Class *_Jv_java_lang_ClassLoader_defineClass(java_lang_ClassLoader *cl, java_lang_String *name, java_bytearray *data, s4 offset, s4 len, java_security_ProtectionDomain *pd);
#endif

#endif /* _JV_JAVA_LANG_CLASSLOADER_H */


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
