# Constant-time implementation 

To make the BF-max decoder constant time, we cannot take advantage of its main feature his main advantages, which is scattered accesses. We have to use bitsliced representation for data.

In such representation updating counter is more expensive then recomputing them from scratch.  
Computing counters is also the most expensive phase on bitsliced representation so we loose all the benefis of using such decoder.