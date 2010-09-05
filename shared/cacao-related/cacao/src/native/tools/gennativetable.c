/* src/native/tools/gennativetable.c - generate nativetable.h for native.c

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

   Contact: cacao@cacaojvm.org

   Authors: Christian Thalinger

   Changes:

   $Id: gennativetable.c 6104 2006-12-02 14:54:27Z tbfg $

*/


#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "cacaoh/headers.h"
#include "mm/gc-common.h"
#include "mm/memory.h"

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#endif

#include "toolbox/chain.h"
#include "vm/classcache.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/loader.h"
#include "vm/options.h"
#include "vm/stringlocal.h"
#include "vm/suck.h"


/* define heap sizes **********************************************************/

#define HEAP_MAXSIZE      4 * 1024 * 1024   /* default 4MB                    */
#define HEAP_STARTSIZE    100 * 1024        /* default 100kB                  */

#define ACC_NATIVELY_OVERLOADED    0x10000000


/* define cacaoh options ******************************************************/

enum {
	OPT_HELP,
	OPT_VERSION,
	OPT_VERBOSE,
	OPT_BOOTCLASSPATH,

	DUMMY
};


opt_struct opts[] = {
	{ "help",             false, OPT_HELP          },
	{ "version",          false, OPT_VERSION       },
	{ "verbose",          false, OPT_VERBOSE       },
	{ "bootclasspath",    true,  OPT_BOOTCLASSPATH },
	{ NULL,               false, 0                 }
};


static JavaVMInitArgs *gennativetable_options_prepare(int argc, char **argv);


int main(int argc, char **argv)
{
	JavaVMInitArgs *vm_args;
	char *bootclasspath;

	chain *nativemethod_chain;
	classcache_name_entry *nmen;
	classcache_class_entry *clsen;
	classinfo *c;
	s4 i;
	s4 j;
	u4 slot;
	methodinfo *m;
	methodinfo *m2;
	bool nativelyoverloaded;

#if defined(DISABLE_GC)
	gc_init(HEAP_MAXSIZE, HEAP_STARTSIZE);
#endif

	vm_args = gennativetable_options_prepare(argc, argv);

	while ((i = options_get(opts, vm_args)) != OPT_DONE) {
		switch (i) {
		case OPT_IGNORE:
			break;

		case OPT_HELP:
/* 			usage(); */
			break;

		case OPT_BOOTCLASSPATH:
			bootclasspath = MNEW(char, strlen(opt_arg) + strlen("0"));
			strcpy(bootclasspath, opt_arg);
			break;

		case OPT_VERSION:
/* 			version(); */
			break;

		case OPT_VERBOSE:
			opt_verbose = true;
			loadverbose = true;
			linkverbose = true;
			break;

		default:
/* 			usage(); */
			;
		}
	}

	/* initialize the garbage collector */

	gc_init(HEAP_MAXSIZE, HEAP_STARTSIZE);

#if defined(ENABLE_THREADS)
	threads_preinit();
#endif

	/* initialize the string hashtable stuff: lock (must be done
	   _after_ threads_preinit) */

	if (!string_init())
		throw_main_exception_exit();

	/* initialize the utf8 hashtable stuff: lock, often used utf8 strings
	   (must be done _after_ threads_preinit) */

	if (!utf8_init())
		throw_main_exception_exit();

	/* initialize the classcache hashtable stuff: lock, hashtable
	   (must be done _after_ threads_preinit) */

	if (!classcache_init())
		throw_main_exception_exit();

	/* initialize the loader with bootclasspath (must be done _after_
	   thread_preinit) */

	if (!suck_init())
		throw_main_exception_exit();

	suck_add(bootclasspath);

	/* initialize the loader subsystems (must be done _after_
       classcache_init) */

	if (!loader_init())
		throw_main_exception_exit();


	/*********************** Load JAVA classes  **************************/

	nativemethod_chain = chain_new();
	ident_chain = chain_new();

	/* load all classes from bootclasspath */

	loader_load_all_classes();

	/* link all classes */

	for (slot = 0; slot < hashtable_classcache.size; slot++) {
		nmen = (classcache_name_entry *) hashtable_classcache.ptr[slot];

		for (; nmen; nmen = nmen->hashlink) {
			/* iterate over all class entries */

			for (clsen = nmen->classes; clsen; clsen = clsen->next) {
				c = clsen->classobj;

				if (c == NULL)
					continue;

				/* find overloaded methods */

				for (i = 0; i < c->methodscount; i++) {
					m = &(c->methods[i]);

					if (!(m->flags & ACC_NATIVE))
						continue;

					if (!(m->flags & ACC_NATIVELY_OVERLOADED)) {
						nativelyoverloaded = false;
				
						for (j = i + 1; j < c->methodscount; j++) {
							m2 = &(c->methods[j]);

							if (!(m2->flags & ACC_NATIVE))
								continue;

							if (m->name == m2->name) {
								m2->flags          |= ACC_NATIVELY_OVERLOADED;
								nativelyoverloaded  = true;
							}
						}

						if (nativelyoverloaded == true)
							m->flags |= ACC_NATIVELY_OVERLOADED;
					}
				}

				for (j = 0; j < c->methodscount; j++) {
					m = &(c->methods[j]);

					if (m->flags & ACC_NATIVE) {
						chain_addlast(nativemethod_chain, m);
					}
				}
			}
		}
	}

	/* create table of native-methods */

	file = stdout;

	fprintf(file, "/* This file is machine generated, don't edit it! */\n\n"); 

	m = chain_first(nativemethod_chain);

	while (m) {
		printmethod(m);
		m = chain_next(nativemethod_chain);
	}

	fprintf(file, "static nativeref nativetable[] = {\n");

	m = chain_first(nativemethod_chain);

	while (m) {
        fprintf(file, "   { \"");

		print_classname(m->class);
		fprintf(file, "\",\n     \"");
		utf_fprint_printable_ascii(file, m->name);
		fprintf(file, "\",\n     \"");
		utf_fprint_printable_ascii(file, m->descriptor);
		fprintf(file, "\",\n     ");

		if (m->flags & ACC_STATIC)
			fprintf(file, "true");
		else
			fprintf(file, "false");

		fprintf(file, ",\n     ");
		fprintf(file, "(functionptr) Java_");
		printID(m->class->name);
		fprintf(file, "_");
		printID(m->name);
	 
		if (m->flags & ACC_NATIVELY_OVERLOADED)
			printOverloadPart(m->descriptor);

		fprintf(file,"\n   },\n");

		m = chain_next(nativemethod_chain);
	}

	chain_free(nativemethod_chain);

	fprintf(file, "};\n");

	fclose(file);
	
	/* release all resources */

	loader_close();

	/* everything is ok */

	return 0;
}


/* gennativetable_options_prepare **********************************************

   Prepare the JavaVMInitArgs.

*******************************************************************************/

static JavaVMInitArgs *gennativetable_options_prepare(int argc, char **argv)
{
	JavaVMInitArgs *vm_args;
	s4              i;

	vm_args = NEW(JavaVMInitArgs);

	vm_args->nOptions = argc - 1;
	vm_args->options  = MNEW(JavaVMOption, argc);

	for (i = 1; i < argc; i++)
		vm_args->options[i - 1].optionString = argv[i];

	return vm_args;
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
