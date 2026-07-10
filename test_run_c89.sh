#!/bin/sh
# test_run_c89.sh - Build and test C89 decoder on all AVIF files, report PSNR vs dav1d
# Usage: ./test_run_c89.sh [avif_file ...]
#   No args: tests all example_avif/*.avif
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)" || exit 1
PASS_THRESHOLD=30.0
ALL_PASS=0; ALL_FAIL=0; ALL_SKIP=0
TOTAL_PSNR=0; COUNT=0
TIMEOUT_SEC=${TIMEOUT:-30}

test_one() {
    local avif="$1" ref_ppm="$2"
    local base out psnr

    base=$(basename "${avif}" .avif)

    # Generate per-file test
    cat > "${SCRIPT_DIR}/_t.c" << TESTEOF
#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void) {
    const char *fn = "${avif}";
    const char *ref_fn = "${ref_ppm}";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { printf("DECODE_FAIL"); free(data); return 1; }
    FILE *fd = fopen(ref_fn, "rb"); if (!fd) return 1;
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    sscanf(hdr, "%d %d", &w, &h); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(w*h*3));
    fread(ref, 1, (size_t)(w*h*3), fd); fclose(fd);
    int i; long long ssd = 0; int bad = 0, maxdiff = 0;
    for (i = 0; i < w * h * 3; i++) {
        int d = img[i] - ref[i]; if (d < 0) d = -d;
        if (d) bad++; if (d > maxdiff) maxdiff = d;
        ssd += (long long)d * d;
    }
    double psnr = 10.0 * log10(255.0 * 255.0 * (w * h * 3) / (double)ssd);
    printf("PSNR=%.2f diff=%d %.1f%% maxdiff=%d", psnr, bad, 100.0*bad/(w*h*3), maxdiff);
    free(img); free(data); free(ref);
    return 0;
}
TESTEOF

    cc -std=c89 -D STB_AVIF_USE_C89_DAV1D \
       -o "${SCRIPT_DIR}/_t" "${SCRIPT_DIR}/_t.c" -lm 2>/dev/null || {
        echo "COMPILE_FAIL"; rm -f "${SCRIPT_DIR}/_t.c"
        return 2
    }
    rm -f "${SCRIPT_DIR}/_t.c"

    # Run with background + sleep kill timeout
    "${SCRIPT_DIR}/_t" > /tmp/_psnr.txt 2>/dev/null &
    BGPID=$!
    (sleep "${TIMEOUT_SEC}" && kill "${BGPID}" 2>/dev/null) &
    SLEEPPID=$!
    wait "${BGPID}" 2>/dev/null || true
    kill "${SLEEPPID}" 2>/dev/null || true
    rm -f "${SCRIPT_DIR}/_t"

    out=$(cat /tmp/_psnr.txt 2>/dev/null || echo "")
    rm -f /tmp/_psnr.txt

    psnr=$(echo "${out}" | grep -oE 'PSNR=[0-9.]+' | cut -d= -f2)
    if [ -n "${psnr}" ]; then
        echo "${psnr} dB"
        return 0
    elif echo "${out}" | grep -q "DECODE_FAIL"; then
        echo "DECODE_FAIL"
        return 1
    else
        echo "TIMEOUT"
        return 1
    fi
}

echo "=== C89 Internal Decoder Test ==="
echo "Building..."
cc -std=c89 -Wall -D STB_AVIF_USE_C89_DAV1D \
   -o "${SCRIPT_DIR}/test_c89_ppm" \
   "${SCRIPT_DIR}/test_c89_ppm.c" -lm 2>&1 | grep -E "error:" || true
if [ ! -f "${SCRIPT_DIR}/test_c89_ppm" ]; then echo "BUILD FAILED"; exit 1; fi
echo "Build OK"
echo ""

if [ $# -gt 0 ]; then
    FILES="$@"
else
    FILES="${SCRIPT_DIR}/example_avif/*.avif"
fi

for AVIF in ${FILES}; do
    [ -f "${AVIF}" ] || continue
    BASENAME=$(basename "${AVIF}" .avif)
    REF_PPM="${SCRIPT_DIR}/output_ppm_dav1d/${BASENAME}.ppm"
    if [ ! -f "${REF_PPM}" ]; then
        printf "  %-44s SKIP (no ref)\n" "${BASENAME}"
        ALL_SKIP=$((ALL_SKIP + 1)); continue
    fi
    printf "  %-44s " "${BASENAME}"

    set +e
    RESULT=$(test_one "${AVIF}" "${REF_PPM}" 2>&1)
    STATUS=$?
    set -e

    PSNR=$(echo "${RESULT}" | grep -oE '^[0-9.]+' | head -1)
    if [ "${STATUS}" -eq 0 ] && [ -n "${PSNR}" ]; then
        TOTAL_PSNR=$(echo "${TOTAL_PSNR} + ${PSNR}" | bc -l 2>/dev/null || echo 0)
        COUNT=$((COUNT + 1))
        echo "${PSNR} dB"
        cmp=$(echo "${PSNR} >= ${PASS_THRESHOLD}" | bc -l 2>/dev/null || echo 0)
        [ "${cmp}" = "1" ] && ALL_PASS=$((ALL_PASS + 1)) || ALL_FAIL=$((ALL_FAIL + 1))
    else
        echo "${RESULT}"
        ALL_FAIL=$((ALL_FAIL + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Tested: ${COUNT}  Passed: ${ALL_PASS}  Failed: ${ALL_FAIL}  Skipped: ${ALL_SKIP}"
if [ "${COUNT}" -gt 0 ]; then
    AVG=$(echo "scale=2; ${TOTAL_PSNR} / ${COUNT}" | bc -l 2>/dev/null || echo "?")
    echo "Average PSNR: ${AVG} dB  (target: ${PASS_THRESHOLD} dB)"
fi
[ "${ALL_FAIL}" -eq 0 ]
