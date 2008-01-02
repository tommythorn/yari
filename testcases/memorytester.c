/*
 * Test memory
 *
 * Construct a 512 KiB linked ring and chase through it
 *
 * This however, only tests word accesses.
 */

#include <stdio.h>

// 512 KiB = 128 KiW = 2^17

//#define K 17
#define K 16
#define N (1 << (K))

unsigned **ring;

extern unsigned int _end;

int main(int argc, char **argv)
{
    unsigned i, j, cur, *p;
    unsigned k;

    printf("Memory Testing\n");

    ring = (unsigned **) &_end;

    for (k = 10; k <= K; ++k) {
        unsigned n = 1 << k;

        // XXX Ok, this is a totally broken way of doing this, but I
        // don't want to implement DIV right now which is needed for
        // the correct way.

        printf("Build linked ring of size %d\n", n);
        cur = 0;
        ring[cur] = (unsigned *) &ring[cur];
        for (i = 1; i < n; ++i)
            ring[i] = 0;

        for (i = 1; i < n - 128; ++i) {
            unsigned next;
            do {
                next = random() & (n - 1);
            } while (ring[next]);
            ring[next] = (unsigned *) &ring[cur];
            //printf("%d -> %d\n", next, cur);
            cur = next;
            if ((i &  127) == 0)
                printf("%d ", n - i);
        }
        ring[0] = (unsigned *) &ring[cur];
        //printf("0 -> %d\n", cur);
        printf("\n");

        for (i = 0; i < 10; ++i) {
            for (p = ring[0], j = n - 128; j; --j)
                p = * (unsigned **) p;
            printf("%d.. ", i);
        }

        if (p == ring[0])
            printf("Succeeded\n");
        else
            printf("Failure\n");
    }
}
