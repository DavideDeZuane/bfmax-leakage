/*
 * bench_decoder - cicli della decodifica (era main.c con -DBENCH).
 *
 * Uso: bench_decoder [runs] [seed] [--csv] [--header]
 *   default una tabella leggibile, con --csv una riga CSV per target
 *
 * Tre target, sempre riportati tutti:
 *   decode          decode() con la firma LEDA: preparazione + decoder
 *   decode_native   solo decode_native(): decode_prepare e' fatta prima,
 *                   fuori dalla misura (confrontabile con main.c)
 *   decode_prepare  solo la preparazione (sindrome espansa, tabelle, ...)
 *
 * decode ~= decode_prepare + decode_native.
 */
 #include <string.h>
 #include "harness.h"
 #include "bench_common.h"
 
 #define POOL   32
 #define WARMUP 100
 
 int main(int argc, char **argv)
 {
     bench_args_t args = bench_parse_args(argc, argv, 1000);
     size_t runs = args.runs;
     srand(args.seed);
 
     static bfkey_t key;
     key_generate(&key);
     static instance_t pool[POOL];
     for (int i = 0; i < POOL; i++) instance_random(&pool[i], &key);
 
     static ALIGNED DIGIT e_out[N0 * NUM_DIGITS_GF2X_ELEMENT];
     static dec_params_t din;
     uint64_t *s_full = malloc(runs * sizeof *s_full);
     uint64_t *s_nat  = malloc(runs * sizeof *s_nat);
     uint64_t *s_prep = malloc(runs * sizeof *s_prep);
     if (!s_full || !s_nat || !s_prep) { perror("malloc"); return 1; }
 
     long wrong = 0;
     for (size_t i = 0; i < runs + WARMUP; i++) {
         const instance_t *in = &pool[i % POOL];
         uint64_t t0, t1, d_full, d_nat, d_prep;
 
         /* decode completo dall'input canonico */
         t0 = cycles_start();
         decode(e_out, key.Htr_sparse, key.H_sparse, (DIGIT *)in->s_dense);
         t1 = cycles_stop();
         d_full = t1 - t0;
         wrong += memcmp(e_out, in->e_dense, sizeof e_out) != 0;
 
         /* solo preparazione */
         t0 = cycles_start();
         decode_prepare(&din, key.Htr_sparse, key.H_sparse, in->s_dense);
         t1 = cycles_stop();
         d_prep = t1 - t0;
 
         /* solo decoder nativo (input gia' preparato sopra) */
         t0 = cycles_start();
         decode_native(e_out, &din);
         t1 = cycles_stop();
         d_nat = t1 - t0;
         wrong += memcmp(e_out, in->e_dense, sizeof e_out) != 0;
 
         if (i >= WARMUP) {
             s_full[i - WARMUP] = d_full;
             s_nat[i - WARMUP]  = d_nat;
             s_prep[i - WARMUP] = d_prep;
         }
     }
     if (wrong) fprintf(stderr, "ATTENZIONE: %ld decodifiche errate\n", wrong);
 
     bench_stat_t st[3] = {
         bench_stat("decode",         s_full, runs),
         bench_stat("decode_native",  s_nat,  runs),
         bench_stat("decode_prepare", s_prep, runs),
     };
     bench_print(&args, "bench_decoder", st, 3, 0);   /* % rispetto a decode */
     free(s_full); free(s_nat); free(s_prep);
     return 0;
 }
 
 
