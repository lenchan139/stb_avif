#!/usr/bin/env python3
"""Extract AV1 OBU stream from AVIF container -> raw .av1 files, plus tile data."""
import struct, os, sys

def read_uleb128(data, pos):
    v = 0; shift = 0; n = 0
    while True:
        b = data[pos]; pos += 1; n += 1
        v |= (b & 0x7f) << shift
        if not (b & 0x80): break
        shift += 7
    return v, pos, n

def extract_av1(path):
    data = open(path, 'rb').read()
    pos = 0
    while pos < len(data) - 8:
        size = struct.unpack('>I', data[pos:pos+4])[0]
        typ = data[pos+4:pos+8]
        hdr = 8
        if size == 1:
            size = struct.unpack('>Q', data[pos+8:pos+16])[0]
            hdr = 16
        if typ == b'mdat':
            return data[pos+hdr:pos+size], 'mdat'
        if size == 0: break
        pos += size
    return None, None

def parse_obus(data):
    """Return list of (type, payload_bytes) skipping OBU headers."""
    pos = 0; obus = []
    while pos < len(data):
        hdr = data[pos]
        obu_type = (hdr >> 3) & 0xf
        has_size = (hdr >> 1) & 1
        ext = (hdr >> 2) & 1
        pos += 1
        if ext:
            sz, pos, n = read_uleb128(data, pos)  # extension size
            pos += sz
        if has_size:
            sz, pos, n = read_uleb128(data, pos)
        else:
            sz = len(data) - pos
        obus.append((obu_type, data[pos:pos+sz]))
        pos += sz
    return obus

def main():
    for f in sorted(os.listdir('example_avif')):
        if not f.endswith('.avif'): continue
        av1, kind = extract_av1(os.path.join('example_avif', f))
        if av1 is None:
            print(f, 'NO MDAT'); continue
        obus = parse_obus(av1)
        os.makedirs('/tmp/av1bits', exist_ok=True)
        base = f[:-5]
        open(f'/tmp/av1bits/{base}.av1', 'wb').write(av1)
        # dump tile group payload
        tile = None
        for t, payload in obus:
            if t == 4:  # tile group
                tile = payload; break
            if t == 6:  # frame
                tile = payload; break
        if tile:
            open(f'/tmp/av1bits/{base}.tile', 'wb').write(tile)
        info = [(t, len(p)) for t, p in obus]
        print(f'{f}: {kind} obus={info}')

if __name__ == '__main__':
    main()
