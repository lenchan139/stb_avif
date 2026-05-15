#!/bin/bash
# Debug loop script for AV1 entropy desync analysis

set -e

AVIF_FILE="/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif"
DEBUG_LOG="/tmp/dbg_levels.log"
TEST_BINARY="/tmp/test_fast"

echo "=========================================="
echo "AV1 Entropy Desync Debug Loop"
echo "=========================================="

# Check if binary exists
if [ ! -x "$TEST_BINARY" ]; then
    echo "ERROR: Test binary not found at $TEST_BINARY"
    echo "Compilation still in progress..."
    exit 1
fi

# Run decoder with debug logging
echo "Running decoder with levels debug logging..."
rm -f "$DEBUG_LOG"
"$TEST_BINARY" "$AVIF_FILE" 2>&1 | head -20

# Check if log was created
if [ ! -f "$DEBUG_LOG" ]; then
    echo "ERROR: Debug log not created"
    exit 1
fi

echo ""
echo "Debug log created: $DEBUG_LOG ($(wc -l < "$DEBUG_LOG") lines)"

# Extract mi=(0,16) blocks
echo ""
echo "Extracting mi=(0,16) coefficient contexts..."
grep -A2 "COEFF_CTX_DBG\[mi=(0,16)\]" "$DEBUG_LOG" | head -50

# Extract levels array values
echo ""
echo "Extracting levels array values..."
grep "LEVELS\[mi=(0,16)\]" "$DEBUG_LOG" | head -30

# Run analysis script
echo ""
echo "Running Python analysis..."
cd /Users/len/Downloads/gcp
python3 analyze_levels.py 2>&1 | head -80

echo ""
echo "=========================================="
echo "Debug loop complete"
echo "=========================================="
