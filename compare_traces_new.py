import re
import sys
from collections import Counter

def parse_dav1d(filename):
    res = []
    with open(filename, 'r') as f:
        for line in f:
            if 'DAV1D_SYM[' in line or 'DAV1D_EQUI[' in line:
                m = re.search(r'ret=(\d+)', line)
                if m:
                    res.append(m.group(1))
    return res

def parse_ours(filename):
    res = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('sym='):
                m = re.search(r'sym=(\d+).*line=(\d+)', line)
                if m:
                    res.append((m.group(1), m.group(2)))
    return res

dav1d_path = '/tmp/dav1d_trace.txt'
ours_path = '/tmp/ours_trace_now.txt'

if len(sys.argv) >= 3:
    dav1d_path = sys.argv[1]
    ours_path = sys.argv[2]

dav1d_syms = parse_dav1d(dav1d_path)
ours_data = parse_ours(ours_path)

print("--- First 40 pairs ---")
limit = min(40, len(dav1d_syms), len(ours_data))
for i in range(limit):
    d_val = dav1d_syms[i]
    o_val, o_line = ours_data[i]
    print(f"[{i:3}] dav1d: {d_val} | ours: {o_val} (line {o_line})")

mismatch_idx = -1
min_len = min(len(dav1d_syms), len(ours_data))
for i in range(min_len):
    if dav1d_syms[i] != ours_data[i][0]:
        mismatch_idx = i
        break

if mismatch_idx == -1 and len(dav1d_syms) != len(ours_data):
    mismatch_idx = min_len

if mismatch_idx == -1:
    print("\nNo mismatch found in symbol streams.")
    mismatch_idx = min_len
else:
    print(f"\nFirst mismatching index: {mismatch_idx}")
    print("\n--- Window around mismatch (+/- 12) ---")
    start = max(0, mismatch_idx - 12)
    end = min(min_len, mismatch_idx + 13)
    for i in range(start, end):
        d_val = dav1d_syms[i] if i < len(dav1d_syms) else "EOF"
        if i < len(ours_data):
            o_val, o_line = ours_data[i]
        else:
            o_val, o_line = "EOF", "N/A"
        marker = ">>>" if i == mismatch_idx else "   "
        print(f"{marker} [{i:3}] dav1d: {d_val} | ours: {o_val} (line {o_line})")

print("\n--- Our callsite histogram (up to mismatch) ---")
lines_up_to = [ours_data[i][1] for i in range(min(mismatch_idx + 1, len(ours_data)))]
hist = Counter(lines_up_to)
for line_num, count in sorted(hist.items(), key=lambda x: int(x[0])):
    print(f"Line {line_num:5}: {count:4} occurrences")
