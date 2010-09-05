#include <SDL.h>
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
int enable_graphics = 0;

int endian_is_big = 0;
struct timeval stat_start_time, stat_stop_time;

void    *memory_segment[NSEGMENT];
unsigned memory_segment_size[NSEGMENT];

unsigned program_entry = 0;
static SDL_Surface *screen;

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
        {"bin-generation", 0, &run, 'b'}, // for Terasic Control panel
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
        {"graphics",       0, &enable_graphics, 1},
        {"icache-way-lines-log2",     1, 0, 1000},
        {"icache-words-in-line-log2", 1, 0, 1001},
        {"dcache-way-lines-log2",     1, 0, 1002},
        {"dcache-words-in-line-log2", 1, 0, 1003},
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

        printf("Gen load hazards:     %12llu (%5.2f%%)\n", stat_gen_load_hazard,
                stat_gen_load_hazard * 100.0 / n_issue);

        printf("Load use hazards, rs: %12llu (%5.2f%%)\n", stat_load_use_hazard_rs,
               stat_load_use_hazard_rs * 100.0 / n_issue);

        printf("                  rt: %12llu (%5.2f%%)\n", stat_load_use_hazard_rt,
               stat_load_use_hazard_rt * 100.0 / n_issue);

        printf("LW use hazards:       %12llu (%5.2f%%)\n", stat_load32_use_hazard,
               stat_load32_use_hazard * 100.0 / n_issue);
        printf("Shift use hazards:    %12llu (%5.2f%%)\n", stat_shift_use_hazard,
               stat_shift_use_hazard  * 100.0 / n_issue);
        printf("Nops:                 %12llu (%5.2f%%)\n", stat_nop,
               stat_nop * 100.0 / n_issue);
        printf("Nops in delay slots:  %12llu (%5.2f%%)\n", stat_nop_delay_slots,
               stat_nop_delay_slots * 100.0 / n_issue);
        printf("Nops after loads that aren't needed:\n"
               "                      %12llu (%5.2f%%)\n", stat_nop_useless,
               stat_nop_useless * 100.0 / n_issue);
}

void mainloop(void)
{
        unsigned last_generation = 0;

        for (;;) {
                SDL_Event event;

                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_MOUSEMOTION:
                                /*printf("Mouse moved by %d,%d to (%d,%d)\n",
                                       event.motion.xrel, event.motion.yrel,
                                       event.motion.x, event.motion.y);*/
                                break;
                        case SDL_MOUSEBUTTONDOWN:
                                /*printf("Mouse button %d pressed at (%d,%d)\n",
                                  event.button.button, event.button.x, event.button.y);*/
                                break;
                        case SDL_KEYDOWN:
                                //printf("Key down: %s\n", SDL_GetKeyName(event.key.keysym.sym));
                                switch (event.key.keysym.sym) {
                                case SDLK_ESCAPE: return;
                                case SDLK_0: keys |= 1 << 0; break;
                                case SDLK_1: keys |= 1 << 1; break;
                                case SDLK_2: keys |= 1 << 2; break;
                                case SDLK_3: keys |= 1 << 3; break;
                                case SDLK_4: keys |= 1 << 4; break;
                                case SDLK_5: keys |= 1 << 5; break;
                                case SDLK_6: keys |= 1 << 6; break;
                                case SDLK_7: keys |= 1 << 7; break;
                                case SDLK_8: keys |= 1 << 8; break;
                                case SDLK_9: keys |= 1 << 9; break;
                                default: break;
                                }
                                break;
                        case SDL_KEYUP:
                                //printf("Key down: %s\n", SDL_GetKeyName(event.key.keysym.sym));
                                switch (event.key.keysym.sym) {
                                case SDLK_ESCAPE: return;
                                case SDLK_0: keys &= ~(1 << 0); break;
                                case SDLK_1: keys &= ~(1 << 1); break;
                                case SDLK_2: keys &= ~(1 << 2); break;
                                case SDLK_3: keys &= ~(1 << 3); break;
                                case SDLK_4: keys &= ~(1 << 4); break;
                                case SDLK_5: keys &= ~(1 << 5); break;
                                case SDLK_6: keys &= ~(1 << 6); break;
                                case SDLK_7: keys &= ~(1 << 7); break;
                                case SDLK_8: keys &= ~(1 << 8); break;
                                case SDLK_9: keys &= ~(1 << 9); break;
                                default: break;
                                }
                                break;
                        case SDL_QUIT:
                                //printf("Quit\n");
                                return;
                        default:
                                //printf("Unknown event: %d\n", event.type);
                                break;
                        }
                }

                /* Only update the screen if the framebuffer has been written
                   since last update */
                if (last_generation != framebuffer_generation) {
                        last_generation = framebuffer_generation;
                        memcpy(screen->pixels, addr2phys(framebuffer_start),
                               framebuffer_size);

                        SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);
                        // SDL_Flip(screen); either seem to work
                }

                SDL_Delay(1000 / 30); // 30 fps
        }
}

void start_sdl(void)
{
        framebuffer_start = 0x40000000 + 1024*1024;
        framebuffer_size  = 1024*768;

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
                return;
        }

        atexit(SDL_Quit);

        screen = SDL_SetVideoMode(1024, 768, 8, SDL_SWSURFACE);
        if (screen == NULL) {
                fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
                return;
        }

        /* Set the window caption */
        SDL_WM_SetCaption("Yarisim framebuffer", "YARISIM");

        /* Populate the palette */
        SDL_Color colors[256];
        int i;

        /* Fill colors with color information RGB332 */
        for (i = 0; i < 256; ++i) {
                colors[i].r = (i >> 5) * (255 / 7.0);
                colors[i].g = ((i >> 2) & 7) * (255 / 7.0);
                colors[i].b = (i & 3) * (255 / 3.0);
        }

        /* Set palette */
        if (!SDL_SetColors(screen, colors, 0, 256)) {
                fprintf(stderr, "Unable to create framebuffer palette: %s\n",
                        SDL_GetError());
                screen = 0; //XXX should free it
                return;
        }
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

                case 1000: icache_way_lines_log2     = atoi(optarg); break;
                case 1001: icache_words_in_line_log2 = atoi(optarg); break;
                case 1002: dcache_way_lines_log2     = atoi(optarg); break;
                case 1003: dcache_words_in_line_log2 = atoi(optarg); break;

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

        int is_bidir = serial_output_file && serial_input_file &&
                strcmp(serial_input_file, serial_output_file) == 0;

        if (serial_input_file) {
                printf("serial input %s\n", serial_input_file);
                rs232in_fd = open(serial_input_file, (is_bidir ? O_RDWR : O_RDONLY) | O_NONBLOCK);
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

        if (serial_output_file && !is_bidir) {
                printf("serial output %s\n", serial_output_file);
                rs232out_fd = open(serial_output_file, O_WRONLY | O_NONBLOCK);
                if (rs232out_fd < 0)
                        perror(optarg), exit(1);
        }

        if (is_bidir)
                rs232out_fd = rs232in_fd;

        gettimeofday(&stat_start_time, NULL);

        switch (run) {
        case '1':
                if (enable_graphics)
                        start_sdl();
                atexit(print_stats);
                signal(SIGINT, exit);
                mips_state.pc = program_entry;
                init_reg_use_map();

                if (screen) {
                        SDL_CreateThread((int (*)(void *))run_simple, &mips_state);
                        mainloop();
                } else
                        run_simple(&mips_state);
                break;

        case 'b':
        case 'r':
        case 'd':
        case 'm':
                if (icache_way_lines_log2 == 0) {
                        printf("Please provide log2 of the number of I$ lines pr way with --icache_way_lines_log2\n");
                        exit(1);
                }
                if (dcache_way_lines_log2 == 0) {
                        printf("Please provide log2 of the number of D$ lines pr way with --dcache_way_lines_log2\n");
                        exit(1);
                }
                if (icache_words_in_line_log2 == 0) {
                        printf("Please provide log2 of the number of words in I$ lines --icache-words-in-line-log2");
                        exit(1);
                }
                if (dcache_words_in_line_log2 == 0) {
                        printf("Please provide log2 of the number of words in D$ lines --dcache-words-in-line-log2");
                        exit(1);
                }

                {
                        uint32_t icache_line_size = 4 << icache_words_in_line_log2;
                        uint32_t icache_way_size  = icache_line_size << icache_way_lines_log2;
                        uint32_t icache_size      = 4 * icache_way_size;
                        uint32_t dcache_line_size = 4 << dcache_words_in_line_log2;
                        uint32_t dcache_way_size  = dcache_line_size << dcache_way_lines_log2;
                        uint32_t dcache_size      = 4 * dcache_way_size;

                        uint32_t start;
                        char filename[99];
                        int way;

                        printf("%2d KiB 4-way I$, organized as %d cache lines, each line being %d bytes\n",
                               icache_size / 1024, 1 << icache_way_lines_log2, icache_line_size);
                        printf("%2d KiB 4-way D$, organized as %d cache lines, each line being %d bytes\n",
                               dcache_size / 1024, 1 << dcache_way_lines_log2, dcache_line_size);

                        for (way = 0, start = text_start; way < 4; ++way, start += icache_way_size) {
                                snprintf(filename, sizeof filename, "icache_ram%d.mif", way);
                                dump(filename, run, start, icache_way_size);
                        }

                        for (way = 0, start = 0x400e0000; way < 4; ++way, start += dcache_way_size) {
                                snprintf(filename, sizeof filename, "dcache_ram%d.mif", way);
                                dump(filename, run, start, dcache_way_size);
                        }

                        exit(0);
                }
                break;

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
