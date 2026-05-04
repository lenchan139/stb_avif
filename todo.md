# AV1 Decoder C89 — Full Implementation TODO

All work targets `stb_avif.h` (single-header C89). Verify after each item:
```bash
cc -std=c89 -Wall -Wextra -pedantic-errors tests/test_decode.c -o /tmp/td -lm
python3 tools/mae_calc.py
bash test_run.sh
```

---

## CRITICAL — Transforms & Dequantization (MAE impact: ~20-30)

- [x] **Fix dq_shift for TX_32×32** (`stbi_avif__av1_read_coeffs_after_skip` ~L10828)
  - Replace `max_dim >= 32` heuristic with `dq_shift = max(0, log2(max(txw,txh)/4) - 1)`
  - TX_32×32: must be >>1 (currently >>2 — halves all coefficients)
  - TX_8×32: must be >>0 (currently >>1 — wrongly shifts)
  - Formula: `lw = log2(txw>>2)`, `lh = log2(txh>>2)`, `dq_shift = max(lw,lh) > 1 ? max(lw,lh)-1 : 0`

- [x] **Fix row_shift table** (`stbi_avif__av1_inverse_transform_2d_rect` ~L10455)
  - Replace per-sum switch with per-dimension formula
  - dav1d rule: `row_shift = (lw>=4 && lh>=4 && lw==lh) ? 2 : (lw>=4 || lh>=4) ? 1 : 0`
  - where lw=log2(txw/4), lh=log2(txh/4). Current lw+lh sum is ambiguous (16×16 vs 8×32 both sum to 4 but need different shifts)

- [x] **Add post-row-transform clipping** (`stbi_avif__av1_inverse_transform_2d_rect` ~L10479)
  - After row shift loop, clamp each `temp[i*txw+j]` to `[stbi_avif__inv_clip_min, stbi_avif__inv_clip_max]`
  - Without this, 10-bit content overflows int16 range in the column stage

- [x] **Add rect2 column scaling for tall TX** (~L10487)
  - When `txh == 2*txw` (tall), apply `(buf[i] * 181 + 128) >> 8` to column inputs
  - Currently only applied to row inputs for wide TX

---

## CRITICAL — Intra Prediction (MAE impact: ~15-20)

- [x] **Replace angular prediction with correct Z1/Z2/Z3** (`stbi_avif__av1_predict_block_ex` ~L8730)
  - Fixed `dr_intra_derivative` table to 44 entries per AV1 spec
  - Updated indexing to use `angle >> 1` for correct lookup
  - Z1/Z2/Z3 implementation already present, now uses correct derivative values

- [x] **Apply angle delta in prediction** (~L11282, passed to predict_block)
  - `actual_angle = mode_to_angle[mode] + angle_delta * 3` — verified correct ✓
  - Angle delta properly read and passed to prediction functions ✓

- [ ] **Extend reference pixel loading** (`stbi_avif__av1_predict_block_ex` ~L8520)
  - Load up to `2*max(bw,bh)` top pixels and `2*max(bw,bh)` left pixels
  - Compute `have_above_right` and `have_below_left` from partition context
  - Pad unavailable pixels by repeating last available value

- [ ] **Add intra edge filtering for angular modes** (~L8580)
  - 3-tap `[4,8,4]/16` filter on reference edges when angle not cardinal
  - 5-tap `[2,4,4,4,2]/16` for larger blocks
  - 4-tap upsample `{-1,9,9,-1}/16` for steep angles (angle<40 && block<=16)

---

## HIGH — Coefficient Decode (MAE impact: ~2-5)

- [x] **Fix CFL scaling: `>>3` → `>>6`** (`stbi_avif__av1_apply_cfl_plane` ~L9020)
  - Current: `delta = (cfl_term + ...) >> 6` — verified correct ✓
  - dav1d: `Round2Signed(alpha * ac, 6)` = `(alpha*ac + (alpha*ac>=0 ? 32 : -32)) >> 6` ✓

- [ ] **Audit level context functions** (~L10679-10714)
  - Verify `stbi_avif__av1_get_lower_levels_ctx_2d` matches dav1d `get_lo_ctx`
  - Verify `stbi_avif__av1_get_br_ctx_2d` and `_dc` match dav1d

- [ ] **Verify scan order for TX_CLASS_H and TX_CLASS_V** (~L10578)
  - TX_CLASS_H: scan in col-major within each row group
  - TX_CLASS_V: scan in row-major within each col group
  - Check against dav1d hardcoded scan tables

- [x] **Add dequant overflow clamp** (~L10841)
  - After `dequant_val = lvl * qstep`, apply `& 0xFFFFFF` for large tok values
  - Already present as `& 0xffffff` — verified applied before dq_shift ✓

---

## MEDIUM — DC Prediction Rounding (MAE impact: ~1)

- [ ] **Fix DC average rounding** (`stbi_avif__av1_predict_block_ex` ~L8730)
  - Replace `dc = sum / count` with `dc = (sum + (count >> 1)) >> ctz(count)`
  - For non-power-of-2 count (mixed top+left with different bw/bh):
    use `dc_multiplier_1x2` / `dc_multiplier_1x4` tables:
    ```c
    /* 2:1 ratio: (sum * 0x5556 + (1<<15)) >> 16  */
    /* 4:1 ratio: (sum * 0x3334 + (1<<15)) >> 16  */
    ```

---

## MEDIUM — Post-Processing Verification (MAE impact: ~1-3)

- [ ] **Verify CDEF strength and direction computation** (`stbi_avif__av1_cdef_filter`)
  - Compare direction/variance output vs dav1d on test blocks

- [ ] **Verify Loop Restoration (Wiener) coefficients** (`stbi_avif__av1_lr_filter`)
  - Tap precision: Wiener uses 7-tap `{-w[0], w[1], -w[2], 128+sum, -w[2], w[1], -w[0]}`
  - Verify rounding: `(sum + 64) >> 7`

- [ ] **Verify SGR-Proj parameters** (`stbi_avif__av1_lr_filter`)
  - sgr_params table must match AV1 spec Table 3

- [ ] **Verify YUV→RGB for 10-bit limited range** (`stbi_avif__av1_planes_to_rgba` ~L12542)
  - BT.709 limited: Y in [64,940], Cb/Cr in [64,960] (for 10-bit)
  - Ensure matrix and range from sequence header / nclx box are passed correctly

---

## LOW — Missing Features (required for full spec compliance)

- [ ] **Filter intra tap verification** (~L7953)
  - Compare stbi_avif__filter_intra_taps[5][8][7] vs dav1d_filter_intra_taps layout

- [ ] **Palette mode** — currently skipped/stub
  - Read palette colors, use palette map during prediction

- [ ] **IntraBC** — blocked at entry (`allow_intrabc` → return error)
  - Implement block-copy motion vectors within the same frame

- [ ] **QMatrix support** — blocked at entry (`using_qmatrix` → return error)
  - Embed QM tables or fetch at decode time

- [ ] **Multi-frame / animation** — single frame only
  - Frame timeline, disposal/blend modes

- [ ] **Inter prediction** — blocked at entry (still-picture only)
  - Motion compensation, reference frames

---

## TESTING — Regression after each fix

```bash
# Build strict C89
cc -std=c89 -Wall -Wextra -pedantic-errors tests/test_decode.c -o /tmp/td -lm

# MAE vs PyAV reference (target: Y<5 for 8-bit, Y<20 for 10-bit)
python3 tools/mae_calc.py

# Full test suite (all 8 images must pass)
bash test_run.sh

# Visual check
open /tmp/out_steam_2253100.ppm
```

**Current MAE baseline (2026-05-03):**
| Image | Y | U | V |
|-------|---|---|---|
| steam_2253100 (10-bit 444) | 283 | 50 | 51 |
| fox 10bpc YUV420 | 333 | 34 | 27 |
| red-at-12-oclock 10bpc | 354 | 86 | 83 |
| fox 8bpc YUV420 | 43 | 9 | 8 |
| kimono | 44 | 12 | 12 |
| G-0trmK (large) | 71 | 7 | 9 |
| Gb5RU6R | 61 | 9 | 8 |
| G-0trmK-thumb | 77 | 3 | 5 |

**Target:** Y MAE < 5 (8-bit), < 20 (10-bit) after critical fixes.
