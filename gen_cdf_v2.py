#!/usr/bin/env python3
"""
Generate correct per-field static const arrays + init function for StbCdfContext.
Each field's array matches our struct's dimensions exactly.
"""
import re, sys, textwrap

BS = {
    'BS_128x128': 0, 'BS_128x64': 1, 'BS_64x128': 2, 'BS_64x64': 3,
    'BS_64x32': 4, 'BS_64x16': 5, 'BS_32x64': 6, 'BS_32x32': 7,
    'BS_32x16': 8, 'BS_32x8': 9, 'BS_16x64': 10, 'BS_16x32': 11,
    'BS_16x16': 12, 'BS_16x8': 13, 'BS_16x4': 14, 'BS_8x32': 15,
    'BS_8x16': 16, 'BS_8x8': 17, 'BS_8x4': 18, 'BS_4x16': 19,
    'BS_4x8': 20, 'BS_4x4': 21,
}

def cdf_val(s):
    """CDFN(a,b,...) -> list of uint16_t values."""
    args = [int(x.strip()) for x in s.split(',') if x.strip()]
    return [32768 - x for x in args]

def find_cdf(text):
    return [v for m in re.finditer(r'CDF(\d+)\(([^)]+)\)', text) for v in cdf_val(m.group(2))]

def extract_block(text, field):
    """Find .field = { ... } and return inner text."""
    m = re.search(r'\.' + re.escape(field) + r'\s*=\s*\{', text)
    if not m: return None
    start = m.end() - 1
    depth = 1; i = start + 1
    while i < len(text) and depth > 0:
        if text[i] == '{': depth += 1
        elif text[i] == '}': depth -= 1
        i += 1
    return text[start+1:i-1]

def extract_indexed_values(block):
    """Extract [idx] = { values } pairs from a block."""
    result = {}
    for m in re.finditer(r'\[(\w+)\]\s*=\s*\{([^}]*(?:CDF\d+\([^)]+\)[^}]*)*)\}', block):
        idx = BS.get(m.group(1))
        if idx is None:
            try: idx = int(m.group(1))
            except: continue
        vals = find_cdf(m.group(2))
        if vals: result[idx] = vals
    return result

def pad_row(vals, width):
    """Pad a list of values to 'width' with zeros."""
    return vals + [0] * (width - len(vals))

def flat_array_1d(name, values, width):
    """Generate C code for a 1D array: unsigned short name[width] = { ... };"""
    padded = pad_row(values, width)
    return f"static const unsigned short {name}[{width}] = {{\n    {','.join(f'{v:5d}' for v in padded)}\n}};"

def flat_array_2d(name, rows, row_width):
    """Generate C code for a 2D array."""
    lines = [f"static const unsigned short {name}[{len(rows)}][{row_width}] = {{"]
    for r in rows:
        padded = pad_row(r, row_width)
        lines.append("    {" + ",".join(f"{v:5d}" for v in padded) + "},")
    lines.append("};")
    return "\n".join(lines)

def flat_array_3d(name, planes, plane_rows, row_width):
    """Generate C code for a 3D array."""
    lines = [f"static const unsigned short {name}[{len(planes)}][{plane_rows}][{row_width}] = {{"]
    for p_idx, plane in enumerate(planes):
        lines.append("    {")
        for r in plane:
            padded = pad_row(r, row_width)
            lines.append("        {" + ",".join(f"{v:5d}" for v in padded) + "},")
        lines.append("    },")
    lines.append("};")
    return "\n".join(lines)

# Read cdf.c
with open('dav1d_ref/cdf.c') as f:
    cdf_src = f.read()

start = cdf_src.find('static const CdfDefaultContext default_cdf = {')
depth = 0; end = start; found = False
for i in range(start, len(cdf_src)):
    if cdf_src[i] == '{': depth += 1; found = True
    elif cdf_src[i] == '}':
        depth -= 1
        if found and depth == 0: end = i + 1; break
init = cdf_src[start:end]
mv_start = init.find('.mv = {')
m_section = init[:mv_start]
mv_section = init[mv_start:]

out = []
def emit(s=""): out.append(s)

# ==================== uv_mode ====================
# dav1d: [2][13][16] -> ours: [2][13][15]
# [0] = 13 rows of CDF12, [1] = 13 rows of CDF13
block = extract_block(m_section, 'uv_mode')
# Parse inner blocks for [0] and [1]
idx0 = block.find('{', block.find('= {') + 1)
# Get [0] plane and [1] plane
planes_text = block
# Find two top-level inner blocks (one for each plane)
brace_count = 0; planes = []; cur = ""
i = block.index('{') + 1
plane_start = i
while i < len(block):
    if block[i] == '{': brace_count += 1
    elif block[i] == '}':
        if brace_count == 0:
            planes.append(block[plane_start:i])
            plane_start = i + 2  # skip ", "
            i += 1
        brace_count -= 1
    i += 1

uv0_rows = []  # [0]: 13 rows of 15 values (from CDF12)
uv1_rows = []  # [1]: 13 rows of 15 values (from CDF13)
for pi, plane_text in enumerate(planes[:2]):
    # Each plane has 13 rows
    rows = []
    brace_count = 0; cur_row = ""
    i = plane_text.index('{') + 1 if '{' in plane_text else 0
    while i < len(plane_text):
        if plane_text[i] == '{':
            brace_count += 1
            if brace_count == 1:
                cur_row = ""
        elif plane_text[i] == '}':
            if brace_count == 1:
                vals = find_cdf(cur_row)
                rows.append(vals)
            brace_count -= 1
        elif brace_count >= 1:
            cur_row += plane_text[i]
        i += 1
    if pi == 0:
        uv0_rows = rows
    else:
        uv1_rows = rows

emit("/* uv_mode: dav1d [2][13][16], ours [2][13][15] */")
emit(flat_array_3d('stb_av1_uv_mode', [uv0_rows, uv1_rows], 13, 15))
emit()

# ==================== partition ====================
# dav1d: [5][4][16], ours: [5][4][16]
block = extract_block(m_section, 'partition')
# 5 levels, each with 4 rows of [16]
part_levels = []
brace_count = 0; i = block.index('{') + 1
level_text = ""; level_depth = 0
while i < len(block):
    if block[i] == '{':
        if level_depth == 0: level_text = ""
        level_depth += 1
    elif block[i] == '}':
        level_depth -= 1
        if level_depth == 0:
            # Parse 4 rows from level_text
            rows = []
            bc = 0; cur = ""
            j = 0
            while j < len(level_text):
                if level_text[j] == '{':
                    bc += 1
                    if bc == 1: cur = ""
                elif level_text[j] == '}':
                    if bc == 1:
                        vals = find_cdf(cur)
                        rows.append(vals)
                    bc -= 1
                elif bc >= 1:
                    cur += level_text[j]
                j += 1
            part_levels.append(rows)
    elif level_depth >= 1:
        level_text += block[i]
    i += 1

emit("/* partition: [5][4][16] */")
emit(flat_array_3d('stb_av1_partition', part_levels, 4, 16))
emit()

# ==================== Simple fields (1D or small) ====================
# For fields that are simple CDF1/2/3/4/6/7 etc per row

def process_1d(field_name, our_width, dav1d_width=None):
    """Process a 1D field where each row has CDF values padded to our_width."""
    block = extract_block(m_section, field_name)
    if block is None:
        return None
    vals = find_cdf(block)
    # Pad to our_width
    return pad_row(vals, our_width)

def process_2d(field_name, rows, our_row_width, dav1d_row_width=None):
    """Process a 2D field."""
    block = extract_block(m_section, field_name)
    if block is None:
        return None
    # Find individual rows
    row_list = []
    bc = 0; cur = ""
    i = 0
    while i < len(block):
        if block[i] == '{':
            bc += 1
            if bc == 1: cur = ""
        elif block[i] == '}':
            if bc == 1:
                vals = find_cdf(cur)
                row_list.append(vals)
            bc -= 1
        elif bc >= 1:
            cur += block[i]
        i += 1
    return row_list

def process_3d(field_name, planes, rows, our_row_width, dav1d_row_width=None):
    """Process a 3D field."""
    block = extract_block(m_section, field_name)
    if block is None:
        return None
    plane_list = []
    bc = 0; depth = 0; plane_text = ""
    i = block.index('{') + 1 if '{' in block else 0
    while i < len(block):
        if block[i] == '{':
            bc += 1
            if bc == 1: plane_text = ""
            depth += 1
        elif block[i] == '}':
            depth -= 1
            if depth == 0:
                # Parse rows from plane_text
                row_list = []
                bc2 = 0; cur = ""
                j = 0
                while j < len(plane_text):
                    if plane_text[j] == '{':
                        bc2 += 1
                        if bc2 == 1: cur = ""
                    elif plane_text[j] == '}':
                        if bc2 == 1:
                            vals = find_cdf(cur)
                            row_list.append(vals)
                        bc2 -= 1
                    elif bc2 >= 1:
                        cur += plane_text[j]
                    j += 1
                plane_list.append(row_list)
        elif depth >= 1:
            plane_text += block[i]
        i += 1
    return plane_list

# Process all simple fields and output them
# cfl_alpha: [6][16], CDF15 per row
cfl_alpha = process_2d('cfl_alpha', 6, 16, 16)
emit("/* cfl_alpha: [6][16], CDF15 -> 15 values + 1 zero */")
emit(flat_array_3d('stb_av1_cfl_alpha', cfl_alpha, 1, 16))
emit()

# filter_intra: [8], CDF4(8949,12776,17211,29558) -> 4 values
filter_intra_vals = find_cdf(extract_block(m_section, 'filter_intra'))
emit("/* filter_intra: [8], only 4 values from CDF4 */")
emit(flat_array_1d('stb_av1_filter_intra', filter_intra_vals, 8))
emit()

# cfl_sign: [8], CDF7 -> 7 values
cfl_sign_vals = find_cdf(extract_block(m_section, 'cfl_sign'))
emit("/* cfl_sign: [8], CDF7 -> 7 values */")
emit(flat_array_1d('stb_av1_cfl_sign', cfl_sign_vals, 8))
emit()

# angle_delta: [8][8], CDF6 per row -> 6 values
angle_delta = process_2d('angle_delta', 8, 8, 8)
emit("/* angle_delta: [8][8], CDF6 -> 6 values per row */")
emit(flat_array_3d('stb_av1_angle_delta', angle_delta, 1, 8))
emit()

# seg_id: [3][8], CDF7 -> 7 values per row
seg_id = process_2d('seg_id', 3, 8, 8)
emit("/* seg_id: [3][8], CDF7 -> 7 values per row */")
emit(flat_array_3d('stb_av1_seg_id', seg_id, 1, 8))
emit()

# pal_sz: [2][7][8], CDF7 per row
pal_sz = process_3d('pal_sz', 2, 7, 8, 8)
emit("/* pal_sz: [2][7][8], CDF7 -> 7 values per row */")
emit(flat_array_3d('stb_av1_pal_sz', pal_sz, 7, 8))
emit()

# color_map: [2][7][5][8], CDF8 per row
color_map = process_3d('color_map', 2, 7*5, 8, 8)
# Flatten to [2][35][8]
cm_planes = []
for p in color_map:
    cm_planes.append(p)
emit("/* color_map: [2][7][5][8] = [2][35][8], CDF8 per row */")
emit(flat_array_3d('stb_av1_color_map', cm_planes, 35, 8))
emit()

# txsz: dav1d [4][3][4], ours [3][3][4]
# dav1d has TX_4X4..TX_32X32 (4 sizes), ours has TX_8X8..TX_32X32 (3)
# The first level in dav1d (TX_4X4) should be skipped for our struct
txsz_block = extract_block(m_section, 'txsz')
txsz_planes = []
bc = 0; depth = 0; plane_text = ""
i = txsz_block.index('{') + 1 if '{' in txsz_block else 0
while i < len(txsz_block):
    if txsz_block[i] == '{':
        bc += 1
        if bc == 1: plane_text = ""
        depth += 1
    elif txsz_block[i] == '}':
        depth -= 1
        if depth == 0:
            row_list = []
            bc2 = 0; cur = ""
            j = 0
            while j < len(plane_text):
                if plane_text[j] == '{':
                    bc2 += 1
                    if bc2 == 1: cur = ""
                elif plane_text[j] == '}':
                    if bc2 == 1:
                        vals = find_cdf(cur)
                        row_list.append(vals)
                    bc2 -= 1
                elif bc2 >= 1:
                    cur += plane_text[j]
                j += 1
            txsz_planes.append(row_list)
    elif depth >= 1:
        plane_text += txsz_block[i]
    i += 1
# Skip first plane (TX_4X4), keep [TX_8X8]..[TX_32X32]
txsz_planes = txsz_planes[1:]  # 3 planes
emit("/* txsz: dav1d [4][3][4], ours [3][3][4], skip TX_4X4 level */")
emit(flat_array_3d('stb_av1_txsz', txsz_planes, 3, 4))
emit()

# delta_q: [4], CDF3 -> 3 values
delta_q_vals = find_cdf(extract_block(m_section, 'delta_q'))
emit("/* delta_q: [4], CDF3 -> 3 values */")
emit(flat_array_1d('stb_av1_delta_q', delta_q_vals, 4))
emit()

# delta_lf: [5][4], CDF3 per row -> 3 values
delta_lf = process_2d('delta_lf', 5, 4, 4)
emit("/* delta_lf: [5][4], CDF3 -> 3 values per row */")
emit(flat_array_3d('stb_av1_delta_lf', delta_lf, 1, 4))
emit()

# restore_switchable: [4], CDF2 -> 2 values
restore_sw = find_cdf(extract_block(m_section, 'restore_switchable'))
emit("/* restore_switchable: [4], CDF2 -> 2 values */")
emit(flat_array_1d('stb_av1_restore_switchable', restore_sw, 4))
emit()

# restore_wiener: [2], CDF1
restore_w = find_cdf(extract_block(m_section, 'restore_wiener'))
emit("/* restore_wiener: [2], CDF1 -> 1 value */")
emit(flat_array_1d('stb_av1_restore_wiener', restore_w, 2))
emit()

# restore_sgrproj: [2], CDF1
restore_s = find_cdf(extract_block(m_section, 'restore_sgrproj'))
emit("/* restore_sgrproj: [2], CDF1 -> 1 value */")
emit(flat_array_1d('stb_av1_restore_sgrproj', restore_s, 2))
emit()

# txtp_inter1: [2][16], CDF15 -> 15 values per row
txtp_inter1 = process_2d('txtp_inter1', 2, 16, 16)
emit("/* txtp_inter1: [2][16], CDF15 -> 15 values */")
emit(flat_array_3d('stb_av1_txtp_inter1', txtp_inter1, 1, 16))
emit()

# txtp_inter2: [16], CDF11 -> 11 values
txtp_inter2_vals = find_cdf(extract_block(m_section, 'txtp_inter2'))
emit("/* txtp_inter2: [16], CDF11 -> 11 values */")
emit(flat_array_1d('stb_av1_txtp_inter2', txtp_inter2_vals, 16))
emit()

# txtp_inter3: [4][2], CDF1 per row
txtp_inter3 = process_2d('txtp_inter3', 4, 2, 2)
emit("/* txtp_inter3: [4][2], CDF1 -> 1 value */")
emit(flat_array_3d('stb_av1_txtp_inter3', txtp_inter3, 1, 2))
emit()

# txtp_intra1: [2][13][8], CDF6 -> 6 values per row
txtp_intra1 = process_3d('txtp_intra1', 2, 13, 8, 8)
emit("/* txtp_intra1: [2][13][8], CDF6 -> 6 values per row */")
emit(flat_array_3d('stb_av1_txtp_intra1', txtp_intra1, 13, 8))
emit()

# txtp_intra2: [3][13][8], CDF4 -> 4 values per row
txtp_intra2 = process_3d('txtp_intra2', 3, 13, 8, 8)
emit("/* txtp_intra2: [3][13][8], CDF4 -> 4 values per row */")
emit(flat_array_3d('stb_av1_txtp_intra2', txtp_intra2, 13, 8))
emit()

# skip: [3][2], CDF1
skip_block = extract_block(m_section, 'skip')
skip_vals = find_cdf(skip_block)
emit("/* skip: [3][2], CDF1 -> 1 value per row */")
emit(flat_array_3d('stb_av1_skip', [skip_vals[:1], skip_vals[1:2], skip_vals[2:3]], 1, 2))
emit()

# skip_mode: [3][2], CDF1
skip_mode_vals = find_cdf(extract_block(m_section, 'skip_mode'))
emit("/* skip_mode: [3][2], CDF1 -> 1 value per row */")
emit(flat_array_3d('stb_av1_skip_mode', [skip_mode_vals[:1], skip_mode_vals[1:2], skip_mode_vals[2:3]], 1, 2))
emit()

# seg_pred: [3][2], CDF1
seg_pred_vals = find_cdf(extract_block(m_section, 'seg_pred'))
emit("/* seg_pred: [3][2], CDF1 -> 1 value per row */")
emit(flat_array_3d('stb_av1_seg_pred', [seg_pred_vals[:1], seg_pred_vals[1:2], seg_pred_vals[2:3]], 1, 2))
emit()

# intrabc: [2], CDF1
intrabc_vals = find_cdf(extract_block(m_section, 'intrabc'))
emit("/* intrabc: [2], CDF1 -> 1 value */")
emit(flat_array_1d('stb_av1_intrabc', intrabc_vals, 2))
emit()

# pal_uv: [2][2], CDF1
pal_uv = process_2d('pal_uv', 2, 2, 2)
emit("/* pal_uv: [2][2] */")
emit(flat_array_3d('stb_av1_pal_uv', pal_uv, 1, 2))
emit()

# y_mode: [4][16], CDF12 -> 12 values per row
y_mode = process_2d('y_mode', 4, 16, 16)
emit("/* y_mode: [4][16], CDF12 -> 12 values per row */")
emit(flat_array_3d('stb_av1_y_mode', y_mode, 1, 16))
emit()

# wedge_idx: [9][16], CDF15 -> 15 values per row
wedge_idx = process_2d('wedge_idx', 9, 16, 16)
emit("/* wedge_idx: [9][16], CDF15 -> 15 values per row */")
emit(flat_array_3d('stb_av1_wedge_idx', wedge_idx, 1, 16))
emit()

# comp_inter_mode: [8][8], CDF7 -> 7 values per row
comp_inter_mode = process_2d('comp_inter_mode', 8, 8, 8)
emit("/* comp_inter_mode: [8][8], CDF7 -> 7 values per row */")
emit(flat_array_3d('stb_av1_comp_inter_mode', comp_inter_mode, 1, 8))
emit()

# filter: dav1d [2][8][4], ours [2][8][8]
# CDF2 -> 2 values per row, padded to 4 (dav1d) or 8 (ours)
filter_3d = process_3d('filter', 2, 8, 4, 4)
emit("/* filter: dav1d [2][8][4], ours [2][8][8] */")
emit(flat_array_3d('stb_av1_filter', filter_3d, 8, 8))
emit()

# interintra_mode: [4][4], CDF3 -> 3 values per row
interintra_mode = process_2d('interintra_mode', 4, 4, 4)
emit("/* interintra_mode: [4][4], CDF3 -> 3 values per row */")
emit(flat_array_3d('stb_av1_interintra_mode', interintra_mode, 1, 4))
emit()

# newmv_mode: [6][2], CDF1
newmv_mode = process_2d('newmv_mode', 6, 2, 2)
emit("/* newmv_mode: [6][2] */")
emit(flat_array_3d('stb_av1_newmv_mode', newmv_mode, 1, 2))
emit()

# globalmv_mode: [2][2], CDF1
globalmv_mode = process_2d('globalmv_mode', 2, 2, 2)
emit("/* globalmv_mode: [2][2] */")
emit(flat_array_3d('stb_av1_globalmv_mode', globalmv_mode, 1, 2))
emit()

# refmv_mode: [6][2], CDF1
refmv_mode = process_2d('refmv_mode', 6, 2, 2)
emit("/* refmv_mode: [6][2] */")
emit(flat_array_3d('stb_av1_refmv_mode', refmv_mode, 1, 2))
emit()

# drl_bit: [3][2], CDF1
drl_bit = process_2d('drl_bit', 3, 2, 2)
emit("/* drl_bit: [3][2] */")
emit(flat_array_3d('stb_av1_drl_bit', drl_bit, 1, 2))
emit()

# intra: [4][2], CDF1
intra = process_2d('intra', 4, 2, 2)
emit("/* intra: [4][2] */")
emit(flat_array_3d('stb_av1_intra', intra, 1, 2))
emit()

# comp: [5][2], CDF1
comp = process_2d('comp', 5, 2, 2)
emit("/* comp: [5][2] */")
emit(flat_array_3d('stb_av1_comp', comp, 1, 2))
emit()

# comp_dir: [5][2], CDF1
comp_dir = process_2d('comp_dir', 5, 2, 2)
emit("/* comp_dir: [5][2] */")
emit(flat_array_3d('stb_av1_comp_dir', comp_dir, 1, 2))
emit()

# jnt_comp: [6][2], CDF1
jnt_comp = process_2d('jnt_comp', 6, 2, 2)
emit("/* jnt_comp: [6][2] */")
emit(flat_array_3d('stb_av1_jnt_comp', jnt_comp, 1, 2))
emit()

# mask_comp: [6][2], CDF1
mask_comp = process_2d('mask_comp', 6, 2, 2)
emit("/* mask_comp: [6][2] */")
emit(flat_array_3d('stb_av1_mask_comp', mask_comp, 1, 2))
emit()

# wedge_comp: [9][2], CDF1
wedge_comp = process_2d('wedge_comp', 9, 2, 2)
emit("/* wedge_comp: [9][2] */")
emit(flat_array_3d('stb_av1_wedge_comp', wedge_comp, 1, 2))
emit()

# ref: [6][3][2], CDF1 per row
ref = process_3d('ref', 6, 3, 2, 2)
emit("/* ref: [6][3][2] */")
emit(flat_array_3d('stb_av1_ref', ref, 3, 2))
emit()

# comp_fwd_ref: [3][3][2], CDF1
comp_fwd_ref = process_3d('comp_fwd_ref', 3, 3, 2, 2)
emit("/* comp_fwd_ref: [3][3][2] */")
emit(flat_array_3d('stb_av1_comp_fwd_ref', comp_fwd_ref, 3, 2))
emit()

# comp_bwd_ref: [2][3][2], CDF1
comp_bwd_ref = process_3d('comp_bwd_ref', 2, 3, 2, 2)
emit("/* comp_bwd_ref: [2][3][2] */")
emit(flat_array_3d('stb_av1_comp_bwd_ref', comp_bwd_ref, 3, 2))
emit()

# comp_uni_ref: [3][3][2], CDF1
comp_uni_ref = process_3d('comp_uni_ref', 3, 3, 2, 2)
emit("/* comp_uni_ref: [3][3][2] */")
emit(flat_array_3d('stb_av1_comp_uni_ref', comp_uni_ref, 3, 2))
emit()

# interintra: [7][2], CDF1
interintra = process_2d('interintra', 7, 2, 2)
emit("/* interintra: [7][2] */")
emit(flat_array_3d('stb_av1_interintra', interintra, 1, 2))
emit()

# interintra_wedge: [7][2], CDF1
interintra_wedge = process_2d('interintra_wedge', 7, 2, 2)
emit("/* interintra_wedge: [7][2] */")
emit(flat_array_3d('stb_av1_interintra_wedge', interintra_wedge, 1, 2))
emit()

# txpart: [7][3][2], CDF1 per row
txpart = process_3d('txpart', 7, 3, 2, 2)
emit("/* txpart: [7][3][2] */")
emit(flat_array_3d('stb_av1_txpart', txpart, 3, 2))
emit()

# pal_y: [7][3][2], CDF1 per row
pal_y = process_3d('pal_y', 7, 3, 2, 2)
emit("/* pal_y: [7][3][2] */")
emit(flat_array_3d('stb_av1_pal_y', pal_y, 3, 2))
emit()

# Indexed fields:
# use_filter_intra: [22][2], BS_* -> CDF1
block = extract_block(m_section, 'use_filter_intra')
idx_vals = extract_indexed_values(block)
rows = [[0, 0] for _ in range(22)]
for idx, vals in idx_vals.items():
    rows[idx] = pad_row(vals, 2)
emit("/* use_filter_intra: [22][2], indexed by BS_* */")
emit(flat_array_3d('stb_av1_use_filter_intra', rows, 1, 2))
emit()

# motion_mode: [22][4], BS_* -> CDF2
block = extract_block(m_section, 'motion_mode')
idx_vals = extract_indexed_values(block)
rows = [[0, 0, 0, 0] for _ in range(22)]
for idx, vals in idx_vals.items():
    rows[idx] = pad_row(vals, 4)
emit("/* motion_mode: [22][4], indexed by BS_* */")
emit(flat_array_3d('stb_av1_motion_mode', rows, 1, 4))
emit()

# obmc: [22][2], BS_* -> CDF1
block = extract_block(m_section, 'obmc')
idx_vals = extract_indexed_values(block)
rows = [[0, 0] for _ in range(22)]
for idx, vals in idx_vals.items():
    rows[idx] = pad_row(vals, 2)
emit("/* obmc: [22][2], indexed by BS_* */")
emit(flat_array_3d('stb_av1_obmc', rows, 1, 2))
emit()

# ==================== MV fields ====================
mv_fields = [
    ('mv_classes', 'classes', 16, 16),
    ('mv_sign', 'sign', 2, 2),
    ('mv_class0', 'class0', 2, 2),
    ('mv_class0_fp', 'class0_fp', [2,4], [2,4]),
    ('mv_class0_hp', 'class0_hp', 2, 2),
    ('mv_classN', 'classN', [10,2], [10,2]),
    ('mv_classN_fp', 'classN_fp', 4, 4),
    ('mv_classN_hp', 'classN_hp', 2, 2),
    ('mv_joint', 'joint', 4, 4),
]

for our_name, dav1d_name, our_dims, dav1d_dims in mv_fields:
    block = extract_block(mv_section, dav1d_name)
    if block is None:
        emit(f"/* WARNING: mv field {dav1d_name} not found */")
        continue
    vals = find_cdf(block)
    if isinstance(our_dims, list):
        # 2D
        total = 1
        for d in our_dims: total *= d
        padded = pad_row(vals, total)
        # Reshape
        rows = [padded[i*our_dims[1]:(i+1)*our_dims[1]] for i in range(our_dims[0])]
        emit(f"/* {our_name}: {our_dims} */")
        emit(flat_array_3d(f'stb_av1_{our_name}', rows, 1, our_dims[1]))
    else:
        emit(f"/* {our_name}: [{our_dims}] */")
        emit(flat_array_1d(f'stb_av1_{our_name}', vals, our_dims))
    emit()

# kfym: [5][5][16], CDF13 -> 13 values per row
kfym_planes = process_3d('kfym', 5, 5, 16, 16)
emit("/* kfym: [5][5][16], CDF13 -> 13 values per row */")
emit(flat_array_3d('stb_av1_kfym', kfym_planes, 5, 16))
emit()

print("\n".join(out), file=sys.stderr)
print("Done generating field arrays", file=sys.stderr)
