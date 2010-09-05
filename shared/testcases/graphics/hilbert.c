#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// As long as we stay in the first segment, we won't have to deal with the crazy segments.
unsigned char *fb = (unsigned char *) 0x40000000 + 1024*1024;
unsigned long cursor_x = 0;
unsigned long cursor_y = 0;
unsigned point_x = 0, point_y = 0;
unsigned char cursor_color = 255;

#define WIDTH 1024
#define HEIGHT 768

void clear(unsigned char color)
{
    unsigned i;
    uint32_t w = color;
    unsigned *fbw = (uint32_t *) fb;
    unsigned *fbw_end = fbw + WIDTH * HEIGHT / 4;
    w |= w << 8;
    w |= w << 16;

    do {
        fbw[0] = w;
        fbw[1] = w;
        fbw[2] = w;
        fbw[3] = w;
        fbw[4] = w;
        fbw[5] = w;
        fbw[6] = w;
        fbw[7] = w;
        fbw[8] = w;
        fbw[9] = w;
        fbw[10] = w;
        fbw[11] = w;
        fbw[12] = w;
        fbw[13] = w;
        fbw[14] = w;
        fbw[15] = w;
        fbw += 16;
    } while (fbw != fbw_end);

    cursor_y = cursor_x = point_x = point_y = 0;
}

static inline void point(unsigned x, unsigned y)
{
#ifdef __APPLE__
    printf("Point (%d,%d) <- %d\n", x, y, cursor_color);
#endif
    fb[x + y * WIDTH] = cursor_color;
}

static inline void moveto(unsigned x, unsigned y)
{
        point_x = x;
        point_y = y;
}

void drawto(unsigned x, unsigned y)
{
        unsigned i, from, to;
        if (x == point_x) {
                if (point_y > y)
                        from = y, to = point_y;
                else
                        from = point_y, to = y;

                for (i = from; i <= to; ++i)
                        point(x, i);
                point_y = y;
        } else if (y == point_y) {
                if (point_x > x)
                        from = x, to = point_x;
                else
                        from = point_x, to = x;

                for (i = from; i <= to; ++i)
                        point(i, y);
                point_x = x;
        }
        cursor_color++;
}

void a(int), b(int), c(int), d(int);

int h0 = 8;
static int h, x, y, x0, y0;

void a(int i)
{
        if (i > 0) {
                d(i-1); x -= h; drawto(x,y);
                a(i-1); y -= h; drawto(x,y);
                a(i-1); x += h; drawto(x,y);
                b(i-1);
        }
}

void b(int i)
{
        if (i > 0) {
                c(i-1); y += h; drawto(x,y);
                b(i-1); x += h; drawto(x,y);
                b(i-1); y -= h; drawto(x,y);
                a(i-1);
        }
}

void c(int i)
{
        if (i > 0) {
                b(i-1); x += h; drawto(x,y);
                c(i-1); y += h; drawto(x,y);
                c(i-1); x -= h; drawto(x,y);
                d(i-1);
        }
}

void d(int i)
{
        if (i > 0) {
                a(i-1); y -= h; drawto(x,y);
                d(i-1); x -= h; drawto(x,y);
                d(i-1); y += h; drawto(x,y);
                c(i-1);
        }
}

void delay(void)
{
    asm __volatile__("");
}

void onesec(void)
{
    int i;

    for (i = 0; i < 1000000; ++i)
        delay();
}

int dummy;

int main(int c, char **v)
{
    int i, d;

#ifdef __APPLE__
    fb = malloc(1024*768);
#endif

#ifndef __APPLE__
    for (;;) {
        for (i = 0; i < 5; ++i) {
            clear(i);
            delay();
        }

        clear(0);

        for (x = 0; x < 300; x += 3) {
            cursor_color = x;
            moveto(  x,  x);
            drawto(500,  x);
            drawto(500,500);
            drawto(  x,500);
            drawto(  x,  x);
        }

        for (i = 0; i < 1000000; ++i)
            delay();

#endif
        for (d = 0; d <= 6; ++d) {
            clear(0);

            h0 = 8;
            h = h0; x0 = h/2; y0 = x0; h = h/ 2;
            x0 += h/2; y0 += h/2;

            x = x0 + 600; y = y0 + 500; moveto(x,y);
            a(d);

            for (i = 0; i < 1000000; ++i)
                delay();
        }
#ifndef __APPLE__
    }
#endif
}
