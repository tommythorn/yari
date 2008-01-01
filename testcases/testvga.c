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

typedef unsigned char pixel_t;

int volatile broken;

#define SCR(x) ((0-((x)&1)) ^ (((x) << 4) + 1))

static void
check(unsigned i, unsigned found)
{
        unsigned want = SCR(i);

        if (found != want) {
/*
                broken++;
*/

                myputs("FAILED at ");
                put_unsigned(i);
                myputs("! Got ");
                put_unsigned(found);
                myputs(" expected ");
                put_unsigned(want);
                myputs("\n");
        }
}


main()
{
        pixel_t *fb = (pixel_t *) 0x90000000;
        unsigned *ft = (unsigned *) 0x90000000;

        unsigned i, n, k, j = 0;
        int x = 10, y = 20;
        int dx = 1, dy = 1;

        putchar('O');
        putchar('k');
        putchar('?');
        putchar('\r');
        putchar('\n');

#define SET(x) (ft[(x)] = SCR(x))

#ifdef NOT
        SET(0);
        SET(1);
        SET(2);
        SET(3);
        SET(4);
        SET(5);

        for (i = 0; i < 1000; i += 4) {
                unsigned ft_i0 = ft[i+0];
                unsigned ft_i1 = ft[i+1];
                unsigned ft_i2 = ft[i+2];
                unsigned ft_i3 = ft[i+3];

                check(i+0, ft_i0);
                check(i+1, ft_i1);
                check(i+2, ft_i2);
                check(i+3, ft_i3);

                SET(i + 6);
                SET(i + 7);
                SET(i + 8);
                SET(i + 9);
        }


        // for (;;) {}
        ((unsigned *)fb)[0] = 0x01020304;
        for (i = 0; i < 1024*1024/sizeof(unsigned); ++i) {
                ((unsigned *)fb)[i+1] = ((unsigned *)fb)[i] + 0x04040404;
        }
#endif

#define DELAY() for (i = 0; i < 10000; ++i) {putchar(0);putchar(0);putchar(0);putchar(0);putchar(0);putchar(0);putchar(0); fb[1024*1024-1]++;}

        for (n = 0; ; ++n) {
#ifdef NOT
                myputs("Test #1: First set fb[i] = i ... \n");
                for (i = 0; i < 1024 * 1024; ++i) {
                        fb[i] = i;
                }

                for (i = 0; i < 1024 * 1024; ++i) {
                        pixel_t got = fb[i];
                        if (got != (i & 255)) {
                                myputs("FAILED at ");
                                put_unsigned(i);
                                myputs("! Got ");
                                put_unsigned(got);
                                myputs(" expected ");
                                put_unsigned(i & 255);
                                myputs("\n");
                                break;
                        }
                }
        test2:  myputs("Test #2: First set ft[i] = (i << 4) + 1 ... \n");
#endif
                for (i = 0; i < /*1024 * */ 1024 / 4; ++i) {
                        ft[i] = (i << 4) + 1;
                }

                for (i = 0; i < 1024 * 1024 / 4; ++i) {
                        unsigned got = ft[i];
                        if (got != (i << 4) + 1) {
                                myputs("FAILED at ");
                                put_unsigned(i);
                                myputs("! Got ");
                                put_unsigned(got);
                                myputs(" expected ");
                                put_unsigned((i << 4) + 1);
                                myputs("\n");
                                break;
                        }
                }

        test3:
                DELAY();
                x = y = 0;
                for (i = 0; i < 640*480; ++i) {
                        fb[y * 640 + x] = j;
                        if (x + dx < 0) dx = 1;
                        if (x + dx >= 640) dx = -1;
                        if (y + dy < 0) dy = 1;
                        if (y + dy >= 480) dy = -1;
                        y += dy;
                        x += dx;
                }

                ++j;

                DELAY();

                x = y = 0;
                for (i = 0; i < 640*480; ++i) {
                        fb[y * 640 + x] <<= 1;
                        if (x + dx < 0) dx = 1;
                        if (x + dx >= 640) dx = -1;
                        if (y + dy < 0) dy = 1;
                        if (y + dy >= 480) dy = -1;
                        y += dy;
                        x += dx;
                }

                DELAY();

                x = y = 0;
                dy = 1; dx = 0;
                for (i = 0; ; ++i) {
                        fb[y * 640 + x] = i;
                        if (x + dx < 0) break;
                        if (x + dx >= 640) break;
                        if (y + dy < 0) dy = 1, ++x;
                        if (y + dy >= 480) dy = -1, ++x;
                        y += dy;
                        x += dx;
                }

                DELAY();

                x = 639; y = 479;
                dy = -1; dx = 0;
                for (;; --i) {
                        fb[y * 640 + x] = ((y >> 3) << 3) ^ (x >> 3);
                        putchar(0);
                        if (x + dx <    0) break;
                        if (x + dx >= 640) break;
                        if (y + dy <    0) dy =  1, --x;
                        if (y + dy >= 480) dy = -1, --x;
                        y += dy;
                        x += dx;
                }
        }
}
