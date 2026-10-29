# Cache Side Channel

Critically, cache components can be shared amongst concurrent processe, running at potentially different privilege levels. This opens the door for one process to infer the bahaviour of another by observing its cache usage

Cache components are defined by two key properties:

- the number of its *sets*
- the number of *ways* within each set.

The entries within a cache are divied up into a number of sets such that each entry for the cache maps to a particular set.


## Prime+Probe

This attack aims to infer the behaviour of some victim process based on its usage of a particular cache set. This is carried out in three main stages:

1. **Prime**: the attacker fills a chosen set within the cache by accessing a number of memory locations that all map to the same set. A collection of suc addresses is known as an eviction set.
2. **Wait**: the attacker waits a short period whilst the victim executes.
3. **Probe**: the attacker re-accesses the eviction set and times the accesses. A fast access inidicates theri data was still stored in the cache and so the victim did not use the set in the time they were waiting. A slow access indicates their entries were evicted from the cache and so the victim used the set 
