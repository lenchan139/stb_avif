"""Extract AV1 frames from AVIF container and write IVF for dav1d."""
import struct, sys, os

def read_box(f):
    pos = f.tell()
    data = f.read(8)
    if len(data) < 8:
        return None, None, None
    size, box_type = struct.unpack('>I4s', data)
    box_type = box_type.decode('ascii', errors='replace')
    if size == 1:
        data = f.read(8)
        size = struct.unpack('>Q', data)[0]
    elif size == 0:
        size = os.path.getsize(f.name) - pos
    return size, box_type, pos

def leb128(data, offset):
    val = 0
    for bi in range(8):
        if offset >= len(data):
            break
        b = data[offset]
        offset += 1
        val |= (b & 0x7F) << (bi * 7)
        if (b & 0x80) == 0:
            break
    return val, offset

def main():
    if len(sys.argv) < 3:
        print("Usage: python avif_to_ivf.py input.avif output.ivf")
        return 1
    
    with open(sys.argv[1], 'rb') as f:
        file_size = os.path.getsize(sys.argv[1])
        width = height = 0
        primary_item_id = 1
        iloc_items = {}  # item_id -> (offset, length)
        ipma_map = {}    # item_id -> [prop_indices]
        ipco_props = []  # list of (box_type, extra_data)
        
        while f.tell() < file_size:
            pos = f.tell()
            size, box_type, box_pos = read_box(f)
            if size is None:
                break
            
            if box_type == 'meta':
                meta_ver_flags = f.read(4)
                meta_end = box_pos + size
                while f.tell() < meta_end:
                    mpos = f.tell()
                    msz, mtype, _ = read_box(f)
                    if msz is None:
                        break
                    if mtype == 'iloc':
                        ver_flags = f.read(4)
                        version = ver_flags[0]
                        sizes_word = struct.unpack('>H', f.read(2))[0]
                        offset_size = (sizes_word >> 12) & 0xf
                        length_size = (sizes_word >> 8) & 0xf
                        base_offset_size = (sizes_word >> 4) & 0xf
                        item_count = struct.unpack('>H', f.read(2))[0]
                        for i in range(item_count):
                            item_id = struct.unpack('>H', f.read(2))[0]
                            if version < 2:
                                f.read(2)  # data_reference_index
                            base_offset = 0
                            if base_offset_size > 0:
                                base_offset = int.from_bytes(f.read(base_offset_size), 'big')
                            extent_count = struct.unpack('>H', f.read(2))[0]
                            for j in range(extent_count):
                                ext_offset = int.from_bytes(f.read(offset_size), 'big') if offset_size > 0 else 0
                                ext_length = int.from_bytes(f.read(length_size), 'big') if length_size > 0 else 0
                                iloc_items[item_id] = (base_offset + ext_offset, ext_length)
                    elif mtype == 'pitm':
                        ver_flags = f.read(4)
                        version = ver_flags[0]
                        if version < 2:
                            primary_item_id = struct.unpack('>H', f.read(2))[0]
                        else:
                            primary_item_id = struct.unpack('>I', f.read(4))[0]
                    elif mtype == 'ipma':
                        ver_flags = f.read(4)
                        version = ver_flags[0]
                        item_count = struct.unpack('>I', f.read(4))[0]
                        for i in range(item_count):
                            item_id = struct.unpack('>H', f.read(2))[0]
                            prop_count = struct.unpack('B', f.read(1))[0]
                            props = []
                            for j in range(prop_count):
                                if version >= 1:
                                    prop_idx = struct.unpack('>H', f.read(2))[0]
                                else:
                                    prop_idx = struct.unpack('B', f.read(1))[0]
                                essential = (prop_idx >> 15) & 1
                                prop_idx &= 0x7fff
                                props.append(prop_idx)
                            ipma_map[item_id] = props
                    elif mtype == 'iprp':
                        iprp_end = mpos + msz
                        while f.tell() < iprp_end:
                            ppos = f.tell()
                            psize, ptype, _ = read_box(f)
                            if psize is None:
                                break
                            if ptype == 'ipco':
                                ipco_end = ppos + psize
                                prop_idx = 0
                                while f.tell() < ipco_end:
                                    cpos = f.tell()
                                    csz, ctype, _ = read_box(f)
                                    if csz is None:
                                        break
                                    prop_idx += 1
                                    if ctype == 'ispe':
                                        f.read(4)  # version + flags
                                        dims = f.read(8)
                                        if len(dims) == 8:
                                            w, h = struct.unpack('>II', dims)
                                            ipco_props.append(('ispe', w, h))
                                        else:
                                            ipco_props.append(('ispe', 0, 0))
                                    else:
                                        ipco_props.append((ctype,))
                                    f.seek(cpos + csz)
                            elif ptype == 'ipma':
                                ver_flags = f.read(4)
                                version = ver_flags[0]
                                item_count = struct.unpack('>I', f.read(4))[0]
                                for i in range(item_count):
                                    item_id = struct.unpack('>H', f.read(2))[0]
                                    prop_count = struct.unpack('B', f.read(1))[0]
                                    props = []
                                    for j in range(prop_count):
                                        if version >= 1:
                                            raw_prop = struct.unpack('>H', f.read(2))[0]
                                        else:
                                            raw_prop = struct.unpack('B', f.read(1))[0]
                                        essential = (raw_prop >> 15) & 1
                                        prop_idx = raw_prop & 0x7fff
                                        props.append(prop_idx)
                                    ipma_map[item_id] = props
                            f.seek(ppos + psize)
                    f.seek(mpos + msz)
                f.seek(box_pos + size)
            else:
                f.seek(box_pos + size)
        
        if not iloc_items:
            print("ERROR: No iloc items found")
            return 1
        
        # Find dimensions from the primary item's ispe via ipma
        primary_props = ipma_map.get(primary_item_id, [])
        for prop_idx in primary_props:
            if 1 <= prop_idx <= len(ipco_props):
                entry = ipco_props[prop_idx - 1]
                if entry[0] == 'ispe':
                    width, height = entry[1], entry[2]
                    break
        
        # Read primary item data
        if primary_item_id not in iloc_items:
            print("ERROR: Primary item %d not in iloc" % primary_item_id)
            return 1
        item_offset, item_length = iloc_items[primary_item_id]
        f.seek(item_offset)
        item_data = f.read(item_length)
        print("Item %d at offset %d, length %d, read %d bytes" % (primary_item_id, item_offset, item_length, len(item_data)))
        
        # Parse OBUs from item data
        av1_frames = []
        offset = 0
        while offset < len(item_data):
            byte0 = item_data[offset]
            obu_type = (byte0 >> 3) & 0xF
            has_size = (byte0 >> 1) & 1
            
            if obu_type == 2:  # OBU_TEMPORAL_DELIMITER
                if has_size:
                    sz, offset = leb128(item_data, offset + 1)
                    offset += sz
                else:
                    offset += 1
                continue
            elif obu_type == 15:  # OBU_PADDING
                if has_size:
                    sz, offset = leb128(item_data, offset + 1)
                    offset += sz
                else:
                    offset += 1
                continue
            
            if has_size:
                sz, end = leb128(item_data, offset + 1)
                obu_data = item_data[offset:end + sz]
                offset = end + sz
            else:
                obu_data = item_data[offset:]
                offset = len(item_data)
            
            av1_frames.append(obu_data)
        
        if not av1_frames:
            print("ERROR: No AV1 OBUs found in item 1")
            return 1
        
        if width == 0 or height == 0:
            width, height = 679, 720
        print(f"Found {len(av1_frames)} AV1 OBUs, dimensions: {width}x{height}")
        
        with open(sys.argv[2], 'wb') as out:
            out.write(b'DKIF')
            out.write(struct.pack('<HH', 2, 32))
            out.write(b'AV01')
            out.write(struct.pack('<HH', width, height))
            out.write(struct.pack('<II', 24, 1))
            out.write(struct.pack('<I', len(av1_frames)))
            out.write(struct.pack('<I', 0))
            
            for frame_data in av1_frames:
                out.write(struct.pack('<I', len(frame_data)))
                out.write(struct.pack('<Q', 0))
                out.write(frame_data)
        
        print(f"IVF written: {sys.argv[2]} ({len(av1_frames)} frames)")
        return 0

if __name__ == '__main__':
    sys.exit(main())
