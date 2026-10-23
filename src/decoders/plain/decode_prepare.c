/*
 * Decoder "plain": decode_prepare / decode_native / decode.
 *
 * decode_prepare espande la sindrome, ne calcola il peso e carica H in
 * registri AVX2 (come l'inizio di bfmax_decoder e il main.c originali).
 */
#include <string.h>
#include "decoder.h"
#include "repr.h"

void decode_prepare(
    OUT dec_params_t *p,
    IN  const POS HtrPosOnes[N0][PAD32(V)],
    IN  const POS HPosOnes[N0][PAD32(V)],
    IN  const DIGIT privateSyndrome[NUM_DIGITS_GF2X_ELEMENT])
{
    p->Htr = HtrPosOnes;
    dense_to_u8(p->syndrome_bits, privateSyndrome, P);
    p->hw_start = population_count((DIGIT *)privateSyndrome);

    const __m256i lane_idx = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    for (int block = 0; block < N0; block++) {
        for (int r = 0; r < N_REGS_H; r++) {
            int remaining = V - r * I32_IN_YMM;
            /* lane j attiva se j < remaining */
            __m256i kmask = _mm256_cmpgt_epi32(_mm256_set1_epi32(remaining), lane_idx);
            p->v_H_sparse[block][r] = _mm256_maskload_epi32(
                (const int *)&HPosOnes[block][r * I32_IN_YMM], kmask);
        }
    }
}

int decode_native(OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT], IN dec_params_t *p)
{
    return bfmax_decoder(out, p);
}

int decode(
    OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  const POS HtrPosOnes[N0][PAD32(V)],
    IN  const POS HPosOnes[N0][PAD32(V)],
    IN  DIGIT privateSyndrome[NUM_DIGITS_GF2X_ELEMENT])
{
    dec_params_t p;
    decode_prepare(&p, HtrPosOnes, HPosOnes, privateSyndrome);
    int ret = decode_native(out, &p);
    memset(&p, 0, sizeof p);   /* sindrome residua: dato segreto */
    return ret;
}

#define STR_(x) #x
#define STR(x)  STR_(x)
const char *decoder_variant_name(void) { return STR(VARIANT_NAME); }

