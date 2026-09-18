#!/usr/bin/env python3
"""test_y4m_compare.py - compare two YUV4MPEG2 files plane-by-plane.

Reports per-plane diff magnitude: max absolute difference, mean absolute
difference, and the number of pixels off by more than 3.  Only the first
frame of each file is compared (the harness emits single-frame streams).

Usage:
    test_y4m_compare.py REFERENCE.y4m CANDIDATE.y4m [--strict] [--maxd N] [--mean N]

Exit status:
    0  byte-identical, or within tolerance when not --strict
    1  geometry mismatch, or beyond tolerance
"""

import argparse
import re
import sys


def parse(path):
    with open(path, 'rb') as f:
        data = f.read()
    if not data.startswith(b'YUV4MPEG2 '):
        return None
    hdr_end = data.index(b'\n')
    hdr = data[:hdr_end].decode('ascii', 'ignore')
    m = re.search(r' W(\d+) H(\d+)', hdr)
    if not m:
        return None
    w, h = int(m.group(1)), int(m.group(2))
    cm = re.search(r' Ip (\w+)', hdr)
    chroma = cm.group(1) if cm else 'C420'
    uw, uh = (w + 1) // 2, (h + 1) // 2
    plane_sizes = {
        'C420': [w * h, uw * uh, uw * uh],
        'C422': [w * h, uw * h, uw * h],
        'C444': [w * h, w * h, w * h],
        'Cmono': [w * h],
    }
    if chroma not in plane_sizes:
        return None
    sizes = plane_sizes[chroma]
    fr = data.find(b'FRAME\n', hdr_end)
    if fr < 0:
        return None
    body = data[fr + 6:]
    if len(body) < sum(sizes):
        return None
    planes = []
    off = 0
    for s in sizes:
        planes.append(body[off:off + s])
        off += s
    return w, h, chroma, planes


def compare_planes(a, b):
    n = min(len(a), len(b))
    maxd = 0
    mean = 0.0
    over3 = 0
    for i in range(n):
        d = abs(a[i] - b[i])
        if d > maxd:
            maxd = d
        mean += d
        if d > 3:
            over3 += 1
    if n:
        mean /= n
    len_extra = max(len(a), len(b)) - n
    return maxd, mean, over3, len_extra


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('reference')
    ap.add_argument('candidate')
    ap.add_argument('--strict', action='store_true',
                    help='demand byte-identical output')
    ap.add_argument('--maxd', type=int, default=32,
                    help='tolerated peak difference per plane (default 32)')
    ap.add_argument('--mean', type=float, default=0.05,
                    help='tolerated mean difference per plane (default 0.05)')
    args = ap.parse_args()

    ref = parse(args.reference)
    cand = parse(args.candidate)
    if not ref:
        print(f"error: cannot parse reference {args.reference}")
        return 2
    if not cand:
        print(f"error: cannot parse candidate {args.candidate}")
        return 2

    name = args.candidate.split('/')[-1]
    if ref[0] != cand[0] or ref[1] != cand[1] or ref[2] != cand[2]:
        print(f"{name}: GEOMETRY {ref[:3]} vs {cand[:3]}")
        return 1

    print(f"{name}: {ref[2]} {ref[0]}x{ref[1]}")
    failing = False
    for plane, rp, cp in zip('Y U V'.split(), ref[3], cand[3]):
        maxd, mean, over3, len_extra = compare_planes(rp, cp)
        ok = len_extra == 0 and ((maxd == 0 and mean == 0) or
                                 (not args.strict and
                                  maxd <= args.maxd and mean <= args.mean))
        if maxd or mean > 0.0:
            print(f"  {plane}: maxd={maxd} mean={mean:.3f} >3={over3} "
                  f"len_extra={len_extra} {'OK' if ok else 'FAIL'}")
        if not ok:
            failing = True

    if args.strict and not failing:
        print("  byte-identical: PASS")
    if failing:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())