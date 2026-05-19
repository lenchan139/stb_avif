import re
import sys


def get_dav1d_syms(filename):
    syms = []
    with open(filename, 'r') as f:
        for line in f:
            if 'DAV1D_SYM[' in line or 'DAV1D_EQUI[' in line:
                match = re.search(r'ret=(\d+)', line)
                if match:
                    syms.append(int(match.group(1)))
    return syms


def get_ours_syms(filename):
    syms = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('sym='):
                match = re.search(r'sym=(\d+)', line)
                if match:
                    syms.append(int(match.group(1)))
    return syms


dav1d_path = '/tmp/dav1d_trace.txt'
ours_path = '/tmp/ours_trace_now.txt'
limit = None

if len(sys.argv) >= 3:
    dav1d_path = sys.argv[1]
    ours_path = sys.argv[2]
if len(sys.argv) >= 4:
    limit = int(sys.argv[3])

dav1d_syms = get_dav1d_syms(dav1d_path)
ours_syms = get_ours_syms(ours_path)

print(f"Dav1d symbols: {len(dav1d_syms)}")
print(f"Ours symbols: {len(ours_syms)}")

compare_len = min(len(dav1d_syms), len(ours_syms))
if limit is not None:
    compare_len = min(compare_len, limit)

print(f"Comparing first {compare_len} symbols...")

mismatch_idx = -1
for i in range(compare_len):
    if dav1d_syms[i] != ours_syms[i]:
        mismatch_idx = i
        break

if mismatch_idx != -1:
    print(f"MISMATCH at index {mismatch_idx}:")
    print(f"  dav1d: {dav1d_syms[mismatch_idx]}")
    print(f"  ours : {ours_syms[mismatch_idx]}")
elif limit is None and len(dav1d_syms) != len(ours_syms):
    print(f"NO MISMATCH in first {compare_len} symbols, but lengths differ (dav1d={len(dav1d_syms)}, ours={len(ours_syms)})")
else:
    print(f"MATCH for first {compare_len} symbols")

