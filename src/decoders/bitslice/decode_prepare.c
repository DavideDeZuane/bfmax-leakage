#include <string.h>
#include "decoder.h"
#include "ct.h"

/* dense[] |= x^pos per ogni pos: tocca tutte le digit, l'indirizzo scritto non
 * dipende da pos (era gf2x_mod_densify_CT) */
static void gf2x_mod_densify_ct(DIGIT dense[NUM_DIGITS_SLICED], const POS pos[], int n)
{
    memset(dense, 0, NUM_DIGITS_SLICED * sizeof(DIGIT));
    for (int k = 0; k < n; k++) {
        uint64_t word = pos[k] / DIGIT_SIZE_b;
        DIGIT    bit  = (DIGIT)1 << (pos[k] % DIGIT_SIZE_b);
        for (uint64_t d = 0; d < NUM_DIGITS_GF2X_ELEMENT; d++)
            dense[d] |= bit & ~ct_nz64_mask(d ^ word);
    }
}

void decode_prepare(
    OUT dec_params_t *p,
    IN  const POS HtrPosOnes[N0][PAD32(V)],
    IN  const POS HPosOnes[N0][PAD32(V)],
    IN  const DIGIT privateSyndrome[NUM_DIGITS_GF2X_ELEMENT])
{
    p->H = HPosOnes;
    for (int b = 0; b < N0; b++)
        gf2x_mod_densify_ct(p->HTr[b], HtrPosOnes[b], V);
    memset(p->syndrome, 0, sizeof p->syndrome);
    memcpy(p->syndrome, privateSyndrome, NUM_DIGITS_GF2X_ELEMENT * sizeof(DIGIT));
}

int decode_native(OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT], IN dec_params_t *p)
{
    return bf_decoding_ct(out, p);
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
    memset(&p, 0, sizeof p);   /* H^T e sindrome residua: dati segreti */
    return ret;
}

#define STR_(x) #x
#define STR(x)  STR_(x)
const char *decoder_variant_name(void) { return STR(VARIANT_NAME); }

