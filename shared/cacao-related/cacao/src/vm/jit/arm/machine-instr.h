#ifndef _MACHINE_INSTR_H
#define _MACHINE_INSTR_H

static inline void atomic_add(int *mem, int val)
{
	int temp, temp2, temp3;
	/*dolog("atomic_add(%p [%d], %d)", mem, *mem, val);*/

	/* TODO: improve this one! */
        __asm__ __volatile__ (
		"1:\t"
		"ldr   %0,[%3]\n\t"
		"add   %1,%0,%4\n\t"
		"swp   %2,%1,[%3]\n\t"
		"cmp   %0,%2\n\t"
		"swpne %1,%2,[%3]\n\t"
		"bne   1b"
		: "=&r" (temp), "=&r" (temp2), "=&r" (temp3)
		: "r" (mem), "r"(val)
		: "cc", "memory"
	);

	/*dolog("atomic_add() mem=%d", *mem);*/
}

static inline long compare_and_swap(long *p, long oldval, long newval)
{
	long ret, temp;
	/*dolog("compare_and_swap(%p [%d], %d, %d)", p, *p, oldval, newval);*/

	/* TODO: improve this one! */
	__asm__ __volatile__ (
		"1:\t"
		"ldr   %0,[%2]\n\t"
		"cmp   %0,%4\n\t"
		"bne   2f\n\t"
		"swp   %1,%3,[%2]\n\t"
		"cmp   %1,%0\n\t"
		"swpne %0,%1,[%2]\n\t"
		"bne   1b\n\t"
		"2:"
		: "=&r" (ret), "=&r" (temp)
		: "r" (p), "r" (newval), "r" (oldval)
		: "cc", "memory"
	);

	/*dolog("compare_and_swap() return=%d mem=%d", ret, *p);*/
	return ret;
}

#define STORE_ORDER_BARRIER() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER_BEFORE_ATOMIC() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER_AFTER_ATOMIC() __asm__ __volatile__ ("" : : : "memory");
#define MEMORY_BARRIER() __asm__ __volatile__ ("" : : : "memory" );

#endif
