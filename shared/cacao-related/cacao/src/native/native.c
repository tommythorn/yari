/* src/native/native.c - table of native functions

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

   $Id: native.c 7813M 2007-05-07 18:02:06Z (local) $

*/


#include "config.h"

#include <assert.h>
#include <ctype.h>

#if !defined(WITH_STATIC_CLASSPATH) && !defined(ENABLE_EMBEDDED_CLASSES)
# include <ltdl.h>
#endif

#include "vm/types.h"

#include "mm/memory.h"
#include "native/jni.h"
#include "native/native.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Throwable.h"

#include "threads/lock-common.h"

#include "toolbox/hashtable.h"
#include "toolbox/logging.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/jit.h"

#include "vmcore/loader.h"
#include "vmcore/options.h"
#include "vm/resolve.h"

#if defined(ENABLE_JVMTI)
#include "native/jvmti/cacaodbg.h"
#endif


/* include table of native functions ******************************************/

#include "native/include/java_lang_Object.h"

#if defined(ENABLE_JAVASE)

#include "native/include/java_io_InputStream.h"
#include "native/include/java_io_PrintStream.h"

#include "native/include/java_lang_Cloneable.h"
#include "native/include/java_util_Properties.h"

#include "native/include/gnu_classpath_VMStackWalker.h"
#include "native/include/gnu_classpath_VMSystemProperties.h"
#include "native/include/gnu_java_lang_management_VMClassLoadingMXBeanImpl.h"
#include "native/include/gnu_java_lang_management_VMMemoryMXBeanImpl.h"
#include "native/include/gnu_java_lang_management_VMRuntimeMXBeanImpl.h"
#include "native/include/java_lang_VMClass.h"
#include "native/include/java_security_ProtectionDomain.h"  /* required by... */
#include "native/include/java_lang_VMClassLoader.h"
#include "native/include/java_lang_VMObject.h"
#include "native/include/java_lang_VMRuntime.h"
#include "native/include/java_lang_VMString.h"
#include "native/include/java_lang_VMSystem.h"
#include "native/include/java_lang_VMThread.h"
#include "native/include/java_lang_VMThrowable.h"
#include "native/include/java_lang_management_VMManagementFactory.h"
#include "native/include/java_lang_reflect_Constructor.h"
#include "native/include/java_lang_reflect_Field.h"
#include "native/include/java_lang_reflect_Method.h"
#include "native/include/java_lang_reflect_VMProxy.h"
#include "native/include/java_security_VMAccessController.h"
#include "native/include/sun_misc_Unsafe.h"

#if defined(ENABLE_JVMTI)
#include "native/include/gnu_classpath_jdwp_event_EventRequest.h"
#include "native/include/java_nio_ByteBuffer.h"
#include "native/include/gnu_classpath_jdwp_VMVirtualMachine.h"
#include "native/include/gnu_classpath_jdwp_VMFrame.h"
#include "native/include/gnu_classpath_jdwp_VMMethod.h"
#endif

#elif defined(ENABLE_JAVAME_CLDC1_1)

#include "native/include/com_sun_cldchi_io_ConsoleOutputStream.h"
#include "native/include/com_sun_cldc_io_ResourceInputStream.h"
#include "native/include/com_sun_cldc_io_j2me_socket_Protocol.h"
#include "native/include/java_lang_Class.h"
#include "native/include/java_lang_Double.h"
#include "native/include/java_lang_Float.h"
#include "native/include/java_lang_Math.h"
#include "native/include/java_lang_Runtime.h"
#include "native/include/java_lang_System.h"
#include "native/include/java_lang_Thread.h"

#endif

#if defined(WITH_STATIC_CLASSPATH)

/* these are required to prevent compiler warnings */

#include "native/include/java_net_DatagramPacket.h"
#include "native/include/java_net_InetAddress.h"
#include "native/include/java_net_SocketImpl.h"

#include "native/include/gnu_java_net_PlainDatagramSocketImpl.h"
#include "native/include/gnu_java_net_PlainSocketImpl.h"
#include "native/include/gnu_java_nio_PipeImpl.h"
#include "native/include/gnu_java_nio_channels_FileChannelImpl.h"
#include "native/include/gnu_java_nio_charset_iconv_IconvEncoder.h"
#include "native/include/gnu_java_nio_charset_iconv_IconvDecoder.h"
#include "native/include/java_lang_VMProcess.h"
#include "native/include/java_nio_MappedByteBufferImpl.h"
#include "native/include/java_nio_channels_spi_SelectorProvider.h"

/* now include the native table */

#include "native/nativetable.inc"

#elif !defined(ENABLE_LIBJVM)

/* dummynativetable ************************************************************

   Ensure that symbols for functions implemented within CACAO are used
   and exported to dlopen.

   ATTENTION: Don't make this table static!!!  Otherwise the compiler
   may optimize it away!

*******************************************************************************/

functionptr dummynativetable[] = {
#if defined(ENABLE_JAVASE)
	(functionptr) Java_gnu_classpath_VMStackWalker_getClassContext,
	(functionptr) Java_gnu_classpath_VMStackWalker_getCallingClass,
	(functionptr) Java_gnu_classpath_VMStackWalker_getCallingClassLoader,
	(functionptr) Java_gnu_classpath_VMStackWalker_firstNonNullClassLoader,

	(functionptr) Java_gnu_classpath_VMSystemProperties_preInit,

	(functionptr) Java_gnu_java_lang_management_VMClassLoadingMXBeanImpl_getLoadedClassCount,
	(functionptr) Java_gnu_java_lang_management_VMClassLoadingMXBeanImpl_getUnloadedClassCount,
	(functionptr) Java_gnu_java_lang_management_VMClassLoadingMXBeanImpl_isVerbose,
	(functionptr) Java_gnu_java_lang_management_VMClassLoadingMXBeanImpl_setVerbose,

	(functionptr) Java_gnu_java_lang_management_VMMemoryMXBeanImpl_getHeapMemoryUsage,
	(functionptr) Java_gnu_java_lang_management_VMMemoryMXBeanImpl_getNonHeapMemoryUsage,
	(functionptr) Java_gnu_java_lang_management_VMMemoryMXBeanImpl_getObjectPendingFinalizationCount,
	(functionptr) Java_gnu_java_lang_management_VMMemoryMXBeanImpl_isVerbose,
	(functionptr) Java_gnu_java_lang_management_VMMemoryMXBeanImpl_setVerbose,

	(functionptr) Java_gnu_java_lang_management_VMRuntimeMXBeanImpl_getInputArguments,
	(functionptr) Java_gnu_java_lang_management_VMRuntimeMXBeanImpl_getStartTime,

	(functionptr) Java_java_lang_VMClass_isInstance,
	(functionptr) Java_java_lang_VMClass_isAssignableFrom,
	(functionptr) Java_java_lang_VMClass_isInterface,
	(functionptr) Java_java_lang_VMClass_isPrimitive,
	(functionptr) Java_java_lang_VMClass_getName,
	(functionptr) Java_java_lang_VMClass_getSuperclass,
	(functionptr) Java_java_lang_VMClass_getInterfaces,
	(functionptr) Java_java_lang_VMClass_getComponentType,
	(functionptr) Java_java_lang_VMClass_getModifiers,
	(functionptr) Java_java_lang_VMClass_getDeclaringClass,
	(functionptr) Java_java_lang_VMClass_getDeclaredClasses,
	(functionptr) Java_java_lang_VMClass_getDeclaredFields,
	(functionptr) Java_java_lang_VMClass_getDeclaredMethods,
	(functionptr) Java_java_lang_VMClass_getDeclaredConstructors,
	(functionptr) Java_java_lang_VMClass_getClassLoader,
	(functionptr) Java_java_lang_VMClass_forName,
	(functionptr) Java_java_lang_VMClass_isArray,
	(functionptr) Java_java_lang_VMClass_throwException,

	(functionptr) Java_java_lang_VMClassLoader_defineClass,
	(functionptr) Java_java_lang_VMClassLoader_resolveClass,
	(functionptr) Java_java_lang_VMClassLoader_loadClass,
	(functionptr) Java_java_lang_VMClassLoader_getPrimitiveClass,
	(functionptr) Java_java_lang_VMClassLoader_nativeGetResources,
	(functionptr) Java_java_lang_VMClassLoader_findLoadedClass,

	(functionptr) Java_java_lang_VMObject_getClass,
	(functionptr) Java_java_lang_VMObject_clone,
	(functionptr) Java_java_lang_VMObject_notify,
	(functionptr) Java_java_lang_VMObject_notifyAll,
	(functionptr) Java_java_lang_VMObject_wait,

	(functionptr) Java_java_lang_VMRuntime_availableProcessors,
	(functionptr) Java_java_lang_VMRuntime_freeMemory,
	(functionptr) Java_java_lang_VMRuntime_totalMemory,
	(functionptr) Java_java_lang_VMRuntime_maxMemory,
	(functionptr) Java_java_lang_VMRuntime_gc,
	(functionptr) Java_java_lang_VMRuntime_runFinalization,
	(functionptr) Java_java_lang_VMRuntime_runFinalizationForExit,
	(functionptr) Java_java_lang_VMRuntime_traceInstructions,
	(functionptr) Java_java_lang_VMRuntime_traceMethodCalls,
	(functionptr) Java_java_lang_VMRuntime_runFinalizersOnExit,
	(functionptr) Java_java_lang_VMRuntime_exit,
	(functionptr) Java_java_lang_VMRuntime_nativeLoad,
	(functionptr) Java_java_lang_VMRuntime_mapLibraryName,

	(functionptr) Java_java_lang_VMString_intern,

	(functionptr) Java_java_lang_VMSystem_arraycopy,
	(functionptr) Java_java_lang_VMSystem_identityHashCode,

	(functionptr) Java_java_lang_VMThread_start,
	(functionptr) Java_java_lang_VMThread_interrupt,
	(functionptr) Java_java_lang_VMThread_isInterrupted,
	(functionptr) Java_java_lang_VMThread_suspend,
	(functionptr) Java_java_lang_VMThread_resume,
	(functionptr) Java_java_lang_VMThread_nativeSetPriority,
	(functionptr) Java_java_lang_VMThread_nativeStop,
	(functionptr) Java_java_lang_VMThread_currentThread,
	(functionptr) Java_java_lang_VMThread_yield,
	(functionptr) Java_java_lang_VMThread_interrupted,
	(functionptr) Java_java_lang_VMThread_holdsLock,
	(functionptr) Java_java_lang_VMThread_getState,

	(functionptr) Java_java_lang_VMThrowable_fillInStackTrace,
	(functionptr) Java_java_lang_VMThrowable_getStackTrace,

	(functionptr) Java_java_lang_management_VMManagementFactory_getMemoryPoolNames,
	(functionptr) Java_java_lang_management_VMManagementFactory_getMemoryManagerNames,
	(functionptr) Java_java_lang_management_VMManagementFactory_getGarbageCollectorNames,

	(functionptr) Java_java_lang_reflect_Constructor_getModifiersInternal,
	(functionptr) Java_java_lang_reflect_Constructor_constructNative,

	(functionptr) Java_java_lang_reflect_Field_getModifiersInternal,
	(functionptr) Java_java_lang_reflect_Field_getType,
	(functionptr) Java_java_lang_reflect_Field_get,
	(functionptr) Java_java_lang_reflect_Field_getBoolean,
	(functionptr) Java_java_lang_reflect_Field_getByte,
	(functionptr) Java_java_lang_reflect_Field_getChar,
	(functionptr) Java_java_lang_reflect_Field_getShort,
	(functionptr) Java_java_lang_reflect_Field_getInt,
	(functionptr) Java_java_lang_reflect_Field_getLong,
	(functionptr) Java_java_lang_reflect_Field_getFloat,
	(functionptr) Java_java_lang_reflect_Field_getDouble,
	(functionptr) Java_java_lang_reflect_Field_set,
	(functionptr) Java_java_lang_reflect_Field_setBoolean,
	(functionptr) Java_java_lang_reflect_Field_setByte,
	(functionptr) Java_java_lang_reflect_Field_setChar,
	(functionptr) Java_java_lang_reflect_Field_setShort,
	(functionptr) Java_java_lang_reflect_Field_setInt,
	(functionptr) Java_java_lang_reflect_Field_setLong,
	(functionptr) Java_java_lang_reflect_Field_setFloat,
	(functionptr) Java_java_lang_reflect_Field_setDouble,

	(functionptr) Java_java_lang_reflect_Method_getModifiersInternal,
	(functionptr) Java_java_lang_reflect_Method_getReturnType,
	(functionptr) Java_java_lang_reflect_Method_getParameterTypes,
	(functionptr) Java_java_lang_reflect_Method_getExceptionTypes,
	(functionptr) Java_java_lang_reflect_Method_invokeNative,

	(functionptr) Java_java_lang_reflect_VMProxy_getProxyClass,
	(functionptr) Java_java_lang_reflect_VMProxy_getProxyData,
	(functionptr) Java_java_lang_reflect_VMProxy_generateProxyClass,

	(functionptr) Java_java_security_VMAccessController_getStack,

	(functionptr) Java_sun_misc_Unsafe_objectFieldOffset,
	(functionptr) Java_sun_misc_Unsafe_compareAndSwapInt,

#if defined(ENABLE_JVMTI)
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_suspendThread,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_resumeThread,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getSuspendCount,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getAllLoadedClassesCount,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getClassStatus,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getAllClassMethods,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getClassMethod,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getFrames,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getFrame,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getFrameCount,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getThreadStatus,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getLoadRequests,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_executeMethod,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getSourceFile,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_registerEvent,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_unregisterEvent,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_clearEvents,
	(functionptr) Java_gnu_classpath_jdwp_VMVirtualMachine_getAllLoadedClasses,
	(functionptr) Java_gnu_classpath_jdwp_VMFrame_setValue,
	(functionptr) Java_gnu_classpath_jdwp_VMFrame_getValue,
	(functionptr) Java_gnu_classpath_jdwp_VMMethod_getName,
	(functionptr) Java_gnu_classpath_jdwp_VMMethod_getSignature,
	(functionptr) Java_gnu_classpath_jdwp_VMMethod_getModifiers,
	(functionptr) Java_gnu_classpath_jdwp_VMMethod_getLineTable,
	(functionptr) Java_gnu_classpath_jdwp_VMMethod_getVariableTable
#endif

#elif defined(ENABLE_JAVAME_CLDC1_1)
	(functionptr) Java_com_sun_cldc_io_ResourceInputStream_open,

/* 	(functionptr) Java_com_sun_cldc_io_j2me_socket_Protocol_open0,
 	(functionptr) Java_com_sun_cldc_io_j2me_socket_Protocol_readBuf,
 	(functionptr) Java_com_sun_cldc_io_j2me_socket_Protocol_writeByte, */

	(functionptr) Java_com_sun_cldchi_io_ConsoleOutputStream_write,

	(functionptr) Java_java_lang_Class_forName,
	(functionptr) Java_java_lang_Class_newInstance,
	(functionptr) Java_java_lang_Class_isInstance,
	(functionptr) Java_java_lang_Class_isAssignableFrom,
	(functionptr) Java_java_lang_Class_isInterface,
	(functionptr) Java_java_lang_Class_isArray,
	(functionptr) Java_java_lang_Class_getName,

	(functionptr) Java_java_lang_Double_doubleToLongBits,

	(functionptr) Java_java_lang_Float_floatToIntBits,

	(functionptr) Java_java_lang_Math_ceil,
	(functionptr) Java_java_lang_Math_cos,
	(functionptr) Java_java_lang_Math_floor,
	(functionptr) Java_java_lang_Math_sin,
	(functionptr) Java_java_lang_Math_sqrt,
	(functionptr) Java_java_lang_Math_tan,

	(functionptr) Java_java_lang_Object_hashCode,
	(functionptr) Java_java_lang_Object_notify,
	(functionptr) Java_java_lang_Object_wait,

	(functionptr) Java_java_lang_Runtime_exitInternal,
	(functionptr) Java_java_lang_Runtime_freeMemory,
	(functionptr) Java_java_lang_Runtime_totalMemory,
	(functionptr) Java_java_lang_Runtime_gc,

	(functionptr) Java_java_lang_String_hashCode,
	(functionptr) Java_java_lang_String_indexOf__I,
	(functionptr) Java_java_lang_String_indexOf__II,
	(functionptr) Java_java_lang_String_lastIndexOf__II,
	(functionptr) Java_java_lang_String_intern,

	(functionptr) Java_java_lang_System_getProperty0,

	(functionptr) Java_java_lang_Thread_currentThread,
	(functionptr) Java_java_lang_Thread_setPriority0,
	(functionptr) Java_java_lang_Thread_start0,
	(functionptr) Java_java_lang_Thread_yield,

	(functionptr) Java_java_lang_Throwable_printStackTrace,
	(functionptr) Java_java_lang_Throwable_fillInStackTrace
#endif
};

#endif /* defined(ENABLE_LIBJVM) */


/* tables for methods *********************************************************/

#if defined(WITH_STATIC_CLASSPATH)
#define NATIVETABLESIZE  (sizeof(nativetable)/sizeof(struct nativeref))

/* table for fast string comparison */
static nativecompref nativecomptable[NATIVETABLESIZE];

/* string comparsion table initialized */
static bool nativecompdone = false;
#endif


/* global variables ***********************************************************/

#if !defined(WITH_STATIC_CLASSPATH) && !defined(ENABLE_EMBEDDED_CLASSES)
static hashtable *hashtable_library;
static lt_dlhandle mainhandle;
#endif


/* native_init *****************************************************************

   Initializes the native subsystem.

*******************************************************************************/

bool native_init(void)
{
#if !defined(WITH_STATIC_CLASSPATH)
#if !defined(ENABLE_EMBEDDED_CLASSES)
	/* initialize libltdl */

	if (lt_dlinit())
		vm_abort("native_init: lt_dlinit failed: %s\n", lt_dlerror());

	/* Get the handle for the main program or for the libjvm.so,
	   depends on the configuration. */

# if defined(ENABLE_LIBJVM)
	/* First try to open where dlopen searches, e.g. LD_LIBRARY_PATH.
	   If not found, try the absolute path. */

	if (!(mainhandle = lt_dlopenext("libjvm")))
		if (!(mainhandle = lt_dlopenext(cacao_libjvm)))
			vm_abort("native_init: lt_dlopenext failed: %s\n", lt_dlerror());
# else
	if (!(mainhandle = lt_dlopen(NULL)))
		vm_abort("native_init: lt_dlopen failed: %s\n", lt_dlerror());
# endif


	/* initialize library hashtable, 10 entries should be enough */

	hashtable_library = NEW(hashtable);

	hashtable_create(hashtable_library, 10);
#endif /* !ENABLE_EMBEDDED_CLASSES */
#endif

	/* everything's ok */

	return true;
}


/* native_hashtable_library_add ************************************************

   Adds an entry to the native library hashtable.

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH) && !defined(ENABLE_EMBEDDED_CLASSES)
void native_hashtable_library_add(utf *filename, java_objectheader *loader,
								  lt_dlhandle handle)
{
	hashtable_library_loader_entry *le;
	hashtable_library_name_entry   *ne; /* library name                       */
	u4   key;                           /* hashkey                            */
	u4   slot;                          /* slot in hashtable                  */

	LOCK_MONITOR_ENTER(hashtable_library->header);

	/* normally addresses are aligned to 4, 8 or 16 bytes */

	key  = ((u4) (ptrint) loader) >> 4;        /* align to 16-byte boundaries */
	slot = key & (hashtable_library->size - 1);
	le   = hashtable_library->ptr[slot];

	/* search external hash chain for the entry */

	while (le) {
		if (le->loader == loader)
			break;

		le = le->hashlink;                  /* next element in external chain */
	}

	/* no loader found? create a new entry */

	if (le == NULL) {
		le = NEW(hashtable_library_loader_entry);

		le->loader   = loader;
		le->namelink = NULL;

		/* insert entry into hashtable */

		le->hashlink =
			(hashtable_library_loader_entry *) hashtable_library->ptr[slot];
		hashtable_library->ptr[slot] = le;

		/* update number of hashtable-entries */

		hashtable_library->entries++;
	}


	/* search for library name */

	ne = le->namelink;

	while (ne) {
		if (ne->name == filename) {
			LOCK_MONITOR_EXIT(hashtable_library->header);

			return;
		}

		ne = ne->hashlink;                  /* next element in external chain */
	}

	/* not found? add the library name to the classloader */

	ne = NEW(hashtable_library_name_entry);

	ne->name   = filename;
	ne->handle = handle;

	/* insert entry into external chain */

	ne->hashlink = le->namelink;
	le->namelink = ne;

	LOCK_MONITOR_EXIT(hashtable_library->header);
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


/* native_hashtable_library_find ***********************************************

   Find an entry in the native library hashtable.

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH) && !defined(ENABLE_EMBEDDED_CLASSES)
hashtable_library_name_entry *native_hashtable_library_find(utf *filename,
															java_objectheader *loader)
{
	hashtable_library_loader_entry *le;
	hashtable_library_name_entry   *ne; /* library name                       */
	u4   key;                           /* hashkey                            */
	u4   slot;                          /* slot in hashtable                  */

	/* normally addresses are aligned to 4, 8 or 16 bytes */

	key  = ((u4) (ptrint) loader) >> 4;        /* align to 16-byte boundaries */
	slot = key & (hashtable_library->size - 1);
	le   = hashtable_library->ptr[slot];

	/* search external hash chain for the entry */

	while (le) {
		if (le->loader == loader)
			break;

		le = le->hashlink;                  /* next element in external chain */
	}

	/* no loader found? return NULL */

	if (!le)
		return NULL;

	/* search for library name */

	ne = le->namelink;

	while (ne) {
		if (ne->name == filename)
			return ne;

		ne = ne->hashlink;                  /* next element in external chain */
	}

	/* return entry, if no entry was found, ne is NULL */

	return ne;
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


/* native_findfunction *********************************************************

   Looks up a method (must have the same class name, method name,
   descriptor and 'static'ness) and returns a function pointer to it.
   Returns: function pointer or NULL (if there is no such method)

   Remark: For faster operation, the names/descriptors are converted
   from C strings to Unicode the first time this function is called.

*******************************************************************************/

#if defined(WITH_STATIC_CLASSPATH)
functionptr native_findfunction(utf *cname, utf *mname, utf *desc,
								bool isstatic)
{
	/* entry of table for fast string comparison */
	struct nativecompref *n;
	s4 i;

	isstatic = isstatic ? true : false;
	
	if (!nativecompdone) {
		for (i = 0; i < NATIVETABLESIZE; i++) {
			nativecomptable[i].classname  = 
				utf_new_char(nativetable[i].classname);

			nativecomptable[i].methodname = 
				utf_new_char(nativetable[i].methodname);

			nativecomptable[i].descriptor =
				utf_new_char(nativetable[i].descriptor);

			nativecomptable[i].isstatic   = nativetable[i].isstatic;
			nativecomptable[i].func       = nativetable[i].func;
		}

		nativecompdone = true;
	}

	for (i = 0; i < NATIVETABLESIZE; i++) {
		n = &(nativecomptable[i]);

		if (cname == n->classname && mname == n->methodname &&
		    desc == n->descriptor && isstatic == n->isstatic)
			return n->func;
	}

	/* no function was found, throw exception */

	*exceptionptr =
			new_exception_utfmessage(string_java_lang_UnsatisfiedLinkError,
									 mname);

	return NULL;
}
#endif /* defined(WITH_STATIC_CLASSPATH) */


/* native_make_overloaded_function *********************************************

   XXX

*******************************************************************************/

#if !defined(WITH_STATIC_CLASSPATH)
static char *native_make_overloaded_function(char *name, utf *desc)
{
	char *newname;
	s4    namelen;
	char *utf_ptr;
	u2    c;
	s4    i;

	utf_ptr = desc->text;
	namelen = strlen(name) + strlen("__") + strlen("0");

	/* calculate additional length */

	while ((c = utf_nextu2(&utf_ptr)) != ')') {
		switch (c) {
		case 'Z':
		case 'B':
		case 'C':
		case 'S':
		case 'I':
		case 'J':
		case 'F':
		case 'D':
			namelen++;
			break;
		case '[':
			namelen += 2;
			break;
		case 'L':
			namelen++;
			while (utf_nextu2(&utf_ptr) != ';')
				namelen++;
			namelen += 2;
			break;
		case '(':
			break;
		default:
			assert(0);
		}
	}


	/* reallocate memory */

	i = strlen(name);

	newname = DMNEW(char, namelen);
	MCOPY(newname, name, char, i);

	utf_ptr = desc->text;

	newname[i++] = '_';
	newname[i++] = '_';

	while ((c = utf_nextu2(&utf_ptr)) != ')') {
		switch (c) {
		case 'Z':
		case 'B':
		case 'C':
		case 'S':
		case 'J':
		case 'I':
		case 'F':
		case 'D':
			newname[i++] = c;
			break;
		case '[':
			newname[i++] = '_';
			newname[i++] = '3';
			break;
		case 'L':
			newname[i++] = 'L';
			while ((c = utf_nextu2(&utf_ptr)) != ';')
				if (((c >= 'a') && (c <= 'z')) ||
					((c >= 'A') && (c <= 'Z')) ||
					((c >= '0') && (c <= '9')))
					newname[i++] = c;
				else
					newname[i++] = '_';
			newname[i++] = '_';
			newname[i++] = '2';
			break;
		case '(':
			break;
		default:
			assert(0);
		}
	}

	/* close string */

	newname[i] = '\0';

	return newname;
}


/* native_insert_char **********************************************************

   Inserts the passed UTF character into the native method name.  If
   necessary it is escaped properly.

*******************************************************************************/

static s4 native_insert_char(char *name, u4 pos, u2 c)
{
	s4 val;
	s4 i;

	switch (c) {
	case '/':
	case '.':
		/* replace '/' or '.' with '_' */
		name[pos] = '_';
		break;

	case '_':
		/* escape sequence for '_' is '_1' */
		name[pos]   = '_';
		name[++pos] = '1';
		break;

	case ';':
		/* escape sequence for ';' is '_2' */
		name[pos]   = '_';
		name[++pos] = '2';
		break;

	case '[':
		/* escape sequence for '[' is '_1' */
		name[pos]   = '_';
		name[++pos] = '3';
		break;

	default:
		if (isalnum(c))
			name[pos] = c;
		else {
			/* unicode character */
			name[pos]   = '_';
			name[++pos] = '0';

			for (i = 0; i < 4; ++i) {
				val = c & 0x0f;
				name[pos + 4 - i] = (val > 10) ? ('a' + val - 10) : ('0' + val);
				c >>= 4;
			}

			pos += 4;
		}
		break;
	}

	/* return the new buffer index */

	return pos;
}


/* native_resolve_function *****************************************************

   Resolves a native function, maybe from a dynamic library.

*******************************************************************************/

functionptr native_resolve_function(methodinfo *m)
{
	char                           *name;
	char                           *newname;
	s4                              namelen;
	char                           *utf_ptr;
	char                           *utf_endptr;
	u2                              c;
	u4                              pos;
	s4                              dumpsize;
#if defined(ENABLE_EMBEDDED_CLASSES)
	void * sym;
	extern char* native_names[];
	extern void* native_symbols[];
	extern int num_natives;
	int i;
#else
	lt_ptr                          sym;
	hashtable_library_loader_entry *le;
	hashtable_library_name_entry   *ne;
	u4                              key;    /* hashkey                        */
	u4                              slot;   /* slot in hashtable              */
#endif
	/* verbose output */

	if (opt_verbosejni) {
		printf("[Dynamic-linking native method ");
		utf_display_printable_ascii_classname(m->class->name);
		printf(".");
		utf_display_printable_ascii(m->name);
		printf(" ... ");
	}
		
	/* Calculate length of native function name.  We multiply the
	   class and method name length by 6 as this is the maxium
	   escape-sequence that can be generated (unicode). */

	namelen = strlen("Java_") +
		utf_get_number_of_u2s(m->class->name) * 6 +
		strlen("_") +
		utf_get_number_of_u2s(m->name) * 6 +
		strlen("0");

	/* allocate memory */

	dumpsize = dump_size();

	name = DMNEW(char, namelen);

	/* generate name of native functions */

	strcpy(name, "Java_");
	pos = strlen("Java_");

	utf_ptr    = m->class->name->text;
	utf_endptr = UTF_END(m->class->name);

	for (; utf_ptr < utf_endptr; utf_ptr++, pos++) {
		c   = *utf_ptr;
		pos = native_insert_char(name, pos, c);
	}

	/* seperator between class and method */

	name[pos++] = '_';

	utf_ptr    = m->name->text;
	utf_endptr = UTF_END(m->name);

	for (; utf_ptr < utf_endptr; utf_ptr++, pos++) {
		c   = *utf_ptr;
		pos = native_insert_char(name, pos, c);
	}

	/* close string */

	name[pos] = '\0';

	/* check for an buffer overflow */

	assert(pos <= namelen);

	/* generate overloaded function (having the types in it's name)           */

	newname = native_make_overloaded_function(name, m->descriptor);

#if defined(ENABLE_EMBEDDED_CLASSES)

	sym = NULL;

	for (i = 0; i < num_natives && sym == NULL; ++i) {
		if (strcmp(native_names[i], name) == 0)
			sym = native_symbols[i];
	}
	
#else

	/* check the library hash entries of the classloader of the
	   methods's class  */

	sym = NULL;

	/* normally addresses are aligned to 4, 8 or 16 bytes */

	key  = ((u4) (ptrint) m->class->classloader) >> 4;    /* align to 16-byte */
	slot = key & (hashtable_library->size - 1);
	le   = hashtable_library->ptr[slot];

	/* iterate through loaders in this hash slot */

	while ((le != NULL) && (sym == NULL)) {
		/* iterate through names in this loader */

		ne = le->namelink;
			
		while ((ne != NULL) && (sym == NULL)) {
			sym = lt_dlsym(ne->handle, name);

			if (sym == NULL)
				sym = lt_dlsym(ne->handle, newname);

			ne = ne->hashlink;
		}

		le = le->hashlink;
	}

	if (sym != NULL)
		if (opt_verbosejni)
			printf("JNI ]\n");


	/* If not found, try to find the native function symbol in the
	   main program. */

	if (sym == NULL) {
		sym = lt_dlsym(mainhandle, name);

		if (sym == NULL)
			sym = lt_dlsym(mainhandle, newname);

		if (sym != NULL)
			if (opt_verbosejni)
				printf("internal ]\n");
	}

#endif /* ENABLE_EMBEDDED_CLASSES */

#if defined(ENABLE_JVMTI)
	/* fire Native Method Bind event */
	if (jvmti) jvmti_NativeMethodBind(m, sym, &sym);
#endif

	/* no symbol found? throw exception */

	if (sym == NULL) {
		if (opt_verbosejni)
			printf("failed ]\n");

		exceptions_throw_unsatisfiedlinkerror(m->name);
	}

	/* release memory */

	dump_release(dumpsize);

	return (functionptr) (ptrint) sym;
}
#endif /* !defined(WITH_STATIC_CLASSPATH) */


/* native_new_and_init *********************************************************

   Creates a new object on the heap and calls the initializer.
   Returns the object pointer or NULL if memory is exhausted.
			
*******************************************************************************/

java_objectheader *native_new_and_init(classinfo *c)
{
	methodinfo *m;
	java_objectheader *o;

	if (c == NULL)
		vm_abort("native_new_and_init: c == NULL");

	/* create object */

	o = builtin_new(c);
	
	if (o == NULL)
		return NULL;

	/* try to find the initializer */

	m = class_findmethod(c, utf_init, utf_void__void);
	                      	                      
	/* ATTENTION: returning the object here is ok, since the class may
       not have an initializer */

	if (m == NULL)
		return o;

	/* call initializer */

	(void) vm_call_method(m, o);

	return o;
}


java_objectheader *native_new_and_init_string(classinfo *c, java_objectheader *s)
{
	methodinfo        *m;
	java_objectheader *o;

	if (c == NULL)
		vm_abort("native_new_and_init_string: c == NULL");

	/* create object */

	o = builtin_new(c);

	if (o == NULL)
		return NULL;

	/* find initializer */

	m = class_resolveclassmethod(c,
								 utf_init,
								 utf_java_lang_String__void,
								 NULL,
								 true);

	/* initializer not found */

	if (m == NULL)
		return NULL;

	/* call initializer */

	(void) vm_call_method(m, o, s);

	return o;
}


java_objectheader *native_new_and_init_int(classinfo *c, s4 i)
{
	methodinfo *m;
	java_objectheader *o;

	if (c == NULL)
		vm_abort("native_new_and_init_int: c == NULL");

	/* create object */

	o = builtin_new(c);
	
	if (o == NULL)
		return NULL;

	/* find initializer */

	m = class_resolveclassmethod(c, utf_init, utf_int__void, NULL, true);

	/* initializer not found  */

	if (m == NULL)
		return NULL;

	/* call initializer */

	(void) vm_call_method(m, o, i);

	return o;
}


java_objectheader *native_new_and_init_throwable(classinfo *c, java_objectheader *t)
{
	java_objectheader *o;
	methodinfo        *m;

	if (c == NULL)
		vm_abort("native_new_and_init_throwable: c == NULL");

	/* create object */

	o = builtin_new(c);
	
	if (o == NULL)
		return NULL;

	/* find initializer */

	m = class_findmethod(c, utf_init, utf_java_lang_Throwable__void);
	                      	                      
	/* initializer not found */

	if (m == NULL)
		return NULL;

	/* call initializer */

	(void) vm_call_method(m, o, t);

	return o;
}


/* native_class_getdeclaredannotations *****************************************

   Implementation for
   java.lang.Class.getDeclaredAnnotations(Ljava/lang/Class;)[Ljava/lang/annotation/Annotation;

*******************************************************************************/

#if defined(ENABLE_JAVASE)
java_objectarray *native_class_getdeclaredannotations(classinfo *c)
{
	java_objectarray *oa;
	s4                count;
	s4                i;

	classinfo *class_java_lang_annotation_Annotation;

	/* create Annotation-array */

	/* XXX should we cache that class? */
	if (!(class_java_lang_annotation_Annotation =
		  load_class_bootstrap(utf_new_char("java/lang/annotation/Annotation"))))
		return NULL;

	count = c->runtimevisibleannotationscount;

	oa = builtin_anewarray(count, class_java_lang_annotation_Annotation);

	if (oa == NULL)
		return NULL;

	/* fill the annotations */

	for (i = 0; i < count; i++) {
	}

	return oa;
}
#endif


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
