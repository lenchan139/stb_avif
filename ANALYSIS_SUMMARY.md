# AV1 Entropy Desync Analysis Summary

## Problem
Entropy desynchronization occurs at superblock boundary mi_col=16 when decoding `fox.profile0.8bpc.yuv420.avif`.

## Key Finding: Coefficient Context Mismatch

At mi=(0,18), coefficient c=5 (pos=8) shows **ctx=7** when expected is **ctx=6**.

### Debug Log Evidence
```
mi=(0,18), ts=0 (TX_4X4), txtp=2 (DCT_ADST), eob=8
  c=6, pos=12, ctx=6
  c=5, pos=8, ctx=7   <- MISMATCH: expected 6
  c=4, pos=5, ctx=7   <- MISMATCH: expected 6
  c=3, pos=2, ctx=6
  c=2, pos=1, ctx=2   <- MISMATCH: expected 1
```

### Root Cause Analysis

1. **Scan Order Verified**: Positions (12, 8, 5, 2, 1) match 4x4 scan table
2. **CDF Values Valid**: All CDFs have correct sentinel (32768)
3. **Range Decoder**: State is continuous across boundary
4. **Context Calculation**: ctx=7 implies base_ctx=1 (non-zero neighbor magnitude)

### The Bug

For c=5 (pos=8, x=2, y=0):
- Expected offset = 6 (from nz_map_5x5[0][0][2])
- Expected ctx = 6 (with zero neighbors)
- Actual ctx = 7

This means `base_ctx = 7 - 6 = 1`, indicating a neighbor has non-zero magnitude when it should be zero.

### Hypothesis: levels Array Not Properly Zeroed

The `levels` array is declared as:
```c
unsigned char levels[(64 + 4) * (66) + 8];  // 4496 bytes
```

And initialized with:
```c
memset(levels, 0, sizeof(levels));
```

But the context mismatch suggests either:
1. `memset` is not working (unlikely)
2. `levels` contains garbage from previous block
3. Padded index calculation is wrong
4. Neighbor indices are calculated incorrectly

### Debug Code Added

1. **levels array verification** (line 10622-10628):
   ```c
   /* Debug: verify levels is zeroed at mi=(0,16) */
   if (ctx->dbg_blocks_fp && ctx->dbg_blocks_fp != (void *)1
       && ctx->mi_row == 0 && ctx->mi_col == 16) {
      int sum = 0;
      for (int i = 0; i < 100; i++) sum += levels[i];
      fprintf((FILE*)ctx->dbg_blocks_fp, "  LEVELS_INIT[mi=(0,16)]: sum_first_100=%d (should be 0)\n", sum);
   }
   ```

2. **Coefficient context debug** (line 10772-10788):
   - Prints c, pos, coeff_ctx at mi=(0,16)
   - Prints 5 neighbor levels from padded buffer

### Scripts Created

1. **analyze_levels.py** - Extracts and verifies coefficient contexts
2. **debug_loop.sh** - Quick debug workflow
3. **run_full_analysis.sh** - Complete analysis pipeline

### Next Steps

1. Compile and run with new debug code
2. Verify `LEVELS_INIT` shows sum=0 (confirms memset works)
3. Check neighbor level values at c=5 decode
4. Identify which neighbor is non-zero
5. Fix the indexing bug

### Compilation Status

The test binary is still compiling (stb_avif.h is ~2.5MB, ~17K lines).
Once `/tmp/minimal_test` is ready, run:
```bash
/Users/len/Downloads/gcp/run_full_analysis.sh
```

## Files Modified

- `/Users/len/Downloads/gcp/stb_avif.h` (lines 10622-10628, 10772-10788)

## Files Created

- `/Users/len/Downloads/gcp/analyze_levels.py`
- `/Users/len/Downloads/gcp/debug_loop.sh`
- `/Users/len/Downloads/gcp/run_full_analysis.sh`
- `/Users/len/Downloads/gcp/ANALYSIS_SUMMARY.md`
