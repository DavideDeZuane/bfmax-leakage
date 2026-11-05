
- `attack.c` is the script that simulate Flush+Reload attack (the two algorithm have shared memory)
- `trace2support.py` is the script that do alignment and voting
- `keyrecovery.py` is the key recovery algorithm (the omogeneus linear system)


- `truth`: this dir contains the support of the secret key
- `trace`: contains the output provided by the F+R oracle
- `suppt`: output of the trace2support algorithm
- `recov`: contains the output of the key recovery algorithm



The attack F+R plain as the theoretical one it doesn't work due to carry on 

```
POS=1 python3 keyrecover.py                  # -> FULL PRIVATE KEY RECOVERED
POS=0 python3 keyrecover.py                  # -> KEY NOT RECOVERED (recall 66/71)
```
