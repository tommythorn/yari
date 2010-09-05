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

void myputs(char *s)
{
  while (*s)
    putch(*s++);
}

volatile unsigned foobar;
void foo(unsigned x)
{
        foobar += x;
}

extern void set_leds(unsigned v);
asm(".globl set_leds;set_leds: mtlo $4;jr $31");

void delay(unsigned time)
{
        unsigned delay;

        for (delay = 0; delay < time; ++delay)
                foo(delay);
}

/* 640 * 480 / 8 = 37.5 KiB */  // XXX use a rw register in the video
                                // controller so we don't have to
                                // change this every time.
unsigned * const frame_buffer_start = (unsigned *) 0x400E6A00;
#define WHITE 1

void inline pixel_on(unsigned x, unsigned y)
{
        unsigned m = 1 << (31-(x & 31));
        unsigned w = ((y + (y << 2)) << 2) + (x >> 5); // (640/32)y
        frame_buffer_start[w] |= m;
}

void pixel_off(unsigned x, unsigned y)
{
        unsigned m = 1 << (31-(x & 31));
        unsigned w = ((y + (y << 2)) << 2) + (x >> 5); // (640/32)y
        frame_buffer_start[w] &= ~m;
}

void pixel_flip(unsigned x, unsigned y)
{
        unsigned m = 1 << (31-(x & 31));
        unsigned w = ((y + (y << 2)) << 2) + (x >> 5); // (640/32)y
        frame_buffer_start[w] ^= m;
}

#define draw_pixel pixel_on

void drawlinex(int x1, int y1, int x2, int y2, int incdec,
               int e, int e_inc, int e_noinc)
{
        for (; x1 <= x2; x1++) {
                draw_pixel(x1, y1);

                if (e < 0)
                        e += e_noinc;
                else {
                        y1 += incdec;
                        e += e_inc;
                }
        }
}

void drawliney(int x1, int y1, int x2, int y2, int incdec,
               int e, int e_inc, int e_noinc)
{
        for (; y1 <= y2; y1++) {
                draw_pixel(x1, y1);

                if (e < 0)
                        e += e_noinc;
                else {
                        x1 += incdec;
                        e += e_inc;
                }
        }
}

void line(int x1, int y1, int x2, int y2)
{
        int dx, dy, e, e_inc, e_noinc;
        int incdec, t, i;

        if (x1 > x2) {
                t = x1; x1 = x2; x2 = t;
                t = y1; y1 = y2; y2 = t;
        }

        dx = x2 - x1; dy = y2 - y1;

        if (dx == 0) { /* vertical line */
                if (y1 > y2) {
                        t = y1; y1 = y2; y2 = t;
                }

                for (i = y1; i <= y2; i++)
                        draw_pixel(x1, i);
        }

        else if (dy == 0) { /* horizontal line */
                for (i = x1; i < x2; i++)
                        draw_pixel(i, y1);
        }

        /* 0 < m < 1 */

        else if (dy < dx && dy > 0) {
                e_noinc = 2 * dy;
                e = 2 * dy - dx;
                e_inc = 2 *(dy - dx);
                drawlinex(x1, y1, x2, y2, 1,
                          e, e_inc, e_noinc);
        }

        /* m = 1 */

        else if (dy == dx && dy > 0) {
                e_noinc = 2 * dy;
                e = 2 * dy - dx;
                e_inc = 2 *(dy - dx);
                drawlinex(x1, y1, x2, y2, 1,
                          e, e_inc, e_noinc);
        }

        /* 1 < m < infinity */


        else if (dy > dx && dy > 0) {
                e_noinc = 2 * dx;
                e = 2 * dx - dy;
                e_inc = 2 *(dx - dy);
                drawliney(x1, y1, x2, y2, 1,
                          e, e_inc, e_noinc);
        }

        /* 0 > m > -1 */

        else if (-dy < dx && dy < 0) {
                dy = -dy;
                e_noinc = 2 * dy;
                e = 2 * dy - dx;
                e_inc = 2 *(dy - dx);
                drawlinex(x1, y1, x2, y2, -1,
                          e, e_inc, e_noinc);
        }

        /* m = -1 */

        else if (dy == -dx && dy < 0) {
                dy = -dy;
                e_noinc = (2 * dy);
                e = 2 * dy - dx;
                e_inc = 2 * (dy - dx);
                drawlinex(x1, y1, x2, y2, -1,
                          e, e_inc, e_noinc);
        }

        /* -1 > m > 0 */

        else if (-dy > dx && dy < 0) {
                dx = -dx;
                e_noinc = -2*dx; e = 2 * dx - dy;
                e_inc = - 2 * (dx - dy);
                drawliney(x2, y2, x1, y1, -1,
                         e, e_inc, e_noinc);
        }
}

void clear(void)
{
        unsigned *p   = frame_buffer_start;
        unsigned *end = frame_buffer_start + 640 * 480 / 32;

        // memset(frame_buffer_start, 0, 640 * 480 / 8);
        do {
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
                p += 4;
        } while (p != end);
}

unsigned random_number = 724389;

unsigned rand()
{
        return random_number = (random_number << 1) | ((random_number >> 31)^((random_number >> 28) & 1));
}

main()
{
        int round = 0;
        int x, y;
        int delaycount = 0;
        unsigned leds;

        putch('O');
        putch('k');
        putch('?');
        putch('\n');

        myputs("Here we go\n");

        /*
          A         B


          C         D


          A -> CD
          C -> DB
          D -> BA
          B -> AC
         */

        // clear();
        for (;;) {
                int n;

                myputs("set_leds\n");

                set_leds(leds = 0);

                delaycount >>= 1;
                if (delaycount <= 1000) {
                        delaycount = 500000;
                }

                set_leds(++leds);
                for (x = 0; x < 640; x += 10) {
                        line(  0,   0,  x, 479); // A -> CD
                }

                myputs("See a line?\n");

#if 0
                set_leds(++leds);
                for (y = 0; y < 480; y += 10) {
                        line(  0, 479,  639, y); // C -> DB
                }
#endif

                set_leds(++leds);
                for (x = 0; x < 640; x += 10) {
                        line(639, 479,  x,   0); // D -> BA
                }

                myputs("More lines\n");

                set_leds(++leds);
                for (y = 0; y < 480; y += 10) {
                        line(639,   0,    0, y); // B -> DC
                }
                set_leds(++leds);
                delay(delaycount);
                //clear();

                set_leds(++leds);
                for (x = y = 0; x < 640; x += 16, y += 12) {
                        line(  0,   y,  x, 479); // AC -> CD
                }

                set_leds(++leds);
                for (x = y = 0; x < 640; x += 16, y += 12) {
                        line(  x,  0,  639, y); // CD -> DB
                }

                set_leds(++leds);
                for (x = y = 0; x < 640; x += 16, y += 12) {
                        line(639, y,  x,   0); // DB -> BA
                }

                set_leds(++leds);
                for (x = y = 0; x < 640; x += 16, y += 12) {
                        line(x,   479,    0, y); // BA -> DC
                }
                delay(delaycount);
                clear();

                continue;

                for (n = 1000; --n; ) {
                        line((680 - 512)/2 + (rand() & 511),
                             (480 - 256)/2 + (rand() & 255),
                             (680 - 512)/2 + (rand() & 511),
                             (480 - 256)/2 + (rand() & 255));
                        rand();
                }

                delay(delaycount);
                clear();
        }
}
