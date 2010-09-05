extern void set_leds(unsigned v);
asm(".globl set_leds;.ent set_leds;set_leds: mtlo $4;jr $31; .end set_leds");

/* 640 * 480 / 8 = 37.5 KiB */  // XXX use a rw register in the video
                                // controller so we don't have to
                                // change this every time.
unsigned * const frame_buffer_start = (unsigned *) 0x400E6A00;

#include "chars.c"

unsigned pos_x = 0;

void my_putchar(unsigned x, unsigned y, unsigned ch)
{
        unsigned line;
        unsigned char *bitmap = chars_bitmaps + (ch << 4);
        unsigned char *fb = (char *)frame_buffer_start + pos_x
                + ((y + (y << 4)) << 8);

        set_leds(x);

        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;
        *fb = *bitmap++, fb += 80;

        ++pos_x;

        if (pos_x == 2400)
                pos_x = 0;
}

main()
{
        unsigned char *fb;
        char *s;
        char pos_x = 10 /*(80 - 12) /  2*/;
        char pos_y = 0 /* (480 / 16) / 2 */;

        for (;;) {
                s = "Hello World!";
                frame_buffer_start[0] = ~0;
                frame_buffer_start[80/4] = ~0;
                frame_buffer_start[160/4] = ~0;
                frame_buffer_start[240/4] = ~0;
                frame_buffer_start[320/4] = ~0;

                fb = (char *)(0x400E6A00 + 5 * 80 * 16 + 5);

                fb[80* 0] = 0x41;
                fb[80* 1] = 0x41;
                fb[80* 2] = 0x41;
                fb[80* 3] = 0x41;
                fb[80* 4] = 0x7f;
                fb[80* 5] = 0x41;
                fb[80* 6] = 0x41;
                fb[80* 7] = 0x41;
                fb[80* 8] = 0x41;
                fb[80* 9] = 0x00;
                fb[80*10] = 0x00;
                fb[80*11] = 0x00;
                fb[80*12] = 0x00;
                fb[80*13] = 0x00;
                fb[80*14] = 0x00;
                fb[80*15] = 0x00;
                fb++;

                fb[80* 0] = 0x00;
                fb[80* 1] = 0x08;
                fb[80* 2] = 0x00;
                fb[80* 3] = 0x18;
                fb[80* 4] = 0x08;
                fb[80* 5] = 0x08;
                fb[80* 6] = 0x08;
                fb[80* 7] = 0x08;
                fb[80* 8] = 0x1c;
                fb[80* 9] = 0x00;
                fb[80*10] = 0x00;
                fb[80*11] = 0x00;
                fb[80*12] = 0x00;
                fb[80*13] = 0x00;
                fb[80*14] = 0x00;
                fb[80*15] = 0x00;
                fb++;

                fb[80* 0] = chars_bitmaps['!' * 16 + 0];
                fb[80* 1] = chars_bitmaps['!' * 16 + 1];
                fb[80* 2] = chars_bitmaps['!' * 16 + 2];
                fb[80* 3] = chars_bitmaps['!' * 16 + 3];
                fb[80* 4] = chars_bitmaps['!' * 16 + 4];
                fb[80* 5] = chars_bitmaps['!' * 16 + 5];
                fb[80* 6] = chars_bitmaps['!' * 16 + 6];
                fb[80* 7] = chars_bitmaps['!' * 16 + 7];
                fb[80* 8] = chars_bitmaps['!' * 16 + 8];
                fb[80* 9] = chars_bitmaps['!' * 16 + 9];
                fb[80*10] = chars_bitmaps['!' * 16 +10];
                fb[80*11] = chars_bitmaps['!' * 16 +11];
                fb[80*12] = chars_bitmaps['!' * 16 +12];
                fb[80*13] = chars_bitmaps['!' * 16 +13];
                fb[80*14] = chars_bitmaps['!' * 16 +14];
                fb[80*15] = chars_bitmaps['!' * 16 +15];

                set_leds(255);

                my_putchar(0,3,'h');
                my_putchar(1,3,'e');
                my_putchar(2,3,'l');
                my_putchar(3,3,'l');
                my_putchar(4,3,'o');
                my_putchar(5,3,' ');
                my_putchar(6,3,'W');
                my_putchar(7,3,'o');
                my_putchar(8,3,'r');
                my_putchar(9,3,'l');
                my_putchar(10,3,'d');
                my_putchar(11,3,'!');

                my_putchar(0,4,'H');
                my_putchar(1,4,'e');
                my_putchar(2,4,'l');
                my_putchar(3,4,'l');
                my_putchar(4,4,'o');
                my_putchar(5,4,' ');
                my_putchar(6,4,'w');
                my_putchar(7,4,'o');
                my_putchar(8,4,'r');
                my_putchar(9,4,'l');
                my_putchar(10,4,'d');
                my_putchar(11,4,'!');

                my_putchar(20,4,'H');
                my_putchar(21,4,'e');
                my_putchar(22,4,'l');
                my_putchar(23,4,'l');
                my_putchar(24,4,'o');
                my_putchar(25,4,' ');
                my_putchar(26,4,'W');
                my_putchar(27,4,'o');
                my_putchar(28,4,'r');
                my_putchar(29,4,'l');
                my_putchar(30,4,'d');
                my_putchar(31,4,'!');

                set_leds(0x55);
        }
}
