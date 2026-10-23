# BF-max leakage

See the leakage of the BF-max decoder

## Variants

This section describes the BF-Max decoder variants available in the repository, what each one protects against, and what is still known to leak.

### `plain`
 
Directory: `src/decoders/plain`. 
The basic BF-Max decoder, with the following feature:
 
- Expanded syndrome; initial counters with `compute_upcs`, to take advantages of sparse access.
- `argmax_u8_ct` recomputes the maximum and finds its first position at every iteration, scanning the whole array.
- `update_syndrome_and_upcs` updates the counters **row by row** with sparse accesses, and returns as soon as the syndrome weight reaches zero.

Remaining leaks: 

- early return in the update and in the main loop, 
- loop bounds and addresses in `compute_upcs`, 
- scatter addresses in the update
- toggle of the error bit

### `steroids`
 

Directory: `src/decoders/steroids`.   
The baseline: **fully variable-time** and the fastest variant.
 

### `bitslice`

Directory: `src/decoders/bitslice`.  
**Fully constant-time**: no branch and no memory address depends on secret data. 
The price to pay is not taking advantage of the scattered memory access, which results in a significant decline in performance (we have to recompute counters at each iteration which is one most expensive phase).

At every iteration:


1. the counters are **recomputed from scratch**;
2. the position of the first maximum counter is found with a constant-time argmax on the bitsliced counters (bit planes from the most significant down, selections with masks);
3. the error bit and the syndrome (`s ^= x^pos · HTr[block]`) are updated with masks, enabled by `active` (syndrome not yet zero). 

The number of iterations is fixed.

Options:
 
| Flag | Effect |
|------|--------|
| `-DCT_ITERATIONS` | t iterations instead of 1.5·t, as in LEDA/BIKE (~50 M cycles). **Not equivalent**: when BF-Max needs more than t flips, decoding fails (about 1 in 5 decodings in our tests). |
| `-DCT_VARSHIFT` | the bit shift inside the rotation uses a single variable-amount shift, as in BIKE (~55 M cycles). Constant-time on x86 hardware, but valgrind cannot track a secret shift amount and reports it as a leak in `rot_from_dup`. 


