#define NCAND 64

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

main()
{
  // 0 1 2 3  4  5  6
  // 3 5 7 9 11 13 15
  static unsigned cand[NCAND];

  unsigned i, j, n = 0;
  unsigned *p;

  for (i = 0; i != NCAND*32; ++i) {
    if ((cand[i >> 5] & (1 << (i & 31))) == 0) {
      unsigned p = 2*i+3;
      put_unsigned(p);
      putchar(' ');
      // Remove 3p 5p 7p 9p ...
      // 3p ~ cand[(3p - 3)/2] = cand[(6i+9-3)/2] = cand[3i+3]
      // 5p ~ cand[(5p - 3)/2] = cand[(10i+15-3)/2] = cand[5i+6]
      // ...
      // np ~ cand[(np - 3)/2] = cand[(2ni+3n-3)/2] = cand[ni+1.5(n-1)]
      //                       = cand[pos(np)]
      //
      // (n+2)p ~ cand[((n+2)p - 3)/2] =
      //          cand[((np + 2p) - 3) / 2] =
      //          cand[(np - 3)/2 + p]
      //
      for (j = i + p; j < NCAND*32; j += p) {
        cand[j >> 5] |= 1 << (j & 31);
        // printf("-%d ", 2*j+3);
      }
      if (++n == 15) {
        putchar('\n');
        n = 0;
      }
    }
  }
  putchar('\n');
}
