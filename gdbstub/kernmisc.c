/*  stuff from the kenel code that gdb-stub.c depends on  */

/*
 *  from linux/lib/string.c
 */

//#include <linux/types.h>
//#include <linux/string.h>

#if 1	/* was ifndef __HAVE_ARCH_STRCPY */
char * strcpy(char * dest,const char *src)
{
	char *tmp = dest;

	while ((*dest++ = *src++) != '\0')
		/* nothing */;
	return tmp;
}
#endif


#ifndef __HAVE_ARCH_STRLEN
unsigned strlen(const char * s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}
#endif

/*
 * from linux/arch/mips/kernel/traps.c
 */

unsigned long exception_handlers[32];

/*
 * As a side effect of the way this is implemented we're limited
 * to interrupt handlers in the address range from
 * KSEG0 <= x < KSEG0 + 256mb on the Nevada.  Oh well ...
 */
void set_except_vector(int n, void *addr)
{
        unsigned handler = (unsigned long) addr;
        exception_handlers[n] = handler;
}


/*
 * from nowhere, needed for exception handling, also need to write and install
 * the actual exception handler
 */

void flush_cache_all(void)
{
/* Implement me to support breakpoints and other exceptions */
}
