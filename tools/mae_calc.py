import sys
import os
import av
import numpy as np
import subprocess

files = [
    "G-0trmKXsAA1sQZ-thumb.avif",
    "G-0trmKXsAA1sQZ.avif",
    "Gb5RU6RWoAAQQ1n.avif",
    "fox.profile0.8bpc.yuv420.avif",
    "fox.profile0.10bpc.yuv420.avif",
    "kimono.avif",
    "red-at-12-oclock-with-color-profile-10bpc.avif",
    "steam_2253100.avif"
]

baseline = {
    "G-0trmKXsAA1sQZ-thumb": (114.63, 5.52, 12.76),
    "G-0trmKXsAA1sQZ": (75.11, 6.25, 7.63),
    "Gb5RU6RWoAAQQ1n": (65.57, 11.42, 8.97),
    "fox.profile0.8bpc.yuv420": (63.63, 9.53, 6.95),
    "fox.profile0.10bpc.yuv420": (257.71, 35.20, 32.82),
    "kimono": (42.11, 11.98, 13.23),
    "red-at-12-oclock-with-color-profile-10bpc": (354.47, 86.27, 83.66),
    "steam_2253100": (280.09, 58.69, 37.42)
}

print(f"{'File':<45} | {'Plane':<5} | {'New MAE':<8} | {'Baseline':<8} | {'Delta':<8}")
print("-" * 88)

for f in files:
    path = os.path.join("example_avif", f)
    env = os.environ.copy()
    env["STBI_AVIF_DUMP_YUV"] = "/tmp/our_yuv.bin"
    subprocess.run(["./tests/test_decode", path, "/tmp/dummy.ppm"], env=env, capture_output=True)
    
    container = av.open(path)
    frame = next(container.decode(video=0))
    fmt = frame.format.name
    is_10bit_ref = "16" in fmt or "10" in fmt or "12" in fmt
    
    yuv_data = open("/tmp/our_yuv.bin", "rb").read()
    
    def get_gt_plane(p_idx):
        plane = frame.planes[p_idx]
        arr = np.frombuffer(plane, dtype=np.uint16 if is_10bit_ref else np.uint8)
        stride = plane.line_size // (2 if is_10bit_ref else 1)
        width = plane.width
        height = plane.height
        arr = arr.reshape((height, stride))
        return arr[:, :width].flatten().astype(np.float32)

    gt_y = get_gt_plane(0)
    gt_u = get_gt_plane(1)
    gt_v = get_gt_plane(2)
    
    y_size, u_size, v_size = len(gt_y), len(gt_u), len(gt_v)
    total_pix = y_size + u_size + v_size
    
    if len(yuv_data) == total_pix:
        our_yuv = np.frombuffer(yuv_data, dtype=np.uint8).astype(np.float32)
    elif len(yuv_data) == total_pix * 2:
        our_yuv = np.frombuffer(yuv_data, dtype=np.uint16).astype(np.float32)
    else:
        if len(yuv_data) % 2 == 0:
             our_yuv = np.frombuffer(yuv_data, dtype=np.uint16).astype(np.float32)
             if len(our_yuv) != total_pix:
                  print(f"Error: total size mismatch for {f}. Got {len(our_yuv)}, expected {total_pix}")
                  continue
        else:
            print(f"Error: total size mismatch for {f}. Got {len(yuv_data)}, expected {total_pix} or {total_pix*2}")
            continue

    our_y = our_yuv[:y_size]
    our_u = our_yuv[y_size:y_size+u_size]
    our_v = our_yuv[y_size+u_size:y_size+u_size+v_size]
    
    mae_y = np.mean(np.abs(our_y - gt_y))
    mae_u = np.mean(np.abs(our_u - gt_u))
    mae_v = np.mean(np.abs(our_v - gt_v))
    
    base_key = f.replace(".avif", "").replace(".yuv420", "")
    b_y, b_u, b_v = baseline.get(base_key, (0,0,0))
    
    for plane, val, base in [("Y", mae_y, b_y), ("U", mae_u, b_u), ("V", mae_v, b_v)]:
        delta = val - base
        print(f"{f:<45} | {plane:<5} | {val:>8.2f} | {base:>8.2f} | {delta:>8.2f}")
