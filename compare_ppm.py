#!/usr/bin/env python3
import sys, math

def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()
    pos = 0
    def next_token():
        nonlocal pos
        while pos < len(data) and data[pos] in b' \t\r\n#':
            if data[pos:pos+1] == b'#':
                while pos < len(data) and data[pos] not in b'\r\n': pos += 1
            else:
                pos += 1
        tok = []
        while pos < len(data) and data[pos] not in b' \t\r\n':
            tok.append(bytes([data[pos]])); pos += 1
        return b''.join(tok)
    magic = next_token()
    w = int(next_token()); h = int(next_token()); maxv = int(next_token())
    if magic == b'P5': ch = 1
    elif magic == b'P6': ch = 3
    else: raise ValueError(magic)
    pix = data[pos:pos + w*h*ch]
    return w, h, ch, pix

def psnr(a, b, ch):
    n = len(a)//ch
    mse = 0.0
    for i in range(n):
        for c in range(ch):
            d = a[i*ch+c] - b[i*ch+c]
            mse += d*d
    mse /= (n*ch)
    if mse == 0: return float('inf')
    return 10*math.log10(255*255/mse)

def maxdiff(a, b, ch):
    md = 0
    for i in range(min(len(a), len(b))//ch):
        for c in range(ch):
            d = abs(a[i*ch+c]-b[i*ch+c])
            if d > md: md = d
    return md

def main():
    base = 'output_ppm_c89'
    refdir = 'output_ppm_dav1d'
    import os, glob
    for f in sorted(glob.glob(os.path.join(refdir, '*.ppm'))):
        b = os.path.basename(f)
        mine = os.path.join(base, b)
        if not os.path.exists(mine):
            print(f'{b}: MISSING')
            continue
        w,h,ch,refpix = read_ppm(f)
        w2,h2,ch2,mypix = read_ppm(mine)
        if (w,h,ch) != (w2,h2,ch2):
            print(f'{b}: size mismatch {w}x{h}x{ch} vs {w2}x{h2}x{ch2}')
            continue
        p = psnr(refpix, mypix, ch)
        md = maxdiff(refpix, mypix, ch)
        print(f'{b}: PSNR={p:7.2f} dB  maxdiff={md}')

if __name__ == '__main__':
    main()
