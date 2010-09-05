/* src/threads/threads-common.c - machine independent thread functions

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

   $Id: threads-common.c 7876 2007-05-07 11:37:00Z twisti $

*/


#include "config.h"

#include <assert.h>

#include "vm/types.h"

#include "native/jni.h"

#include "native/include/java_lang_Object.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Thread.h"

#if defined(WITH_CLASSPATH_GNU)
# include "native/include/java_lang_VMThread.h"
#endif

#include "threads/critical.h"
#include "threads/lock-common.h"
#include "threads/threads-common.h"

#include "vm/builtin.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/stacktrace.h"

#include "vmcore/class.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/options.h"
# include "vmcore/statistics.h"
#endif

#include "vmcore/utf8.h"


/* global variables ***********************************************************/

/* global threads table */
static threads_table_t threads_table;


/* prototypes *****************************************************************/

static void threads_table_init(threadobject *mainthread);


/* threads_preinit *************************************************************

   Do some early initialization of stuff required.

   ATTENTION: Do NOT use any Java heap allocation here, as gc_init()
   is called AFTER this function!

*******************************************************************************/

void threads_preinit(void)
{
	threadobject *mainthread;

	/* Initialize the threads implementation (sets the thinlock on the
	   main thread). */

	threads_impl_preinit();

	/* create internal thread data-structure for the main thread */

	mainthread = threads_create_thread();

	mainthread->object   = NULL;
	mainthread->index    = 1;
	mainthread->thinlock = lock_pre_compute_thinlock(mainthread->index);

	/* thread is a Java thread and running */

	mainthread->flags = THREAD_FLAG_JAVA;
	mainthread->state = THREAD_STATE_RUNNABLE;

	/* store the internal thread data-structure in the TSD */

	threads_set_current_threadobject(mainthread);
	
	/* initialize the threads table with the main-thread */

	threads_table_init(mainthread);

	/* initialize locking subsystems */

	lock_init();

	/* initialize the critical section */

	critical_init();
}


/* threads_table_init **********************************************************

   Initialize the global threads table.  We initialize the table with
   the main-thread, which has always the index 1.

   IN:
      mainthread....the main-thread

*******************************************************************************/

#define THREADS_INITIAL_TABLE_SIZE    8

static void threads_table_init(threadobject *mainthread)
{
	threads_table_entry_t *ttemain;
	s4                     size;
	s4                     i;

	/* initialize the threads table lock */

	threads_impl_table_init();

	/* initialize the table */

	size = THREADS_INITIAL_TABLE_SIZE;

	threads_table.table   = MNEW(threads_table_entry_t, size);
	threads_table.size    = size;
	threads_table.used    = 0;
	threads_table.daemons = 0;

	/* Link the entries in a freelist.  Skip 2 entries: 0 is the
	   free-list header and 1 is the main thread. */

	for (i = 2; i < size; i++) {
		threads_table.table[i].thread = NULL;
		threads_table.table[i].next   = i + 1;
	}

	threads_table.table[0].next = 2;

	/* terminate the freelist */

	threads_table.table[size - 1].next = 0;          /* index 0 is never free */

	/* insert the main-thread */

	ttemain = &(threads_table.table[1]);

	ttemain->thread = mainthread;
	ttemain->next   = 0;

	/* now 1 entry is used */

	threads_table.used = 1;
}


/* threads_table_add ***********************************************************

   Add a thread to the global threads table. The index is entered in the
   threadobject. The thinlock value for the thread is pre-computed.

   IN:
      thread............the thread to add

   RETURN VALUE:
      The table index for the newly added thread. This value has also been
	  entered in the threadobject.

*******************************************************************************/

s4 threads_table_add(threadobject *thread)
{
	threads_table_entry_t *ttefree;
	threads_table_entry_t *ttemain;
	threads_table_entry_t *tte;
	s4 index;
	s4 oldsize;
	s4 newsize;
	s4 i;

	/* lock the threads table */

	threads_table_lock();

	/* get free and main entry */

	ttefree = &(threads_table.table[0]);
	ttemain = &(threads_table.table[1]);

	/* get the next free index */

	index = ttefree->next;

	/* no entry free anymore? resize the table */

	if (index == 0) {
		/* we must grow the table */

		oldsize = threads_table.size;
		newsize = oldsize * 2;

		threads_table.table = MREALLOC(threads_table.table,
									   threads_table_entry_t, oldsize, newsize);
		threads_table.size = newsize;

		/* the addresses have changed, get them again */

		ttefree = &(threads_table.table[0]);
		ttemain = &(threads_table.table[1]);

		/* link the new entries to a free list */

		for (i = oldsize; i < newsize; i++) {
			threads_table.table[i].thread = NULL;
			threads_table.table[i].next   = i + 1;
		}

		ttefree->next = oldsize;

		/* terminate the freelist */

		threads_table.table[newsize - 1].next = 0;   /* index 0 is never free */

		/* use the first of the new entries */

		index = ttefree->next;
	}

	/* get the entry with the assigned index */

	tte = &(threads_table.table[index]);

	/* store the next free index into the free-list header */

	ttefree->next = tte->next;

	/* store the thread in the table */

	tte->thread = thread;

	/* link the new entry into the used-list */

	tte->next     = ttemain->next;
	ttemain->next = index;

	/* update the counters */

	threads_table.used++;

	if (thread->flags & THREAD_FLAG_DAEMON)
		threads_table.daemons++;

	assert(threads_table.used < threads_table.size);

	/* set the thread variables */

	thread->index    = index;
	thread->thinlock = lock_pre_compute_thinlock(index);

	/* unlock the threads table */

	threads_table_unlock();

	return index;
}


/* threads_table_remove *******************************************************

   Remove a thread from the global threads table.

   IN:
      thread............the thread to remove

******************************************************************************/

void threads_table_remove(threadobject *thread)
{
	threads_table_entry_t *ttefree;
	threads_table_entry_t *tte;
	s4                     index;
	s4                     i;

	/* lock the threads table */

	threads_table_lock();

	/* get the free entry */

	ttefree = &(threads_table.table[0]);

	/* get the current entry */

	index = thread->index;
	tte   = &(threads_table.table[index]);

	assert(tte->thread == thread);

	/* Find the entry which has the one to be removed as next entry (I
	   think it's better to do it at the removal in linear time than
	   to have a list or to do it every time we iterate over all
	   threads). */

	for (i = 0; i < threads_table.size; i++) {
		if (threads_table.table[i].next == index) {
			threads_table.table[i].next = tte->next;
			break;
		}
	}

	/* clear the thread pointer in the entry */

	tte->thread = NULL;

	/* this entry is free now, add it to the free-list */

	tte->next     = ttefree->next;
	ttefree->next = index;

	/* update the counters */

	threads_table.used--;

	if (thread->flags & THREAD_FLAG_DAEMON)
		threads_table.daemons--;

	assert(threads_table.used >= 0);

	/* delete the index in the threadobject to discover bugs */
#if !defined(NDEBUG)
	thread->index = 0;
#endif

	/* unlock the threads table */

	threads_table_unlock();
}


/* threads_table_get ***********************************************************

   Return the thread of the given table-entry index.

   NOTE: It is valid to pass and index of 0, as this entry is the
         free-list header where the thread pointer is always NULL and
         this is thre expected behavior.

   NOTE: This function does not lock the table.

*******************************************************************************/

static threadobject *threads_table_get(s4 index)
{
	threadobject *thread;

	/* get the requested entry */

	assert((index >= 0) && (index < threads_table.size));

	thread = threads_table.table[index].thread;

	return thread;
}


/* threads_table_get_threads ***************************************************

   Return the number of running threads.

   NOTE: This function does not lock the table.

*******************************************************************************/

s4 threads_table_get_threads(void)
{
	return threads_table.used;
}


/* threads_table_get_non_daemons ***********************************************

   Return the number of non-daemon threads.

*******************************************************************************/

s4 threads_table_get_non_daemons(void)
{
	s4 nondaemons;

	/* lock the threads table */

	threads_table_lock();

	nondaemons = threads_table.used - threads_table.daemons;

	/* unlock the threads table */

	threads_table_unlock();

	return nondaemons;
}


/* threads_table_first *********************************************************

   Return the first thread of the threads table.

   NOTE: This is always the entry with index 1 and must be the main
         thread.

   NOTE: This function does not lock the table.

*******************************************************************************/

threadobject *threads_table_first(void)
{
	threadobject *thread;

	/* get the requested entry */

	thread = threads_table_get(1);

	return thread;
}


/* threads_table_next **********************************************************

   Return the next thread of the threads table relative to the passed
   one.

   NOTE: This function does not lock the table.

*******************************************************************************/

threadobject *threads_table_next(threadobject *thread)
{
	threads_table_entry_t *tte;
	threadobject          *next;
	s4                     index;

	index = thread->index;

	/* get the passed entry */

	assert((index > 0) && (index < threads_table.size));

	tte = &(threads_table.table[index]);

	/* get the requested entry */

	next = threads_table_get(tte->next);

	return next;
}


/* threads_table_dump *********************************************************

   Dump the threads table for debugging purposes.

******************************************************************************/

#if !defined(NDEBUG)
void threads_table_dump(void)
{
	s4 i;
	s4 size;
	ptrint index;

	size = threads_table.size;

	log_println("threads table ==========");

	log_println("size:    %d", size);
	log_println("used:    %d", threads_table.used);
	log_println("daemons: %d", threads_table.daemons);

	for (i = 0; i < size; i++) {
		index = threads_table.table[i].next;

		if (threads_table.table[i].thread != NULL)
			log_println("%4d: thread=0x%08x, next=%d", i,
						threads_table.table[i].thread->tid, (int) index);
		else
			log_println("%4d: free, next=%d", i, (int) index);
	}

	log_println("end of threads table ==========");
}
#endif


/* threads_create_thread *******************************************************

   Creates and initializes an internal thread data-structure.

*******************************************************************************/

threadobject *threads_create_thread(void)
{
	threadobject *thread;

	/* allocate internal thread data-structure */

#if defined(ENABLE_GC_BOEHM)
	thread = GCNEW_UNCOLLECTABLE(threadobject, 1);
#else
	thread = NEW(threadobject);
#endif

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		size_threadobject += sizeof(threadobject);
#endif

	/* initialize thread data structure */

	threads_init_threadobject(thread);
	lock_init_execution_env(thread);

	return thread;
}


/* threads_thread_start_internal ***********************************************

   Start an internal thread in the JVM.  No Java thread objects exists
   so far.

   IN:
      name.......UTF-8 name of the thread
      f..........function pointer to C function to start

*******************************************************************************/

bool threads_thread_start_internal(utf *name, functionptr f)
{
	threadobject       *thread;
	java_lang_Thread   *t;
#if defined(WITH_CLASSPATH_GNU)
	java_lang_VMThread *vmt;
#endif

	/* create internal thread data-structure */

	thread = threads_create_thread();

	/* create the java thread object */

	t = (java_lang_Thread *) builtin_new(class_java_lang_Thread);

	if (t == NULL)
		return false;

#if defined(WITH_CLASSPATH_GNU)
	vmt = (java_lang_VMThread *) builtin_new(class_java_lang_VMThread);

	if (vmt == NULL)
		return false;

	vmt->thread = t;
	vmt->vmdata = (java_lang_Object *) thread;

	t->vmThread = vmt;
#elif defined(WITH_CLASSPATH_CLDC1_1)
	t->vm_thread = (java_lang_Object *) thread;
#endif

	thread->object     = t;
	thread->flags      = THREAD_FLAG_DAEMON;

	/* set java.lang.Thread fields */

	t->name     = (java_lang_String *) javastring_new(name);
#if defined(ENABLE_JAVASE)
	t->daemon   = true;
#endif
	t->priority = NORM_PRIORITY;

	/* start the thread */

	threads_impl_thread_start(thread, f);

	/* everything's ok */

	return true;
}


/* threads_thread_start ********************************************************

   Start a Java thread in the JVM.  Only the java thread object exists
   so far.

   IN:
      object.....the java thread object java.lang.Thread

*******************************************************************************/

void threads_thread_start(java_lang_Thread *object)
{
	threadobject *thread;

	/* create internal thread data-structure */

	thread = threads_create_thread();

	/* link the two objects together */

	thread->object = object;

#if defined(ENABLE_JAVASE)
	/* is this a daemon thread? */

	if (object->daemon == true)
		thread->flags |= THREAD_FLAG_DAEMON;
#endif

#if defined(WITH_CLASSPATH_GNU)
	assert(object->vmThread);
	assert(object->vmThread->vmdata == NULL);

	object->vmThread->vmdata = (java_lang_Object *) thread;
#elif defined(WITH_CLASSPATH_CLDC1_1)
	object->vm_thread = (java_lang_Object *) thread;
#endif

	/* Start the thread.  Don't pass a function pointer (NULL) since
	   we want Thread.run()V here. */

	threads_impl_thread_start(thread, NULL);
}


/* threads_get_current_tid *****************************************************

   Return the tid of the current thread.
   
   RETURN VALUE:
       the current tid

*******************************************************************************/

ptrint threads_get_current_tid(void)
{
	threadobject *thread;

	thread = THREADOBJECT;

	/* this may happen during bootstrap */

	if (thread == NULL)
		return 0;

	return (ptrint) thread->tid;
}


/* threads_thread_get_state ****************************************************

   Returns the current state of the given thread.

*******************************************************************************/

utf *threads_thread_get_state(threadobject *thread)
{
	utf *u;

	switch (thread->state) {
	case THREAD_STATE_NEW:
		u = utf_new_char("NEW");
		break;
	case THREAD_STATE_RUNNABLE:
		u = utf_new_char("RUNNABLE");
		break;
	case THREAD_STATE_BLOCKED:
		u = utf_new_char("BLOCKED");
		break;
	case THREAD_STATE_WAITING:
		u = utf_new_char("WAITING");
		break;
	case THREAD_STATE_TIMED_WAITING:
		u = utf_new_char("TIMED_WAITING");
		break;
	case THREAD_STATE_TERMINATED:
		u = utf_new_char("TERMINATED");
		break;
	default:
		vm_abort("threads_get_state: unknown thread state %d", thread->state);
	}

	return u;
}


/* threads_thread_is_alive *****************************************************

   Returns if the give thread is alive.

*******************************************************************************/

bool threads_thread_is_alive(threadobject *thread)
{
	bool result;

	switch (thread->state) {
	case THREAD_STATE_NEW:
	case THREAD_STATE_TERMINATED:
		result = false;
		break;

	case THREAD_STATE_RUNNABLE:
	case THREAD_STATE_BLOCKED:
	case THREAD_STATE_WAITING:
	case THREAD_STATE_TIMED_WAITING:
		result = true;
		break;

	default:
		vm_abort("threads_is_alive: unknown thread state %d", thread->state);
	}

	return result;
}


/* threads_dump ****************************************************************

   Dumps info for all threads running in the JVM.  This function is
   called when SIGQUIT (<ctrl>-\) is sent to CACAO.

*******************************************************************************/

void threads_dump(void)
{
	threadobject     *t;
	java_lang_Thread *object;
	utf              *name;

	/* XXX we should stop the world here */

	/* lock the threads table */

	threads_table_lock();

	printf("Full thread dump CACAO "VERSION":\n");

	/* iterate over all started threads */

	for (t = threads_table_first(); t != NULL; t = threads_table_next(t)) {
		/* get thread object */

		object = t->object;

		/* the thread may be currently in initalization, don't print it */

		if (object != NULL) {
			/* get thread name */

#if defined(ENABLE_JAVASE)
			name = javastring_toutf((java_objectheader *) object->name, false);
#elif defined(ENABLE_JAVAME_CLDC1_1)
			name = object->name;
#endif

			printf("\n\"");
			utf_display_printable_ascii(name);
			printf("\"");

			if (t->flags & THREAD_FLAG_DAEMON)
				printf(" daemon");

			printf(" prio=%d", object->priority);

#if SIZEOF_VOID_P == 8
			printf(" tid=0x%016lx (%ld)", (ptrint) t->tid, (ptrint) t->tid);
#else
			printf(" tid=0x%08x (%d)", (ptrint) t->tid, (ptrint) t->tid);
#endif

			/* print thread state */

			switch (t->state) {
			case THREAD_STATE_NEW:
				printf(" new");
				break;
			case THREAD_STATE_RUNNABLE:
				printf(" runnable");
				break;
			case THREAD_STATE_BLOCKED:
				printf(" blocked");
				break;
			case THREAD_STATE_WAITING:
				printf(" waiting");
				break;
			case THREAD_STATE_TIMED_WAITING:
				printf(" waiting on condition");
				break;
			case THREAD_STATE_TERMINATED:
				printf(" terminated");
				break;
			default:
				vm_abort("threads_dump: unknown thread state %d", t->state);
			}

			printf("\n");

			/* print trace of thread */

			threads_thread_print_stacktrace(t);
		}
	}

	/* unlock the threads table */

	threads_table_unlock();
}


/* threads_thread_print_stacktrace *********************************************

   Print the current stacktrace of the current thread.

*******************************************************************************/

void threads_thread_print_stacktrace(threadobject *thread)
{
	stackframeinfo   *sfi;
	stacktracebuffer *stb;
	s4                dumpsize;

	/* mark start of dump memory area */

	dumpsize = dump_size();

	/* create a stacktrace for the passed thread */

	sfi = thread->_stackframeinfo;

	stb = stacktrace_create(sfi);

	/* print stacktrace */

	if (stb != NULL)
		stacktrace_print_trace_from_buffer(stb);
	else {
		puts("\t<<No stacktrace available>>");
		fflush(stdout);
	}

	dump_release(dumpsize);
}


/* threads_print_stacktrace ****************************************************

   Print the current stacktrace of the current thread.

*******************************************************************************/

void threads_print_stacktrace(void)
{
	threadobject *thread;

	thread = THREADOBJECT;

	threads_thread_print_stacktrace(thread);
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
