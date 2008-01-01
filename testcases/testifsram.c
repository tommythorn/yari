/*
 * This test case tests execution from SRAM, which means copying code
 * to SRAM and executing it from there.
 */

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
asm("        nop                 ");
asm("        nop                 ");
asm("        nop                 ");
asm("r1729:  li $2, 0x1729       ");  // 80000064:       03e00008        jr      $31        
asm("        jr $31              ");  // 80000068:       24021729        li      $2,5929           
asm("        nop                 ");  // 8000006c:       00000000        nop               

// XXX BUG: stalling on jr $31 is broken for really short (2 instr) routines                                                                                       
                                        
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

#define X 1
#ifdef X
extern unsigned r1729[3];
#else
extern unsigned r1729();
#endif

typedef unsigned (*funcp)();

main()
{
        unsigned *fb = (unsigned *) 0x90000000;
        unsigned i;
        
        // myputs("Ready?\n");

#ifdef X
        fb[0] = r1729[0];
        fb[1] = r1729[1];
        fb[2] = r1729[2];
#endif

        i = 2*fb[2];
        i ^= 12;

#ifdef X
        i = ((funcp) fb)(i);
#else
        i = r1729(i);
#endif

        put_unsigned(i);

        myputs(i == 0x1729 ? "SUCCESS :-)\n" : "failure :-(");

        for (;;);
}
