#!/usr/bin/env python3
"""
Debug CDF corruption in AV1 decoder.
Analyzes debug logs to find when coeff_base_cdf gets corrupted.
"""

import sys
import re

def analyze_cdf_log(log_path):
    with open(log_path) as f:
        lines = f.readlines()
    
    print(f"Analyzing {len(lines)} lines from {log_path}")
    print("="*60)
    
    # Find first CDF_CHECK at mi=(0,16)
    found_blk_16 = False
    for i, line in enumerate(lines):
        if "BLK mi=(0,16)" in line:
            found_blk_16 = True
            print(f"\nLine {i+1}: {line.strip()}")
            # Check next few lines for CDF_CHECK
            for j in range(i+1, min(i+20, len(lines))):
                if "CDF" in lines[j]:
                    print(f"  Line {j+1}: {lines[j].strip()}")
            break
    
    if not found_blk_16:
        print("ERROR: No BLK mi=(0,16) found in log!")
        return
    
    # Check POST_RESET_CDF
    print("\n" + "="*60)
    print("CDF state immediately after reset:")
    for i, line in enumerate(lines):
        if "POST_RESET_CDF" in line:
            print(f"Line {i+1}: {line.strip()}")
    
    # Find coeff_base_cdf access patterns
    print("\n" + "="*60)
    print("First 5 coeff_base_cdf accesses:")
    count = 0
    for i, line in enumerate(lines):
        if "coeff_base_cdf" in line and "CDF_CHECK" in line:
            print(f"Line {i+1}: {line.strip()}")
            count += 1
            if count >= 5:
                break
    
    # Check if any CDF values are out of range
    print("\n" + "="*60)
    print("Checking for out-of-range CDF values (> 32768):")
    for i, line in enumerate(lines):
        if "CDF_CHECK[coeff_base_cdf" in line:
            # Extract numbers after colon
            match = re.search(r':\s*([\d\s]+)', line)
            if match:
                nums_str = match.group(1)
                nums = [int(n) for n in nums_str.split() if n.isdigit()]
                for n in nums:
                    if n > 32768 or n < 0:
                        print(f"Line {i+1}: INVALID CDF value {n}")
                        print(f"  {line.strip()}")

if __name__ == "__main__":
    log_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/dbg_cdf.log"
    analyze_cdf_log(log_path)
