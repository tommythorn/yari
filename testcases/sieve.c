#include <stdio.h>

#define NCAND 20

unsigned cand[NCAND];

void sieve(unsigned *cand, unsigned max)
{
    unsigned i, j, n = 0;
    unsigned *p;

    putchar('2');

    for (p = cand; p < cand + max/32; ++p)
        p[0] = 0;

    for (i = 0; i != max; ++i) {
        if ((cand[i >> 5] & (1 << (i & 31))) == 0) {
            unsigned p = 2*i+3;

            if (++n == 15) {
                putchar('\n');
                n = 0;
            } else
                putchar(' ');

            printf("%d", p);
            // Remove 3p 5p 7p 9p ...
            // 3p ~ cand[(3p - 3)/2] = cand[(6i+9-3)/2] = cand[3i+3]
            // 5p ~ cand[(5p - 3)/2] = cand[(10i+15-3)/2] = cand[5i+6]
            // ...
            // np ~ cand[(np - 3)/2] = cand[(2ni+3n-3)/2] = cand[ni+1.5(n-1)]
            //                       = cand[pos(np)]
            //hw/t
            // (n+2)p ~ cand[((n+2)p - 3)/2] =
            //          cand[((np + 2p) - 3) / 2] =
            //          cand[(np - 3)/2 + p]
            //
            for (j = i + p; j < max; j += p) {
                cand[j >> 5] |= 1 << (j & 31);
                // printf("-%d ", 2*j+3);
            }

        }
    }

    printf("\n");
}

main()
{
    sieve(cand, NCAND*32);
}

