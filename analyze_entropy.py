#!/usr/bin/env python3
"""Analyze entropy desync in AV1 decoder debug log."""

import re
import sys

def analyze_log(log_path):
    with open(log_path) as f:
        lines = f.readlines()
    
    print(f"Analyzing {len(lines)} lines from {log_path}")
    print("="*60)
    
    # Find all BLK entries with mi=(0,16) or nearby
    print("\nBlocks around mi_col=16:")
    for i, line in enumerate(lines):
        match = re.search(r'BLK mi=\((\d+),(\d+)\)', line)
        if match:
            row, col = int(match.group(1)), int(match.group(2))
            if row == 0 and 14 <= col <= 20:
                # Parse range decoder state
                rd_match = re.search(r'rd_rng=(\d+)', line)
                rng = int(rd_match.group(1)) if rd_match else 0
                print(f"Line {i+1}: mi=({row},{col}) rng={rng}")
    
    # Look for mi=(0,16) specifically
    print("\n" + "="*60)
    print("Detail for mi=(0,16):")
    for i, line in enumerate(lines):
        if 'BLK mi=(0,16)' in line:
            print(f"Found at line {i+1}")
            # Print 20 lines after
            for j in range(i, min(i+20, len(lines))):
                l = lines[j].rstrip()
                if 'BLK' in l or 'END' in l or 'r=' in l:
                    print(f"  {j+1}: {l}")
            break
    
    # Check coefficient contexts
    print("\n" + "="*60)
    print("Coefficient contexts at mi=(0,16):")
    for i, line in enumerate(lines):
        if 'BLK mi=(0,16)' in line:
            for j in range(i, min(i+50, len(lines))):
                if 'COEFF[' in lines[j] and 'base_sym' in lines[j]:
                    match = re.search(r'ctx=(\d+).*base_sym=(\d+)', lines[j])
                    if match:
                        ctx, sym = match.groups()
                        print(f"  ctx={ctx}, sym={sym}")
            break

if __name__ == "__main__":
    log_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/dbg_fresh.log"
    analyze_log(log_path)
