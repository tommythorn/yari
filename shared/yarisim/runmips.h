#ifndef _RUNMIPS_H_
#define _RUNMIPS_H_ 1

#include <SDL.h>
#include <sys/time.h>
#include <stdint.h>

/* Basic latencies */
#define LOAD_LATENCY 0
#define MULT_LATENCY 0
#define DIV_LATENCY  0
#define SH_LATENCY   0

extern int enable_disass;
extern int enable_disass_user;
extern int enable_verb_elf;
extern int enable_forwarding;
extern int enable_fastbranch;
extern int enable_testcases;
extern int enable_regwrites;
extern int enable_firmware_mode;
extern int enable_cosimulation;

extern int endian_is_big;

extern struct timeval stat_start_time, stat_stop_time;

/* The hazards here count def-use cases with no intervening cycles,
   not all hazards today */
uint64_t stat_gen_load_hazard;
uint64_t stat_load_use_hazard_rs;
uint64_t stat_load_use_hazard_rt;
uint64_t stat_load32_use_hazard;
uint64_t stat_shift_use_hazard;
uint64_t stat_nop;
uint64_t stat_nop_delay_slots;
uint64_t stat_nop_useless;

/*
  The simulation space address to physical address translation is a
  key operation, so it has been optimized slightly.

  First off, we can't implement a 1-1 mapping as the virtual address
  space on the host is smaller than the target (both are 32-bit), so
  we have to live with some sort of segmentation.  An obvious first
  approach is to divide the simulation space into 2^S segments with a
  mapping from segments to physical segments:

    N = 32 - S
    segment(x)   = x >> N
    offset(x)    = x & ((1 << N) - 1)
    Inv: for all x, (segment(x) << N) + offset(x) == x

    addr2phys(x) = memory_segment[segment(x)] + offset(x)

   As we can't map all segments, we represent the "holes" as areas
   laying outside the segment space:

    addr_mapped(x) = offset(x) < memory_segment_size[segment(x)]

   OPTIMIZATION

   Note, the offset(x) can also be written

     offset(x) = x - (segment(x) << N)

   thus

     addr2phys(x) = memory_segment[segment(x)] + x - (segment(x) << N)

   or by arranging for memory_segment'[s] = memory_segment[s] - (s << N)

     addr2phys(x) = memory_segment'[segment(x)] + x

   BUT we don't do it like that below, for clairity.
*/

#define SEGMENTBITS 4
#define OFFSETBITS (32 - SEGMENTBITS)
#define NSEGMENT (1 << SEGMENTBITS)

extern void    *memory_segment[NSEGMENT];
extern unsigned memory_segment_size[NSEGMENT];

#define segment(x)     (((unsigned)(x)) >> OFFSETBITS)
#define seg2virt(s)     (((unsigned)(s)) << OFFSETBITS)
#define offset(x)      (((unsigned)(x)) & ((1 << OFFSETBITS) - 1))
#define addr2phys(x)   (memory_segment[segment(x)] + offset(x))
#define addr_mapped(x) (offset(x) < memory_segment_size[segment(x)])

#define EXT8(b) ((int8_t) (u_int8_t) (b))
#define EXT16(h)((int16_t)(u_int16_t)(h))
#define LD8(a)  load(a,1,0)
#define LD16(a) load(a,2,0)
#define LD32(a) load(a,4,0)

#define ST8(a,v)  store(a,v,1)
#define ST16(a,v) store(a,v,2)
#define ST32(a,v) store(a,v,4)

#define fatal(msg...) ({printf(msg); exit(1);})

extern unsigned program_entry;
extern unsigned text_start, text_size;
extern unsigned mif_size;

long long unsigned n_cycle, n_stall;
long long unsigned n_issue;
long long unsigned n_call;
long long unsigned n_icache_hits, n_icache_misses;

extern int rs232in_fd;
extern int rs232out_fd;

unsigned keys;

unsigned segfault;
uint32_t framebuffer_start, framebuffer_size;
unsigned framebuffer_generation;

// Cache parameters in words log2
uint32_t icache_way_lines_log2, icache_words_in_line_log2;
uint32_t dcache_way_lines_log2, dcache_words_in_line_log2;

void exception(char *kind);
void loadsection(FILE *f, unsigned f_offset, unsigned f_len, unsigned m_addr, unsigned m_len);
void readelf(char *name);
void initialize_memory(void);
void ensure_mapped_memory_range(unsigned addr, unsigned len);
void dis_load_store(char *buf, char *name, inst_t i);
unsigned load(unsigned a, int c, int fetch);
void store(unsigned a, unsigned v, int c);
void disass(unsigned pc, inst_t i);

void init_reg_use_map(void);
void run_simple(MIPS_state_t *s);
void dump(const char *filename, char kind, uint32_t width, uint32_t *memory, uint32_t start, uint32_t size);
void dump_tinymon(void);

// Local Variables:
// mode: C
// c-style-variables-are-local-p: t
// c-file-style: "linux"
// End:

#endif
