

## Ridurre il leakage 

Calcolare i counter in modalita' bisliced e poi convertire in uint8, in questo modo non ho piu' accessi che dipendono dalla chiave ma una parte costante l'unica parte che rimarrebbe sarebbe update della sindrome.

```

void sliced_to_uint8(const bs_operand_t* bs, uint8_t* ctrs, int total_elements, int bitsliced_width) {
   memset(ctrs, 0, total_elements * sizeof(uint8_t));

   for (int i = 0; i < N0; i++) {
      // global offset 
      const bs_operand_t* bs_block = bs + i * NUM_SLICES_GF2X_ELEMENT;
      
      for (int j = 0; j < P; j++) {

         // inversione dell'ordine dato che il polinomio è rappresentato in big endian mentre il counter array considera le posizioni in little endian
         int poly_idx    = (P - 1) - j;
         // tocca aggiungere il padding di 61 bit che si trova all'inizio
         int adjusted    = poly_idx + SLACK_SIZE;
         int block       = adjusted / 256;
         int lane        = (adjusted / 64) % 4;
         int bit_in_lane = 63 - (adjusted % 64);

         uint64_t lanes[4];
         uint8_t val = 0;
         for (int k = 0; k < bitsliced_width; k++) {
            memcpy(lanes, &bs_block[block].slice[k], sizeof(lanes));
            uint64_t bit = (lanes[lane] >> bit_in_lane) & 1ULL;
            val += (uint8_t)(bit << k);
         }
         ctrs[i * P + j] = val;
      }
  }
}
```


































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


## Mastik 

Micro-architectural side-channel attacks exploit contention on internal components of the processor to leak information between processes. While in theory such attacks are straightforward, practical implementations tend to be finicky and require significant understanding of poorly documented processor features and other domain-specific arcane knowledge.


[Mastik](https://github.com/0xADE1A1DE/Mastik)is a toolkit for experimenting with micro-architectural side-channel attacks. Mastik aims to provide implementations of published attack and analysis techniques.

#### References

- [timecop](https://trailofbits.github.io/testing-handbook-preview/pr-preview/pr-2/docs/crypto/constant_time_tool/timecop/)


#### Attacks 

- cross-VM
