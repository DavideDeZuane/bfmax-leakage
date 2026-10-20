/* ------------------------------------------------------------------------------*/
/*                                  LIBRARIES                                    */ 
/* ------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <immintrin.h>
#include <stdalign.h>
#include <assert.h>
#include <valgrind/memcheck.h>

/* ------------------------------------------------------------------------------*/
/*                                  MODULUES                                     */ 
/* ------------------------------------------------------------------------------*/
#include "helpers.h"        // printers and random sampling
#include "defines.h"        // all defines necessary
#include "test_utils.h"     // functions to compute syndrome, transpose H, etc.
//#include "bfmax.h"        // bfmax decoding function
#include "steroids.h"       // optimized implementation of the bfmax

/* ------------------------------------------------------------------------------*/
/*                                    MAIN                                       */ 
/* ------------------------------------------------------------------------------*/

#define RUNS 10000
#define SEED 42

int main() {

    srand(SEED);

    uint64_t count_1;
    uint64_t count_2;
    uint64_t sum = 0;

    uint8_t checksum = 0;

    for (int test = 0; test < TESTS + WARMUP; test++) {

        uint64_t s_dense[NUM_DIGITS_GF2X_ELEMENT] = {0};        
        uint64_t e_out_dense[N0*NUM_DIGITS_GF2X_ELEMENT] = {0};
	    uint64_t e_in_dense[N0*NUM_DIGITS_GF2X_ELEMENT] = {0};  // input error vector (dense)
	    uint32_t e_in_sparse[NUM_ERRORS_T] = {0};               // input error vector (sparse)

	    /* -------------------------------------------------- */ 
	    /*                  SECRET KEY 		 	              */ 
	    /* -------------------------------------------------- */ 
	    ALIGNED POS H_sparse[N0][PAD32(V)] = {0};
	    ALIGNED POS Htr_sparse[N0][PAD32(V)] = {0};     
	    /* -------------------------------------------------- */ 

	    /* -------------------------------------------------- */ 
	    /*                    KEYGEN  		 	              */ 
	    /* -------------------------------------------------- */ 
        keygen(H_sparse, Htr_sparse);
        /* ---- Autocorrelation Matrix ---- */
        static bfmax_ctx_t ctx;
        bfmax_ctx_init(&ctx, Htr_sparse, H_sparse);
	    /* -------------------------------------------------- */ 

	    /* -------------------------------------------------- */ 
	    /*                    ENCRYPT  		 	              */ 
	    /* -------------------------------------------------- */ 
        u32_arr_rand_mod_unique(e_in_sparse, NUM_ERRORS_T, N0*P);
        util_densify_error(e_in_dense, e_in_sparse);
        util_compute_syndrome(s_dense, Htr_sparse, e_in_sparse);

        uint8_t syndrome_bits[P]; // bit expanded representation for the syndrome
        dense_to_u8(syndrome_bits, s_dense, P);
	    /* -------------------------------------------------- */ 

        #ifdef RUNNING_ON_VALGRIND
        VALGRIND_MAKE_MEM_UNDEFINED(H_sparse, sizeof(H_sparse));
        VALGRIND_MAKE_MEM_UNDEFINED(Htr_sparse, sizeof(Htr_sparse));
        #endif

	    /* -------------------------------------------------- */ 
	    /*                    DECRYPT  		 	              */ 
	    /* -------------------------------------------------- */ 
        int hw = population_count(s_dense);
        /* benchmark start */
        count_1 = CPUCYCLES(test);
        //uint8_t ret = bfmax_decoder(e_out_dense, Htr_sparse, H_sparse, syndrome_bits, hw);
        uint8_t ret = bfmax_decoder_steroids(e_out_dense, Htr_sparse, H_sparse, syndrome_bits, &ctx);
        count_2 = CPUCYCLES(test);
        /* benchmark end */
        uint8_t cmp = memcmp(e_out_dense, e_in_dense, N0*NUM_DIGITS_GF2X_ELEMENT*sizeof(uint64_t));
        if (cmp != 0) ERROR("e_in != e_out");
	    /* -------------------------------------------------- */ 

        sum += count_2 - count_1;
        checksum += ret;
    }
    printf("[%d] %lu\n", checksum % 100, sum / RUNS);
}
