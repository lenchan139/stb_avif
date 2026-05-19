import re
import sys

def parse_dav1d(filename):
    res = []
    with open(filename, 'r') as f:
        for line in f:
            if 'DAV1D_SYM[' in line:
                m = re.search(r'ret=(\d+)', line)
                if m:
                    res.append(m.group(1))
    return res

def parse_ours(filename):
    res = []
    # Our trace doesn't have sym=, it has PRE lines and we need to know what it decoded.
    # But wait, looking at the previous output, DAV1D_SYM has ret=
    # And our PRE lines are followed by another CALLSITE or PRE.
    # Let's check the trace again.
    return res

dav1d_path = sys.argv[1]
ours_path = sys.argv[2]

dav1d_syms = parse_dav1d(dav1d_path)
print(f"Dav1d symbols: {len(dav1d_syms)}")
