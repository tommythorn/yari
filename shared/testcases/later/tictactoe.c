/*
 * A simple Tic-Tac-Toe solver
 * Copyright (c) 2007,2008 Tommy Thorn
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// FIXME: Not winning is considered loosing.  Could make this a little
/// more sophisticated.

#define BUILD(a,b,c,d,e,f,g,h,i) \
  (((a) << 16) | ((b) << 14) | ((c) << 12) | ((d) << 10) | \
   ((e) << 8) | ((f) << 6) | ((g) << 4) | ((h) << 2) | (i))

#define GET(n, conf) (((conf) >> (2 * (n))) & 3)
#define SET(n, conf, v) ((conf & ~(3 << (2 * (n)))) | ((v) << (2 * (n))))

#define SWAP(conf) (0x3FFFF - (conf))

#define PERMUTE(conf, a, b, c, d, e, f, g, h, i) \
  BUILD(GET(a, conf), GET(b, conf), GET(c, conf), \
        GET(d, conf), GET(e, conf), GET(f, conf), \
        GET(g, conf), GET(h, conf), GET(i, conf))

#define MIRROR(conf) PERMUTE(conf, 6, 7, 8, 3, 4, 5, 0, 1, 2)
// 0 1 2       6 3 0
// 3 4 5  -->  7 4 1
// 6 7 8       8 5 2
#define ROTATE(conf) PERMUTE(conf, 6, 3, 0, 7, 4, 1, 8, 5, 2)

void print_conf(unsigned conf0)
{
    char const *s = " XO";
    printf("%c|%c|%c\n", s[GET(8, conf0)], s[GET(7, conf0)], s[GET(6, conf0)]);
    printf("-+-+-\n");
    printf("%c|%c|%c\n", s[GET(5, conf0)], s[GET(4, conf0)], s[GET(3, conf0)]);
    printf("-+-+-\n");
    printf("%c|%c|%c\n", s[GET(2, conf0)], s[GET(1, conf0)], s[GET(0, conf0)]);
    printf("\n-----------\n");
}

// Q: how many truly distinct T-T-T configurations are there?
// A: 19638 = 3^9
//     2862 if symmetries are accounted for, but only
//      764 can actually occur in legal games
//
//      539 in the danish variant that is played with only six tokens

#define random() 1

static inline unsigned umin(unsigned a, unsigned b)
{
    if (a <= b)
        return a;
    else
        return b;
}

#define FOREACHCOLOR(p) for (p = 0; p <= 2; ++p)

#define MAXCONF (1 << 18)
unsigned char *known_outcome;
int delay = 300000;
int game = 1;

int threeinarow(unsigned conf)
{
    int a = GET(8, conf);
    int b = GET(7, conf);
    int c = GET(6, conf);
    int d = GET(5, conf);
    int e = GET(4, conf);
    int f = GET(3, conf);
    int g = GET(2, conf);
    int h = GET(1, conf);
    int i = GET(0, conf);

    // a b c
    // d e f
    // g h i

    if (a && a == b && a == c) return 1;
    if (d && d == e && d == f) return 1;
    if (g && g == h && g == i) return 1;

    if (a && a == d && a == g) return 1;
    if (b && b == e && b == h) return 1;
    if (c && c == f && c == i) return 1;

    if (a && a == e && a == i) return 1;
    if (c && c == e && c == g) return 1;

    return 0;
}

unsigned canonical(unsigned conf0)
{
    unsigned minconf = conf0;
    int r;

    for (r = 0; r <= 7; ++r) {
        // print_conf(conf0);
        conf0 = ROTATE(conf0);
        if (r == 3)
            conf0 = MIRROR(conf0);
        minconf = umin(minconf, conf0);
    }
    return minconf;
}

void learn_victory(unsigned conf, int winner)
{
    conf = canonical(conf);
    known_outcome[conf] = winner;

    conf = canonical(SWAP(conf));
    known_outcome[conf] = 3 - winner;
}

void play(void)
{
    int color = 1;
    unsigned conf = 0;
    unsigned pconf = 0, ppconf = 0;

    for (;;) {
        int other = 3 - color;
        int p;

        printf("Game #%d\n", game);
        print_conf(conf);

        if (threeinarow(conf)) {
            printf("%c: Heh, I won!\n", " XO"[other]);
            learn_victory(conf,other);
            learn_victory(pconf,other);
            return;
        }

        // Quick win?
        for (p = 0; p < 9; ++p) {
            unsigned newconf;

            if (GET(p, conf))
                continue;

            newconf = canonical(SET(p, conf, color));
            if (known_outcome[newconf] == other)
                continue;
            if (known_outcome[newconf] == color)
                break;
            if (threeinarow(newconf))
                break;
        }

        // Defense move?
        if (p == 9)
            for (p = 0; p < 9; ++p) {
                if (GET(p, conf))
                    continue;

                if (threeinarow(SET(p, conf, other))) {
                    if (known_outcome[canonical(SET(p, conf, color))] == other)
                        p = 666;
                    break;
                }
            }

        // Any move?
        if (p == 9) {
            int move[9], n = 0;
            for (p = 0; p < 9; ++p) {
                unsigned newconf;

                if (GET(p, conf))
                    continue;

                newconf = canonical(SET(p, conf, color));
                if (known_outcome[newconf] == other)
                    continue;
                move[n++] = p;
            }
            if (n)
                p = move[random() % n];
        }

        if (p == 9 || p == 666) {
            if (conf && pconf)
                printf("%c: I have no winning move, I give up!\n", " XO"[color]);
            else {
                printf("%c: stupid game, can't be won!\n", " XO"[color]);
                exit(1);
            }

            // delay = (int) (0.85 * delay) + 1;
            delay -= 300000 / 60 - 1;

            learn_victory(conf,other);
            learn_victory(pconf,other);
            learn_victory(ppconf,other);
            return;
        }
        ppconf = pconf;
        pconf = conf;
        conf = SET(p, conf, color);
        color = 3 - color;
    }
}

int main(int argc, char **argv)
{
    known_outcome = malloc(MAXCONF);

    memset(known_outcome, 0, MAXCONF);

    for (;;) {
        play();
        game++;
    }
}
