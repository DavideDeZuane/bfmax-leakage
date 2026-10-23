#include <string.h>
#include "harness.h"

/* ------------------------------------------------------------------------------*/
/*                                  RANDOM                                       */
/* ------------------------------------------------------------------------------*/
/* fill an array of n 32-bit words with random values modulo m */
void u32_arr_rand_mod(uint32_t *arr, size_t n, size_t m) {
    for (size_t i = 0; i < n; i++) arr[i] = rand() % m;
}

static int u32_cmp(const void *a, const void *b) {
    uint32_t ua = *(const uint32_t *)a;
    uint32_t ub = *(const uint32_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

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
/*                                 PRINTERS                                      */
/* ------------------------------------------------------------------------------*/
void u32_arr_print_dec(const uint32_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%lu ", (unsigned long)arr[i]);
    printf("\n");
}

void u8_arr_print_dec(const uint8_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%lu ", (unsigned long)arr[i]);
    printf("\n");
}

/* bits in a byte are BE, bytes in a word can be BE or LE */
void u64_arr_print_bin(const uint64_t *arr, size_t n, uint8_t endianness) {
    for (size_t i = 0; i < n; i++) {
        uint64_t value = arr[i];
        if (endianness == LE) value = __builtin_bswap64(value);
        for (int bit = 64 - 1; bit >= 0; bit--) printf("%lu", (unsigned long)((value >> bit) & 1));
        printf(" ");
    }
    printf("\n");
}

/* ------------------------------------------------------------------------------*/
/*                         KEYGEN / ENCRYPT (lato test)                          */
/* ------------------------------------------------------------------------------*/
static void transposeHPosOnes(POS HtrPosOnes[N0][PAD32(V)], POS CONST HPosOnes[N0][PAD32(V)])
{
   for (int i = 0; i < N0; i++) {
      for (int k = 0; k < V; k++) {
         HtrPosOnes[i][k] = (P - HPosOnes[i][k]) % P; /* transposes indexes */
      }
   }
}

static void keygen(uint32_t H[N0][PAD32(V)], uint32_t H_tr[N0][PAD32(V)])
{
    for (int block = 0; block < N0; block++) {
        u32_arr_rand_mod_unique(H[block], V, P);
    }
    transposeHPosOnes(H_tr, H);
}

static void util_densify_error(DIGIT dense[N0*NUM_DIGITS_GF2X_ELEMENT], CONST POS positions[])
{
   for (int j = 0; j < NUM_ERRORS_T; j++) {
      int block = positions[j] / P;
      gf2x_set_coeff(dense + block * NUM_DIGITS_GF2X_ELEMENT, positions[j] % P, 1);
   }
}

/* for r in rows, for c in columns: s[c] = s[c] xor (e[r] & H[r][c]) */
static void util_compute_syndrome(
   OUT DIGIT s_dense[NUM_DIGITS_GF2X_ELEMENT],
   IN  POS Htr_sparse[N0][PAD32(V)],
   IN  POS e_sparse[NUM_ERRORS_T])
{
   DIGIT Htr_dense[N0][NUM_DIGITS_GF2X_ELEMENT];
   memset(Htr_dense, 0, sizeof Htr_dense);
   for (int block = 0; block < N0; block++)
      gf2x_mod_densify_VT(Htr_dense[block], Htr_sparse[block], V);

   DIGIT saux[NUM_DIGITS_GF2X_ELEMENT];
   memset(s_dense, 0x00, NUM_DIGITS_GF2X_ELEMENT * DIGIT_SIZE_B);
   POS blkErrorPos[NUM_ERRORS_T];
   for (int i = 0; i < N0; i++) {
      unsigned int filled = 0;
      for (int j = 0; j < NUM_ERRORS_T; j++) {
         if (e_sparse[j] / P == (POS)i) {
            blkErrorPos[filled] = e_sparse[j] % P;
            filled++;
         }
      }
      gf2x_mod_mul_dense_to_sparse(saux, Htr_dense[i], blkErrorPos, filled);
      gf2x_mod_add(s_dense, s_dense, saux);
   }
}

void key_generate(bfkey_t *key)
{
    memset(key, 0, sizeof *key);
    keygen(key->H_sparse, key->Htr_sparse);
}

void instance_generate(instance_t *in, bfkey_t *key, uint32_t lo, uint32_t range)
{
    u32_arr_rand_mod_unique(in->e_sparse, NUM_ERRORS_T, range);
    for (int i = 0; i < NUM_ERRORS_T; i++) in->e_sparse[i] += lo;

    memset(in->e_dense, 0, sizeof in->e_dense);
    util_densify_error(in->e_dense, in->e_sparse);
    util_compute_syndrome(in->s_dense, key->Htr_sparse, in->e_sparse);
}

