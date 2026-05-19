import av, numpy as np, subprocess, os, sys

def analyze(filename, dump_file):
    print(f"Analyzing {filename}...")
    env = os.environ.copy()
    env["STBI_AVIF_DUMP_YUV"] = dump_file
    subprocess.run(["./tests/test_decode", filename, "/tmp/dummy.ppm"], env=env, capture_output=True)

    container = av.open(filename)
    frame = next(container.decode(video=0))
    plane = frame.planes[0]
    w, h = frame.width, frame.height
    
    # GT for 8-bit is uint8
    if frame.format.name == 'yuv420p':
        gt = np.frombuffer(bytes(plane), dtype=np.uint8).reshape(h, plane.line_size)[:, :w].astype(int)
    else:
        # Just in case it's 10-bit or something else, but the task says 8-bit
        gt = np.frombuffer(bytes(plane), dtype=np.uint16).reshape(h, plane.line_size // 2)[:, :w]
        if gt.max() > 1023: gt = gt >> 6
        gt = gt.astype(int)

    # Our dump is always uint16
    ours = np.fromfile(dump_file, dtype=np.uint16)[:h*w].reshape(h, w).astype(int)

    print(f"GT max={gt.max()} min={gt.min()} mean={gt.mean():.1f}")
    print(f"OURS max={ours.max()} min={ours.min()} mean={ours.mean():.1f}")

    print("Row 0 cols 0..127 cell-by-cell diff:")
    for c in range(128):
        d = ours[0,c] - gt[0,c]
        print(f"  c={c:3d} o={ours[0,c]:3d} g={gt[0,c]:3d} d={d:+d}")

    for c in range(w):
        if abs(ours[0,c] - gt[0,c]) > 5:
            print(f"First col where row 0 |diff|>5: col={c}")
            break

    print("Per-row MAE for rows 0-15:")
    for r in range(16):
        mae = np.mean(np.abs(ours[r,:] - gt[r,:]))
        print(f"  row {r:2d}: {mae:.2f}")

    print("Top 5 worst 64x64 SBs:")
    sb_mae = []
    for sr in range(0, h, 64):
        for sc in range(0, w, 64):
            block_o = ours[sr:sr+64, sc:sc+64]
            block_g = gt[sr:sr+64, sc:sc+64]
            if block_o.shape == (64, 64):
                m = np.mean(np.abs(block_o - block_g))
                sb_mae.append((m, sr, sc))
    sb_mae.sort(key=lambda x: x[0], reverse=True)
    for m, sr, sc in sb_mae[:5]:
        print(f"  SB ({sc:4d},{sr:4d}): MAE={m:.2f}")
    print("-" * 40)

analyze("example_avif/fox.profile0.8bpc.yuv420.avif", "/tmp/fox8_ours.bin")
# Assuming kimono file name based on pattern
kimono_path = "example_avif/kimono.avif"
if os.path.exists(kimono_path):
    analyze(kimono_path, "/tmp/kimono_ours.bin")
else:
    # Try looking for kimono file
    files = [f for f in os.listdir("example_avif") if "kimono" in f and "8bpc" in f]
    if files:
        analyze(os.path.join("example_avif", files[0]), "/tmp/kimono_ours.bin")
    else:
        print("Kimono file not found")
