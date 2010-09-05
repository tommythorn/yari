/* src/native/vm/gnu/java_lang_VMRuntime.c

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

   $Id: java_lang_VMRuntime.c 7718 2007-04-16 15:26:01Z twisti $

*/


#include "config.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>

#if defined(__DARWIN__)
# define OS_INLINE    /* required for <libkern/ppc/OSByteOrder.h> */
# include <mach/mach.h>
#endif

#include "vm/types.h"

#include "mm/gc-common.h"
#include "mm/memory.h"

#include "native/jni.h"

#include "native/include/java_io_File.h"
#include "native/include/java_lang_ClassLoader.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Process.h"

#include "native/vm/java_lang_Runtime.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"


/* this should work on BSD */
/*
#if defined(__DARWIN__)
#include <sys/sysctl.h>
#endif
*/


/*
 * Class:     java/lang/VMRuntime
 * Method:    exit
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_exit(JNIEnv *env, jclass clazz, s4 status)
{
	_Jv_java_lang_Runtime_exit(status);
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    freeMemory
 * Signature: ()J
 */
JNIEXPORT s8 JNICALL Java_java_lang_VMRuntime_freeMemory(JNIEnv *env, jclass clazz)
{
	return _Jv_java_lang_Runtime_freeMemory();
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    totalMemory
 * Signature: ()J
 */
JNIEXPORT s8 JNICALL Java_java_lang_VMRuntime_totalMemory(JNIEnv *env, jclass clazz)
{
	return _Jv_java_lang_Runtime_totalMemory();
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    maxMemory
 * Signature: ()J
 */
JNIEXPORT s8 JNICALL Java_java_lang_VMRuntime_maxMemory(JNIEnv *env, jclass clazz)
{
	return gc_get_max_heap_size();
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    gc
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_gc(JNIEnv *env, jclass clazz)
{
	_Jv_java_lang_Runtime_gc();
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    runFinalization
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_runFinalization(JNIEnv *env, jclass clazz)
{
	gc_invoke_finalizers();
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    runFinalizersOnExit
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_runFinalizersOnExit(JNIEnv *env, jclass clazz, s4 value)
{
	_Jv_java_lang_Runtime_runFinalizersOnExit(value);
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    runFinalizationsForExit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_runFinalizationForExit(JNIEnv *env, jclass clazz)
{
/*  	if (finalizeOnExit) { */
/*  		gc_call(); */
	/* gc_finalize_all(); */
/*  	} */
/*  	log_text("Java_java_lang_VMRuntime_runFinalizationForExit called"); */
	/*gc_finalize_all();*/
	/*gc_invoke_finalizers();*/
	/*gc_call();*/
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    traceInstructions
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_traceInstructions(JNIEnv *env, jclass clazz, s4 par1)
{
	/* not supported */
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    traceMethodCalls
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMRuntime_traceMethodCalls(JNIEnv *env, jclass clazz, s4 par1)
{
	/* not supported */
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    availableProcessors
 * Signature: ()I
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMRuntime_availableProcessors(JNIEnv *env, jclass clazz)
{
#if defined(_SC_NPROC_ONLN)
	return (s4) sysconf(_SC_NPROC_ONLN);

#elif defined(_SC_NPROCESSORS_ONLN)
	return (s4) sysconf(_SC_NPROCESSORS_ONLN);

#elif defined(__DARWIN__)
	/* this should work in BSD */
	/*
	int ncpu, mib[2], rc;
	size_t len;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(ncpu);
	rc = sysctl(mib, 2, &ncpu, &len, NULL, 0);

	return (s4) ncpu;
	*/

	host_basic_info_data_t hinfo;
	mach_msg_type_number_t hinfo_count = HOST_BASIC_INFO_COUNT;
	kern_return_t rc;

	rc = host_info(mach_host_self(), HOST_BASIC_INFO,
				   (host_info_t) &hinfo, &hinfo_count);
 
	if (rc != KERN_SUCCESS) {
		return -1;
	}

    return (s4) hinfo.avail_cpus;

#else
	return 1;
#endif
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    nativeLoad
 * Signature: (Ljava/lang/String;Ljava/lang/ClassLoader;)I
 */
JNIEXPORT s4 JNICALL Java_java_lang_VMRuntime_nativeLoad(JNIEnv *env, jclass clazz, java_lang_String *libname, java_lang_ClassLoader *loader)
{
	java_objectheader *cl;

	cl = (java_objectheader *) loader;

#if defined(ENABLE_JNI)
	return _Jv_java_lang_Runtime_loadLibrary(env, libname, cl);
#else
	return _Jv_java_lang_Runtime_loadLibrary(libname, cl);
#endif
}


/*
 * Class:     java/lang/VMRuntime
 * Method:    mapLibraryName
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT java_lang_String* JNICALL Java_java_lang_VMRuntime_mapLibraryName(JNIEnv *env, jclass clazz, java_lang_String *libname)
{
	utf               *u;
	char              *buffer;
	s4                 buffer_len;
	s4                 dumpsize;
	java_objectheader *o;

	if (libname == NULL) {
		exceptions_throw_nullpointerexception();
		return NULL;
	}

	u = javastring_toutf((java_objectheader *) libname, false);

	/* calculate length of library name */

	buffer_len = strlen("lib");

	buffer_len += utf_bytes(u);

#if defined(__DARWIN__)
	buffer_len += strlen(".dylib");
#else
	buffer_len += strlen(".so");
#endif

	buffer_len += strlen("0");

	dumpsize = dump_size();
	buffer = DMNEW(char, buffer_len);

	/* generate library name */

	strcpy(buffer, "lib");
	utf_cat(buffer, u);

#if defined(__DARWIN__)
	strcat(buffer, ".dylib");
#else
	strcat(buffer, ".so");
#endif

	o = javastring_new_from_utf_string(buffer);

	/* release memory */

	dump_release(dumpsize);

	return (java_lang_String *) o;
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
