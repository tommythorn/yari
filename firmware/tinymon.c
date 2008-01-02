/*
  This is a tiny program loader that takes it's input through the
  serial port.

  First, however, we need to test loading bytes into the framebuffer.
*/

typedef void (*func_t)(void);

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

void serial_out(unsigned ch)
{
        check_serial_input();

        if (ch == '\n')
                serial_out('\r');

        check_serial_input();
        while (SER_OUTBUSY())
                check_serial_input();
        SER_OUT(ch);
        set_leds(ch);
}

void print_hex2(unsigned char);

unsigned char serial_in_lowercase(void)
{
        unsigned char tag, ch;

        do
                check_serial_input();
        while (serial_buffer_wp == serial_buffer_rp);

        ch = serial_buffer[serial_buffer_rp++];
        serial_buffer_rp &= BUF_SIZE-1;

        set_leds(ch);

        if (ch == '\r')
                ch = '\n';

        if ('A' <= ch && ch <= 'Z')
                ch = ch - 'A' + 'a';

        return ch;
}

static void out4(unsigned ch4)
{
        if (ch4 >> 8)
                out4(ch4 >> 8);
        serial_out(ch4 & 0xFF);
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

unsigned get_number(void)
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

int main()
{
 restart: ;

        volatile unsigned *addr = 0;
        unsigned in_error = 0;

        /* This is run out of the I$, which in practice means a ROM
         * that cannot even be read. We must manually initialize all
         * data and we can't use strings!
         */

        last_serial_tag = RS232IN_TAG;
        serial_buffer_rp = serial_buffer_wp = 1;

        serial_out('\n');
        serial_out('\n');
        out4('Tiny');
        out4('mon ');
        out4('2008');
        out4('-01-');
        out4('05\n');

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
         */

        for (;;) {
                unsigned arg = 0;
                unsigned i;
                unsigned char cmd, chk_ext;
                unsigned char error_code = ' ';

                do
                        c = serial_in_lowercase();
                while (c == '\n' || c == '\r' || c == ' ');

                /* Skip cruft until a command is encountered. */
                while (c != 'c' && c != 'l' && c != 'w' && c != 'r' && c != 'e' && c != 't')
                        c = serial_in_lowercase();

                chk = cmd = c;

                do
                        c = serial_in_lowercase();
                while (c == ' ');

                arg = get_number();

                while (c == ' ')
                        c = serial_in_lowercase();

                if (c != '\n') {
                        unsigned char good_chk = chk;
                        // Non-interactive use

                        chk_ext = get_number();

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

                if (cmd == 'c')
                        in_error = 0;
                else if (cmd == 'l')
                        addr = (unsigned *) arg;
                else if (cmd == 'w') {
                        *addr = arg;
                        set_leds(arg);
                        if (*addr != arg) {
                            serial_out('!');
                            goto error;
                        }
                        addr++;
                }
                else if (cmd == 'r') {
                        print_hex8(*addr++);
                        serial_out('\n');
                }
                else if (cmd == 'e') {
                        out4('\nE!\n');
                        ((func_t)arg)();
                        goto restart;
                }
                else if (cmd == 't') {
                    out4('Mem ');
                    out4('test');
                    serial_out('\n');

                    /* Simple memory tester */
                    for (addr  = (unsigned *) 0x40000000;
                         addr != (unsigned *) 0x400E0000;
                         addr += 16) {

                        addr[0] = ((unsigned) addr >> 13) ^~ (unsigned) addr;
                        addr[1] = ~ (unsigned) addr;
                        addr[2] = 42 + (unsigned) addr;
                        addr[3] = - (unsigned) addr;
                    }

                    for (addr  = (unsigned *) 0x40000000;
                         addr != (unsigned *) 0x400E0000;
                         addr += 16) {

                        unsigned a, b, c, d;
                        a = ((unsigned) addr >> 13) ^~ (unsigned) addr;
                        b = ~ (unsigned) addr;
                        c = 42 + (unsigned) addr;
                        d = - (unsigned) addr;

                        if (a != addr[0] || b != addr[1] || c != addr[2] || d != addr[3]) {
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
