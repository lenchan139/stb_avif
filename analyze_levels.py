#!/usr/bin/env python3
"""Analyze coefficient context and levels array from debug log."""

import re

def analyze_coefficient_contexts():
    """Extract and verify coefficient contexts from debug log."""
    with open('/tmp/dbg_fresh.log') as f:
        lines = f.readlines()
    
    print("=" * 60)
    print("Coefficient Context Analysis at mi=(0,16) boundary")
    print("=" * 60)
    
    # Find mi=(0,16) and mi=(0,18) blocks
    blocks = []
    for i, line in enumerate(lines):
        m = re.search(r'BLK mi=\((\d+),(\d+)\).*rd_rng=(\d+)', line)
        if m:
            row, col, rng = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if row == 0 and col in [14, 16, 18, 20]:
                blocks.append((i, col, rng))
    
    print("\nBlocks around superblock boundary:")
    for idx, col, rng in blocks:
        print(f"  mi=(0,{col}): line {idx+1}, rd_rng={rng}")
    
    # Extract coefficient decode sequence
    print("\n" + "=" * 60)
    print("Coefficient Decode Sequence at mi=(0,18)")
    print("=" * 60)
    
    for i, line in enumerate(lines):
        if 'BLK mi=(0,18)' in line:
            print("\nY plane coefficients:")
            for j in range(i, min(i+100, len(lines))):
                if 'COEFF[c=' in lines[j] and 'base_sym' in lines[j]:
                    m = re.search(r'COEFF\[c=(\d+),pos=(\d+),ctx=(\d+)\]: base_sym=(\d+)', lines[j])
                    if m:
                        c, pos, ctx, sym = m.groups()
                        print(f"  c={c:>2}, pos={pos:>3}, ctx={ctx:>2}, sym={sym}")
            
            print("\nUV plane coefficients:")
            for j in range(i, min(i+100, len(lines))):
                if 'UV-' in lines[j] and 'COEFF' in lines[j]:
                    print(f"  {lines[j].strip()[:100]}")
            break
    
    # Calculate expected contexts
    print("\n" + "=" * 60)
    print("Expected Context Calculation for 4x4 TX")
    print("=" * 60)
    
    scan_4x4 = [0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15]
    nz_map_5x5 = [
        [[0,1,6,6,21],[1,6,6,21,21],[6,6,21,21,21],[6,21,21,21,21],[21,21,21,21,21]],
        [[0,16,6,6,21],[16,16,6,21,21],[16,16,21,21,21],[16,16,21,21,21],[16,16,21,21,21]],
        [[0,11,11,11,11],[11,11,11,11,11],[6,6,21,21,21],[6,21,21,21,21],[21,21,21,21,21]]
    ]
    
    bhl = 2
    shape = 0
    
    print(f"\n{'c':>3} {'pos':>5} {'x':>4} {'y':>4} {'offset':>7} {'exp_ctx':>8} {'log_ctx':>8}")
    print("-" * 50)
    
    # Log shows: c=6->ctx=6, c=5->ctx=7, c=4->ctx=7, c=3->ctx=6, c=2->ctx=2
    log_ctx = {6: 6, 5: 7, 4: 7, 3: 6, 2: 2}
    
    for c in [6, 5, 4, 3, 2]:
        pos = scan_4x4[c]
        x = pos >> bhl
        y = pos - (x << bhl)
        xcl = min(x, 4)
        ycl = min(y, 4)
        offset = nz_map_5x5[shape][ycl][xcl]
        # With zero neighbors, ctx = offset
        exp_ctx = offset
        log = log_ctx.get(c, '?')
        match = "✓" if exp_ctx == log else "✗"
        print(f"{c:>3} {pos:>5} {x:>4} {y:>4} {offset:>7} {exp_ctx:>8} {log:>8} {match}")
    
    print("\n" + "=" * 60)
    print("Analysis Summary")
    print("=" * 60)
    print("Expected contexts assume zero neighbor magnitudes (first decode).")
    print("Mismatches indicate non-zero neighbors in levels array.")
    print("c=5 and c=4 show higher ctx than expected - neighbors had magnitude.")
    
def verify_padded_buffer_layout():
    """Verify the padded buffer layout calculation."""
    print("\n" + "=" * 60)
    print("Padded Buffer Layout Verification")
    print("=" * 60)
    
    # Buffer: levels[(64 + 4) * (64 + 2) + 8] = 68 * 66 + 8 = 4496 bytes
    buffer_size = (64 + 4) * (64 + 2) + 8
    print(f"\nBuffer size: (64+4)*(64+2)+8 = {buffer_size}")
    
    # For 4x4: txh=4, stride=8
    txh = 4
    stride = txh + 4
    print(f"\nFor 4x4 transform:")
    print(f"  txh={txh}, stride={stride}")
    
    # Padded layout: padded_pos = coeff_idx + (x << 2)
    print(f"\nPadded position calculation: padded_pos = coeff_idx + (x << 2)")
    
    scan_4x4 = [0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15]
    
    print(f"\n{'pos':>5} {'x':>4} {'padded':>8} {'neighbors':>40}")
    print("-" * 60)
    
    for c in [6, 5, 4, 3, 2, 1, 0]:
        pos = scan_4x4[c]
        x = pos >> 2
        padded = pos + (x << 2)
        
        # 5 neighbors: (+1,0), (0,+1), (+1,+1), (+2,0), (0,+2)
        n1 = padded + 1
        n2 = padded + stride
        n3 = padded + stride + 1
        n4 = padded + 2
        n5 = padded + stride * 2
        
        neighbors = f"+1={n1}, +s={n2}, +s+1={n3}, +2={n4}, +2s={n5}"
        print(f"{pos:>5} {x:>4} {padded:>8} {neighbors}")
        
        # Check bounds
        max_n = max(n1, n2, n3, n4, n5)
        if max_n >= buffer_size:
            print(f"  *** BUFFER OVERFLOW: max_neighbor={max_n} >= {buffer_size}")
    
    print("\nAll padded positions within buffer bounds for 4x4.")

if __name__ == "__main__":
    analyze_coefficient_contexts()
    verify_padded_buffer_layout()
