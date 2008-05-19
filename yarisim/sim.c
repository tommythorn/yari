#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <netinet/in.h>
#include <signal.h>
#include "elf.h"
#include <getopt.h>
#include "mips32.h"
#include "runmips.h"

int enable_disass     = 0;
int enable_disass_user= 0; // Enable disass once we reach user code (0x4...)
int enable_verb_elf   = 0;
int enable_forwarding = 0;
int enable_fastbranch = 0;
int enable_testcases  = 0;
int enable_regwrites  = 1; // XXX
int enable_firmware_mode = 0;
int enable_cosimulation = 0;

int endian_is_big = 0;
struct timeval stat_start_time, stat_stop_time;

void    *memory_segment[NSEGMENT];
unsigned memory_segment_size[NSEGMENT];

unsigned program_entry = 0;

/*
 * struct option {
 *      const char *name;
 *      int has_arg;
 *      int *flag;
 *      int val;
 * };
 */

static int run = '1';
static char *filename = 0;


static struct option long_options[] = {
        {"help",           0, NULL, '?'},
        {"data-generation",0, &run, 'd'}, // for Icarus Verilog simulation
        {"hex-generation", 0, &run, 'h'}, // for Quartus (HEX)
        {"mif-generation", 0, &run, 'm'}, // for Quartus (MIF)
        {"tinymon-generation", 0, &run, 't'}, // for Tinymon
        {"forward-values", 0, &enable_forwarding, 1},
        {"fast-branch",    0, &enable_fastbranch, 1},
        {"verbose",        0, &enable_disass, 1},
        {"disass-usercode",0, &enable_disass_user, 1},
        {"elf-header",     0, &enable_verb_elf, 1},
        {"testcases",      0, &enable_testcases, 1},
        {"regwrites",      0, &enable_regwrites, 1},
        {"firmware",       0, &enable_firmware_mode, 1}, // load only .text
        {"cosimulation",   0, &enable_cosimulation, 1},
        // {"file",        1, 0, 'f'}, // 1 = required arg
        // {"serial_in",   1, 0, 'i'}, // 1 = required arg
        // {"serial_out",  1, 0, 'o'}, // 1 = required arg
        {0, 0, 0, 0}
};


void usage(char *program)
{
        int i;

        if (strchr(program, '/')) {
                program = strrchr(program, '/') + 1;
        }

        fprintf(stderr,
                "YARI ISA Simulator\n"
                "\n"
                "Usage: %s [options] <mips-elf-files ...>\n"
                "\n"
                "  where options can be one or more of\n"
                "\n",
                program);

        for (i = 0; long_options[i].name; ++i) {
                fprintf(stderr, "  --%s\n", long_options[i].name);
        }

        fprintf(stderr,
                "  -i serialtty/file\n"
                "  -s serialtty/file\n"
                "\n"
                "Comments to <tommy-git@thorn.ws>\n");

        exit(1);
}

void print_stats(void)
{
        gettimeofday(&stat_stop_time, NULL);

        double delta = stat_stop_time.tv_sec - stat_start_time.tv_sec
                + 1e-6 * (stat_stop_time.tv_usec - stat_start_time.tv_usec);

        putchar('\n');
        printf("Simulation of %llu instructions in %4.2fs ~= %4.6f MIPS \n",
               n_issue, delta, n_issue / (1e6 * delta));

        // printf("%4.2f%% jal\n", 100.0 * n_call / n_issue);
        printf("I$ %llu hits / %llu misses = %4.2f%% miss rate\n",
               n_icache_hits, n_icache_misses,
               100.0 * n_icache_misses / (n_icache_hits + n_icache_misses));

        if (enable_cosimulation) {
                double freq = 25.0;

                printf("RTL stalls %llu\n", n_stall);
                printf("RTL CPI: %4.2f  ", (double) n_cycle / (double) n_issue);
                printf("Cosim speed %4.2fX slower than realtime (assuming %g MHz)\n",
                       freq * 1e6 / (n_cycle / delta), freq);
        }

        printf("Gen load hazards:   %12llu (%5.2f%%)\n", stat_gen_load_hazard,
                stat_gen_load_hazard * 100.0 / n_issue);
        printf("Load use hazards:   %12llu (%5.2f%%)\n", stat_load_use_hazard,
               stat_load_use_hazard * 100.0 / n_issue);
        printf("LW use hazards:     %12llu (%5.2f%%)\n", stat_load32_use_hazard,
               stat_load32_use_hazard * 100.0 / n_issue);
        printf("Shift use hazards:  %12llu (%5.2f%%)\n", stat_shift_use_hazard,
               stat_shift_use_hazard  * 100.0 / n_issue);
        printf("Nops:               %12llu (%5.2f%%)\n", stat_nop,
               stat_nop * 100.0 / n_issue);
        printf("Nops in delay slots:%12llu (%5.2f%%)\n", stat_nop_delay_slots,
               stat_nop_delay_slots * 100.0 / n_issue);
}

int main(int argc, char **argv)
{
        /* The Global MIPS State */
        MIPS_state_t mips_state;

        rs232in_fd = rs232out_fd = -1;
        char *serial_input_file = NULL;
        char *serial_output_file = NULL;

        for (;;) {
                int c;
                int option_index = 0;

                c = getopt_long(argc, argv, "tf:i:o:",
                                long_options, &option_index);
                if (c == -1)
                        break;

                switch (c) {
                case 0:
                        break;

                case 'f':
                        printf("file %s\n", filename = optarg);
                        break;

                case 'i': serial_input_file = optarg; break;
                case 'o': serial_output_file = optarg; break;
                case '?':
                        usage(argv[0]);
                        break;

                default:
                        printf ("?? getopt returned character code 0%o ??\n", c);
                }
        }

        if (optind >= argc) {
                usage(argv[0]);
        }

        while (optind < argc) {
                readelf(argv[optind++]);
        }

        if (serial_input_file) {
                printf("serial input %s\n", serial_input_file);
                rs232in_fd = open(serial_input_file, O_RDONLY | O_NONBLOCK);
                if (rs232in_fd < 0)
                        perror(optarg), exit(1);

                {
                        /* Turn off echo */
                        struct termios t;
                        if (tcgetattr(rs232in_fd, &t))
                                /*perror("getattr")*/;
                        else {
                                t.c_lflag &= ~(ECHO|ECHOE|ECHOK);
                                if (tcsetattr(rs232in_fd, TCSANOW, &t))
                                        perror("setattr");
                        }
                }
        }

        if (serial_output_file) {
                printf("serial output %s\n", serial_output_file);
                rs232out_fd = open(serial_output_file, O_WRONLY | O_NONBLOCK);
                if (rs232out_fd < 0)
                        perror(optarg), exit(1);
        }

        gettimeofday(&stat_start_time, NULL);

        switch (run) {
        case '1':
                atexit(print_stats);
                signal(SIGINT, exit);
                mips_state.pc = program_entry;
                run_simple(&mips_state);
                break;

        case 'r':
        case 'd':
        case 'm':
                dump(run);
                exit(0);

        case 't':
                dump_tinymon();
                exit(0);

        default:
                printf("No XX-run option given\n");
                exit(1);
        }

        if (enable_testcases) {
                printf("Test ");
                if (mips_state.r[7] == 0x1729) {
                        printf("SUCCEED!\n");
                        exit(0);
                } else {
                        printf("FAILED!  r7 = 0x%08x != 0x1729\n", mips_state.r[7]);
                        exit(1);
                }
        }

        exit(0);
}

// Local Variables:
// mode: C
// c-style-variables-are-local-p: t
// c-file-style: "linux"
// End:
