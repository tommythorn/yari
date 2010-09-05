#include <stdio.h>

void external_observer(unsigned x)
{
    asm __volatile__("# %0" :: "r" (x));
}

int main()
{
    int i;
    printf("Random shifts:\n");

    for (i = 0; i != 200; ++i) {
        putchar('.');
        unsigned a = random();
        unsigned b = random();
        external_observer(a << b);
        external_observer(a >> b);
        external_observer((int) a >> b);
    }
    putchar('\n');
}
