# AV1 Entropy Desync - Investigation Summary

## Executive Summary
The AV1 decoder in `stb_avif.h` has a **catastrophic entropy desync** starting exactly at the superblock boundary (mi_col=16, pixel column 64). The decoder works correctly within the first 64x64 superblock but fails dramatically at subsequent superblocks.

## Error Characteristics

### Spatial Error Pattern
| mi_col | Pixels | Max Error | Mean Error | Status |
|--------|--------|-----------|------------|--------|
| 0-12 | 0-51 | 5 | 1.9 | ✓ Good |
| **16** | **64-71** | **174** | **38.7** | ✗ Catastrophic |
| 20 | 80-87 | 199 | 178.4 | ✗ Severe |
| 24+ | 96+ | 180-255 | 47-102 | ✗ Compounding |

### Key Observation
- **Exact boundary**: Error explodes at mi_col=16 (start of second 64x64 superblock column)
- **Compounding**: Errors increase toward the right edge of the image
- **First SB**: Within first 64x64 superblock, errors are minimal (< 5)

## Root Cause Analysis

### What We Know
1. **Partition decoding** at mi=(0,16) produces plausible structure:
   - 64x64 → SPLIT → 32x32 → SPLIT → 16x16 → HORZ_A → 8x8 blocks
   
2. **Coefficient decoding** produces values but they don't match reference:
   - EOB point decoded as 4 (leads to eob=14 non-zero coeffs)
   - Coefficient levels are high (2-3 instead of expected 0-1)
   - Dequantized residuals produce -17 instead of expected +7

3. **Context state** at mi=(0,16):
   - Range decoder: rng=46304, dif_hi=31553, cnt=12
   - txb_skip_ctx: top=0 (fresh column), lft=4 (accumulated)
   - left_entropy[0]=15 (from previous blocks in row)
   - above_entropy[16]=0 (new column, row 0)

### Hypotheses (Ranked by Likelihood)

#### H1: CDF Adaptation Bug (Most Likely)
The CDF tables for coefficient/EOB decoding have been adapted by all prior blocks. If the adaptation formula is slightly wrong, the CDFs drift, causing wrong symbols to be decoded at superblock boundaries where contexts change.

**Evidence:**
- First SB decodes correctly (CDFs still close to initial values)
- Second SB shows errors (CDFs adapted incorrectly from first SB)
- Large coefficient values suggest wrong CDF selection

**Test:** Compare CDF values at mi=(0,16) against reference decoder.

#### H2: Coefficient Context Computation Bug
The `get_lower_levels_ctx_2d` function computes which CDF to use based on neighbor coefficients. If this computation is wrong, we read from wrong CDF, getting wrong levels.

**Evidence:**
- coeff_ctx values shown in logs (21, 36, 32, 6, 2, etc.)
- No verification these contexts match reference

**Test:** Log coeff_ctx for each position and compare with reference.

#### H3: Left Entropy Context Mishandling
The `left_entropy` context at mi_col=16 might be wrong (should carry from mi_col=15 but maybe reset or computed incorrectly).

**Evidence:**
- txb_skip_ctx uses `lft=4` from `left_entropy[0]=15`
- If this context is wrong, txb_skip CDF is wrong, causing desync

**Test:** Verify left_entropy[0] at mi_col=16 matches reference.

## Debug Tools Created

### debug_avif.sh
Located at `/Users/len/Downloads/gcp/debug_avif.sh`:
```bash
./debug_avif.sh build      # Compile decoder
./debug_avif.sh reference  # Generate reference with PyAV  
./debug_avif.sh compare    # Show error by column bands
./debug_avif.sh visual     # Create side-by-side comparison
./debug_avif.sh entropy    # Run with detailed entropy logging
./debug_avif.sh analyze    # Parse entropy trace
```

### Debug Logging Added to stb_avif.h
- SEQ_FLAGS at mi=(0,0): Frame parameters, quantization, context sizes
- BLK entry: Range decoder state (rng, dif_hi, cnt, bytes_left)
- Partition decisions: bsctx, partctx, above/left, symbol decoded
- TX type: txtp_set1/set2 with symbol and CDF index
- EOB: eob_pt, hi_bit/lo_bits, final eob value
- Coefficients: coeff_ctx, position, base_sym, level
- DC sign: sign_ctx, sign value
- BR extension: br_ctx, symbols added

## Recommended Next Steps

### Immediate (High Priority)
1. **Verify CDF state at mi=(0,16)**:
   - Log `coeff_base_cdf[0][0][0..41]` values at mi=(0,16) entry
   - Compare with reference decoder (dav1d/PyAV with debug)
   - If CDF differs, adaptation is wrong

2. **Check coefficient context**:
   - Add logging: `coeff_ctx` for each position at mi=(0,16)
   - Verify against AV1 spec reference implementation
   - Focus on positions with high levels (ctx 21, 36, etc.)

### Short Term
3. **Implement reference comparison**:
   - Modify PyAV script to dump decoder state
   - Compare symbol-by-symbol at mi=(0,16)
   - Identify first divergent symbol

4. **Fix CDF adaptation if confirmed**:
   - Check `update_cdf` formula against AV1 spec §8.2.7
   - Verify rate calculation: `(4 | (count >> 4)) + (nsyms > 3)`
   - Ensure update is applied correctly per dav1d convention

### Medium Term
5. **Add regression test**:
   - Use fox.profile0.8bpc.yuv420.avif as test case
   - Assert mean error < 1.0 per 64-col band
   - CI check to prevent regression

## Files Modified
- `/Users/len/Downloads/gcp/stb_avif.h`: Added debug logging
- `/Users/len/Downloads/gcp/debug_avif.sh`: Debug workflow automation
- `/Users/len/Downloads/gcp/AV1_DESYNC_INVESTIGATION.md`: Detailed notes

## Key Commands
```bash
# Quick build and compare
cd /Users/len/Downloads/gcp
./debug_avif.sh build && ./debug_avif.sh compare

# Generate and view visual diff
./debug_avif.sh visual && open /tmp/compare.png

# Run with full entropy trace
STBI_AVIF_DBG_BLOCKS=/tmp/full.log /tmp/dump_rgb fox.avif output
```

## Conclusion
The entropy desync is **localized to superblock boundaries** and likely caused by **CDF adaptation errors** or **coefficient context computation bugs** that compound across blocks. The decoder infrastructure is sound; the issue is in the probabilistic decoding state management.
