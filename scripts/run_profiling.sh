#!/usr/bin/env bash
# =============================================================================
# profile_hotspot.sh - profiling del decoder con perf + hotspot, con setup
# dell'ambiente e gestione degli errori tipici:
#
#   - "failed to mmap ... perf_event_mlock_kb"  -> alza kernel.perf_event_mlock_kb
#     (o riprova con un buffer piu' piccolo)
#   - samples/symbol mancanti                    -> perf_event_paranoid / kptr_restrict
#   - hotspot: "formato non leggibile"           -> record senza compressione +
#                                                   --buildid-all, fallback a perf report
#
# Uso:
#   scripts/profile_hotspot.sh [VARIANT]
#   VARIANT=plain RUNS=3000 CPU=2 scripts/profile_hotspot.sh
#
# Variabili d'ambiente (tutte opzionali):
#   VARIANT   versione del decoder (default: plain)
#   RUNS      decodifiche nel bench da profilare (default: 3000)
#   CPU       core su cui pinnare il processo (default: 0)
#   FREQ      frequenza di campionamento perf (default: max)
#   MMAP      buffer mmap di perf, es. 16M / 512K (default: 16M)
#   NO_SETUP  =1 salta la configurazione sysctl (se l'hai gia' fatta)
#   NO_OPEN   =1 registra soltanto, non apre hotspot
# =============================================================================
set -euo pipefail

# ---- posizionati nella root del repo (lo script sta in scripts/) ------------
cd "$(dirname "${BASH_SOURCE[0]}")/.."

VARIANT=${1:-${VARIANT:-plain}}
RUNS=${RUNS:-3000}
CPU=${CPU:-0}
FREQ=${FREQ:-max}
MMAP=${MMAP:-16M}
PERF=${PERF:-perf}
HOTSPOT=${HOTSPOT:-hotspot}
OUT="results/profile/${VARIANT}.perf.data"

# ---- sudo solo se serve -----------------------------------------------------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if command -v sudo >/dev/null 2>&1; then SUDO="sudo"; else
    echo "!! non sei root e manca sudo: salto il setup kernel (NO_SETUP=1)"; NO_SETUP=1
  fi
fi

# ---- mmap (es. 16M) -> KB, per dimensionare perf_event_mlock_kb -------------
mmap_to_kb() {
  local s=$1 num unit
  num=${s%[MmKk]}; unit=${s: -1}
  case "$unit" in
    M|m) echo $((num*1024)) ;;
    K|k) echo $((num)) ;;
    *)   echo $(((${s}+1023)/1024)) ;;   # bytes
  esac
}
MLOCK_KB=$(( $(mmap_to_kb "$MMAP") + 2048 ))   # buffer + margine

# =============================================================================
# 1) SETUP KERNEL (sysctl) - risolve gli errori di mmap/permessi di perf
# =============================================================================
if [ "${NO_SETUP:-0}" != "1" ]; then
  echo "== setup perf (sysctl) =="
  for kv in \
      "kernel.perf_event_paranoid=1" \
      "kernel.kptr_restrict=0" \
      "kernel.perf_event_mlock_kb=${MLOCK_KB}"; do
    key=${kv%%=*}
    old=$(cat /proc/sys/${key//.//} 2>/dev/null || echo "?")
    if $SUDO sysctl -w "$kv" >/dev/null 2>&1; then
      printf "   %-32s %s -> %s\n" "$key" "$old" "${kv#*=}"
    else
      printf "   %-32s NON impostato (serve sudo) - era %s\n" "$key" "$old"
    fi
  done
  echo "   (per ripristinare: sudo sysctl -w kernel.perf_event_paranoid=<vecchio> ...)"
fi

# =============================================================================
# 2) BUILD (PROFILE=profile: -O3 senza inline, simboli e frame pointer)
# =============================================================================
echo "== build VARIANT=$VARIANT PROFILE=profile =="
make --no-print-directory VARIANT="$VARIANT" PROFILE=profile TOOL=bench
BIN="build/${VARIANT}-profile/bench_decoder"
[ -x "$BIN" ] || { echo "!! binario non trovato: $BIN"; exit 1; }

# =============================================================================
# 3) RECORD - niente compressione (hotspot la legge male) + buildid completi.
#    Fallback automatico a un buffer mmap piu' piccolo se il mlock non basta.
# =============================================================================
mkdir -p results/profile
record() {   # $1 = mmap-pages
  $SUDO -E 2>/dev/null taskset -c "$CPU" "$PERF" record \
      -F "$FREQ" --call-graph dwarf,8192 --sample-cpu \
      --compression-level=0 --buildid-all \
      --mmap-pages "$1" -o "$OUT" -- "$BIN" "$RUNS"
}
echo "== perf record ($RUNS decodifiche, core $CPU, mmap $MMAP) =="
if ! record "$MMAP"; then
  echo "!! record con mmap=$MMAP fallito (mlock insufficiente?), riprovo con 512K"
  record 512K
fi
# perf.data e' di root se abbiamo usato sudo: rendilo leggibile all'utente
[ -n "$SUDO" ] && $SUDO chown "$(id -u):$(id -g)" "$OUT" 2>/dev/null || true
echo "   registrazione: $(readlink -f "$OUT")"

# =============================================================================
# 4) APRI HOTSPOT, con fallback a perf report se il formato non e' leggibile
# =============================================================================
[ "${NO_OPEN:-0}" = "1" ] && { echo "NO_OPEN=1: apri con  $HOTSPOT $(readlink -f "$OUT")"; exit 0; }

if command -v "$HOTSPOT" >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
  echo "== hotspot =="
  if ! "$HOTSPOT" "$(readlink -f "$OUT")"; then
    echo "!! hotspot non apre il file (versione perfparser/perf non allineate)."
    echo "   fallback: perf report (testuale)"
    "$PERF" report -i "$OUT" --stdio | head -40
    echo "   ... oppure:  perf report -i $(readlink -f "$OUT")"
  fi
else
  echo "nessun display grafico (o hotspot assente)."
  echo "apri la registrazione con:"
  echo "   $HOTSPOT $(readlink -f "$OUT")      # su una macchina con GUI"
  echo "   $PERF report -i $(readlink -f "$OUT")   # testuale, qui"
fi
