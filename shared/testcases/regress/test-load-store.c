#include <stdio.h>

void test(char *what, unsigned expected, unsigned got)
{
    printf("%s %d %s %d %s\n",
           what,
           expected,
           got == expected ? " == " : " != ",
           got,
           got == expected ? " OK" : " FAILED!");
}

main()
{
    unsigned a = 0x12345678;
    unsigned z = 0xFEDCBA98;
    unsigned *p;

    printf("Test load and store.  The following equalities should hold:\n");
    test("LB  ", 0xFFFFFFFE, *(signed   char  *) &z);
    test("LBU ", 0x000000FE, *(unsigned char  *) &z);
    test("LH  ", 0xFFFFFEDC, *(signed   short *) &z);
    test("LHU ", 0x0000FEDC, *(unsigned short *) &z);

    puts("\nStore tests:\n");
    ((char *)&a)[3] = 0x33;
    test("SB ", 0x12345633, a);
    ((short *)&z)[1] = 0x1111;
    test("SH ", 0xFEDC1111, z);
}
