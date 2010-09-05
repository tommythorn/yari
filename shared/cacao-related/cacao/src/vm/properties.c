/* src/vm/properties.c - handling commandline properties

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

   $Id: properties.c 7783M 2007-05-07 19:34:38Z (local) $

*/


#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vm/types.h"

#include "mm/memory.h"

#include "native/jni.h"

#include "vm/global.h"                      /* required by java_lang_String.h */
#include "native/include/java_lang_String.h"

#include "toolbox/list.h"
#include "toolbox/util.h"

#include "vm/properties.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"

#include "vmcore/method.h"
#include "vmcore/options.h"


/* internal property structure ************************************************/

typedef struct list_properties_entry list_properties_entry;

struct list_properties_entry {
	char       *key;
	char       *value;
	listnode_t  linkage;
};


/* global variables ***********************************************************/

static list_t *list_properties = NULL;


/* properties_init *************************************************************

   Initialize the properties list and fill the list with default
   values.

*******************************************************************************/

bool properties_init(void)
{
#if defined(ENABLE_JAVASE)
	char           *cwd;
	char           *env_java_home;
	char           *env_user;
	char           *env_home;
	char           *env_lang;
	char           *java_home;
	char           *extdirs;
	char           *lang;
	char           *country;
	struct utsname *utsnamebuf;
	s4              len;
#endif

	/* create the properties list */

	list_properties = list_create(OFFSET(list_properties_entry, linkage));

#if defined(ENABLE_JAVASE)
	/* get properties from system */

	cwd           = _Jv_getcwd();
	env_java_home = getenv("JAVA_HOME");
	env_user      = getenv("USER");
	env_home      = getenv("HOME");
	env_lang      = getenv("LANG");

	utsnamebuf = NEW(struct utsname);

	uname(utsnamebuf);

	/* set JAVA_HOME to default prefix if not defined */

	if (env_java_home == NULL)
		env_java_home = cacao_prefix;

	/* fill in system properties */

	properties_add("java.version", JAVA_VERSION);
	properties_add("java.vendor", "GNU Classpath");
	properties_add("java.vendor.url", "http://www.gnu.org/software/classpath/");

	/* add /jre to java.home property */

	len = strlen(env_java_home) + strlen("/jre") + strlen("0");

	java_home = MNEW(char, len);

	strcpy(java_home, env_java_home);
	strcat(java_home, "/jre");

	properties_add("java.home", java_home);

	properties_add("java.vm.specification.version", "1.0");
	properties_add("java.vm.specification.vendor", "Sun Microsystems Inc.");
	properties_add("java.vm.specification.name", "Java Virtual Machine Specification");
	properties_add("java.vm.version", VERSION);
	properties_add("java.vm.vendor", "CACAO Team");
	properties_add("java.vm.name", "CACAO");
	properties_add("java.specification.version", "1.5");
	properties_add("java.specification.vendor", "Sun Microsystems Inc.");
	properties_add("java.specification.name", "Java Platform API Specification");
	properties_add("java.class.version", CLASS_VERSION);
	properties_add("java.class.path", _Jv_classpath);

	properties_add("java.runtime.version", VERSION);
	properties_add("java.runtime.name", "CACAO");

	/* Set bootclasspath properties. One for GNU classpath and the
	   other for compatibility with Sun (required by most
	   applications). */

	properties_add("java.boot.class.path", _Jv_bootclasspath);
	properties_add("sun.boot.class.path", _Jv_bootclasspath);

#if defined(WITH_STATIC_CLASSPATH)
	properties_add("gnu.classpath.boot.library.path", ".");
	properties_add("java.library.path" , ".");
#else
	/* fill gnu.classpath.boot.library.path with GNU Classpath library
       path */

	properties_add("gnu.classpath.boot.library.path", classpath_libdir);
	properties_add("java.library.path", _Jv_java_library_path);
#endif

	properties_add("java.io.tmpdir", "/tmp");

#if defined(ENABLE_INTRP)
	if (opt_intrp) {
		/* XXX We don't support java.lang.Compiler */
/*  		properties_add("java.compiler", "cacao.intrp"); */
		properties_add("java.vm.info", "interpreted mode");
		properties_add("gnu.java.compiler.name", "cacao.intrp");
	}
	else
#endif
	{
		/* XXX We don't support java.lang.Compiler */
/*  		properties_add("java.compiler", "cacao.jit"); */
		properties_add("java.vm.info", "JIT mode");
		properties_add("gnu.java.compiler.name", "cacao.jit");
	}

	/* set the java.ext.dirs property */

	len = strlen(env_java_home) + strlen("/jre/lib/ext") + strlen("0");

	extdirs = MNEW(char, len);

	strcpy(extdirs, env_java_home);
	strcat(extdirs, "/jre/lib/ext");

	properties_add("java.ext.dirs", extdirs);

	properties_add("java.endorsed.dirs", ""CACAO_PREFIX"/jre/lib/endorsed");

#if defined(DISABLE_GC)
	/* When we disable the GC, we mmap the whole heap to a specific
	   address, so we can compare call traces. For this reason we have
	   to add the same properties on different machines, otherwise
	   more memory may be allocated (e.g. strlen("i386")
	   vs. strlen("alpha"). */

	properties_add("os.arch", "unknown");
 	properties_add("os.name", "unknown");
	properties_add("os.version", "unknown");
#else
	/* We need to set the os.arch hardcoded to be compatible with SUN. */

# if defined(__I386__)
	/* map all x86 architectures (i386, i486, i686) to i386 */

	properties_add("os.arch", "i386");
# elif defined(__POWERPC__)
	properties_add("os.arch", "ppc");
# elif defined(__X86_64__)
	properties_add("os.arch", "amd64");
# else
	/* default to what uname returns */

	properties_add("os.arch", utsnamebuf->machine);
# endif

 	properties_add("os.name", utsnamebuf->sysname);
	properties_add("os.version", utsnamebuf->release);
#endif

	properties_add("file.separator", "/");
	properties_add("path.separator", ":");
	properties_add("line.separator", "\n");
	properties_add("user.name", env_user ? env_user : "null");
	properties_add("user.home", env_home ? env_home : "null");
	properties_add("user.dir", cwd ? cwd : "null");

#if defined(WITH_STATIC_CLASSPATH)
	/* This is just for debugging purposes and can cause troubles in
       GNU Classpath. */

	properties_add("gnu.cpu.endian", "unknown");
#else
# if WORDS_BIGENDIAN == 1
	properties_add("gnu.cpu.endian", "big");
# else
	properties_add("gnu.cpu.endian", "little");
# endif
#endif

	/* get locale */

	if (env_lang != NULL) {
		/* get the local stuff from the environment */

		if (strlen(env_lang) <= 2) {
			properties_add("user.language", env_lang);
		}
		else {
			if ((env_lang[2] == '_') && (strlen(env_lang) >= 5)) {
				lang = MNEW(char, 3);
				strncpy(lang, (char *) &env_lang[0], 2);
				lang[2] = '\0';

				country = MNEW(char, 3);
				strncpy(country, (char *) &env_lang[3], 2);
				country[2] = '\0';

				properties_add("user.language", lang);
				properties_add("user.country", country);
			}
		}
	}
	else {
		/* if no default locale was specified, use `en_US' */

		properties_add("user.language", "en");
		properties_add("user.country", "US");
	}
#elif defined(ENABLE_JAVAME_CLDC1_1)
    properties_add("microedition.configuration", "CLDC-1.1");
    properties_add("microedition.platform", "generic");
    properties_add("microedition.encoding", "ISO8859_1");
    properties_add("microedition.profiles", "");
#else
#error unknown Java configuration
#endif

	/* everything's ok */

	return true;
}


/* properties_postinit *********************************************************

   Re-set some properties that may have changed during command-line
   parsing.

*******************************************************************************/

bool properties_postinit(void)
{
#if defined(ENABLE_JAVASE)
	properties_add("java.class.path", _Jv_classpath);
	properties_add("java.boot.class.path", _Jv_bootclasspath);
	properties_add("sun.boot.class.path", _Jv_bootclasspath);
#endif

	/* everything's ok */

	return true;
}


/* properties_add **************************************************************

   Adds a property entry to the internal property list.  If there's
   already an entry with the same key, replace it.

*******************************************************************************/

void properties_add(char *key, char *value)
{
	list_properties_entry *pe;

	/* search for the entry */

	for (pe = list_first_unsynced(list_properties); pe != NULL;
		 pe = list_next_unsynced(list_properties, pe)) {
		if (strcmp(pe->key, key) == 0) {
			/* entry was found, replace the value */

			pe->value = value;

			return;
		}
	}

	/* entry was not found, insert a new one */

	pe = NEW(list_properties_entry);

	pe->key   = key;
	pe->value = value;

	list_add_last_unsynced(list_properties, pe);
}


/* properties_get **************************************************************

   Get a property entry from the internal property list.

*******************************************************************************/

char *properties_get(char *key)
{
	list_properties_entry *pe;

	for (pe = list_first_unsynced(list_properties); pe != NULL;
		 pe = list_next_unsynced(list_properties, pe)) {
		if (strcmp(pe->key, key) == 0)
			return pe->value;
	}

	return NULL;
}


/* properties_system_add *******************************************************

   Adds a given property to the Java system properties.

*******************************************************************************/

void properties_system_add(java_objectheader *p, char *key, char *value)
{
	methodinfo        *m;
	java_objectheader *k;
	java_objectheader *v;

	/* search for method to add properties */

	m = class_resolveclassmethod(p->vftbl->class,
								 utf_put,
								 utf_new_char("(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"),
								 NULL,
								 true);

	if (m == NULL)
		return;

	/* add to the Java system properties */

	k = javastring_new_from_utf_string(key);
	v = javastring_new_from_utf_string(value);

	(void) vm_call_method(m, p, k, v);
}


/* properties_system_add_all ***************************************************

   Adds all properties from the properties list to the Java system
   properties.

   ARGUMENTS:
       p.... is actually a java_util_Properties structure

*******************************************************************************/

#if defined(ENABLE_JAVASE)
void properties_system_add_all(java_objectheader *p)
{
	list_properties_entry *pe;
	methodinfo            *m;
	java_objectheader     *key;
	java_objectheader     *value;

	/* search for method to add properties */

	m = class_resolveclassmethod(p->vftbl->class,
								 utf_put,
								 utf_new_char("(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"),
								 NULL,
								 true);

	if (m == NULL)
		return;

	/* process all properties stored in the internal table */

	for (pe = list_first(list_properties); pe != NULL;
		 pe = list_next(list_properties, pe)) {
		/* add to the Java system properties */

		key   = javastring_new_from_utf_string(pe->key);
		value = javastring_new_from_utf_string(pe->value);

		(void) vm_call_method(m, (java_objectheader *) p, key, value);
	}
}
#endif /* defined(ENABLE_JAVASE) */


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
