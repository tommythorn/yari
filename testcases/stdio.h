#ifdef STANDALONE

#define RS232IN_DATA (*(volatile unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(volatile unsigned *) 0xFF000008)
#define TSC          (*(volatile unsigned *) 0xFF00000C)
#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))

#define BUF_SIZE 1024 // must be a power of two

char _serial_buffer[BUF_SIZE];
unsigned _serial_buffer_wp;
unsigned _serial_buffer_rp;

void check_serial_input(void)
{
        static unsigned last_serial_tag;

        unsigned char tag = RS232IN_TAG;
        if (tag != last_serial_tag) {
                _serial_buffer[_serial_buffer_wp++] = RS232IN_DATA;
                _serial_buffer_wp &= BUF_SIZE-1;
                last_serial_tag = tag;
                if (_serial_buffer_wp == _serial_buffer_rp) {
                        while (SER_OUTBUSY());
                        SER_OUT('<');
                        while (SER_OUTBUSY());
                        SER_OUT('#');
                        while (SER_OUTBUSY());
                        SER_OUT('>');
                }
        }
}

static int _column = 0;
int putchar(unsigned ch)
{
    if (ch == '\n') {
        putchar('\r');
        _column = -1;
    } else if (ch == '\t') {
        do
            putchar(' ');
        while (_column & 7);
        return 1;
    }

    check_serial_input();
    while (SER_OUTBUSY())
        check_serial_input();
    SER_OUT(ch);
    ++_column;

    return 1;
}

int getchar(void)
{
    unsigned char tag, ch;

    do
        check_serial_input();
    while (_serial_buffer_wp == _serial_buffer_rp);

    ch = _serial_buffer[_serial_buffer_rp++];
    _serial_buffer_rp &= BUF_SIZE-1;

    // Local echo
    putchar(ch);

    if (ch == '\r')
        ch = '\n';

    return ch;
}

int puts(char *s)
{
    int n = 0;
    while (*s)
        putchar(*s++), ++n;

    putchar('\n');

    return n;
}


int printf(char *fmt, ...);

int exit(int v)
{
    asm("li $2, 0x87654321");
    asm("mtlo $2");
    asm(".word 0x48000000");

    printf("THE END\n");
    for (;;);
}

#endif

#include <stdarg.h>

/* Division-free integer printing */
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

/* Hokey printf substitute */
int printf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    for (;; ++fmt)
        switch (*fmt) {
        case '\0':
            va_end(ap);
            return 1; // XXX return val incorrect
        case '%':
            switch (*++fmt) {
            case 'd': {
                int value = va_arg(ap, int);
                if (value < 0)
                    putchar('-'), value = -value;
                put_unsigned((unsigned) value);
                break;
            }
            case 'c': {
                char c = va_arg(ap, int);
                putchar(c);
                break;
            }
            case 's': {
                char *s = va_arg(ap, char *);
                while (*s)
                    putchar(*s++);
                break;
            }
            default:
                putchar(*fmt);
                break;
            }
            break;

        default:
            putchar(*fmt);
            break;
        }
}

static char _lookahead = ' ';

static void _next(void)
{
    _lookahead = getchar();
}

static _skip_ws(void)
{
    while (_lookahead == ' ' ||
           _lookahead == '\t' ||
           _lookahead == '\r' ||
           _lookahead == '\n')
        _next();
}

static _skip_line(void)
{
    while (_lookahead != '\n')
        _next();
}

int scanf_hack(char *fmt, int *v)
{
    static int next = 2;

    *v = next;

    next = 34;
}

int scanf(char *fmt, ...)
{
    int n = 0;
    va_list ap;
    va_start(ap, fmt);

    for (;; ++fmt)
        switch (*fmt) {
        case '\0':
            va_end(ap);
            return n;
        case '%':
            switch (*++fmt) {
            case 'd': {

                int neg = 0;
                int value = 0;

                _skip_ws();
                if (_lookahead == '-')
                    neg = 1, _next();

                while ('0' <= _lookahead && _lookahead <= '9') {
                    value = value * 10 + _lookahead - '0';
                    _next();
                }

                if (neg)
                    value = -value;
                *va_arg(ap, int *) = value;
                ++n;
                break;
            }

            default:
                continue;
            }
            break;

        case ' ':
            _skip_ws();
            break;

        default:
            if (*fmt == _lookahead)
                break;
            else {
                _skip_line();
                return n;
            }
        }
}

int strlen(char *s)
{
    int n;

    for (n = 0; *s; ++n, ++s)
        ;

    return n;
}

int abs(int v) { return v < 0 ? -v : v; }

int atoi(char *s)
{
    int neg = 0;
    int value = 0;

    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
        ++s;

    if (*s == '-')
        neg = 1, ++s;

    while ('0' <= *s && *s <= '9') {
        value = value * 10 + _lookahead - '0';
        ++s;
    }

    if (neg)
        value = -value;

    return value;
}

typedef char *FILE;

FILE *fopen(char *name, char *mode)
{
    static FILE f =
"#include <stdio.h>\n"
"#define X(s) (!(s&3)-((s&3)==2))\n"
"#define W while\n"
"char Z[82][82],A,B,f,g=26;z(q){return atoi(q);}m(d,l){return\n"
"Z[   B       +    X      (   f     +\n"
"3) * d+l *X(f+ 2 )][ A+X ( f ) * d +\n"
"l* X           (     f     + 3 ) ] ;}int\n"
"h= 0;D(p,s)char*s; {W(h>>3<p>> 3 ) {putchar('\\t'\n"
");           h =       (       h   +8\n"
")&~7 ;}W(h < p ){putchar(' ');++h; }(void)printf(\n"
"\"%s\"   ,   s                 )     ;h+=strlen(s);}main(x,a)char **a; {\n"
"# define P(x) (x?(5-(x))*(6-(x ))/2:11)\n"
"int b; { char b[256],i,  j=0;  FILE*F;F=fopen(x-1?a[1]:\"buzzard.c\",\"r\");W(\n"
"fgets( b ,256 ,F)){for(i=0;b[ i];++ i)\n"
"Z[j][i ] =( b [     i   ]     ==' '?1:2*(b[i]==(x>2?*a[2]:'\\\\')));++j;}fclose\n"
"(F);}A   =4 ; B = 3 ; f = 1;x >3? A=z(a[3]),B=z(a[4]):0;b='\\n';do{if(b=='\\n'\n"
"){int y ,     s , d , p   , q       ,i;for\n"
"(y=-11; y<= 11;++ y){ for(s = 1 ,d=0;s+3;s-=2){for\n"
"(;d!=2    +       3   * s     ;     d+=s){\n"
"if(m(d,0) !=1 ){p=P (d) ;if (abs( y )\n"
"   <p&&   !   m       (       d   , 0 )||abs(y)>p)break;for\n"
"(i  =-p;i<p;++i)D(g+i*2,\"--\");D(0,\"-\");break;}if(d==5)continue;\n"
"p=P(d+1);q=P(d);if\n"
"(abs(y)         >q)continue;if \n"
"(abs(y)         <p)D(g-s*(2*p+1),\"|\");else if(m(d,s)){if\n"
"(abs(y)         <=p)for(i=(s==1?-q:p);i!=(s==1?-p:q);\n"
"(abs(y)         ),++i)D(g+2*i+(s==-1),\"--\");}else if\n"
"(abs(y)         ==p)D(g-s*(2*p+1),\"|\");else D(g-\n"
"(abs(y)         *s*2),(s==1)^(y>0)?\"\\\\\":\"/\");}d-=s;}puts(\n"
"\"\");h=0;}}f+=(b=='r')-(b=='l');f&=3;if(b=='f'){if(!m(1,0))continue;\n"
"A+=X(f);B+=X(f-1);}}W((b=getchar())!=-1&&m(0,0)==1);return 0;}\n"
;

    return &f;
}

#define NULL ((void *) 0)

int fclose(FILE *f) {}
char *fgets(char *str, int size, FILE *f)
{
    char *s = str;
    char *fp = *f;

    if (!*fp)
        return NULL;

    while (1 < size && *fp) {
        *s++ = *fp++;
        --size;
        if (fp[-1] == '\n')
            break;
    }

    if (s != str && size)
        *s = '\0';

    *f = fp;
    return str;
}

unsigned __pre_main_t0;

void __pre_main(void)
{
    __pre_main_t0 = TSC;
}

void __post_main(void)
{
    printf("Program spent %d cycles in main\n", TSC - __pre_main_t0);
}

extern char _end[];


void *__sbrk_p = _end;

void *malloc(unsigned size)
{
    void *res = __sbrk_p;

    __sbrk_p += size;
}

void memset(void *d, char v, unsigned size)
{
    char *cp = (char *) d;

    while ((unsigned) cp & 3 && size)
        *cp++ = v, --size;

    unsigned *up = (unsigned *) cp;
    unsigned vvvv = v * 0x1010101;
    while (size > 32) {
        up[0] = vvvv;
        up[1] = vvvv;
        up[2] = vvvv;
        up[3] = vvvv;
        up[4] = vvvv;
        up[5] = vvvv;
        up[6] = vvvv;
        up[7] = vvvv;
        size -= 32;
    }

    cp = (char *) up;
    while (size)
        *cp++ = v, --size;
}

// Modern GCCs provide these builtin, but ours is a bit stale


static inline int  __builtin_rdhwr(int k)
{
    int r;

    asm(".set push;"
        ".set mips32r2;"
        "rdhwr %0,$%1;"
        ".set pop" : "=r" (r) : "i" (k));

    return r;
}

typedef unsigned size_t;

static inline void __builtin_flush_icache(void *location, size_t len)
{
    const int SYNCI_Step = 1;
    unsigned inc = __builtin_rdhwr(SYNCI_Step);
    void *end = location + len;

    for (location = (void *) ((unsigned) location & ~(inc - 1));
         location < end;
         location += inc) {
            asm(".set push;"
                ".set mips32r2;"
                "synci %0;"
                ".set pop" :: "m" (* (char *) location));
    }
}
