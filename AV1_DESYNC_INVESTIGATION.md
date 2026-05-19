# AV1 Entropy Desync Investigation Summary

## Current Status
- **Decoder**: stb_avif.h - AV1 keyframe decoder
- **Test file**: fox.profile0.8bpc.yuv420.avif (1204x800)
- **Error magnitude**: Mean error ~68, Max error 255
- **Error pattern**: Starts at col 0-63 (mean 28), compounds to 100+ at right edge

## Key Findings

### 1. Error Localization
- Pixel (64,0): Ref=56, Ours=32, Error=-24
- Prediction is correct (filter_intra DC mode gives 49)
- Residual is wrong: Ours=-17, Expected=+7
- Issue is in **coefficient decoding**, not prediction

### 2. Partition Structure at mi=(0,16)
```
PART mi=(0,16) bs=12 => SPLIT (symbol 3)
PART mi=(0,16) bs=9 => SPLIT (symbol 3)
PART mi=(0,16) bs=6 => HORZ_A (symbol 4) - potentially suspicious
BLK mi=(0,16) bs=3 (8x8 leaf)
```

### 3. Coefficient Decoding at mi=(0,16)
- TX type: FLIPADST_DCT (symbol 2 from txtp_set1)
- EOB point: 4 (symbol from eob_multi16_cdf[0][0])
- EOB: 14 non-zero coefficients in 4x4 block (very dense)
- Coefficient levels: 0, 2, -1, 1, -3, etc. (large magnitudes)

### 4. Suspected Root Cause
The high EOB (14) and large coefficient values suggest either:
- **Option A**: Block actually has high energy (unlikely for smooth image region)
- **Option B**: EOB symbol was decoded incorrectly due to CDF desync
- **Option C**: Wrong CDF table used for coefficient context

## Debug Tools Created

### debug_avif.sh
```bash
./debug_avif.sh build      # Build decoder
./debug_avif.sh reference  # Generate reference with PyAV
./debug_avif.sh compare    # Compare outputs, show error bands
./debug_avif.sh visual     # Create visual comparison
./debug_avif.sh entropy    # Run with entropy tracing
./debug_avif.sh analyze    # Analyze entropy trace
```

## Next Steps to Verify

### 1. Confirm EOB Decode Correctness
Need to verify if eob=14 is correct or if it should be smaller (e.g., eob=3-4).

### 2. Check CDF State at mi=(0,16)
The eob_multi16_cdf[0][0] has been adapted by all prior 4x4 blocks. If adaptation is wrong, we'd decode wrong symbols.

### 3. Validate Coefficient Context
The coeff_ctx computation uses neighbor levels. If this is wrong, we read from wrong CDF, getting wrong levels.

## Hypothesis: Coefficient Context Bug

The `get_lower_levels_ctx_2d` function computes context based on already-decoded neighbor coefficients. If there's a bug in:
- The padded index calculation
- The neighbor access pattern
- The magnitude computation

Then we'd compute wrong `coeff_ctx`, read from wrong CDF, and decode wrong levels.

## Recommended Fix Investigation

1. **Add coeff_ctx tracing**: Log coeff_ctx for every coefficient at mi=(0,16)
2. **Compare with reference**: Use dav1d/PyAV to get expected coeff_ctx values
3. **Fix context computation**: Correct the bug in get_lower_levels_ctx_2d or neighbor access

## Files Modified
- `/Users/len/Downloads/gcp/stb_avif.h`: Added debug logging for BR coefficients, signs, EOB
- `/Users/len/Downloads/gcp/debug_avif.sh`: Debug workflow automation

## Commands for Quick Testing
```bash
cd /Users/len/Downloads/gcp
./debug_avif.sh build && ./debug_avif.sh compare
```
