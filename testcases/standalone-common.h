#ifdef STANDALONE
#include <stdarg.h>

#define RS232IN_DATA (*(volatile unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(volatile unsigned *) 0xFF000008)
#define TSC          (*(volatile unsigned *) 0xFF00000C)
#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))

#define BUF_SIZE 1024 // must be a power of two

unsigned last_serial_tag;
char serial_buffer[BUF_SIZE];
unsigned serial_buffer_wp;
unsigned serial_buffer_rp;
unsigned char c, chk;

void check_serial_input(void)
{
        unsigned char tag = RS232IN_TAG;
        if (tag != last_serial_tag) {
                serial_buffer[serial_buffer_wp++] = RS232IN_DATA;
                serial_buffer_wp &= BUF_SIZE-1;
                last_serial_tag = tag;
                if (serial_buffer_wp == serial_buffer_rp) {
                        while (SER_OUTBUSY());
                        SER_OUT('<');
                        while (SER_OUTBUSY());
                        SER_OUT('#');
                        while (SER_OUTBUSY());
                        SER_OUT('>');
                }
        }
}

int putchar(unsigned ch)
{
    check_serial_input();

    if (ch == '\n')
        putchar('\r');

    check_serial_input();
    while (SER_OUTBUSY())
        check_serial_input();
    SER_OUT(ch);

    return 1;
}

void print_hex2(unsigned char);

int getchar(void)
{
    unsigned char tag, ch;

    do
        check_serial_input();
    while (serial_buffer_wp == serial_buffer_rp);

    ch = serial_buffer[serial_buffer_rp++];
    serial_buffer_rp &= BUF_SIZE-1;

    /*
      serial_out('<');
      print_hex2(ch);
      serial_out('>');
    */

    return ch;
}

int puts(char *s)
{
    int n = 0;
    while (*s)
        putchar(*s++), ++n;

    return n;
}
#endif

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

#ifdef STANDALONE
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
            case 's':
                puts(va_arg(ap, char *));
                break;
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

#endif
