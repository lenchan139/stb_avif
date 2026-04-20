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

- [ ] Remove gradient from V_PRED: `val = (int)top[x]`
- [ ] Remove gradient from H_PRED: `val = (int)left[y]`
- [ ] Store and use angle_delta in prediction (currently read but discarded)
- [ ] Fix DC rounding to use `(sum + (count >> 1)) >> log2(count)`
- [ ] Verify SMOOTH weights table indices are correct for non-square blocks
- [ ] Check ref_count limit (currently `bw + bh + 2`, should be `2 * max(bw, bh)`)
