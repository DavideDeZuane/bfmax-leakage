 * trace.c - oracolo di accesso per il decoder "plain", single-trace.

  * Esegue UNA decodifica con chiave ed errore fissi (come la vittima di un
 * attacco di cache) e, a ogni iterazione, emette SOLO l'insieme delle
 * posizioni della sindrome toccate da update_syndrome_and_upcs:
 *
 *     { (Htr[b_t][k] + flip_t) mod P : k = 0..V-1 }
 *
 * Non emette ne' il blocco b_t ne' la posizione flippata flip_t: e' esatta-
 * mente cio' che un Prime+Probe sulla L1 osserverebbe (quali linee/posizioni
 * sono state toccate), non il valore del segreto. Con GRAN=set le posizioni
 * sono ridotte all'indice di linea di cache (>>6), come nel canale reale;
 * con GRAN=bit (default) resta la posizione esatta: e' l'oracolo ideale, che
 * isola la domanda crittografica "il pattern di accesso determina la chiave?".



   /* Granularity of the reported positions, modelling two attacker capabilities: */
   /* - bit: it is the strongest possible attacker and gives an upper bound on what is leaked. Report exactly the position touched */
   /*        This answer to the question — does the access pattern determine the key?*/
   /* - set (cache line): Each position is reduced to its cache line. This matches what a Prime+Probe adversary actually observes: */
   /*       which cache line/set was accessed, not the exact byte.*/   

   /* number of possible traces, which depens on the granularity (P because with bit they are all different, P/64 for sets because they can collide) */

