#!/usr/bin/env python3
"""Generate correct stb_av1_cdf_default_data[3602] in struct memory layout order."""
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

# Our struct field order with dims: (name, our_dims, dav1d_dims, section)
# our_dims = dimensions in our StbCdfContext
# dav1d_dims = dimensions in dav1d's CdfModeContext (what cdf.c fills)
# cdf_rows = list of (num_rows, values_per_row_from_cdf, row_width_in_dav1d)
# For proper padding: values from CDF go at start of each row, rest=0

# Instead of complex parsing, I'll place values field-by-field into a flat buffer
# Each field's flat buffer range is pre-computed

fields_order = [
    ('uv_mode', [2,13,15], [2,13,16]),
    ('partition', [5,4,16], [5,4,16]),
    ('cfl_alpha', [6,16], [6,16]),
    ('txtp_inter1', [2,16], [2,16]),
    ('txtp_inter2', [16], [16]),
    ('txtp_intra1', [2,13,8], [2,13,8]),
    ('txtp_intra2', [3,13,8], [3,13,8]),
    ('cfl_sign', [8], [8]),
    ('angle_delta', [8,8], [8,8]),
    ('filter_intra', [8], [8]),
    ('seg_id', [3,8], [3,8]),
    ('pal_sz', [2,7,8], [2,7,8]),
    ('color_map', [2,7,5,8], [2,7,5,8]),
    ('txsz', [3,3,4], [4,3,4]),
    ('delta_q', [4], [4]),
    ('delta_lf', [5,4], [5,4]),
    ('restore_switchable', [4], [4]),
    ('restore_wiener', [2], [2]),
    ('restore_sgrproj', [2], [2]),
    ('txtp_inter3', [4,2], [4,2]),
    ('use_filter_intra', [22,2], [22,2]),
    ('txpart', [7,3,2], [7,3,2]),
    ('skip', [3,2], [3,2]),
    ('pal_y', [7,3,2], [7,3,2]),
    ('pal_uv', [2,2], [2,2]),
    ('intrabc', [2], [2]),
    ('y_mode', [4,16], [4,16]),
    ('wedge_idx', [9,16], [9,16]),
    ('comp_inter_mode', [8,8], [8,8]),
    ('filter', [2,8,8], [2,8,4]),
    ('interintra_mode', [4,4], [4,4]),
    ('motion_mode', [22,4], [22,4]),
    ('skip_mode', [3,2], [3,2]),
    ('newmv_mode', [6,2], [6,2]),
    ('globalmv_mode', [2,2], [2,2]),
    ('refmv_mode', [6,2], [6,2]),
    ('drl_bit', [3,2], [3,2]),
    ('intra', [4,2], [4,2]),
    ('comp', [5,2], [5,2]),
    ('comp_dir', [5,2], [5,2]),
    ('jnt_comp', [6,2], [6,2]),
    ('mask_comp', [6,2], [6,2]),
    ('wedge_comp', [9,2], [9,2]),
    ('ref', [6,3,2], [6,3,2]),
    ('comp_fwd_ref', [3,3,2], [3,3,2]),
    ('comp_bwd_ref', [2,3,2], [2,3,2]),
    ('comp_uni_ref', [3,3,2], [3,3,2]),
    ('seg_pred', [3,2], [3,2]),
    ('interintra', [7,2], [7,2]),
    ('interintra_wedge', [7,2], [7,2]),
    ('obmc', [22,2], [22,2]),
    ('mv_classes', [16], [16]),
    ('mv_sign', [2], [2]),
    ('mv_class0', [2], [2]),
    ('mv_class0_fp', [2,4], [2,4]),
    ('mv_class0_hp', [2], [2]),
    ('mv_classN', [10,2], [10,2]),
    ('mv_classN_fp', [4], [4]),
    ('mv_classN_hp', [2], [2]),
    ('mv_joint', [4], [4]),
    ('kfym', [5,5,16], [5,5,16]),
]

mv_names = {'classes':'mv_classes','sign':'mv_sign','class0':'mv_class0',
            'class0_fp':'mv_class0_fp','class0_hp':'mv_class0_hp',
            'classN':'mv_classN','classN_fp':'mv_classN_fp',
            'classN_hp':'mv_classN_hp','joint':'mv_joint'}
mv_field_set = set(mv_names.values())

def dim_prod(d):
    r=1
    for x in d: r*=x
    return r

# Compute flat offsets
offsets={}
pos=0
for name,our,_ in fields_order:
    offsets[name]=pos
    pos+=dim_prod(our)
total=pos
buf=[0]*total
print(f"Total: {total}", file=sys.stderr)

def place_flat(flat_off, our_dims, values, dav1d_dims):
    """Place values into buf at flat_off, expanding for row padding.
    values = flat list from CDF macros, row by row.
    our_dims = our struct dimensions.
    dav1d_dims = dav1d's dimensions (CDF output format).
    """
    if our_dims == dav1d_dims:
        # Same dims, just copy
        for i,v in enumerate(values):
            buf[flat_off+i]=v
        return

    # For any mismatch, we need to handle row-by-row
    # Flatten both to a simple "rows of values" representation
    # dav1d: outer x mid x inner_dim
    # ours: outer x mid x our_inner_dim

    # Simple case: 3D with only last dim different
    if len(our_dims)==3 and len(dav1d_dims)==3 and our_dims[:2]==dav1d_dims[:2]:
        outer,mid = our_dims[0],our_dims[1]
        d_inner = dav1d_dims[2]
        o_inner = our_dims[2]
        idx=0
        for i in range(outer):
            for j in range(mid):
                # Copy min(d_inner, o_inner) values
                n = min(d_inner, o_inner, len(values)-idx)
                for k in range(n):
                    buf[flat_off + i*mid*o_inner + j*o_inner + k] = values[idx]
                    idx += 1
                # Skip remaining values in dav1d's wider row
                idx += max(0, d_inner - n)
        return

    # 3D with first dim different (txsz: [4][3][4] -> [3][3][4])
    if len(our_dims)==3 and len(dav1d_dims)==3 and our_dims[1:]==dav1d_dims[1:]:
        d_outer = dav1d_dims[0]
        o_outer = our_dims[0]
        mid = our_dims[1]
        inner = our_dims[2]
        # Skip first (d_outer - o_outer) planes
        skip = d_outer - o_outer
        vals_per_plane = mid * inner
        idx = skip * vals_per_plane
        for i in range(o_outer):
            for j in range(mid):
                for k in range(inner):
                    if idx < len(values):
                        buf[flat_off + i*mid*inner + j*inner + k] = values[idx]
                        idx += 1
        return

    # 2D with inner dim different (filter: [2][8][8] vs [2][8][4])
    # Actually this is 3D
    if len(our_dims)==3 and len(dav1d_dims)==3 and our_dims[0]==dav1d_dims[0] and our_dims[1]==dav1d_dims[1]:
        outer = our_dims[0]
        rows = our_dims[1]
        d_inner = dav1d_dims[2]
        o_inner = our_dims[2]
        idx=0
        for i in range(outer):
            for j in range(rows):
                n = min(d_inner, o_inner, len(values)-idx)
                for k in range(n):
                    buf[flat_off + i*rows*o_inner + j*o_inner + k] = values[idx]
                    idx += 1
                idx += max(0, d_inner - n)
        return

    # 1D, pad to our width
    if len(our_dims)==1 and len(dav1d_dims)==1:
        n=min(len(values),our_dims[0])
        for i in range(n):
            buf[flat_off+i]=values[i]
        return

    # 2D with inner dim same
    if len(our_dims)==2 and len(dav1d_dims)==2 and our_dims[1]==dav1d_dims[1]:
        for i,v in enumerate(values):
            buf[flat_off+i]=v
        return

    # Generic: just copy what fits
    n=min(len(values),dim_prod(our_dims))
    for i in range(n):
        buf[flat_off+i]=v
    print(f"  WARNING: generic fallback for dims {our_dims} vs {dav1d_dims}", file=sys.stderr)

# Process each field
for field_name, our_dims, dav1d_dims in fields_order:
    flat_off = offsets[field_name]
    
    if field_name in mv_field_set:
        section = MV
        dav1d_key = None
        for k,v in mv_names.items():
            if v==field_name: dav1d_key=k; break
        block = extract(section, dav1d_key or field_name)
    elif field_name == 'kfym':
        block = extract(init, 'kfym')
    else:
        block = extract(M, field_name)
    
    if block is None:
        print(f"MISSING: {field_name}", file=sys.stderr)
        continue
    
    # Check for indexed initialization [BS_*] = ...
    has_idx = bool(re.search(r'\[(BS_\w+|\d+)\]\s*=\s*\{', block))
    
    if has_idx:
        # Parse indexed entries
        our_total = dim_prod(our_dims)
        for m in re.finditer(r'\[(\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
            idx_name = m.group(1)
            idx = BS.get(idx_name)
            if idx is None:
                try: idx=int(idx_name)
                except: continue
            vals = all_cdf(m.group(2))
            # Place at the correct index in the field
            # Assume last dim is the row width
            if len(our_dims)==2:
                row_w = our_dims[1]
                for k,v in enumerate(vals):
                    if k < row_w and idx < our_dims[0]:
                        buf[flat_off + idx*row_w + k] = v
            elif len(our_dims)==1:
                for k,v in enumerate(vals):
                    if idx+k < our_dims[0]:
                        buf[flat_off + idx+k] = v
        n_set = sum(1 for x in buf[flat_off:flat_off+dim_prod(our_dims)] if x != 0)
        print(f"  {field_name}: indexed, {n_set}/{dim_prod(our_dims)} set", file=sys.stderr)
    else:
        vals = all_cdf(block)
        place_flat(flat_off, our_dims, vals, dav1d_dims)
        print(f"  {field_name}: {len(vals)} CDF vals, {our_dims} vs {dav1d_dims}", file=sys.stderr)

# Output
print("static const unsigned short stb_av1_cdf_default_data[] = {")
for i in range(0, total, 12):
    c = buf[i:min(i+12,total)]
    line = "    " + ",".join(f"{v:5d}" for v in c)
    if i < total: line += ","
    print(line)
print("};")
