/* src/native/vm/java_lang_ClassLoader.c - java/lang/ClassLoader

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

   $Id: java_lang_VMClass.c 6131 2006-12-06 22:15:57Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <string.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "vm/global.h"                          /* required by native headers */

#include "native/jni.h"

#include "native/include/java_lang_Object.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_String.h"/* required by java_lang_Class.h */
# include "native/include/java_lang_Class.h"
# include "native/include/java_lang_ClassLoader.h"
# include "native/include/java_security_ProtectionDomain.h"
#endif

#include "vm/exceptions.h"
#include "vm/stringlocal.h"

#include "vmcore/class.h"
#include "vmcore/classcache.h"
#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif


/*
 * Class:     java/lang/ClassLoader
 * Method:    defineClass
 * Signature: (Ljava/lang/ClassLoader;Ljava/lang/String;[BIILjava/security/ProtectionDomain;)Ljava/lang/Class;
 */
java_lang_Class *_Jv_java_lang_ClassLoader_defineClass(java_lang_ClassLoader *cl, java_lang_String *name, java_bytearray *data, s4 offset, s4 len, java_security_ProtectionDomain *pd)
{
	java_objectheader *loader;
	utf               *utfname;
	classinfo         *c;
	classinfo         *r;
	classbuffer       *cb;
	java_lang_Class   *co;
#if defined(ENABLE_JVMTI)
	jint new_class_data_len = 0;
	unsigned char* new_class_data = NULL;
#endif

	loader = (java_objectheader *) cl;

	/* check if data was passed */

	if (data == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	/* check the indexes passed */

	if ((offset < 0) || (len < 0) || ((offset + len) > data->header.size)) {
		exceptions_throw_arrayindexoutofboundsexception();
		return NULL;
	}

	if (name != NULL) {
		/* convert '.' to '/' in java string */

		utfname = javastring_toutf((java_objectheader *) name, true);
		
		/* check if this class has already been defined */

		c = classcache_lookup_defined_or_initiated(loader, utfname);

		if (c != NULL) {
			exceptions_throw_linkageerror("duplicate class definition: ", c);
			return NULL;
		}
	} 
	else {
		utfname = NULL;
	}

#if defined(ENABLE_JVMTI)
	/* fire Class File Load Hook JVMTI event */

	if (jvmti)
		jvmti_ClassFileLoadHook(utfname, len, (unsigned char *) data->data, 
								loader, (java_objectheader *) pd, 
								&new_class_data_len, &new_class_data);
#endif

	/* create a new classinfo struct */

	c = class_create_classinfo(utfname);

#if defined(ENABLE_STATISTICS)
	/* measure time */

	if (opt_getloadingtime)
		loadingtime_start();
#endif

	/* build a classbuffer with the given data */

	cb = NEW(classbuffer);
	cb->class = c;
#if defined(ENABLE_JVMTI)
	/* check if the JVMTI wants to modify the class */
	if (new_class_data == NULL) {
#endif
	cb->size  = len;
	cb->data  = (u1 *) &data->data[offset];
#if defined(ENABLE_JVMTI)
	} else {
		cb->size  = new_class_data_len;
		cb->data  = (u1 *) new_class_data;
	}
#endif
	cb->pos   = cb->data;

	/* preset the defining classloader */

	c->classloader = loader;

	/* load the class from this buffer */

	r = load_class_from_classbuffer(cb);

	/* free memory */

	FREE(cb, classbuffer);

#if defined(ENABLE_STATISTICS)
	/* measure time */

	if (opt_getloadingtime)
		loadingtime_stop();
#endif

	if (r == NULL) {
		/* If return value is NULL, we had a problem and the class is
		   not loaded.  Now free the allocated memory, otherwise we
		   could run into a DOS. */

		class_free(c);

		return NULL;
	}

	/* set ProtectionDomain */

	co = (java_lang_Class *) c;

	co->pd = pd;

	/* Store the newly defined class in the class cache. This call also       */
	/* checks whether a class of the same name has already been defined by    */
	/* the same defining loader, and if so, replaces the newly created class  */
	/* by the one defined earlier.                                            */
	/* Important: The classinfo given to classcache_store must be             */
	/*            fully prepared because another thread may return this       */
	/*            pointer after the lookup at to top of this function         */
	/*            directly after the class cache lock has been released.      */

	c = classcache_store(loader, c, true);

	return (java_lang_Class *) c;
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
