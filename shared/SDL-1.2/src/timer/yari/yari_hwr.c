#include <stdint.h>

#define MIPS32_HWR_COUNTER         2
#define MIPS32_HWR_CYCLES_PR_COUNT 3

uint32_t mips32_rdhwr_counter(void)
{
    uint32_t v;

    asm volatile(".set push;"
        ".set mips32r2;"
        "rdhwr %0,$%1;"
        ".set pop" : "=r" (v) : "i" (MIPS32_HWR_COUNTER));

    return v;
}

uint32_t mips32_rdhwr_cycles_pr_count(void)
{
    uint32_t v;

    asm volatile(".set push;"
        ".set mips32r2;"
        "rdhwr %0,$%1;"
        ".set pop" : "=r" (v) : "i" (MIPS32_HWR_CYCLES_PR_COUNT));

    return v;
}
