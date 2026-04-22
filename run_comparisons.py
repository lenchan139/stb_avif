import subprocess
import os
import sys
import numpy as np
from PIL import Image

def compare(ref_path, ours_path):
    try:
        ref = Image.open(ref_path).convert('RGB')
        ours = Image.open(ours_path).convert('RGB')
        if ref.size != ours.size:
            ours = ours.resize(ref.size)
        ref_arr = np.array(ref).astype(np.float32)
        ours_arr = np.array(ours).astype(np.float32)
        mae = np.mean(np.abs(ref_arr - ours_arr))
        return mae
    except Exception as e:
        return str(e)

images = [
    ("kimono", "example_avif/kimono.avif", "output_png/kimono.png"),
    ("fox.profile0.8bpc.yuv420", "example_avif/fox.profile0.8bpc.yuv420.avif", "output_png/fox.profile0.8bpc.yuv420.png"),
    ("steam_2253100", "example_avif/steam_2253100.avif", "output_png/steam_2253100.png"),
    ("G-0trmKXsAA1sQZ", "example_avif/G-0trmKXsAA1sQZ.avif", "output_png/G-0trmKXsAA1sQZ.png")
]

for name, avif, ref in images:
    tmp_ppm = f"/tmp/{name}.ppm"
    res = subprocess.run(["/tmp/test_decode_fast", avif, tmp_ppm], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"{name}: Decode failed")
        continue
    mae = compare(ref, tmp_ppm)
    print(f"{name} MAE: {mae}")
