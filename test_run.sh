#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SCRIPT_DIR/tests/test_decode.c"
BIN="$SCRIPT_DIR/test_decode_run"
INPUT_DIR="$SCRIPT_DIR/example_avif"
OUTPUT_DIR="$SCRIPT_DIR/output_png"

# Compile
echo "==> Compiling..."
cc -O2 -o "$BIN" "$SRC" -lm
echo "==> Build complete: $BIN"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Convert each .avif to .png
echo "==> Converting AVIF files to PNG..."
for avif in "$INPUT_DIR"/*.avif; do
    [ -f "$avif" ] || continue
    base="$(basename "${avif%.avif}")"
    out="$OUTPUT_DIR/${base}.png"
    echo "  $base.avif -> $base.png"
    "$BIN" "$avif" "$out"
done

# Cleanup build artifact
echo "==> Cleaning up build files..."
rm -f "$BIN"

echo "==> Done. Output in: $OUTPUT_DIR"
