#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static struct {
    int w, h;
    unsigned char pixels[];
} image =
#include "Puzzle_Bobble-rgb332-image.h"

// As long as we stay in the first segment, we won't have to deal with the crazy segments.
#define FB_START ((unsigned char *) 0x40000000 + 1024*1024)

unsigned current_pos_x = 0, current_pos_y = 0;
unsigned char *current_pos = FB_START;
unsigned char current_fg_color = 255, current_bg_color = 3 << 2;

extern unsigned char font1[];
const unsigned font1_w = 8, font1_h = 12;

void display_char(unsigned char ch)
{
    unsigned char *fp = current_pos;
    unsigned x, y;
    unsigned char *fontp = font1 + ch * 16;

    if (ch == '\n' || 1024 < current_pos_x + font1_w) {
        if (768 <= current_pos_y + font1_h) {
            /* Scroll up */
            for (fp = FB_START; fp != FB_START + 1024 * current_pos_y; fp += 4 * 8) {
                unsigned *wp = fp;
                wp[0] = wp[1024 * font1_h / 4];
                wp[1] = wp[1024 * font1_h / 4 + 1];
                wp[2] = wp[1024 * font1_h / 4 + 2];
                wp[3] = wp[1024 * font1_h / 4 + 3];
                wp[4] = wp[1024 * font1_h / 4 + 4];
                wp[5] = wp[1024 * font1_h / 4 + 5];
                wp[6] = wp[1024 * font1_h / 4 + 6];
                wp[7] = wp[1024 * font1_h / 4 + 7];
            }

            for (; fp != FB_START + 1024 * 768; ++fp)
                *fp = current_bg_color;

                if (0)
            for (; fp != FB_START + 1024 * 768; fp += 8) {
                unsigned *wp = fp;
                wp[0] = current_bg_color * 0x1010101;
                wp[1] = current_bg_color * 0x1010101;
            }

                //current_pos_y -= font1_h;
        } else
            current_pos_y += font1_h;
        current_pos_x = 0;
        fp = current_pos = FB_START +  1024 * current_pos_y;
        if (ch == '\n')
            return;
    }


    for (y = 0; y < font1_h; ++y) {
        unsigned char font_line = *fontp++;
        for (x = 0; x < font1_w; ++x) {
            *fp++ = font_line & 0x80 ? current_fg_color : current_bg_color;
            font_line <<= 1;
        }
        fp += 1024 - font1_w;
    }

    current_pos_x += font1_w;
    current_pos += font1_w;
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
    display_string("Hello World! go to\nnext line!");

    for (i = 0; i < 3000; ++i) {
        current_fg_color++;
        if (current_fg_color == 0)
            current_bg_color = yari_rdhwr_TSC();
        sprintf(buf, "%d ", i);
        display_string(buf);
    }

    display_string("\nTHE END");

    printf("Done with test-font1\n");

    char ch;
    for (;;) {
        ch = getchar();
        display_char(ch);
    }
}
