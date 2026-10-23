/*
 * gf2x.h - aritmetica su GF(2)[x] nella rappresentazione DENSA (da test_utils.h).
 * Usata dal decoder (gf2x_toggle_coeff sull'errore) e dagli harness
 * (calcolo della sindrome).
 */
#pragma once

#include <string.h>
#include "types.h"

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_copy(DIGIT dest[], CONST DIGIT in[])
{
   for (int i = NUM_DIGITS_GF2X_ELEMENT-1; i >= 0; i--) dest[i] = in[i];
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_set_coeff(DIGIT poly[], CONST unsigned int exponent, DIGIT value)
{
   int digitIdx = exponent / DIGIT_SIZE_b;
   unsigned int inDigitIdx = exponent % DIGIT_SIZE_b;
   /* clear given coefficient */
   DIGIT mask = ~( ((DIGIT) 1) << inDigitIdx);
   poly[digitIdx] = poly[digitIdx] & mask;
   poly[digitIdx] = poly[digitIdx] | (( value & ((DIGIT) 1)) << inDigitIdx);
}

/* ------------------------------------------------------------------------------*/
static INLINE DIGIT gf2x_get_coeff(const DIGIT poly[], CONST unsigned int exponent)
{
   unsigned int digitIdx = exponent / DIGIT_SIZE_b;
   unsigned int inDigitIdx = exponent % DIGIT_SIZE_b;
   return (poly[digitIdx] >> inDigitIdx) & ((DIGIT) 1);
}

/* ------------------------------------------------------------------------------*/
/* ATTENZIONE: indice di parola segreto quando exponent e' segreto (cache) */
static INLINE void gf2x_toggle_coeff(DIGIT poly[], CONST unsigned int exponent)
{
   int digitIdx = exponent / DIGIT_SIZE_b;
   unsigned int inDigitIdx = exponent % DIGIT_SIZE_b;
   DIGIT mask = ( ((DIGIT) 1) << inDigitIdx);
   poly[digitIdx] = poly[digitIdx] ^ mask;
}

/* ------------------------------------------------------------------------------*/
static INLINE int population_count(DIGIT upc[])
{
   int ret = 0;
   for(int i = NUM_DIGITS_GF2X_ELEMENT - 1; i >= 0; i--) {
      ret += __builtin_popcountll((unsigned long long int) (upc[i]));
   }
   return ret;
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_mod_densify_VT(DIGIT dense[NUM_DIGITS_GF2X_ELEMENT],
                                       CONST POS exponent[],
                                       int num_exponents)
{
   for(int j=0; j<num_exponents; j++) {
      gf2x_set_coeff(dense, exponent[j], (DIGIT) 1);
   }
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_add(CONST int nr, DIGIT Res[],
                            CONST int na, CONST DIGIT A[],
                            CONST int nb, CONST DIGIT B[])
{
   (void)na; (void)nb;
   for (int i = 0; i < nr; i++)
      Res[i] = A[i] ^ B[i];
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_mod_add(DIGIT Res[], CONST DIGIT A[], CONST DIGIT B[])
{
   gf2x_add(NUM_DIGITS_GF2X_ELEMENT, Res,
            NUM_DIGITS_GF2X_ELEMENT, A,
            NUM_DIGITS_GF2X_ELEMENT, B);
}

/* ------------------------------------------------------------------------------*/
static INLINE void right_bit_shift_n(CONST int length, DIGIT in[], CONST int amount)
{
   if ( amount == 0 ) return;
   DIGIT mask = ((DIGIT)0x01 << amount) - 1;
   for (int j = 0; j < length - 1 ; j++) {
      in[j] >>= amount;
      in[j] |=  (in[j+1] & mask) << (DIGIT_SIZE_b - amount);
   }
   in[length - 1] >>= amount;
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_mod(DIGIT out[], CONST int nin, CONST DIGIT in[])
{
   (void)nin;
   DIGIT aux[NUM_DIGITS_GF2X_ELEMENT+1];
   memcpy(aux, in+NUM_DIGITS_GF2X_ELEMENT-1, (NUM_DIGITS_GF2X_ELEMENT+1)*DIGIT_SIZE_B);
#if MSb_POSITION_IN_MSB_DIGIT_OF_MODULUS != 0
   right_bit_shift_n(NUM_DIGITS_GF2X_ELEMENT+1, aux,
                     MSb_POSITION_IN_MSB_DIGIT_OF_MODULUS);
#endif
   gf2x_add(NUM_DIGITS_GF2X_ELEMENT,out,
            NUM_DIGITS_GF2X_ELEMENT,aux,
            NUM_DIGITS_GF2X_ELEMENT,in);
#if MSb_POSITION_IN_MSB_DIGIT_OF_MODULUS != 0
   out[NUM_DIGITS_GF2X_ELEMENT-1] &=  ((DIGIT)1 << MSb_POSITION_IN_MSB_DIGIT_OF_MODULUS) - 1 ;
#endif
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_fmac(DIGIT Res[], CONST DIGIT operand[], CONST unsigned int shiftAmt)
{
   unsigned int digitShift = shiftAmt / DIGIT_SIZE_b;
   unsigned int inDigitShift= shiftAmt % DIGIT_SIZE_b;
   DIGIT tmp,prevLo=0;
   int i;
   SIGNED_DIGIT inDigitShiftMask = ((SIGNED_DIGIT) (inDigitShift>0) << (DIGIT_SIZE_b-1)) >> (DIGIT_SIZE_b-1);
   for(i = 0; i < NUM_DIGITS_GF2X_ELEMENT; i++) {
      tmp = operand[i];
      Res[i + digitShift] ^= prevLo | (tmp << inDigitShift);
      prevLo = (tmp >> (DIGIT_SIZE_b - inDigitShift)) & inDigitShiftMask;
   }
   Res[i + digitShift] ^= prevLo;
}

/* ------------------------------------------------------------------------------*/
static INLINE void gf2x_mod_mul_dense_to_sparse(DIGIT Res[],
                                  CONST DIGIT dense[],
                                  CONST POS sparse[],
                                  unsigned int nPos)
{
   DIGIT resDouble[2*NUM_DIGITS_GF2X_ELEMENT] = {0};
   for (unsigned int i = 0; i < nPos; i++) {
      if (sparse[i] != INVALID_POS_VALUE) {
         gf2x_fmac(resDouble, dense, sparse[i]);
      }
   }
   gf2x_mod(Res, 2*NUM_DIGITS_GF2X_ELEMENT, resDouble);
}

