#!/usr/bin/env bash
# run_fr.sh - Flush+Reload parametrico. Crea truth/ trace/, compila ed esegue.
# Uso:   ./run_fr.sh [toy|real] [seed] [line] 
# Env:   CC MARCH THRESH CPU   (CPU=core per taskset)
# Prima, sull'host:  ./setup-bench.sh <CPU>   (prefetcher off, governor, ...)
set -euo pipefail
INST="${1:-toy}"; SEED="${2:-1}"; LINE_ARG="${3:-64}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
CC="${CC:-cc}"; MARCH="${MARCH:-x86-64-v3}"; THRESH="${THRESH:-130}"; CPU="${CPU:-}"
INC=(-I"$ROOT/include" -I"$ROOT/src/core" -I"$ROOT/harness/common" -I"$ROOT/src/decoders/plain")
case "$INST" in
  toy)  DTOY=(-DTOY); BUILD="$HERE/build/fr_toy" ;;
  real) DTOY=();      BUILD="$HERE/build/fr_real" ;;
  *) echo "istanza '$INST' non valida: usa toy|real" >&2; exit 1 ;;
esac
mkdir -p "$BUILD" "$HERE/truth" "$HERE/trace" "$HERE/recov"
echo ">> compilo attack_fr.c [$INST]"
"$CC" -O2 -march="$MARCH" "${DTOY[@]}" "${INC[@]}" -include "$ROOT/src/decoders/plain/decoder.c" -c "$HERE/attack_fr.c" -o "$BUILD/attack_fr.o"
"$CC" -O2 -march="$MARCH" "${DTOY[@]}" "${INC[@]}" -c "$ROOT/harness/common/instance.c" -o "$BUILD/instance.o"
"$CC" "$BUILD/attack_fr.o" "$BUILD/instance.o" -o "$BUILD/attack_fr" -lm
RUN=("$BUILD/attack_fr" "$SEED"); [ -n "$CPU" ] && RUN=(taskset -c "$CPU" "${RUN[@]}")
echo ">> eseguo [$INST seed=$SEED line=$LINE_ARG thresh=$THRESH cpu=${CPU:-any}]"
cd "$HERE"
GRAN=set LINE="$LINE_ARG" THRESH="$THRESH" TRUTH_DIR=truth TRACE_DIR=trace "${RUN[@]}"
echo; echo "prodotti: truth/H0 truth/H1  trace/H0 trace/H1  trace/meta"
echo "ora:  ./solve  &&  diff truth/H0 recov/H0 ; diff truth/H1 recov/H1"
