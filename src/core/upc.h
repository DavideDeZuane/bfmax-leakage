/*
 * upc.h - calcolo iniziale dei contatori e massimo, condivisi da tutte le
 * versioni (compute_upcs era identica in bfmax.h e steroids.h).
 */
#pragma once

#include <immintrin.h>
#include "types.h"

/* ------------------------------------------------------------------------------*/
/*                               COMPUTE COUNTERS                                */
/* ------------------------------------------------------------------------------*/
/* NOTA leakage: il numero di iterazioni totale e' costante (P per riga), ma
 * il punto di split "wrap" dipende da Htr_sparse -> ctgrind segnala i bound
 * dei due loop (branch) e gli indirizzi di syndrome_bits. */
static INLINE void compute_upcs(
   OUT uint8_t upc[PAD8(N0 * P)],
   IN  const POS Htr_sparse[N0][PAD32(V)],
   IN  const uint8_t syndrome_bits[P])
{
   /* for each block */
   for (int block = 0; block < N0; block++) {
      uint8_t *upc_block = &upc[block * P];
      /* scan each idx in the first column */
      for (int i = 0; i < V; i++) {
         int idx = Htr_sparse[block][i];
         int wrap = P - idx;
         /* increment idx to shift the column (first half of upcs) */
         for (int j = 0; j < wrap; j++) {
            upc_block[j] += syndrome_bits[idx + j];
         }
         /* increment idx to shift the column (second half of upcs) */
         for (int k = wrap; k < wrap + idx; k++) {
            upc_block[k] += syndrome_bits[k - wrap];
         }
      }
   }
}

/* -------------------- Horizontal Reduction to find the maximum ------------------- */
static INLINE uint8_t bfx_hmax(__m256i v)
{
   __m128i m = _mm_max_epu8(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 8));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 4));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 2));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 1));
   return (uint8_t)_mm_cvtsi128_si32(m);
}

/* -------------------- Scan the full array to find the maximums ------------------- */
static INLINE uint8_t bfmax_max(IN const uint8_t upc[PAD8(N0 * P)])
{
   __m256i vm = _mm256_setzero_si256();
   int i = 0;
   for (; i + 32 <= N0 * P; i += 32) {
      vm = _mm256_max_epu8(vm, _mm256_loadu_si256((const __m256i *)(upc + i)));
   }
   uint8_t m = bfx_hmax(vm);
   for (; i < N0 * P; i++) {
      if (upc[i] > m) m = upc[i];
   }
   return m;
}

