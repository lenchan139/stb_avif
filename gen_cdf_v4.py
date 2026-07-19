#!/usr/bin/env python3
"""
Generate correct per-field static arrays for StbCdfContext init.
Each field's values are padded per-row to match our struct's dimensions.
"""
import re, sys

BS = {'BS_128x128':0,'BS_128x64':1,'BS_64x128':2,'BS_64x64':3,'BS_64x32':4,'BS_64x16':5,
      'BS_32x64':6,'BS_32x32':7,'BS_32x16':8,'BS_32x8':9,'BS_16x64':10,'BS_16x32':11,
      'BS_16x16':12,'BS_16x8':13,'BS_16x4':14,'BS_8x32':15,'BS_8x16':16,'BS_8x8':17,
      'BS_8x4':18,'BS_4x16':19,'BS_4x8':20,'BS_4x4':21}

def cdf(s):
    a=[int(x.strip()) for x in s.split(',') if x.strip()]; return [32768-x for x in a]
def all_cdf(t): return [v for m in re.finditer(r'CDF(\d+)\(([^)]+)\)',t) for v in cdf(m.group(2))]

def extract(text,field):
    m=re.search(r'\.'+re.escape(field)+r'\s*=\s*\{',text)
    if not m: return None
    s=m.end()-1;d=1;i=s+1
    while i<len(text) and d>0:
        if text[i]=='{':d+=1
        elif text[i]=='}':d-=1
        i+=1
    return text[s+1:i-1]

def extract_rows(block):
    """Extract leaf-level arrays from nested braces.
    Returns list of lists of values, one per leaf array."""
    rows = []
    # Recursively find all CDF macro calls at each nesting level
    # Simple approach: split by top-level commas within each brace level
    # to separate individual leaf arrays
    depth = 0
    cur = ""
    for ch in block:
        if ch == '{':
            depth += 1
            if depth == 1:
                cur = ""
        elif ch == '}':
            depth -= 1
            if depth == 0:
                vals = all_cdf(cur)
                if vals:
                    rows.append(vals)
        elif depth >= 1:
            cur += ch
    return rows

def expand_rows_to_flat(rows, row_width):
    """Expand rows of CDF values to flat array with zero-padding per row."""
    result = []
    for row in rows:
        padded = row + [0] * (row_width - len(row))
        result.extend(padded)
    return result

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

# For each field, we need: name, our_dims, the row_width (last dim), and the number of leaf rows
# The structure of cdf.c nests = how the leaves map to dimensions

# Field specs: (name, our_dims, dav1d_last_dim, num_leaf_rows, section)
# Our struct puts values as: for each leaf row, pad to our_row_width
# For 3D: the middle dim gives rows per "plane", planes = first dim

# Approach: For each field, extract all leaf rows, then distribute them
# across our struct dimensions

# Our struct: (name, our_dims)
# cdf.c nesting tells us how leaf rows map to our struct dims
# Leaf row width = our_dims[-1] (innermost dim)

fields = [
    # (name, our_dims, section_is_mv)
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

mv_name_map = {'classes':'mv_classes','sign':'mv_sign','class0':'mv_class0',
               'class0_fp':'mv_class0_fp','class0_hp':'mv_class0_hp',
               'classN':'mv_classN','classN_fp':'mv_classN_fp',
               'classN_hp':'mv_classN_hp','joint':'mv_joint'}

# For fields with [BS_*] indexed initialization, handle separately
indexed_fields = {'use_filter_intra', 'motion_mode', 'obmc'}

# For txsz: dav1d has 4 planes (TX_4X4..TX_32X32), ours has 3 (skip TX_4X4)
# For uv_mode: dav1d has [16] innermost, ours has [15]
# For filter: dav1d has [4] innermost, ours has [8]

# Generate per-field static arrays
print("/* Auto-generated per-field CDF data from dav1d cdf.c */")
print("/* Each array matches StbCdfContext field dimensions exactly */")
print()

for field_name, our_dims, is_mv in fields:
    if is_mv:
        dav1d_key = None
        for k,v in mv_name_map.items():
            if v==field_name: dav1d_key=k; break
        block = extract(MV, dav1d_key or field_name)
    else:
        block = extract(M, field_name)
    
    if block is None:
        print(f"/* WARNING: {field_name} not found */")
        continue
    
    our_total = 1
    for d in our_dims: our_total *= d
    
    # Handle indexed fields
    if field_name in indexed_fields:
        if field_name == 'use_filter_intra':
            # [22][2]
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
        elif field_name == 'motion_mode':
            # [22][4]
            rows = [[0,0,0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 4: rows[idx][k] = v
            flat = []
            for r in rows: flat.extend(r)
            assert len(flat) == our_total
        elif field_name == 'obmc':
            # [22][2]
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
        
        # Emit as 1D array
        c_vals = ",".join(f"{v:5d}" for v in flat)
        print(f"static const unsigned short stb_av1_{field_name}[{our_total}] = {{")
        for i in range(0, our_total, 12):
            chunk = flat[i:min(i+12,our_total)]
            print("    " + ",".join(f"{v:5d}" for v in chunk) + ",")
        print("};")
        print()
        continue
    
    # For uv_mode: special handling due to [15] vs [16] dimension
    if field_name == 'uv_mode':
        rows = extract_rows(block)
        # rows = 26 leaf arrays: 13 for plane 0 (CDF12=12 vals), 13 for plane 1 (CDF13=13 vals)
        flat = []
        for i in range(2):
            for j in range(13):
                row_idx = i * 13 + j
                if row_idx < len(rows):
                    # Pad to 15 (our innermost dim)
                    flat.extend(rows[row_idx][:15])
                else:
                    flat.extend([0]*15)
        assert len(flat) == our_total
        c_vals = ",".join(f"{v:5d}" for v in flat)
        print(f"static const unsigned short stb_av1_{field_name}[{','.join(str(d) for d in our_dims)}] = {{")
        for i in range(0, our_total, 12):
            chunk = flat[i:min(i+12,our_total)]
            print("    " + ",".join(f"{v:5d}" for v in chunk) + ",")
        print("};")
        print()
        continue
    
    # For txsz: skip dav1d's TX_4X4 plane (first plane)
    if field_name == 'txsz':
        # dav1d: [4][3][4], ours: [3][3][4]
        # Extract all leaves, skip first 3 (plane 0 = TX_4X4, 3 rows)
        rows = extract_rows(block)
        # First 3 rows = TX_4X4 level, skip them
        rows = rows[3:]  # Start from TX_8X8 level
        flat = []
        row_width = our_dims[-1]  # 4
        for row in rows:
            padded = row[:row_width] + [0] * max(0, row_width - len(row))
            flat.extend(padded)
        # Should be 3*3*4 = 36 values
        assert len(flat) == our_total, f"txsz: {len(flat)} != {our_total}"
        print(f"static const unsigned short stb_av1_{field_name}[{','.join(str(d) for d in our_dims)}] = {{")
        for i in range(0, our_total, 12):
            chunk = flat[i:min(i+12,our_total)]
            print("    " + ",".join(f"{v:5d}" for v in chunk) + ",")
        print("};")
        print()
        continue
    
    # For filter: dav1d has [4] innermost, ours has [8]
    if field_name == 'filter':
        # dav1d: [2][8][4], ours: [2][8][8]
        rows = extract_rows(block)
        flat = []
        our_inner = our_dims[-1]  # 8
        for row in rows:
            padded = row[:our_inner] + [0] * max(0, our_inner - len(row))
            flat.extend(padded)
        assert len(flat) == our_total, f"filter: {len(flat)} != {our_total}"
        print(f"static const unsigned short stb_av1_{field_name}[{','.join(str(d) for d in our_dims)}] = {{")
        for i in range(0, our_total, 12):
            chunk = flat[i:min(i+12,our_total)]
            print("    " + ",".join(f"{v:5d}" for v in chunk) + ",")
        print("};")
        print()
        continue
    
    # General case: extract leaf rows and pad each to our innermost dim
    rows = extract_rows(block)
    flat = []
    our_inner = our_dims[-1]  # innermost dimension
    
    for row in rows:
        padded = row[:our_inner] + [0] * max(0, our_inner - len(row))
        flat.extend(padded)
    
    # Verify we have the right number of values
    if len(flat) != our_total:
        print(f"/* WARNING: {field_name}: got {len(flat)} values, expected {our_total} */", file=sys.stderr)
        # Pad with zeros
        flat.extend([0] * (our_total - len(flat)))
        flat = flat[:our_total]
    
    # Emit as nested C array matching our struct
    if len(our_dims) == 1:
        print(f"static const unsigned short stb_av1_{field_name}[{our_dims[0]}] = {{")
        print("    " + ",".join(f"{v:5d}" for v in flat) + ",")
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
        stride2 = our_dims[2] * our_dims[3]
        stride1 = our_dims[1] * stride2
        for i in range(our_dims[0]):
            print("    {")
            for j in range(our_dims[1]):
                print("        {")
                for k in range(our_dims[2]):
                    chunk = flat[i*stride1 + j*stride2 + k*our_dims[3]:i*stride1 + j*stride2 + (k+1)*our_dims[3]]
                    print("            {" + ",".join(f"{v:5d}" for v in chunk) + "},")
                print("        },")
            print("    },")
        print("};")
    print()
