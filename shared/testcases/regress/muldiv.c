#include <stdio.h>

volatile unsigned *res_p;

main()
{
    unsigned i, x, y, k[3];

    res_p = k;

    for (i = 0; i < 200; ++i) {
        x = random();
        do y = random(); while (!y);
        // printf("%d: %d / %d\n", i, x, y);
        putchar('.');

        k[0] = x / y;
        k[1] = x % y;
        k[2] = x * y;
    }
}
