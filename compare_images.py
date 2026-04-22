import numpy as np
from PIL import Image
import sys

def compare(ref_path, ours_path, out_path):
    try:
        ref = Image.open(ref_path).convert('RGB')
        ours = Image.open(ours_path).convert('RGB')
        
        # 1. Whether both have content
        print(f"Reference size: {ref.size}")
        print(f"Ours size: {ours.size}")
        
        if ref.size != ours.size:
            print("Warning: Sizes differ, resizing ours to match reference.")
            ours = ours.resize(ref.size)
            
        ref_arr = np.array(ref).astype(np.float32)
        ours_arr = np.array(ours).astype(np.float32)
        
        # 2. MAE
        mae = np.mean(np.abs(ref_arr - ours_arr))
        print(f"MAE: {mae:.4f}")
        
        # 3. Degradation or noise
        # Heuristic: if MAE is very high, it might be noise.
        # If MAE is low but visible, it's degradation.
        if mae > 100:
            print("Status: Ours looks like noise (High MAE)")
        elif mae > 0:
            print("Status: Ours looks like a degraded version of the reference")
        else:
            print("Status: Images are identical")
            
        # Side-by-side
        combined = Image.new('RGB', (ref.width * 2, ref.height))
        combined.paste(ref, (0, 0))
        combined.paste(ours, (ref.width, 0))
        combined.save(out_path)
        print(f"Side-by-side saved to {out_path}")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    compare('/tmp/fox_ref.png', '/Users/len/Downloads/gcp/output_png/fox.profile0.8bpc.yuv420.png', '/tmp/compare.png')
