/*
 * harness.h - utilita' comuni a tutti gli harness: generazione di chiavi,
 * errori e sindromi (da helpers.h e test_utils.h originali).
 * NON fa parte della libreria del decoder.
 */
 #pragma once

 #include <stdio.h>
 #include <stdlib.h>
 #include "decoder.h"
 #include "gf2x.h"
 
 #define ERROR(...)                                  \
     do {                                            \
         printf("\n\033[0;31m [!] %s: ", __func__);  \
         printf(__VA_ARGS__);                        \
         printf("\033[0m\n\n");                      \
         exit(1);                                    \
     } while(0)
 
 /* ------------------------------------------------------------------------------*/
 /*                               KEY / INSTANCE                                  */
 /* ------------------------------------------------------------------------------*/
 /* Chiave segreta nel formato LEDA (input di decode). */
 typedef struct {
     ALIGNED POS H_sparse[N0][PAD32(V)];
     ALIGNED POS Htr_sparse[N0][PAD32(V)];
 } bfkey_t;
 
 /* Un input del decoder (sindrome densa) + il risultato atteso. */
 typedef struct {
     POS     e_sparse[NUM_ERRORS_T];
     DIGIT   e_dense[N0 * NUM_DIGITS_GF2X_ELEMENT];   /* errore atteso (denso) */
     DIGIT   s_dense[NUM_DIGITS_GF2X_ELEMENT];        /* sindrome densa        */
 } instance_t;
 
 /* keygen: H casuale e la sua trasposta */
 void key_generate(bfkey_t *key);
 
 /* errore casuale di peso NUM_ERRORS_T con posizioni in [lo, lo + range) */
 void instance_generate(instance_t *in, bfkey_t *key, uint32_t lo, uint32_t range);
 
 /* errore casuale su tutto [0, N0*P) */
 static inline void instance_random(instance_t *in, bfkey_t *key)
 {
     instance_generate(in, key, 0, N0 * P);
 }
 
 /* ------------------------------------------------------------------------------*/
 /*                                  RANDOM                                       */
 /* ------------------------------------------------------------------------------*/
 void u32_arr_rand_mod(uint32_t *arr, size_t n, size_t m);
 void u32_arr_rand_mod_unique(uint32_t *arr, size_t n, size_t m);
 
 /* ------------------------------------------------------------------------------*/
 /*                                 PRINTERS                                      */
 /* ------------------------------------------------------------------------------*/
 /* ordine dei byte nella parola per u64_arr_print_bin */
 #define BE 0                // big endian
 #define LE 1                // little endian
 
 void u32_arr_print_dec(const uint32_t *arr, size_t n);
 void u8_arr_print_dec(const uint8_t *arr, size_t n);
 void u64_arr_print_bin(const uint64_t *arr, size_t n, uint8_t endianness);
 
 