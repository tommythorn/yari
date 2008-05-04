#define PERF_BRANCH_HAZARD 0
#define PERF_DCACHE_MISSES 1
#define PERF_DELAY_SLOT_BUBBLE 2
#define PERF_DIV_HAZARD 3
#define PERF_ICACHE_MISSES 4
#define PERF_IO_LOAD_BUSY 5
#define PERF_IO_STORE_BUSY 6
#define PERF_LOAD_USE_HAZARD 7
#define PERF_MULT_HAZARD 8
#define PERF_SB_FULL 9
#define PERF_COUNTERS 10

char *__perf_counter_names[] = {
"branch hazard",
"dcache misses",
"delay slot bubble",
"div hazard",
"icache misses",
"io load busy",
"io store busy",
"load use hazard",
"mult hazard",
"sb full",
"counters",
};
#define __FORALL_PERF_COUNTERS(F) \
F(PERF_BRANCH_HAZARD) \
F(PERF_DCACHE_MISSES) \
F(PERF_DELAY_SLOT_BUBBLE) \
F(PERF_DIV_HAZARD) \
F(PERF_ICACHE_MISSES) \
F(PERF_IO_LOAD_BUSY) \
F(PERF_IO_STORE_BUSY) \
F(PERF_LOAD_USE_HAZARD) \
F(PERF_MULT_HAZARD) \
F(PERF_SB_FULL) \
F(PERF_COUNTERS) \

