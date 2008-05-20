/* src/cacao/cacao.c - contains main() of cacao

   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, J. Wenninger, Institut f. Computersprachen - TU Wien

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

   $Id: cacao.c 7698M 2007-12-20 15:22:17Z (local) $

*/


#include "config.h"

#include <assert.h>

#if defined(ENABLE_LIBJVM)
# include <ltdl.h>
#endif

#if defined(WITH_JRE_LAYOUT)
# include <errno.h>
# include <libgen.h>
# include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "native/jni.h"

#if defined(ENABLE_JVMTI)
# include "native/jvmti/jvmti.h"
# include "native/jvmti/cacaodbg.h"
#endif

#include "vm/vm.h"


/* forward declarations *******************************************************/

static JavaVMInitArgs *cacao_options_prepare(int argc, char **argv);


/* main ************************************************************************

   The main program.
   
*******************************************************************************/

int main(int argc, char **argv)
{
#if defined(ENABLE_LIBJVM) && !defined(WITH_STATIC_CLASSPATH)
	char           *path;
#endif

#if defined(ENABLE_LIBJVM)	
	/* Variables for JNI_CreateJavaVM dlopen call. */
	lt_dlhandle     libjvm_handle;
	lt_ptr          libjvm_vm_createjvm;
	lt_ptr          libjvm_vm_run;
	const char     *lterror;

	bool (*vm_createjvm)(JavaVM **, void **, void *);
	void (*vm_run)(JavaVM *, JavaVMInitArgs *);
#endif

	JavaVM         *vm;                 /* denotes a Java VM                  */
	JNIEnv         *env;
	JavaVMInitArgs *vm_args;

	/* prepare the options */

	/* vm_args = cacao_options_prepare(argc, argv); */
/*	char *tmp[] = {"cacao", "hello"};
	vm_args = cacao_options_prepare(2, &tmp); 
	char *tmp[] = {"cacao", "jbe.DoKernel"};
	char *tmp[] = {"cacao", "jbe.DoMicro"};
	char *tmp[] = {"cacao", "jbe.BenchKfl"};
	char *tmp[] = {"cacao", "jbe.BenchLift"};
	char *tmp[] = {"cacao", "jbe.BenchUdpIp"};
*/
	char *tmp[] = {"cacao", "jbe.BenchUdpIp"};
	vm_args = cacao_options_prepare(2, &tmp); 
	
	
	/* load and initialize a Java VM, return a JNI interface pointer in env */

#if defined(ENABLE_LIBJVM) && !defined(WITH_STATIC_CLASSPATH)
# if defined(WITH_JRE_LAYOUT)
	/* SUN also uses a buffer of 4096-bytes (strace is your friend). */

	path = malloc(sizeof(char) * 4096);

	if (readlink("/proc/self/exe", path, 4095) == -1) {
		fprintf(stderr, "main: readlink failed: %s\n", strerror(errno));
		abort();
	}

	/* get the path of the current executable */

	path = dirname(path);

	if ((strlen(path) + strlen("/../lib/libjvm") + strlen("0")) > 4096) {
		fprintf(stderr, "main: libjvm name to long for buffer\n");
		abort();
	}

	/* concatinate the library name */

	strcat(path, "/../lib/libjvm");
# else
	path = CACAO_LIBDIR"/libjvm";
# endif

	if (lt_dlinit()) {
		fprintf(stderr, "main: lt_dlinit failed: %s\n", lt_dlerror());
		abort();
	}

	/* First try to open where dlopen searches, e.g. LD_LIBRARY_PATH.
	   If not found, try the absolute path. */

	if (!(libjvm_handle = lt_dlopenext("libjvm"))) {
		/* save the error message */

		lterror = strdup(lt_dlerror());

		if (!(libjvm_handle = lt_dlopenext(path))) {
			/* print the first error message too */

			fprintf(stderr, "main: lt_dlopenext failed: %s\n", lterror);

			/* and now the current one */

			fprintf(stderr, "main: lt_dlopenext failed: %s\n", lt_dlerror());
			abort();
		}

		/* free the error string */

		free((void *) lterror);
	}

	if (!(libjvm_vm_createjvm = lt_dlsym(libjvm_handle, "vm_createjvm"))) {
		fprintf(stderr, "main: lt_dlsym failed: %s\n", lt_dlerror());
		abort();
	}

	vm_createjvm =
		(bool (*)(JavaVM **, void **, void *)) (ptrint) libjvm_vm_createjvm;
#endif

	/* create the Java VM */

	(void) vm_createjvm(&vm, (void *) &env, vm_args);

#if defined(ENABLE_JVMTI)
	pthread_mutex_init(&dbgcomlock,NULL);
	if (jvmti) jvmti_set_phase(JVMTI_PHASE_START);
#endif

#if !defined(WITH_STATIC_CLASSPATH) && defined(ENABLE_LIBJVM)
	if (!(libjvm_vm_run = lt_dlsym(libjvm_handle, "vm_run"))) {
		fprintf(stderr, "lt_dlsym failed: %s\n", lt_dlerror());
		abort();
	}

	vm_run = (void (*)(JavaVM *, JavaVMInitArgs *)) (ptrint) libjvm_vm_run;
#endif

	/* run the VM */

	vm_run(vm, vm_args);

	/* keep compiler happy */

	return 0;
}


/* cacao_options_prepare *******************************************************

   Prepare the JavaVMInitArgs.

*******************************************************************************/

static JavaVMInitArgs *cacao_options_prepare(int argc, char **argv)
{
	JavaVMInitArgs *vm_args;
	s4              i;

	vm_args = malloc(sizeof(JavaVMInitArgs));

	vm_args->version            = JNI_VERSION_1_2;
	vm_args->nOptions           = argc - 1;
	vm_args->options            = malloc(sizeof(JavaVMOption) * argc);
	vm_args->ignoreUnrecognized = JNI_FALSE;

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
