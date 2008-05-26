/* yarimon.c -- I/O code for the opensource mips core yari
 */

#include <_syslist.h>
#include <sys/times.h>
#include <stdint.h>
#include "yari_perfcounters.h"

#define RS232IN_DATA (*(volatile unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(volatile unsigned *) 0xFF000008)
#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))

#define BUF_SIZE 1024 // must be a power of two

static char _serial_buffer[BUF_SIZE];
static unsigned _serial_buffer_wp;
static unsigned _serial_buffer_rp;

static void check_serial_input(void)
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
static int putchar(unsigned ch)
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

static int getchar(void)
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

/* transmit a single byte over the serial line */
int
outbyte(byte)
     unsigned char byte;
{
  return putchar(byte);
}

/* receive a single byte from the serial line */
unsigned char
inbyte()
{
  return getchar();
}




/* define the size of the memory - if not yet available */
#ifndef YARI_MEM_SIZE
#define YARI_MEM_SIZE 0x100000
#endif

/* define the size of the instruction cache - if not yet available */
#ifndef YARI_ICACHE_SIZE
#define YARI_ICACHE_SIZE 8192
#endif

/* define the size of the data cache - if not yet available */
#ifndef YARI_DCACHE_SIZE
#define YARI_DCACHE_SIZE 16384
#endif

struct s_mem
{
  unsigned int size;
  unsigned int icsize;
  unsigned int dcsize;
};

void
get_mem_info (mem)
     struct s_mem *mem;
{
  mem->size = YARI_MEM_SIZE;
  mem->icsize = YARI_ICACHE_SIZE;
  mem->dcsize = YARI_DCACHE_SIZE;
}

static uint32_t yari_rdhwr_SYNCI_Step(void)
{
    uint32_t r;

    asm(".set push;"
        ".set mips32r2;"
        "rdhwr %0,$%1;"
        ".set pop" : "=r" (r) : "i" (1));

    return r;
}

static uint64_t yari_rdhwr_TSC(void)
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

static inline uint32_t yari_rdhwr_mfcp2(int k)
{
    uint32_t r;

    asm(".set push;"
        ".set mips32r2;"
        "mfc2 %0,$%1;"
        ".set pop" : "=r" (r) : "i" (k));

    return r;
}

static inline uint32_t yari_rdhwr_freq_khz(void)
{
    return yari_rdhwr_mfcp2(PERF_FREQUENCY); // In kHz
}




typedef uint32_t size_t;

void yari_flush_icache(void *location, size_t len)
{
    uint32_t inc = yari_rdhwr_SYNCI_Step();
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

clock_t times(struct tms *buf)
{
    uint32_t T = 0;
    uint32_t frequency = yari_rdhwr_freq_khz();

    if (frequency)
        T = yari_rdhwr_TSC() / frequency;

    buf->tms_utime = T;
    buf->tms_stime = buf->tms_cutime = buf->tms_cstime = 0;

    return T;
}

int gettimeofday(struct timeval *__p, struct timezone *__z)
{
  return -1;
}

static char HEAP_MSG[] = "HEAP EXEEDS RAM!\n\0";

void *
sbrk (incr)
     int incr;
{
    extern char   end; /* Set by linker.  */
    static char * heap_end;
    char *        prev_heap_end;
    unsigned x, i;

    if (heap_end == 0)
        heap_end = & end;

    prev_heap_end = heap_end;
    heap_end += incr;

    if (heap_end > 0x40100000)
        for (i = 0; i < 16;i++)
            outbyte(HEAP_MSG[i]);

    return (void *) prev_heap_end;
}



static uint32_t __perf_counter[PERF_COUNTERS];
static uint64_t __pre_main_t0;

unsigned PERFMON = 1;

void __pre_main(void)
{
    /*
     * We have to use the macro construct as mfc2 requires a constant
     * argument and this is simpler than generating code on the fly.
     */

    extern void __post_main(void);

    atexit(__post_main);

#define X(H) __perf_counter[H] = -yari_rdhwr_mfcp2(H);
    __FORALL_PERF_COUNTERS(X);
#undef X
    __pre_main_t0 = yari_rdhwr_TSC();
}

void __post_main(void)
{
    int i;

    uint64_t c = yari_rdhwr_TSC() - __pre_main_t0;
#define X(H) __perf_counter[H] += yari_rdhwr_mfcp2(H);
    __FORALL_PERF_COUNTERS(X);
#undef X

    uint64_t mcpi = 0;
    uint32_t time = c / yari_rdhwr_freq_khz();

    if (__perf_counter[PERF_RETIRED_INST])
        mcpi = (100 * c) / (16ULL * __perf_counter[PERF_RETIRED_INST]);

    printf("Executed %llu instructions in %llu cycles (%u.%2u s) -> CPI: %llu.%02llu\n",
           16ULL * __perf_counter[PERF_RETIRED_INST],
           c, time / 100, time % 100,
           mcpi / 100, mcpi % 100);

    for (i = 0; i != PERF_COUNTERS; ++i)
        printf("%-22s = %10d\n", __perf_counter_names[i], __perf_counter[i]);

    printf("Finished\n");
}
