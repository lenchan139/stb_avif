#!/usr/bin/env python3
"""Generate correct per-field CDF data for StbCdfContext from dav1d cdf.c."""
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
    rows = []
    depth = 0; content_at_depth = {}
    for ch in block:
        if ch == '{':
            depth += 1; content_at_depth[depth] = ''
        elif ch == '}':
            if depth in content_at_depth:
                text = content_at_depth[depth]
                if re.search(r'CDF\d+\(', text):
                    rows.append(all_cdf(text))
            depth -= 1
        elif depth >= 1:
            content_at_depth[depth] += ch
    if not rows:
        vals = all_cdf(block)
        if vals: rows.append(vals)
    return rows

def emit_array(name, flat, dims):
    """Emit a static const unsigned short array with proper nesting."""
    total = 1
    for d in dims: total *= d
    assert len(flat) == total, f"{name}: {len(flat)} != {total}"
    
    def fmt_flat(arr, dims, depth=0):
        if len(dims) == 1:
            return "{" + ",".join(f"{v:5d}" for v in arr) + "}"
        lines = ["{"]
        stride = 1
        for d in dims[1:]: stride *= d
        for i in range(dims[0]):
            sub = fmt_flat(arr[i*stride:(i+1)*stride], dims[1:], depth+1)
            sep = "\n" + "    " * (depth+1)
            lines.append(sep + sub + ("," if i < dims[0]-1 else ""))
        lines.append("\n" + "    " * depth + "}")
        return "".join(lines)
    
    dims_str = ",".join(str(d) for d in dims)
    print(f"static const unsigned short {name}[{dims_str}] = {fmt_flat(flat, dims)};")
    print()

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

# Fields: (name, our_dims, search_in)
# search_in: 'm' for .m section, 'mv' for .mv.comp, 'init' for top-level
fields = [
    ('uv_mode', [2,13,15], 'm'), ('partition', [5,4,16], 'm'),
    ('cfl_alpha', [6,16], 'm'), ('txtp_inter1', [2,16], 'm'),
    ('txtp_inter2', [16], 'm'), ('txtp_intra1', [2,13,8], 'm'),
    ('txtp_intra2', [3,13,8], 'm'), ('cfl_sign', [8], 'm'),
    ('angle_delta', [8,8], 'm'), ('filter_intra', [8], 'm'),
    ('seg_id', [3,8], 'm'), ('pal_sz', [2,7,8], 'm'),
    ('color_map', [2,7,5,8], 'm'), ('txsz', [3,3,4], 'm'),
    ('delta_q', [4], 'm'), ('delta_lf', [5,4], 'm'),
    ('restore_switchable', [4], 'm'), ('restore_wiener', [2], 'm'),
    ('restore_sgrproj', [2], 'm'), ('txtp_inter3', [4,2], 'm'),
    ('use_filter_intra', [22,2], 'm'), ('txpart', [7,3,2], 'm'),
    ('skip', [3,2], 'm'), ('pal_y', [7,3,2], 'm'),
    ('pal_uv', [2,2], 'm'), ('intrabc', [2], 'm'),
    ('y_mode', [4,16], 'm'), ('wedge_idx', [9,16], 'm'),
    ('comp_inter_mode', [8,8], 'm'), ('filter', [2,8,8], 'm'),
    ('interintra_mode', [4,4], 'm'), ('motion_mode', [22,4], 'm'),
    ('skip_mode', [3,2], 'm'), ('newmv_mode', [6,2], 'm'),
    ('globalmv_mode', [2,2], 'm'), ('refmv_mode', [6,2], 'm'),
    ('drl_bit', [3,2], 'm'), ('intra', [4,2], 'm'),
    ('comp', [5,2], 'm'), ('comp_dir', [5,2], 'm'),
    ('jnt_comp', [6,2], 'm'), ('mask_comp', [6,2], 'm'),
    ('wedge_comp', [9,2], 'm'), ('ref', [6,3,2], 'm'),
    ('comp_fwd_ref', [3,3,2], 'm'), ('comp_bwd_ref', [2,3,2], 'm'),
    ('comp_uni_ref', [3,3,2], 'm'), ('seg_pred', [3,2], 'm'),
    ('interintra', [7,2], 'm'), ('interintra_wedge', [7,2], 'm'),
    ('obmc', [22,2], 'm'),
    ('mv_classes', [16], 'mv'), ('mv_sign', [2], 'mv'),
    ('mv_class0', [2], 'mv'), ('mv_class0_fp', [2,4], 'mv'),
    ('mv_class0_hp', [2], 'mv'), ('mv_classN', [10,2], 'mv'),
    ('mv_classN_fp', [4], 'mv'), ('mv_classN_hp', [2], 'mv'),
    ('mv_joint', [4], 'mv'),
    ('kfym', [5,5,16], 'init'),
]

indexed_fields = {'use_filter_intra', 'motion_mode', 'obmc'}

print("/* Auto-generated per-field CDF data from dav1d cdf.c */")
print("/* Generated by gen_final2.py - DO NOT EDIT MANUALLY */")
print()

ok = 0; fail = 0
for field_name, our_dims, search_in in fields:
    our_total = 1
    for d in our_dims: our_total *= d
    our_inner = our_dims[-1]
    
    if search_in == 'mv':
        dav1d_key = None
        for k,v in mv_name_map.items():
            if v==field_name: dav1d_key=k; break
        block = extract_block(MV, dav1d_key or field_name)
    elif search_in == 'init':
        block = extract_block(init, field_name)
    else:
        block = extract_block(M, field_name)
    
    if block is None:
        print(f"/* WARNING: {field_name} not found */")
        fail += 1; continue
    
    if field_name in indexed_fields:
        if field_name == 'use_filter_intra':
            rows_data = [[0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 2: rows_data[idx][k] = v
        elif field_name == 'motion_mode':
            rows_data = [[0,0,0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 4: rows_data[idx][k] = v
        elif field_name == 'obmc':
            rows_data = [[0,0] for _ in range(22)]
            for m in re.finditer(r'\[(BS_\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
                idx = BS.get(m.group(1))
                if idx is None: continue
                vals = all_cdf(m.group(2))
                for k,v in enumerate(vals):
                    if k < 2: rows_data[idx][k] = v
        flat = []
        for r in rows_data: flat.extend(r)
    elif field_name == 'uv_mode':
        leaf_rows = extract_leaf_arrays(block)
        flat = []
        for i in range(2):
            for j in range(13):
                ri = i*13+j
                if ri < len(leaf_rows):
                    flat.extend(leaf_rows[ri][:our_inner] + [0]*max(0, our_inner - len(leaf_rows[ri])))
                else:
                    flat.extend([0]*our_inner)
    elif field_name == 'txsz':
        leaf_rows = extract_leaf_arrays(block)
        leaf_rows = leaf_rows[3:]  # Skip TX_4X4 level
        flat = []
        for row in leaf_rows:
            flat.extend(row[:our_inner] + [0]*max(0, our_inner-len(row)))
    elif field_name == 'filter':
        leaf_rows = extract_leaf_arrays(block)
        flat = []
        for row in leaf_rows:
            flat.extend(row[:our_inner] + [0]*max(0, our_inner-len(row)))
    else:
        leaf_rows = extract_leaf_arrays(block)
        flat = []
        for row in leaf_rows:
            flat.extend(row[:our_inner] + [0]*max(0, our_inner-len(row)))
        # If fewer values than expected (e.g., interintra has 4 rows but struct has 7), pad with zeros
        if len(flat) < our_total:
            flat.extend([0] * (our_total - len(flat)))
    
    try:
        assert len(flat) == our_total
        emit_array(f'stb_av1_{field_name}', flat, our_dims)
        ok += 1
    except AssertionError as e:
        print(f"/* FAIL {field_name}: {len(flat)} != {our_total} */")
        fail += 1

print(f"/* Summary: {ok} OK, {fail} FAIL */", file=sys.stderr)
