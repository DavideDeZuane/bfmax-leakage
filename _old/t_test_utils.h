#pragma once
/* ------------------------------------------------------------------------------*/
/*                 UTILITIES FOR THE FIXED-VS-RANDOM WELCH t-TEST                */
/* ------------------------------------------------------------------------------*/
/*
 * Reusable pieces of a dudect-style timing test:
 *   - cycle counters (rdtscp + lfence), to wrap ONLY the function under test
 *   - Welford running mean/variance and Welch's t statistic
 *   - ttest_analyse(): t on all measurements and on percentile crops
 *
 * Typical use:
 *     uint64_t *t = malloc(n * sizeof *t);  int *cls = malloc(n * sizeof *cls);
 *     for (i ...) {
 *        ... prepare the input of class cls[i] (not timed) ...
 *        uint64_t t0 = ttest_cycles_start();
 *        function_under_test(...);
 *        t[i] = ttest_cycles_stop() - t0;
 *     }
 *     ttest_analyse("name", t, cls, n, 1);
 *
 * |t| > TTEST_THRESHOLD (4.5) means the two classes have different mean time.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

#define TTEST_THRESHOLD 4.5
#define TTEST_N_CROPS   4

/* ------------------------------ timing ---------------------------------- */
/* the fences keep the measured code from overlapping with the counter reads */
static inline uint64_t ttest_cycles_start(void) {
   unsigned aux; _mm_lfence(); uint64_t t = __rdtscp(&aux); _mm_lfence(); return t;
}
static inline uint64_t ttest_cycles_stop(void) {
   unsigned aux; uint64_t t = __rdtscp(&aux); _mm_lfence(); return t;
}

/* ------------------------------ statistics ------------------------------ */
/* running mean and variance (Welford): numerically stable, O(1) memory */
typedef struct { double n, mean, m2; } welford_t;

static inline void welford_push(welford_t *w, double x) {
   w->n += 1.0;
   double d = x - w->mean;
   w->mean += d / w->n;
   w->m2 += d * (x - w->mean);
}
static inline double welford_var(const welford_t *w) { return w->m2 / (w->n - 1.0); }
static inline double welford_sd(const welford_t *w)  { return sqrt(welford_var(w)); }

/* Welch's t: (mean_a - mean_b) / sqrt(var_a/n_a + var_b/n_b) */
static inline double welch_t(const welford_t *a, const welford_t *b) {
   return (a->mean - b->mean) / sqrt(welford_var(a) / a->n + welford_var(b) / b->n);
}

static int ttest_cmp_u64(const void *x, const void *y) {
   uint64_t a = *(const uint64_t *)x, b = *(const uint64_t *)y;
   return (a > b) - (a < b);
}

/* ------------------------------ analysis -------------------------------- */
/*
 * t[i]   = cycles of measurement i,  cls[i] = 0 or 1 (its class)
 * Computes t on all measurements and keeping only those below the 99th, 95th
 * and 90th percentile (interrupts make long tails that hide differences),
 * prints the results if verbose, and returns the largest |t|.
 */
static double ttest_analyse(const char *name, const uint64_t *t, const int *cls,
                            long n, int verbose) {
   static const double crop[TTEST_N_CROPS] = { 1.00, 0.99, 0.95, 0.90 };
   uint64_t *sorted = malloc(n * sizeof(uint64_t));
   memcpy(sorted, t, n * sizeof(uint64_t));
   qsort(sorted, n, sizeof(uint64_t), ttest_cmp_u64);

   if (verbose)
      printf("%s: %ld misure, cicli min %lu  mediana %lu  max %lu\n", name, n,
             (unsigned long)sorted[0], (unsigned long)sorted[n / 2],
             (unsigned long)sorted[n - 1]);

   double max_t = 0;
   for (int c = 0; c < TTEST_N_CROPS; c++) {
      uint64_t cut = (crop[c] >= 1.0) ? UINT64_MAX : sorted[(long)(crop[c] * (n - 1))];
      welford_t w[2] = {{0}};
      for (long i = 0; i < n; i++)
         if (t[i] <= cut) welford_push(&w[cls[i]], (double)t[i]);
      double tv = fabs(welch_t(&w[0], &w[1]));
      if (tv > max_t) max_t = tv;
      if (verbose)
         printf("  taglio %3.0f%%: cl.0 %10.0f (sd %8.0f)  cl.1 %10.0f (sd %8.0f)"
                "  diff %+9.0f cicli  |t| = %8.2f\n",
                crop[c] * 100, w[0].mean, welford_sd(&w[0]), w[1].mean,
                welford_sd(&w[1]), w[0].mean - w[1].mean, tv);
   }
   if (verbose)
      printf("max |t| = %.2f -> %s\n", max_t,
             max_t > TTEST_THRESHOLD ? "LEAKAGE: il tempo dipende dall'input segreto"
                                     : "nessuna differenza rilevata con queste misure");
   free(sorted);
   return max_t;
}
