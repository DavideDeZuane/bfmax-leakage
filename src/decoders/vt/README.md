# Full sparse implementation

This implementation takes full advantage of the sparsity to achieve the best possible performances.  
To minimize scattered memory accesses, we used an autocorrelation matrix to split the counter updates phase in:

- *a fixed contribution*: which depens on the aucorrelation matrix and touch every position of the counter array.
- *a sparse contribution*: which touch the position who have to be incremented, decrease with each iteration.
