#!/bin/sh
# test_run_c89.sh - Build and test C89 decoder on all AVIF files, report PSNR vs dav1d
# Usage: ./test_run_c89.sh [avif_file ...]
#   No args: tests all example_avif/*.avif
#   With args: tests only the specified files

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS_THRESHOLD=30.0
ALL_PASS=0
ALL_FAIL=0
ALL_SKIP=0
TOTAL_PSNR=0
COUNT=0

# Determine which files to test
if [ $# -gt 0 ]; then
    FILES="$@"
else
    FILES="${SCRIPT_DIR}/example_avif/*.avif"
fi

echo "=== C89 Internal Decoder Test ==="
echo ""

# Build once
echo "--- Building ---"
cc -std=c89 -Wall -D STB_AVIF_USE_C89_DAV1D \
   -o "${SCRIPT_DIR}/test_c89_ppm" \
   "${SCRIPT_DIR}/test_c89_ppm.c" -lm 2>&1 | \
   grep -E "error:" || true

if [ ! -f "${SCRIPT_DIR}/test_c89_ppm" ]; then
    echo "BUILD FAILED"
    exit 1
fi
echo "Build OK"
echo ""

for AVIF in ${FILES}; do
    [ -f "${AVIF}" ] || continue

    BASENAME=$(basename "${AVIF}" .avif)
    REF_PPM="${SCRIPT_DIR}/output_ppm_dav1d/${BASENAME}.ppm"

    if [ ! -f "${REF_PPM}" ]; then
        printf "  %-40s SKIP (no ref PPM)\n" "${BASENAME}"
        ALL_SKIP=$((ALL_SKIP + 1))
        continue
    fi

    printf "  %-40s " "${BASENAME}"

    # Write per-file PSNR test using heredoc with quoted delimiter
    cat > "${SCRIPT_DIR}/_test_psnr.c" << TESTEOF
#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void) {
    const char *fn = "${AVIF}";
    const char *ref_fn = "${REF_PPM}";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    if (!data) return 1;
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stdout, "DECODE_FAIL"); free(data); return 1; }
    FILE *fd = fopen(ref_fn, "rb"); if (!fd) { free(img); free(data); return 1; }
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    sscanf(hdr, "%d %d", &w, &h); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(w*h*3));
    if (!ref) { free(img); free(data); fclose(fd); return 1; }
    fread(ref, 1, (size_t)(w*h*3), fd); fclose(fd);
    int i; long long ssd = 0; int bad = 0, maxdiff = 0;
    for (i = 0; i < w * h * 3; i++) {
        int d = img[i] - ref[i]; if (d < 0) d = -d;
        if (d) bad++;
        if (d > maxdiff) maxdiff = d;
        ssd += (long long)d * d;
    }
    double mse = (double)ssd / (w * h * 3);
    double psnr = 10.0 * log10(255.0 * 255.0 / mse);
    fprintf(stdout, "diff=%d (%.1f%%) maxdiff=%d PSNR=%.2f\\n", bad, 100.0*bad/(w*h*3), maxdiff, psnr);
    free(img); free(data); free(ref);
    return bad > (w*h*3/2) ? 1 : 0;
}
TESTEOF

    cc -std=c89 -D STB_AVIF_USE_C89_DAV1D \
       -o "${SCRIPT_DIR}/_test_psnr" \
       "${SCRIPT_DIR}/_test_psnr.c" -lm 2>/dev/null || {
        echo "COMPILE_FAIL"
        rm -f "${SCRIPT_DIR}/_test_psnr.c" "${SCRIPT_DIR}/_test_psnr"
        ALL_FAIL=$((ALL_FAIL + 1))
        continue
    }

    # Run and capture stdout only (the MSAC progress goes to stderr)
    OUTPUT=$("${SCRIPT_DIR}/_test_psnr" 2>/dev/null) || true
    rm -f "${SCRIPT_DIR}/_test_psnr.c" "${SCRIPT_DIR}/_test_psnr"

    PSNR=$(echo "${OUTPUT}" | grep -oE 'PSNR=[0-9.]+' | cut -d= -f2)

    if [ -n "${PSNR}" ]; then
        echo "PSNR=${PSNR} dB"
        TOTAL_PSNR=$(echo "${TOTAL_PSNR} + ${PSNR}" | bc -l 2>/dev/null || echo "0")
        COUNT=$((COUNT + 1))
        if [ "$(echo "${PSNR} >= ${PASS_THRESHOLD}" | bc -l 2>/dev/null || echo 0)" = "1" ]; then
            ALL_PASS=$((ALL_PASS + 1))
        else
            ALL_FAIL=$((ALL_FAIL + 1))
        fi
    else
        echo "FAIL"
        ALL_FAIL=$((ALL_FAIL + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Tested: ${COUNT}, Passed: ${ALL_PASS}, Failed: ${ALL_FAIL}, Skipped: ${ALL_SKIP}"
if [ "${COUNT}" -gt 0 ]; then
    AVG_PSNR=$(echo "scale=2; ${TOTAL_PSNR} / ${COUNT}" | bc -l 2>/dev/null || echo "?")
    echo "Average PSNR: ${AVG_PSNR} dB (target: ${PASS_THRESHOLD} dB)"
fi

[ "${ALL_FAIL}" -eq 0 ]
