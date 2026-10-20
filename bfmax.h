#pragma once
#include "helpers.h"
#include "defines.h"
#include "test_utils.h"
#include <stdint.h>

/* ------------------------------------------------------------------------------*/
/*                                   ARGMAX                                      */ 
/* ------------------------------------------------------------------------------*/
static INLINE POS argmax_u8(uint8_t arr[PAD8(N0 * P)]) {

   /* ------------- Scan to find maximum values ------------------ */
   __m256i max_vec = _mm256_setzero_si256();
   for (int i = 0; i < N_REGS_UPC; i++) {
      __m256i v = _mm256_loadu_si256((__m256i *)&arr[i * I8_IN_YMM]);
      max_vec = _mm256_max_epu8(max_vec, v);
   }

   /* ----------- Find the maximum inside the vector ------------- */
   __m128i lo = _mm256_castsi256_si128(max_vec);
   __m128i hi = _mm256_extracti128_si256(max_vec, 1);
   __m128i m = _mm_max_epu8(lo, hi);
   m = _mm_max_epu8(m, _mm_srli_si128(m, 8));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 4));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 2));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 1));
   uint8_t max_val = (uint8_t)_mm_extract_epi8(m, 0);
   
   /* ------------- Scan to find the max position ---------------- */
   __m256i vmax = _mm256_set1_epi8((uint8_t)max_val);
   for (int i = 0; i < N_REGS_UPC; i++) {
      __m256i v = _mm256_loadu_si256((__m256i *)&arr[i * I8_IN_YMM]);
      __m256i cmp = _mm256_cmpeq_epi8(v, vmax);
      int mask = _mm256_movemask_epi8(cmp);
      if (mask) {
         // this is an early exit, possible timing leakage
         return (POS)(i * I8_IN_YMM + __builtin_ctz(mask));
      }
   }
   return (POS)-1;
}

/* ------------------------------------------------------------------------------*/
/*                              UPDATE S AND UPC                                 */ 
/* ------------------------------------------------------------------------------*/
static INLINE int update_syndrome_and_upcs(
   OUT uint8_t upc[PAD8(N0 * P)], 
   IN  POS Htr_sparse[N0][PAD32(V)], 
   IN  __m256i v_H_sparse[N0][N_REGS_H],
   IN  POS flip, 
   OUT uint8_t syndrome_bits[P],
   IN  int hw)
{
   int flip_block = flip / P;
   int flip_bit = flip - flip_block * P;
   __m256i vp = _mm256_set1_epi32((uint32_t)P);
   __m256i vpos = _mm256_set1_epi32((uint32_t)flip_bit);
   /* update syndrome and save upc positions to update */
   __m256i up_pos[V][N0][N_REGS_H];
   int up_sign[V];
   for (int col_reg = 0; col_reg < N_REGS_H; col_reg++) {
      /* get the column of H corresponding to the flipped bit */
      uint32_t tmp[I32_IN_YMM];
      __m256i htr = _mm256_loadu_si256((__m256i *)&Htr_sparse[flip_block][col_reg * I32_IN_YMM]);
      __m256i sum = _mm256_add_epi32(htr, vpos);
      __m256i sub = _mm256_sub_epi32(sum, vp);
      __m256i res = _mm256_min_epu32(sum, sub);
      _mm256_storeu_si256((__m256i *)tmp, res);
      /* scan each idx in the column */
      for (int i = 0; (i < I32_IN_YMM) && (col_reg * I32_IN_YMM + i < V); i++) {
         /* update the syndrome */
         POS row = tmp[i];
         syndrome_bits[row] ^= 1;
         //int delta = (syndrome_bits[row] == 0) ? -1 : 1;
         int delta = ((int)syndrome_bits[row] << 1) - 1;
         hw += delta;
         if (hw == 0) {
            return hw;
         }
         up_sign[col_reg * I32_IN_YMM + i] = delta;
         /* save upc positions to update (faster than updating upcs directly) */
         __m256i vrow = _mm256_set1_epi32((uint32_t)row);
         for (int block = 0; block < N0; block++) {
            for (int row_reg = 0; row_reg < N_REGS_H; row_reg++) {
               __m256i col = _mm256_add_epi32(v_H_sparse[block][row_reg], vrow);
               __m256i sub = _mm256_sub_epi32(col, vp);
               __m256i res = _mm256_min_epu32(col, sub);
               up_pos[col_reg * I32_IN_YMM + i][block][row_reg] = res;
            }
         }
      }
   }
   /* update upcs */
   uint32_t *RESTRICT up_pos_u32 = (uint32_t *)up_pos;
   for (int i = 0; i < V; i++) {
      int delta = up_sign[i];
      for (int block = 0; block < N0; block++) {
         for (int row_reg = 0; row_reg < N_REGS_H; row_reg++) {
            for (int lane = 0; (lane < 8) && (row_reg * 8 + lane < V); lane++) {
               int up_idx = (((i * N0 + block) * N_REGS_H + row_reg) * 8) + lane;
               uint32_t col = up_pos_u32[up_idx];
               upc[block * P + col] += delta;
            }
         }
      }
   }
   return hw;
}

/* ------------------------------------------------------------------------------*/
/*                               COMPUTE COUNTERS                                */ 
/* ------------------------------------------------------------------------------*/
static INLINE void compute_upcs(
   OUT uint8_t upc[PAD8(N0 * P)],
   IN  POS Htr_sparse[N0][PAD32(V)],
   IN  uint8_t syndrome_bits[P])
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
////////////////////////////////////////////////////////////////////////////////
int bfmax_decoder(
   OUT DIGIT error[N0*NUM_DIGITS_GF2X_ELEMENT], 
   IN  POS Htr_sparse[N0][PAD32(V)], 
   IN  POS H_sparse[N0][PAD32(V)], 
   IN  uint8_t syndrome_bits[P],
   IN  int hw_start
)
{
   __m256i v_H_sparse[N0][N_REGS_H];
   for (int block = 0; block < N0; block++) {
   for (int r = 0; r < N_REGS_H; r++) {
      int remaining = V - r * I32_IN_YMM;
      __mmask8 kmask = (remaining >= I32_IN_YMM) ? 0xFF
                     : (remaining <= 0 ? 0x00 : (__mmask8)((1u << remaining) - 1));
      v_H_sparse[block][r] = _mm256_maskz_loadu_epi32(kmask, (const void *)&H_sparse[block][r * I32_IN_YMM]);
      }
   }

   /* compute unsatisfied parity checks */
   ALIGNED uint8_t upc[PAD8(N0 * P)] = {0};
   compute_upcs(upc, Htr_sparse, syndrome_bits);
   
   /* ------------- Decoding Loop ---------------- */
   int iter = 0;
   int hw = hw_start;
   do {
      POS col = argmax_u8(upc);
      int col_block = col / P;
      int col_bit = col % P;
      gf2x_toggle_coeff(error + col_block * NUM_DIGITS_GF2X_ELEMENT, col_bit);
      hw = update_syndrome_and_upcs(upc, Htr_sparse, v_H_sparse, col, syndrome_bits, hw);
      DEBUG_PRINT("i: %d \t hw(s): %d \n", iter, hw);
      iter++;
   } while ((iter < 1.5 * NUM_ERRORS_T) && (hw != 0));
   return 1;
}
////////////////////////////////////////////////////////////////////////////////

