/*
 * bfmax.h - interfaccia interna del decoder plain.
 */
 #pragma once

 #include "decoder.h"
 
 /* Il decode_native di plain (era bfmax_decoder in bfmax.h). */
 int bfmax_decoder(
    OUT DIGIT error[N0 * NUM_DIGITS_GF2X_ELEMENT],
    IN  dec_params_t *p);
 
 