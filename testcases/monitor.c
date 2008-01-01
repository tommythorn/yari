void
skipspace(char **p)
{
        while (*p == ' ' || *p == '\t') { 
                ++p;
        }
}

void
readline(char *buf, unsigned size)
{
        ...
}

void
parse_char(char **p, char **error, char ch)
{
        if (*error) {
                return;
        }

        skipspace(p);
        if (**p == ch) {
                ++(*p);
        } else {
                *error = *p;
        }
}

void
parse_hex(char **p, char **error, unsigned *val)
{
        if (*error) {
                return;
        }

        skipspace(p);

        *val = 0;
        
        ...
}

void
unparse_hex(unsigned val)
{
        int i;
        for (i = 0; i < 8; ++i) {
                putchar("0123456789ABCDEF"[val >> 24]);
                val <<= 4;
        }
}

int
main()
{
        unsigned scratch[128];
        char buf[99];
        puts("Tommy's MIPS monitor version 0\n");
        puts("Hint: buffer @ %08x\n", scratch);

        for (;;) {
                char error = NULL; // No error
                char *p = buf;
                unsigned addr, val;
                readline(buf, sizeof buf);

                while (*p == ' ') ++p;
                switch (*p) {
                case 'm':
                        parse_char(&p, &error, 'm');
                        parse_hex(&p, &error, &addr);
                        if (!error && *p == '=') {
                                parse_char(&p, &error, '=');
                                parse_hex(&p, &error, &val);
                                if (!error) {
                                        *(unsigned *)addr = val;
                                }
                        }
                        unparse_hex(addr);
                        putchar(':');
                        unparse_hex(*(unsigned *)addr);
                        break;
                default:
                        error = p;
                        break;
                }

                if (error) {
                        for (p = buf; p < error; ++p) {
                                putchar(' ');
                        }
                        putchar('^');
                        puts(" not understood\n");
                }
        }
}
