#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

#if 0

/* If anyone wants to compile with gcc, use this section.
 * It is not usable with the MIPSPro compiler.
 *
 * It is outdated, too.
 */ 

static inline long
__attribute__ ((unused))
atomic_swap (volatile long *p, long val)
{
  long ret, temp;

  __asm__ __volatile__
    ("1:\n\t"
     ".set  push\n\t"
     ".set  mips2\n\t"
     "lld   %2,%4\n\t"
     "move  %0,%3\n\t"
     "scd   %0,%1\n\t"
     ".set  pop\n\t"
     "beqz  %0,1b\n"
     : "=&r" (temp), "=m" (*p), "=&r" (ret)
     : "r" (val), "m" (*p)
     : "memory");

  return ret;
}
static inline int
__attribute__ ((unused))
compare_and_swap (volatile long *p, long oldval, long newval)
{
  long ret, temp;

  __asm__ __volatile__
    ("1:\n\t"
     ".set  push\n\t"
     ".set  mips2\n\t"
     "lld   %1,%5\n\t"
     "move  %0,$0\n\t"
     "bne   %1,%3,2f\n\t"
     "move  %0,%4\n\t"
     "scd   %0,%2\n\t"
     ".set  pop\n\t"
     "beqz  %0,1b\n"
     "2:\n\t"
     : "=&r" (ret), "=&r" (temp), "=m" (*p)
     : "r" (oldval), "r" (newval), "m" (*p)
     : "memory");

  return ret;
}

#else

static inline void
atomic_add(int *mem, int val)
{
	*mem += val;
}

long compare_and_swap (long *p, long oldval, long newval);

#define STORE_ORDER_BARRIER()
#define MEMORY_BARRIER_BEFORE_ATOMIC()
#define MEMORY_BARRIER_AFTER_ATOMIC()
#define MEMORY_BARRIER()

#endif
#endif
