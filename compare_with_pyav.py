#!/usr/bin/env python3
"""Compare stb_avif output with PyAV (libdav1d) reference decoder."""

import av
import numpy as np
from PIL import Image
import sys

def decode_with_pyav(avif_path):
    """Decode AVIF using PyAV (libdav1d) as reference."""
    container = av.open(avif_path)
    for frame in container.decode(video=0):
        rgb_frame = frame.to_rgb()
        return np.array(rgb_frame.to_ndarray())
    return None

def load_stb_output(png_path):
    """Load stb_avif PNG output."""
    img = Image.open(png_path)
    return np.array(img)

def compare_outputs(avif_path, png_path):
    """Compare decoder outputs pixel by pixel."""
    print(f"Comparing:")
    print(f"  AVIF: {avif_path}")
    print(f"  PNG:  {png_path}")
    print()
    
    # Load reference (PyAV)
    print("Loading reference (PyAV/dav1d)...")
    ref = decode_with_pyav(avif_path)
    if ref is None:
        print("ERROR: Could not decode with PyAV")
        return False
    print(f"  Reference shape: {ref.shape}")
    
    # Load stb_avif output
    print("Loading stb_avif output...")
    try:
        stb = load_stb_output(png_path)
        print(f"  stb_avif shape: {stb.shape}")
    except Exception as e:
        print(f"ERROR: Could not load PNG: {e}")
        return False
    
    # Compare shapes
    if ref.shape != stb.shape:
        print(f"  WARNING: Shape mismatch!")
        print(f"    Reference: {ref.shape}")
        print(f"    stb_avif:  {stb.shape}")
        # Try to compare common region
        min_h = min(ref.shape[0], stb.shape[0])
        min_w = min(ref.shape[1], stb.shape[1])
        ref = ref[:min_h, :min_w]
        stb = stb[:min_h, :min_w]
    
    # Calculate difference
    diff = np.abs(ref.astype(int) - stb.astype(int))
    max_diff = diff.max()
    mean_diff = diff.mean()
    
    print()
    print("Comparison Results:")
    print(f"  Maximum pixel difference: {max_diff}")
    print(f"  Mean pixel difference:    {mean_diff:.2f}")
    print()
    
    # Find first significant difference
    if max_diff > 0:
        diff_mask = diff > 10  # threshold
        if diff_mask.any():
            first_diff = np.where(diff_mask)
            y, x = first_diff[0][0], first_diff[1][0]
            print(f"  First significant diff at: ({y}, {x})")
            print(f"    Reference: {ref[y, x]}")
            print(f"    stb_avif:  {stb[y, x]}")
            print()
    
    # Overall assessment
    if max_diff == 0:
        print("  Result: PERFECT MATCH")
        return True
    elif max_diff <= 2:
        print("  Result: GOOD (minor rounding differences)")
        return True
    elif max_diff <= 10:
        print("  Result: ACCEPTABLE (small differences)")
        return True
    else:
        print("  Result: SIGNIFICANT DIFFERENCES FOUND")
        # Save diff visualization
        diff_img = (diff * 255 / max_diff).astype(np.uint8)
        diff_path = png_path.replace('.png', '_diff.png')
        Image.fromarray(diff_img).save(diff_path)
        print(f"  Diff visualization saved: {diff_path}")
        return False

if __name__ == "__main__":
    # Compare fox.avif
    avif_file = "/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif"
    png_file = "/Users/len/Downloads/gcp/output_png/fox.profile0.8bpc.yuv420.avif.png"
    
    if len(sys.argv) > 1:
        avif_file = sys.argv[1]
        png_file = sys.argv[2] if len(sys.argv) > 2 else avif_file.replace('.avif', '.png')
    
    success = compare_outputs(avif_file, png_file)
    sys.exit(0 if success else 1)
