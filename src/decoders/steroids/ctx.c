/*
 * ctx.c - tabelle per-chiave del decoder steroids (da helpers.h).
 * Chiamata da decode_prepare: il bench la riporta in "decode_prepare".
 */
 #include <string.h>
 #include "ctx.h"
 
 void bfmax_ctx_init(
     OUT bfmax_ctx_t *ctx,
     IN  const POS Htr_sparse[N0][PAD32(V)],
     IN  const POS H_sparse[N0][PAD32(V)])
 {
     memset(ctx->A, 0, sizeof ctx->A);
     for (int b = 0; b < N0; b++) {
        for (int b2 = 0; b2 < N0; b2++) {
           uint8_t *A = ctx->A[b][b2];
           for (int k = 0; k < V; k++) {
              for (int m = 0; m < V; m++) {
                 uint32_t t = Htr_sparse[b][k] + H_sparse[b2][m];
                 t -= (t >= P) ? P : 0;
                 A[t]++;
              }
           }
           memcpy(A + P, A, 32);   /* wrap pad: a vector may straddle the rotation point */
        }
     }
     /* own copy with zeroed padding lanes, independent of the caller's padding */
     memset(ctx->H, 0, sizeof ctx->H);
     for (int b2 = 0; b2 < N0; b2++) {
        memcpy(ctx->H[b2], H_sparse[b2], V * sizeof(POS));
     }
 }
 
 