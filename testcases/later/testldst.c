#define NCAND 16

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
asm("_start: la    $29,0x80002000"); /* sp = + 8KiB */
asm("        la    $28, _gp      "); /* gp */
asm("        la    $4, _ftext    "); /* a0 */
asm("        la    $5, _etext    "); /* a1 */
asm("        jal   main          ");
asm("        nop                 ");
asm("        break               ");

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
#endif


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

puthex4(unsigned x)
{
  x &= 0xF;
  putchar(x > 9 ? x - 10 + 'A' : x + '0');
}

puthex8(unsigned x)
{
  x &= 0xFF;
  puthex4(x >> 4);
  puthex4(x & 0xF);
}

puthex16(unsigned x)
{
  x &= 0xFFFF;
  puthex8(x >> 8);
  puthex8(x & 0xFF);
}

puthex32(unsigned x)
{
  puthex16(x >> 16);
  puthex16(x & 0xFFFF);
}

void myputs(char *s)
{
  while (*s)
    putchar(*s++);
}

void test(char *what, unsigned expected, unsigned got)
{
  myputs(what);
  puthex32(expected);
  myputs(got == expected ? " == " : " != ");
  puthex32(got);
  myputs(got == expected ? " OK" : " FAILED!");
  putchar('\n');
}

main()
{
  unsigned a = 0x12345678;
  unsigned z = 0xFEDCBA98;
  unsigned *p = 0x9000;

  myputs("\nBegin load and store tests.  The following equalities should match:\n");
  test("LB  ", 0xFFFFFFFE, *(signed   char  *) p);
  test("LBU ", 0x000000FE, *(unsigned char  *) p);
  test("LH  ", 0xFFFFFEDC, *(signed   short *) p);
  test("LHU ", 0x0000FEDC, *(unsigned short *) p);

  myputs("\nStore tests:\n");
  ((char *)p)[3] = 0x33;
  test("SB ", 0x12345633, *p);
  ((short *)p)[1] = 0x1111;
  test("SH ", 0xFEDC1111, *(short *)p);

  myputs("That's it folks!\n\n");
  for (;;);
}
