#!/bin/sh
# gen_dav1d_ref.sh - Decode example_avif/*.avif -> output_ppm_dav1d/*.ppm using libdav1d
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/output_ppm_dav1d"
PASS=0
FAIL=0

# Use set -- for zsh compat
set -- "${SCRIPT_DIR}/example_avif/"*.avif

echo "=== Generate dav1d reference PPMs ==="
echo ""

# Build decoder
cc -std=c89 -D STB_AVIF_USE_DAV1D \
   -I"${HOME}/.local/include" \
   -o "${SCRIPT_DIR}/_dav1d_decode" \
   "${SCRIPT_DIR}/test_dav1d_ppm.c" \
   -L"${HOME}/.local/lib" -ldav1d -lm 2>&1 | grep -E "error:" || true
if [ ! -f "${SCRIPT_DIR}/_dav1d_decode" ]; then
    echo "BUILD FAILED (need libdav1d in ~/.local)"
    exit 1
fi
echo "Build OK"
echo ""

mkdir -p "${OUT_DIR}"

for AVIF; do
    [ -f "${AVIF}" ] || continue
    BASENAME=$(basename "${AVIF}" .avif)
    OUT_PPM="${OUT_DIR}/${BASENAME}.ppm"

    printf "  %-44s " "${BASENAME}"
    "${SCRIPT_DIR}/_dav1d_decode" "${AVIF}" "${OUT_PPM}" 2>/dev/null
    if [ -f "${OUT_PPM}" ]; then
        echo "OK"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        FAIL=$((FAIL + 1))
    fi
done

rm -f "${SCRIPT_DIR}/_dav1d_decode"

echo ""
echo "=== Summary ==="
echo "Passed: ${PASS}  Failed: ${FAIL}"
exit ${FAIL}
