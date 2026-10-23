/*
 * cycles.h - contatore di cicli condiviso da bench e ttest.
 *
 * Default: rdtscp con lfence (da t_test_utils.h). Il TSC conta a frequenza
 * COSTANTE, non i cicli reali del core: disabilitare turbo boost e usare il
 * governor "performance" (scripts/run_bench.sh registra lo stato).
 *
 * Con CYCLES=cpucycles si usa libcpucycles (come lo start.sh originale).
 */
#pragma once

#include <stdint.h>

#if defined(USE_CPUCYCLES)
#include <cpucycles.h>
static inline uint64_t cycles_start(void) { return (uint64_t)cpucycles(); }
static inline uint64_t cycles_stop(void)  { return (uint64_t)cpucycles(); }
#define CYCLES_SOURCE "cpucycles"
#else
#include <x86intrin.h>
/* the fences keep the measured code from overlapping with the counter reads */
static inline uint64_t cycles_start(void) {
   unsigned aux; _mm_lfence(); uint64_t t = __rdtscp(&aux); _mm_lfence(); return t;
}
static inline uint64_t cycles_stop(void) {
   unsigned aux; uint64_t t = __rdtscp(&aux); _mm_lfence(); return t;
}
#define CYCLES_SOURCE "rdtscp"
#endif
 
 