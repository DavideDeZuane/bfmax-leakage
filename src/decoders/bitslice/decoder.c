#include <string.h>
#include <immintrin.h>
#include "decoder.h"
#include "ct.h"

#ifndef CT_ITERATIONS
#define CT_ITERATIONS MAX_ITER
#endif

#define NW        NUM_DIGITS_GF2X_ELEMENT                 /* digit significative       */
#define NWS       NUM_DIGITS_SLICED                       /* digit allocate (padding)  */
#define TAIL_BITS (P % DIGIT_SIZE_b)                      /* bit validi nell'ultima    */
#define TAIL_MASK (TAIL_BITS ? (((DIGIT)1 << TAIL_BITS) - 1) : ~(DIGIT)0)
#define ROT_STAGES BITS_TO_REPRESENT(P - 1)               /* bit dell'esponente < P    */

/*#########################################################################################*/
/* CONSTANT-TIME HELPERS                                                                   */
/*#########################################################################################*/

/* 0xFF..FF se a == b, 0 altrimenti */
static inline uint64_t ct_mask_eq64(uint64_t a, uint64_t b)
{
   return ~ct_nz64_mask(a ^ b);
}

/* a se m = 0xFF..FF, b se m = 0 */
static inline uint64_t ct_select64(uint64_t m, uint64_t a, uint64_t b)
{
   return (a & m) | (b & ~m);
}

/*#########################################################################################*/
/* ROTAZIONE CONSTANT-TIME: res = x^k * a  mod (x^P - 1)                                    */
/*#########################################################################################*/
/*
 * Come rotate_right di BIKE:
 *   1) dup = a | x^P * a  (2P bit): ogni rotazione di a e' una finestra di P
 *      bit di dup. Si costruisce UNA volta per polinomio e si riusa per tutte
 *      le rotazioni (le 2*V del calcolo dei contatori usano la stessa sindrome).
 *   2) finestra a offset m = P - k (in [1, P]): x^k * a ha nel bit i il
 *      coefficiente (i - k) mod P = bit (i + m) di dup.
 *   3) offset in qword q = m / 64 (segreto): barrel shifter con DUP_STAGES
 *      stadi pubblici di 2^j qword, ognuno applicato con una maschera, dal
 *      piu' grande al piu' piccolo su un buffer che si accorcia.
 *   4) offset in bit r = m % 64 (segreto): altri 6 stadi pubblici di 1..32
 *      bit con maschere (default), oppure con -DCT_VARSHIFT un solo shift di
 *      quantita' variabile come BIKE (constant-time sull'hardware, ma
 *      segnalato da ctgrind).
 */
#define DUP_LEN     (2 * NW)                                  /* qword di dup              */
#define DUP_STAGES  BITS_TO_REPRESENT(P / DIGIT_SIZE_b)       /* q <= P/64                 */
#define DUP_SLACK   (1 << DUP_STAGES)                         /* letture oltre DUP_LEN     */

typedef struct {
   ALIGNED DIGIT w[DUP_LEN + DUP_SLACK];                      /* coda sempre a zero        */
} dup_t;

/* dup = a | x^P * a: shift sinistro di P bit, quantita' pubblica */
static void dup_build(dup_t *dup, const DIGIT a[NWS])
{
   const uint32_t q = P / DIGIT_SIZE_b, r = P % DIGIT_SIZE_b;
   memset(dup, 0, sizeof *dup);
   for (uint32_t i = 0; i < NW; i++) dup->w[i] = a[i];
   for (uint32_t i = 0; i < NW; i++) {
      dup->w[i + q] |= a[i] << r;
      if (r) dup->w[i + q + 1] |= a[i] >> (DIGIT_SIZE_b - r);
   }
}

/* res = x^k * a mod (x^P - 1), con dup = dup_build(a) e k segreto in [0, P) */
static void rot_from_dup(DIGIT res[NWS], const dup_t *dup, uint32_t k)
{
   ALIGNED DIGIT buf0[DUP_LEN + DUP_SLACK], buf1[DUP_LEN + DUP_SLACK];
   const uint32_t m = P - k;                                  /* in [1, P]                 */
   const uint64_t q = m / DIGIT_SIZE_b;
   const uint64_t r = m % DIGIT_SIZE_b;

   /* 3) barrel shifter sulle qword, dallo stadio piu' grande al piu' piccolo
    *    (gli stadi commutano): dopo lo stadio j restano da fare al massimo
    *    2^j - 1 qword di spostamento, quindi servono solo NW + 2^j + 1 qword
 *    (la +1 e' letta dallo shift di bit).
    *    src/dst separati, cosi' il compilatore vettorizza le select. */
   /* il primo stadio legge direttamente da dup: nessuna copia */
   const DIGIT *src = dup->w;
   DIGIT *dst = buf1, *other = buf0;
   for (int j = DUP_STAGES - 1; j >= 0; j--) {
      const uint32_t sh   = (uint32_t)1 << j;
      const uint32_t len  = NW + sh + 1;             /* qword ancora necessarie (+1 per lo shift di bit) */
      const DIGIT    take = ct_nz64_mask((q >> j) & 1);
      for (uint32_t i = 0; i < len; i++)
         dst[i] = (src[i + sh] & take) | (src[i] & ~take);
      src = dst; dst = other; other = (DIGIT *)src;
   }

   /* 4) shift di r bit, r segreto in [0, 63] */
#if defined(CT_VARSHIFT)
   /* un solo shift di quantita' variabile (come BIKE): constant-time
    * sull'hardware x86, ma valgrind lo segnala perche' modella lo shift con
    * un confronto sul contatore. (x << 1) << (63 - r) evita lo shift di 64. */
   for (uint32_t i = 0; i < NW; i++)
      res[i] = (src[i] >> r) | ((src[i + 1] << 1) << (DIGIT_SIZE_b - 1 - r));
#else
   /* default: barrel shifter anche sui bit, 6 stadi PUBBLICI (1..32 bit)
    * selezionati con maschere: nessuno shift di quantita' segreta, quindi
    * pulito anche per ctgrind */
   for (int b = 0; b < 6; b++) {
      const uint32_t sb   = (uint32_t)1 << b;
      const uint32_t len  = NW + 1;                   /* letture fino a i + 1 */
      const DIGIT    take = ct_nz64_mask((r >> b) & 1);
      for (uint32_t i = 0; i < len; i++) {
         const DIGIT shifted = (src[i] >> sb) | (src[i + 1] << (DIGIT_SIZE_b - sb));
         dst[i] = (shifted & take) | (src[i] & ~take);
      }
      dst[len] = 0;
      src = dst; dst = other; other = (DIGIT *)src;
   }
   for (uint32_t i = 0; i < NW; i++) res[i] = src[i];
#endif
   res[NW - 1] &= TAIL_MASK;
   for (uint32_t i = NW; i < NWS; i++) res[i] = 0;
}

/* res = x^k * a mod (x^P - 1) per un solo prodotto */
static void gf2x_mod_mul_monom_ct(DIGIT res[NWS], uint32_t k, const DIGIT a[NWS])
{
   dup_t dup;
   dup_build(&dup, a);
   rot_from_dup(res, &dup, k);
}

/*#########################################################################################*/
/* CONTATORI BITSLICED                                                                     */
/*#########################################################################################*/

/* bs_res += sum_i x^sparse[i] * a, con dup = dup_build(a). Dopo i+1 addizioni
 * i contatori valgono al massimo i+1: bastano BITS_TO_REPRESENT(i+1) piani
 * (come LOG2_MSB(j + 1) in BIKE), quelli sopra restano a zero. */
static void lift_mul_dense_to_sparse_ct(bs_operand_t bs_res[NUM_SLICES_GF2X_ELEMENT],
                                        const dup_t *dup,
                                        const POS sparse[], unsigned int nPos)
{
   ALIGNED DIGIT tmp[NWS];
   for (unsigned int i = 0; i < nPos; i++) {
      const int planes = 32 - __builtin_clz(i + 1);          /* i pubblico */
      rot_from_dup(tmp, dup, sparse[i]);
      for (int j = 0; j < NUM_SLICES_GF2X_ELEMENT; j++)
         bitslice_inc_planes(&bs_res[j], _mm256_load_si256((const __m256i *)&tmp[j * DIGITS_IN_SLICE]), planes);
   }
}

/*
 * argmax constant-time sui contatori bitsliced.
 *
 * Restituisce block*P + j del contatore massimo; a parita' sceglie l'indice
 * globale piu' basso (prima il blocco, poi j), come gli altri decoder.
 *
 * Fase 1: dal piano piu' significativo al meno significativo, tiene solo i
 *         candidati che hanno quel bit a 1, se ne esiste almeno uno. Tutti i
 *         piani sono sempre processati, la scelta e' fatta con una maschera.
 * Fase 2: per ogni blocco scorre tutte le digit e tiene la PRIMA digit non
 *         nulla (little-endian: digit piu' bassa = esponente piu' basso), poi
 *         il bit piu' basso al suo interno.
 */
static POS argmax_bitsliced_ct(const bs_operand_t bs[N0 * NUM_SLICES_GF2X_ELEMENT])
{
   __m256i candidate[N0 * NUM_SLICES_GF2X_ELEMENT] __attribute__((aligned(32)));
   __m256i new_cand[N0 * NUM_SLICES_GF2X_ELEMENT]  __attribute__((aligned(32)));

   /* candidati iniziali: tutti i coefficienti validi. Esclusi i bit oltre P
    * nell'ultima digit e le digit di padding (indici pubblici) */
   for (int b = 0; b < N0; b++) {
      DIGIT *c = (DIGIT *)&candidate[b * NUM_SLICES_GF2X_ELEMENT];
      for (int d = 0; d < NWS; d++)
         c[d] = (d < NW) ? ~(DIGIT)0 : (DIGIT)0;
      c[NW - 1] = TAIL_MASK;
   }

   /* FASE 1 */
   for (int i = BITSLICED_OPERAND_WIDTH - 1; i >= 0; i--) {
      __m256i any = _mm256_setzero_si256();
      for (int z = 0; z < N0 * NUM_SLICES_GF2X_ELEMENT; z++) {
         new_cand[z] = _mm256_and_si256(candidate[z], bs[z].slice[i]);
         any = _mm256_or_si256(any, new_cand[z]);
      }
      /* keep = 1..1 se almeno un candidato ha il bit i a 1 */
      uint64_t keep = ct_nz64_mask((uint64_t)(1 - _mm256_testz_si256(any, any)));
      __m256i vkeep = _mm256_set1_epi64x((long long)keep);
      for (int z = 0; z < N0 * NUM_SLICES_GF2X_ELEMENT; z++)
         candidate[z] = _mm256_or_si256(_mm256_and_si256(new_cand[z], vkeep),
                                        _mm256_andnot_si256(vkeep, candidate[z]));
   }

   /* FASE 2: blocchi dall'ultimo al primo, cosi' vince il blocco piu' basso */
   uint64_t res_block = 0, res_j = 0;
   for (int b = N0 - 1; b >= 0; b--) {
      const DIGIT *c = (const DIGIT *)&candidate[b * NUM_SLICES_GF2X_ELEMENT];
      uint64_t best_d = 0, best_w = 0, has = 0;
      for (uint64_t d = 0; d < NW; d++) {
         uint64_t nz  = ct_nz64_mask(c[d]);
         uint64_t sel = nz & ~has;                /* solo la prima digit non nulla */
         best_d = ct_select64(sel, d, best_d);
         best_w = ct_select64(sel, c[d], best_w);
         has |= nz;
      }
      /* se il blocco non ha candidati best_w = 0: "| (~has & 1)" evita ctz(0),
       * il valore viene comunque scartato dalla select */
      uint64_t j = best_d * DIGIT_SIZE_b + ct_ctz64(best_w | (~has & 1));
      res_block = ct_select64(has, (uint64_t)b, res_block);
      res_j     = ct_select64(has, j, res_j);
   }
   return (POS)(res_block * P + res_j);
}

/*#########################################################################################*/
/* AGGIORNAMENTI CONSTANT-TIME                                                             */
/*#########################################################################################*/

/* out[block] ^= x^exponent se enable = 0xFF..FF. Tocca tutte le digit e cambia
 * solo quella giusta: l'indirizzo scritto non dipende da block ne' da exponent. */
static inline void gf2x_toggle_coeff_ct(DIGIT out[], uint64_t block, uint64_t exponent, uint64_t enable)
{
   uint64_t target = block * NW + exponent / DIGIT_SIZE_b;
   DIGIT bit       = (DIGIT)1 << (exponent % DIGIT_SIZE_b);
   for (uint64_t d = 0; d < (uint64_t)N0 * NW; d++)
      out[d] ^= bit & ct_mask_eq64(d, target) & enable;
}

/* syndrome ^= x^exponent * HTr[block] se enable = 0xFF..FF. HTr[block] e'
 * selezionato con una maschera (si leggono tutti i blocchi), poi una sola
 * rotazione constant-time. */
static inline void syndrome_update_ct(DIGIT syndrome[NWS], const DIGIT HTr[N0][NWS],
                                      uint64_t block, uint64_t exponent, uint64_t enable)
{
   ALIGNED DIGIT htr_sel[NWS];
   ALIGNED DIGIT update[NWS];
   for (int d = 0; d < NWS; d++) {
      DIGIT acc = 0;
      for (int b = 0; b < N0; b++) acc |= HTr[b][d] & ct_mask_eq64(block, (uint64_t)b);
      htr_sel[d] = acc;
   }
   gf2x_mod_mul_monom_ct(update, (uint32_t)exponent, htr_sel);
   for (int d = 0; d < NWS; d++)
      syndrome[d] ^= update[d] & enable;
}

/*#########################################################################################*/
/* DECODER                                                                                 */
/*#########################################################################################*/
int bf_decoding_ct(OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT], IN dec_params_t *p)
{
   memset(out, 0, N0 * NUM_DIGITS_GF2X_ELEMENT * sizeof(DIGIT));

   /* numero fisso di iterazioni; dopo la convergenza flip e update sono
    * disattivati dalla maschera active invece che da un'uscita anticipata */
   for (int iter = 0; iter < CT_ITERATIONS; iter++) {

      bs_operand_t bs[N0 * NUM_SLICES_GF2X_ELEMENT];
      memset(bs, 0, sizeof(bs));
      dup_t sdup;                                   /* una volta per iterazione */
      dup_build(&sdup, p->syndrome);
      for (int i = 0; i < N0; i++)
         lift_mul_dense_to_sparse_ct(bs + i * NUM_SLICES_GF2X_ELEMENT, &sdup, p->H[i], V);

      POS flip       = argmax_bitsliced_ct(bs);
      uint64_t block = flip / P;   /* P costante: il compilatore usa una moltiplicazione */
      uint64_t x     = flip % P;

      /* active = 0xFF..FF finche' la sindrome non e' nulla */
      DIGIT synd_or = 0;
      for (int d = 0; d < NW; d++) synd_or |= p->syndrome[d];
      uint64_t active = ct_nz64_mask(synd_or);

      gf2x_toggle_coeff_ct(out, block, x, active);
      syndrome_update_ct(p->syndrome, (const DIGIT (*)[NWS])p->HTr, block, x, active);
      DEBUG_PRINT("i: %d \t flip: %u \t active: %d \n", iter, flip, (int)(active & 1));
   }
   return 1;
}

