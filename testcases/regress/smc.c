#include <stdio.h>

/*
 * Test self-modifying code
 */

int return_const(void);
asm("return_const:");
asm(".set noreorder");
asm("jr $31");
asm("li $2, 1729");
asm(".set reorder");


int main()
{
    int i;

    for (i = 0; i < 100; ++i) {
        short *p = (short *) return_const;
        p[3] = i;
        __builtin_flush_icache(p + 3, 2);
        if (return_const() != i) {
            printf("Failure\n");
            return 1;
        }
    }

    printf("Success\n");

    return 0;
}
