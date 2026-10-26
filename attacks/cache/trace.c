/*
 * trace.c - oracolo di accesso per il decoder "plain", single-trace.
 *
 * Esegue UNA decodifica con chiave ed errore fissi (come la vittima di un
 * attacco di cache) e, a ogni iterazione, emette SOLO l'insieme delle
 * posizioni della sindrome toccate da update_syndrome_and_upcs:
 *
 *     { (Htr[b_t][k] + flip_t) mod P : k = 0..V-1 }
 *
 * Non emette ne' il blocco b_t ne' la posizione flippata flip_t: e' esatta-
 * mente cio' che un Prime+Probe sulla L1 osserverebbe (quali linee/posizioni
 * sono state toccate), non il valore del segreto. Con GRAN=set le posizioni
 * sono ridotte all'indice di linea di cache (>>6), come nel canale reale;
 * con GRAN=bit (default) resta la posizione esatta: e' l'oracolo ideale, che
 * isola la domanda crittografica "il pattern di accesso determina la chiave?".
 *
 * Output (stdout): una riga per iterazione, interi separati da spazio =
 * insiemi toccati (ordinati, senza duplicati). In testa una riga "# D <dom>".
 * Il file di verita' (argv[1]) contiene i supporti VERI di Htr[0], Htr[1]:
 * lo usa solo lo scorer, l'attaccante no.
 *
 * Uso: trace [truth_file] [seed]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "harness.h"
#include "gf2x.h"
#include "upc.h"
#include "repr.h"

int main(int argc, char **argv)
{
   /* Reference file for the keys */
   const char *truth = argc > 1 ? argv[1] : "truth.txt";
   /* You can provide a fixed seed as second parameters */
   unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : 1;
   srand(seed);

   /* Granularity of the reported positions, modelling two attacker capabilities: */
   /* - bit: it is the strongest possible attacker and gives an upper bound on what is leaked. Report exactly the position touched */
   /*        This answer to the question — does the access pattern determine the key?*/
   /* - set (cache line): Each position is reduced to its cache line. This matches what a Prime+Probe adversary actually observes: */
   /*       which cache line/set was accessed, not the exact byte.*/   
   int set_gran = getenv("GRAN") && strcmp(getenv("GRAN"), "set") == 0;
   uint32_t shift = set_gran ? 6 : 0;
   /* number of possible traces, which depens on the granularity (P because with bit they are all different, P/64 for sets because they can collide) */
   uint32_t dom   = set_gran ? ((P + 63) / 64) : P;

   /* ------------------------------------------------------------------------------------------------------------------------------------ */
   /* Setup                                                                                                                                */ 
   /* ------------------------------------------------------------------------------------------------------------------------------------ */
   static bfkey_t key;
   static instance_t in;
   key_generate(&key);
   instance_random(&in, &key);

   static ALIGNED uint8_t upc[PAD8(N0 * P)];
   static uint8_t syn[P];
   memset(upc, 0, sizeof upc);
   dense_to_u8(syn, in.s_dense, P);
   compute_upcs(upc, key.Htr_sparse, syn);

   __m256i v_H_sparse[N0][N_REGS_H];
   const __m256i lane_idx = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
   for (int b = 0; b < N0; b++){
      for (int r = 0; r < N_REGS_H; r++) {
         int rem = V - r * I32_IN_YMM;
         __m256i km = _mm256_cmpgt_epi32(_mm256_set1_epi32(rem), lane_idx);
         v_H_sparse[b][r] = _mm256_maskload_epi32((const int *)&key.H_sparse[b][r * I32_IN_YMM], km);
      }
   }

   int hw = population_count(in.s_dense);
   static DIGIT e_out[N0 * NUM_DIGITS_GF2X_ELEMENT];
   memset(e_out, 0, sizeof e_out);

   printf("# D %u\n", dom);

   /* seen[]: deduplica le posizioni (o linee) toccate in questa iterazione */
   uint8_t *seen = calloc(dom, 1);
   int iter = 0;
   while (iter < MAX_ITER && hw != 0) {
      POS col = argmax_u8_ct(upc);
      uint32_t b = col / P, pos = col % P;

      /* ------------------------------------------------------------------------------------------------------------------------------------ */
      /* Logging of the position that will be touched during the update                                                                       */
      /* ------------------------------------------------------------------------------------------------------------------------------------ */
      for (int k = 0; k < V; k++) {
         uint32_t row = key.Htr_sparse[b][k] + pos;
         row -= (row >= P) ? P : 0;
         seen[row >> shift] = 1;
      }
      int first = 1;
      for (uint32_t s = 0; s < dom; s++)
         if (seen[s]) { printf(first ? "%u" : " %u", s); seen[s] = 0; first = 0; }
      printf("\n");
      /* ------------------------------------------------------------------------------------------------------------------------------------ */

      gf2x_toggle_coeff(e_out + b * NUM_DIGITS_GF2X_ELEMENT, pos);
      hw = update_syndrome_and_upcs(upc, key.Htr_sparse, v_H_sparse, col, syn, hw);
      iter++;
   }
   free(seen);

   /* Reference file for the keys, it will be not used by the attacker */
   FILE *f = fopen(truth, "w");
   fprintf(f, "P %d V %d N0 %d gran %s iters %d\n", P, V, N0, set_gran ? "set" : "bit", iter);
   for (int b = 0; b < N0; b++) {
      for (int k = 0; k < V; k++) fprintf(f, "%s%u", k ? " " : "", key.Htr_sparse[b][k]);
      fprintf(f, "\n");
   }
   fclose(f);

   fprintf(stderr, "trace: seed=%u gran=%s iterazioni=%d (decode %s)\n",
           seed, set_gran ? "set" : "bit", iter, hw == 0 ? "ok" : "non converge");
   return 0;
}

