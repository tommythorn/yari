/*
  This is a tiny program loader that takes it's input through the
  serial port.
*/

typedef void (*func_t)(void);

#if !defined(HOSTTEST)

asm("        .globl _start       ");
asm("_init:                      ");
asm("_start: la    $28,_gp       ");
asm("        la    $29,_gp+8192  ");/* + 8KiB */
asm("        jal   main          ");
asm("        nop                 ");
asm("        break               ");

extern void set_leds(unsigned v);
asm(".globl set_leds;set_leds: mtlo $4;jr $31");

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

void init_serial(void)
{
        last_serial_tag = RS232IN_TAG;
        serial_buffer_rp = serial_buffer_wp = 1;
}

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

void serial_out(unsigned ch)
{
        check_serial_input();

        if (ch == '\n')
                serial_out('\r');

        check_serial_input();

        while (SER_OUTBUSY()) {
            set_leds(0);
            check_serial_input();
        }

        SER_OUT(ch);
        set_leds(ch);
}

unsigned char serial_in(void)
{
        unsigned char tag, ch;

        do
                check_serial_input();
        while (serial_buffer_wp == serial_buffer_rp);

        ch = serial_buffer[serial_buffer_rp++];
        serial_buffer_rp &= BUF_SIZE-1;

        set_leds(ch);

        return ch;
}

#define store4(addr, v) (*(volatile unsigned *)(addr) = (v))
#define load4(addr)     (*(volatile unsigned *)(addr))

#else

void init_serial(void) { }
void serial_out(unsigned ch) { putchar(ch); }
unsigned char serial_in(void) {
        int c = getchar();
        if (c < 0)
                exit(0);
}

unsigned memory[1024*1024/4];
#define store4(addr, v) (memory[(unsigned) (addr) / 4 & (1 << 18) - 1] = (v))
#define load4(addr)     (memory[(unsigned) (addr) / 4 & (1 << 18) - 1])

#endif

unsigned char serial_in_lowercase(void)
{
        unsigned char ch = serial_in();

        if (ch == '\r')
                ch = '\n';

        if ('A' <= ch && ch <= 'Z')
                ch = ch - 'A' + 'a';

        return ch;
}

unsigned char c, chk;
unsigned in_error;

void print_hex2(unsigned char);

static int puts(const char *s)
{
        while (*s)
                serial_out(*s++);

        return 0;
}

static inline void print_hex1(unsigned d)
{
        serial_out(d + (d < 10 ? '0' : 'A' - 10));
}

void print_hex2(unsigned char v)
{
        print_hex1((v >> 4) & 15);
        print_hex1(v & 15);
}

void print_hex8(unsigned v)
{
        int i;

        for (i = 8; i; --i) {
                print_hex1((v >> 28) & 0xF);
                v <<= 4;
        }
}

void print_dec(unsigned v)
{
        if (v >= 10)
                print_dec(v / 10);
        serial_out((v % 10) + '0');
}

unsigned get_hexnum(void)
{
        unsigned arg;

        for (arg = 0;;) {
                if ('0' <= c && c <= '9')
                        arg = (arg << 4) + c - '0';
                else if ('a' <= c && c <= 'f')
                        arg = (arg << 4) + c - ('a' - 10);
                else
                        break;
                chk += c;
                c = serial_in_lowercase();
        }

        return arg;
}

unsigned d85(unsigned v)
{
    if (in_error)
        goto fail;

    unsigned char c;
    do
            c = serial_in();
    while (c <= ' ');

    if (c < '(' || '|' < c)
        goto fail;

    unsigned nv = v*85 + c - '(';

    // Check for overflow
    if (nv < v)
        goto fail;

    return nv;

fail:
    in_error = 1;
    return 0;
}

unsigned get_base85_word(void)
{
    return d85(d85(d85(d85(d85(0)))));
}

#if defined(HOSTTEST)
void tinymon_encode_word_base85(unsigned w)
{
        unsigned e = w % 85; w /= 85;
        unsigned d = w % 85; w /= 85;
        unsigned c = w % 85; w /= 85;
        unsigned b = w % 85; w /= 85;
        unsigned a = w % 85; w /= 85;
        putchar(a + '(');
        putchar(b + '(');
        putchar(c + '(');
        putchar(d + '(');
        putchar(e + '(');
}
#endif

void help()
{
        puts("Here we will tell about commands and "
             "statistics (like available memory and frequency\n");
}

int main()
{
 restart: ;

        unsigned *addr = 0;
        in_error = 0;

        /* This is run out of the I$, which in practice means a ROM
         * that cannot even be read. We must manually initialize all
         * data and we can't use strings!
         */

        init_serial();
        puts("\n\n");

        /*
         * Very simple protocol
         *
         *   <cmd> <hex8> ' ' <hex2> '\n'
         *
         * The <hex2> byte is the checksum of <cmd> + each byte of
         * <hex8>. If a mismatch is detected all input is ignored
         * until a clear command is seen.
         *
         * Commands
         * C - clear the error state (arg ignored)
         * L - set the load address
         * W - write a word to the current load address and move it forwards
         * R - read a word from the current load address and move it forwards
         * E - execute starting at the given address
         * X - receive a block of binary data in base85 encoding
         */

        for (;;) {
                unsigned arg = 0;
                unsigned i;
                unsigned char cmd, chk_ext;
                unsigned char error_code = ' ';

                do {
                        puts("Tinymon 2010-11-10 (Use 'h' for help)\n");

                        do
                            c = serial_in_lowercase();
                        while (c == '\r' || c == ' ');
                } while (c == '\n');

                /* Skip cruft until a command is encountered. */
                while (c != 'c' && c != 'l' && c != 'w' && c != 'r' && c != 'e' &&
                       c != 't' && c != 'x' && c != 'h')
                        c = serial_in_lowercase();

                chk = cmd = c;

                do
                        c = serial_in_lowercase();
                while (c == ' ');

                arg = get_hexnum();

                while (c == ' ')
                        c = serial_in_lowercase();

                if (c != '\n') {
                        unsigned char good_chk = chk;
                        // Non-interactive use

                        chk_ext = get_hexnum();

                        if (good_chk != chk_ext) {
                                serial_out('<');
                                print_hex2(good_chk);
                                serial_out('!');
                                serial_out('=');
                                print_hex2(chk_ext);
                                serial_out('>');

                                error_code = '1';
                                goto error;
                        }

                        while (c == ' ')
                                c = serial_in_lowercase();

                        if (c != '\n') {
                                error_code = '2';
                                goto error;
                        }
                }

                if (in_error && cmd != 'c' && cmd != 't')
                        continue;

                if (cmd == 'h')
                        help();
                else if (cmd == 'c')
                        in_error = 0;
                else if (cmd == 'l')
                        addr = (unsigned *) arg;
#if 0
                else if (cmd == 'w') {
                        store4(addr, arg);
                        if (load4(addr) != arg) {
                            serial_out('!');
                            goto error;
                        }
                        addr++;
                }
#endif
                else if (cmd == 'r') {
                        print_hex8(load4(addr++));
                        serial_out('\n');
                }
                else if (cmd == 'e') {
                        puts("\nExecute!\n");
#if defined(HOSTTEST)
                        printf("Execute from %08x\n", arg);
#else
                        ((func_t)arg)();
#endif
                        goto restart;
                } else if (cmd == 'x') {
                        unsigned *end = addr + arg;
                        unsigned chk = 0;
                        unsigned k = 0;

                        serial_out('\n');
                        for (k = 1; addr != end; addr += 1, ++k) {
                                unsigned w = get_base85_word();
                                if (in_error) {
                                        error_code = '8';
                                        goto error;
                                }
                                chk += w;
                                store4(addr, w);

                                if (((unsigned) addr & (1 << 10) - 1) == 0)
                                    serial_out('\r'), print_dec(100 * k / arg), serial_out('%');
                        }
                        serial_out('\r');
                        print_dec(100);
                        serial_out('%');
                        serial_out('\n');

                        k = get_base85_word();
                        if (k != -chk) {
#if defined(HOSTTEST)
                                printf("Got:");
                                tinymon_encode_word_base85(k);
                                printf("\nWanted:");
                                tinymon_encode_word_base85(-chk);
                                printf("\nRest: ");
                                while (~0 != (k = getchar()))
                                       putchar(k);
#endif
                                error_code = '7';
                                goto error;
                        }
#if 0
                } else if (cmd == 't') {
                    out4('Mem ');
                    out4('test');
                    serial_out('\n');

                    /* Simple memory tester */
                    for (addr  = (unsigned *) 0x40000000;
                         addr != (unsigned *) 0x400E0000;
                         addr += 4) {

                            store4(addr, ((unsigned) addr >> 13) ^~ (unsigned) addr);
                            store4(addr + 1, ~ (unsigned) addr);
                            store4(addr + 2, 42 + (unsigned) addr);
                            store4(addr + 3, - (unsigned) addr);
                    }

                    for (addr  = (unsigned *) 0x40000000;
                         addr != (unsigned *) 0x400E0000;
                         addr += 4) {

                        unsigned a, b, c, d;
                        a = ((unsigned) addr >> 13) ^~ (unsigned) addr;
                        b = ~ (unsigned) addr;
                        c = 42 + (unsigned) addr;
                        d = - (unsigned) addr;

                        if (a != load4(addr) ||
                            b != load4(addr + 1) ||
                            c != load4(addr + 2) ||
                            d != load4(addr + 3)) {
                                out4('Memo');
                                out4('ry e');
                                out4('rror');
                                out4(' at ');
                                print_hex8((unsigned) addr);
                                serial_out('\n');
                                goto restart;
                        }
                    }
                    out4('Ok!\n');
                    continue;
#endif
                }

                serial_out('.');
                continue;

        error:
                serial_out('<');
                serial_out(error_code);
                serial_out('>');
                in_error = 1;
                while (c != '\n')
                        c = serial_in_lowercase();
        }
}
