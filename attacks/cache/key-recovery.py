#!/usr/bin/env python3
import sys
import numpy as np

def read_trace(f):
    dom = None
    iters = []
    for line in f:
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            if line.startswith("# D"):
                dom = int(line.split()[2])
            continue
        iters.append([int(x) for x in line.split()])
    return dom, iters

# This function generates the matrix with a row for number of iteration and a one in the position touched 
# M =  [
#           [0,0,1,1,0,0,0,1],      # riga 0: toccate 2,3,7
#           [1,0,0,0,1,0,0,0],      # riga 1: toccate 0,4
#           [0,0,0,1,0,0,0,1]       # riga 2: touched 3,7
#      ]    
def indicators(iters, dom):
    M = np.zeros((len(iters), dom), dtype=np.float64)
    for t, s in enumerate(iters):
        M[t, s] = 1.0
    return M

def best_shift(fa, b_vec):
    """Shift s e picco della cross-correlazione circolare di a (via FFT fa) con b."""
    corr = np.fft.irfft(fa * np.conj(np.fft.rfft(b_vec)), n=len(b_vec))
    s = int(np.argmax(corr))
    return s, corr[s]

def recover_block(members, M, F, dom, ref):
    """Allinea tutte le iterazioni del blocco a 'ref' e vota il supporto."""
    votes = np.zeros(dom)
    for j in members:
        s, _ = best_shift(F[ref], M[j])      # M[j] = shift di M[ref] di s
        votes += np.roll(M[j], s)            # riporta all'allineamento di ref
    return votes

def support_up_to_shift(votes, weight):
    """Le 'weight' posizioni piu' votate = supporto recuperato (traslato)."""
    return set(np.argsort(votes)[-weight:].tolist())

def score_up_to_cyclic_shift(rec, true_set, dom):
    """Massimo overlap tra rec e true_set su tutti gli shift ciclici."""
    a = np.zeros(dom); a[list(rec)] = 1
    b = np.zeros(dom); b[list(true_set)] = 1
    corr = np.fft.irfft(np.fft.rfft(a) * np.conj(np.fft.rfft(b)), n=dom)
    s = int(np.argmax(corr))
    return int(round(corr[s])), s

def main():
    truth_file = sys.argv[1]
    dom, iters = read_trace(sys.stdin)
    with open(truth_file) as tf:
        hdr = tf.readline().split()
        P = int(hdr[1]); V = int(hdr[3]); N0 = int(hdr[5]); gran = hdr[7]
        true_rows = [set(int(x) for x in tf.readline().split()) for _ in range(N0)]
    if gran == "set":
        true_rows = [set(p >> 6 for p in r) for r in true_rows]

    n = len(iters)
    print(f"traccia: {n} iterazioni, dominio {dom} ({gran}-granularity), "
          f"peso per riga {V}\n")

    M = indicators(iters, dom)
    F = np.fft.rfft(M, axis=1)

    # peso effettivo dei supporti osservati (con GRAN=set collisioni -> < V)
    weights = M.sum(axis=1)
    w_typ = int(round(np.median(weights)))

    # --- Fase 1: clustering nei 2 blocchi ---------------------------------
    # riferimento del blocco A = iterazione 0; picco vs ref separa A da B
    peaks = np.array([best_shift(F[0], M[j])[1] for j in range(n)])
    thr = w_typ * 0.6                    # netto: same-block ~w_typ, cross ~0
    A = [j for j in range(n) if peaks[j] >= thr]
    B = [j for j in range(n) if peaks[j] < thr]
    print(f"clustering: blocco A = {len(A)} iterazioni, blocco B = {len(B)} "
          f"(soglia picco {thr:.0f}; picchi A~{peaks[A].mean():.0f}, "
          f"B~{peaks[B].mean():.1f})" if B else
          f"clustering: tutte le {len(A)} iterazioni in un blocco solo")

    groups = [g for g in (A, B) if g]

    # --- Fase 2: allineamento + voto per ogni blocco ----------------------
    recovered = []
    for gi, g in enumerate(groups):
        ref = g[0]
        votes = recover_block(g, M, F, dom, ref)
        rec = support_up_to_shift(votes, V if gran == "bit" else w_typ)
        recovered.append(rec)

    # --- Scoring: ogni blocco recuperato vs le righe vere, a meno di shift -
    print()
    used = set()
    total_ok = 0
    for gi, rec in enumerate(recovered):
        best = (-1, -1, -1)
        for bi, tr in enumerate(true_rows):
            if bi in used:
                continue
            ov, s = score_up_to_cyclic_shift(rec, tr, dom)
            if ov > best[0]:
                best = (ov, bi, s)
        ov, bi, s = best
        used.add(bi)
        total_ok += ov
        print(f"blocco recuperato #{gi}  ->  Htr[{bi}]  "
              f"(shift {s}):  {ov}/{len(true_rows[bi])} posizioni corrette "
              f"({100*ov/len(true_rows[bi]):.1f}%)")

    denom = sum(len(r) for r in true_rows[:len(recovered)])
    print(f"\ntotale: {total_ok}/{denom} posizioni della chiave recuperate "
          f"da UNA sola traccia ({100*total_ok/denom:.1f}%)")

if __name__ == "__main__":
    main()

