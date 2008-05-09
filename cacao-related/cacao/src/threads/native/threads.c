/* src/threads/native/threads.c - native threads support

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

   $Id: threads.c 7885 2007-05-07 21:29:55Z twisti $

*/


#include "config.h"

/* XXX cleanup these includes */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <pthread.h>

#include "vm/types.h"

#include "arch.h"

#if !defined(USE_FAKE_ATOMIC_INSTRUCTIONS)
# include "machine-instr.h"
#else
# include "threads/native/generic-primitives.h"
#endif

#include "mm/gc-common.h"
#include "mm/memory.h"

#include "native/jni.h"
#include "native/native.h"
#include "native/include/java_lang_Object.h"
#include "native/include/java_lang_String.h"
#include "native/include/java_lang_Throwable.h"
#include "native/include/java_lang_Thread.h"

#if defined(ENABLE_JAVASE)
# include "native/include/java_lang_ThreadGroup.h"
#endif

#if defined(WITH_CLASSPATH_GNU)
# include "native/include/java_lang_VMThread.h"
#endif

#include "threads/lock-common.h"
#include "threads/threads-common.h"

#include "threads/native/threads.h"

#include "toolbox/avl.h"
#include "toolbox/logging.h"

#include "vm/builtin.h"
#include "vm/exceptions.h"
#include "vm/global.h"
#include "vm/stringlocal.h"
#include "vm/vm.h"

#include "vm/jit/asmpart.h"

#include "vmcore/options.h"

#if defined(ENABLE_STATISTICS)
# include "vmcore/statistics.h"
#endif

#if !defined(__DARWIN__)
# if defined(__LINUX__)
#  define GC_LINUX_THREADS
# elif defined(__MIPS__)
#  define GC_IRIX_THREADS
# endif
# include <semaphore.h>
# if defined(ENABLE_GC_BOEHM)
#  include "mm/boehm-gc/include/gc.h"
# endif
#endif

#if defined(ENABLE_JVMTI)
#include "native/jvmti/cacaodbg.h"
#endif

#if defined(__DARWIN__)
/* Darwin has no working semaphore implementation.  This one is taken
   from Boehm-GC. */

/*
   This is a very simple semaphore implementation for darwin. It
   is implemented in terms of pthreads calls so it isn't async signal
   safe. This isn't a problem because signals aren't used to
   suspend threads on darwin.
*/
   
static int sem_init(sem_t *sem, int pshared, int value)
{
	if (pshared)
		assert(0);

	sem->value = value;
    
	if (pthread_mutex_init(&sem->mutex, NULL) < 0)
		return -1;

	if (pthread_cond_init(&sem->cond, NULL) < 0)
		return -1;

	return 0;
}

static int sem_post(sem_t *sem)
{
	if (pthread_mutex_lock(&sem->mutex) < 0)
		return -1;

	sem->value++;

	if (pthread_cond_signal(&sem->cond) < 0) {
		pthread_mutex_unlock(&sem->mutex);
		return -1;
	}

	if (pthread_mutex_unlock(&sem->mutex) < 0)
		return -1;

	return 0;
}

static int sem_wait(sem_t *sem)
{
	if (pthread_mutex_lock(&sem->mutex) < 0)
		return -1;

	while (sem->value == 0) {
		pthread_cond_wait(&sem->cond, &sem->mutex);
	}

	sem->value--;

	if (pthread_mutex_unlock(&sem->mutex) < 0)
		return -1;    

	return 0;
}

static int sem_destroy(sem_t *sem)
{
	if (pthread_cond_destroy(&sem->cond) < 0)
		return -1;

	if (pthread_mutex_destroy(&sem->mutex) < 0)
		return -1;

	return 0;
}
#endif /* defined(__DARWIN__) */


/* internally used constants **************************************************/

/* CAUTION: Do not change these values. Boehm GC code depends on them.        */
#define STOPWORLD_FROM_GC               1
#define STOPWORLD_FROM_CLASS_NUMBERING  2


/* startupinfo *****************************************************************

   Struct used to pass info from threads_start_thread to 
   threads_startup_thread.

******************************************************************************/

typedef struct {
	threadobject *thread;      /* threadobject for this thread             */
	functionptr   function;    /* function to run in the new thread        */
	sem_t        *psem;        /* signals when thread has been entered     */
	                           /* in the thread list                       */
	sem_t        *psem_first;  /* signals when pthread_create has returned */
} startupinfo;


/* prototypes *****************************************************************/

static void threads_calc_absolute_time(struct timespec *tm, s8 millis, s4 nanos);


/******************************************************************************/
/* GLOBAL VARIABLES                                                           */
/******************************************************************************/

static methodinfo *method_thread_init;

/* the thread object of the current thread                                    */
/* This is either a thread-local variable defined with __thread, or           */
/* a thread-specific value stored with key threads_current_threadobject_key.  */
#if defined(HAVE___THREAD)
__thread threadobject *threads_current_threadobject;
#else
pthread_key_t threads_current_threadobject_key;
#endif

/* global mutex for the threads table */
static pthread_mutex_t mutex_threads_table;

/* global mutex for stop-the-world                                            */
static pthread_mutex_t stopworldlock;

/* global mutex and condition for joining threads on exit */
static pthread_mutex_t mutex_join;
static pthread_cond_t  cond_join;

/* this is one of the STOPWORLD_FROM_ constants, telling why the world is     */
/* being stopped                                                              */
static volatile int stopworldwhere;

/* semaphore used for acknowleding thread suspension                          */
static sem_t suspend_ack;
#if defined(__MIPS__)
static pthread_mutex_t suspend_ack_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t suspend_cond = PTHREAD_COND_INITIALIZER;
#endif

static pthread_attr_t threadattr;

/* mutexes used by the fake atomic instructions                               */
#if defined(USE_FAKE_ATOMIC_INSTRUCTIONS)
pthread_mutex_t _atomic_add_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t _cas_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t _mb_lock = PTHREAD_MUTEX_INITIALIZER;
#endif


/* threads_sem_init ************************************************************
 
   Initialize a semaphore. Checks against errors and interruptions.

   IN:
       sem..............the semaphore to initialize
	   shared...........true if this semaphore will be shared between processes
	   value............the initial value for the semaphore
   
*******************************************************************************/

void threads_sem_init(sem_t *sem, bool shared, int value)
{
	int r;

	assert(sem);

	do {
		r = sem_init(sem, shared, value);
		if (r == 0)
			return;
	} while (errno == EINTR);

	vm_abort("sem_init failed: %s", strerror(errno));
}


/* threads_sem_wait ************************************************************
 
   Wait for a semaphore, non-interruptible.

   IMPORTANT: Always use this function instead of `sem_wait` directly, as
              `sem_wait` may be interrupted by signals!
  
   IN:
       sem..............the semaphore to wait on
   
*******************************************************************************/

void threads_sem_wait(sem_t *sem)
{
	int r;

	assert(sem);

	do {
		r = sem_wait(sem);
		if (r == 0)
			return;
	} while (errno == EINTR);

	vm_abort("sem_wait failed: %s", strerror(errno));
}


/* threads_sem_post ************************************************************
 
   Increase the count of a semaphore. Checks for errors.

   IN:
       sem..............the semaphore to increase the count of
   
*******************************************************************************/

void threads_sem_post(sem_t *sem)
{
	int r;

	assert(sem);

	/* unlike sem_wait, sem_post is not interruptible */

	r = sem_post(sem);
	if (r == 0)
		return;

	vm_abort("sem_post failed: %s", strerror(errno));
}


/* lock_stopworld **************************************************************

   Enter the stopworld lock, specifying why the world shall be stopped.

   IN:
      where........ STOPWORLD_FROM_GC              (1) from within GC
                    STOPWORLD_FROM_CLASS_NUMBERING (2) class numbering

******************************************************************************/

void lock_stopworld(int where)
{
	pthread_mutex_lock(&stopworldlock);
	stopworldwhere = where;
}


/* unlock_stopworld ************************************************************

   Release the stopworld lock.

******************************************************************************/

void unlock_stopworld(void)
{
	stopworldwhere = 0;
	pthread_mutex_unlock(&stopworldlock);
}

#if !defined(__DARWIN__)
/* Caller must hold threadlistlock */
static void threads_cast_sendsignals(int sig)
{
	threadobject *t;
	threadobject *self;

	self = THREADOBJECT;

	/* iterate over all started threads */

	for (t = threads_table_first(); t != NULL; t = threads_table_next(t)) {
		/* don't send the signal to ourself */

		if (t != self)
			pthread_kill(t->tid, sig);
	}
}

#else

static void threads_cast_darwinstop(void)
{
	threadobject *tobj = mainthreadobj;
	threadobject *self = THREADOBJECT;

	do {
		if (tobj != self)
		{
			thread_state_flavor_t flavor = MACHINE_THREAD_STATE;
			mach_msg_type_number_t thread_state_count = MACHINE_THREAD_STATE_COUNT;
#if defined(__I386__)
			i386_thread_state_t thread_state;
#else
			ppc_thread_state_t thread_state;
#endif
			mach_port_t thread = tobj->mach_thread;
			kern_return_t r;

			r = thread_suspend(thread);

			if (r != KERN_SUCCESS)
				vm_abort("thread_suspend failed");

			r = thread_get_state(thread, flavor, (natural_t *) &thread_state,
								 &thread_state_count);

			if (r != KERN_SUCCESS)
				vm_abort("thread_get_state failed");

			md_critical_section_restart((ucontext_t *) &thread_state);

			r = thread_set_state(thread, flavor, (natural_t *) &thread_state,
								 thread_state_count);

			if (r != KERN_SUCCESS)
				vm_abort("thread_set_state failed");
		}

		tobj = tobj->next;
	} while (tobj != mainthreadobj);
}

static void threads_cast_darwinresume(void)
{
	threadobject *tobj = mainthreadobj;
	threadobject *self = THREADOBJECT;

	do {
		if (tobj != self)
		{
			mach_port_t thread = tobj->mach_thread;
			kern_return_t r;

			r = thread_resume(thread);

			if (r != KERN_SUCCESS)
				vm_abort("thread_resume failed");
		}

		tobj = tobj->next;
	} while (tobj != mainthreadobj);
}

#endif

#if defined(__MIPS__)
static void threads_cast_irixresume(void)
{
	pthread_mutex_lock(&suspend_ack_lock);
	pthread_cond_broadcast(&suspend_cond);
	pthread_mutex_unlock(&suspend_ack_lock);
}
#endif

#if !defined(DISABLE_GC)

void threads_cast_stopworld(void)
{
#if !defined(__DARWIN__) && !defined(__CYGWIN__)
	int count, i;
#endif

	lock_stopworld(STOPWORLD_FROM_CLASS_NUMBERING);

	/* lock the threads table */

	threads_table_lock();

#if defined(__DARWIN__)
	threads_cast_darwinstop();
#elif defined(__CYGWIN__)
	/* TODO */
	assert(0);
#else
	/* send all threads the suspend signal */

	threads_cast_sendsignals(GC_signum1());

	/* wait for all threads to suspend (except the current one) */

	count = threads_table_get_threads() - 1;

	for (i = 0; i < count; i++)
		threads_sem_wait(&suspend_ack);
#endif

	/* unlock the threads table */

	threads_table_unlock();
}

void threads_cast_startworld(void)
{
	/* lock the threads table */

	threads_table_lock();

#if defined(__DARWIN__)
	threads_cast_darwinresume();
#elif defined(__MIPS__)
	threads_cast_irixresume();
#elif defined(__CYGWIN__)
	/* TODO */
	assert(0);
#else
	threads_cast_sendsignals(GC_signum2());
#endif

	/* unlock the threads table */

	threads_table_unlock();

	unlock_stopworld();
}


#if !defined(__DARWIN__)
static void threads_sigsuspend_handler(ucontext_t *_uc)
{
	int sig;
	sigset_t sigs;

	/* XXX TWISTI: this is just a quick hack */
#if defined(ENABLE_JIT)
	md_critical_section_restart(_uc);
#endif

	/* Do as Boehm does. On IRIX a condition variable is used for wake-up
	   (not POSIX async-safe). */
#if defined(__IRIX__)
	pthread_mutex_lock(&suspend_ack_lock);
	threads_sem_post(&suspend_ack);
	pthread_cond_wait(&suspend_cond, &suspend_ack_lock);
	pthread_mutex_unlock(&suspend_ack_lock);
#elif defined(__CYGWIN__)
	/* TODO */
	assert(0);
#else
	threads_sem_post(&suspend_ack);

	sig = GC_signum2();
	sigfillset(&sigs);
	sigdelset(&sigs, sig);
	sigsuspend(&sigs);
#endif
}

/* This function is called from Boehm GC code. */

int cacao_suspendhandler(ucontext_t *_uc)
{
	if (stopworldwhere != STOPWORLD_FROM_CLASS_NUMBERING)
		return 0;

	threads_sigsuspend_handler(_uc);
	return 1;
}
#endif

#endif /* DISABLE_GC */


/* threads_set_current_threadobject ********************************************

   Set the current thread object.
   
   IN:
      thread.......the thread object to set

*******************************************************************************/

void threads_set_current_threadobject(threadobject *thread)
{
#if !defined(HAVE___THREAD)
	if (pthread_setspecific(threads_current_threadobject_key, thread) != 0)
		vm_abort("threads_set_current_threadobject: pthread_setspecific failed: %s", strerror(errno));
#else
	threads_current_threadobject = thread;
#endif
}


/* threads_init_threadobject **************************************************

   Initialize implementation fields of a threadobject.

   IN:
      thread............the threadobject

******************************************************************************/

void threads_init_threadobject(threadobject *thread)
{
	/* get the pthread id */

	thread->tid = pthread_self();

	thread->index = 0;

	/* TODO destroy all those things */

	pthread_mutex_init(&(thread->waitmutex), NULL);
	pthread_cond_init(&(thread->waitcond), NULL);

	thread->interrupted = false;
	thread->signaled    = false;
	thread->sleeping    = false;
}


/* threads_get_current_threadobject ********************************************

   Return the threadobject of the current thread.
   
   RETURN VALUE:
       the current threadobject * (an instance of java.lang.Thread)

*******************************************************************************/

threadobject *threads_get_current_threadobject(void)
{
	return THREADOBJECT;
}


/* threads_impl_preinit ********************************************************

   Do some early initialization of stuff required.

   ATTENTION: Do NOT use any Java heap allocation here, as gc_init()
   is called AFTER this function!

*******************************************************************************/

void threads_impl_preinit(void)
{
	pthread_mutex_init(&stopworldlock, NULL);

	/* initialize exit mutex and condition (on exit we join all
	   threads) */

	pthread_mutex_init(&mutex_join, NULL);
	pthread_cond_init(&cond_join, NULL);

#if !defined(HAVE___THREAD)
	pthread_key_create(&threads_current_threadobject_key, NULL);
#endif

	threads_sem_init(&suspend_ack, 0, 0);
}


/* threads_table_lock **********************************************************

   Initialize threads table mutex.

*******************************************************************************/

void threads_impl_table_init(void)
{
	pthread_mutex_init(&mutex_threads_table, NULL);
}


/* threads_table_lock **********************************************************

   Enter the threads table mutex.

   NOTE: We need this function as we can't use an internal lock for
         the threads table because the thread's lock is initialized in
         threads_table_add (when we have the thread index), but we
         already need the lock at the entry of the function.

*******************************************************************************/

void threads_table_lock(void)
{
	if (pthread_mutex_lock(&mutex_threads_table) != 0)
		vm_abort("threads_table_lock: pthread_mutex_lock failed: %s",
				 strerror(errno));
}


/* threads_table_unlock ********************************************************

   Leave the threads table mutex.

*******************************************************************************/

void threads_table_unlock(void)
{
	if (pthread_mutex_unlock(&mutex_threads_table) != 0)
		vm_abort("threads_table_unlock: pthread_mutex_unlock failed: %s",
				 strerror(errno));
}


/* threads_init ****************************************************************

   Initializes the threads required by the JVM: main, finalizer.

*******************************************************************************/

bool threads_init(void)
{
	threadobject          *mainthread;
	java_objectheader     *threadname;
	java_lang_Thread      *t;
	java_objectheader     *o;

#if defined(ENABLE_JAVASE)
	java_lang_ThreadGroup *threadgroup;
	methodinfo            *m;
#endif

#if defined(WITH_CLASSPATH_GNU)
	java_lang_VMThread    *vmt;
#endif

	/* get methods we need in this file */

#if defined(WITH_CLASSPATH_GNU)
	method_thread_init =
		class_resolveclassmethod(class_java_lang_Thread,
								 utf_init,
								 utf_new_char("(Ljava/lang/VMThread;Ljava/lang/String;IZ)V"),
								 class_java_lang_Thread,
								 true);
#else
	method_thread_init =
		class_resolveclassmethod(class_java_lang_Thread,
								 utf_init,
								 utf_new_char("(Ljava/lang/String;)V"),
								 class_java_lang_Thread,
								 true);
#endif

	if (method_thread_init == NULL)
		return false;

	/* Get the main-thread (NOTE: The main threads is always the first
	   thread in the table). */

	mainthread = threads_table_first();

	/* create a java.lang.Thread for the main thread */

	t = (java_lang_Thread *) builtin_new(class_java_lang_Thread);

	if (t == NULL)
		return false;

	/* set the object in the internal data structure */

	mainthread->object = t;

#if defined(ENABLE_INTRP)
	/* create interpreter stack */

	if (opt_intrp) {
		MSET(intrp_main_stack, 0, u1, opt_stacksize);
		mainthread->_global_sp = (Cell*) (intrp_main_stack + opt_stacksize);
	}
#endif

	threadname = javastring_new(utf_new_char("main"));

#if defined(ENABLE_JAVASE)
	/* allocate and init ThreadGroup */

	threadgroup = (java_lang_ThreadGroup *)
		native_new_and_init(class_java_lang_ThreadGroup);

	if (threadgroup == NULL)
		return false;
#endif

#if defined(WITH_CLASSPATH_GNU)
	/* create a java.lang.VMThread for the main thread */

	vmt = (java_lang_VMThread *) builtin_new(class_java_lang_VMThread);

	if (vmt == NULL)
		return false;

	/* set the thread */

	vmt->thread = t;
	vmt->vmdata = (java_lang_Object *) mainthread;

	/* call java.lang.Thread.<init>(Ljava/lang/VMThread;Ljava/lang/String;IZ)V */
	o = (java_objectheader *) t;

	(void) vm_call_method(method_thread_init, o, vmt, threadname, NORM_PRIORITY,
						  false);
#elif defined(WITH_CLASSPATH_CLDC1_1)
	/* set the thread */

	t->vm_thread = (java_lang_Object *) mainthread;

	/* call public Thread(String name) */

	o = (java_objectheader *) t;

	(void) vm_call_method(method_thread_init, o, threadname);
#endif

	if (*exceptionptr)
		return false;

#if defined(ENABLE_JAVASE)
	t->group = threadgroup;

	/* add main thread to java.lang.ThreadGroup */

	m = class_resolveclassmethod(class_java_lang_ThreadGroup,
								 utf_addThread,
								 utf_java_lang_Thread__V,
								 class_java_lang_ThreadGroup,
								 true);

	o = (java_objectheader *) threadgroup;

	(void) vm_call_method(m, o, t);

	if (*exceptionptr)
		return false;
#endif

	threads_set_thread_priority(pthread_self(), NORM_PRIORITY);

	/* initialize the thread attribute object */

	if (pthread_attr_init(&threadattr)) {
		log_println("pthread_attr_init failed: %s", strerror(errno));
		return false;
	}

	pthread_attr_setdetachstate(&threadattr, PTHREAD_CREATE_DETACHED);

	/* everything's ok */

	return true;
}


/* threads_startup_thread ******************************************************

   Thread startup function called by pthread_create.

   Thread which have a startup.function != NULL are marked as internal
   threads. All other threads are threated as normal Java threads.

   NOTE: This function is not called directly by pthread_create. The Boehm GC
         inserts its own GC_start_routine in between, which then calls
		 threads_startup.

   IN:
      t............the argument passed to pthread_create, ie. a pointer to
	               a startupinfo struct. CAUTION: When the `psem` semaphore
				   is posted, the startupinfo struct becomes invalid! (It
				   is allocated on the stack of threads_start_thread.)

******************************************************************************/

static void *threads_startup_thread(void *t)
{
	startupinfo        *startup;
	threadobject       *thread;
#if defined(WITH_CLASSPATH_GNU)
	java_lang_VMThread *vmt;
#endif
	sem_t              *psem;
	classinfo          *c;
	methodinfo         *m;
	java_objectheader  *o;
	functionptr         function;

#if defined(ENABLE_INTRP)
	u1 *intrp_thread_stack;

	/* create interpreter stack */

	if (opt_intrp) {
		intrp_thread_stack = GCMNEW(u1, opt_stacksize);
		MSET(intrp_thread_stack, 0, u1, opt_stacksize);
	}
	else
		intrp_thread_stack = NULL;
#endif

	/* get passed startupinfo structure and the values in there */

	startup = t;
	t = NULL; /* make sure it's not used wrongly */

	thread   = startup->thread;
	function = startup->function;
	psem     = startup->psem;

	/* Seems like we've encountered a situation where thread->tid was not set by
	 * pthread_create. We alleviate this problem by waiting for pthread_create
	 * to return. */
	threads_sem_wait(startup->psem_first);

#if defined(__DARWIN__)
	thread->mach_thread = mach_thread_self();
#endif

	/* store the internal thread data-structure in the TSD */

	threads_set_current_threadobject(thread);

	/* thread is running */

	thread->state = THREAD_STATE_RUNNABLE;

	/* insert the thread into the threads table */

	threads_table_add(thread);

	/* tell threads_startup_thread that we registered ourselves */
	/* CAUTION: *startup becomes invalid with this!             */

	startup = NULL;
	threads_sem_post(psem);

	/* set our priority */

	threads_set_thread_priority(thread->tid, thread->object->priority);

#if defined(ENABLE_INTRP)
	/* set interpreter stack */

	if (opt_intrp)
		thread->_global_sp = (Cell *) (intrp_thread_stack + opt_stacksize);
#endif

#if defined(ENABLE_JVMTI)
	/* fire thread start event */

	if (jvmti) 
		jvmti_ThreadStartEnd(JVMTI_EVENT_THREAD_START);
#endif

	/* find and run the Thread.run()V method if no other function was passed */

	if (function == NULL) {
		/* this is a normal Java thread */

		thread->flags |= THREAD_FLAG_JAVA;

#if defined(WITH_CLASSPATH_GNU)
		/* We need to start the run method of
		   java.lang.VMThread. Since this is a final class, we can use
		   the class object directly. */

		c   = class_java_lang_VMThread;
#elif defined(WITH_CLASSPATH_CLDC1_1)
		c   = thread->object->header.vftbl->class;
#endif

		m = class_resolveclassmethod(c, utf_run, utf_void__void, c, true);

		if (m == NULL)
			vm_abort("threads_startup_thread: run() method not found in class");

		/* set ThreadMXBean variables */

		_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount++;
		_Jv_jvm->java_lang_management_ThreadMXBean_TotalStartedThreadCount++;

		if (_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount >
			_Jv_jvm->java_lang_management_ThreadMXBean_PeakThreadCount)
			_Jv_jvm->java_lang_management_ThreadMXBean_PeakThreadCount =
				_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount;

#if defined(WITH_CLASSPATH_GNU)
		/* we need to start the run method of java.lang.VMThread */

		vmt = (java_lang_VMThread *) thread->object->vmThread;
		o   = (java_objectheader *) vmt;

#elif defined(WITH_CLASSPATH_CLDC1_1)
		o   = (java_objectheader *) thread->object;
#endif

		/* run the thread */

		(void) vm_call_method(m, o);
	}
	else {
		/* this is an internal thread */

		thread->flags |= THREAD_FLAG_INTERNAL;

		/* set ThreadMXBean variables */

		_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount++;
		_Jv_jvm->java_lang_management_ThreadMXBean_TotalStartedThreadCount++;

		if (_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount >
			_Jv_jvm->java_lang_management_ThreadMXBean_PeakThreadCount)
			_Jv_jvm->java_lang_management_ThreadMXBean_PeakThreadCount =
				_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount;

		/* call passed function, e.g. finalizer_thread */

		(function)();
	}

#if defined(ENABLE_JVMTI)
	/* fire thread end event */

	if (jvmti)
		jvmti_ThreadStartEnd(JVMTI_EVENT_THREAD_END);
#endif

	if (!threads_detach_thread(thread))
		vm_abort("threads_startup_thread: threads_detach_thread failed");

	/* set ThreadMXBean variables */

	_Jv_jvm->java_lang_management_ThreadMXBean_ThreadCount--;

	return NULL;
}


/* threads_impl_thread_start ***************************************************

   Start a thread in the JVM.  Both (vm internal and java) thread
   objects exist.

   IN:
      thread....the thread object
	  f.........function to run in the new thread. NULL means that the
	            "run" method of the object `t` should be called

******************************************************************************/

void threads_impl_thread_start(threadobject *thread, functionptr f)
{
	sem_t          sem;
	sem_t          sem_first;
	pthread_attr_t attr;
	startupinfo    startup;

	/* fill startupinfo structure passed by pthread_create to
	 * threads_startup_thread */

	startup.thread     = thread;
	startup.function   = f;              /* maybe we don't call Thread.run()V */
	startup.psem       = &sem;
	startup.psem_first = &sem_first;

	threads_sem_init(&sem, 0, 0);
	threads_sem_init(&sem_first, 0, 0);

	/* initialize thread attribute object */

	if (pthread_attr_init(&attr))
		vm_abort("pthread_attr_init failed: %s", strerror(errno));

	/* initialize thread stacksize */

	if (pthread_attr_setstacksize(&attr, opt_stacksize))
		vm_abort("pthread_attr_setstacksize failed: %s", strerror(errno));

	/* create the thread */

	if (pthread_create(&(thread->tid), &attr, threads_startup_thread, &startup))
		vm_abort("pthread_create failed: %s", strerror(errno));

	/* signal that pthread_create has returned, so thread->tid is valid */

	threads_sem_post(&sem_first);

	/* wait here until the thread has entered itself into the thread list */

	threads_sem_wait(&sem);

	/* cleanup */

	sem_destroy(&sem);
	sem_destroy(&sem_first);
}


/* threads_set_thread_priority *************************************************

   Set the priority of the given thread.

   IN:
      tid..........thread id
	  priority.....priority to set

******************************************************************************/

void threads_set_thread_priority(pthread_t tid, int priority)
{
	struct sched_param schedp;
	int policy;

	pthread_getschedparam(tid, &policy, &schedp);
	schedp.sched_priority = priority;
	pthread_setschedparam(tid, policy, &schedp);
}


/* threads_attach_current_thread ***********************************************

   Attaches the current thread to the VM.  Used in JNI.

*******************************************************************************/

bool threads_attach_current_thread(JavaVMAttachArgs *vm_aargs, bool isdaemon)
{
	threadobject          *thread;
	utf                   *u;
	java_objectheader     *s;
	java_objectheader     *o;
	java_lang_Thread      *t;

#if defined(ENABLE_JAVASE)
	java_lang_ThreadGroup *group;
	threadobject          *mainthread;
	methodinfo            *m;
#endif

#if defined(WITH_CLASSPATH_GNU)
	java_lang_VMThread    *vmt;
#endif

	/* create internal thread data-structure */

	thread = threads_create_thread();

	/* create a java.lang.Thread object */

	t = (java_lang_Thread *) builtin_new(class_java_lang_Thread);

	if (t == NULL)
		return false;

	thread->object = t;

	/* thread is a Java thread and running */

	thread->flags = THREAD_FLAG_JAVA;

	if (isdaemon)
		thread->flags |= THREAD_FLAG_DAEMON;

	thread->state = THREAD_STATE_RUNNABLE;

	/* insert the thread into the threads table */

	threads_table_add(thread);

#if defined(ENABLE_INTRP)
	/* create interpreter stack */

	if (opt_intrp) {
		MSET(intrp_main_stack, 0, u1, opt_stacksize);
		thread->_global_sp = (Cell *) (intrp_main_stack + opt_stacksize);
	}
#endif

#if defined(WITH_CLASSPATH_GNU)
	/* create a java.lang.VMThread object */

	vmt = (java_lang_VMThread *) builtin_new(class_java_lang_VMThread);

	if (vmt == NULL)
		return false;

	/* set the thread */

	vmt->thread = t;
	vmt->vmdata = (java_lang_Object *) thread;
#elif defined(WITH_CLASSPATH_CLDC1_1)
	t->vm_thread = (java_lang_Object *) thread;
#endif

	if (vm_aargs != NULL) {
		u     = utf_new_char(vm_aargs->name);
#if defined(ENABLE_JAVASE)
		group = (java_lang_ThreadGroup *) vm_aargs->group;
#endif
	}
	else {
		u     = utf_null;
#if defined(ENABLE_JAVASE)
		/* get the main thread */

		mainthread = threads_table_first();
		group = mainthread->object->group;
#endif
	}

	/* the the thread name */

	s = javastring_new(u);

	/* for convenience */

	o = (java_objectheader *) thread->object;

#if defined(WITH_CLASSPATH_GNU)
	(void) vm_call_method(method_thread_init, o, vmt, s, NORM_PRIORITY,
						  isdaemon);
#elif defined(WITH_CLASSPATH_CLDC1_1)
	(void) vm_call_method(method_thread_init, o, s);
#endif

	if (*exceptionptr)
		return false;

#if defined(ENABLE_JAVASE)
	/* store the thread group in the object */

	thread->object->group = group;

	/* add thread to given thread-group */

	m = class_resolveclassmethod(group->header.vftbl->class,
								 utf_addThread,
								 utf_java_lang_Thread__V,
								 class_java_lang_ThreadGroup,
								 true);

	o = (java_objectheader *) group;

	(void) vm_call_method(m, o, t);

	if (*exceptionptr)
		return false;
#endif

	return true;
}


/* threads_detach_thread *******************************************************

   Detaches the passed thread from the VM.  Used in JNI.

*******************************************************************************/

bool threads_detach_thread(threadobject *thread)
{
#if defined(ENABLE_JAVASE)
	java_lang_ThreadGroup *group;
	methodinfo            *m;
	java_objectheader     *o;
	java_lang_Thread      *t;
#endif

	/* Allow lock record pools to be used by other threads. They
	   cannot be deleted so we'd better not waste them. */

	/* XXX We have to find a new way to free lock records */
	/*     with the new locking algorithm.                */
	/* lock_record_free_pools(thread->ee.lockrecordpools); */

	/* XXX implement uncaught exception stuff (like JamVM does) */

#if defined(ENABLE_JAVASE)
	/* remove thread from the thread group */

	group = thread->object->group;

	/* XXX TWISTI: should all threads be in a ThreadGroup? */

	if (group != NULL) {
		m = class_resolveclassmethod(group->header.vftbl->class,
									 utf_removeThread,
									 utf_java_lang_Thread__V,
									 class_java_lang_ThreadGroup,
									 true);

		if (m == NULL)
			return false;

		o = (java_objectheader *) group;
		t = thread->object;

		(void) vm_call_method(m, o, t);

		if (*exceptionptr)
			return false;
	}
#endif

	/* thread is terminated */

	thread->state = THREAD_STATE_TERMINATED;

	/* remove thread from the threads table */

	threads_table_remove(thread);

	/* signal that this thread has finished */

	pthread_mutex_lock(&mutex_join);
	pthread_cond_signal(&cond_join);
	pthread_mutex_unlock(&mutex_join);

	/* free the vm internal thread object */

#if defined(ENABLE_GC_BOEHM)
	GCFREE(thread);
#else
	FREE(thread, threadobject);
#endif

#if defined(ENABLE_STATISTICS)
	if (opt_stat)
		size_threadobject -= sizeof(threadobject);
#endif

	return true;
}


/* threads_join_all_threads ****************************************************

   Join all non-daemon threads.

*******************************************************************************/

void threads_join_all_threads(void)
{
	threadobject *thread;

	/* get current thread */

	thread = THREADOBJECT;

	/* this thread is waiting for all non-daemon threads to exit */

	thread->state = THREAD_STATE_WAITING;

	/* enter join mutex */

	pthread_mutex_lock(&mutex_join);

	/* Wait for condition as long as we have non-daemon threads.  We
	   compare against 1 because the current (main thread) is also a
	   non-daemon thread. */

	while (threads_table_get_non_daemons() > 1)
		pthread_cond_wait(&cond_join, &mutex_join);

	/* leave join mutex */

	pthread_mutex_unlock(&mutex_join);
}


/* threads_timespec_earlier ****************************************************

   Return true if timespec tv1 is earlier than timespec tv2.

   IN:
      tv1..........first timespec
	  tv2..........second timespec

   RETURN VALUE:
      true, if the first timespec is earlier

*******************************************************************************/

static inline bool threads_timespec_earlier(const struct timespec *tv1,
											const struct timespec *tv2)
{
	return (tv1->tv_sec < tv2->tv_sec)
				||
		(tv1->tv_sec == tv2->tv_sec && tv1->tv_nsec < tv2->tv_nsec);
}


/* threads_current_time_is_earlier_than ****************************************

   Check if the current time is earlier than the given timespec.

   IN:
      tv...........the timespec to compare against

   RETURN VALUE:
      true, if the current time is earlier

*******************************************************************************/

static bool threads_current_time_is_earlier_than(const struct timespec *tv)
{
	struct timeval tvnow;
	struct timespec tsnow;

	/* get current time */

	if (gettimeofday(&tvnow, NULL) != 0)
		vm_abort("gettimeofday failed: %s\n", strerror(errno));

	/* convert it to a timespec */

	tsnow.tv_sec = tvnow.tv_sec;
	tsnow.tv_nsec = tvnow.tv_usec * 1000;

	/* compare current time with the given timespec */

	return threads_timespec_earlier(&tsnow, tv);
}


/* threads_wait_with_timeout ***************************************************

   Wait until the given point in time on a monitor until either
   we are notified, we are interrupted, or the time is up.

   IN:
      t............the current thread
	  wakeupTime...absolute (latest) wakeup time
	                   If both tv_sec and tv_nsec are zero, this function
					   waits for an unlimited amount of time.

   RETURN VALUE:
      true.........if the wait has been interrupted,
	  false........if the wait was ended by notification or timeout

*******************************************************************************/

static bool threads_wait_with_timeout(threadobject *thread,
									  struct timespec *wakeupTime)
{
	bool wasinterrupted;

	/* acquire the waitmutex */

	pthread_mutex_lock(&thread->waitmutex);

	/* mark us as sleeping */

	thread->sleeping = true;

	/* wait on waitcond */

	if (wakeupTime->tv_sec || wakeupTime->tv_nsec) {
		/* with timeout */
		while (!thread->interrupted && !thread->signaled
			   && threads_current_time_is_earlier_than(wakeupTime))
		{
			thread->state = THREAD_STATE_TIMED_WAITING;

			pthread_cond_timedwait(&thread->waitcond, &thread->waitmutex,
								   wakeupTime);

			thread->state = THREAD_STATE_RUNNABLE;
		}
	}
	else {
		/* no timeout */
		while (!thread->interrupted && !thread->signaled) {
			thread->state = THREAD_STATE_WAITING;

			pthread_cond_wait(&thread->waitcond, &thread->waitmutex);

			thread->state = THREAD_STATE_RUNNABLE;
		}
	}

	/* check if we were interrupted */

	wasinterrupted = thread->interrupted;

	/* reset all flags */

	thread->interrupted = false;
	thread->signaled    = false;
	thread->sleeping    = false;

	/* release the waitmutex */

	pthread_mutex_unlock(&thread->waitmutex);

	return wasinterrupted;
}


/* threads_wait_with_timeout_relative ******************************************

   Wait for the given maximum amount of time on a monitor until either
   we are notified, we are interrupted, or the time is up.

   IN:
      t............the current thread
	  millis.......milliseconds to wait
	  nanos........nanoseconds to wait

   RETURN VALUE:
      true.........if the wait has been interrupted,
	  false........if the wait was ended by notification or timeout

*******************************************************************************/

bool threads_wait_with_timeout_relative(threadobject *thread, s8 millis,
										s4 nanos)
{
	struct timespec wakeupTime;

	/* calculate the the (latest) wakeup time */

	threads_calc_absolute_time(&wakeupTime, millis, nanos);

	/* wait */

	return threads_wait_with_timeout(thread, &wakeupTime);
}


/* threads_calc_absolute_time **************************************************

   Calculate the absolute point in time a given number of ms and ns from now.

   IN:
      millis............milliseconds from now
	  nanos.............nanoseconds from now

   OUT:
      *tm...............receives the timespec of the absolute point in time

*******************************************************************************/

static void threads_calc_absolute_time(struct timespec *tm, s8 millis, s4 nanos)
{
	if ((millis != 0x7fffffffffffffffLLU) && (millis || nanos)) {
		struct timeval tv;
		long nsec;
		gettimeofday(&tv, NULL);
		tv.tv_sec += millis / 1000;
		millis %= 1000;
		nsec = tv.tv_usec * 1000 + (s4) millis * 1000000 + nanos;
		tm->tv_sec = tv.tv_sec + nsec / 1000000000;
		tm->tv_nsec = nsec % 1000000000;
	}
	else {
		tm->tv_sec = 0;
		tm->tv_nsec = 0;
	}
}


/* threads_thread_interrupt ****************************************************

   Interrupt the given thread.

   The thread gets the "waitcond" signal and 
   its interrupted flag is set to true.

   IN:
      thread............the thread to interrupt

*******************************************************************************/

void threads_thread_interrupt(threadobject *thread)
{
	/* Signal the thread a "waitcond" and tell it that it has been
	   interrupted. */

	pthread_mutex_lock(&thread->waitmutex);

	/* Interrupt blocking system call using a signal. */

	pthread_kill(thread->tid, SIGHUP);

	if (thread->sleeping)
		pthread_cond_signal(&thread->waitcond);

	thread->interrupted = true;

	pthread_mutex_unlock(&thread->waitmutex);
}


/* threads_check_if_interrupted_and_reset **************************************

   Check if the current thread has been interrupted and reset the
   interruption flag.

   RETURN VALUE:
      true, if the current thread had been interrupted

*******************************************************************************/

bool threads_check_if_interrupted_and_reset(void)
{
	threadobject *thread;
	bool intr;

	thread = THREADOBJECT;

	/* get interrupted flag */

	intr = thread->interrupted;

	/* reset interrupted flag */

	thread->interrupted = false;

	return intr;
}


/* threads_thread_has_been_interrupted *****************************************

   Check if the given thread has been interrupted

   IN:
      t............the thread to check

   RETURN VALUE:
      true, if the given thread had been interrupted

*******************************************************************************/

bool threads_thread_has_been_interrupted(threadobject *thread)
{
	return thread->interrupted;
}


/* threads_sleep ***************************************************************

   Sleep the current thread for the specified amount of time.

*******************************************************************************/

void threads_sleep(s8 millis, s4 nanos)
{
	threadobject    *thread;
	struct timespec  wakeupTime;
	bool             wasinterrupted;

	thread = THREADOBJECT;

	threads_calc_absolute_time(&wakeupTime, millis, nanos);

	wasinterrupted = threads_wait_with_timeout(thread, &wakeupTime);

	if (wasinterrupted)
		exceptions_throw_interruptedexception();
}


/* threads_yield ***************************************************************

   Yield to the scheduler.

*******************************************************************************/

void threads_yield(void)
{
	sched_yield();
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
