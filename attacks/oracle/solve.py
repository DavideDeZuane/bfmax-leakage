#!/usr/bin/env python3
"""
Stadio finale: dati la chiave pubblica h (BIKE) e, per ciascuna circolante, un
INSIEME DI BLOCCHI CANDIDATI da 64 bit (output di recover_from_trace2.py, scelto
generoso -> alto recall), recupera una chiave segreta valida risolvendo il
sistema lineare  (h * h0)_i = 0  per i fuori dai blocchi di h1.

Qualunque rotazione ciclica (x^i h0, x^i h1) e' una chiave segreta equivalente
per un codice quasi-ciclico: il successo e' "peso(h0)=peso(h1)=w/2 e h1=h*h0".

Uso tipico (demo): si ricostruisce h dal segreto noto; in un attacco reale h e'
la chiave pubblica e si passa quella.
"""
import time

def ring_mul(a, b, r):
    sa, sb = bin(a).count("1"), bin(b).count("1")
    if sa > sb: a, b = b, a
    mask = (1 << r) - 1; out = 0; j = 0; aa = a
    while aa:
        if aa & 1: out ^= ((b << j) | (b >> (r - j))) & mask
        aa >>= 1; j += 1
    return out & mask

def ring_inverse(a, r):
    def deg(x): return x.bit_length() - 1
    old, new = (1 << r) | 1, a; oa, na = 0, 1
    while new:
        if deg(old) < deg(new):
            old, new = new, old; oa, na = na, oa; continue
        sh = deg(old) - deg(new); old ^= new << sh; oa ^= na << sh
        if oa.bit_length() > r:
            oa = (oa & ((1 << r) - 1)) ^ (oa >> r)
    assert old == 1, "non invertibile"
    return oa & ((1 << r) - 1)

def gf2_nullspace(rows_bits, ncols):
    pivots = {}
    for r in rows_bits:
        cur = r
        for pc, prow in pivots.items():
            if (cur >> pc) & 1: cur ^= prow
        if not cur: continue
        c = (cur & -cur).bit_length() - 1
        for pc in list(pivots):
            if (pivots[pc] >> c) & 1: pivots[pc] ^= cur
        pivots[c] = cur
    pivot_cols = set(pivots)
    free = [c for c in range(ncols) if c not in pivot_cols]
    basis = []
    for fc in free:
        x = 1 << fc
        for pc, prow in pivots.items():
            if (prow >> fc) & 1: x ^= (1 << pc)
        basis.append(x)
    return basis

def recover(h, R, W2, blocks0, blocks1, B=64, verbose=True):
    full = (1 << R) - 1
    def cols(bs):
        c = []
        for b in bs: c += list(range(b * B, min(b * B + B, R)))
        return c
    U0 = cols(sorted(set(blocks0))); U1 = set(cols(sorted(set(blocks1))))
    rows_zero = [i for i in range(R) if i not in U1]
    if verbose:
        print(f"sistema: {len(rows_zero)} equazioni x {len(U0)} incognite "
              f"(sovradet {len(rows_zero) - len(U0)})")
    t = time.time()
    colpos = {c: i for i, c in enumerate(U0)}
    rowidx = {i: k for k, i in enumerate(rows_zero)}
    M = [0] * len(rows_zero)
    for j in U0:
        sh = ((h << j) | (h >> (R - j))) & full; s = sh; ci = colpos[j]
        while s:
            i = (s & -s).bit_length() - 1; ri = rowidx.get(i)
            if ri is not None: M[ri] |= (1 << ci)
            s &= s - 1
    tb = time.time() - t
    t = time.time(); basis = gf2_nullspace(M, len(U0)); ts = time.time() - t
    d = len(basis)
    if verbose: print(f"build {tb:.1f}s, Gauss {ts:.1f}s, dim nucleo {d}")
    # cerca una chiave valida fra i vettori del nucleo (combinazioni se d piccolo)
    def to_poly(x):
        cand = 0; s = x
        while s:
            ci = (s & -s).bit_length() - 1; cand |= (1 << U0[ci]); s &= s - 1
        return cand
    seqs = range(1, 1 << d) if d <= 24 else (1 << i for i in range(d))
    for bits in seqs:
        x = 0; bb = bits; idx = 0
        while bb:
            if bb & 1: x ^= basis[idx]
            bb >>= 1; idx += 1
        h0c = to_poly(x)
        if bin(h0c).count("1") != W2: continue
        h1c = ring_mul(h, h0c, R)
        if bin(h1c).count("1") == W2:
            return h0c, h1c, d
    return None, None, d

# ----------------------------- demo sulla chiave data -----------------------
if __name__ == "__main__":
    R, W2 = 12323, 71
    H0 = [12189,11951,11860,11844,11242,11126,11075,11037,10692,10282,10270,9951,9695,
9623,9589,9363,9059,8850,8676,8189,8100,8064,7860,7440,7308,7217,6591,6428,6427,5779,
5653,5618,5586,5503,5499,5051,4769,4505,4432,4420,4401,4374,4264,4258,3897,3603,3458,
3249,3069,2812,2387,2237,2202,2170,2072,1978,1797,1523,1452,1450,1410,1247,1233,1150,
1114,900,832,253,165,130,73]
    H1 = [12106,11933,11919,11735,11569,11333,11279,10885,10620,10603,10465,10292,10048,
10031,9789,9781,9457,9382,9100,8029,8026,7892,7779,7532,7456,7445,7325,7215,7073,6943,
6935,6819,6779,6686,6384,6336,5584,4557,4531,4382,4314,3950,3904,3849,3707,3677,3667,
3644,3604,3357,3178,3140,3027,2955,2889,2693,2608,2479,2471,2467,2358,2296,2282,2019,
1691,1368,1186,923,682,397,334]
    h0 = 0
    for p in H0: h0 |= (1 << p)
    h1 = 0
    for p in H1: h1 |= (1 << p)
    h = ring_mul(h1, ring_inverse(h0, R), R)   # chiave pubblica (nota all'attaccante)

    # blocchi top-90 (dal recupero traccia)
    B0 = [0,1,2,3,12,13,14,16,17,18,19,21,22,23,27,28,30,31,32,33,34,35,36,37,43,44,47,48,
50,53,54,55,56,60,61,65,66,67,68,69,70,73,74,78,79,85,86,87,88,89,90,99,100,102,103,112,
113,114,115,116,122,123,125,126,127,128,135,137,138,141,145,146,149,150,151,155,160,161,
166,167,172,173,174,175,176,184,185,186,187,190]
    B1 = [4,5,6,10,13,14,17,18,20,21,25,26,30,31,35,36,37,38,40,41,42,44,45,46,47,48,49,51,
52,55,56,57,58,59,60,61,62,66,67,68,70,71,86,87,98,99,100,104,105,106,107,108,110,112,113,
114,115,116,117,118,121,122,123,125,141,142,146,147,148,152,153,156,157,160,161,163,164,
165,166,169,170,176,177,180,181,183,186,187,188,189]

    t0 = time.time()
    h0r, h1r, d = recover(h, R, W2, B0, B1)
    if h0r is None:
        print("non recuperata: alza topN nel recupero traccia"); raise SystemExit
    # verifica chiave valida ed equivalenza (a meno di rotazione)
    assert ring_mul(h, h0r, R) == h1r and bin(h1r).count("1") == W2
    full = (1 << R) - 1
    delta = next((dl for dl in range(R)
                  if ((h0 << dl) | (h0 >> (R - dl))) & full == h0r), None)
    pos0 = sorted((i for i in range(R) if (h0r >> i) & 1), reverse=True)
    pos1 = sorted((i for i in range(R) if (h1r >> i) & 1), reverse=True)
    print("\n==================  CHIAVE RECUPERATA  ==================")
    print(f"peso(h0)={bin(h0r).count('1')}  peso(h1)={bin(h1r).count('1')}  h1 == h*h0: True")
    print(f"equivalente alla chiave vera: {'ESATTA' if delta==0 else f'rotazione x^{delta}'} "
          f"(per un QC-code e' la stessa chiave)")
    print(f"tempo totale attacco: {time.time()-t0:.1f}s")
    print("h0 (posizioni):", pos0)
    print("h1 (posizioni):", pos1)
