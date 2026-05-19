import sys
import os
import av
import numpy as np
import subprocess

files = [
    "fox.profile0.10bpc.yuv420.avif",
    "red-at-12-oclock-with-color-profile-10bpc.avif",
    "steam_2253100.avif"
]

for f in files:
    path = os.path.join("example_avif", f)
    env = os.environ.copy()
    env["STBI_AVIF_DUMP_YUV"] = "/tmp/our_yuv.bin"
    subprocess.run(["./tests/test_decode", path, "/tmp/dummy.ppm"], env=env, capture_output=True)
    
    container = av.open(path)
    frame = next(container.decode(video=0))
    plane = frame.planes[0]
    is_10bit_ref = "16" in frame.format.name or "10" in frame.format.name or "12" in frame.format.name
    
    gt_arr = np.frombuffer(plane, dtype=np.uint16 if is_10bit_ref else np.uint8)
    stride = plane.line_size // (2 if is_10bit_ref else 1)
    width, height = plane.width, plane.height
    gt_arr = gt_arr.reshape((height, stride))[:, :width].astype(np.float32)
    
    yuv_data = open("/tmp/our_yuv.bin", "rb").read()
    if len(yuv_data) == (width * height + 2 * (width//2 * height//2)) * 2: # 4:2:0 10-bit
        our_y = np.frombuffer(yuv_data[:width*height*2], dtype=np.uint16).reshape((height, width)).astype(np.float32)
    elif len(yuv_data) == (width * height * 3) * 2: # 4:4:4 10-bit
        our_y = np.frombuffer(yuv_data[:width*height*2], dtype=np.uint16).reshape((height, width)).astype(np.float32)
    else:
        # Fallback for 8-bit or other
        our_y = np.frombuffer(yuv_data[:width*height], dtype=np.uint8).reshape((height, width)).astype(np.float32)

    diff = np.abs(our_y - gt_arr)
    sb_size = 64
    sb_h = (height + sb_size - 1) // sb_size
    sb_w = (width + sb_size - 1) // sb_size
    
    sb_errors = []
    for sy in range(sb_h):
        for sx in range(sb_w):
            y0, y1 = sy * sb_size, min((sy + 1) * sb_size, height)
            x0, x1 = sx * sb_size, min((sx + 1) * sb_size, width)
            sb_mae = np.mean(diff[y0:y1, x0:x1])
            sb_errors.append((sx, sy, sb_mae))
            
    sb_errors.sort(key=lambda x: x[2], reverse=True)
    
    print(f"\nFILE: {f}")
    print(f"Top 10 worst superblocks (x, y, MAE):")
    for i in range(min(10, len(sb_errors))):
        print(f"  ({sb_errors[i][0]:2d}, {sb_errors[i][1]:2d}): {sb_errors[i][2]:8.2f}")
    
    top_64_mae = np.mean(diff[:64, :])
    bot_64_mae = np.mean(diff[-64:, :])
    left_64_mae = np.mean(diff[:, :64])
    right_64_mae = np.mean(diff[:, -64:])
    
    print(f"Mean MAE top 64 rows: {top_64_mae:.2f} | bottom 64 rows: {bot_64_mae:.2f}")
    print(f"Mean MAE left 64 col: {left_64_mae:.2f} | right 64 col: {right_64_mae:.2f}")
    
    maes = [e[2] for e in sb_errors]
    hists = [
        ("<5", lambda x: x < 5),
        ("5-20", lambda x: 5 <= x < 20),
        ("20-50", lambda x: 20 <= x < 50),
        ("50-100", lambda x: 50 <= x < 100),
        ("100-200", lambda x: 100 <= x < 200),
        ("200+", lambda x: x >= 200)
    ]
    print("Histogram (MAE):")
    for label, cond in hists:
        count = sum(1 for x in maes if cond(x))
        print(f"  {label:<8}: {count}")
