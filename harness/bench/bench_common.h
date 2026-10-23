/*
 * bench_common.h - argomenti, statistiche e output dei benchmark.
 *
 * Uso dei bench:  <bench> [runs] [seed] [--csv] [--header]
 *   default   tabella leggibile (unita' di misura, quartili, dispersione)
 *   --csv     una riga CSV per target, per gli script e i grafici
 *   --header  con --csv, stampa anche l'intestazione delle colonne
 */
 #pragma once

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include "decoder.h"
 #include "cycles.h"
 
 #ifndef PROFILE_NAME
 #define PROFILE_NAME "unknown"
 #endif
 
 #define BENCH_CSV_HEADER "variant,profile,cycles_src,target,n,median,q1,q3"
 
 /* ------------------------------------------------------------------------------*/
 /*                                 ARGOMENTI                                     */
 /* ------------------------------------------------------------------------------*/
 typedef struct {
     size_t   runs;
     unsigned seed;
     int      csv;
     int      header;
 } bench_args_t;
 
 static bench_args_t bench_parse_args(int argc, char **argv, size_t default_runs)
 {
     bench_args_t a = { default_runs, 42, 0, 0 };
     int pos = 0;
     for (int i = 1; i < argc; i++) {
         if      (strcmp(argv[i], "--csv") == 0)    a.csv = 1;
         else if (strcmp(argv[i], "--header") == 0) a.header = 1;
         else if (pos == 0) { a.runs = strtoul(argv[i], NULL, 10); pos++; }
         else if (pos == 1) { a.seed = (unsigned)atoi(argv[i]);    pos++; }
         else { fprintf(stderr, "argomento non riconosciuto: %s\n", argv[i]); exit(1); }
     }
     if (a.runs == 0) { fprintf(stderr, "runs deve essere > 0\n"); exit(1); }
     return a;
 }
 
 /* ------------------------------------------------------------------------------*/
 /*                                STATISTICHE                                    */
 /* ------------------------------------------------------------------------------*/
 typedef struct {
     const char *target;
     size_t      n;
     uint64_t    median, q1, q3;
 } bench_stat_t;
 
 static int bench_cmp_u64(const void *a, const void *b)
 {
     uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
     return (x > y) - (x < y);
 }
 
 /* Mediana e quartili (ordina samples in place). */
 static bench_stat_t bench_stat(const char *target, uint64_t *samples, size_t n)
 {
     qsort(samples, n, sizeof *samples, bench_cmp_u64);
     bench_stat_t s = { target, n, samples[n / 2], samples[n / 4], samples[(3 * n) / 4] };
     return s;
 }
 
 /* ------------------------------------------------------------------------------*/
 /*                                  OUTPUT                                       */
 /* ------------------------------------------------------------------------------*/
 /* Unita' scelta sulla mediana e usata per tutta la riga. */
 static void bench_unit(uint64_t ref, double *div, const char **name)
 {
     if      (ref >= 1000000) { *div = 1e6; *name = "Mcycles"; }
     else if (ref >= 10000)   { *div = 1e3; *name = "kcycles"; }
     else                     { *div = 1;   *name = "cycles";  }
 }
 
 static void bench_fmt(char *buf, size_t len, uint64_t v, double div)
 {
     if (div == 1) snprintf(buf, len, "%llu", (unsigned long long)v);
     else          snprintf(buf, len, "%.3f", (double)v / div);
 }
 
 /*
  * Stampa i risultati.
  *   ref >= 0: aggiunge la colonna "% di stats[ref]" (es. quanto pesa
  *             decode_prepare su decode); ref < 0: nessuna colonna.
  */
 static void bench_print(const bench_args_t *a, const char *bench_name,
                         const bench_stat_t *stats, int count, int ref)
 {
     if (a->csv) {
         if (a->header) puts(BENCH_CSV_HEADER);
         for (int i = 0; i < count; i++)
             printf("%s,%s,%s,%s,%zu,%llu,%llu,%llu\n",
                    decoder_variant_name(), PROFILE_NAME, CYCLES_SOURCE,
                    stats[i].target, stats[i].n,
                    (unsigned long long)stats[i].median,
                    (unsigned long long)stats[i].q1,
                    (unsigned long long)stats[i].q3);
         return;
     }
 
     printf("\n%s  |  variant %s  |  profile %s  |  counter %s\n",
            bench_name, decoder_variant_name(), PROFILE_NAME, CYCLES_SOURCE);
     printf("P = %d, V = %d, t = %d  |  %zu runs per target, seed %u\n\n",
            P, V, NUM_ERRORS_T, a->runs, a->seed);
 
     printf("  %-16s %10s   %-22s %-8s %7s", "target", "median", "q1 .. q3", "unit", "spread");
     if (ref >= 0) printf("  %8s", "% of ref");
     printf("\n  ");
     for (int k = 0; k < 16 + 1 + 10 + 3 + 22 + 1 + 8 + 1 + 7 + (ref >= 0 ? 10 : 0); k++) putchar('-');
     putchar('\n');
 
     for (int i = 0; i < count; i++) {
         const bench_stat_t *s = &stats[i];
         double div; const char *unit;
         bench_unit(s->median, &div, &unit);
         char med[32], lo[32], hi[32], range[72];
         bench_fmt(med, sizeof med, s->median, div);
         bench_fmt(lo,  sizeof lo,  s->q1,     div);
         bench_fmt(hi,  sizeof hi,  s->q3,     div);
         snprintf(range, sizeof range, "%s .. %s", lo, hi);
         /* dispersione: ampiezza interquartile relativa alla mediana */
         double spread = s->median ? 100.0 * (double)(s->q3 - s->q1) / (double)s->median : 0;
 
         printf("  %-16s %10s   %-22s %-8s %6.1f%%", s->target, med, range, unit, spread);
         if (ref >= 0 && stats[ref].median)
             printf("  %7.1f%%", 100.0 * (double)s->median / (double)stats[ref].median);
         putchar('\n');
     }
     printf("\n  spread: (q3 - q1) / mediana");
     if (ref >= 0) printf("   |   %% of ref: mediana rispetto a %s", stats[ref].target);
     printf("\n\n");
 }
 
 