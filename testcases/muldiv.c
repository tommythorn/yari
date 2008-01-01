#ifdef STANDALONE
asm("        .globl _start");
asm("        # This is to stay compatible with crt0 C programs (starts at 0x80000003c)");
asm("_init:");
asm("        nop                     # 0x80000000");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("        nop");
asm("");
asm("# 0x8000003c");
asm("_start: la    $28,_gp       ");
asm("        la    $29,0x80002000");/* + 8KiB */
asm("        jal   main          ");
asm("        nop                 ");
asm("        break               ");
#endif

static inline putchar(unsigned ch)
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


unsigned foobar;
void foo() {
        foobar ++;
}

void delay(unsigned time)
{
        unsigned delay;
        for (delay = 0; delay < time; ++delay)
                foo();
}

main()
{
        unsigned x, y;
        // myputs("1..10 * 1..10: ");

        for (x = 1; x <= 10; ++x)
                for (y = 1; y <= 10; ++y) {
                        ((unsigned *)0x90000000)[0] = x * y;
                        ((int *)0x90000000)[1] = ((int)x - 5) * ((int)y - 5);
                }

        for (x = 1; x <= 10; ++x)
                for (y = 1; y <= 10; ++y) {
                        ((unsigned *)0x90000000)[0] = x / y;
                        ((int *)0x90000000)[1] = ((int)x - 5) / ((int)y - 5);
                }
}
