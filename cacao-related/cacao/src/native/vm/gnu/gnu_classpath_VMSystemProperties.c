/* src/native/vm/gnu/gnu_classpath_VMSystemProperties.c

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

   $Id: gnu_classpath_VMSystemProperties.c 7264 2007-01-31 14:05:57Z twisti $

*/


#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "native/jni.h"
#include "native/include/java_util_Properties.h"

#include "vm/exceptions.h"
#include "vm/properties.h"
#include "vm/vm.h"


/*
 * Class:     gnu/classpath/VMSystemProperties
 * Method:    preInit
 * Signature: (Ljava/util/Properties;)V
 */
JNIEXPORT void JNICALL Java_gnu_classpath_VMSystemProperties_preInit(JNIEnv *env, jclass clazz, java_util_Properties *properties)
{
	java_objectheader *p;

	p = (java_objectheader *) properties;

	if (p == NULL) {
		exceptions_throw_nullpointerexception();
		return;
	}

	/* fill the java.util.Properties object */

	properties_system_add_all(p);
}


/*
 * Class:     gnu/classpath/VMSystemProperties
 * Method:    postInit
 * Signature: (Ljava/util/Properties;)V
 */
JNIEXPORT void JNICALL Java_gnu_classpath_VMSystemProperties_postInit(JNIEnv *env, jclass clazz, java_util_Properties *properties)
{
	java_objectheader *p;
#if defined(WITH_JRE_LAYOUT)
	char *path;
	s4    len;
#endif

	p = (java_objectheader *) properties;

	if (p == NULL) {
		exceptions_throw_nullpointerexception();
		return;
	}

	/* post-set some properties */

#if defined(WITH_JRE_LAYOUT)
	/* XXX when we do it that way, we can't set these properties on
	   commandline */

	properties_system_add(p, "gnu.classpath.home", cacao_prefix);

	len =
		strlen("file://") +
		strlen(cacao_prefix) +
		strlen("/lib") +
		strlen("0");

	path = MNEW(char, len);

	strcpy(path, "file://");
	strcat(path, cacao_prefix);
	strcat(path, "/lib");

	properties_system_add(p, "gnu.classpath.home.url", path);

	MFREE(path, char, len);
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
 */
