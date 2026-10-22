

The normal use case of `valgrind/memcheck` is to find uninitialised memory, which it means memory never used. But if we mark as unitialised memory areas where the secret key is stored we can find:

- *Conditional jump or move* depending on the secret key;
- *Memory access* depending on the secret key;

this two condition are very dangerous for cryptographic implementation because they can cause leakage.
Since many constant time violations occur due to memory access or control flow changes, which depend on a secret value, using Valgrind’s ability to track these operations can help developers find timing vulnerabilities.


## Valgrind

Valgrind works by running the binary on a synthetic CPU created by Valgrind and does not introduce any instrumentations during the compilation process. Doing so allows it to run on any binary and makes debugging more straightforward, but it comes at the cost of runtime performance.


## t-test

Il t-test serve a capire se il tempo di esecuzione cambia in base al dato segreto.
Si scelgono due modi di generare l'input segreto 

- **classe fixed**: sempre lo stesso input, per esempio nel caso dell'argmax il massimo in posizione 0, oppure stessa sindrome. 
- **classe random**: un input nuovo a ogni misura.

Si alternano a caso le due classi di misura e per ognuna si registrano i cicli. 
Se il codice e' consant-time, il tempo non dipende dall'input, quindi le due classi hanno la stessa distribuzione di tempi. Se invece dipende dall’input, la classe fixed impiega un tempo “suo”, quello di quell’input particolare, mentre la random impiega il tempo medio su tutti gli input, e le medie si separano.

Ogni misura contiene rumore (interrupt, cache, frequenza della CPU), quindi le due medie non coincidono mai esattamente. Il test serve a capire se la differenza osservata è spiegabile dal rumore. La statistica di Welch è:
```
            media_fixed − media_random
t = ───────────────────────────────────────
     √( var_fixed / n_fixed + var_random / n_random )
```

Quindi t dice di quanti errori standard sono lontane le due medie. Con t ≈ 1 la differenza è del tutto compatibile con il rumore, con t ≈ 100 no.

Si usa Welch e non lo Student classico perché non assume che le due classi abbiano la stessa varianza. Con codice leaky non ce l’hanno: la classe random varia di più, perché il tempo cambia da input a input.

Se il codice è constant-time (ipotesi nulla: medie uguali), con molte misure t si distribuisce circa come una normale standard, quindi di solito sta fra −2 e +2. La probabilità che superi 4.5 in valore assoluto per puro caso è circa 7 su un milione. Per questo |t| > 4.5 è considerato evidenza di leakage. È la soglia usata nella valutazione side-channel (TVLA) e in dudect.


#### References

- [timecop](https://trailofbits.github.io/testing-handbook-preview/pr-preview/pr-2/docs/crypto/constant_time_tool/timecop/)


#### Attacks 

- cross-VM
