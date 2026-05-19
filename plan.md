# Missing AV1 Features: Plan + Implementation Status (2026-04-25)

## Goal
- Close the highest-value gaps between stb_avif's still-image subset and broader AV1-in-AVIF streams while keeping strict, clean rejection for truly unsupported decode tools.

## Implemented In This Pass
- AV1 OBU pre-parse is now forward-compatible for non-critical OBUs:
   - Redundant frame header OBU is skipped (not fatal).
   - Unknown/unhandled OBU types are skipped by size (not fatal).
- This removes a known "missing behavior" where valid streams could fail before decode due to strict OBU-type rejection.

## Remaining Missing Features (Prioritized)
1. Multi-frame / animation sequencing (item + frame timeline handling).
2. Inter-frame prediction (P/B) and reference frame state.
3. IntraBC decode path (`allow_intrabc`) for screen-content tools.
4. QMatrix-enabled decode path (`using_qmatrix`) rather than entry reject.
5. Temporal/spatial layered streams (currently single-layer only).

## Next Implementation Steps
1. Add focused trace fixtures that exercise non-critical/unknown OBUs and ensure decode continues.
2. Implement minimal animation plumbing (frame iteration + disposal/blend behavior) behind a compile-time guard.
3. Keep hard-fail guards for inter-frame tools until full ref/motion pipeline is added.

# AVIF → PNG Decoder: Detailed Implementation Plan

## 1. Architecture Overview

`stb_avif.h` is a ~14,800-line single-header C89 library implementing a complete AVIF decode pipeline:

```
AVIF container (ISOBMFF)
  → AV1 OBU parsing (sequence header, frame header, tile group)
    → Per-tile decode loop
      → Recursive partition tree decode (64×64 / 128×128 superblocks)
        → Coding unit: mode info, skip, CDEF index, Y/UV mode, angle delta,
          CFL, palette, filter-intra, TX size
        → Intra prediction (DC/V/H/D45-D67/SMOOTH/PAETH/filter-intra/palette)
        → Coefficient decode (EOB, base levels, BR levels, signs, dequant)
        → Inverse transform (DCT/ADST/FLIPADST/Identity, 4→64)
        → Reconstruction (pred + residual → plane)
    → Post-processing: CDEF → SuperRes → Loop Restoration → Film Grain
  → YUV → RGBA conversion (BT.601/709/2020, full/limited range)
  → Alpha plane decode (separate monochrome AV1 frame)
```

**Public API:**
- `stbi_avif_load(filename, &w, &h, &ch, desired_ch)` → `unsigned char *rgba`
- `stbi_avif_info(filename, &w, &h, &ch)` → dimensions without decode
- `stbi_avif_image_free(pixels)`

**Test harness:** `tests/test_decode.c` writes PPM/PNG/BMP. Build: `cc -O2 -o test_decode_run tests/test_decode.c -lm`

---

## 2. Current Quality Status

**Primary test image:** `steam_2253100.avif` (1024×772, YUV444, 10-bit, BT.709 limited)

| Metric | Current | Target |
|--------|---------|--------|
| MAE | **27.9** | < 1.0 |
| Max diff | 130 | < 10 |
| Median diff | 17.0 | < 1.0 |

**Error distribution (spatial heatmap, 4×8 grid):**
```
 59.8  26.3  24.2  17.1  17.6  21.6  17.0  12.6
 83.4  22.5  17.6  16.6  14.8  17.5  25.5  26.2
 88.5  21.7  19.3  25.2  17.8  12.1  23.3  15.6
103.5  28.7   9.3  18.4  26.1  26.7  18.9  18.0
```

**Key observation:** Left column has dramatically higher MAE (60→103). Errors are spatially widespread (not just cascading from top-left). This suggests systematic issues in prediction and/or transforms rather than a single cascading bug.

**All 8 test images decode without crashes** (fox 8/10bpc YUV420, kimono, Gb5RU6, G-0trmK, steam).

---

## 3. Known Issues (Priority Order)

### 3.1 CRITICAL: Directional Prediction Modes (V, H, D45-D67) — ~50% of error

The V_PRED and H_PRED modes have spurious gradient terms that should not exist:

```c
// CURRENT (WRONG):
case 1u: /* V */
   val = (int)top[tix] + ((int)(2u * x + 1u) * amp) / (int)(2u * bw) - amp / 2;
case 2u: /* H */
   val = (int)left[tiy] + ((int)(2u * y + 1u) * amp) / (int)(2u * bh) - amp / 2;

// CORRECT (per AV1 spec):
case 1u: /* V */  val = (int)top[x];
case 2u: /* H */  val = (int)left[y];
```

Angular modes D45/D135/D113/D157/D203/D67 (modes 3-8) use crude averaging approximations instead of the proper Z1/Z2/Z3 angular predictors from AV1 spec §7.11.2.4-6. The spec uses:
- Angle-to-dx/dy lookup tables
- Bilinear interpolation between integer positions
- Derivative-based sub-pixel shifts

### 3.2 CRITICAL: Angle Delta Not Applied

The angle delta value is read from the bitstream but never used to modify the prediction angle. For directional modes 1-8, the actual angle should be:
```
nominal_angle = mode_to_angle[mode - 1]  // V=90, H=180, D45=45, D135=135, ...
actual_angle = nominal_angle + angle_delta * 3
```
Then map actual_angle → Z1/Z2/Z3 predictor with appropriate dx/dy.

### 3.3 HIGH: DC Prediction Rounding for Non-Square Blocks

Current DC averaging uses simple `sum / count`. The AV1 spec requires power-of-2 rounding:
```c
// AV1 spec §7.11.2.3:
dc = (sum + (count >> 1)) >> log2(count)
```
For non-square blocks where `count = bw + bh` is not a power of 2, the multiplier correction tables (`dc_multiplier_1x2`, `dc_multiplier_1x4`) should be used.

### 3.4 HIGH: Extended Reference Pixels for Prediction

Current code reads `bw + bh + 2` reference pixels, but:
1. The AV1 spec allows up to `2 * max(bw, bh)` pixels from each edge
2. Extended neighbor availability (`have_above_right`, `have_below_left`) is not computed
3. Without extended refs, directional modes that reach beyond the block produce wrong values

### 3.5 MEDIUM: Inverse Transform Accuracy

The IDCT/IADST implementations may have:
- Missing intermediate rounding (AV1 spec requires `ROUND2SIGNED` at specific stages)
- Incorrect scaling constants (should use exact AV1 spec fixed-point values, not floating-point derived)
- No `ROUND_POWER_OF_TWO_SIGNED` on the final column output

Verification approach: extract a single TX block's coefficients, compare our inverse transform output vs dav1d's.

### 3.6 MEDIUM: TX Size Decode Edge Cases

TX size selection works for the common case but may have edge cases:
- Split TX mode (`txfm_partition`) for larger blocks is parsed but the subdivision into sub-TX blocks may not correctly iterate all sub-blocks
- Rectangular TX (non-square) size derivation from `tx_size_cdf` result might not match spec for all block sizes

### 3.7 LOW: CFL (Chroma From Luma) Implementation

CFL is structurally implemented but:
- The luma average subtraction should use only the overlap region
- The `ac_q` contribution (alpha * AC luma component) rounding might be off by 1

### 3.8 LOW: Filter Intra Mode

`stbi_avif__av1_filter_intra_predict` is implemented but needs verification against spec:
- 7 recursive 4×2 sub-block filters with 7 tap weight tables
- Edge handling at frame boundaries

### 3.9 COSMETIC: YUV→RGB Conversion Edge Cases

- BT.2020 path may need verification for 10-bit content
- Chroma upsampling for YUV420 (bilinear vs nearest-neighbor)
- SuperRes upsampling (already implemented but untested)

---

## 4. Implementation Phases

### Phase 1: Fix V/H Prediction (Immediate Win)

**Expected MAE reduction: 27.9 → ~20**

1. Remove gradient terms from V_PRED (case 1) and H_PRED (case 2) in `stbi_avif__av1_predict_block` (~line 8175)
2. V_PRED: `val = (int)top[x]` (pure vertical copy)
3. H_PRED: `val = (int)left[y]` (pure horizontal copy)
4. Test & measure MAE

### Phase 2: Implement Proper Angular Prediction (Z1/Z2/Z3)

**Expected MAE reduction: → ~12**

1. Add angle lookup table: `mode_to_angle[8] = {90, 180, 45, 135, 113, 157, 203, 67}`
2. Apply angle delta: read the delta value (already parsed), compute `actual_angle = nominal + delta * 3`
3. Implement the three angular predictor zones:
   - **Z1** (angle 0–90): Top-right diagonal, uses `dx = -tan(angle)` lookup
   - **Z2** (angle 90–180): Uses both top and left refs, `dx` and `dy` lookups
   - **Z3** (angle 180–270): Bottom-left diagonal, uses `dy = -1/tan(angle)` lookup
4. For each pixel (x, y):
   - Compute fractional source position from dx/dy
   - Bilinear interpolate between two reference pixels
   - `val = ((64 - frac) * ref[base] + frac * ref[base + 1] + 32) >> 6`
5. Add `dr_intra_derivative[90]` table from AV1 spec

**Key references:**
- AV1 spec §7.11.2.4 (Z1), §7.11.2.5 (Z2), §7.11.2.6 (Z3)
- dav1d source: `src/ipred_tmpl.c` functions `ipred_z1_c`, `ipred_z2_c`, `ipred_z3_c`

### Phase 3: Fix DC Prediction Rounding

**Expected MAE reduction: → ~10**

1. Replace `sum / count` with proper power-of-2 rounding for square blocks
2. Add multiplier tables for non-square ratio corrections
3. Handle `count = bw` (top-only), `count = bh` (left-only), `count = bw + bh` (both)

### Phase 4: Extended Reference Pixel Loading

**Expected MAE reduction: → ~6**

1. Compute `have_above_right` and `have_below_left` per-block based on partition structure
2. Load up to `2 * bw` top reference pixels (extending right) and `2 * bh` left reference pixels (extending down)
3. When extended pixels unavailable, repeat the last available pixel
4. This primarily affects angular modes that look diagonally beyond the block

### Phase 5: Verify & Fix Inverse Transforms

**Expected MAE reduction: → ~2**

1. Add debug hook to dump coefficients at a specific block position
2. Compare dequantized coefficients against dav1d output
3. Compare post-transform residuals against dav1d
4. Fix any rounding/scaling discrepancies found in:
   - IDCT 4/8/16/32/64
   - IADST 4/8/16
   - Identity transforms
   - rect2 scaling factor application
   - Row shift values
   - Final column normalization (>>4)

### Phase 6: Coefficient Decode Audit

**Expected MAE reduction: → ~1**

1. Verify EOB position decode for all TX sizes (4×4 through 32×32)
2. Verify coeff_base context function (`get_lower_levels_ctx_2d` / `_1d`)
3. Verify BR (base range) context function
4. Verify sign decode (including DC sign context from neighbors)
5. Verify dequantization step: `(level * qstep + round) >> shift`
6. Check the scan order tables match AV1 spec

### Phase 7: End-to-End Pixel Matching

**Target: MAE < 1.0**

1. Build a per-SB comparison tool: decode both with our decoder and dav1d, compare SB-by-SB
2. Identify any remaining systematic errors
3. Fix coefficient rounding, transform precision, prediction edge cases
4. Verify CDEF application matches dav1d
5. Verify Loop Restoration filter matches dav1d
6. Verify film grain synthesis (if applicable)

---

## 5. Detailed Function Map

### Container Layer (lines 13526–14779)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__parse_file` | 14340 | ✅ Working |
| `stbi_avif__parse_meta` | 14163 | ✅ Working |
| `stbi_avif__parse_iloc` | 13799 | ✅ Working |
| `stbi_avif__resolve_primary` | 14211 | ✅ Working |
| `stbi_avif_load_from_memory` | 14460 | ✅ Working |

### AV1 Bitstream Layer (lines 652–2112)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__parse_av1_sequence_header` | 783 | ✅ Working |
| `stbi_avif__parse_av1_frame_header` | 1253 | ✅ Working |
| `stbi_avif__parse_av1_tile_group_header` | 2112 | ✅ Working |

### Entropy Coding (lines 2196–2406)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_range_decoder_init` | 2225 | ✅ Verified identical to dav1d |
| `stbi_avif__av1_read_symbol_adapt` | 2351 | ✅ Verified |
| `stbi_avif__av1_update_cdf` | 2324 | ✅ Verified |

### Prediction (lines 7851–8263)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_predict_block` | 8060 | ❌ V/H have gradient terms; D45-D67 are approximations |
| `stbi_avif__av1_filter_intra_predict` | 7953 | ⚠️ Implemented, needs verification |
| `stbi_avif__av1_paeth_predictor` | 7873 | ✅ Working |
| `stbi_avif__av1_apply_cfl_plane` | 8263 | ⚠️ Partial |

### Transforms (lines 8373–9786)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_idct4/8/16/32/64` | 8373–8808 | ⚠️ Needs rounding audit |
| `stbi_avif__av1_iadst4/8/16` | 9451–9509 | ⚠️ Needs rounding audit |
| `stbi_avif__av1_iidentity4-64` | 9608–9630 | ✅ Simple scaling |
| `stbi_avif__av1_inverse_transform_2d_rect` | 9647 | ⚠️ Row shift table needs verification |

### Coefficient Decode (lines 9800–10131)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_read_coeffs_after_skip` | 9800 | ⚠️ Needs edge case audit |
| `stbi_avif__av1_read_coeffs` | 10096 | ⚠️ Wrapper, skips txb_skip read |

### Decode Loop (lines 10186–11427)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_decode_coding_unit` | 10190 | ⚠️ Mode decode working; angle delta unused |
| `stbi_avif__av1_decode_partition` | 11027 | ✅ Working |
| `stbi_avif__av1_decode_tile` | 11360 | ✅ Working |
| `stbi_avif__av1_reconstruct_tx_block` | 10131 | ⚠️ Structure OK; transform accuracy TBD |

### Post-Processing (lines 11777–12962)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_cdef_filter` | 11777 | ✅ Implemented |
| `stbi_avif__av1_lr_filter` | 12540 | ✅ Implemented (Wiener + Sgrproj) |
| `stbi_avif__av1_apply_film_grain` | 12011 | ✅ Implemented |
| `stbi_avif__av1_apply_superres` | 12854 | ✅ Implemented |

### Color Conversion (lines 11427–11777)
| Function | Line | Status |
|----------|------|--------|
| `stbi_avif__av1_planes_to_rgba` | 11427 | ✅ Working (BT.601/709/2020) |

---

## 6. Testing Strategy

### 6.1 Per-Block Comparison Tool

Build a debug mode that dumps per-4×4 block data for a given SB position:
```c
#ifdef STB_AVIF_DEBUG
// At each coding unit, dump: mi_row, mi_col, mode, tx_size, coeffs[0..3], prediction[0..3], recon[0..3]
#endif
```

Compare against dav1d with `DAV1D_LOG=3` or custom patch.

### 6.2 Transform Unit Test

Create a standalone test that feeds known coefficient arrays through inverse transforms and checks output against spec reference values.

### 6.3 MAE Regression Tests

After each phase, measure MAE on all 8 test images:
```bash
for f in example_avif/*.avif; do
    ./test_decode_run "$f" "/tmp/out_$(basename $f .avif).ppm"
    python3 tests/analyze_seq.py "/tmp/out_$(basename $f .avif).ppm" "ref/$(basename $f .avif).ppm"
done
```

### 6.4 Test Image Coverage

| Image | Format | Resolution | Profile | Notes |
|-------|--------|-----------|---------|-------|
| steam_2253100 | YUV444 10-bit | 1024×772 | High | Primary test, has reference PPM |
| fox 8bpc | YUV420 8-bit | 1204×800 | Main | 4:2:0 subsampling test |
| fox 10bpc | YUV420 10-bit | 1204×800 | Main | 10-bit 4:2:0 |
| kimono | YUV420 10-bit | 722×1024 | Main | Tall aspect ratio |
| Gb5RU6R | Varies | 1429×1623 | - | Large image |
| G-0trmK | Varies | 89×100 | - | Small image, edge cases |
| hato | Varies | 1461×1530 | - | Large, varied content |
| testimg | Varies | 800×800 | - | Square |

---

## 7. Reference Materials

- **AV1 Spec:** https://aomediacodec.github.io/av1-spec/ (§7.11 for intra prediction, §7.12 for transforms)
- **dav1d source:** `/tmp/dav1d/` — `src/ipred_tmpl.c` (prediction), `src/itx_tmpl.c` (transforms), `src/recon_tmpl.c` (reconstruction)
- **dav1d build:** `/tmp/dav1d/build_noasm/tools/dav1d`
- **AOM reference:** `/tmp/aom_ref/` (CDF tables extracted from here)

---

## 8. Estimated Impact by Phase

| Phase | Change | MAE Before | MAE After (est.) | Effort |
|-------|--------|-----------|------------------|--------|
| 1 | Fix V/H prediction | 27.9 | ~20 | 10 min |
| 2 | Angular Z1/Z2/Z3 | ~20 | ~12 | 2-4 hours |
| 3 | DC rounding | ~12 | ~10 | 30 min |
| 4 | Extended refs | ~10 | ~6 | 1-2 hours |
| 5 | Transform audit | ~6 | ~2 | 2-4 hours |
| 6 | Coeff decode audit | ~2 | ~1 | 1-2 hours |
| 7 | End-to-end polish | ~1 | < 0.5 | 2-4 hours |

**Total estimated path from MAE 27.9 → < 1.0: 7 phases**

---

## 9. Quick Wins Checklist

- [x] Remove gradient from V_PRED: `val = (int)top[x]`
- [x] Remove gradient from H_PRED: `val = (int)left[y]`
- [x] Store and use angle_delta in prediction (currently read but discarded)
- [x] Fix DC rounding to use `(sum + (count >> 1)) >> log2(count)`
- [x] Verify SMOOTH weights table indices are correct for non-square blocks
- [x] Check ref_count limit (currently `bw + bh + 2`, should be `2 * max(bw, bh)`)

---

## 10. Detailed Comparison: stb_avif.h vs dav1d Reference Decoder

### 10.1 Intra Prediction — CRITICAL DIFFERENCES

#### V_PRED / H_PRED (Modes 1, 2) — **WRONG**

| | stb_avif.h (line 8175) | dav1d (`ipred_v_c` / `ipred_h_c`) |
|---|---|---|
| V_PRED | `val = top[tix] + ((2*x+1) * amp) / (2*bw) - amp/2` | `dst[x] = top[x]` (pure copy) |
| H_PRED | `val = left[tiy] + ((2*y+1) * amp) / (2*bh) - amp/2` | `dst[x] = left[y]` (constant fill per row) |

stb_avif.h adds a linear gradient ramp that does not exist in the AV1 spec. This alone causes significant error on every V/H predicted block.

#### Angular Modes D45–D67 (Modes 3-8) — **COMPLETELY WRONG**

stb_avif.h uses crude averaging formulas:
```c
case 3u: /* D45 */  val = top[dix] + (di * amp) / (bw+bh) - amp/2;
case 4u: /* D135 */ val = (top[tix] + left[tiy] + top_left) / 3 + ...;
case 5u: /* D113 */ val = (2*top[tix] + top[dix] + left[tiy]) / 4;
// etc.
```

dav1d uses the proper **Z1/Z2/Z3 angular predictors** with sub-pixel interpolation:
```c
// Z1 (angles 0-90): top-right diagonal
dx = dav1d_dr_intra_derivative[angle >> 1];
xpos = dx * (y + 1);
frac = xpos & 0x3E;  // 6-bit fraction, even-only
base = xpos >> 6;
val = ((64 - frac) * top[base] + frac * top[base + 1] + 32) >> 6;

// Z2 (angles 90-180): uses both top and left refs with dx/dy
// Z3 (angles 180-270): bottom-left diagonal, uses dy
```

**Required tables:**
```c
// dr_intra_derivative[44] — derivative lookup indexed by (angle >> 1)
static const int dr_intra_derivative[44] = {
    0, 1023, 0, 547, 372, 0, 0, 273, 215, 0, 178, 151, 0, 132, 116, 0,
    102, 0, 90, 80, 0, 71, 64, 0, 57, 51, 0, 45, 0, 40, 35, 0,
    31, 27, 0, 23, 19, 0, 15, 0, 11, 0, 7, 3
};

// mode_to_angle_map[8] — nominal angle per directional mode
static const int mode_to_angle_map[8] = { 90, 180, 45, 135, 113, 157, 203, 67 };
```

#### Angle Delta — **READ BUT DISCARDED** (line 10315)

```c
// CURRENT (stb_avif.h line 10313-10316):
if (y_mode >= 1u && y_mode <= 8u && block_size >= STBI_AVIF_BLOCK_8X8) {
    stbi_avif__av1_read_symbol_adapt(&ctx->rd,
        ctx->angle_delta_cdf[y_mode - 1u], 7);  // return value IGNORED
}
```

dav1d: `angle = mode_to_angle_map[mode - V_PRED] + 3 * (angle_delta - 3)`
(CDF symbol 0-6 maps to delta -3..+3, so actual_delta = symbol - 3)

The final angle selects which zone predictor (Z1/Z2/Z3) to use:
- angle ≤ 90 → Z1
- 90 < angle < 180 → Z2
- angle ≥ 180 → Z3
- angle == 90 exactly → V_PRED (pure vertical)
- angle == 180 exactly → H_PRED (pure horizontal)

#### DC Prediction Rounding — **APPROXIMATE**

| | stb_avif.h | dav1d |
|---|---|---|
| Square blocks | `sum / count` (truncating division) | `(sum + (count >> 1)) >> ctz(count)` (rounded) |
| Non-square 2:1 | `sum / count` | After shift, multiply by `0x5556 >> 16` |
| Non-square 4:1 | `sum / count` | After shift, multiply by `0x3334 >> 16` |

#### Smooth Weights — **CORRECT but indexing needs verification**

stb_avif.h: `stbi_avif__sm_weights[sm_bw + sm_x]` (offset = block width)
dav1d: `dav1d_sm_weights[width]` then index `weights[x]` for x=0..width-1

Both use the same offset-by-blocksize scheme. Values match ✓

However stb_avif.h has a `sm_x` scaling for non-standard sizes (`sm_x = bw > sm_bw ? (x * sm_bw) / bw : x`) which shouldn't be needed — block sizes are always powers of 2.

#### Edge Preparation — **MISSING MAJOR FEATURES**

dav1d's `ipred_prepare` does:
1. **Extended reference loading**: reads `2*bw` top pixels (into top-right) and `2*bh` left pixels (below-left)
2. **Edge filtering**: 3-tap `[4,8,4]/16`, `[5,6,5]/16`, or 5-tap `[2,4,4,4,2]/16` kernel on edges, selected by angle and block size
3. **Edge upsampling**: 4-tap `{-1,9,9,-1}/16` kernel doubles resolution for steep angles on small blocks (angle < 40 && size ≤ 16)
4. **Top-left corner filtering** for Z2: `*topleft = ((topleft[-1] + topleft[1]) * 5 + topleft[0] * 6 + 8) >> 4`

stb_avif.h has NONE of these. It loads exactly `bw+bh+2` reference pixels with simple clamping.

### 10.2 Inverse Transforms — MISSING CLIPPING

| Feature | stb_avif.h | dav1d |
|---------|-----------|-------|
| COS_BIT | 12 ✓ | 12 ✓ |
| cospi table | Matches ✓ | ✓ |
| HALF_BTF macro | Uses `long` to avoid overflow ✓ | Uses decomposed rotation to avoid overflow |
| **Inter-stage clipping** | **MISSING** | Clips to `INT16_MIN..INT16_MAX` between butterfly stages |
| **Post-row clipping** | **MISSING** | Clips to `col_clip_min..col_clip_max` after row_shift |
| Row shift values | Computed from `lw+lh` sum | Hardcoded per TX size pair (may differ for edge cases) |
| Rect2 scaling | `(coeff * 181 + 128) >> 8` ✓ | Same ✓ |
| Final normalization | `>> 4` ✓ | `(val + 8) >> 4` ✓ |
| DC-only fast path | Not implemented | `(dc * 181 + 128) >> 8` chain for rect, then column `(dc * 181 + 128 + 2048) >> 12` |

**Impact:** Missing clipping causes overflow on 10-bit content with large coefficients. For 8-bit content it's less critical but can still produce wrong values for extreme coefficient magnitudes.

### 10.3 Dequantization — **dq_shift IS WRONG FOR TX_32X32**

```c
// stb_avif.h (line 10060-10064):
if (tx2dszctx >= 6)       dequant_val >>= 2;
else if (tx2dszctx >= 5 || (txw >= 32 || txh >= 32))
                           dequant_val >>= 1;
```

```c
// dav1d:
dq_shift = imax(0, t_dim->ctx - 2);
// t_dim->ctx values: TX_32X32 → ctx=3, shift=1
// t_dim->ctx values: TX_64X64 → ctx=4, shift=2
```

| TX size | stb tx2dszctx | stb shift | dav1d ctx | dav1d shift | **Match?** |
|---------|-------------|-----------|-----------|-------------|------------|
| 4×4 | 0 | 0 | 0 | 0 | ✓ |
| 8×8 | 2 | 0 | 1 | 0 | ✓ |
| 16×16 | 4 | 0 | 2 | 0 | ✓ |
| 32×32 | 6 | **2** | 3 | **1** | **✗ WRONG** |
| 64×64 | 6 | 2 | 4 | 2 | ✓ |
| 16×32 | 5 | 1 | 3 | 1 | ✓ |
| 32×64 | 6 | 2 | 4 | 2 | ✓ |
| 8×32 | 4 (txh≥32) | 1 | 2 | 0 | **✗ WRONG** |
| 4×16 | 2 | 0 | 1 | 0 | ✓ |

**TX_32×32 gets >>2 instead of >>1** — all 32×32 TX block coefficients are halved (divided by 4 instead of 2). This causes systematic under-reconstruction.

**TX_8×32 gets >>1 instead of >>0** — the `(txw >= 32 || txh >= 32)` clause catches this wrongly.

**Fix:** Replace the heuristic with `dq_shift = max(0, max(log2w, log2h) - 2)` where `log2w`, `log2h` are the log2 of TX dimensions in pixels divided by 4, i.e.:
```c
int max_txsz_log2 = (tx_log2w > tx_log2h) ? tx_log2w : tx_log2h;
int dq_shift = max_txsz_log2 > 2 ? max_txsz_log2 - 2 : 0;
```
Where `tx_log2w/h` are 0=4px, 1=8px, 2=16px, 3=32px, 4=64px.

### 10.4 Coefficient Decode — MOSTLY CORRECT

| Feature | stb_avif.h | dav1d | Match? |
|---------|-----------|-------|--------|
| EOB bin decode | ✓ | ✓ | ✓ |
| EOB extra bits | ✓ | ✓ | ✓ |
| Scan order (square) | Hardcoded tables | Hardcoded tables | ✓ |
| Scan order (rect) | Runtime generated | Hardcoded tables | Functionally ✓ |
| Level context (2D) | `get_lower_levels_ctx_2d` | `get_lo_ctx` + offset tables | Needs audit |
| BR context | Separate functions | Inline with mag sum | Needs audit |
| Golomb coding | ✓ read_golomb for lvl≥15 | ✓ | ✓ |
| DC sign context | ✓ from neighbor entropy | ✓ | ✓ |
| **Overflow mask** | **MISSING** | `(dq * tok) & 0xffffff` for tok≥15 | **WRONG for large coefficients** |
| **cf_max clamp** | **MISSING** | `umin(dq, cf_max)` where `cf_max = ~(~127U << bpc)` | **Can overflow** |
| **QM (quant matrix)** | **MISSING** | `(dq * qm[rc] + 16) >> 5` when txtp < IDTX | **Missing feature** |
| cul_level compute | ✓ | ✓ | ✓ |

### 10.5 CFL (Chroma from Luma) — **WRONG ALGORITHM**

dav1d CFL:
1. During luma reconstruction, compute the **subsampled luma AC signal**: average neighboring luma pixels into chroma-resolution grid, then subtract the block-wide DC average
2. Store this AC signal as a temporary buffer
3. For UV prediction: `pred[x] = DC_pred[x] + clip(alpha * AC[x], -(1<<bd-1), (1<<bd-1)-1)`
4. The AC signal rounding: `(alpha * AC + 32) >> 6`

stb_avif.h CFL (line 8263-8372):
1. Computes a **per-pixel** luma average by averaging the subsample region (correct general idea)
2. But computes `y_avg` as the block-wide average, then `ac = per_pixel_luma_avg - y_avg`
3. Scales as `delta = (alpha * ac + 4) >> 3` — wrong scaling factor (should be >>6 per spec, though alpha might be pre-scaled)
4. The per-pixel approach is functionally similar but slower and may give slightly different rounding

**Key issue:** The CFL scaling factor `>> 3` differs from dav1d's `>> 6`. This could cause 8× over-prediction on chroma, producing severely wrong colors on CFL blocks.

### 10.6 Filter Intra — **IMPLEMENTED, MOSTLY CORRECT**

stb_avif.h (line 7953-8057) implements the 4×2 sub-block 7-tap filter correctly:
- Processes in 4×2 sub-blocks ✓
- 7 reference pixels per sub-block ✓
- `acc = sum(tap[i] * ref[i])`, then `(acc + 8) >> 4`, then clip ✓
- Tap table `stbi_avif__filter_intra_taps[5][8][7]` — needs to be verified against dav1d's `dav1d_filter_intra_taps[5][64]` (different memory layout but same values)

### 10.7 TX Size Selection — MOSTLY CORRECT

| Feature | stb_avif.h | dav1d | Match? |
|---------|-----------|-------|--------|
| tx_size_cdf index | `max_dim - 1` | `t_dim->max - 1` | ✓ |
| Context (tctx) | above/left TX comparison | `get_tx_ctx()` — similar comparison | Needs audit |
| Depth iteration | Manual: halve larger dim | Lookup: `t_dim->sub` | Approximate ✓ |
| nsyms | `min(max_dim, 2) + 1` | `imin(t_dim->max, 2) + 1` | ✓ |

### 10.8 Post-Processing — IMPLEMENTED

| Feature | stb_avif.h | dav1d | Status |
|---------|-----------|-------|--------|
| CDEF | ✓ (line 11777) | ✓ | Implemented (needs verification) |
| Loop Restoration (Wiener) | ✓ (line 12540) | ✓ | Implemented |
| Loop Restoration (Sgrproj) | ✓ (line 12540) | ✓ | Implemented |
| Film Grain | ✓ (line 12011) | ✓ | Implemented |
| SuperRes | ✓ (line 12854) | ✓ | Implemented |

---

## 11. Complete Missing/Wrong Feature List (sorted by estimated impact)

| # | Feature | Location | Issue | Est. MAE Impact |
|---|---------|----------|-------|-----------------|
| 1 | **V/H prediction gradient** | L8175-8176 | Spurious gradient terms | ~5-8 MAE |
| 2 | **Angular prediction (Z1/Z2/Z3)** | L8177-8196 | Crude approximations, not real algorithm | ~8-12 MAE |
| 3 | **Angle delta discarded** | L10313-10316 | Read but return value ignored | ~2-4 MAE |
| 4 | **dq_shift wrong for TX_32×32** | L10060-10064 | >>2 instead of >>1 (halves coefficients) | ~3-5 MAE |
| 5 | **dq_shift wrong for TX_8×32** | L10060-10064 | >>1 instead of >>0 | ~1-2 MAE |
| 6 | **CFL scaling factor** | L8354 | `>>3` instead of `>>6` (8× over-prediction) | ~2-4 MAE |
| 7 | **Transform inter-stage clipping** | idct/iadst functions | No clipping between butterfly stages | ~1-3 MAE |
| 8 | **DC rounding** | L8164 | `sum/count` vs `(sum+count/2)>>ctz(count)` | ~0.5-1 MAE |
| 9 | **Extended reference pixels** | L8092-8117 | Only bw+bh+2 refs, no top-right/below-left | ~1-2 MAE |
| 10 | **Edge filtering** | Not implemented | 3/5-tap filter on reference edges | ~0.5-1 MAE |
| 11 | **Edge upsampling** | Not implemented | 4-tap upsample for steep angles on small blocks | ~0.3-0.5 MAE |
| 12 | **Dequant overflow mask** | L10058 | Missing `& 0xffffff` for tok≥15 | ~0.1-0.5 MAE |
| 13 | **Dequant cf_max clamp** | L10058 | Missing upper clamp | ~0.1 MAE |
| 14 | **Quantization matrix** | Not implemented | Missing QM support (only used if enabled in frame header) | 0-1 MAE |
| 15 | **Top-left corner filtering (Z2)** | Not implemented | `(tl[-1]+tl[1])*5 + tl[0]*6 + 8) >> 4` | ~0.1 MAE |
| 16 | **Smooth weight scaling** | L8207 | Unnecessary sm_x/sm_y rescaling for non-power-of-2 | ~0 (blocks are always power-of-2) |

**Total estimated recoverable MAE: ~25-40 (current MAE is 27.9)**
