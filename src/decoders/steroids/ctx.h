/*
 * ctx.h - tabelle per-chiave del decoder "steroids" (erano in helpers.h,
 * poi in decoder.h). Le usa solo il decoder steroids: gli altri decoder non
 * le includono. Contengono dati SEGRETI.
 */
 #pragma once

 #include "types.h"
 
 typedef struct {
     ALIGNED uint8_t  A[N0][N0][BFX_ALEN];          /* autocorrelazioni H^T H         */
     ALIGNED POS      H[N0][BFX_HPAD];              /* copia di H_sparse, padding a 0 */
     ALIGNED uint16_t idx[N0 * V * BFX_HPAD + 32];  /* buffer di lavoro dello scatter */
 } bfmax_ctx_t;
 
 void bfmax_ctx_init(
     OUT bfmax_ctx_t *ctx,
     IN  const POS Htr_sparse[N0][PAD32(V)],
     IN  const POS H_sparse[N0][PAD32(V)]);
 
 