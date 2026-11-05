#!/usr/bin/env python3
"""
key-recovery.py - recupero a LINEE dalla traccia dell'oracolo teorico.

Legge la struttura a directory (trace/H{b}, truth/H{b}, trace/meta): per ogni
blocco allinea le iterazioni (cross-correlazione), vota, e misura recall/precision
rispetto alla chiave vera, a meno di shift ciclico. NESSUN clustering: i blocchi
sono gia' separati nei file per-blocco.

E' lo scorer a linee dell'oracolo teorico (il "trace2support" del lato teorico);
per il recupero ESATTO della chiave usa invece  SEL=pos ./trace2support + keyrecover.py.

Uso:  python3 key-recovery.py [trace_dir=trace] [truth_dir=truth]
Env:  K=<n>  linee emesse per blocco. Default = max peso-riga OSSERVATO (lato
             attaccante, non la verita'). Alto K -> recall alta, precision bassa.
"""
import sys, os, math
import numpy as np

def read_sets(path):
    return [[int(x) for x in l.split()] if l.strip() else [] for l in open(path)]
def read_ints(path):
    return [int(x) for x in open(path).read().split()]

def block_votes(iters, dom):
    """Allinea ogni iterazione alla #0 (cross-correlazione ciclica) e somma i voti."""
    its = [s for s in iters if s]
    if not its:
        return np.zeros(dom), []
    M = np.zeros((len(its), dom))
    for t, s in enumerate(its): M[t, s] = 1.0
    F = np.fft.rfft(M, axis=1); fa = F[0]
    v = np.zeros(dom)
    for j in range(len(its)):
        sh = int(np.argmax(np.fft.irfft(fa * np.conj(F[j]), n=dom)))
        v += np.roll(M[j], sh)
    return v, [len(s) for s in its]

def score_up_to_cyclic_shift(rec, truth, dom):
    """Overlap massimo tra rec e truth su tutti gli shift ciclici."""
    a = np.zeros(dom); a[list(rec)] = 1
    b = np.zeros(dom); b[list(truth)] = 1
    corr = np.fft.irfft(np.fft.rfft(a) * np.conj(np.fft.rfft(b)), n=dom)
    s = int(np.argmax(corr))
    return int(round(corr[s])), s

def main():
    trace_dir = sys.argv[1] if len(sys.argv) > 1 else "trace"
    truth_dir = sys.argv[2] if len(sys.argv) > 2 else "truth"
    P, V, N0, line, gran = open(os.path.join(trace_dir, "meta")).read().split()
    P, V, N0, line = int(P), int(V), int(N0), int(line)
    shift = line.bit_length() - 1 if gran == "set" else 0
    dom = math.ceil(P / line) if gran == "set" else P

    print(f"key-recovery (linee): P={P} V={V} N0={N0} line={line}B gran={gran} dom={dom}")
    print("  recall    = overlap/|vere|   precision = overlap/|recuperate|   (a meno di shift)\n")

    tot_tp = tot_truth = tot_rec = 0
    for b in range(N0):
        iters = read_sets(os.path.join(trace_dir, f"H{b}"))
        truth_bits = sorted(set(read_ints(os.path.join(truth_dir, f"H{b}"))))
        truth = sorted(set(p >> shift for p in truth_bits))   # linee (o bit se shift=0)
        votes, weights = block_votes(iters, dom)

        # k generoso lato attaccante: default = max linee viste in un'iterazione
        maxw = int(max(weights)) if weights else V
        k = int(os.environ["K"]) if "K" in os.environ else maxw
        k = max(1, min(k, dom))
        rec = set(np.argsort(votes)[-k:].tolist())

        ov, s = score_up_to_cyclic_shift(rec, truth, dom)
        nt, nr = len(truth), len(rec)
        tot_tp += ov; tot_truth += nt; tot_rec += nr
        print(f"  H{b}  (shift {s}):  recall {ov}/{nt} ({100*ov/nt:.1f}%)   "
              f"precision {ov}/{nr} ({100*ov/nr:.1f}%)   [k={k}]")

    print(f"\ntotale:  recall {tot_tp}/{tot_truth} ({100*tot_tp/tot_truth:.1f}%)   "
          f"precision {tot_tp}/{tot_rec} ({100*tot_tp/tot_rec:.1f}%)  "
          f"[una sola traccia, {gran}-granularity]")

if __name__ == "__main__":
    main()
