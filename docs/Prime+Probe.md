# Introduction


- DRAM vs SRAM per velocità di accesso
- memory wall 
- principio di località soprattutto località spaziale
- le linee
- set-associative
- politica di rimpiazzo

La cache non lavora a byte, ma a linee (o blocchi) di dimensione fissa, tipicamente 64 byte. Quando tocchi anche un solo byte che non è in cache, l’hardware carica dalla DRAM l’intera linea di 64 byte allineata che lo contiene.






How the cache leaks informations?

Il tempo di accesso alla memoria rivela se un dato era in cache o no.

Una hit in L1 costa ~4 cicli mentre una hit in LLC ~40 cicli e una miss che va in DRAM 200/300. 
Misurando la latenza di accesso sappiamo distinguere una hit da una miss con una semplice soglia.

La cache è una risorsa condivisa tra proccesi (mentre la LLC, tra core).  
Se la vittima e l'attaccante competono per gli stessi slot di cache, l'attività della vittima lascia una traccia osservabile nei tempi dell'attaccante. 

Prime+Probe sfrutta esattamente la contesa per gli slot, senza bisogno di condividre memoria con la vittima.


Questo attacco è rimuoroso a causa di:

- prefetcher hardwar: può caricare linee che non hai chiesto. Spesso si dsabilita
- altri processi
- non determinismo delle policy di rimpiazzo 

L'attacco è piu' rumoroso ma non richiede memoria condivisa con la vitima (cosa che Flush e Reload fae quindi è piu' precisa ma richiede co-residenza sullo stesso core)
