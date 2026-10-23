#pragma once

#include <immintrin.h>
#include "types.h"

#define IS_REPRESENTABLE_IN_D_BITS(D, N) (((unsigned long) N >= (1UL << (D - 1)) && (unsigned long) N < (1UL << D)) ? D : -1)
#define BITS_TO_REPRESENT(N) (N == 0 ? 1 : (31 + IS_REPRESENTABLE_IN_D_BITS( 1, N) + IS_REPRESENTABLE_IN_D_BITS( 2, N) + IS_REPRESENTABLE_IN_D_BITS( 3, N) + IS_REPRESENTABLE_IN_D_BITS( 4, N) + IS_REPRESENTABLE_IN_D_BITS( 5, N) + IS_REPRESENTABLE_IN_D_BITS( 6, N) + IS_REPRESENTABLE_IN_D_BITS( 7, N) + IS_REPRESENTABLE_IN_D_BITS( 8, N) + IS_REPRESENTABLE_IN_D_BITS( 9, N) + IS_REPRESENTABLE_IN_D_BITS(10, N) + IS_REPRESENTABLE_IN_D_BITS(11, N) + IS_REPRESENTABLE_IN_D_BITS(12, N) + IS_REPRESENTABLE_IN_D_BITS(13, N) + IS_REPRESENTABLE_IN_D_BITS(14, N) + IS_REPRESENTABLE_IN_D_BITS(15, N) + IS_REPRESENTABLE_IN_D_BITS(16, N) + IS_REPRESENTABLE_IN_D_BITS(17, N) + IS_REPRESENTABLE_IN_D_BITS(18, N) + IS_REPRESENTABLE_IN_D_BITS(19, N) + IS_REPRESENTABLE_IN_D_BITS(20, N) + IS_REPRESENTABLE_IN_D_BITS(21, N) + IS_REPRESENTABLE_IN_D_BITS(22, N) + IS_REPRESENTABLE_IN_D_BITS(23, N) + IS_REPRESENTABLE_IN_D_BITS(24, N) + IS_REPRESENTABLE_IN_D_BITS(25, N) + IS_REPRESENTABLE_IN_D_BITS(26, N) + IS_REPRESENTABLE_IN_D_BITS(27, N) + IS_REPRESENTABLE_IN_D_BITS(28, N) + IS_REPRESENTABLE_IN_D_BITS(29, N) + IS_REPRESENTABLE_IN_D_BITS(30, N) + IS_REPRESENTABLE_IN_D_BITS(31, N) + IS_REPRESENTABLE_IN_D_BITS(32, N) ) )

#define BITSLICED_OPERAND_WIDTH  (BITS_TO_REPRESENT(V)+1)
#define SLICE_TYPE               __m256i
#define NUM_BITS_IN_BITSLICED_OP (256)
#define DIGITS_IN_SLICE          (NUM_BITS_IN_BITSLICED_OP/DIGIT_SIZE_b)
#define NUM_SLICES_GF2X_ELEMENT  ( (NUM_DIGITS_GF2X_ELEMENT+DIGITS_IN_SLICE-1)/DIGITS_IN_SLICE )
#define NUM_DIGITS_SLICED        (NUM_SLICES_GF2X_ELEMENT*DIGITS_IN_SLICE)

/* i contatori arrivano fino a V: devono stare nei bit dell'operando */
_Static_assert((1 << BITSLICED_OPERAND_WIDTH) > V, "BITSLICED_OPERAND_WIDTH troppo piccolo");

/* bitsliced operand: slice[0] is the LSB */
typedef struct {
   SLICE_TYPE slice[BITSLICED_OPERAND_WIDTH];
} bs_operand_t;

static inline void bitslice_half_adder(SLICE_TYPE  addend_a, SLICE_TYPE  addend_b, SLICE_TYPE *result, SLICE_TYPE *carry_out)
{
   _mm256_storeu_si256 (result, _mm256_xor_si256(addend_a, addend_b));
   _mm256_storeu_si256 (carry_out, _mm256_and_si256(addend_a, addend_b));
   return;
}

/* a + b, dove b vale 0 o 1 per ciascun contatore */
static inline bs_operand_t bitslice_inc(bs_operand_t a, SLICE_TYPE b)
{
   bs_operand_t result;
   SLICE_TYPE carry;
   bitslice_half_adder(a.slice[0],b,&(result.slice[0]),&carry);
   for(int i = 1; i<BITSLICED_OPERAND_WIDTH; i++) {
      bitslice_half_adder(a.slice[i],
                          carry,
                          &(result.slice[i]),
                          &carry);
   }
   return result;
}

/* *a += b solo sui primi nplanes piani (nplanes pubblico). Corretto quando i
 * contatori, dopo la somma, stanno in nplanes bit: i piani sopra restano a
 * zero e il riporto finale e' nullo. In-place, senza copie dell'operando. */
static inline void bitslice_inc_planes(bs_operand_t *a, SLICE_TYPE b, int nplanes)
{
   SLICE_TYPE carry = b;
   for(int i = 0; i < nplanes; i++) {
      SLICE_TYPE x = a->slice[i];
      a->slice[i] = _mm256_xor_si256(x, carry);
      carry       = _mm256_and_si256(x, carry);
   }
}

/* a - b, dove b vale 0 o 1 per ciascun contatore (per aggiornare i contatori) */
static inline bs_operand_t bitslice_dec(bs_operand_t a, SLICE_TYPE b)
{
   bs_operand_t result;
   SLICE_TYPE carry;
   const SLICE_TYPE ones = _mm256_set1_epi64x(-1);
   bitslice_half_adder(_mm256_xor_si256(a.slice[0], ones),b,&(result.slice[0]),&carry);
   result.slice[0] = _mm256_xor_si256(result.slice[0], ones);
   for(int i = 1; i<BITSLICED_OPERAND_WIDTH; i++) {
      bitslice_half_adder(_mm256_xor_si256(a.slice[i], ones),
                          carry,
                          &(result.slice[i]),
                          &carry);
      result.slice[i] = _mm256_xor_si256(result.slice[i], ones);
   }
   return result;
}

/* tutti i contatori = constant (constant e' pubblico) */
static inline bs_operand_t slice_constant(uint16_t constant)
{
   bs_operand_t result;
   const __m256i one  = _mm256_set1_epi64x(-1);
   const __m256i zero = _mm256_setzero_si256();
   for(int bit = 0; bit < BITSLICED_OPERAND_WIDTH; bit++) {
      result.slice[bit] = (constant & 1) ? one : zero;
      constant >>= 1;
   }
   return result;
}

