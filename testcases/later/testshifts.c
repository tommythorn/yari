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
#endif

#ifdef mips
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

foo(unsigned n)
{
  int i = n;

  put_unsigned(n >> 5); putchar('\n');
  put_unsigned(i >> 5); putchar('\n');
  put_unsigned(i << 5); putchar('\n');
  put_unsigned(n >> 31); putchar('\n');
  put_unsigned(i >> 31); putchar('\n');
  put_unsigned(i << 31); putchar('\n');
}

main()
{
  int i, sh;
  unsigned u;
  unsigned failed = 0;

  i = 1;
  u = 1;
  for (sh = 0; sh < 32; ++sh) {
    put_unsigned(1 << sh);  putchar('\n');
    // putchar(i == (1 << sh) ? '.' : (failed++, '!'));
    i += i;
  }
  putchar('\n');

  i = ~0;
  for (sh = 31; sh >= 0; --sh) {
    put_unsigned(((int) 0x80000000) >> sh);  putchar('\n');
    // putchar(i == (((int) 0x80000000) >> sh) ? '.' : (failed++, '!'));
    i += i;
  }
  putchar('\n');
  u =  1;
  for (sh = 31; sh >= 0; --sh) {
    put_unsigned(((unsigned) 0x80000000) >> sh);  putchar('\n');
    // putchar(u == ((unsigned) 0x80000000 >> sh) ? '.' : (failed++, '!'));
    u += u;
  }
  putchar('\n');
  put_unsigned(failed);
  putchar('\n');
  foo(0x1729);
  foo(-1729);
}
