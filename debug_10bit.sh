#!/bin/bash
# Debug 10-bit AV1 decoding issues
# Runs various diagnostic checks to help identify the root cause

echo "=========================================="
echo "10-bit AV1 Decoding Debug Script"
echo "=========================================="
echo ""

# Check if bc is available (needed for floating point comparison)
if ! command -v bc &> /dev/null; then
    echo "WARNING: 'bc' not found. Installing..."
    # Try to install bc (works on macOS with Homebrew)
    if command -v brew &> /dev/null; then
        brew install bc 2>/dev/null || echo "Please install bc manually"
    fi
fi

# Build fresh test decoder
echo "[1/5] Building test decoder..."
cc -O2 -o test_decode_run tests/test_decode.c -lm 2>&1 || {
    echo "ERROR: Failed to compile"
    exit 1
}
echo "  ✓ Build successful"
echo ""

# Run test suite
echo "[2/5] Running test suite..."
./test_run.sh 2>&1 | tail -5
echo "  ✓ Test suite complete"
echo ""

# Measure MAE
echo "[3/5] Measuring MAE..."
python3 tools/mae_calc.py 2>&1 | tee mae_results.txt | grep -E "fox|File|---"
echo ""

# Check for spatial errors
echo "[4/5] Analyzing spatial errors..."
echo "  10-bit fox spatial analysis:"
python3 tools/spatial_error.py example_avif/fox.profile0.10bpc.yuv420.avif 2>&1 | head -15
echo ""
echo "  8-bit fox spatial analysis:"
python3 tools/spatial_error.py example_avif/fox.profile0.8bpc.yuv420.avif 2>&1 | head -15
echo ""

# Compare coefficients using PyAV if available
echo "[5/5] Checking PyAV availability..."
python3 -c "import av; print('  ✓ PyAV available')" 2>/dev/null || echo "  ✗ PyAV not available (pip install av)"
echo ""

# Summary
echo "=========================================="
echo "Summary"
echo "=========================================="
FOX_10BPC_Y_MAE=$(grep "fox.profile0.10bpc.yuv420.avif.*Y" mae_results.txt | awk '{print $3}')
FOX_8BPC_Y_MAE=$(grep "fox.profile0.8bpc.yuv420.avif.*Y" mae_results.txt | awk '{print $3}')

echo "8-bit fox Y MAE:  $FOX_8BPC_Y_MAE"
echo "10-bit fox Y MAE: $FOX_10BPC_Y_MAE"
echo ""

if command -v bc &> /dev/null; then
    RATIO=$(echo "scale=2; $FOX_10BPC_Y_MAE / $FOX_8BPC_Y_MAE" | bc)
    echo "Ratio (10-bit / 8-bit): ${RATIO}x"
    echo "Expected ratio from qstep: ~4x"
    echo ""
    
    if (( $(echo "$RATIO > 6" | bc -l) )); then
        echo "⚠️  RATIO TOO HIGH - indicates additional error source beyond quantization"
        echo "   Investigate: CDF adaptation, coefficient context, transform"
    else
        echo "✓ Ratio within expected range"
    fi
fi

# Cleanup
rm -f mae_results.txt test_decode_run

echo ""
echo "=========================================="
echo "Debug complete"
echo "=========================================="
