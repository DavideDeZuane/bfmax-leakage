/*
 * types.h - tipi e macro di dimensionamento.
 *
 * Le due rappresentazioni in gioco (vedi src/core/repr.h):
 *   - DENSA:   polinomi in GF(2)[x] come array di DIGIT (1 bit per coefficiente)
 *              -> sindrome s_dense, errore stimato error[]
 *   - ESPANSA: un uint8_t per bit -> syndrome_bits[P], usata dentro il decoder
 *   - SPARSA:  posizioni degli 1 (POS) -> chiave H_sparse / Htr_sparse
 */
#pragma once

#include <stdint.h>
#include <stdalign.h>
#include "params.h"
#include "compiler.h"

/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* TYPES                                                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
#define DIGIT          uint64_t
#define SIGNED_DIGIT   int64_t
#define DIGIT_SIZE_B   8
#define DIGIT_SIZE_b   (DIGIT_SIZE_B << 3)
#define POS            uint32_t
#define SIGNED_POS     int32_t

#define                NUM_BITS_GF2X_ELEMENT (P)
#define              NUM_DIGITS_GF2X_ELEMENT ((P+DIGIT_SIZE_b-1)/DIGIT_SIZE_b)
#define MSb_POSITION_IN_MSB_DIGIT_OF_ELEMENT ( (P % DIGIT_SIZE_b) ? (P % DIGIT_SIZE_b)-1 : DIGIT_SIZE_b-1 )
#define                NUM_BITS_GF2X_MODULUS (P+1)
#define              NUM_DIGITS_GF2X_MODULUS ((P+1+DIGIT_SIZE_b-1)/DIGIT_SIZE_b)
#define MSb_POSITION_IN_MSB_DIGIT_OF_MODULUS (P-DIGIT_SIZE_b*(NUM_DIGITS_GF2X_MODULUS-1))

#define                    INVALID_POS_VALUE (P)

/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* PADDING MACROES                                                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* AVX2 padding macroes*/
#define I32_IN_YMM (256/32)                                     /* number of 32-bit integers that fit inside a YMM register */
#define I8_IN_YMM  (256/8)                                      /* number of 8-bit integers that fit inside a YMM register */
#define PAD32(x)   (((x) + I32_IN_YMM - 1) & ~(I32_IN_YMM - 1)) /* round up the size of an array of 32-bit integers to fit inside YMM regs */
#define PAD8(x)    (((x) + I8_IN_YMM - 1) & ~(I8_IN_YMM - 1))   /* round up the size of an array of 8-bit integers to fit inside YMM regs */

/* AVX512 padding macroes*/
#define I8_IN_ZMM  (512/8)                                      /* number of 8-bit integers that fit inside a ZMM register */
#define PAD64(x)   (((x) + I8_IN_ZMM - 1) & ~(I8_IN_ZMM - 1))   /* round up the size of an array of 8-bit integers to fit inside ZMM regs */

#define N_REGS_H   (PAD32(V) / I32_IN_YMM)
#define N_REGS_UPC (PAD8(N0 * P) / I8_IN_YMM)
#define BFX_HPAD   PAD32(V)                  /* H rows padded to a multiple of 8 */
#define BFX_ALEN   (P + 64)                  /* table + 32-byte wrap pad (+slack) */
#define BFX_PV     ((P / 32) * 32)

#define UPC_LEN        (N0 * P)
#define UPC_TAIL_BITS  (UPC_LEN % I8_IN_YMM)
#define UPC_TAIL_MASK  (UPC_TAIL_BITS ? ((1u << UPC_TAIL_BITS) - 1u) : 0xFFFFFFFFu)
 