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

   int set_gran = getenv("GRAN") && strcmp(getenv("GRAN"), "set") == 0;

   /* larghezza linea in byte (solo per granularita' 'set'): env LINE, default 64.
    * deve essere una potenza di 2; shift = log2(LINE). bit  ->  shift 0, LINE 1. */
   uint32_t line_bytes = 1;          /* bit: 1 posizione per "linea" */
   uint32_t shift      = 0;
   if (set_gran) {
      const char *g = getenv("LINE");
      line_bytes = g ? (uint32_t)atoi(g) : 64;
      if (line_bytes == 0 || (line_bytes & (line_bytes - 1)) != 0) {
         fprintf(stderr, "LINE='%s' non valida: usa una potenza di 2 (8,16,32,64...)\n",
                 g ? g : "");
         return 1;
      }
      uint32_t b = line_bytes;
      while (b > 1) { b >>= 1; shift++; }          /* shift = log2(line_bytes) */
   }
   /* domain */
   uint32_t dom = (P + line_bytes - 1) / line_bytes;   /* = ceil(P / LINE); bit -> P */

   /* -------------------------------------------------------------------------------------------- */
   /* Setup */ 
   /* -------------------------------------------------------------------------------------------- */
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

  	/* --------------------------------------------------------------------------------------- */
   	/* Logging of the position will be touched during the update */ 
   	/* --------------------------------------------------------------------------------------- */
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
   fprintf(f, "P %d V %d N0 %d gran %s line %u iters %d\n",
      P, V, N0, set_gran ? "set" : "bit", line_bytes, iter);
   for (int b = 0; b < N0; b++) {
      for (int k = 0; k < V; k++) fprintf(f, "%s%u", k ? " " : "", key.Htr_sparse[b][k]);
      fprintf(f, "\n");
   }
   fclose(f);

   fprintf(stderr, "trace: seed=%u gran=%s iterazioni=%d (decode %s)\n",
           seed, set_gran ? "set" : "bit", iter, hw == 0 ? "ok" : "non converge");
   return 0;
}

