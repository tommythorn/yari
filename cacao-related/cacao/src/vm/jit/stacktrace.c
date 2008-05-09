/* src/vm/jit/stacktrace.c - machine independent stacktrace system

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

   $Id: stacktrace.c 7840 2007-04-30 10:53:13Z tbfg $

*/


#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "vm/types.h"

#include "mm/gc-common.h"
#include "mm/memory.h"

#include "vm/jit/stacktrace.h"

#include "vm/global.h"                   /* required here for native includes */
#include "native/jni.h"
#include "native/include/java_lang_Throwable.h"

#if defined(WITH_CLASSPATH_GNU)
# include "native/include/java_lang_VMThrowable.h"
#endif

#if defined(ENABLE_THREADS)
# include "threads/native/threads.h"
#else
# include "threads/none/threads.h"
#endif

#include "toolbox/logging.h"

#include "vm/builtin.h"
#include "vm/cycles-stats.h"
#include "vm/exceptions.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"
#include "vm/jit/codegen-common.h"
#include "vm/jit/methodheader.h"

#include "vmcore/class.h"
#include "vmcore/loader.h"
#include "vmcore/options.h"


/* global variables ***********************************************************/
#if !defined(ENABLE_THREADS)
stackframeinfo *_no_threads_stackframeinfo = NULL;
#endif

CYCLES_STATS_DECLARE(stacktrace_overhead        ,100,1)
CYCLES_STATS_DECLARE(stacktrace_fillInStackTrace,40,5000)
CYCLES_STATS_DECLARE(stacktrace_getClassContext ,40,5000)
CYCLES_STATS_DECLARE(stacktrace_getCurrentClass ,40,5000)
CYCLES_STATS_DECLARE(stacktrace_getStack        ,40,10000)


/* stacktrace_create_stackframeinfo ********************************************

   Creates an stackframe info structure for inline code in the
   interpreter.

*******************************************************************************/

#if defined(ENABLE_INTRP)
void stacktrace_create_stackframeinfo(stackframeinfo *sfi, u1 *pv, u1 *sp,
									  u1 *ra)
{
	stackframeinfo **psfi;
	methodinfo      *m;
	codeinfo        *code;

	/* get current stackframe info pointer */

	psfi = &STACKFRAMEINFO;

	/* if we don't have pv handy */

	if (pv == NULL) {
#if defined(ENABLE_INTRP)
		if (opt_intrp)
			pv = codegen_get_pv_from_pc(ra);
		else
#endif
			{
#if defined(ENABLE_JIT)
				pv = md_codegen_get_pv_from_pc(ra);
#endif
			}
	}

	/* get codeinfo pointer from data segment */

	code = *((codeinfo **) (pv + CodeinfoPointer));

	/* For asm_vm_call_method the codeinfo pointer is NULL. */

	m = (code == NULL) ? NULL : code->m;

	/* fill new stackframe info structure */

	sfi->prev   = *psfi;
	sfi->method = m;
	sfi->pv     = pv;
	sfi->sp     = sp;
	sfi->ra     = ra;

	/* xpc is the same as ra, but is required in stacktrace_create */

	sfi->xpc    = ra;

	/* store new stackframe info pointer */

	*psfi = sfi;
}
#endif /* defined(ENABLE_INTRP) */


/* stacktrace_create_extern_stackframeinfo *************************************

   Creates an stackframe info structure for an extern exception
   (hardware or assembler).

*******************************************************************************/

void stacktrace_create_extern_stackframeinfo(stackframeinfo *sfi, u1 *pv,
											 u1 *sp, u1 *ra, u1 *xpc)
{
	stackframeinfo **psfi;
#if !defined(__I386__) && !defined(__X86_64__) && !defined(__S390__) && !defined(__M68K__)
	bool             isleafmethod;
#endif
#if defined(ENABLE_JIT)
	s4               framesize;
#endif

	/* get current stackframe info pointer */

	psfi = &STACKFRAMEINFO;

	/* sometimes we don't have pv handy (e.g. in asmpart.S:
       L_asm_call_jit_compiler_exception or in the interpreter). */

	if (pv == NULL) {
#if defined(ENABLE_INTRP)
		if (opt_intrp)
			pv = codegen_get_pv_from_pc(ra);
		else
#endif
			{
#if defined(ENABLE_JIT)
# if defined(__SPARC_64__)
				pv = md_get_pv_from_stackframe(sp);
# else
				pv = md_codegen_get_pv_from_pc(ra);
# endif
#endif
			}
	}

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
	/* When using the interpreter, we pass RA to the function. */

	if (!opt_intrp) {
# endif
# if defined(__I386__) || defined(__X86_64__) || defined(__S390__) || defined(__M68K__)
		/* On i386 and x86_64 we always have to get the return address
		   from the stack. */
		/* m68k has return address on stack always */
		/* On S390 we use REG_RA as REG_ITMP3, so we have always to get
		   the RA from stack. */

		framesize = *((u4 *) (pv + FrameSize));

		ra = md_stacktrace_get_returnaddress(sp, framesize);
# else
		/* If the method is a non-leaf function, we need to get the return
		   address from the stack. For leaf functions the return address
		   is set correctly. This makes the assembler and the signal
		   handler code simpler. */

		isleafmethod = *((s4 *) (pv + IsLeaf));

		if (!isleafmethod) {
			framesize = *((u4 *) (pv + FrameSize));

			ra = md_stacktrace_get_returnaddress(sp, framesize);
		}
# endif
# if defined(ENABLE_INTRP)
	}
# endif
#endif /* defined(ENABLE_JIT) */

	/* fill new stackframe info structure */

	sfi->prev   = *psfi;
	sfi->method = NULL;
	sfi->pv     = pv;
	sfi->sp     = sp;
	sfi->ra     = ra;
	sfi->xpc    = xpc;

	/* store new stackframe info pointer */

	*psfi = sfi;
}


/* stacktrace_create_native_stackframeinfo *************************************

   Creates a stackframe info structure for a native stub.

*******************************************************************************/

void stacktrace_create_native_stackframeinfo(stackframeinfo *sfi, u1 *pv,
											 u1 *sp, u1 *ra)
{
	stackframeinfo **psfi;
	methodinfo      *m;
	codeinfo        *code;

	/* get codeinfo pointer from data segment */

	code = *((codeinfo **) (pv + CodeinfoPointer));

	/* For asm_vm_call_method the codeinfo pointer is NULL. */

	m = (code == NULL) ? NULL : code->m;

	/* get current stackframe info pointer */

	psfi = &STACKFRAMEINFO;

	/* fill new stackframe info structure */

	sfi->prev   = *psfi;
	sfi->method = m;
	sfi->pv     = NULL;
	sfi->sp     = sp;
	sfi->ra     = ra;
	sfi->xpc    = NULL;

	/* store new stackframe info pointer */

	*psfi = sfi;
}


/* stacktrace_remove_stackframeinfo ********************************************

   Remove the topmost stackframeinfo in the current thread.

*******************************************************************************/

void stacktrace_remove_stackframeinfo(stackframeinfo *sfi)
{
	stackframeinfo **psfi;

	/* get current stackframe info pointer */

	psfi = &STACKFRAMEINFO;

	/* restore the old pointer */

	*psfi = sfi->prev;
}


/* stacktrace_add_entry ********************************************************

   Adds a new entry to the stacktrace buffer.

*******************************************************************************/

static void stacktrace_add_entry(stacktracebuffer *stb, methodinfo *m, u2 line)
{
	stacktrace_entry *ste;

	/* check if we already reached the buffer capacity */

	if (stb->used >= stb->capacity) {
		/* reallocate new memory */

		stb->entries = DMREALLOC(stb->entries, stacktrace_entry, stb->capacity,
								 stb->capacity + STACKTRACE_CAPACITY_INCREMENT);

		/* set new buffer capacity */

		stb->capacity = stb->capacity + STACKTRACE_CAPACITY_INCREMENT;
	}

	/* insert the current entry */

	ste = &(stb->entries[stb->used]);

	ste->method     = m;
	ste->linenumber = line;

	/* increase entries used count */

	stb->used += 1;
}


/* stacktrace_add_method *******************************************************

   Add stacktrace entries[1] for the given method to the stacktrace buffer.

   IN:
       stb.........stacktracebuffer to fill
	   m...........method for which entries should be created
	   pv..........pv of method
	   pc..........position of program counter within the method's code

   OUT:
       true, if stacktrace entries were successfully created, false otherwise.

   [1] In case of inlined methods there may be more than one stacktrace
       entry for a codegen-level method. (see doc/inlining_stacktrace.txt)

*******************************************************************************/

static bool stacktrace_add_method(stacktracebuffer *stb, methodinfo *m, u1 *pv,
								  u1 *pc)
{
	codeinfo *code;                     /* compiled realization of method     */
	s4        linenumber;

	/* find the realization of the method the pc is in */

	code = *((codeinfo **) (pv + CodeinfoPointer));

	/* search the line number table */

	linenumber = dseg_get_linenumber_from_pc(&m, pv, pc);

	/* now add a new entry to the staktrace */

	stacktrace_add_entry(stb, m, linenumber);

	return true;
}


/* stacktrace_create ***********************************************************

   Generates a stacktrace from the thread passed into a
   stacktracebuffer.  The stacktracebuffer is allocated on the
   dump memory.
   
   NOTE: The first element in the stackframe chain must always be a
         native stackframeinfo (e.g. VMThrowable.fillInStackTrace() is
         a native function).

   RETURN VALUE:
      pointer to the stacktracebuffer, or
      NULL if there is no stacktrace available for the
      given thread.

*******************************************************************************/

stacktracebuffer *stacktrace_create(stackframeinfo *sfi)
{
	stacktracebuffer *stb;
	methodinfo       *m;
	codeinfo         *code;
	u1               *pv;
	u1               *sp;
	u4                framesize;
	u1               *ra;
	u1               *xpc;

	/* prevent compiler warnings */

	pv = NULL;
	sp = NULL;
	ra = NULL;

	/* create a stacktracebuffer in dump memory */

	stb = DNEW(stacktracebuffer);

	stb->capacity = STACKTRACE_CAPACITY_DEFAULT;
	stb->used     = 0;
	stb->entries  = DMNEW(stacktrace_entry, STACKTRACE_CAPACITY_DEFAULT);

#define PRINTMETHODS 0

#if PRINTMETHODS
	printf("\n\nfillInStackTrace start:\n");
	fflush(stdout);
#endif

	/* Loop while we have a method pointer (asm_calljavafunction has
	   NULL) or there is a stackframeinfo in the chain. */

	m = NULL;

	while ((m != NULL) || (sfi != NULL)) {
		/* m == NULL should only happen for the first time and inline
		   stackframe infos, like from the exception stubs or the
		   patcher wrapper. */

		if (m == NULL) {
			/* for native stub stackframe infos, pv is always NULL */

			if (sfi->pv == NULL) {
				/* get methodinfo, sp and ra from the current stackframe info */

				m  = sfi->method;
				sp = sfi->sp;           /* sp of parent Java function         */
				ra = sfi->ra;

				if (m)
					stacktrace_add_entry(stb, m, 0);

#if PRINTMETHODS
				printf("ra=%p sp=%p, ", ra, sp);
				method_print(m);
				printf(": native stub\n");
				fflush(stdout);
#endif
				/* This is an native stub stackframe info, so we can
				   get the parent pv from the return address
				   (ICMD_INVOKE*). */

#if defined(ENABLE_INTRP)
				if (opt_intrp)
					pv = codegen_get_pv_from_pc(ra);
				else
#endif
					{
#if defined(ENABLE_JIT)
						pv = md_codegen_get_pv_from_pc(ra);
#endif
					}

				/* get methodinfo pointer from parent data segment */

				code = *((codeinfo **) (pv + CodeinfoPointer));

				/* For asm_vm_call_method the codeinfo pointer is
				   NULL. */

				m = (code == NULL) ? NULL : code->m;

			} else {
				/* Inline stackframe infos are special: they have a
				   xpc of the actual exception position and the return
				   address saved since an inline stackframe info can
				   also be in a leaf method (no return address saved
				   on stack!!!).  ATTENTION: This one is also for
				   hardware exceptions!!! */

				/* get methodinfo, sp and ra from the current stackframe info */

				m   = sfi->method;      /* m == NULL                          */
				pv  = sfi->pv;          /* pv of parent Java function         */
				sp  = sfi->sp;          /* sp of parent Java function         */
				ra  = sfi->ra;          /* ra of parent Java function         */
				xpc = sfi->xpc;         /* actual exception position          */

#if PRINTMETHODS
				printf("ra=%p sp=%p, ", ra, sp);
				printf("NULL: inline stub\n");
				fflush(stdout);
#endif

				/* get methodinfo from current Java method */

				code = *((codeinfo **) (pv + CodeinfoPointer));

				/* For asm_vm_call_method the codeinfo pointer is
				   NULL. */

				m = (code == NULL) ? NULL : code->m;

				/* if m == NULL, this is a asm_calljavafunction call */

				if (m != NULL) {
#if PRINTMETHODS
					printf("ra=%p sp=%p, ", ra, sp);
					method_print(m);
					printf(": inline stub parent");
					fflush(stdout);
#endif

#if defined(ENABLE_INTRP)
					if (!opt_intrp) {
#endif

					/* add the method to the stacktrace */

					stacktrace_add_method(stb, m, pv, (u1 *) ((ptrint) xpc));

					/* get the current stack frame size */

					framesize = *((u4 *) (pv + FrameSize));

#if PRINTMETHODS
					printf(", framesize=%d\n", framesize);
					fflush(stdout);
#endif

					/* Set stack pointer to stackframe of parent Java
					   function of the current Java function. */

#if defined(__I386__) || defined (__X86_64__) || defined (__M68K__)
					sp += framesize + SIZEOF_VOID_P;
#elif defined(__SPARC_64__)
					sp = md_get_framepointer(sp);
#else
					sp += framesize;
#endif

					/* get data segment and methodinfo pointer from
					   parent method */

#if defined(ENABLE_JIT)
					pv = md_codegen_get_pv_from_pc(ra);
#endif

					code = *((codeinfo **) (pv + CodeinfoPointer));

					/* For asm_vm_call_method the codeinfo pointer is
					   NULL. */

					m = (code == NULL) ? NULL : code->m;

#if defined(ENABLE_INTRP)
					}
#endif
				}
#if PRINTMETHODS
				else {
					printf("ra=%p sp=%p, ", ra, sp);
					printf("asm_calljavafunction\n");
					fflush(stdout);
				}
#endif
			}

			/* get previous stackframeinfo in the chain */

			sfi = sfi->prev;

		} else {
#if PRINTMETHODS
			printf("ra=%p sp=%p, ", ra, sp);
			method_print(m);
			printf(": JIT");
			fflush(stdout);
#endif

			/* JIT method found, add it to the stacktrace (we subtract
			   1 from the return address since it points the the
			   instruction after call). */

			stacktrace_add_method(stb, m, pv, (u1 *) ((ptrint) ra) - 1);

			/* get the current stack frame size */

			framesize = *((u4 *) (pv + FrameSize));

#if PRINTMETHODS
			printf(", framesize=%d\n", framesize);
			fflush(stdout);
#endif

			/* get return address of current stack frame */

#if defined(ENABLE_JIT)
# if defined(ENABLE_INTRP)
			if (opt_intrp)
				ra = intrp_md_stacktrace_get_returnaddress(sp, framesize);
			else
# endif
				ra = md_stacktrace_get_returnaddress(sp, framesize);
#else
			ra = intrp_md_stacktrace_get_returnaddress(sp, framesize);
#endif

			/* get data segment and methodinfo pointer from parent method */

#if defined(ENABLE_INTRP)
			if (opt_intrp)
				pv = codegen_get_pv_from_pc(ra);
			else
#endif
				{
#if defined(ENABLE_JIT)
# if defined(__SPARC_64__)
					sp = md_get_framepointer(sp);
					pv = md_get_pv_from_stackframe(sp);
# else
					pv = md_codegen_get_pv_from_pc(ra);
# endif
#endif
				}

			code = *((codeinfo **) (pv + CodeinfoPointer));

			/* For asm_vm_call_method the codeinfo pointer is NULL. */

			m = (code == NULL) ? NULL : code->m;

			/* walk the stack */

#if defined(ENABLE_INTRP)
			if (opt_intrp)
				sp = *(u1 **) (sp - framesize);
			else
#endif
				{
#if defined(__I386__) || defined (__X86_64__) || defined (__M68K__)
					sp += framesize + SIZEOF_VOID_P;
#elif defined(__SPARC_64__)
					/* already has the new sp */
#else
					sp += framesize;
#endif
				}
		}
	}

	/* return the stacktracebuffer */

	if (stb->used == 0)
		return NULL;
	else
		return stb;
}


/* stacktrace_fillInStackTrace *************************************************

   Generate a stacktrace from the current thread for
   java.lang.VMThrowable.fillInStackTrace.

*******************************************************************************/

stacktracecontainer *stacktrace_fillInStackTrace(void)
{
	stacktracebuffer    *stb;
	stacktracecontainer *gcstc;
	s4                   gcstc_size;
	s4                   dumpsize;
	CYCLES_STATS_DECLARE_AND_START_WITH_OVERHEAD

	/* mark start of dump memory area */

	dumpsize = dump_size();

	/* create a stacktrace from the current thread */

	stb = stacktrace_create(STACKFRAMEINFO);

	if (stb == NULL)
		goto return_NULL;

	/* allocate memory from the GC heap and copy the stacktrace buffer */
	/* ATTENTION: use stacktracecontainer for this and make it look like
       an array */

	gcstc_size = sizeof(stacktracebuffer) +
	             sizeof(stacktrace_entry) * stb->used;
	gcstc = (stacktracecontainer *) builtin_newarray_byte(gcstc_size);

	if (gcstc == NULL)
		goto return_NULL;

	gcstc->stb.capacity = stb->capacity;
	gcstc->stb.used     = stb->used;
	gcstc->stb.entries  = gcstc->data;

	MCOPY(gcstc->data, stb->entries, stacktrace_entry, stb->used);

	/* release dump memory */

	dump_release(dumpsize);

	CYCLES_STATS_END_WITH_OVERHEAD(stacktrace_fillInStackTrace,
								   stacktrace_overhead)
	return gcstc;

return_NULL:
	dump_release(dumpsize);

	CYCLES_STATS_END_WITH_OVERHEAD(stacktrace_fillInStackTrace,
								   stacktrace_overhead)

	return NULL;
}


/* stacktrace_getClassContext **************************************************

   Creates a Class context array.

   RETURN VALUE:
      the array of java.lang.Class objects, or
	  NULL if an exception has been thrown

*******************************************************************************/

java_objectarray *stacktrace_getClassContext(void)
{
	stacktracebuffer  *stb;
	stacktrace_entry  *ste;
	java_objectarray  *oa;
	s4                 oalength;
	s4                 i;
	s4                 dumpsize;
	CYCLES_STATS_DECLARE_AND_START

	/* mark start of dump memory area */

	dumpsize = dump_size();

	/* create a stacktrace for the current thread */

	stb = stacktrace_create(STACKFRAMEINFO);

	if (stb == NULL)
		goto return_NULL;

	/* calculate the size of the Class array */

	for (i = 0, oalength = 0; i < stb->used; i++)
		if (stb->entries[i].method != NULL)
			oalength++;

	/* The first entry corresponds to the method whose implementation */
	/* calls stacktrace_getClassContext. We remove that entry.        */

	ste = &(stb->entries[0]);
	ste++;
	oalength--;

	/* allocate the Class array */

	oa = builtin_anewarray(oalength, class_java_lang_Class);
	if (!oa)
		goto return_NULL;

	/* fill the Class array from the stacktracebuffer */

	for(i = 0; i < oalength; i++, ste++) {
		if (ste->method == NULL) {
			i--;
			continue;
		}

		oa->data[i] = (java_objectheader *) ste->method->class;
	}

	/* release dump memory */

	dump_release(dumpsize);

	CYCLES_STATS_END(stacktrace_getClassContext)

	return oa;

return_NULL:
	dump_release(dumpsize);

	CYCLES_STATS_END(stacktrace_getClassContext)

	return NULL;
}


/* stacktrace_getCurrentClass **************************************************

   Find the current class by walking the stack trace.

   Quote from the JNI documentation:
	 
   In the Java 2 Platform, FindClass locates the class loader
   associated with the current native method.  If the native code
   belongs to a system class, no class loader will be
   involved. Otherwise, the proper class loader will be invoked to
   load and link the named class. When FindClass is called through the
   Invocation Interface, there is no current native method or its
   associated class loader. In that case, the result of
   ClassLoader.getBaseClassLoader is used."

*******************************************************************************/

#if defined(ENABLE_JAVASE)
classinfo *stacktrace_getCurrentClass(void)
{
	stacktracebuffer  *stb;
	stacktrace_entry  *ste;
	methodinfo        *m;
	s4                 i;
	s4                 dumpsize;
	CYCLES_STATS_DECLARE_AND_START

	/* mark start of dump memory area */

	dumpsize = dump_size();

	/* create a stacktrace for the current thread */

	stb = stacktrace_create(STACKFRAMEINFO);

	if (stb == NULL)
		goto return_NULL; /* XXX exception: how to distinguish from normal NULL return? */

	/* iterate over all stacktrace entries and find the first suitable
	   class */

	for (i = 0, ste = &(stb->entries[0]); i < stb->used; i++, ste++) {
		m = ste->method;

		if (m == NULL)
			continue;

		if (m->class == class_java_security_PrivilegedAction)
			goto return_NULL;

		if (m->class != NULL) {
			dump_release(dumpsize);

			CYCLES_STATS_END(stacktrace_getCurrentClass)

			return m->class;
		}
	}

	/* no Java method found on the stack */

return_NULL:
	dump_release(dumpsize);

	CYCLES_STATS_END(stacktrace_getCurrentClass)

	return NULL;
}
#endif /* ENABLE_JAVASE */


/* stacktrace_getStack *********************************************************

   Create a 2-dimensional array for java.security.VMAccessControler.

   RETURN VALUE:
      the arrary, or
	  NULL if an exception has been thrown

*******************************************************************************/

#if defined(ENABLE_JAVASE)
java_objectarray *stacktrace_getStack(void)
{
	stacktracebuffer  *stb;
	stacktrace_entry  *ste;
	java_objectarray  *oa;
	java_objectarray  *classes;
	java_objectarray  *methodnames;
	classinfo         *c;
	java_objectheader *string;
	s4                 i;
	s4                 dumpsize;
	CYCLES_STATS_DECLARE_AND_START

	/* mark start of dump memory area */

	dumpsize = dump_size();

	/* create a stacktrace for the current thread */

	stb = stacktrace_create(STACKFRAMEINFO);

	if (stb == NULL)
		goto return_NULL;

	/* get the first stacktrace entry */

	ste = &(stb->entries[0]);

	/* allocate all required arrays */

	oa = builtin_anewarray(2, arrayclass_java_lang_Object);

	if (oa == NULL)
		goto return_NULL;

	classes = builtin_anewarray(stb->used, class_java_lang_Class);

	if (classes == NULL)
		goto return_NULL;

	methodnames = builtin_anewarray(stb->used, class_java_lang_String);

	if (methodnames == NULL)
		goto return_NULL;

	/* set up the 2-dimensional array */

	oa->data[0] = (java_objectheader *) classes;
	oa->data[1] = (java_objectheader *) methodnames;

	/* iterate over all stacktrace entries */

	for (i = 0, ste = &(stb->entries[0]); i < stb->used; i++, ste++) {
		c = ste->method->class;

		classes->data[i] = (java_objectheader *) c;

		string = javastring_new(ste->method->name);

		if (string == NULL)
			goto return_NULL;

		methodnames->data[i] = string;
	}

	/* return the 2-dimensional array */

	dump_release(dumpsize);

	CYCLES_STATS_END(stacktrace_getStack)

	return oa;

return_NULL:
	dump_release(dumpsize);

	CYCLES_STATS_END(stacktrace_getStack)

	return NULL;
}
#endif /* ENABLE_JAVASE */


/* stacktrace_print_trace_from_buffer ******************************************

   Print the stacktrace of a given stacktracebuffer with CACAO intern
   methods (no Java help). This method is used by
   stacktrace_dump_trace and builtin_trace_exception.

*******************************************************************************/

void stacktrace_print_trace_from_buffer(stacktracebuffer *stb)
{
	stacktrace_entry *ste;
	methodinfo       *m;
	s4                i;

	ste = &(stb->entries[0]);

	for (i = 0; i < stb->used; i++, ste++) {
		m = ste->method;

		printf("\tat ");
		utf_display_printable_ascii_classname(m->class->name);
		printf(".");
		utf_display_printable_ascii(m->name);
		utf_display_printable_ascii(m->descriptor);

		if (m->flags & ACC_NATIVE) {
			puts("(Native Method)");

		} else {
			printf("(");
			utf_display_printable_ascii(m->class->sourcefile);
			printf(":%d)\n", (u4) ste->linenumber);
		}
	}

	/* just to be sure */

	fflush(stdout);
}


/* stacktrace_print_trace ******************************************************

   Print the stacktrace of a given exception. More or less a wrapper
   to stacktrace_print_trace_from_buffer.

*******************************************************************************/

void stacktrace_print_trace(java_objectheader *xptr)
{
	java_lang_Throwable   *t;
#if defined(WITH_CLASSPATH_GNU)
	java_lang_VMThrowable *vmt;
#endif
	stacktracecontainer   *stc;
	stacktracebuffer      *stb;

	t = (java_lang_Throwable *) xptr;

	if (t == NULL)
		return;

	/* now print the stacktrace */

#if defined(WITH_CLASSPATH_GNU)
	vmt = t->vmState;
	stc = (stacktracecontainer *) vmt->vmData;
	stb = &(stc->stb);
#elif defined(WITH_CLASSPATH_CLDC1_1)
	stc = (stacktracecontainer *) t->backtrace;
	stb = &(stc->stb);
#endif

	stacktrace_print_trace_from_buffer(stb);
}


#if defined(ENABLE_CYCLES_STATS)
void stacktrace_print_cycles_stats(FILE *file)
{
	CYCLES_STATS_PRINT_OVERHEAD(stacktrace_overhead,file);
	CYCLES_STATS_PRINT(stacktrace_fillInStackTrace,file);
	CYCLES_STATS_PRINT(stacktrace_getClassContext ,file);
	CYCLES_STATS_PRINT(stacktrace_getCurrentClass ,file);
	CYCLES_STATS_PRINT(stacktrace_getStack        ,file);
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
 * vim:noexpandtab:sw=4:ts=4:
 */
