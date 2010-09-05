#include <stdio.h>

int main()
{
    int i;

    unsigned char bytes[] = {
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA
    };


    for (i = 0; i < sizeof bytes - 3; ++i) {
        unsigned tmp = random();
        unsigned tmp2;
        __asm__ ("usw %1,%0" : "=m" (bytes[i]) : "r" (tmp));
        __asm__ ("ulw %0,%1" : "=r" (tmp2) : "m" (bytes[i]));
        if (tmp != tmp2)
            printf("%d != %d\n", tmp, tmp2);
    }
}
