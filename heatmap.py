import numpy as np
from PIL import Image
import os

def analyze(name, decoded_path, reference_path):
    print(f"--- Analysis for {name} ---")
    img_dec = np.array(Image.open(decoded_path).convert('RGB')).astype(float)
    img_ref = np.array(Image.open(reference_path).convert('RGB')).astype(float)
    
    diff = np.abs(img_dec - img_ref)
    sum_diff = np.sum(diff, axis=2)
    
    h, w = sum_diff.shape
    grid_h, grid_w = h // 8, w // 8
    heatmap = np.zeros((8, 8))
    for i in range(8):
        for j in range(8):
            cell = sum_diff[i*grid_h:(i+1)*grid_h, j*grid_w:(j+1)*grid_w]
            heatmap[i, j] = np.mean(cell)
    
    print("8x8 Heatmap (mean abs-diff):")
    for row in heatmap:
        print(" ".join([f"{x:6.2f}" for x in row]))
        
    row_means = np.mean(sum_diff, axis=1)
    high_diff_row = np.where(row_means > 5)[0]
    if len(high_diff_row) > 0:
        print(f"First row index with mean abs-diff > 5: {high_diff_row[0]}")
    else:
        print("No row with mean abs-diff > 5")
        
    total_pixels = sum_diff.size
    print(f"Exact match (diff==0): {np.sum(sum_diff == 0) / total_pixels * 100:.2f}%")
    print(f"Diff < 2: {np.sum(sum_diff < 2) / total_pixels * 100:.2f}%")
    print(f"Diff < 5: {np.sum(sum_diff < 5) / total_pixels * 100:.2f}%")
    print(f"Diff > 30: {np.sum(sum_diff > 30) / total_pixels * 100:.2f}%")
    
    print("Row means at markers:")
    markers = [0, 8, 16, 32, 64, 128, 256, 512, 1023]
    for m in markers:
        if m < h:
            print(f"Row {m:4}: {row_means[m]:.4f}")
            
    if name == "fox":
        print("First 4x4 R-channel pixels (Decoded):")
        print(img_dec[0:4, 0:4, 0])
        print("First 4x4 R-channel pixels (Reference):")
        print(img_ref[0:4, 0:4, 0])

analyze("fox", "/tmp/fox_out.ppm", "output_png/fox.profile0.8bpc.yuv420.ppm")
analyze("steam", "/tmp/steam_out.ppm", "output_png/steam_2253100.ppm")
