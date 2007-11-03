#ifdef STANDALONE

#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))

static void putch(unsigned ch)
{
        if (ch == '\n')
                putch('\r');

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
    putch('0');
    return;
  }

  for (p = powers10; *p; ++p) {
    unsigned scale = *p;
    unsigned i, t;

    for (i = 0; n >= scale; ++i, n -= scale)
      ;
    if (i != 0 || !leading_zero) {
      putch('0' + i);
      leading_zero = 0;
    }
  }
}

void puts(char *s)
{
  while (*s)
    putch(*s++);
}
#endif

int fib(int n)
{
    if (n < 2)
        return n;
    else
        return fib(n - 1) + fib(n - 2);
}

int main()
{
    int i;

    puts("Fibonacci numbers:\n");

    for (i = 1;; ++i) {
        put_unsigned(i);
        puts(": ");
        put_unsigned(fib(i));
        puts("\n");
    }
}
