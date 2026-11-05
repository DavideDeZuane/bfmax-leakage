#!/usr/bin/env python3
"""
Recover the sets U0 and U1 starting from the execution trace thanks to alignment and voting

Modes (env SEL):
  pos               carry-aware BIT-level candidates (uses trace/pos*) [BIT space]
  median (default)  top-k lines, k = median(row weight)          [LINE space]
  kN                top-k lines, k = N                            [LINE space]

Usage: SEL=pos python3 trace2support.py [trace_dir=trace] [out_dir=suppt] [truth_dir=truth]
Writes: <out>/H{b} and <out>/truth_H{b} in the SAME space (LINE modes), or <out>/candidates_H{b} (POS mode).
"""
import sys, os, math
import numpy as np

TRACE_DIR = "trace"
TRUTH_DIR = "truth"
SUPPT_DIR = "suppt" 

def read_sets(path):
    return [[int(x) for x in l.split()] if l.strip() else [] for l in open(path)]

def read_ints(path):
    return [int(x) for x in open(path).read().split()]

def block_votes(iters, dom):
    its = [s for s in iters if s]
    if not its: return np.zeros(dom)
    M = np.zeros((len(its), dom))
    for t, s in enumerate(its): M[t, s] = 1.0
    F = np.fft.rfft(M, axis=1); fa = F[0]
    v = np.zeros(dom)
    for j in range(len(its)):
        sh = int(np.argmax(np.fft.irfft(fa * np.conj(F[j]), n=dom)))
        v += np.roll(M[j], sh)
    return v

def align(rec, truth, dom):
    """Rotate rec to the cyclic shift that maximises overlap with truth."""
    if not truth: return sorted(rec), 0
    tset = set(truth); best = (-1, 0)
    for r in range(dom):
        ov = sum(1 for x in rec if (x + r) % dom in tset)
        if ov > best[0]: best = (ov, r)
    ov, r = best
    return sorted((x + r) % dom for x in rec), ov

def recover_pos(iters, pos, P, shift, V):
    """Carry-aware, BIT level. For each candidate q, count iterations where the
       predicted line ((q+pos)>>shift) is among the touched ones. True positions
       score in every iteration; shadowed ones nearly so."""
    n = len(pos)
    votes = np.zeros(P, dtype=np.int32)
    q = np.arange(P)
    for j in range(n):
        touched = iters[j]
        if not touched: continue
        tset = np.zeros(((P >> shift) + 1), dtype=bool)
        tset[touched] = True
        votes += tset[((q + pos[j]) % P) >> shift]
    return votes

def _c(s, code, tty):
    return f"\033[{code}m{s}\033[0m" if tty else s

def main():
    trace_dir = sys.argv[1] if len(sys.argv) > 1 else "trace"
    out_dir   = sys.argv[2] if len(sys.argv) > 2 else "suppt"
    truth_dir = sys.argv[3] if len(sys.argv) > 3 else "truth"
    os.makedirs(out_dir, exist_ok=True)
    sel = os.environ.get("SEL", "median")
    tty = sys.stdout.isatty()
    BOLD, GREEN, DIM = "1", "32;1", "2"
    bar = "=" * 56

    P, V, N0, line, gran = open(os.path.join(trace_dir, "meta")).read().split()
    P, V, N0, line = int(P), int(V), int(N0), int(line)
    shift = line.bit_length() - 1 if gran == "set" else 0
    dom = math.ceil(P / line) if gran == "set" else P

    print()
    print(_c(bar, BOLD, tty))
    sub = "bit-level candidates" if sel == "pos" else f"{gran} space"
    print(_c(f"  TRACE -> SUPPORT   (mode: {sel}, {sub})", BOLD, tty))
    print(_c(bar, BOLD, tty))
    print(f"\n  Instance   P={P}  V={V}  blocks={N0}  line={line}B ({gran})  domain={dom}\n")

    def cell(text, width, color=None):
        """Right-justify to `width`, then colour (padding stays outside the ANSI)."""
        s = text.rjust(width)
        return s.replace(text, _c(text, color, tty)) if color else s

    if sel == "pos":
        print(_c("   block   iters  candidates  covers true  narrow  residual", BOLD, tty))
        for b in range(N0):
            iters = read_sets(os.path.join(trace_dir, f"H{b}"))
            pos   = read_ints(os.path.join(trace_dir, f"pos{b}"))
            truth = sorted(set(read_ints(os.path.join(truth_dir, f"H{b}"))))
            votes = recover_pos(iters, pos, P, shift, V)
            n = len([s for s in iters if s])
            cand = sorted(int(x) for x in np.where(votes == n)[0])
            cov = len(set(cand) & set(truth))
            with open(os.path.join(out_dir, f"candidates_H{b}"), "w") as f:
                for x in cand: f.write(f"{x}\n")
            color = GREEN if cov == len(truth) else "33;1"
            print(f"    H{b}  {n:>6}  {len(cand):>10}  "
                  + cell(f"{cov}/{len(truth)}", 11, color)
                  + f"  {str(P//max(1,len(cand)))+'x':>6}  {len(cand)/max(1,len(truth)):.1f}x/pos")
        print(_c(f"\n  output -> {out_dir}/candidates_H0..{N0-1}", DIM, tty))
    else:
        print(_c("   block   iters  recovered   match    recall", BOLD, tty))
        for b in range(N0):
            iters = read_sets(os.path.join(trace_dir, f"H{b}"))
            nz = [s for s in iters if s]
            truth_bits = sorted(set(read_ints(os.path.join(truth_dir, f"H{b}"))))
            truth_lines = sorted(set(p >> shift for p in truth_bits))
            votes = block_votes(iters, dom)
            if gran == "bit":         rec = set(np.argsort(votes)[-V:])
            elif sel.startswith("f"): rec = set(int(i) for i in np.where(votes >= float(sel[1:])*len(nz))[0])
            elif sel.startswith("k"): rec = set(np.argsort(votes)[-int(sel[1:]):])
            else:
                k = int(round(np.median([len(s) for s in nz]))) if nz else V
                rec = set(np.argsort(votes)[-k:])
            rec_out, ov = align(rec, truth_lines, dom)
            with open(os.path.join(out_dir, f"H{b}"), "w") as f:
                for x in rec_out: f.write(f"{x}\n")
            with open(os.path.join(out_dir, f"truth_H{b}"), "w") as f:
                for x in truth_lines: f.write(f"{x}\n")
            pct = 100 * ov / max(1, len(truth_lines))
            print(f"    H{b}  {len(nz):>6}  {len(rec_out):>9}  "
                  + f"{ov}/{len(truth_lines)}".rjust(7)
                  + f"  {pct:>6.1f}%")
        print(_c(f"\n  output -> {out_dir}/H0..{N0-1}, {out_dir}/truth_H0..{N0-1}"
                 f"   (compare:  diff {out_dir}/truth_H0 {out_dir}/H0)", DIM, tty))
    print(_c(bar, BOLD, tty))
    print()

if __name__ == "__main__":
    main()
