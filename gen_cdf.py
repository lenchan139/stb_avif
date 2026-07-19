#!/usr/bin/env python3
"""Generate correct stb_av1_cdf_default_data in struct memory layout order."""
import re, sys

BS = {
    'BS_128x128': 0, 'BS_128x64': 1, 'BS_64x128': 2, 'BS_64x64': 3,
    'BS_64x32': 4, 'BS_64x16': 5, 'BS_32x64': 6, 'BS_32x32': 7,
    'BS_32x16': 8, 'BS_32x8': 9, 'BS_16x64': 10, 'BS_16x32': 11,
    'BS_16x16': 12, 'BS_16x8': 13, 'BS_16x4': 14, 'BS_8x32': 15,
    'BS_8x16': 16, 'BS_8x8': 17, 'BS_8x4': 18, 'BS_4x16': 19,
    'BS_4x8': 20, 'BS_4x4': 21,
}

def cdf_n(args_str):
    args = [int(a.strip()) for a in args_str.split(',') if a.strip()]
    return [32768 - x for x in args]

def find_all_cdf(text):
    vals = []
    for m in re.finditer(r'CDF(\d+)\(([^)]+)\)', text):
        vals.extend(cdf_n(m.group(2)))
    return vals

def find_cdf_per_line(text):
    """Return list of (line_values) for each line containing CDF macros."""
    result = []
    for line in text.split('\n'):
        v = find_all_cdf(line)
        if v:
            result.append(v)
    return result

def extract_field_block(text, field_name):
    """Extract text from .field = { through matching }."""
    pat = r'\.' + re.escape(field_name) + r'\s*=\s*\{'
    m = re.search(pat, text)
    if not m:
        return None, None
    start = m.start()
    brace_start = m.end() - 1
    depth = 1; i = brace_start + 1
    while i < len(text) and depth > 0:
        if text[i] == '{': depth += 1
        elif text[i] == '}': depth -= 1
        i += 1
    return text[start:i], text[brace_start+1:i-1]

with open('dav1d_ref/cdf.c') as f:
    cdf_src = f.read()

start = cdf_src.find('static const CdfDefaultContext default_cdf = {')
assert start >= 0
depth = 0; end = start; found = False
for i in range(start, len(cdf_src)):
    if cdf_src[i] == '{': depth += 1; found = True
    elif cdf_src[i] == '}':
        depth -= 1
        if found and depth == 0: end = i + 1; break
init = cdf_src[start:end]

mv_start = init.find('.mv = {')
assert mv_start >= 0
m_section = init[:mv_start]
mv_section = init[mv_start:]

fields = [
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

mv_field_names = {'mv_classes','mv_sign','mv_class0','mv_class0_fp',
                  'mv_class0_hp','mv_classN','mv_classN_fp','mv_classN_hp','mv_joint'}
mv_name_map = {'classes':'mv_classes','sign':'mv_sign','class0':'mv_class0',
               'class0_fp':'mv_class0_fp','class0_hp':'mv_class0_hp',
               'classN':'mv_classN','classN_fp':'mv_classN_fp',
               'classN_hp':'mv_classN_hp','joint':'mv_joint'}

def dim_prod(d):
    r = 1
    for x in d: r *= x
    return r

offsets = {}
pos = 0
for name, our_dims, _ in fields:
    offsets[name] = pos
    pos += dim_prod(our_dims)

print(f"Total struct entries: {pos}", file=sys.stderr)
flat_data = [0] * pos

def place_sequential(vals, flat_off, our_dims, dav1d_dims):
    """Place values sequentially, handling row padding if dims differ."""
    if our_dims == dav1d_dims:
        n = min(len(vals), dim_prod(our_dims))
        for i in range(n):
            flat_data[flat_off + i] = vals[i]
        return
    
    # For uv_mode: dav1d=[2][13][16], ours=[2][13][15]
    # Values are in CDF12 (12 values) and CDF13 (13 values) format
    # They're stored sequentially, filling the innermost dimension first
    # But the innermost dim in dav1d is [16] while ours is [15]
    # CDF12 gives 12 values, padded to 16 with 4 zeros (count+padding)
    # CDF13 gives 13 values, padded to 16 with 3 zeros
    
    if len(dav1d_dims) == len(our_dims) and len(our_dims) == 3:
        # Handle [2][13][16] -> [2][13][15]
        d_out_dim = dav1d_dims[2]  # 16
        o_out_dim = our_dims[2]    # 15
        outer = dav1d_dims[0]      # 2
        mid = dav1d_dims[1]        # 13
        
        val_idx = 0
        for i in range(outer):
            for j in range(mid):
                for k in range(o_out_dim):
                    if val_idx < len(vals):
                        flat_data[flat_off + i*our_dims[1]*o_out_dim + j*o_out_dim + k] = vals[val_idx]
                        val_idx += 1
                # Skip the padding value in dav1d's [16] dim
                if val_idx < len(vals):
                    val_idx += 1  # skip the 16th element (count/padding)
        return
    
    # Default: sequential
    n = min(len(vals), dim_prod(our_dims))
    for i in range(n):
        flat_data[flat_off + i] = vals[i]

def place_indexed(block_text, flat_off, our_dims):
    """Handle [BS_XX] = { ... } designated initializers."""
    for m in re.finditer(r'\[(\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block_text):
        idx_name = m.group(1)
        idx = BS.get(idx_name)
        if idx is None:
            try: idx = int(idx_name)
            except: continue
        inner_vals = find_all_cdf(m.group(2))
        if not inner_vals:
            continue
        
        ndims = len(our_dims)
        if ndims == 2:
            # use_filter_intra[22][2], motion_mode[22][4], etc.
            row = idx
            for k, v in enumerate(inner_vals):
                if k < our_dims[1]:
                    flat_data[flat_off + row * our_dims[1] + k] = v
        elif ndims == 1:
            for k, v in enumerate(inner_vals):
                flat_data[flat_off + idx + k] = v
        elif ndims == 3:
            row = idx
            row_size = our_dims[2]
            for k, v in enumerate(inner_vals):
                if k < row_size:
                    flat_data[flat_off + row * row_size + k] = v

for field_name, our_dims, dav1d_dims in fields:
    flat_off = offsets[field_name]
    our_total = dim_prod(our_dims)
    dav1d_total = dim_prod(dav1d_dims)
    
    if field_name in mv_field_names:
        src = mv_section
        dav1d_mv = None
        for k, v in mv_name_map.items():
            if v == field_name:
                dav1d_mv = k; break
        block_text, block_inner = extract_field_block(src, dav1d_mv or field_name)
    elif field_name == 'kfym':
        block_text, block_inner = extract_field_block(init, 'kfym')
    else:
        block_text, block_inner = extract_field_block(m_section, field_name)
    
    if block_text is None:
        print(f"WARNING: field '{field_name}' not found", file=sys.stderr)
        continue
    
    # Check for indexed initialization
    has_indices = re.search(r'\[BS_\w+\]\s*=\s*\{', block_text) or re.search(r'\[\d+\]\s*=\s*\{', block_text)
    
    if has_indices:
        # Initialize zeros first, then place indexed values
        place_indexed(block_text, flat_off, our_dims)
        n_set = sum(1 for x in flat_data[flat_off:flat_off+our_total] if x != 0)
        print(f"  {field_name}: indexed, {n_set}/{our_total} set", file=sys.stderr)
    else:
        vals = find_all_cdf(block_inner)
        # For fields where CDF gives fewer values than array width, pad with zeros
        # e.g., CDF12 gives 12 values but row is [16] wide -> pad 4 zeros
        place_sequential(vals, flat_off, our_dims, dav1d_dims)
        print(f"  {field_name}: sequential, {len(vals)} CDF values -> {our_total} slots (dav1d={dav1d_dims})", file=sys.stderr)

# Output
print("static const unsigned short stb_av1_cdf_default_data[] = {")
for i in range(0, pos, 12):
    chunk = flat_data[i:min(i+12, pos)]
    line = "    " + ",".join(f"{v:5d}" for v in chunk)
    if i < pos: line += ","
    print(line)
print("};")
