/* ------------------------------------------------------------------------------*/
/*                 FIXED-VS-RANDOM WELCH t-TEST (dudect methodology)             */
/* ------------------------------------------------------------------------------*/
/*
 * Measures how much the execution time of the BF-max decoder depends on the
 * secret error vector e.
 *
 *   key:           fixed for the whole test (keygen + bfmax_ctx_init once)
 *   class fixed:   always the same error vector e_fixed
 *   class random:  a fresh random error vector at every measurement
 *
 * Inputs are prepared in batches BEFORE timing and copied into the same work
 * buffers whatever the class, so the two classes run the same code on the
 * same memory and differ only in the value of e.
 *
 *   |t| > 4.5  -> the time depends on e (leakage)
 *
 * Build/run:  ./start.sh ttest  [measurements]  (default 20000)
 * Options (environment variables):
 *   DECODER=plain    use bfmax_decoder from bfmax.h instead of the steroids one
 *   SEED=<n>         fix the key and e_fixed (default 42)
 *   MODE=lowhigh     class 0: random e with all positions in the lowest quarter
 *                    of [0, N0*P); class 1: random e in the highest quarter.
 *                    Both classes are random: a difference can only come from
 *                    where the errors are (e.g. the early exit of the argmax).
 */
#include "helpers.h"
#include "defines.h"
#include "test_utils.h"
#ifdef USE_PLAIN_DECODER
#include "bfmax.h"
#else
#include "steroids.h"
#endif
#include "t_test_utils.h"

/* ------------------------------ inputs ---------------------------------- */
typedef struct {
   uint8_t  syndrome_bits[P];
   DIGIT    e_dense[N0 * NUM_DIGITS_GF2X_ELEMENT];
} input_t;

/* t distinct random positions in [lo, lo + range) */
static void rand_error_in_range(uint32_t e_sparse[NUM_ERRORS_T], uint32_t lo, uint32_t range) {
   u32_arr_rand_mod_unique(e_sparse, NUM_ERRORS_T, range);
   for (int i = 0; i < NUM_ERRORS_T; i++) e_sparse[i] += lo;
}

static void make_input(input_t *in, POS Htr_sparse[N0][PAD32(V)], uint32_t e_sparse[NUM_ERRORS_T]) {
   uint64_t s_dense[NUM_DIGITS_GF2X_ELEMENT] = {0};
   memset(in->e_dense, 0, sizeof(in->e_dense));
   util_densify_error(in->e_dense, e_sparse);
   util_compute_syndrome(s_dense, Htr_sparse, e_sparse);
   dense_to_u8(in->syndrome_bits, s_dense, P);
}

#define BATCH 32

int main(int argc, char *argv[]) {
   long n_meas = (argc > 1) ? atol(argv[1]) : 20000;
   unsigned seed = getenv("SEED") ? (unsigned)atoi(getenv("SEED")) : 42;
   int lowhigh = getenv("MODE") && strcmp(getenv("MODE"), "lowhigh") == 0;
   const uint32_t quarter = (N0 * P) / 4;
   srand(seed);

   /* key, fixed for the whole test */
   static ALIGNED POS H_sparse[N0][PAD32(V)];
   static ALIGNED POS Htr_sparse[N0][PAD32(V)];
   keygen(H_sparse, Htr_sparse);
   static bfmax_ctx_t ctx;
   bfmax_ctx_init(&ctx, Htr_sparse, H_sparse);

   /* fixed-class input */
   static input_t in_fixed;
   uint32_t e_sparse[NUM_ERRORS_T];
   u32_arr_rand_mod_unique(e_sparse, NUM_ERRORS_T, N0 * P);
   make_input(&in_fixed, Htr_sparse, e_sparse);

   /* work slots, shared by the two classes */
   static input_t slot[BATCH];
   static ALIGNED DIGIT e_out[N0 * NUM_DIGITS_GF2X_ELEMENT];
   int slot_cls[BATCH];

   uint64_t *t_all = malloc(n_meas * sizeof(uint64_t));
   int *cls_all = malloc(n_meas * sizeof(int));
   long fails = 0;
   volatile int sink = 0;

   for (long i = 0; i < n_meas; ) {
      /* preparation (not timed) */
      for (int k = 0; k < BATCH; k++) {
         slot_cls[k] = rand() & 1;
         if (lowhigh) {
            rand_error_in_range(e_sparse, slot_cls[k] ? N0 * P - quarter : 0, quarter);
            make_input(&slot[k], Htr_sparse, e_sparse);
         } else if (slot_cls[k] == 0) {
            memcpy(&slot[k], &in_fixed, sizeof(input_t));
         } else {
            u32_arr_rand_mod_unique(e_sparse, NUM_ERRORS_T, N0 * P);
            make_input(&slot[k], Htr_sparse, e_sparse);
         }
      }
      /* measurements */
      for (int k = 0; k < BATCH && i < n_meas; k++, i++) {
         memset(e_out, 0, sizeof(e_out));
         uint64_t t0 = ttest_cycles_start();
#ifdef USE_PLAIN_DECODER
         int hw = 0;
         for (int b = 0; b < P; b++) hw += slot[k].syndrome_bits[b];
         sink += bfmax_decoder(e_out, Htr_sparse, H_sparse, slot[k].syndrome_bits, hw);
#else
         sink += bfmax_decoder_steroids(e_out, Htr_sparse, H_sparse, slot[k].syndrome_bits, &ctx);
#endif
         uint64_t t1 = ttest_cycles_stop();
         fails += memcmp(e_out, slot[k].e_dense, sizeof(e_out)) != 0;
         t_all[i] = t1 - t0;
         cls_all[i] = slot_cls[k];
      }
      if ((i % 1024) == 0) fprintf(stderr, "\r  %ld/%ld", i, n_meas);
   }
   fprintf(stderr, "\n");

#ifdef USE_PLAIN_DECODER
   const char *name = "bfmax_decoder (bfmax.h)";
#else
   const char *name = "bfmax_decoder_steroids";
#endif
   printf("%s | P=%d V=%d t=%d | seed %u | %s | %ld decodifiche errate\n",
          name, P, V, NUM_ERRORS_T, seed, lowhigh ? "low vs high" : "fixed vs random", fails);
   ttest_analyse(name, t_all, cls_all, n_meas, 1);

   (void)sink;
   free(t_all); free(cls_all);
   return 0;
}
