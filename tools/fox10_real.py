import av, numpy as np, subprocess, os
env = os.environ.copy()
env["STBI_AVIF_DUMP_YUV"] = "/tmp/fox10_ours.bin"
subprocess.run(["./tests/test_decode", "example_avif/fox.profile0.10bpc.yuv420.avif", "/tmp/dummy.ppm"], env=env, capture_output=True)

container = av.open("example_avif/fox.profile0.10bpc.yuv420.avif")
frame = next(container.decode(video=0))
plane = frame.planes[0]
w, h = plane.width, plane.height
stride = plane.line_size // 2
gt = np.frombuffer(bytes(plane), dtype=np.uint16).reshape(h, stride)[:, :w].copy()
# scale: detect — pyav often returns shifted; if max > 1023, right-shift by 6
if gt.max() > 1023:
    gt = gt >> 6

ours = np.fromfile("/tmp/fox10_ours.bin", dtype=np.uint16)[:h*w].reshape(h, w)

print(f"GT max={gt.max()} min={gt.min()} mean={gt.mean():.1f}")
print(f"OURS max={ours.max()} min={ours.min()} mean={ours.mean():.1f}")

# Row 0 first 80 cols
print("Row 0 cols 0..79:")
for c in range(80):
    d = int(ours[0,c]) - int(gt[0,c])
    print(f"  c={c:3d} ours={ours[0,c]:4d} gt={gt[0,c]:4d} diff={d:+d}")

# First col where |diff|>20 in row 0
for c in range(w):
    if abs(int(ours[0,c]) - int(gt[0,c])) > 20:
        print(f"First |diff|>20 in row 0: col={c}")
        break

# Per-row MAE rows 0..40
print("Per-row MAE:")
for r in range(40):
    mae = np.mean(np.abs(ours[r,:].astype(int) - gt[r,:].astype(int)))
    print(f"  row {r}: {mae:.2f}")

# Find spatial hot spots (64x64 grid)
print("\nWorst 10 64x64 SBs:")
sb_mae = []
for sr in range(0, h, 64):
    for sc in range(0, w, 64):
        block_o = ours[sr:sr+64, sc:sc+64].astype(int)
        block_g = gt[sr:sr+64, sc:sc+64].astype(int)
        m = np.mean(np.abs(block_o - block_g))
        sb_mae.append((m, sr, sc))
sb_mae.sort(reverse=True)
for m, sr, sc in sb_mae[:10]:
    print(f"  SB ({sc:4d},{sr:4d}): MAE={m:.2f}")
