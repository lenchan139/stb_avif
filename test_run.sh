#!/bin/sh
# test_run.sh - Decode AVIF sample images and convert to PNG for visual verification
#
# Usage:
#   ./test_run.sh              - decode all example_avif/*.avif
#   ./test_run.sh file1.avif ... - decode specific files
#
# Dependencies: cc (C compiler), sips (macOS) or ImageMagick (convert)
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/output_png"
PPM_DIR="${SCRIPT_DIR}/output_ppm"

# --- Step 1: Compile the decoder ---
echo "=== Compiling stb_avif decoder ==="
DAV1D_FLAGS=""
DAV1D_LIBS=""
if command -v pkg-config >/dev/null 2>&1; then
    DAV1D_CFLAGS="$(pkg-config --cflags dav1d 2>/dev/null || true)"
    DAV1D_LIBS="$(pkg-config --libs dav1d 2>/dev/null || true)"
    if [ -n "$DAV1D_LIBS" ]; then
        DAV1D_FLAGS="-D STB_AVIF_USE_DAV1D ${DAV1D_CFLAGS}"
        echo "  dav1d detected via pkg-config"
    fi
fi
if [ -z "$DAV1D_FLAGS" ]; then
    for _libdir in /usr/local/lib /opt/homebrew/lib "$HOME/.local/lib"; do
        if [ -f "$_libdir/libdav1d.dylib" ] || [ -f "$_libdir/libdav1d.7.dylib" ]; then
            _incdir="${_libdir%/lib}/include"
            DAV1D_FLAGS="-D STB_AVIF_USE_DAV1D -I$_incdir"
            DAV1D_LIBS="$_libdir/libdav1d.7.dylib"
            echo "  dav1d detected in $_libdir"
            break
        fi
    done
fi
if [ -n "$DAV1D_FLAGS" ]; then
    echo "  Building WITH dav1d (correct output)"
else
    echo "  Building WITHOUT dav1d (using the built-in scalar decoder)"
fi

cc -std=c89 -Wall -Wextra ${DAV1D_FLAGS} -o "${SCRIPT_DIR}/test_avif2png" \
   "${SCRIPT_DIR}/test_avif2pnm.c" -lm ${DAV1D_LIBS} 2>&1
echo "Compilation OK"
echo

# --- Step 2: Create output directories ---
mkdir -p "$OUT_DIR" "$PPM_DIR"

# --- Step 3: Run decoder to produce PPM files ---
echo "=== Decoding AVIF -> PPM ==="
TOTAL=0
if [ $# -gt 0 ]; then
    TOTAL=$#
else
    for f in "${SCRIPT_DIR}"/example_avif/*.avif; do
        TOTAL=$((TOTAL + 1))
    done
fi
echo "Files to process: ${TOTAL}"
echo

# Run with a 120-second total safety limit
# (uses timeout if available, otherwise runs directly)
if command -v timeout >/dev/null 2>&1; then
    CMD="timeout 120"
elif command -v gtimeout >/dev/null 2>&1; then
    CMD="gtimeout 120"
else
    CMD=""
fi

if [ $# -gt 0 ]; then
    if [ -n "$CMD" ]; then
        $CMD "${SCRIPT_DIR}/test_avif2png" "$@" || echo "WARNING: some files timed out"
    else
        "${SCRIPT_DIR}/test_avif2png" "$@"
    fi
else
    if [ -n "$CMD" ]; then
        $CMD "${SCRIPT_DIR}/test_avif2png" || echo "WARNING: some files timed out"
    else
        "${SCRIPT_DIR}/test_avif2png"
    fi
fi
echo

# --- Step 4: Convert PPM -> PNG ---
echo "=== Converting PPM -> PNG ==="
# Prefer sips (macOS), fall back to ImageMagick convert
if command -v sips >/dev/null 2>&1; then
    CONVERT_TOOL="sips"
elif command -v convert >/dev/null 2>&1; then
    CONVERT_TOOL="convert"
else
    echo "WARNING: Neither 'sips' nor ImageMagick 'convert' found."
    echo "PPM files are in ${PPM_DIR}/"
    echo "Convert manually, or view PPM files directly."
    CONVERT_TOOL=""
fi

if [ -n "$CONVERT_TOOL" ]; then
    for ppm in "${PPM_DIR}"/*.ppm; do
        [ -f "$ppm" ] || continue
        base="$(basename "$ppm" .ppm)"
        png="${OUT_DIR}/${base}.png"
        if [ "$CONVERT_TOOL" = "sips" ]; then
            sips -s format png "$ppm" --out "$png" >/dev/null 2>&1
        else
            convert "$ppm" "$png"
        fi
        echo "  -> $png"
    done
fi

echo
echo "=== Done ==="
echo "PNG outputs: ${OUT_DIR}/"
echo "PPM outputs: ${PPM_DIR}/"
echo

# --- Step 5: Open for visual inspection (optional) ---
if [ -n "$CONVERT_TOOL" ] && [ -d "$OUT_DIR" ]; then
    first_png="$(ls "${OUT_DIR}"/*.png 2>/dev/null | head -1)"
    if [ -n "$first_png" ]; then
        echo "Opening first output for visual inspection..."
        open "$first_png" 2>/dev/null || echo "(could not open)"
    fi
fi
