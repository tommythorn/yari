#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

#include "toolbox/logging.h"

/*
static inline void
__attribute__ ((unused))
atomic_add (volatile int *mem, int val)
{
    int temp;

  __asm__ __volatile__ (
    "1:\t"
    "ldl_l  %1,%3\n\t"
    "addl   %1,%2,%1\n\t"
    "stl_c  %1,%0\n\t"
    "beq    %1,1b\n\t"
    : "=m"(*mem), "=&r"(temp)
    : "r"(val), "m"(*mem));
}
*/

static inline long
__attribute__ ((unused))
compare_and_swap (volatile long *p, long oldval, long newval)
{
  long ret;
  /*dolog("compare_and_swap(%p [%d], %d, %d)", p, *p, oldval, newval);*/

  __asm__ __volatile__ (
    "mov %3,%0\n\t"
    "casx %1,%2,%0\n\t"
    : "=&r"(ret), "=m"(*p) 
    : "r"(oldval), "r"(newval), "m"(*p));

  /*dolog("compare_and_swap() return=%d mem=%d", ret, *p);*/
  return ret;
}

#define STORE_ORDER_BARRIER() __asm__ __volatile__ ("wmb" : : : "memory");
#define MEMORY_BARRIER_BEFORE_ATOMIC() __asm__ __volatile__ ("mb" : : : "memory");
#define MEMORY_BARRIER_AFTER_ATOMIC() __asm__ __volatile__ ("mb" : : : "memory");
#define MEMORY_BARRIER() __asm__ __volatile__ ( \
		"membar 0x0F" : : : "memory" );

#endif
