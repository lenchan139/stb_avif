#!/bin/bash
# Complete AV1 entropy desync analysis workflow

set -e

echo "=========================================="
echo "AV1 Entropy Desync Analysis Pipeline"
echo "=========================================="

# Configuration
AVIF_FILE="/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif"
TEST_BINARY="/tmp/test_fast"
DEBUG_LOG="/tmp/dbg_levels.log"
REF_PNG="/tmp/ref_fox.png"
DECODED_PPM="/tmp/decoded_fox.ppm"

cd /Users/len/Downloads/gcp

# Step 1: Check binary
if [ ! -x "$TEST_BINARY" ]; then
    echo "ERROR: Test binary not found. Compilation may still be in progress."
    echo "Checking for compilation process..."
    ps aux | grep -E "cc|clang" | grep -v grep | head -2
    exit 1
fi
echo "[1/5] Test binary ready: $(ls -la "$TEST_BINARY" | awk '{print $5}') bytes"

# Step 2: Run decoder with debug logging
echo "[2/5] Running decoder with levels debug..."
rm -f "$DEBUG_LOG"
"$TEST_BINARY" "$AVIF_FILE" 2>&1 | head -20
if [ ! -f "$DEBUG_LOG" ]; then
    echo "ERROR: Debug log not created"
    exit 1
fi
echo "      Debug log: $(wc -l < "$DEBUG_LOG") lines"

# Step 3: Extract key debug info
echo "[3/5] Extracting coefficient context data at mi=(0,16)..."
echo ""
echo "=== Coefficient Context at mi=(0,16) ==="
grep "COEFF_CTX_DBG\[mi=(0,16)\]" "$DEBUG_LOG" | head -20
echo ""
echo "=== Levels Array Values at mi=(0,16) ==="
grep "LEVELS\[mi=(0,16)\]" "$DEBUG_LOG" | head -20

# Step 4: Run Python analysis
echo ""
echo "[4/5] Running Python analysis..."
python3 analyze_levels.py 2>&1

# Step 5: Compare with reference (if available)
echo ""
echo "[5/5] Generating decoded output for comparison..."
if command -v convert >/dev/null 2>&1; then
    # Use ImageMagick to convert if available
    echo "      ImageMagick available for comparison"
else
    echo "      Install ImageMagick for visual comparison: brew install imagemagick"
fi

echo ""
echo "=========================================="
echo "Analysis complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Review the levels array values above"
echo "2. Compare contexts with reference decoder (dav1d)"
echo "3. Identify which neighbor has incorrect non-zero value"
echo "4. Fix the levels array indexing bug in stb_avif.h"
echo ""
echo "Debug files:"
echo "  - $DEBUG_LOG"
echo "  - analyze_levels.py (analysis script)"
echo "  - stb_avif.h (modified with debug code)"
