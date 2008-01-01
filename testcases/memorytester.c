/*
 * This is intended to be an extensive RAM tester ... mostly really just
 * testing the SRAM controller.
 */

/* First a support library. */

static inline
putchar(unsigned ch)
{
  if (ch == '\n')
    putchar('\r');

#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))
  while (SER_OUTBUSY())
    ;
  SER_OUT(ch);
}

static void put_unsigned(unsigned n)
{
  // (unsigned)-1 == 4294967295
  static unsigned powers10[] = {
    1000000000,
    100000000,
    10000000,
    1000000,
    100000,
    10000,
    1000,
    100,
    10,
    1,
    0
  };
  unsigned *p;
  unsigned leading_zero = 1;

  if (!n) {
    putchar('0');
    return;
  }

  for (p = powers10; *p; ++p) {
    unsigned scale = *p;
    unsigned i, t;

    for (i = 0; n >= scale; ++i, n -= scale)
      ;
    if (i != 0 || !leading_zero) {
      putchar('0' + i);
      leading_zero = 0;
    }
  }
}

void myputs(char *s)
{
  while (*s)
    putchar(*s++);
}

#define RS232IN_DATA (*(unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(unsigned *) 0xFF000008)
#define TSC          (*(unsigned *) 0xFF00000C)



int
main()
{
    unsigned  * const sram = (unsigned *) 0x40000000;
    unsigned i, count = 0;

    put_unsigned(123);
    myputs("\nOk, are we ready to roll?\n");

    while (SER_OUTBUSY()) ;
    SER_OUT('O');
    while (SER_OUTBUSY()) ;
    SER_OUT('k');
    while (SER_OUTBUSY()) ;
    SER_OUT('!');
    while (SER_OUTBUSY()) ;
    SER_OUT('\r');
    while (SER_OUTBUSY()) ;
    SER_OUT('\n');


    for (i = 1;; ++i) {
        if (i == 0x1000) {
            i = 1;
            put_unsigned(++count);
        }
        sram[i] = i;
        while (SER_OUTBUSY()) ;
        if (sram[i-1] != i-1) {
            while (SER_OUTBUSY()) ;
            SER_OUT("0123456789abcdef"[(i >> 12) & 15]);
            while (SER_OUTBUSY()) ;
            SER_OUT("0123456789abcdef"[(i >>  8) & 15]);
            while (SER_OUTBUSY()) ;
            SER_OUT("0123456789abcdef"[(i >>  4) & 15]);
            while (SER_OUTBUSY()) ;
            SER_OUT("0123456789abcdef"[(i >>  0) & 15]);
            while (SER_OUTBUSY()) ;
            SER_OUT('\n');
        }
    }
}
