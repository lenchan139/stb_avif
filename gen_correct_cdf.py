#!/usr/bin/env python3
"""
Parse dav1d's cdf.c to generate a correct flat CDF data array
that matches the memory layout of StbCdfContext.
"""
import re

with open('dav1d_ref/cdf.c', 'r') as f:
    src = f.read()

# Extract BS_* enum values from dav1d's block size enum
# BS_4x4=0, BS_4x8=1, BS_8x4=2, BS_8x8=3, BS_8x16=4, BS_16x8=5,
# BS_16x16=6, BS_16x32=7, BS_32x16=8, BS_32x32=9, BS_32x64=10, BS_64x32=11,
# BS_64x64=12, BS_64x128=13, BS_128x64=14, BS_128x128=15,
# BS_4x16=16, BS_16x4=17, BS_8x32=18, BS_32x8=19, BS_16x64=20, BS_64x16=21
BS = {
    'BS_4x4': 0, 'BS_4x8': 1, 'BS_8x4': 2, 'BS_8x8': 3,
    'BS_8x16': 4, 'BS_16x8': 5, 'BS_16x16': 6, 'BS_16x32': 7,
    'BS_32x16': 8, 'BS_32x32': 9, 'BS_32x64': 10, 'BS_64x32': 11,
    'BS_64x64': 12, 'BS_64x128': 13, 'BS_128x64': 14, 'BS_128x128': 15,
    'BS_4x16': 16, 'BS_16x4': 17, 'BS_8x32': 18, 'BS_32x8': 19,
    'BS_16x64': 20, 'BS_64x16': 21,
}

def eval_cdf(name, args_str):
    """Evaluate CDFN(a,b,...) to list of uint16_t values."""
    args = [int(a.strip()) for a in args_str.split(',') if a.strip()]
    return [32768 - x for x in args]

def eval_cdf_line(line):
    """Extract all CDFN(...) from a line, return list of values."""
    results = []
    for m in re.finditer(r'CDF(\d+)\(([^)]+)\)', line):
        n = int(m.group(1))
        args = [int(a.strip()) for a in m.group(2).split(',')]
        assert len(args) == n, f"CDF{n} expected {n} args, got {len(args)}: {m.group(0)}"
        results.extend([32768 - x for x in args])
    return results

# Extract the initializer body from cdf.c
start_marker = 'static const CdfDefaultContext default_cdf = {'
start_idx = src.find(start_marker)
assert start_idx >= 0, "Could not find default_cdf"

# Find matching closing brace
depth = 0
in_init = False
end_idx = start_idx
for i in range(start_idx, len(src)):
    if src[i] == '{':
        depth += 1
        in_init = True
    elif src[i] == '}':
        depth -= 1
        if in_init and depth == 0:
            end_idx = i + 1
            break

init_body = src[start_idx:end_idx]

# Now I need to extract the CdfCoefContext default values too
# These are in cdf.c starting after the default_cdf definition
coef_start = src.find('.skip = {', end_idx)

# Actually, let me take a different approach.
# Instead of parsing the complex nested initializer, I'll extract
# each field's values by finding the field markers in the source.

# Field extraction from the initializer body
# Each field starts with .fieldname = { and contains CDF macros

fields_order_init = [
    'y_mode', 'use_filter_intra', 'filter_intra', 'uv_mode',
    'angle_delta', 'filter', 'newmv_mode', 'globalmv_mode', 'refmv_mode',
    'drl_bit', 'comp_inter_mode', 'intra', 'comp', 'comp_dir',
    'jnt_comp', 'mask_comp', 'wedge_comp', 'wedge_idx',
    'interintra', 'interintra_mode', 'interintra_wedge', 'ref',
    'comp_fwd_ref', 'comp_bwd_ref', 'comp_uni_ref',
    'txsz', 'txpart', 'txtp_inter1', 'txtp_inter2', 'txtp_inter3',
    'txtp_intra1', 'txtp_intra2', 'skip', 'skip_mode', 'partition',
    'seg_pred', 'seg_id', 'cfl_sign', 'cfl_alpha',
    'restore_wiener', 'restore_sgrproj', 'restore_switchable',
    'delta_q', 'delta_lf', 'motion_mode', 'obmc',
    'pal_y', 'pal_sz', 'pal_uv', 'color_map', 'intrabc',
]

mv_fields_init = ['classes', 'sign', 'class0', 'class0_fp', 'class0_hp',
                  'classN', 'classN_fp', 'classN_hp', 'joint']

# Actually, parsing the full CDF initializer is extremely complex due to
# nested braces, designated indices, etc. Let me take a completely different approach.

# I'll just manually hardcode the correct values for each field by
# evaluating the CDF macros from cdf.c. This is tedious but reliable.

# Actually, the BEST approach: use C compiler to do the evaluation.
# Write a C99 program that includes dav1d's cdf.c, creates a
# StbCdfContext using designated initializers matching dav1d's layout,
# and dumps the memory.

# But we can't do that because dav1d's cdf.c uses its own types.

# OK, let me try yet another approach. I'll write a C99 program that:
# 1. Creates a struct matching dav1d's CdfDefaultContext layout
# 2. Initializes it with the same designated initializers from cdf.c
# 3. Dumps the memory

# Actually the simplest correct approach is to just restructure our init function
# to set each field individually. Let me generate that.

# For the flat data approach, I need to know which field in our struct
# corresponds to which offset, and what values it should have.

# Our struct memory layout (from StbCdfContext definition):
struct_layout = []

# CdfModeContext fields in our struct order:
struct_layout.append(('uv_mode', [2, 13, 15]))       # 390
struct_layout.append(('partition', [5, 4, 16]))        # 320
struct_layout.append(('cfl_alpha', [6, 16]))           # 96
struct_layout.append(('txtp_inter1', [2, 16]))         # 32
struct_layout.append(('txtp_inter2', [16]))             # 16
struct_layout.append(('txtp_intra1', [2, 13, 8]))      # 208 (overridden)
struct_layout.append(('txtp_intra2', [3, 13, 8]))      # 312 (overridden)
struct_layout.append(('cfl_sign', [8]))                # 8
struct_layout.append(('angle_delta', [8, 8]))          # 64
struct_layout.append(('filter_intra', [8]))             # 8
struct_layout.append(('seg_id', [3, 8]))               # 24
struct_layout.append(('pal_sz', [2, 7, 8]))            # 112
struct_layout.append(('color_map', [2, 7, 5, 8]))      # 560
struct_layout.append(('txsz', [3, 3, 4]))              # 36
struct_layout.append(('delta_q', [4]))                  # 4
struct_layout.append(('delta_lf', [5, 4]))             # 20
struct_layout.append(('restore_switchable', [4]))       # 4
struct_layout.append(('restore_wiener', [2]))           # 2
struct_layout.append(('restore_sgrproj', [2]))          # 2
struct_layout.append(('txtp_inter3', [4, 2]))           # 8
struct_layout.append(('use_filter_intra', [22, 2]))     # 44
struct_layout.append(('txpart', [7, 3, 2]))             # 42
struct_layout.append(('skip', [3, 2]))                  # 6
struct_layout.append(('pal_y', [7, 3, 2]))             # 42
struct_layout.append(('pal_uv', [2, 2]))                # 4
struct_layout.append(('intrabc', [2]))                  # 2
struct_layout.append(('y_mode', [4, 16]))              # 64
struct_layout.append(('wedge_idx', [9, 16]))            # 144
struct_layout.append(('comp_inter_mode', [8, 8]))       # 64
struct_layout.append(('filter', [2, 8, 8]))             # 128
struct_layout.append(('interintra_mode', [4, 4]))       # 16
struct_layout.append(('motion_mode', [22, 4]))          # 88
struct_layout.append(('skip_mode', [3, 2]))             # 6
struct_layout.append(('newmv_mode', [6, 2]))            # 12
struct_layout.append(('globalmv_mode', [2, 2]))         # 4
struct_layout.append(('refmv_mode', [6, 2]))            # 12
struct_layout.append(('drl_bit', [3, 2]))               # 6
struct_layout.append(('intra', [4, 2]))                 # 8
struct_layout.append(('comp', [5, 2]))                  # 10
struct_layout.append(('comp_dir', [5, 2]))              # 10
struct_layout.append(('jnt_comp', [6, 2]))              # 12
struct_layout.append(('mask_comp', [6, 2]))             # 12
struct_layout.append(('wedge_comp', [9, 2]))            # 18
struct_layout.append(('ref', [6, 3, 2]))                # 36
struct_layout.append(('comp_fwd_ref', [3, 3, 2]))       # 18
struct_layout.append(('comp_bwd_ref', [2, 3, 2]))       # 12
struct_layout.append(('comp_uni_ref', [3, 3, 2]))       # 18
struct_layout.append(('seg_pred', [3, 2]))              # 6
struct_layout.append(('interintra', [7, 2]))            # 14
struct_layout.append(('interintra_wedge', [7, 2]))      # 14
struct_layout.append(('obmc', [22, 2]))                 # 44

# CdfMvContext fields:
struct_layout.append(('mv_classes', [16]))
struct_layout.append(('mv_sign', [2]))
struct_layout.append(('mv_class0', [2]))
struct_layout.append(('mv_class0_fp', [2, 4]))
struct_layout.append(('mv_class0_hp', [2]))
struct_layout.append(('mv_classN', [10, 2]))
struct_layout.append(('mv_classN_fp', [4]))
struct_layout.append(('mv_classN_hp', [2]))
struct_layout.append(('mv_joint', [4]))

# kfym:
struct_layout.append(('kfym', [5, 5, 16]))

def total_size(dims):
    s = 1
    for d in dims:
        s *= d
    return s

def flat_index(dims, indices):
    """Compute flat array index for multi-dimensional access."""
    idx = 0
    stride = 1
    for i in range(len(dims)-1, -1, -1):
        idx += indices[i] * stride
        stride *= dims[i]
    return idx

# Compute total
total = sum(total_size(dims) for _, dims in struct_layout)
print(f"Total struct entries: {total}")

# Now I need to build a mapping: for each field, what are the CDF values?
# I'll extract them by finding each field in cdf.c and evaluating macros.

# This is complex, so let me take the approach of generating
# per-field static data arrays. Instead of a flat copy, I'll
# replace the flat copy in stb_av1_cdf_full_init with per-field initialization.

print("Generating per-field CDF initialization code...")

# Parse each field from cdf.c's initializer
# For each field, find its section and extract CDF values

def find_field_values(field_name):
    """Find the CDF values for a field in cdf.c's initializer."""
    # Find .field_name = { ... }
    pattern = r'\.' + re.escape(field_name) + r'\s*=\s*\{'
    m = re.search(pattern, init_body)
    if not m:
        return None
    
    # Find the matching closing brace
    start = m.end() - 1  # Position of opening brace
    depth = 1
    pos = start + 1
    while pos < len(init_body) and depth > 0:
        if init_body[pos] == '{':
            depth += 1
        elif init_body[pos] == '}':
            depth -= 1
        pos += 1
    
    field_body = init_body[start:pos]
    return field_body

# For each field, extract values and output as C array assignment
# This generates the code for stb_av1_cdf_full_init

print("/* Field-by-field CDF initialization */")
print("/* Generated from dav1d's cdf.c default_cdf initializer */")

for field_name, dims in struct_layout:
    sz = total_size(dims)
    body = find_field_values(field_name)
    if body is None:
        print(f"/* WARNING: Could not find field {field_name} */")
        continue
    
    # Extract all CDF values from the field body
    # Remove designated initializers like [BS_4x4] = 
    clean = re.sub(r'\[BS_\w+\]\s*=\s*', '', body)
    clean = re.sub(r'\[.*?\]\s*=\s*', '', clean)  # Remove any [index] = 
    
    # Find all CDF macro calls
    values = []
    for m in re.finditer(r'CDF(\d+)\(([^)]+)\)', clean):
        n = int(m.group(1))
        args = [int(a.strip()) for a in m.group(2).split(',')]
        values.extend([32768 - x for x in args])
    
    if len(values) != sz:
        # For uv_mode, dav1d has [2][13][16] but we have [2][13][15]
        # The values include the 16th element which we don't have
        if field_name == 'uv_mode' and len(values) == 2 * 13 * 16:
            # Truncate to [2][13][15]
            truncated = []
            for i in range(2):
                for j in range(13):
                    truncated.extend(values[i*13*16 + j*16 : i*13*16 + j*16 + 15])
            values = truncated
            print(f"/* uv_mode truncated from [2][13][16] to [2][13][15]: {len(values)} values */", file=__import__('sys').stderr)
    
    if len(values) != sz:
        print(f"/* WARNING: {field_name} expected {sz} values, got {len(values)} */")
        continue
    
    # Check if this field is overridden in the init function
    if field_name in ('txtp_intra1', 'txtp_intra2'):
        print(f"/* {field_name}: overridden by separate memcpy, skipping */")
        continue
    
    # Output as array assignment
    print(f"/* {field_name}[{','.join(str(d) for d in dims)}] = {len(values)} values */")

print("\nDone analyzing fields.")
