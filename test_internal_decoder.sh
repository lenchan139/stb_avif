#!/bin/sh
# test_internal_decoder.sh - Build and test the C89 internal AV1 decoder
# Usage: ./test_internal_decoder.sh
#
# This builds stb_avif.h with STB_AVIF_USE_C89_DAV1D (internal C89 decoder)
# and runs it against all example_avif/*.avif files.
#
# Unlike the external dav1d path, this requires no external library.
# The C89 decoder is self-contained within stb_avif.h.
#
# Current status: INCOMPLETE - the C89 decoder is not yet fully functional.
# See plan.md for task breakdown.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/output_ppm_c89"

echo "=== C89 Internal Decoder Test ==="
echo ""
echo "This builds stb_avif.h with STB_AVIF_USE_C89_DAV1D"
echo "The internal C89 decoder is a work-in-progress (see plan.md)"
echo ""

# Build
echo "--- Building ---"
cc -std=c89 -Wall -Wextra -D STB_AVIF_USE_C89_DAV1D \
   -o "${SCRIPT_DIR}/test_c89_decoder" \
   "${SCRIPT_DIR}/test_avif2png.c" -lm 2>&1 | \
   grep -E "error:|warning:" | grep -v "long long" | grep -v "unused" || true

echo ""
echo "--- Running ---"
mkdir -p "$OUT_DIR"

# Run with timeout
if command -v perl >/dev/null 2>&1; then
    perl -e 'alarm 30; exec @ARGV' "${SCRIPT_DIR}/test_c89_decoder" 2>&1 || \
        echo "WARNING: timed out or failed (expected during development)"
else
    "${SCRIPT_DIR}/test_c89_decoder" 2>&1 || true
fi

echo ""
echo "=== Done ==="
echo "Output PPM files: ${OUT_DIR}/"
echo ""

# Show results
PASS=0
FAIL=0
for f in "${SCRIPT_DIR}"/example_avif/*.avif; do
    base="$(basename "$f" .avif)"
    if [ -f "${OUT_DIR}/${base}.ppm" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
done
echo "Results: ${PASS} passed, ${FAIL} failed (${PASS} of $((PASS + FAIL)) files produced PPM output)"
