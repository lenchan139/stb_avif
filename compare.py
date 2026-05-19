import av, numpy as np
try:
    c=av.open('example_avif/fox.profile0.10bpc.yuv420.avif')
    f=next(c.decode(video=0))
    W,H=f.width,f.height
    Y=np.frombuffer(f.planes[0],dtype=np.uint16).reshape(-1, f.planes[0].line_size//2)[:H,:W].astype(int)
    data = np.frombuffer(open('/tmp/our_yuv.bin','rb').read(), dtype=np.uint16)
    y_ours = data[:H*W].reshape(H,W).astype(int)
    print('Second TX (cols 8-15, rows 0-7) diff pattern:')
    for r in range(8):
        diffs = (y_ours[r,8:16] - Y[r,8:16]).tolist()
        print(f'  row {r}: ours={y_ours[r,8:16].tolist()} gt={Y[r,8:16].tolist()} diff={diffs} mean={sum(diffs)/8:+.1f}')
    print()
    print('First TX (cols 0-7, rows 0-7) diff pattern:')
    for r in range(8):
        diffs = (y_ours[r,0:8] - Y[r,0:8]).tolist()
        print(f'  row {r}: diff={diffs} mean={sum(diffs)/8:+.1f}')
except Exception as e:
    print(e)
