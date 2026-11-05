#!/usr/bin/env bash
# setup-bench.sh - prepara l'HOST Proxmox per misure di cache stabili.
# Uso:  ./setup-bench.sh <core1> [core2 ...]
#   es: ./setup-bench.sh 11            (milestone 1, un core)
#       ./setup-bench.sh 1 11          (milestone 2, coppia sibling SMT)
# Lancialo SULL'HOST come root. Idempotente: rilancialo a ogni boot/sessione.
set -euo pipefail
[ "$#" -ge 1 ] || { echo "uso: $0 <core> [core...]"; exit 1; }
CORES=("$@")

echo ">> governor performance + no turbo"
if command -v cpupower >/dev/null; then cpupower frequency-set -g performance >/dev/null; fi
for c in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > "$c" 2>/dev/null || true; done
[ -w /sys/devices/system/cpu/intel_pstate/no_turbo ] && echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo || true

echo ">> C-states profondi OFF"
command -v cpupower >/dev/null && cpupower idle-set -D 0 >/dev/null 2>&1 || true

echo ">> SMT on (serve per milestone 2)"
[ -w /sys/devices/system/cpu/smt/control ] && echo on > /sys/devices/system/cpu/smt/control 2>/dev/null || true

echo ">> msr module"
modprobe msr 2>/dev/null || true

echo ">> prefetcher OFF + core online, sui core richiesti: ${CORES[*]}"
for cpu in "${CORES[@]}"; do
  [ -w "/sys/devices/system/cpu/cpu$cpu/online" ] && echo 1 > "/sys/devices/system/cpu/cpu$cpu/online" 2>/dev/null || true
  wrmsr -p "$cpu" 0x1a4 0xf 2>/dev/null || echo "   (wrmsr su cpu$cpu fallito: core offline?)"
done

echo ">> hugepages (512 x 2MB = 1GB)"
echo 512 > /proc/sys/vm/nr_hugepages 2>/dev/null || true

echo; echo ">> verifica:"
for cpu in "${CORES[@]}"; do
  pf=$(rdmsr -p "$cpu" 0x1a4 2>/dev/null || echo "?")
  mhz=$(awk -v c="$cpu" 'NR>1{} /processor/{p=$3} /MHz/{if(p==c)print $4}' /proc/cpuinfo 2>/dev/null | head -1)
  echo "   cpu$cpu: prefetcher(0x1a4)=$pf  MHz~$mhz"
done
echo "   turbo_off=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null||echo ?)  hugepages=$(cat /proc/sys/vm/nr_hugepages 2>/dev/null)"
echo ">> fatto. (prefetcher deve dire f)"
