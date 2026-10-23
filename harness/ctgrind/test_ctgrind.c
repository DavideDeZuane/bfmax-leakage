 /*
 * Uso: valgrind --error-limit=no ./ctgrind [runs]
 *      (oppure scripts/run_ctgrind.sh, che classifica i report)
 *
 * Dati marcati segreti DOPO la generazione: gli input LEDA del decoder,
 * cioe' HPosOnes, HtrPosOnes e la sindrome (dipende dalla chiave).
 * Tutto cio' che decode_prepare ne ricava (sindrome espansa, tabelle
 * per-chiave, ...) eredita automaticamente il "segreto": non serve conoscere
 * i parametri interni di ciascun decoder.
 *
 *   "Conditional jump or move depends on uninitialised value(s)" -> branch
 *   "Use of uninitialised value of size N"                       -> indirizzo
 */
 #include <string.h>
 #include "harness.h"
 #include "secret.h"
 
 int main(int argc, char **argv)
 {
     long runs = argc > 1 ? atol(argv[1]) : 1;
     srand(42);
 
     static bfkey_t key;
     static instance_t in;
     static ALIGNED DIGIT s[NUM_DIGITS_GF2X_ELEMENT];
     static ALIGNED DIGIT e_out[N0 * NUM_DIGITS_GF2X_ELEMENT];
     long wrong = 0;
 
     for (long r = 0; r < runs; r++) {
         key_generate(&key);
         instance_random(&in, &key);
         memcpy(s, in.s_dense, sizeof s);
 
         poison(key.H_sparse,   sizeof key.H_sparse);
         poison(key.Htr_sparse, sizeof key.Htr_sparse);
         poison(s,              sizeof s);
 
         decode(e_out, key.Htr_sparse, key.H_sparse, s);
 
         /* l'output dipende dal segreto per definizione: non e' un leak */
         unpoison(e_out, sizeof e_out);
         unpoison(&key,  sizeof key);
         wrong += memcmp(e_out, in.e_dense, sizeof e_out) != 0;
     }
     printf("variant=%s runs=%ld wrong=%ld\n", decoder_variant_name(), runs, wrong);
     return 0;
 }
 
 