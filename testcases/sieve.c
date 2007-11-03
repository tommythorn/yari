#ifdef STANDALONE
asm(".globl _start");
asm("_start: la    $28,_gp       ");
asm("        la    $29,_gp + 8192");
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


void sieve(unsigned *cand, unsigned max);

#define TEST_EXT_SRAM 1

#ifdef TEST_EXT_SRAM
//# define NCAND 1024*1024/4 // External SRAM
# define NCAND 1024 // External SRAM
#else
# define NCAND 4
#endif

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

const unsigned bpp = 1;
unsigned *frame_buffer_start;
#define WHITE 1

void drawpixel(unsigned x, unsigned y, unsigned color)
{
        if (bpp == 1) {
                unsigned m = 1 << (31-(x & 31));
                unsigned w = ((y + (y << 2)) << 2) + (x >> 5); // (640/32)y
                if (color & 1)
                        frame_buffer_start[w] |= m;
                else
                        frame_buffer_start[w] &= ~m;
        } else if (bpp == 8) {
                ((unsigned char *)frame_buffer_start)[640 * y + x] = color;
        }
}

void drawlinex(int x1, int y1, int x2, int y2, int incdec,
               int e, int e_inc, int e_noinc)
{
        int i, start, end, var;

        start = x1;  end = x2;  var = y1;

        for (i = start; i <= end; i++) {
                drawpixel(i, var, WHITE);

                if (e < 0)
                        e += e_noinc;
                else {
                        var += incdec;
                        e += e_inc;
                }
        }
}

void drawliney(int x1, int y1, int x2, int y2, int incdec,
               int e, int e_inc, int e_noinc)
{
        int i, start, end, var;

        start = y1;  end = y2;  var = x1;

        for (i = start; i <= end; i++) {
                drawpixel(var, i, WHITE);

                if (e < 0)
                        e += e_noinc;
                else {
                        var += incdec;
                        e += e_inc;
                }
        }
}

int offsetx, offsety; // HACK

void bressline(int x1, int y1, int x2, int y2)
{
        int dx, dy, e, e_inc, e_noinc;
        int incdec, t, i;

        x1 += offsetx;
        x2 += offsetx;
        y1 += offsety;
        y2 += offsety;
        delay(50000);

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
                        drawpixel(x1, i, WHITE);
        }

        else if (dy == 0) { /* horizontal line */
                for (i = x1; i < x2; i++)
                        drawpixel(i, y1, WHITE);
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


void testdrawing(void)
{
        for (offsetx = 0; offsetx <= 500; offsetx += 25)
                for (offsety = 0; offsety <= 500; offsety += 25) {
                        bressline(  0, 300, 300, 300);  /* m = 0, horizontal line */
                        bressline(100, 200, 100, 300);  /* m = infinity, vertical line */
                        bressline(  0,   0, 100,  50);  /* 0 < m < 1 */
                        bressline(100,  50,   0,   0);  /* 0 < m < 1 */
                        bressline(  0,   0, 100, 100);  /* m = 1 */
                        bressline(100, 100,   0,   0);  /* m = 1 */
                        bressline(  0  , 0, 100, 150);  /* 1 < m < infinity */
                        bressline(100, 150,   0,   0);  /* 1 < m < infinity */
                        bressline(  0, 150, 100, 100);  /* 0 > m > -1 */
                        bressline(100, 100,   0, 150);  /* 0 > m > -1 */
                        bressline(  0, 200, 100, 100);  /* m = -1 */
                        bressline(100, 100,   0, 200);  /* m = -1 */
                        bressline(100, 100,   0, 300);  /* -1 > m > 0 */
                        bressline(  0, 300, 100, 100);  /* -1 > m > 0 */
                }
}


main()
{
        unsigned color = 0;
  /*
    XXX Here I start playing with external SRAM which is
    assigned to 0x9000_0000
  */
#ifdef TEST_EXT_SRAM
  unsigned *cand = (unsigned *) 0x90000000;
#else
  static unsigned cand[NCAND];
#endif
  unsigned char *pixel;
  unsigned n;

  frame_buffer_start = cand;

  for (pixel = (unsigned char *) cand, n = NCAND * 8; n != 0; n--, pixel++) {
          *pixel = n;
          // delay(1000);
  }

  myputs("New version!\n");
  for (;;) {
          unsigned *p;
          unsigned x,y,k;

          testdrawing();

          for (n = 0; n != 256; ++n) {
                  cand[n] = ~0;
                  delay(50000);
                  cand[n] = 0;
          }
          delay(10000000);

          color += 1;
          for (pixel = (unsigned char *) cand, n = NCAND * 8; n != 0; n--, pixel++) {
                  *pixel = 0;
                  // delay(1000);
          }
          for (x = k = 0; k < 40; ++k, ++x) {
                  put_unsigned(x); myputs("\n");
                  drawpixel(x, x, 1);
                  delay(50000);
                  drawpixel(x, x, 0);
                  delay(50000);
                  drawpixel(x, x, 1);
                  delay(50000);
                  drawpixel(x, x, 0);
                  delay(50000);
                  drawpixel(x, x, 1);
                  delay(600000);
          }
          delay(10000000);
          for (k = 0; k < 10; ++k) {
                  unsigned c = 1; // k | (1 << k)
                  for (x = 0; x < 64; ++x) {
                          drawpixel(x, k, c);
                          // drawpixel(cand, x, 479-k, c);
                  }
                  for (y = 0; y < 48; ++y) {
                          drawpixel(k, y, c);
                          // drawpixel(cand, 639-k, y, c);
                  }
                  delay(1000000);
          }
          delay(10000000);
          sieve(cand, NCAND*32);
  }
}


void sieve(unsigned *cand, unsigned max)
{
  unsigned i, j, n = 0;
  unsigned *p;

  myputs("Some prime numbers: ");
  putchar('2');
  putchar(' ');

  /*
   * Clear the candidate array.  Unrolled mostly for the benefit of
   * simulation.
   */
/*
  for (p = cand; p < cand + NCAND; p += 8) {
    p[0] = p[1] = p[2] = p[3] = p[4] = p[5] = p[6] = p[7] = 0;
  }
*/
  for (p = cand; p < cand + max/32; ++p) {
          p[0] = 0;
  }

  for (i = 0; i != max; ++i) {
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
      //hw/t
      // (n+2)p ~ cand[((n+2)p - 3)/2] =
      //          cand[((np + 2p) - 3) / 2] =
      //          cand[(np - 3)/2 + p]
      //
      for (j = i + p; j < max; j += p) {
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
  myputs("\nDone!\n");
}
