#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <immintrin.h>
#include <stdalign.h>
#include <string.h>

/* ------------------------------------------------------------------------------*/
/*                                PARAMETERS                                     */ 
/* ------------------------------------------------------------------------------*/
#define P               (10883)
#define N0              (2)
#define V               (71)
#define NUM_ERRORS_T    (133)
#define CATEGORY        (1)

/* ------------------------------------------------------------------------------*/
/*                                  TYPES                                        */ 
/* ------------------------------------------------------------------------------*/
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

/* ------------------------------------------------------------------------------*/
/*                             PADDING MACROES                                   */ 
/* ------------------------------------------------------------------------------*/
/* AVX2 padding macroes*/
#define I32_IN_YMM (256/32)                                     /* number of 32-bit integers that fit inside a YMM register */
#define I8_IN_YMM  (256/8)                                      /* number of 8-bit integers that fit inside a YMM register */
#define PAD32(x)   (((x) + I32_IN_YMM - 1) & ~(I32_IN_YMM - 1)) /* round up the size of an array of 32-bit integers to fit inside YMM regs */
#define PAD8(x)    (((x) + I8_IN_YMM - 1) & ~(I8_IN_YMM - 1))   /* round up the size of an array of 8-bit integers to fit inside YMM regs */

/* AVX512 padding macroes*/
#define I8_IN_ZMM  (512/8)                                      /* number of 8-bit integers that fit inside a ZMM register */
#define PAD64(x)   (((x) + I8_IN_ZMM - 1) & ~(I8_IN_ZMM - 1))   /* round up the size of an array of 8-bit integers to fit inside ZMM regs */

/* ------------------------------------------------------------------------------*/
/*                               BFMAX MACROES                                   */ 
/* ------------------------------------------------------------------------------*/
#define N_REGS_H   (PAD32(V) / I32_IN_YMM)
#define N_REGS_UPC (PAD8(N0 * P) / I8_IN_YMM)
#define BFX_HPAD   PAD32(V)                  /* H rows padded to a multiple of 8 */
#define BFX_ALEN   (P + 64)                  /* table + 32-byte wrap pad (+slack) */
#define BFX_PV     ((P / 32) * 32)

/* ------------------------------------------------------------------------------*/
/*                              SPACES MACROES                                   */ 
/* ------------------------------------------------------------------------------*/
#define IS_REPRESENTABLE_IN_D_BITS(D, N) (((unsigned long) N >= (1UL << (D - 1)) && (unsigned long) N < (1UL << D)) ? D : -1)
#define BITS_TO_REPRESENT(N) (N == 0 ? 1 : (31 + IS_REPRESENTABLE_IN_D_BITS( 1, N) + IS_REPRESENTABLE_IN_D_BITS( 2, N) + IS_REPRESENTABLE_IN_D_BITS( 3, N) + IS_REPRESENTABLE_IN_D_BITS( 4, N) + IS_REPRESENTABLE_IN_D_BITS( 5, N) + IS_REPRESENTABLE_IN_D_BITS( 6, N) + IS_REPRESENTABLE_IN_D_BITS( 7, N) + IS_REPRESENTABLE_IN_D_BITS( 8, N) + IS_REPRESENTABLE_IN_D_BITS( 9, N) + IS_REPRESENTABLE_IN_D_BITS(10, N) + IS_REPRESENTABLE_IN_D_BITS(11, N) + IS_REPRESENTABLE_IN_D_BITS(12, N) + IS_REPRESENTABLE_IN_D_BITS(13, N) + IS_REPRESENTABLE_IN_D_BITS(14, N) + IS_REPRESENTABLE_IN_D_BITS(15, N) + IS_REPRESENTABLE_IN_D_BITS(16, N) + IS_REPRESENTABLE_IN_D_BITS(17, N) + IS_REPRESENTABLE_IN_D_BITS(18, N) + IS_REPRESENTABLE_IN_D_BITS(19, N) + IS_REPRESENTABLE_IN_D_BITS(20, N) + IS_REPRESENTABLE_IN_D_BITS(21, N) + IS_REPRESENTABLE_IN_D_BITS(22, N) + IS_REPRESENTABLE_IN_D_BITS(23, N) + IS_REPRESENTABLE_IN_D_BITS(24, N) + IS_REPRESENTABLE_IN_D_BITS(25, N) + IS_REPRESENTABLE_IN_D_BITS(26, N) + IS_REPRESENTABLE_IN_D_BITS(27, N) + IS_REPRESENTABLE_IN_D_BITS(28, N) + IS_REPRESENTABLE_IN_D_BITS(29, N) + IS_REPRESENTABLE_IN_D_BITS(30, N) + IS_REPRESENTABLE_IN_D_BITS(31, N) + IS_REPRESENTABLE_IN_D_BITS(32, N) ) )

/* ------------------------------------------------------------------------------*/
/*                                  BENCHMARK                                    */ 
/* ------------------------------------------------------------------------------*/
#ifdef BENCH
#include <cpucycles.h>
#define TESTS RUNS
#define WARMUP 1000
#define CPUCYCLES(test) ((test) >= WARMUP ? cpucycles() : 0)
#else
/* ------------------------------------------------------------------------------*/
/*                               DEBUG AND TEST                                  */ 
/* ------------------------------------------------------------------------------*/
#define TESTS 1
#define WARMUP 0
#define CPUCYCLES(test) 0
#endif

#ifdef SKIP_INLINE
#define INLINE
#else
#define INLINE inline
#endif

#define RESTRICT
#define CONST

#ifdef DEBUG
#define DEBUG_PRINT(...)                            \
    do {                                            \
        printf("\033[0;33m");                       \
        printf(__VA_ARGS__);                        \
        printf("\033[0m");                          \
    } while(0)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

#define ALIGNED alignas(32)

#define ERROR(...)                                  \
    do {                                            \
        printf("\n\033[0;31m [!] %s: ", __func__);  \
        printf(__VA_ARGS__);                        \
        printf("\033[0m\n\n");                      \
        exit(1);                                    \
    } while(0)

#define BE 0                // big endian
#define LE 1                // little endian

#define IN                  // input parameter
#define OUT                 // output parameter
