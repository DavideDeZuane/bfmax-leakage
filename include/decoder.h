/*
 * decoder.h - API del decoder, IDENTICA per tutte le versioni.
 *
 * decode() ha la firma di bf_decoding di LEDAcrypt:
 *
 *   int bf_decoding(DIGIT out[], const POSITION_T HtrPosOnes[N0][V],
 *                   const POSITION_T HPosOnes[N0][V], DIGIT privateSyndrome[])
 *
 * con POS al posto di POSITION_T e righe lunghe PAD32(V) invece di V: i
 * kernel AVX2 leggono le righe 8 posizioni alla volta, quindi le righe
 * devono essere allineate a un multiplo di 8 (le posizioni oltre V sono
 * ignorate).
 *
 * Internamente decode() e' divisa in due fasi, esposte per poterle misurare
 * separatamente:
 *
 *   decode_prepare()  costruisce i parametri del decoder a partire dagli
 *                     input LEDA (es. sindrome espansa, tabelle per-chiave)
 *   decode_native()   solo la decodifica
 *
 *   decode() == decode_prepare() + decode_native()
 *
 * Il tipo dei parametri, dec_params_t, e' definito da ciascun decoder in
 * src/decoders/<decoder>/decoder_params.h, scelto dal build (variants.mk).
 */
#pragma once

#include "types.h"

/* Signature of the decoding phase in LEDA for portability */
int decode(
    OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  const POS HtrPosOnes[N0][PAD32(V)],
    IN  const POS HPosOnes[N0][PAD32(V)],
    IN  DIGIT privateSyndrome[NUM_DIGITS_GF2X_ELEMENT]
);

#include "decoder_params.h"   /* dec_params_t: specifico del decoder */

 /* Each decoder can use some differences between this strucutre so we define a function to prepare the parameters for the decoder */
 /* In such a way we know how many clock cyles are required to do the conversion */
void decode_prepare(
    OUT dec_params_t *p,
    IN  const POS HtrPosOnes[N0][PAD32(V)],
    IN  const POS HPosOnes[N0][PAD32(V)],
    IN  const DIGIT privateSyndrome[NUM_DIGITS_GF2X_ELEMENT]);

/* Each decoder has its own dec_params_t */
int decode_native(
    OUT DIGIT out[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  dec_params_t *p);

const char *decoder_variant_name(void);

