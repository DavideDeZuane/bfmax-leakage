#!/usr/bin/env bash
# run.sh - Flush+Reload oracle: builds the victim and runs it, writing the trace.
set -euo pipefail

# ---------------------------------------------------------------------------
# help
# ---------------------------------------------------------------------------
usage() {
cat <<'EOF'
run.sh — Flush+Reload oracle. Compiles the bfmax victim, runs one decoding, and
writes the per-block trace that the key-recovery pipeline consumes.

USAGE
  ./run.sh [INSTANCE] [SEED] [LINE]
  ./run.sh -h | --help

POSITIONAL ARGUMENTS
  INSTANCE   toy | real      problem size                         (default: toy)
                               toy  = P=4801,  V=45   (fast, for debugging)
                               real = P=12323, V=71   (BIKE-like)
  SEED       integer         fixes key + error, reproducible run   (default: 1)
  LINE       bytes (pow2)    cache-line granularity; 64 = real cache line
                             (8/16/32 = finer, fewer collisions)   (default: 64)

ENVIRONMENT VARIABLES
  CPU        core id         pin the victim to this core (taskset) (default: any)
  THRESH     cycles          hit/miss timing threshold             (default: 130)
  PERFECT    1               write ground-truth instead of timing  (no measurement;
                             gives the detection-perfect ceiling)  (default: off)
  CC         compiler        C compiler                            (default: cc)
  MARCH      arch            -march value (needs AVX2)             (default: x86-64-v3)

OUTPUT (created in this directory)
  truth/H0  truth/H1         true key support per block (bit positions)
  trace/H0  trace/H1         observed touched cache-line sets, one row per iteration
  trace/pos_H0 trace/pos_H1     argmax position per iteration (auxiliary channel)
  trace/meta                 "P V N0 line gran"  (read by the recovery tools)

PREREQUISITE (host, once per session)
  ./setup-bench.sh <CPU>     prefetcher off, performance governor, C-states off,
                             hugepages, core online — for stable measurements.

EXAMPLES
  ./run.sh                           # toy, seed 1, measured, any core
  CPU=11 ./run.sh real 7             # real, seed 7, pinned to core 11
  PERFECT=1 ./run.sh real 1          # real, detection-perfect ceiling (no F+R)
  THRESH=120 CPU=11 ./run.sh real 1  # custom hit/miss threshold

NEXT STEPS (after this writes trace/)
  SEL=pos ./trace2support trace recov    # pos candidates   -> recov/
  ./trace2support trace suppt            # line recovery    -> suppt/
  POS=1 python3 keyrecover.py            # exact key (uses pos)
  POS=0 python3 keyrecover.py            # fails without pos (demonstration)
EOF
}

case "${1:-}" in -h|--help|help) usage; exit 0 ;; esac

# ---------------------------------------------------------------------------
# arguments
# ---------------------------------------------------------------------------
INST="${1:-toy}"; SEED="${2:-1}"; LINE_ARG="${3:-64}"

# paths
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
# compilation
CC="${CC:-cc}"; MARCH="${MARCH:-x86-64-v3}"; THRESH="${THRESH:-130}"; CPU="${CPU:-}"
PERFECT="${PERFECT:-}"
# includes
INC=(-I"$ROOT/include" -I"$ROOT/src/core" -I"$ROOT/harness/common" -I"$ROOT/src/decoders/plain")

case "$INST" in
  toy)  DTOY=(-DTOY); BUILD="$HERE/build/fr_toy" ;;
  real) DTOY=();      BUILD="$HERE/build/fr_real" ;;
  *) echo "invalid instance '$INST': use toy|real (see ./run.sh --help)" >&2; exit 1 ;;
esac

mkdir -p "$BUILD" "$HERE/truth" "$HERE/trace"

echo "-----------------------------------"
echo ">> compiling attack.c [$INST]"
echo "-----------------------------------"
"$CC" -O2 -march="$MARCH" "${DTOY[@]}" "${INC[@]}" -include "$ROOT/src/decoders/plain/decoder.c" -c "$HERE/attack.c" -o "$BUILD/attack.o"
"$CC" -O2 -march="$MARCH" "${DTOY[@]}" "${INC[@]}" -c "$ROOT/harness/common/instance.c" -o "$BUILD/instance.o"
"$CC" "$BUILD/attack.o" "$BUILD/instance.o" -o "$BUILD/attack" -lm

RUN=("$BUILD/attack" "$SEED"); [ -n "$CPU" ] && RUN=(taskset -c "$CPU" "${RUN[@]}")

echo ">> exec [$INST seed=$SEED line=$LINE_ARG thresh=$THRESH cpu=${CPU:-any} perfect=${PERFECT:-0}]"
cd "$HERE"
GRAN=set LINE="$LINE_ARG" THRESH="$THRESH" PERFECT="$PERFECT" TRUTH_DIR=truth TRACE_DIR=trace "${RUN[@]}"

echo; echo "results: truth/H0 truth/H1  trace/H0 trace/H1  trace/pos_H0 trace/pos_H1  trace/meta"
echo "next:    SEL=pos ./trace2support trace recov  &&  POS=1 python3 keyrecover.py"
