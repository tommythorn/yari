/* yarimon.c -- I/O code for the opensource mips core yari
 */

#include <_syslist.h>
#include <sys/times.h>

#define RS232IN_DATA (*(volatile unsigned *) 0xFF000004)
#define RS232IN_TAG  (*(volatile unsigned *) 0xFF000008)
#define TSC          (*(volatile unsigned *) 0xFF00000C)
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

static int yari_rdhwr_SYNCI_Step()
{
    int r;

    asm(".set push;"
        ".set mips32r2;"
        "rdhwr %0,$%1;"
        ".set pop" : "=r" (r) : "i" (1));

    return r;
}

typedef unsigned size_t;

void yari_flush_icache(void *location, size_t len)
{
    unsigned inc = yari_rdhwr_SYNCI_Step();
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

static unsigned __pre_main_t0;

void __pre_main(void)
{
    __pre_main_t0 = TSC;
}

void __post_main(void)
{
    printf("Program spent %d cycles in main\n", TSC - __pre_main_t0);
}

clock_t times(struct tms *buf)
{
  unsigned T = TSC/40000;
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
