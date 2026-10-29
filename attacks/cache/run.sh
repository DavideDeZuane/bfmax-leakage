#!/usr/bin/env bash
#
# run_trace.sh - compila ed esegue l'oracolo attacks/cache/trace.c
#
# Uso:
#   ./run_trace.sh [bit|set] [seed]
#
#   arg 1 : granularita'  ->  bit (default) | set
#               bit = posizione esatta toccata (dominio P)
#               set = linea di cache, pos>>6 (dominio ~P/64), come un
#                     attaccante Prime+Probe
#   arg 2 : seed (default 1) - fissa chiave ed errore, run riproducibile
#
# Variabili d'ambiente opzionali: CC (default cc), MARCH (default x86-64-v3)
#
# Output, nella cartella da cui lanci lo script:
#   trace.txt  insiemi toccati per iterazione (input dello scorer)
#   truth.txt  chiavi vere ridotte alla granularita' scelta (per lo scorer)
#
set -euo pipefail

GRAN_ARG="${1:-bit}"
SEED="${2:-1}"

case "$GRAN_ARG" in
  bit) GRAN_ENV="" ;;         # trace.c: default = bit
  set) GRAN_ENV="set" ;;
  *) echo "granularita' '$GRAN_ARG' non valida: usa 'bit' o 'set'" >&2; exit 1 ;;
esac

# radice della repo, relativa a questo script (.../attacks/cache/run_trace.sh)
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

CC="${CC:-cc}"
MARCH="${MARCH:-x86-64-v3}"
INC=(-I"$ROOT/include" -I"$ROOT/src/core" -I"$ROOT/harness/common" -I"$ROOT/src/decoders/plain")

BUILD="$HERE/build"
mkdir -p "$BUILD"

# trace.c chiama argmax_u8_ct e update_syndrome_and_upcs, che sono 'static
# INLINE' dentro src/decoders/plain/decoder.c e non esposte da alcun header:
# le rendiamo visibili includendo decoder.c nella SOLA unita' di trace.c.
# instance.c va compilata a parte, altrimenti bfmax_decoder risulta duplicato.
echo ">> compilo trace.c (con decoder.c incluso)"
"$CC" -O2 -march="$MARCH" "${INC[@]}" \
      -include "$ROOT/src/decoders/plain/decoder.c" \
      -c "$HERE/trace.c" -o "$BUILD/trace.o"

echo ">> compilo instance.c"
"$CC" -O2 -march="$MARCH" "${INC[@]}" \
      -c "$ROOT/harness/common/instance.c" -o "$BUILD/instance.o"

echo ">> linko"
"$CC" "$BUILD/trace.o" "$BUILD/instance.o" -o "$BUILD/trace" -lm

echo ">> eseguo: granularita'=$GRAN_ARG  seed=$SEED"
GRAN="$GRAN_ENV" "$BUILD/trace" truth.txt "$SEED" > trace.txt

echo
echo "fatto. Prodotti nella cartella corrente:"
echo "  trace.txt  ($(grep -vc '^#' trace.txt) iterazioni)"
echo "  truth.txt"
echo
echo "per il recupero chiave:"
echo "  python3 $HERE/key-recovery.py truth.txt < trace.txt"
