#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

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

static inline long
__attribute__ ((unused))
compare_and_swap (volatile long *p, long oldval, long newval)
{
  long ret, temp;

  __asm__ __volatile__ (
    "1:\t"
    "ldq_l  %0,%5\n\t"
    "cmpeq  %0,%3,%2\n\t"
    "beq    %2,2f\n\t"
    "mov    %4,%2\n\t"
    "stq_c  %2,%1\n\t"
    "beq    %2,1b\n\t"
    "2:\t"
    : "=&r"(ret), "=m"(*p), "=&r"(temp)
    : "r"(oldval), "r"(newval), "m"(*p));

  return ret;
}

#define STORE_ORDER_BARRIER() __asm__ __volatile__ ("wmb" : : : "memory");
#define MEMORY_BARRIER_BEFORE_ATOMIC() __asm__ __volatile__ ("mb" : : : "memory");
#define MEMORY_BARRIER_AFTER_ATOMIC() __asm__ __volatile__ ("mb" : : : "memory");
#define MEMORY_BARRIER() __asm__ __volatile__ ( \
		"mb" : : : "memory" );

#endif
