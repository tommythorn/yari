#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// As long as we stay in the first segment, we won't have to deal with the crazy segments.
#define SCREEN_ADDR ((unsigned char *) 0x40000000 + 1024*1024)
#define SCREEN_W 1024
#define SCREEN_H  768

unsigned current_pos_x = 0, current_pos_y = 0;
unsigned char *current_pos = SCREEN_ADDR;
unsigned char current_fg_color = 255, current_bg_color = 3 << 2;

extern unsigned char font_fixed_6x13[];
unsigned char *font;
const unsigned font_w = 6, font_h = 13;

void clear(unsigned char color)
{
    unsigned i;
    uint32_t w = current_bg_color;
    uint32_t *fbw = (uint32_t *) SCREEN_ADDR;
    uint32_t *fbw_end = fbw + SCREEN_W * SCREEN_H / 4;
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

    current_pos_x = current_pos_y = 0;
    current_pos = SCREEN_ADDR;
}

void display_char(unsigned char ch)
{
    unsigned char *fp = current_pos;
    unsigned x, y;
    unsigned char *fontp = font + ch * 16;

    if (ch == '\n' || 1024 < current_pos_x + font_w) {
        if (SCREEN_H < current_pos_y + font_h) {
            /* Scroll up */
            for (fp = SCREEN_ADDR;
                 fp != SCREEN_ADDR + SCREEN_W * current_pos_y;
                 fp += 4 * 8) {
                unsigned *wp = (unsigned *) fp;
                wp[0] = wp[SCREEN_W * font_h / 4];
                wp[1] = wp[SCREEN_W * font_h / 4 + 1];
                wp[2] = wp[SCREEN_W * font_h / 4 + 2];
                wp[3] = wp[SCREEN_W * font_h / 4 + 3];
                wp[4] = wp[SCREEN_W * font_h / 4 + 4];
                wp[5] = wp[SCREEN_W * font_h / 4 + 5];
                wp[6] = wp[SCREEN_W * font_h / 4 + 6];
                wp[7] = wp[SCREEN_W * font_h / 4 + 7];
            }

            for (; fp != SCREEN_ADDR + SCREEN_W * SCREEN_H; ++fp)
                *fp = current_bg_color;

                if (0)
            for (; fp != SCREEN_ADDR + SCREEN_W * SCREEN_H; fp += 8) {
                unsigned *wp = (unsigned *) fp;
                wp[0] = current_bg_color * 0x1010101;
                wp[1] = current_bg_color * 0x1010101;
            }
        } else
            current_pos_y += font_h;
        current_pos_x = 0;
        fp = current_pos = SCREEN_ADDR +  SCREEN_W * current_pos_y;
        if (ch == '\n')
            return;
    }


    for (y = 0; y < font_h; ++y) {
        unsigned char font_line = *fontp++;
        for (x = 0; x < font_w; ++x) {
            *fp++ = font_line & 0x80 ? current_fg_color : current_bg_color;
            font_line <<= 1;
        }
        fp += SCREEN_W - font_w;
    }

    current_pos_x += font_w;
    current_pos += font_w;
}

void display_string(char *s)
{
    while (*s)
        display_char(*s++);
}

uint64_t yari_rdhwr_TSC(void)
{
    uint32_t count;
    uint32_t cycles_pr_count;

    asm(".set push;"
        ".set mips32r2;"
        "rdhwr %0,$2;"
        "rdhwr %1,$3;"
        ".set pop" : "=r" (count), "=r" (cycles_pr_count) : "i" (2));

    return (uint64_t) count * cycles_pr_count;
}

int main(int c, char **v)
{
    int i;
    char buf[99];

    font = font_fixed_6x13;
    //font_h = 13;
    //font_w = 6;

    display_string("Hello World! go to\nnext line!");

    for (i = 0; i < 3000; ++i) {
        current_fg_color++;
        if (current_fg_color == 0)
            current_bg_color = yari_rdhwr_TSC();
        sprintf(buf, "%d ", i);
        display_string(buf);
    }

    display_string("\nTHE END");

    printf("Done with test-font\n");

    char ch;
    for (;;) {
        ch = getchar();
        display_char(ch);
    }
}
