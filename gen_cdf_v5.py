#!/usr/bin/env python3
"""
Generate correct per-field static arrays for StbCdfContext init.
v5: Fixed nested brace parsing for leaf-level extraction.
"""
import re, sys

BS = {'BS_128x128':0,'BS_128x64':1,'BS_64x128':2,'BS_64x64':3,'BS_64x32':4,'BS_64x16':5,
      'BS_32x64':6,'BS_32x32':7,'BS_32x16':8,'BS_32x8':9,'BS_16x64':10,'BS_16x32':11,
      'BS_16x16':12,'BS_16x8':13,'BS_16x4':14,'BS_8x32':15,'BS_8x16':16,'BS_8x8':17,
      'BS_8x4':18,'BS_4x16':19,'BS_4x8':20,'BS_4x4':21}

def cdf(s):
    a=[int(x.strip()) for x in s.split(',') if x.strip()]; return [32768-x for x in a]
def all_cdf(t): return [v for m in re.finditer(r'CDF(\d+)\(([^)]+)\)',t) for v in cdf(m.group(2))]

def extract_block(text, field):
    m=re.search(r'\.'+re.escape(field)+r'\s*=\s*\{',text)
    if not m: return None
    s=m.end()-1;d=1;i=s+1
    while i<len(text) and d>0:
        if text[i]=='{':d+=1
        elif text[i]=='}':d-=1
        i+=1
    return text[s+1:i-1]

def extract_leaf_arrays(block):
    """Extract leaf-level arrays from nested braces.
    A leaf array is a {} that contains CDF macros but no nested {} with CDF macros.
    Returns list of lists of values."""
    rows = []
    depth = 0
    content_at_depth = {}  # depth -> accumulated text
    
    for ch in block:
        if ch == '{':
            depth += 1
            content_at_depth[depth] = ""
        elif ch == '}':
            if depth in content_at_depth:
                text = content_at_depth[depth]
                # Check if this level contains CDF macros directly
                # (not just in sub-braces)
                # Remove all sub-brace content to check
                has_cdf = bool(re.search(r'CDF\d+\(', text))
                if has_cdf:
                    vals = all_cdf(text)
                    rows.append(vals)
            depth -= 1
        elif depth >= 1:
            content_at_depth[depth] += ch
    
    return rows

with open('dav1d_ref/cdf.c') as f: src=f.read()
s=src.find('static const CdfDefaultContext default_cdf = {')
d=0;e=s;fnd=False
for i in range(s,len(src)):
    if src[i]=='{':d+=1;fnd=True
    elif src[i]=='}':
        d-=1
        if fnd and d==0:e=i+1;break
init=src[s:e]
mv_s=init.find('.mv = {')
M=init[:mv_s]; MV=init[mv_s:]

mv_name_map = {'classes':'mv_classes','sign':'mv_sign','class0':'mv_class0',
               'class0_fp':'mv_class0_fp','class0_hp':'mv_class0_hp',
               'classN':'mv_classN','classN_fp':'mv_classN_fp',
               'classN_hp':'mv_classN_hp','joint':'mv_joint'}
mv_field_set = set(mv_name_map.values())

fields = [
    ('uv_mode', [2,13,15], False),
    ('partition', [5,4,16], False),
    ('cfl_alpha', [6,16], False),
    ('txtp_inter1', [2,16], False),
    ('txtp_inter2', [16], False),
    ('txtp_intra1', [2,13,8], False),
    ('txtp_intra2', [3,13,8], False),
    ('cfl_sign', [8], False),
    ('angle_delta', [8,8], False),
    ('filter_intra', [8], False),
    ('seg_id', [3,8], False),
    ('pal_sz', [2,7,8], False),
    ('color_map', [2,7,5,8], False),
    ('txsz', [3,3,4], False),
    ('delta_q', [4], False),
    ('delta_lf', [5,4], False),
    ('restore_switchable', [4], False),
    ('restore_wiener', [2], False),
    ('restore_sgrproj', [2], False),
    ('txtp_inter3', [4,2], False),
    ('use_filter_intra', [22,2], False),
    ('txpart', [7,3,2], False),
    ('skip', [3,2], False),
    ('pal_y', [7,3,2], False),
    ('pal_uv', [2,2], False),
    ('intrabc', [2], False),
    ('y_mode', [4,16], False),
    ('wedge_idx', [9,16], False),
    ('comp_inter_mode', [8,8], False),
    ('filter', [2,8,8], False),
    ('interintra_mode', [4,4], False),
    ('motion_mode', [22,4], False),
    ('skip_mode', [3,2], False),
    ('newmv_mode', [6,2], False),
    ('globalmv_mode', [2,2], False),
    ('refmv_mode', [6,2], False),
    ('drl_bit', [3,2], False),
    ('intra', [4,2], False),
    ('comp', [5,2], False),
    ('comp_dir', [5,2], False),
    ('jnt_comp', [6,2], False),
    ('mask_comp', [6,2], False),
    ('wedge_comp', [9,2], False),
    ('ref', [6,3,2], False),
    ('comp_fwd_ref', [3,3,2], False),
    ('comp_bwd_ref', [2,3,2], False),
    ('comp_uni_ref', [3,3,2], False),
    ('seg_pred', [3,2], False),
    ('interintra', [7,2], False),
    ('interintra_wedge', [7,2], False),
    ('obmc', [22,2], False),
    ('mv_classes', [16], True),
    ('mv_sign', [2], True),
    ('mv_class0', [2], True),
    ('mv_class0_fp', [2,4], True),
    ('mv_class0_hp', [2], True),
    ('mv_classN', [10,2], True),
    ('mv_classN_fp', [4], True),
    ('mv_classN_hp', [2], True),
    ('mv_joint', [4], True),
    ('kfym', [5,5,16], False),
]

indexed_fields = {'use_filter_intra', 'motion_mode', 'obmc'}

for field_name, our_dims, is_mv in fields:
    if is_mv:
        dav1d_key = None
        for k,v in mv_name_map.items():
            if v==field_name: dav1d_key=k; break
        block = extract_block(MV, dav1d_key or field_name)
    else:
        block = extract_block(M, field_name)
    
    if block is None:
        print(f"/* WARNING: {field_name} not found */")
        continue
    
    our_total = 1
    for d in our_dims: our_total *= d
    our_inner = our_dims[-1]
    
    # Handle indexed fields
    if field_name in indexed_fields:
        if field_name == 'use_filter_intra':
            rows = [[0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 2: rows[idx][k] = v
        elif field_name == 'motion_mode':
            rows = [[0,0,0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 4: rows[idx][k] = v
        elif field_name == 'obmc':
            rows = [[0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 2: rows[idx][k] = v
        flat = []
        for r in rows: flat.extend(r)
        assert len(flat) == our_total
        
        print(f"static const unsigned short stb_av1_{field_name}[{our_total}] = {{")
        for i in range(0, our_total, 12):
            chunk = flat[i:min(i+12,our_total)]
            print("    " + ",".join(f"{v:5d}" for v in chunk) + ",")
        print("};")
        print()
        continue
    
    # Special handling for uv_mode: [2][13][15] vs dav1d [2][13][16]
    if field_name == 'uv_mode':
        leaf_rows = extract_leaf_arrays(block)
        # Should be 26 leaf rows (13 per plane)
        flat = []
        for i in range(2):
            for j in range(13):
                row_idx = i * 13 + j
                if row_idx < len(leaf_rows):
                    flat.extend(leaf_rows[row_idx][:our_inner])
                else:
                    flat.extend([0]*our_inner)
        assert len(flat) == our_total
    elif field_name == 'txsz':
        # dav1d [4][3][4], ours [3][3][4] - skip first level
        leaf_rows = extract_leaf_arrays(block)
        # dav1d has 12 leaf rows (4 planes * 3 rows). Skip first 3 (TX_4X4)
        leaf_rows = leaf_rows[3:]  # Skip TX_4X4 level
        flat = []
        for row in leaf_rows:
            flat.extend(row[:our_inner] + [0]*max(0, our_inner - len(row)))
        assert len(flat) == our_total, f"txsz: {len(flat)} != {our_total}"
    elif field_name == 'filter':
        # dav1d [2][8][4], ours [2][8][8]
        leaf_rows = extract_leaf_arrays(block)
        flat = []
        for row in leaf_rows:
            flat.extend(row[:our_inner] + [0]*max(0, our_inner - len(row)))
        assert len(flat) == our_total, f"filter: {len(flat)} != {our_total}"
    else:
        # General case: extract leaf rows, pad each to our_inner
        leaf_rows = extract_leaf_arrays(block)
        flat = []
        for row in leaf_rows:
            flat.extend(row[:our_inner] + [0]*max(0, our_inner - len(row)))
        if len(flat) != our_total:
            print(f"/* {field_name}: leaf_rows={len(leaf_rows)}, flat={len(flat)}, expected={our_total} */", file=sys.stderr)
            # Pad or truncate
            if len(flat) < our_total:
                flat.extend([0] * (our_total - len(flat)))
            else:
                flat = flat[:our_total]
    
    # Emit as nested C array
    if len(our_dims) == 1:
        print(f"static const unsigned short stb_av1_{field_name}[{our_dims[0]}] = {{")
        print("    " + ",".join(f"{v:5d}" for v in flat[:our_dims[0]]) + ",")
        print("};")
    elif len(our_dims) == 2:
        print(f"static const unsigned short stb_av1_{field_name}[{our_dims[0]}][{our_dims[1]}] = {{")
        for i in range(our_dims[0]):
            chunk = flat[i*our_dims[1]:(i+1)*our_dims[1]]
            print("    {" + ",".join(f"{v:5d}" for v in chunk) + "},")
        print("};")
    elif len(our_dims) == 3:
        print(f"static const unsigned short stb_av1_{field_name}[{our_dims[0]}][{our_dims[1]}][{our_dims[2]}] = {{")
        stride = our_dims[1] * our_dims[2]
        for i in range(our_dims[0]):
            print("    {")
            for j in range(our_dims[1]):
                chunk = flat[i*stride + j*our_dims[2]:i*stride + (j+1)*our_dims[2]]
                print("        {" + ",".join(f"{v:5d}" for v in chunk) + "},")
            print("    },")
        print("};")
    elif len(our_dims) == 4:
        print(f"static const unsigned short stb_av1_{field_name}[{our_dims[0]}][{our_dims[1]}][{our_dims[2]}][{our_dims[3]}] = {{")
        s3 = our_dims[3]
        s2 = our_dims[2] * s3
        s1 = our_dims[1] * s2
        for i in range(our_dims[0]):
            print("    {")
            for j in range(our_dims[1]):
                print("        {")
                for k in range(our_dims[2]):
                    chunk = flat[i*s1 + j*s2 + k*s3 : i*s1 + j*s2 + (k+1)*s3]
                    print("            {" + ",".join(f"{v:5d}" for v in chunk) + "},")
                print("        },")
            print("    },")
        print("};")
    print()
