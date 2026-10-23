

Each directory contains the implementation of the BF-max decoder with some modifications.  
The general structure is the following:

- `decoder_params.h`
- `decode_prepare.c`
- `decoder.c`








# Constant-time implementation 

To make the BF-max decoder constant time, we cannot take advantage of its main feature his main advantages, which is scattered accesses. We have to use bitsliced representation for data.

In such representation updating counter is more expensive then recomputing them from scratch.  
Computing counters is also the most expensive phase on bitsliced representation so we loose all the benefis of using such decoder.

# Full sparse implementation

This implementation takes full advantage of the sparsity to achieve the best possible performances.  
To minimize scattered memory accesses, we used an autocorrelation matrix to split the counter updates phase in:

- *a fixed contribution*: which depens on the aucorrelation matrix and touch every position of the counter array.
- *a sparse contribution*: which touch the position who have to be incremented, decrease with each iteration.
