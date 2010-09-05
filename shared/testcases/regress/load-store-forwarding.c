#include <stdio.h>

/*

asm(".set push;"
    ".set noreorder;"

    ".globl lwsw;"
    "lwsw: "
        "b here;"
        "synci %0" :: "m" (*p));

asm(".set pop");

*/

void lwsw(volatile unsigned *p, unsigned *d)
{
    //*d = *p;
    asm(".set push;"
        ".set noreorder;"
        "lw      $2,0($4);"
        "sw      $2,0($5);"
        "j       $31;"
        "nop");
}

void lbusb(volatile unsigned char *p, unsigned char *d)
{
    //*d = *p;
    asm(".set push;"
        ".set noreorder;"
        "lbu     $2,0($4);"
        "sb      $2,0($5);"
        "j       $31;"
        "nop");
}

void lhsw(volatile char *p, unsigned *d)
{
    //*d = *p;
    asm(".set push;"
        ".set noreorder;"
        "lh     $2,0($4);"
        "sw     $2,0($5);"
        "j       $31;"
        "nop");
}

static unsigned words[] = {
    0x11223344,
    0x55667788,
    0x99AABBCC,
};


int main()
{
    int i;
    unsigned tmp;
    unsigned char tmpb;

    for (i = 0; i < sizeof words; ++i) {
        lhsw((short *) words + i, &tmp);
        if (tmp != *((short *)words + i))
            printf("%d!!!\n", i);
    }

    for (i = 0; i < sizeof words; ++i) {
        lbusb((unsigned char *) words + i, &tmpb);
        if (tmpb != *((unsigned char *)words + i))
            printf("%d!!\n", i);
    }

    for (i = 0; i < 3; ++i) {
        lwsw(words + i, &tmp);
        if (tmp != words[i])
            printf("%d!\n", i);
    }
}
