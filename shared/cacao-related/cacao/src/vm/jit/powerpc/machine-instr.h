#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

static inline void
atomic_add(int *mem, int val)
{
	int temp;

  __asm__ __volatile__ ("\n\
1:  lwarx  %0,0,%2 \n\
    add    %0,%0,%1 \n\
	stwcx. %0,0,%2 \n\
	bne-   1b \n\
"   : "=&r"(temp)
    : "r"(val), "r"(mem) : "cr0", "memory");
}

static inline long compare_and_swap(long *p, long oldval, long newval)
{
  long ret, temp;

  __asm__ __volatile__ ("\n\
1:  lwarx  %0,0,%4 \n\
    subf.  %1,%0,%2 \n\
    bne-   2f \n\
    or     %1,%3,%3 \n\
    stwcx. %1,0,%4 \n\
    bne-   1b \n\
2: \n\
"   : "=&r"(ret), "=&r"(temp)
    : "r"(oldval), "r"(newval), "r"(p) : "cr0", "memory");

  return ret;
}

#define STORE_ORDER_BARRIER() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER_BEFORE_ATOMIC() __asm__ __volatile__ ("sync" : : : "memory");
#define MEMORY_BARRIER_AFTER_ATOMIC() __asm__ __volatile__ ("isync" : : : "memory");
#define MEMORY_BARRIER() __asm__ __volatile__ ( "sync" : : : "memory" );

#endif
