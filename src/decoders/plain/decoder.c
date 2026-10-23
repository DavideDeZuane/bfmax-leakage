/*
 * bfmax.c - decoder BF-Max base (era bfmax.h).
 *
 * Funzioni proprie di questo decoder: argmax ricalcolato a ogni iterazione,
 * aggiornamento degli UPC riga per riga, uscita anticipata quando la
 * sindrome si annulla. La preparazione (sindrome espansa, v_H_sparse, peso
 * iniziale) e' in prepare.c.
 */
#include <immintrin.h>
#include "decoder.h"
#include "ct.h"
#include "gf2x.h"
#include "upc.h"

/**
 * @brief Find the maximum value inside an array of uint8_t of length UPC_LEN.
 *        This is constant time because we have to scan all the array to find the maximum.
 * @return The maximum value.
 */
static inline uint8_t hmax_u8(const uint8_t *arr) {

   __m256i a0 = _mm256_setzero_si256();
   __m256i a1 = a0;
   /* ------------------------------------- Scan with two register ---------------------------------------- */
   /* a0 contains the maximum between odds registers   */
   /* a1 contains the maximum between events registers */
   /* With two acculmulator we broke the sequential dependency */
   int i = 0;
   for (; i + 1 < N_REGS_UPC; i += 2) {
      a0 = _mm256_max_epu8(a0, _mm256_loadu_si256((const __m256i *)&arr[i * 32]));
      a1 = _mm256_max_epu8(a1, _mm256_loadu_si256((const __m256i *)&arr[(i + 1) * 32]));
   }
   /* This is necessary because otherwise the register 771 (valid only for r = 12323) will be not included */
   if (i < N_REGS_UPC) a0 = _mm256_max_epu8(a0, _mm256_loadu_si256((const __m256i *)&arr[i * 32]));

   /* -------------------------------------- Horizontal reduction ----------------------------------------- */
   a0 = _mm256_max_epu8(a0, a1);
   __m128i m = _mm_max_epu8(_mm256_castsi256_si128(a0), _mm256_extracti128_si256(a0, 1));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 8));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 4));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 2));
   m = _mm_max_epu8(m, _mm_srli_si128(m, 1));
   return (uint8_t)_mm_extract_epi8(m, 0);
}

static inline uint32_t mm(__m256i vmax, const uint8_t *p) {
   return (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)p), vmax));
}

static INLINE POS argmax_u8_ct(const uint8_t *arr) {

   /* ------------------------------------- V copies of the max value ---------------------------------------- */
   __m256i vmax = _mm256_set1_epi8((char)hmax_u8(arr));

   /* ------------------------------------- Scan to find the position ---------------------------------------- */
   uint64_t found = 0, bm = 0, br = 0;
   int i = 0;
   for (; i + 1 < N_REGS_UPC; i += 2) {
      uint64_t lo = mm(vmax, &arr[i * 32]);
      uint64_t hi = mm(vmax, &arr[(i + 1) * 32]);
      if (i + 1 == N_REGS_UPC - 1) hi &= UPC_TAIL_MASK;
      /* -------------- Combine the two masks ---------------- */
      /* cmpeq return a 32 bit mask but we can use a bigger register to combine them */
      uint64_t mask = lo | (hi << 32);

      uint64_t nz = ct_nz64_mask(mask);
      uint64_t sel = nz & ~found;
      bm |= mask & sel; br |= (uint64_t)i & sel; found |= nz;
   }
   if (i < N_REGS_UPC) {               /* registro dispari finale (i è pubblico) */
      uint64_t mask = mm(vmax, &arr[i * 32]) & UPC_TAIL_MASK;
      uint64_t nz = ct_nz64_mask(mask);
      uint64_t sel = nz & ~found;
      bm |= mask & sel; br |= (uint64_t)i & sel; found |= nz;
   }
   return (POS)(br * 32 + ct_ctz64(bm));
}

/* ------------------------------------------------------------------------------*/
/*                              UPDATE S AND UPC                                 */
/* ------------------------------------------------------------------------------*/
static INLINE int update_syndrome_and_upcs(
   OUT uint8_t upc[PAD8(N0 * P)],
   IN  const POS Htr_sparse[N0][PAD32(V)],
   IN  const __m256i v_H_sparse[N0][N_REGS_H],
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
      __m256i htr = _mm256_loadu_si256((const __m256i *)&Htr_sparse[flip_block][col_reg * I32_IN_YMM]);
      __m256i sum = _mm256_add_epi32(htr, vpos);
      __m256i sub = _mm256_sub_epi32(sum, vp);
      __m256i res = _mm256_min_epu32(sum, sub);
      _mm256_storeu_si256((__m256i *)tmp, res);
      /* scan each idx in the column */
      for (int i = 0; (i < I32_IN_YMM) && (col_reg * I32_IN_YMM + i < V); i++) {
         /* update the syndrome */
         POS row = tmp[i];
         syndrome_bits[row] ^= 1;
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
               __m256i sub2 = _mm256_sub_epi32(col, vp);
               __m256i res2 = _mm256_min_epu32(col, sub2);
               up_pos[col_reg * I32_IN_YMM + i][block][row_reg] = res2;
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

////////////////////////////////////////////////////////////////////////////////
int bfmax_decoder(
   OUT DIGIT error[N0 * NUM_DIGITS_GF2X_ELEMENT],
   IN  dec_params_t *p)
{
   memset(error, 0, N0 * NUM_DIGITS_GF2X_ELEMENT * sizeof(DIGIT));

   /* compute unsatisfied parity checks */
   ALIGNED uint8_t upc[PAD8(N0 * P)] = {0};
   compute_upcs(upc, p->Htr, p->syndrome_bits);

   /* ------------- Decoding Loop ---------------- */
   int iter = 0;
   int hw = p->hw_start;
   do {
      POS col = argmax_u8_ct(upc);
      int col_block = col / P;
      int col_bit = col % P;
      gf2x_toggle_coeff(error + col_block * NUM_DIGITS_GF2X_ELEMENT, col_bit);
      hw = update_syndrome_and_upcs(upc, p->Htr, p->v_H_sparse, col, p->syndrome_bits, hw);
      DEBUG_PRINT("i: %d \t hw(s): %d \n", iter, hw);
      iter++;
   } while ((iter < MAX_ITER) && (hw != 0));
   return 1;
}

