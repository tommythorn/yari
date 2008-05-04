#include <stdio.h>
#include "perfcounters.h"

static unsigned __perf_counter[PERF_COUNTERS];

static inline int  __builtin_mfcp2(int k)
{
    int r;

    asm(".set push;"
        ".set mips32r2;"
        "mfc2 %0,$%1;"
        ".set pop" : "=r" (r) : "i" (k));

    return r;
}

void perfmon_start(void)
{
    int i;

    /*
     * We have to use the macro construct as mfc2 requires a constant
     * argument and this is simpler than generating code on the fly.
     */

#define X(H) __perf_counter[H] = -__builtin_mfcp2(H);
    __FORALL_PERF_COUNTERS(X);
#undef X
}

void perfmon_report(void)
{
    int i;

#define X(H) __perf_counter[H] += __builtin_mfcp2(H);
    __FORALL_PERF_COUNTERS(X);
#undef X

    for (i = 0; i < PERF_COUNTERS; ++i)
        printf("%s = %d\n", __perf_counter_names[i], __perf_counter[i]);
}

