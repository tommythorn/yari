/*
 * Serial Monitor - the begining of a boot loader/monitor?
 *
 * Might be, but for now this is nothing but test code for the serial
 * port.
 */

#define NCAND 8 // 512

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

void putbinr8(unsigned x)
{
  int i;

  for (i = 8; i; --i, x >>= 1)
    putchar('0' + (x & 1));
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

#define RS232IN_DATA (*(unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(unsigned *) 0xFF000008)
#define TSC          (*(unsigned *) 0xFF00000C)

#define BUFFERSIZE 1024
char input_buffer[BUFFERSIZE];
unsigned input_pending = 0, input_read = 0;

int overrun = 0;

void service_rs232in()
{
        static unsigned char last_rs232_tag;
        unsigned char new_tag = RS232IN_TAG;
        unsigned ch;
        unsigned new_pending;

        if (new_tag == last_rs232_tag)
                return;

        if (new_tag != ((last_rs232_tag + 1) & 255))
                overrun += (last_rs232_tag+1 - new_tag) & 255;

        last_rs232_tag = new_tag;
        ch =  RS232IN_DATA;

        new_pending = input_pending + 1;
        new_pending &= BUFFERSIZE - 1;
        if (new_pending == input_read)
                return;
        input_buffer[input_pending] = ch;
        input_pending = new_pending;
}

void putchar(unsigned ch)
{
  if (ch == '\n')
    putchar('\r');

#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))
  while (SER_OUTBUSY())
    service_rs232in();
  SER_OUT(ch);
}

unsigned char getchar()
{
        unsigned char ch;
        while (input_pending == input_read)
                service_rs232in();
        ch = input_buffer[input_read++];
        input_read &= BUFFERSIZE - 1;
        return ch;
}

int main()
{
        unsigned *fb = (unsigned *) 0x90000000;
        unsigned i, n, k, j = 0;

        static unsigned last_rs232_tag;
        unsigned last_TSC;
        unsigned new_tag;
        unsigned new_TSC;
        unsigned lastcatch[32];


        int x = 10, y = 20;
        int dx = 1, dy = 1;
        
        myputs("Ready to go!\n");

        for (;;)
                putchar(getchar());
}
