#pragma once

#include "defines.h"

typedef struct {
    ALIGNED uint8_t  A[N0][N0][BFX_ALEN];
    ALIGNED POS      H[N0][BFX_HPAD];
    ALIGNED uint16_t idx[N0 * V * BFX_HPAD + 32];
} bfmax_ctx_t;

static INLINE void bfmax_ctx_init(
    OUT bfmax_ctx_t *ctx,
    IN  POS Htr_sparse[N0][PAD32(V)],
    IN  POS H_sparse[N0][PAD32(V)])
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
 
/* ------------------------------------------------------------------------------*/
/* fill an array of n 32-bit words with random values modulo m */
void u32_arr_rand_mod(uint32_t *arr, size_t n, size_t m) {
    for (size_t i = 0; i < n; i++) arr[i] = rand() % m;
}

/* ------------------------------------------------------------------------------*/
int u32_cmp(const void *a, const void *b) {
    uint32_t ua = *(const uint32_t *)a;
    uint32_t ub = *(const uint32_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

/* ------------------------------------------------------------------------------*/
/* fill an array of n 32-bit words with unique random values modulo m */
void u32_arr_rand_mod_unique(uint32_t *arr, size_t n, size_t m) {
    if (n > m) ERROR("n > m");
    size_t placed = 0;
    while (placed < n) {
        uint32_t r = rand() % m;
        uint8_t unique = 1;
        for (size_t i = 0; i < placed && unique; i++) {
            if (arr[i] == r) unique = 0;
        }
        if (unique) {
            arr[placed] = r;
            placed++;
        }
    }
    qsort(arr, n, sizeof(uint32_t), u32_cmp);
}

/* ------------------------------------------------------------------------------*/
/* print an array of n 32-bit words in decimal format */
void u32_arr_print_dec(const uint32_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%lu ", (uint64_t)arr[i]);
    printf("\n");
}

/* ------------------------------------------------------------------------------*/
/* print an array of n 8-bit words in decimal format */
void u8_arr_print_dec(const uint8_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%lu ", (uint64_t)arr[i]);
    printf("\n");
}

/* ------------------------------------------------------------------------------*/
/* print an array of n 64-bit words in binary format bits in a byte are BE bytes in a word can be BE or LE */
void u64_arr_print_bin(const uint64_t *arr, size_t n, uint8_t endianness) {
    for (size_t i = 0; i < n; i++) {
        uint64_t value = arr[i];
        if (endianness == LE) value = __builtin_bswap64(value);
        for (int bit = 64 - 1; bit >= 0; bit--) {
            uint64_t b = (value >> bit) & 1;
            printf("%lu", (uint64_t)b);
        }
        if (endianness == LE) value = __builtin_bswap64(value);
        printf(" ");
    }
    printf("\n");
}
