#include <stdio.h>

/*
 * Test self-modifying code
 */

volatile int array[20];

int return_const(short *p, int i)
{
    // Include lots of writes to stress store buffer flushing
    array[0] = i;
    array[1] = i;
    array[2] = i;
    array[3] = i;
    array[4] = i;
    array[5] = i;
    array[6] = i;
    array[7] = i;
    array[8] = i;
    array[9] = i;
    array[10] = i;
    array[11] = i;
    array[12] = i;
    array[13] = i;
    array[14] = i;
    array[15] = i;
    array[16] = i;
    array[17] = i;
    array[18] = i;
    array[19] = i;
    *p = i;

    //__builtin_flush_icache(p, 2);

    // calling __builtin_flush_icache(p, 2) is correct, but here
    // I use synci directly to test the worst case.
    asm(".set push;"
        ".set mips32r2;"
        "synci %0;"
        ".set pop" :: "m" (*p));

    asm(".set noreorder");
    asm("li $2, 1729");
    asm("jr $31");
    asm("nop");
    asm(".set reorder");
}

int return_const2(short *p, int i)
{
    // Include lots of writes to stress store buffer flushing
    array[0] = i;
    array[1] = i;
    array[2] = i;
    array[3] = i;
    array[4] = i;
    array[5] = i;
    array[6] = i;
    array[7] = i;
    array[8] = i;
    array[9] = i;
    array[10] = i;
    array[11] = i;
    array[12] = i;
    array[13] = i;
    array[14] = i;
    array[15] = i;
    array[16] = i;
    array[17] = i;
    array[18] = i;
    array[19] = i;
    *p = i;

    //__builtin_flush_icache(p, 2);

    // calling __builtin_flush_icache(p, 2) is correct, but here
    // I use synci directly to test the worst case.
    asm(".set push;"
        ".set mips32r2;"
        ".set noreorder;"
        "b here;"
        "synci %0" :: "m" (*p));

    asm("loop: b loop");

    asm("here: li $2, 1729");
    asm("jr $31");
    asm("nop");
    asm(".set pop");
}

int main()
{
    int i;

    for (i = 0; i < 100; ++i) {
        short *p = (short *) return_const2 + 53; // Magic!
        if (return_const2(p, i) != i) {
            printf("Failure\n");
            return 1;
        }
    }

    printf("Success\n");

    return 0;
}
