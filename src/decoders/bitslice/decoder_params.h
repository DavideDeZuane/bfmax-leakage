/*
 * Decoder "ct": BF-Max constant-time con contatori bitsliced
 * (da bf_decoding_CT di LEDAcrypt, convertito alla rappresentazione densa
 * little-endian della repo: coefficiente i = bit i%64 della digit i/64).
 */
 #pragma once

 #include "types.h"
 #include "bitslice.h"
 
 typedef struct {
     const POS (*H)[PAD32(V)];                              /* HPosOnes del chiamante (non copiata)   */
     ALIGNED DIGIT HTr[N0][NUM_DIGITS_SLICED];              /* H^T densa, una riga per blocco          */
     ALIGNED DIGIT syndrome[NUM_DIGITS_SLICED];             /* copia della sindrome, aggiornata        */
 } dec_params_t;
 
 /* Funzione nativa del decoder (decoder.c), chiamata da decode_native. */
 int bf_decoding_ct(
    OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  dec_params_t *p);
 
 