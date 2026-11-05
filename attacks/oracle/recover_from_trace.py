#!/usr/bin/env python3
"""
Recupero del supporto (a livello di blocco) da UNA sola traccia, versione
migliorata per chiavi effimere.  Due cambiamenti rispetto all'originale:

 1) CLUSTERING ITERATIVO invariante per shift (k-means ciclico) invece del
    one-shot a 2 riferimenti: piu' robusto, i blocchi veri salgono di rango.

 2) OUTPUT GENEROSO: non taglia al peso esatto k, ma stampa i top-N blocchi
    piu' votati per circolante (default N grande).  Il taglio fine lo fa dopo
    l'algebra lineare (recover_bike_key.py), che tollera blocchi falsi finche'
    il sistema resta sovradeterminato (|U|<~96 blocchi) e INVECE ha bisogno
    che i blocchi veri siano presenti.  Alto recall > alta precisione.

Uso:  python3 recover_from_trace2.py truth.txt [topN] < trace.txt
"""
import sys, math
import numpy as np

def read_trace(f):
    dom = None; iters = []
    for line in f:
        line = line.strip()
        if not line: continue
        if line.startswith("#"):
            if line.startswith("# D"): dom = int(line.split()[2])
            continue
        iters.append([int(x) for x in line.split()])
    return dom, iters

def indicators(iters, dom):
    M = np.zeros((len(iters), dom))
    for t, s in enumerate(iters):
        M[t, s] = 1.0
    return M

def best_shift_corr(Fref, b_vec):
    """shift s e valore che allineano b_vec al template il cui rfft e' Fref."""
    corr = np.fft.irfft(Fref * np.conj(np.fft.rfft(b_vec)), n=len(b_vec))
    s = int(np.argmax(corr))
    return s, corr[s]

def vote_template(members, M, dom, ref_vec):
    """allinea ogni membro a ref_vec (per shift) e somma: template dei voti."""
    Fref = np.fft.rfft(ref_vec)
    votes = np.zeros(dom)
    for j in members:
        s, _ = best_shift_corr(Fref, M[j])
        votes += np.roll(M[j], s)
    return votes

def iterative_cluster(M, dom, n_clusters=2, n_iter=25, seed=0):
    """
    k-means ciclico: distanza = max correlazione su tutti gli shift.
    Init: riferimento 0 e il piu' lontano da esso (come l'originale), poi itera.
    Ritorna (assegnazioni, templates).
    """
    n = len(M)
    # init a 2 riferimenti (robusto per N0=2)
    F0 = np.fft.rfft(M[0])
    p0 = np.array([best_shift_corr(F0, M[j])[1] for j in range(n)])
    ref_b = int(np.argmin(p0))
    templates = [M[0].copy(), M[ref_b].copy()][:n_clusters]
    assign = np.zeros(n, dtype=int)
    for _ in range(n_iter):
        Ft = [np.fft.rfft(t) for t in templates]
        # assegna ogni iterazione al template con cui correla meglio
        new = np.empty(n, dtype=int)
        for j in range(n):
            scores = [best_shift_corr(Ft[c], M[j])[1] for c in range(n_clusters)]
            new[j] = int(np.argmax(scores))
        # ricostruisci i template come voto allineato dentro ogni cluster
        new_templates = []
        for c in range(n_clusters):
            members = [j for j in range(n) if new[j] == c]
            if not members:                      # cluster vuoto: reinizializza
                new_templates.append(M[np.random.randint(n)].copy()); continue
            ref = members[0]
            new_templates.append(vote_template(members, M, dom, M[ref]))
        templates = new_templates
        if np.array_equal(new, assign):
            assign = new; break
        assign = new
    return assign, templates

def main():
    truth_file = sys.argv[1]
    topN = int(sys.argv[2]) if len(sys.argv) > 2 else 90

    dom, iters = read_trace(sys.stdin)
    with open(truth_file) as tf:
        hdr = tf.readline().split()
        P = int(hdr[1]); V = int(hdr[3]); N0 = int(hdr[5]); gran = hdr[7]
        line_bytes = int(hdr[9]) if len(hdr) > 9 and hdr[8] == "line" else 64
        true_rows = [set(int(x) for x in tf.readline().split()) for _ in range(N0)]

    exp_dom = math.ceil(P / line_bytes) if gran == "set" else P
    if dom != exp_dom:
        sys.exit(f"trace/truth non combaciano: dom={dom} atteso {exp_dom}")
    if gran == "set":
        sh = line_bytes.bit_length() - 1
        true_rows = [set(p >> sh for p in r) for r in true_rows]

    n = len(iters)
    M = indicators(iters, dom)
    w_typ = int(round(np.median(M.sum(axis=1))))
    print(f"traccia: {n} iterazioni, dominio {dom}, peso/riga {w_typ} (V={V}), topN={topN}\n")

    assign, templates = iterative_cluster(M, dom, n_clusters=N0)
    for c in range(N0):
        print(f"cluster {c}: {int((assign==c).sum())} iterazioni")

    # voto finale per cluster e ranking completo dei blocchi
    ranked = []
    for c in range(N0):
        members = [j for j in range(n) if assign[j] == c]
        ref = members[0]
        votes = vote_template(members, M, dom, M[ref])
        order = np.argsort(votes)[::-1]          # blocchi dal piu' al meno votato
        ranked.append((votes, order))

    # scoring vs verita' (a meno di shift ciclico) e stampa dei top-N
    def align_shift(order_set, tr):
        a = np.zeros(dom); a[list(order_set)] = 1
        b = np.zeros(dom); b[list(tr)] = 1
        corr = np.fft.irfft(np.fft.rfft(a) * np.conj(np.fft.rfft(b)), n=dom)
        s = int(np.argmax(corr)); return s, int(round(corr[s]))

    used = set()
    out_aligned, out_raw = {}, {}
    for c in range(N0):
        votes, order = ranked[c]
        topset = set(order[:topN].tolist())
        # abbina alla circolante vera e misura recall sui top-N
        best = (-1, -1, -1)
        for bi, tr in enumerate(true_rows):
            if bi in used: continue
            s, ov = align_shift(topset, tr)
            if ov > best[1]: best = (bi, ov, s)
        bi, ov, s = best; used.add(bi)
        aligned = sorted((int(p) - s) % dom for p in order[:topN])
        out_aligned[bi] = aligned
        out_raw[bi] = sorted(int(p) for p in order[:topN])
        print(f"  cluster {c} -> Htr[{bi}]: top-{topN} contiene "
              f"{ov}/{len(true_rows[bi])} blocchi veri "
              f"({100*ov/len(true_rows[bi]):.1f}% recall)")

    print("\n" + "=" * 60)
    print(f"TOP-{topN} BLOCCHI per circolante (da passare al recupero lineare)")
    print("=" * 60)
    for bi in sorted(out_aligned):
        print(f"\n# circolante {bi} - frame VERITA' (top-{topN}):")
        print(" ".join(map(str, out_aligned[bi])))
    print("\n--- grezzo (frame del voto) ---")
    for bi in sorted(out_raw):
        print(f"# circolante {bi} - grezzo (top-{topN}):")
        print(" ".join(map(str, out_raw[bi])))

if __name__ == "__main__":
    main()
