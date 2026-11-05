
import sys

# ---------------------------------------------------------------- lettura ----
def read_trace(f):
    dom, iters = None, []
    for line in f:
        line = line.strip()
        if line.startswith("# D"):
            dom = int(line.split()[2])
        elif line and not line.startswith("#"):
            iters.append(set(int(x) for x in line.split()))   # ogni iterazione = un INSIEME
    return dom, iters

def shift_set(S, s, D):
    """insieme S traslato a destra di s (b -> b+s)"""
    return set((x + s) % D for x in S)


def best_shift_overlap(A, B, D):
    overlaps = [0] * D
    for a in A:
        for b in B:
            overlaps[(a - b) % D] += 1
    best_val, best_shift = 0, 0
    for s in range(D):
        if overlaps[s] > best_val:
            best_val, best_shift = overlaps[s], s
    return best_shift, best_val

def main():
    truth_file = sys.argv[1]
    # domain (which depends on the granularity) and number of iterations
    D, iters = read_trace(sys.stdin)
    
    # truth file contains:
    #  - keys to compare at the end
    #  - parameters of the code and other 
    with open(truth_file) as tf:
        h = tf.readline().split(); 
        p = int(h[1]); 
        v = int(h[3]); 
        N0 = int(h[5]); 
        gran = h[7];
        t = h[9]
        # this is the secret key
        true_rows = [set(int(x) for x in tf.readline().split()) for _ in range(N0)]

    print(f"Code parameters: ({N0*p}, {p}, {t})")

    ref0 = iters[0]
    same = []
    other = []
    shift = {0: 0}


    thr = 0.6 * v

    for j in range(1,134):
        overlaps = [0] * p
        for a in ref0:
            for b in iters[j]:
                overlaps[(a - b) % p] += 1

        best_val = 0
        best_shift = 0
        # scan all the possible shift and find the
        # - the entity of the shift 
        # - the number of overlaps
        for s in range(D):                    
            if overlaps[s] > best_val:
                best_val   = overlaps[s] 
                best_shift = s

    w   = max(len(s) for s in iters)
    thr = 0.6 * w
    same, other = [0], []                       # ref0 nel suo gruppo
    for j in range(1, len(iters)):
        _, val = best_shift_overlap(ref0, iters[j], D)
        (same if val > thr else other).append(j)

    
    recovered = []
    for g in (same, other):
        if not g: continue
        ref = iters[g[0]]
        votes = {}
        for j in g:
            s, _ = best_shift_overlap(ref, iters[j], D)   # <-- vs il ref DEL GRUPPO, non ref0
            for x in shift_set(iters[j], s, D):
                votes[x] = votes.get(x, 0) + 1
        
        top = sorted(votes, key=votes.get, reverse=True)[:v]
        recovered.append(set(top))

    print("confronto con la chiave vera (a meno di shift ciclico):")
    used, tot = set(), 0
    for gi, rec in enumerate(recovered):
        best = (-1, -1, -1)
        for bi, tr in enumerate(true_rows):
            if bi in used: continue
            # overlap massimo su tutti gli shift tra rec e la riga vera
            o = max(len(rec & shift_set(tr, s, D)) for s in range(D))
            if o > best[0]: best = (o, bi, 0)
        o, bi, _ = best; used.add(bi); tot += o
        print(f"  blocco #{gi} -> Htr[{bi}]: {o}/{len(true_rows[bi])} posizioni ({100*o/len(true_rows[bi]):.0f}%)")
    denom = sum(len(r) for r in true_rows[:len(recovered)])
    print(f"\ntotale: {tot}/{denom} posizioni recuperate da UNA traccia ({100*tot/denom:.0f}%)")





if __name__=="__main__":
    main()
