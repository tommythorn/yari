#define NCAND 512

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
asm("_start: lui   $29,0x8000    ");
asm("        ori   $29,$29,0x2000"); /* + 8KiB */
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




void puts(char *s)
{
  while (*s)
    putchar(*s++);
}

main()
{
  // 0 1 2 3  4  5  6
  // 3 5 7 9 11 13 15
  static unsigned cand[NCAND];

  unsigned i, j, n = 0;
  unsigned *p;

  puts("Hello Chris!\n");
}
