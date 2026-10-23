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
 * Uso: ttest [measurements]   (default 20000)
 * Options (environment variables):
 *   SEED=<n>         fix the key and e_fixed (default 42)
 *   MODE=lowhigh     class 0: random e with all positions in the lowest quarter
 *                    of [0, N0*P); class 1: random e in the highest quarter.
 *                    Both classes are random: a difference can only come from
 *                    where the errors are (e.g. the early exit of the argmax).
 *   NATIVE=1         time only decode_native(): decode_prepare (expanded
 *                    syndrome, per-key tables, ...) is done in the untimed
 *                    batch phase. Default: time decode().
 *
 * The decoder is selected at build time (make VARIANT=... TOOL=ttest).
 */
 #include <string.h>
 #include "harness.h"
 #include "ttest.h"
 
 #define BATCH 32
 
 int main(int argc, char *argv[]) {
    long n_meas = (argc > 1) ? atol(argv[1]) : 20000;
    unsigned seed = getenv("SEED") ? (unsigned)atoi(getenv("SEED")) : 42;
    int lowhigh = getenv("MODE") && strcmp(getenv("MODE"), "lowhigh") == 0;
    int native = getenv("NATIVE") && atoi(getenv("NATIVE")) != 0;
    const uint32_t quarter = (N0 * P) / 4;
    srand(seed);
 
    /* key, fixed for the whole test */
    static bfkey_t key;
    key_generate(&key);
 
    /* fixed-class input */
    static instance_t in_fixed;
    instance_random(&in_fixed, &key);
 
    /* work slots, shared by the two classes */
    static instance_t slot[BATCH];
    static dec_params_t slot_in[BATCH];
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
             instance_generate(&slot[k], &key, slot_cls[k] ? N0 * P - quarter : 0, quarter);
          } else if (slot_cls[k] == 0) {
             memcpy(&slot[k], &in_fixed, sizeof(instance_t));
          } else {
             instance_random(&slot[k], &key);
          }
          if (native) decode_prepare(&slot_in[k], key.Htr_sparse, key.H_sparse, slot[k].s_dense);
       }
       /* measurements */
       for (int k = 0; k < BATCH && i < n_meas; k++, i++) {
          memset(e_out, 0, sizeof(e_out));
          uint64_t t0 = cycles_start();
          if (native) sink += decode_native(e_out, &slot_in[k]);
          else        sink += decode(e_out, key.Htr_sparse, key.H_sparse, slot[k].s_dense);
          uint64_t t1 = cycles_stop();
          fails += memcmp(e_out, slot[k].e_dense, sizeof(e_out)) != 0;
          t_all[i] = t1 - t0;
          cls_all[i] = slot_cls[k];
       }
       if ((i % 1024) == 0) fprintf(stderr, "\r  %ld/%ld", i, n_meas);
    }
    fprintf(stderr, "\n");
 
    const char *name = decoder_variant_name();
    printf("%s | P=%d V=%d t=%d | seed %u | %s | %s | %ld decodifiche errate\n",
           name, P, V, NUM_ERRORS_T, seed, lowhigh ? "low vs high" : "fixed vs random",
           native ? "decode_native" : "decode", fails);
    double max_t = ttest_analyse(name, t_all, cls_all, n_meas, 1);
 
    /* riga per gli script */
    printf("RESULT variant=%s mode=%s target=%s n=%ld max_t=%.2f verdict=%s\n", name,
           lowhigh ? "lowhigh" : "fixed_random", native ? "decode_native" : "decode", n_meas, max_t,
           max_t > TTEST_THRESHOLD ? "leak" : "ok");
 
    (void)sink;
    free(t_all); free(cls_all);
    return 0;
 }
 
 