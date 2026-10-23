/*
 * decoder.c - decoder BF-Max "steroids" in un unico file (era steroids.h).
 *
 * Le versioni si scelgono con i flag di compilazione di steroids.h, passati
 * da variants.mk (si possono combinare):
 *
 *   -DVT_FIND        bfmax_find variable-time: si ferma al primo blocco da 64
 *                    contatori che contiene il massimo (default: CT)
 *   -DFIXED_NEG      update con segno fisso: sempre -A denso e scatter +2 sulle
 *                    righe diventate insoddisfatte (default: segno adattivo)
 *   -DFIXED_SCATTER  update con scatter di lunghezza fissa N0*V*V
 *                    (ha precedenza su FIXED_NEG)
 *
 * La preparazione (sindrome espansa, tabelle per-chiave) e' in prepare.c.
 */
 #include <string.h>
 #include <immintrin.h>
 #include "decoder.h"
 #include "ct.h"
 #include "gf2x.h"
 #include "upc.h"
 
 ////////////////////////////////////////////////////////////////////////////////
 /* sb[j] -= A[(j-pos) mod r]  (or += when !neg); returns running max */
 static INLINE __m256i bfx_dense(uint8_t *sb, const uint8_t *A, uint32_t pos, int neg,
                                 __m256i vmax, uint8_t *smax)
 {
    const uint8_t *s1 = A + P - pos;   /* j <  pos; overrun into the wrap pad is correct */
    const uint8_t *s2 = A - pos;            /* j >= pos */
    uint32_t split = (pos + 31) & ~31u;
    if (split > BFX_PV) split = BFX_PV;
    uint32_t j = 0;
    if (neg) {
       for (; j < split; j += 32) {
          __m256i x = _mm256_sub_epi8(_mm256_loadu_si256((__m256i *)(sb + j)), _mm256_loadu_si256((const __m256i *)(s1 + j)));
          _mm256_storeu_si256((__m256i *)(sb + j), x); vmax = _mm256_max_epu8(vmax, x);
       }
       for (; j < BFX_PV; j += 32) {
          __m256i x = _mm256_sub_epi8(_mm256_loadu_si256((__m256i *)(sb + j)), _mm256_loadu_si256((const __m256i *)(s2 + j)));
          _mm256_storeu_si256((__m256i *)(sb + j), x); vmax = _mm256_max_epu8(vmax, x);
       }
    } else {
       for (; j < split; j += 32) {
          __m256i x = _mm256_add_epi8(_mm256_loadu_si256((__m256i *)(sb + j)), _mm256_loadu_si256((const __m256i *)(s1 + j)));
          _mm256_storeu_si256((__m256i *)(sb + j), x); vmax = _mm256_max_epu8(vmax, x);
       }
       for (; j < BFX_PV; j += 32) {
          __m256i x = _mm256_add_epi8(_mm256_loadu_si256((__m256i *)(sb + j)), _mm256_loadu_si256((const __m256i *)(s2 + j)));
          _mm256_storeu_si256((__m256i *)(sb + j), x); vmax = _mm256_max_epu8(vmax, x);
       }
    }
    for (; j < P; j++) {   /* P % 32 tail, stays inside the block */
       uint32_t t = j + P - pos;
       t -= (t >= P) ? P : 0;
       sb[j] = neg ? (uint8_t)(sb[j] - A[t]) : (uint8_t)(sb[j] + A[t]);
       if (sb[j] > *smax) *smax = sb[j];
    }
    return vmax;
 }
 
 /* --------------------- First index whose counter equals mx --------------------- */
 #ifdef VT_FIND
 /* variable-time version: stops at the first 64-counter block holding the maximum,
    so the time reveals flip / 64 */
 static INLINE POS bfmax_find(IN const uint8_t upc[PAD8(N0 * P)], IN uint8_t mx)
 {
    const __m256i v = _mm256_set1_epi8((char)mx);
    int i = 0;
    for (; i + 64 <= N0 * P; i += 64) {
       uint64_t lo = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(upc + i)), v));
       uint64_t hi = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(upc + i + 32)), v));
       uint64_t m = lo | (hi << 32);
       if (m) return (POS)(i + __builtin_ctzll(m));
    }
    for (; i < N0 * P; i++) {
       if (upc[i] == mx) return (POS)i;
    }
    return (POS)-1;
 }
 #else
 /* constant-time version: always scans the whole array, two YMM (64 counters)
    at a time; the first block with a match is selected with masks */
 #define FIND_FULL ((N0 * P) / 64 * 64)    /* counters covered by whole 64-blocks */
 #define FIND_TAIL ((N0 * P) - FIND_FULL)  /* remaining counters (< 64)           */
 _Static_assert(PAD8(N0 * P) >= FIND_FULL + ((FIND_TAIL + 31) / 32) * 32,
                "upc is too short for the tail loads of bfmax_find");
 
 static INLINE uint64_t find_mask64(const uint8_t *p, __m256i v)
 {
    uint64_t lo = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)p), v));
    uint64_t hi = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(p + 32)), v));
    return lo | (hi << 32);
 }
 
 static INLINE POS bfmax_find(IN const uint8_t upc[PAD8(N0 * P)], IN uint8_t mx)
 {
    const __m256i v = _mm256_set1_epi8((char)mx);
    uint64_t found = 0, best_mask = 0, best_i = 0;
 
    for (uint64_t i = 0; i < FIND_FULL; i += 64) {
       uint64_t m   = find_mask64(upc + i, v);
       uint64_t nz  = ct_nz64_mask(m);        /* not on the dependency chain */
       uint64_t sel = nz & ~found;            /* all ones only at the first match */
       best_mask |= m & sel;
       best_i    |= i & sel;
       found     |= nz;
    }
 #if FIND_TAIL > 0
    {  /* last partial block: the padding counters are masked out */
       uint64_t m = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(
                       _mm256_loadu_si256((const __m256i *)(upc + FIND_FULL)), v));
    #if FIND_TAIL > 32
       m |= (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(
                       _mm256_loadu_si256((const __m256i *)(upc + FIND_FULL + 32)), v)) << 32;
    #endif
       m &= (FIND_TAIL == 64) ? ~(uint64_t)0 : (((uint64_t)1 << FIND_TAIL) - 1);
       uint64_t nz  = ct_nz64_mask(m);
       uint64_t sel = nz & ~found;
       best_mask |= m & sel;
       best_i    |= (uint64_t)FIND_FULL & sel;
       found     |= nz;
    }
 #endif
    /* best_mask != 0 because mx is the maximum of the counters */
    return (POS)(best_i + ct_ctz64(best_mask));
 }
 #endif
 
 ////////////////////////////////////////////////////////////////////////////////
 /* Apply the flip (in [0, N0*P)): updates syndrome_bits and upc, returns
  * the new syndrome weight and writes the new max counter to *new_max. */
 static INLINE int bfmax_update(
    IN    dec_params_t *p,
    OUT   uint8_t upc[PAD8(N0 * P)],
    IN    POS flip,
    IN    int hw,
    OUT   uint8_t *new_max)
 {
    bfmax_ctx_t *ctx = &p->ctx;
    const POS (*Htr_sparse)[PAD32(V)] = p->Htr;
    uint8_t *syndrome_bits = p->syndrome_bits;
    const int b = flip >= P;
    const uint32_t pos = flip - b * P;
 
 #if defined(FIXED_SCATTER)
    /* ----------------------------------------------------------------------
     * neg fixed + scatter of fixed length:
     *   dense step: -A for all V rows (as if every row became satisfied)
     *   scatter:    +2 on every row of the column, multiplied by its new
     *               syndrome bit (0 for rows that became satisfied)
     * The number of updates is always N0*V*V: it no longer depends on how
     * many rows became unsatisfied. Addresses are still secret (cache).
     * ---------------------------------------------------------------------- */
    const int neg = 1;
    uint32_t rows[V];
    uint8_t  drow[V];
    int unsat = 0;
    for (int k = 0; k < V; k++) {
       uint32_t r = Htr_sparse[b][k] + pos;
       r -= (r >= P) ? P : 0;
       uint8_t s = (syndrome_bits[r] ^= 1);
       rows[k] = r;
       drow[k] = (uint8_t)(s << 1);   /* 2 if the row became unsatisfied, 0 otherwise */
       unsat += s;
    }
    hw += unsat - (V - unsat);
 
    const __m256i vR = _mm256_set1_epi32(P);
    uint16_t *idx = ctx->idx;
    int n = 0;
    for (int b2 = 0; b2 < N0; b2++) {
       const __m256i voff = _mm256_set1_epi32(b2 * P);
       for (int k = 0; k < V; k++) {
          const __m256i vr = _mm256_set1_epi32(rows[k]);
          for (int m = 0; m < BFX_HPAD; m += 8) {
             __m256i c = _mm256_add_epi32(_mm256_load_si256((const __m256i *)&ctx->H[b2][m]), vr);
             c = _mm256_min_epu32(c, _mm256_sub_epi32(c, vR));   /* mod r */
             c = _mm256_add_epi32(c, voff);
             __m128i pk = _mm_packus_epi32(_mm256_castsi256_si128(c), _mm256_extracti128_si256(c, 1));
             _mm_storeu_si128((__m128i *)&idx[n + m], pk);
          }
          n += V;
       }
    }
    for (int b2 = 0; b2 < N0; b2++) {
       for (int k = 0; k < V; k++) {
          const uint8_t d = drow[k];
          const uint16_t *ix = idx + (b2 * V + k) * V;
          for (int m = 0; m < V; m++) upc[ix[m]] += d;
       }
    }
 #else
    /* 1. toggle the D syndrome bits, split rows by their new value */
    uint32_t rows[2][V];
    int cnt[2] = {0, 0};
    for (int k = 0; k < V; k++) {
       uint32_t r = Htr_sparse[b][k] + pos;
       r -= (r >= P) ? P : 0;
       uint8_t s = (syndrome_bits[r] ^= 1);
       rows[s][cnt[s]++] = r;
    }
    hw += cnt[1] - cnt[0];
 
 #if defined(FIXED_NEG)
    /* always dense -A and scatter +2 on the rows that became unsatisfied:
       correct for any flip, no branch on the syndrome bits */
    const int neg = 1;
 #else
    /* 2. minority sign -> scatter, majority -> dense table */
    const int neg = cnt[1] <= cnt[0];   /* usual case: dense -A, scatter +2 on newly unsat rows */
 #endif
    const uint32_t *sr = rows[neg];
    const int ns = cnt[neg];
    const uint8_t d2 = neg ? 2 : (uint8_t)-2;
 
    /* 3a. all scatter targets with AVX2 into a uint16 buffer ... */
    const __m256i vR = _mm256_set1_epi32(P);
    uint16_t *idx = ctx->idx;
    int n = 0;
    for (int b2 = 0; b2 < N0; b2++) {
       const __m256i voff = _mm256_set1_epi32(b2 * P);
       for (int i = 0; i < ns; i++) {
          const __m256i vr = _mm256_set1_epi32(sr[i]);
          for (int m = 0; m < BFX_HPAD; m += 8) {
             __m256i c = _mm256_add_epi32(_mm256_load_si256((const __m256i *)&ctx->H[b2][m]), vr);
             c = _mm256_min_epu32(c, _mm256_sub_epi32(c, vR));   /* mod r */
             c = _mm256_add_epi32(c, voff);
             __m128i pk = _mm_packus_epi32(_mm256_castsi256_si128(c), _mm256_extracti128_si256(c, 1));
             _mm_storeu_si128((__m128i *)&idx[n + m], pk);   /* lanes >= D are overwritten / ignored */
          }
          n += V;
       }
    }
    /* 3b. ... then a tight byte read-modify-write loop */
    int i = 0;
    for (; i + 4 <= n; i += 4) {
       upc[idx[i]] += d2;     upc[idx[i + 1]] += d2;
       upc[idx[i + 2]] += d2; upc[idx[i + 3]] += d2;
    }
    for (; i < n; i++) upc[idx[i]] += d2;
 #endif
 
    /* 4. dense rotated table, fused with the max for the next iteration */
    __m256i vmax = _mm256_setzero_si256();
    uint8_t smax = 0;
    for (int b2 = 0; b2 < N0; b2++) {
       vmax = bfx_dense(upc + b2 * P, ctx->A[b][b2], pos, neg, vmax, &smax);
    }
    uint8_t mv = bfx_hmax(vmax);
    *new_max = mv > smax ? mv : smax;
    return hw;
 }
 
 ////////////////////////////////////////////////////////////////////////////////
 int bfmax_decoder_steroids(
    OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  dec_params_t *p)
 {
    uint8_t *syndrome_bits = p->syndrome_bits;
    memset(out, 0, N0 * NUM_DIGITS_GF2X_ELEMENT * sizeof(DIGIT));
 
    int hw = 0;
    for (int i = 0; i < P; i++) hw += syndrome_bits[i];
 
    /* compute unsatisfied parity checks (padding bytes stay 0 and are never used) */
    ALIGNED uint8_t upc[PAD8(N0 * P)] = {0};
    compute_upcs(upc, p->Htr, syndrome_bits);
    uint8_t mx = bfmax_max(upc);
 
    /* decoding iterations */
    int iter = 0;
    while (iter < MAX_ITER && hw != 0) {
       POS flip = bfmax_find(upc, mx);
       gf2x_toggle_coeff(out + (flip / P) * NUM_DIGITS_GF2X_ELEMENT, flip % P);
       hw = bfmax_update(p, upc, flip, hw, &mx);
       iter++;
       DEBUG_PRINT("i: %d \t flip: %u \t hw(s): %d \n", iter, flip, hw);
    }
    return 1;
 }
 
 