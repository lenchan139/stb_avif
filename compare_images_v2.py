import numpy as np
from PIL import Image
import sys

def compare(ref_path, ours_path, out_path):
    try:
        ref = Image.open(ref_path).convert('RGB')
        ours = Image.open(ours_path).convert('RGB')
        
        if ref.size != ours.size:
            ours = ours.resize(ref.size)
            
        ref_arr = np.array(ref).astype(np.float32)
        ours_arr = np.array(ours).astype(np.float32)
        
        mae = np.mean(np.abs(ref_arr - ours_arr))
        print(f"MAE: {mae:.4f}")
        
        combined = Image.new('RGB', (ref.width * 2, ref.height))
        combined.paste(ref, (0, 0))
        combined.paste(ours, (ref.width, 0))
        combined.save(out_path)
        print(f"Side-by-side saved to {out_path}")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    compare('/tmp/fox_ref.png', 'output_png/fox.profile0.8bpc.yuv420.png', '/tmp/compare2.png')
