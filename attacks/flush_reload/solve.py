#!/usr/bin/env python3
"""solve - recupero chiave per-blocco da traccia Flush+Reload.

Legge <trace>/meta, <trace>/H0, <trace>/H1 e scrive <out>/H0, <out>/H1
(supporto recuperato, linee ordinate ascendenti). Il recupero e' a meno di
uno shift ciclico (equivalenza QC-MDPC): se <truth>/Hb esiste, la rotazione
globale viene fissata su di esso SOLO per rendere leggibile il diff; il
supporto recuperato (quali linee) e' indipendente da truth.

Uso:  ./solve [trace_dir=trace] [out_dir=recovered] [truth_dir=truth]
Poi:  diff truth/H0 recovered/H0   ;   diff truth/H1 recovered/H1
"""
import sys, os, math
import numpy as np

def read_sets(path):
    its = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            its.append([int(x) for x in line.split()] if line else [])
    return its

def read_lines(path):
    with open(path) as f:
        return sorted(int(x) for x in f.read().split())

def recover_block(iters, dom, k):
    """Allinea ogni iterazione alla #0 via cross-correlazione ciclica e vota.
       Ritorna le k linee piu' votate (supporto, a meno di shift)."""
    its = [s for s in iters if s]          # scarta righe vuote
    if not its:
        return set()
    M = np.zeros((len(its), dom))
    for t, s in enumerate(its):
        M[t, s] = 1.0
    F = np.fft.rfft(M, axis=1)
    fa = F[0]
    votes = np.zeros(dom)
    for j in range(len(its)):
        corr = np.fft.irfft(fa * np.conj(F[j]), n=dom)
        shift = int(np.argmax(corr))
        votes += np.roll(M[j], shift)
    return set(np.argsort(votes)[-k:].tolist())

def align_to_truth(rec, truth, dom):
    """Ruota 'rec' sulla rotazione che massimizza l'overlap con 'truth'."""
    if not truth:
        return sorted(rec), 0, 0
    rv = np.zeros(dom); rv[list(rec)] = 1
    tv = np.zeros(dom); tv[list(truth)] = 1
    corr = np.fft.irfft(np.fft.rfft(rv) * np.conj(np.fft.rfft(tv)), n=dom)
    r = int(np.argmax(corr))                # roll(rv, r) ~ tv
    aligned = sorted(((x + r) % dom) for x in rec)
    ov = len(set(aligned) & set(truth))
    return aligned, ov, r

def main():
    trace_dir = sys.argv[1] if len(sys.argv) > 1 else "trace"
    out_dir   = sys.argv[2] if len(sys.argv) > 2 else "recovered"
    truth_dir = sys.argv[3] if len(sys.argv) > 3 else "truth"
    os.makedirs(out_dir, exist_ok=True)

    P, V, N0, line, gran = open(os.path.join(trace_dir, "meta")).read().split()
    P, V, N0, line = int(P), int(V), int(N0), int(line)
    dom = math.ceil(P / line) if gran == "set" else P

    print(f"solve: P={P} V={V} N0={N0} line={line}B gran={gran} dom={dom}")
    for b in range(N0):
        iters = read_sets(os.path.join(trace_dir, f"H{b}"))
        nz = [s for s in iters if s]
        # il blocco occupa ~mediana(peso riga) linee (bit-gran: V posizioni)
        k = V if gran == "bit" else int(round(np.median([len(s) for s in nz]))) if nz else V
        rec = recover_block(iters, dom, k)

        tp = os.path.join(truth_dir, f"H{b}")
        truth = read_lines(tp) if os.path.exists(tp) else []
        aligned, ov, r = align_to_truth(rec, truth, dom)

        with open(os.path.join(out_dir, f"H{b}"), "w") as f:
            for x in aligned:
                f.write(f"{x}\n")

        if truth:
            print(f"  H{b}: {len(nz)} iter -> {len(rec)} linee recuperate, "
                  f"shift={r}, match {ov}/{len(truth)} ({100*ov/len(truth):.1f}%)")
        else:
            print(f"  H{b}: {len(nz)} iter -> {len(rec)} linee recuperate (no truth)")

if __name__ == "__main__":
    main()
