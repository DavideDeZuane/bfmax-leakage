/*
 * Decoder "plain" (bfmax.h): parametri costruiti da decode_prepare.
 * Nessuna tabella per-chiave.
 */
 #pragma once

 #include <immintrin.h>
 #include "types.h"
 
 typedef struct {
     const POS (*Htr)[PAD32(V)];            /* HtrPosOnes del chiamante (non copiata) */
     __m256i   v_H_sparse[N0][N_REGS_H];    /* righe di H in registri, lane >= V a 0  */
     uint8_t   syndrome_bits[P];            /* sindrome espansa, aggiornata nel loop  */
     int       hw_start;                    /* peso iniziale della sindrome           */
 } dec_params_t;
 
 