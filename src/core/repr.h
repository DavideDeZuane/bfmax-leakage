/*
 * repr.h - confine tra le due rappresentazioni della sindrome.
 *
 *   DENSA   DIGIT s_dense[NUM_DIGITS_GF2X_ELEMENT]   1 bit per coefficiente
 *           -> quella del KEM: calcolo della sindrome, peso (popcount)
 *   ESPANSA uint8_t syndrome_bits[P]                  1 byte per coefficiente
 *           -> quella del decoder: compute_upcs somma byte a byte con AVX2
 *              e update_* fa toggle/lettura di singoli bit senza shift/maschere
 *
 * La conversione avviene UNA volta per decodifica, prima del decoder.
 * Il bench la misura a parte (target "dense_to_u8") per poterla sommare.
 */
 #pragma once

 #include "gf2x.h"
 
 /* dense -> espansa. Indici pubblici: costante in tempo e accessi. */
 static INLINE void dense_to_u8(
    OUT uint8_t u8[],
    IN  const DIGIT dense[],
    IN  int len)
 {
    for (int i = 0; i < len; i++) u8[i] = (uint8_t)gf2x_get_coeff(dense, i);
 }
 
 /* espansa -> densa (utile per controlli sulla sindrome residua). */
 static INLINE void u8_to_dense(
    OUT DIGIT dense[],
    IN  const uint8_t u8[],
    IN  int len)
 {
    memset(dense, 0, ((len + DIGIT_SIZE_b - 1) / DIGIT_SIZE_b) * DIGIT_SIZE_B);
    for (int i = 0; i < len; i++)
       dense[i / DIGIT_SIZE_b] |= (DIGIT)(u8[i] & 1) << (i % DIGIT_SIZE_b);
 }
 
 