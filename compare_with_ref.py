#!/usr/bin/env python3
"""Compare stb_avif output with reference decoder (PyAV) to find entropy desync."""

import av
import numpy as np
import sys

def decode_with_pyav(avif_path):
    """Decode AVIF using PyAV (libdav1d) as reference."""
    container = av.open(avif_path)
    for frame in container.decode(video=0):
        # Convert to RGB
        rgb_frame = frame.to_rgb()
        return np.array(rgb_frame.to_ndarray())
    return None

def analyze_entropy_desync():
    """Analyze the entropy desync at mi_col=16."""
    avif_path = "/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif"
    
    print("Loading reference frame with PyAV...")
    ref_frame = decode_with_pyav(avif_path)
    if ref_frame is None:
        print("ERROR: Could not decode with PyAV")
        return
    
    print(f"Reference frame shape: {ref_frame.shape}")
    
    # The superblock boundary at mi_col=16 corresponds to pixel column:
    # mi_col=16, each mi is 4 pixels for Y, so 16*4 = 64 pixels
    boundary_x = 64
    
    # Extract pixels around boundary
    y_start = 0
    y_end = min(64, ref_frame.shape[0])
    
    # Pixels before boundary (cols 60-63)
    before_boundary = ref_frame[y_start:y_end, 60:64, :]
    print(f"\nReference pixels before boundary (x=60-63):")
    print(f"  Shape: {before_boundary.shape}")
    print(f"  Mean RGB: {before_boundary.mean(axis=(0,1)).astype(int)}")
    
    # Pixels at/after boundary (cols 64-67)
    at_boundary = ref_frame[y_start:y_end, 64:68, :]
    print(f"\nReference pixels at boundary (x=64-67):")
    print(f"  Shape: {at_boundary.shape}")
    print(f"  Mean RGB: {at_boundary.mean(axis=(0,1)).astype(int)}")
    
    # Save reference for comparison
    from PIL import Image
    ref_img = Image.fromarray(ref_frame)
    ref_img.save("/tmp/ref_fox.png")
    print(f"\nSaved reference to /tmp/ref_fox.png")
    
    return ref_frame

def analyze_coeff_ctx_function():
    """Analyze the get_lower_levels_ctx_2d function for potential bugs."""
    print("\n=== Analyzing get_lower_levels_ctx_2d ===")
    
    # The function uses:
    # - padded_pos = coeff_idx + (x << 2)
    # - stride = txh + STBI_AVIF_TX_PAD_HOR
    # - 5 neighbors: (+1,0), (0,+1), (+1,+1), (+2,0), (0,+2)
    
    # For a 4x4 transform (txw=4, txh=4, bhl=2):
    txw, txh, bhl = 4, 4, 2
    stride = txh + 4  # TX_PAD_HOR = 4
    
    print(f"For 4x4 transform:")
    print(f"  txw={txw}, txh={txh}, bhl={bhl}")
    print(f"  stride = {txh} + 4 = {stride}")
    print(f"  padded array size = (64+4) * (64+2) + 8 = {68*66+8}")
    
    # Check position calculation for scan positions
    scan_4x4 = [0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15]
    
    for c in [6, 5, 4, 3, 2, 1]:
        coeff_idx = scan_4x4[c]
        x = coeff_idx >> bhl  # column
        y = coeff_idx - (x << bhl)  # row
        padded_pos = coeff_idx + (x << 2)
        
        print(f"\n  c={c}: scan_pos={coeff_idx}, x={x}, y={y}")
        print(f"    padded_pos = {coeff_idx} + ({x}<<2) = {padded_pos}")
        print(f"    neighbors at: +1={padded_pos+1}, +stride={padded_pos+stride}, "
              f"+stride+1={padded_pos+stride+1}, +2={padded_pos+2}, +stride*2={padded_pos+stride*2}")
        
        # Check if padded_pos + stride*2 exceeds buffer
        max_pos = padded_pos + stride * 2
        if max_pos >= 68 * 66 + 8:
            print(f"    *** BUFFER OVERFLOW RISK: max_pos={max_pos} > buffer_size={68*66+8}")

if __name__ == "__main__":
    print("=" * 60)
    print("AV1 Entropy Desync Analysis")
    print("=" * 60)
    
    # Analyze the context function
    analyze_coeff_ctx_function()
    
    # Try to get reference output
    try:
        analyze_entropy_desync()
    except Exception as e:
        print(f"\nPyAV analysis failed: {e}")
        print("Continuing with manual analysis...")
