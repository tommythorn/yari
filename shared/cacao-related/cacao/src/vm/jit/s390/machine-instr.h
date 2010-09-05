#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

/*  Taken from linux kernel source 
 *  include/asm-s390/atomic.h
 *
 *  S390 version
 *    Copyright (C) 1999-2005 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Denis Joseph Barrow,
 *		 Arnd Bergmann (arndb@de.ibm.com)
 *
 *  Derived from "include/asm-i386/bitops.h"
 *    Copyright (C) 1992, Linus Torvalds
 */

#define __CS_LOOP(ptr, op_val, op_string) ({				\
	int old_val, new_val;				\
        __asm__ __volatile__("   l     %0,0(%3)\n"			\
                             "0: lr    %1,%0\n"				\
                             op_string "  %1,%4\n"			\
                             "   cs    %0,%1,0(%3)\n"			\
                             "   jl    0b"				\
                             : "=&d" (old_val), "=&d" (new_val),	\
			       "=m" (*ptr)	\
			     : "a" (ptr), "d" (op_val),			\
			       "m" (*ptr)	\
			     : "cc", "memory" );			\
	new_val;							\
})

static inline void
atomic_add (volatile int *mem, int val)
{
	__CS_LOOP(mem, val, "ar");
}

static inline long
compare_and_swap (volatile long *p, long oldval, long newval)
{
	__asm__ __volatile__("  cs   %0,%3,0(%2)\n"
			     : "+d" (oldval), "=m" (*p)
			     : "a" (p), "d" (newval), "m" (*p)
			     : "cc", "memory" );
	return oldval;
}

/*
 *  Taken from linux kerenl source
 *  include/asm-s390/system.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *
 *  Derived from "include/asm-i386/system.h"
 */

/*
 * Force strict CPU ordering.
 * And yes, this is required on UP too when we're talking
 * to devices.
 *
 * This is very similar to the ppc eieio/sync instruction in that is
 * does a checkpoint syncronisation & makes sure that 
 * all memory ops have completed wrt other CPU's ( see 7-15 POP  DJB ).
 */

#define eieio()  __asm__ __volatile__ ( "bcr 15,0" : : : "memory" ) 

#define STORE_ORDER_BARRIER() eieio()
#define MEMORY_BARRIER() eieio()

/* TODO not sure if the following two can't be just empty. */

#define MEMORY_BARRIER_BEFORE_ATOMIC() eieio()
#define MEMORY_BARRIER_AFTER_ATOMIC() eieio()

#endif
