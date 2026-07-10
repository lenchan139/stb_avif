#!/bin/sh
# c89_gen.sh - Decode example_avif/*.avif -> output_c89/*.ppm using C89 internal decoder
# Usage: ./c89_gen.sh [avif_file ...]
#   No args: processes all example_avif/*.avif
#   With args: processes only the specified files

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)" || exit 1
OUT_DIR="${SCRIPT_DIR}/output_c89"
TIMEOUT_SEC=60
PASS=0
FAIL=0

# Determine which files to process (use set -- for zsh compat)
if [ $# -gt 0 ]; then
    set -- "$@"
else
    set -- "${SCRIPT_DIR}/example_avif/"*.avif
fi

echo "=== C89 AVIF -> PPM Generator ==="
echo ""

# Build the decoder tool
echo "Building..."
cc -std=c89 -Wall -D STB_AVIF_USE_C89_DAV1D \
   -o "${SCRIPT_DIR}/avif2ppm" \
   "${SCRIPT_DIR}/avif2ppm.c" -lm 2>&1 | grep -E "error:" || true
if [ ! -f "${SCRIPT_DIR}/avif2ppm" ]; then
    echo "BUILD FAILED"
    exit 1
fi
echo "Build OK"
echo ""

# Create output directory
mkdir -p "${OUT_DIR}"

for AVIF; do
    [ -f "${AVIF}" ] || continue

    BASENAME=$(basename "${AVIF}" .avif)
    OUT_PPM="${OUT_DIR}/${BASENAME}.ppm"

    printf "  %-44s " "${BASENAME}"

    # Run with timeout
    "${SCRIPT_DIR}/avif2ppm" "${AVIF}" "${OUT_PPM}" 3 > /tmp/_avif2ppm.txt 2>&1 &
    BGPID=$!
    (sleep ${TIMEOUT_SEC} && kill ${BGPID} 2>/dev/null) &
    SLEEPPID=$!
    wait ${BGPID} 2>/dev/null || true
    kill ${SLEEPPID} 2>/dev/null || true

    if [ -f "${OUT_PPM}" ]; then
        echo "OK"
        PASS=$((PASS + 1))
    else
        cat /tmp/_avif2ppm.txt 2>/dev/null | head -3
        echo "FAIL"
        FAIL=$((FAIL + 1))
    fi
done

rm -f /tmp/_avif2ppm.txt

echo ""
echo "=== Summary ==="
echo "Passed: ${PASS}  Failed: ${FAIL}"
exit ${FAIL}
