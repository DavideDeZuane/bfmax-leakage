#!/usr/bin/env python3
"""
====================================================================
Algebraic key recovery from cache-channel candidates.

  1) (data)   h = h1 * h0^{-1} mod (x^P+1)   -> public key
  2) (input)  candidate sets C0, C1
  3) (output) (h0, h1):  h0 from the linear system,  h1 = h0 * h

Candidate source (env POS):
  POS=1  (default)  bit-level candidates from  <recov>/candidates_H{0,1}
                    produced by  SEL=pos ./trace2support   (USES the argmax pos)
  POS=0             64-bit windows of the recovered LINES in <line>/H{0,1}
                    produced by  ./trace2support            (NO pos)

The POS=0 path demonstrates that WITHOUT the argmax the candidate set does not
contain all true key positions (recall gap), so the exact recovery fails.
====================================================================
"""
import sys, os
import numpy as np

RECOV_DIR = "suppt"      # candidati pos        (SEL=pos ./trace2support)
LINE_DIR  = "suppt"      # recupero a linee     (./trace2support)
TRUTH_DIR = "truth"
TRACE_DIR = "trace"
ENUM_CAP  = 20           # dimensione max del nucleo enumerabile a forza bruta

def deg(a): return a.bit_length() - 1

# ---------------------------------------------------------------------------------
# GF2X Arithm
# ---------------------------------------------------------------------------------
def gf2mul(a, b):
    r = 0
    while b:
        if b & 1: r ^= a
        b >>= 1; a <<= 1
    return r

def gf2divmod(a, b):
    q = 0; db = deg(b)
    while a and deg(a) >= db:
        d = deg(a) - db
        q ^= (1 << d); a ^= b << d
    return q, a

def gf2inv(a, f):
    r0, r1 = f, a
    s0, s1 = 0, 1
    while r1:
        q, rem = gf2divmod(r0, r1)
        r0, r1 = r1, rem
        s0, s1 = s1, s0 ^ gf2mul(q, s1)
    if r0 != 1: return None
    return gf2divmod(s0, f)[1]

def poly_from_support(sup):
    v = 0
    for p in sup: v |= (1 << p)
    return v
def support_of(poly, P):
    return [i for i in range(P) if (poly >> i) & 1]

# ---------------------------------------------------------------------------------
# candidate sets
# ---------------------------------------------------------------------------------
def line_window_candidates(line_dir, b, P, line, shift):
    """NO-POS: ogni linea recuperata -> la sua finestra di `line` posizioni bit."""
    rec_lines = sorted(set(int(x) for x in open(f"{line_dir}/H{b}").read().split()))
    C = set()
    for L in rec_lines:
        base = L << shift
        for r in range(line):
            if base + r < P: C.add(base + r)
    return sorted(C)

# ---------------------------------------------------------------------------------
# KERNEL
# ---------------------------------------------------------------------------------
def nullspace(rows, ncols):
    pivots = {}
    for r in rows:
        cur = r
        while cur:
            c = (cur & -cur).bit_length() - 1
            if c in pivots: cur ^= pivots[c]
            else: pivots[c] = cur; break
    for c in sorted(pivots):
        for c2 in sorted(pivots):
            if c2 != c and (pivots[c] >> c2) & 1:
                pivots[c] ^= pivots[c2]
    piv_cols = set(pivots)
    free = [c for c in range(ncols) if c not in piv_cols]
    basis = []
    for fcol in free:
        vec = (1 << fcol)
        for c, row in pivots.items():
            if (row >> fcol) & 1: vec ^= (1 << c)
        basis.append(vec)
    return basis, free

def recover_block(g, C0, C1, P, V):
    """Nucleo di 'h0*g zero fuori da C1', poi cerca la codeword di peso V.
       Ritorna (soluzioni, dim_nucleo, enumerato?)."""
    # righe del sistema, costruite in modo vettoriale (numpy packing dei bit)
    gbits = np.unpackbits(np.frombuffer(g.to_bytes((P + 7) // 8, "little"),
                          dtype=np.uint8), bitorder="little")[:P].astype(bool)
    C0arr = np.array(C0)
    inC1 = np.zeros(P, bool); inC1[C1] = True
    rows = []
    for p in range(P):
        if inC1[p]: continue
        rb = gbits[(p - C0arr) % P]
        if rb.any():
            rows.append(int.from_bytes(np.packbits(rb, bitorder="little").tobytes(), "little"))
    basis, _ = nullspace(rows, len(C0))
    dim = len(basis)
    sols, enumerated = [], (dim <= ENUM_CAP)
    if enumerated:
        for combo in range(1, 1 << dim):
            v = 0
            for b in range(dim):
                if (combo >> b) & 1: v ^= basis[b]
            if bin(v).count("1") == V:
                sols.append([C0[j] for j in range(len(C0)) if (v >> j) & 1])
    return sols, dim, enumerated

def _c(s, code, tty):
    return f"\033[{code}m{s}\033[0m" if tty else s

# ---------------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------------
def main():
    rec_dir   = sys.argv[1] if len(sys.argv) > 1 else RECOV_DIR
    truth_dir = sys.argv[2] if len(sys.argv) > 2 else TRUTH_DIR
    use_pos   = os.environ.get("POS", "1") != "0"

    P, V, N0, line, gran = open(os.path.join(TRACE_DIR, "meta")).read().split()
    P, V, N0, line = int(P), int(V), int(N0), int(line)
    shift = line.bit_length() - 1 if gran == "set" else 0
    f = (1 << P) | 1
    readints = lambda p: sorted(set(int(x) for x in open(p).read().split()))
    tty = sys.stdout.isatty()
    GREEN, RED, BOLD, DIM = "32;1", "31;1", "1", "2"
    bar = "=" * 56

    print(_c(bar, BOLD, tty))
    print(_c("  ALGEBRAIC KEY RECOVERY", BOLD, tty))
    print(_c(bar, BOLD, tty))

    # public key h = h1 * h0^{-1}
    t = [poly_from_support(readints(f"{truth_dir}/H{b}")) for b in range(N0)]
    t0inv = gf2inv(t[0], f)
    assert t0inv is not None, "t0 not invertible"
    g = gf2divmod(gf2mul(t[1], t0inv), f)[1]

    # --- candidate sets ---
    def need(paths, hint):
        missing = [p for p in paths if not os.path.exists(p)]
        if missing:
            print(f"\n  [!] input not found: {', '.join(missing)}")
            print(f"      generate it first with:\n        {hint}\n")
            sys.exit(2)

    if use_pos:
        need([f"{rec_dir}/candidates_H0", f"{rec_dir}/candidates_H1"],
             f"SEL=pos ./trace2support {TRACE_DIR} {rec_dir}")
        C0 = readints(f"{rec_dir}/candidates_H0")
        C1 = readints(f"{rec_dir}/candidates_H1")
        src = f"pos candidates ({rec_dir}/candidates_H*)"
    else:
        need([f"{LINE_DIR}/H0", f"{LINE_DIR}/H1"],
             f"./trace2support {TRACE_DIR} {LINE_DIR}")
        C0 = line_window_candidates(LINE_DIR, 0, P, line, shift)
        C1 = line_window_candidates(LINE_DIR, 1, P, line, shift)
        src = f"line windows, NO pos ({LINE_DIR}/H*)"
    neq = P - len(C1)

    # --- candidate recall: i candidati contengono la chiave vera? (requisito) ---
    truth_t0 = readints(f"{truth_dir}/H0")
    truth_t1 = readints(f"{truth_dir}/H1")
    cov0 = len(set(C0) & set(truth_t0)); cov1 = len(set(C1) & set(truth_t1))
    full = (cov0 == V and cov1 == V)

    print(f"\n  Candidate source   {src}")
    print(f"  Candidates         |C0| = {len(C0)}     |C1| = {len(C1)}")
    print("  Candidate recall   "
          + _c(f"C0 covers {cov0}/{V} true", GREEN if cov0 == V else RED, tty)
          + "   "
          + _c(f"C1 covers {cov1}/{V} true", GREEN if cov1 == V else RED, tty))
    if not full:
        print(_c("                     -> true key NOT fully inside candidates: "
                 "recovery cannot succeed", RED, tty))

    print(f"\n  {_c('Linear system over GF(2):', BOLD, tty)}")
    print(f"     equations   {neq:>6}   (key positions that must be zero: outside C1)")
    print(f"     unknowns    {len(C0):>6}   (candidate positions of block 0)")

    sols, dim, enumerated = recover_block(g, C0, C1, P, V)
    if dim == 0:
        tail = "trivial kernel (only the zero vector)"
    elif dim == 1:
        tail = "unique non-trivial solution"
    elif enumerated:
        tail = f"{2**dim - 1} non-trivial vectors (enumerated)"
    else:
        tail = f"too large to enumerate (> 2^{ENUM_CAP}): would need ISD"
    print(f"     kernel dim  {dim:>6}   -> {tail}")

    # --- verdict ---
    exact = next((s for s in sols if sorted(s) == truth_t0), None)
    print(f"\n  {_c('Result:', BOLD, tty)}")
    if exact is not None:
        t1_rec = support_of(gf2divmod(gf2mul(poly_from_support(exact), g), f)[1], P)
        print(f"     block 0  (h0)    {len(exact)}/{V} positions   " + _c("[EXACT]", GREEN, tty))
        print(f"     block 1  (h1)    {len(t1_rec)}/{V} positions   "
              + _c("[EXACT]", GREEN, tty) + "   (from h1 = h0*h)")
        os.makedirs("keys", exist_ok=True)
        for name, sup in (("H0", exact), ("H1", t1_rec)):
            with open(f"keys/{name}", "w") as fo:
                for x in sorted(sup): fo.write(f"{x}\n")
        print("\n  " + _c(" FULL PRIVATE KEY RECOVERED ", GREEN, tty))
        print(_c("  written to  keys/H0  keys/H1", DIM, tty))
    else:
        if not full:
            print("     " + _c("true key outside candidate set -> no valid solution", RED, tty))
            # mostra ESATTAMENTE quali posizioni vere il supporto non copre
            print("\n  " + _c("Uncovered true-key positions (lost to the channel):", BOLD, tty))
            for b, (C, tru) in enumerate(((C0, truth_t0), (C1, truth_t1))):
                miss = sorted(set(tru) - set(C))
                tag = _c(f"{len(miss)}/{V} missing", RED if miss else GREEN, tty)
                print(f"     block {b} (h{b}):  {tag}")
                for i in range(0, len(miss), 6):
                    row = "  ".join(_c(f"pos {p:>6} (line {p>>shift})", RED, tty)
                                    for p in miss[i:i+6])
                    print("        " + row)
        elif not enumerated:
            print("     " + _c(f"kernel too large to enumerate (dim {dim})", RED, tty)
                  + " -> the key is in a 2^%d space; use ISD (isd_nopos.py)" % dim)
        else:
            print("     " + _c("no weight-V vector in the kernel", RED, tty)
                  + "  (candidates cover the key but the system has no sparse solution)")
        print("\n  " + _c(" KEY NOT RECOVERED ", RED, tty))
        if not use_pos:
            print(_c("  (NO-pos case: the line-granularity carry blur puts true positions\n"
                     "   on the wrong line, so the candidate windows miss some of them)", DIM, tty))
    print(_c(bar, BOLD, tty))
    print()

if __name__ == "__main__":
    main()
