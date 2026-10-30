#!/usr/bin/env python3
import sys
import math
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

# Matrice indicatrice: una riga per iterazione, un 1 nelle posizioni/linee toccate.
def indicators(iters, dom):
    M = np.zeros((len(iters), dom), dtype=np.float64)
    for t, s in enumerate(iters):
        M[t, s] = 1.0
    return M

def best_shift(fa, b_vec):
    """Shift ciclico s e overlap che allineano b_vec su a (a di cui fa=rfft)."""
    corr = np.fft.irfft(fa * np.conj(np.fft.rfft(b_vec)), n=len(b_vec))
    s = int(np.argmax(corr))
    return s, corr[s]

def recover_block(members, M, F, dom, ref):
    """Allinea tutte le iterazioni del blocco a 'ref' e vota il supporto."""
    votes = np.zeros(dom)
    for j in members:
        s, _ = best_shift(F[ref], M[j])      # porta M[j] nel frame di ref
        votes += np.roll(M[j], s)
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
        # header: "P .. V .. N0 .. gran .. line <bytes> iters .."
        # retrocompat: se manca 'line', assume 64 (vecchie truth.txt).
        line_bytes = int(hdr[9]) if len(hdr) > 9 and hdr[8] == "line" else 64
        true_rows = [set(int(x) for x in tf.readline().split()) for _ in range(N0)]

    # --- coerenza trace/truth: devono venire dallo stesso LINE ------------
    exp_dom = math.ceil(P / line_bytes) if gran == "set" else P
    if dom != exp_dom:
        sys.exit(f"trace/truth non combaciano: trace dom={dom}, "
                 f"truth P={P} line={line_bytes}B -> atteso dom={exp_dom}. "
                 f"Rigenera la coppia con lo stesso LINE.")

    # riduce le posizioni vere alla granularita' della trace: pos >> log2(line)
    if gran == "set":
        shift = line_bytes.bit_length() - 1          # log2(line_bytes)
        true_rows = [set(p >> shift for p in r) for r in true_rows]

    n = len(iters)
    M = indicators(iters, dom)
    F = np.fft.rfft(M, axis=1)

    weights = M.sum(axis=1)
    w_typ = int(round(np.median(weights)))

    print(f"traccia: {n} iterazioni, dominio {dom} ({gran}-granularity, "
          f"line={line_bytes}B), peso osservato per riga {w_typ} (V={V})\n")

    # --- Fase 1: clustering a DUE riferimenti -----------------------------
    # Niente soglia fissa: ref_a = iter 0, ref_b = l'iterazione che correla
    # PEGGIO con ref_a (quasi certamente l'altro blocco). Ogni iterazione va
    # al riferimento con cui ha l'overlap maggiore. Robusto anche a line=64.
    ref_a = 0
    pa = np.array([best_shift(F[ref_a], M[j])[1] for j in range(n)])
    ref_b = int(np.argmin(pa))
    pb = np.array([best_shift(F[ref_b], M[j])[1] for j in range(n)])

    A = [j for j in range(n) if pa[j] >= pb[j]]
    B = [j for j in range(n) if pa[j] <  pb[j]]

    print(f"clustering: blocco A = {len(A)} iterazioni (ref {ref_a}), "
          f"blocco B = {len(B)} (ref {ref_b})")
    print(f"  picchi vs ref_a: istogramma {np.histogram(pa, bins=8)[0].tolist()}")
    print(f"  overlap medio nel proprio gruppo: "
          f"A~{pa[A].mean():.1f}, B~{pb[B].mean():.1f}\n")

    groups = [g for g in (A, B) if g]

    # --- Fase 2: allineamento + voto per ogni blocco ----------------------
    # bit -> V posizioni; line-space -> w_typ linee (il blocco occupa ~w_typ linee)
    k = V if gran == "bit" else w_typ
    recovered = []
    for g in groups:
        votes = recover_block(g, M, F, dom, g[0])
        recovered.append(support_up_to_shift(votes, k))

    # --- Scoring: blocco recuperato vs righe vere, a meno di shift ciclico -
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
        print(f"blocco recuperato #{gi}  ->  Htr[{bi}]  (shift {s}):  "
              f"{ov}/{len(true_rows[bi])} posizioni corrette "
              f"({100*ov/len(true_rows[bi]):.1f}%)")

    denom = sum(len(true_rows[bi]) for bi in used)
    print(f"\ntotale: {total_ok}/{denom} posizioni della chiave recuperate "
          f"da UNA sola traccia ({100*total_ok/denom:.1f}%)")

if __name__ == "__main__":
    main()