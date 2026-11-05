/* trace.c - oracolo TEORICO per il decoder "plain", single-trace.
 *
 * Esegue UNA decodifica con chiave ed errore fissi e, a ogni iterazione, logga
 * l'insieme delle linee/posizioni toccate dall'update della sindrome:
 *     { (Htr[b][k] + pos) mod P : k }   ridotto a linea se GRAN=set.
 * NON esegue la misura F+R: e' l'oracolo ideale (ground truth), per isolare la
 * domanda "il pattern di accesso determina la chiave?".
 *
 * Scrive la STESSA struttura a directory di attack.c, cosi' i tool a valle
 * (trace2support, keyrecover.py) sono condivisi:
 *     <TRUTH_DIR>/H0 , <TRUTH_DIR>/H1        chiave vera (posizioni bit, ordinate)
 *     <TRACE_DIR>/H0 , <TRACE_DIR>/H1        linee toccate, una riga per iterazione
 *     <TRACE_DIR>/pos_H0 , <TRACE_DIR>/pos_H1  argmax per iterazione
 *     <TRACE_DIR>/meta                       "P V N0 line gran"
 *
 * Env: GRAN (bit|set) LINE (64) NOCARRY (1 = ablazione del carry di linea)
 *      TRUTH_DIR (truth) TRACE_DIR (trace).  Le directory devono esistere.
 * argv[1] = seed (default 1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "harness.h"
#include "gf2x.h"
#include "upc.h"
#include "repr.h"

static int cmp_u32(const void *a, const void *b){
   uint32_t x=*(const uint32_t*)a, y=*(const uint32_t*)b; return (x>y)-(x<y);
}

int main(int argc, char **argv)
{
   unsigned seed = argc > 1 ? (unsigned)atoi(argv[1]) : 1;
   srand(seed);

   const char *trace_dir = getenv("TRACE_DIR") ? getenv("TRACE_DIR") : "trace";
   const char *truth_dir = getenv("TRUTH_DIR") ? getenv("TRUTH_DIR") : "truth";
   int set_gran = getenv("GRAN") && strcmp(getenv("GRAN"), "set") == 0;
   const char *_nc = getenv("NOCARRY");
   int nocarry = _nc && _nc[0] && _nc[0] != '0';      /* ablazione del carry */

   uint32_t line_bytes = 1, shift = 0;
   if (set_gran) {
      const char *g = getenv("LINE");
      line_bytes = g ? (uint32_t)atoi(g) : 64;
      if (line_bytes == 0 || (line_bytes & (line_bytes - 1)) != 0) {
         fprintf(stderr, "LINE='%s' non valida: potenza di 2\n", g ? g : ""); return 1;
      }
      uint32_t b = line_bytes; while (b > 1) { b >>= 1; shift++; }
   }
   uint32_t dom = (P + line_bytes - 1) / line_bytes;

   /* --- setup --- */
   static bfkey_t key; static instance_t in;
   key_generate(&key); instance_random(&in, &key);
   static ALIGNED uint8_t upc[PAD8(N0 * P)];
   static uint8_t syn[P];
   memset(upc, 0, sizeof upc);
   dense_to_u8(syn, in.s_dense, P);
   compute_upcs(upc, key.Htr_sparse, syn);
   __m256i v_H_sparse[N0][N_REGS_H];
   const __m256i lane_idx = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
   for (int b = 0; b < N0; b++)
      for (int r = 0; r < N_REGS_H; r++) {
         int rem = V - r * I32_IN_YMM;
         __m256i km = _mm256_cmpgt_epi32(_mm256_set1_epi32(rem), lane_idx);
         v_H_sparse[b][r] = _mm256_maskload_epi32((const int *)&key.H_sparse[b][r * I32_IN_YMM], km);
      }
   int hw = population_count(in.s_dense);
   static DIGIT e_out[N0 * NUM_DIGITS_GF2X_ELEMENT];
   memset(e_out, 0, sizeof e_out);

   /* file per-blocco (stessa struttura di attack.c) */
   char path[600];
   FILE *tf[N0], *pf[N0];
   for (int b = 0; b < N0; b++) {
      snprintf(path, sizeof path, "%s/H%d",     trace_dir, b); tf[b] = fopen(path, "w");
      snprintf(path, sizeof path, "%s/pos_H%d", trace_dir, b); pf[b] = fopen(path, "w");
      if (!tf[b] || !pf[b]) { fprintf(stderr, "non apro i file in %s/\n", trace_dir); return 1; }
   }

   uint8_t *seen = calloc(dom, 1);
   int iter = 0;
   while (iter < MAX_ITER && hw != 0) {
      POS col = argmax_u8_ct(upc);
      uint32_t b = col / P, pos = col % P;

      for (int k = 0; k < V; k++) {
         uint32_t row;
         if (nocarry) {
            /* riduci a LINEA prima di sommare -> niente carry (spazio-linea) */
            row  = (key.Htr_sparse[b][k] >> shift) + (pos >> shift);
            row -= (row >= dom) ? dom : 0;
         } else {
            /* reale: somma a livello bit, POI riduci -> il carry puo' esserci */
            row  = key.Htr_sparse[b][k] + pos;
            row -= (row >= P) ? P : 0;
            row >>= shift;
         }
         seen[row] = 1;
      }
      int first = 1;
      for (uint32_t s = 0; s < dom; s++)
         if (seen[s]) { fprintf(tf[b], first ? "%u" : " %u", s); seen[s] = 0; first = 0; }
      fprintf(tf[b], "\n");
      fprintf(pf[b], "%u\n", pos);

      gf2x_toggle_coeff(e_out + b * NUM_DIGITS_GF2X_ELEMENT, pos);
      hw = update_syndrome_and_upcs(upc, key.Htr_sparse, v_H_sparse, col, syn, hw);
      iter++;
   }
   for (int b = 0; b < N0; b++) { fclose(tf[b]); fclose(pf[b]); }
   free(seen);

   /* truth per-blocco: posizioni piene (bit), ordinate */
   for (int b = 0; b < N0; b++) {
      uint32_t *lines = malloc(V * sizeof(uint32_t)); int nn = 0;
      for (int k = 0; k < V; k++) lines[nn++] = key.Htr_sparse[b][k];
      qsort(lines, nn, sizeof(uint32_t), cmp_u32);
      snprintf(path, sizeof path, "%s/H%d", truth_dir, b);
      FILE *fo = fopen(path, "w");
      uint32_t prev = (uint32_t)-1;
      for (int i = 0; i < nn; i++) if (lines[i] != prev) { fprintf(fo, "%u\n", lines[i]); prev = lines[i]; }
      fclose(fo); free(lines);
   }
   /* meta */
   snprintf(path, sizeof path, "%s/meta", trace_dir);
   FILE *mf = fopen(path, "w");
   fprintf(mf, "%d %d %d %u %s\n", P, V, N0, line_bytes, set_gran ? "set" : "bit");
   fclose(mf);

   fprintf(stderr, "trace: seed=%u gran=%s line=%u carry=%s iter=%d (decode %s)\n",
           seed, set_gran ? "set" : "bit", line_bytes, nocarry ? "OFF" : "on",
           iter, hw == 0 ? "ok" : "non converge");
   return 0;
}
