/*
 * Decoder "steroids" (steroids.h): parametri costruiti da decode_prepare.
 * Versioni: vt, ct_find, fixed_neg, fixed_scatter (vedi variants.mk).
 */
#pragma once

#include "ctx.h"

typedef struct {
    const POS   (*Htr)[PAD32(V)];   /* HtrPosOnes del chiamante (non copiata)  */
    uint8_t     syndrome_bits[P];   /* sindrome espansa, aggiornata nel loop   */
    bfmax_ctx_t ctx;                /* tabelle di autocorrelazione + copia di H */
} dec_params_t;

/* Funzione nativa del decoder (decoder.c), chiamata da decode_native. */
int bfmax_decoder_steroids(
    OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  dec_params_t *p);
 
 