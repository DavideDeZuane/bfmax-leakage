/*
 * Decoder "steroids": decode_prepare / decode_native / decode.
 *
 * decode_prepare costruisce TUTTO cio' che serve al decoder a partire dagli
 * input LEDA: sindrome espansa e tabelle per-chiave (bfmax_ctx_init).
 * In un KEM le tabelle si possono calcolare una volta per chiave; qui sono
 * ricalcolate a ogni chiamata perche' la firma LEDA non ha uno stato
 * per-chiave. Il bench le riporta a parte (decode_prepare).
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
    bfmax_ctx_init(&p->ctx, HtrPosOnes, HPosOnes);
}

int decode_native(OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT], IN dec_params_t *p)
{
    return bfmax_decoder_steroids(out, p);
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
    memset(&p, 0, sizeof p);   /* tabelle e sindrome residua: dati segreti */
    return ret;
}

#define STR_(x) #x
#define STR(x)  STR_(x)
const char *decoder_variant_name(void) { return STR(VARIANT_NAME); }

