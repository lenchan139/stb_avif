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

echo ""
echo "==> Running validation checklist..."
CHECKLIST_SRC="$SCRIPT_DIR/tests/test_checklist.c"
CHECKLIST_BIN="$SCRIPT_DIR/test_checklist_run"
cc -O2 -o "$CHECKLIST_BIN" "$CHECKLIST_SRC" -lm
"$CHECKLIST_BIN" "$INPUT_DIR"/*.avif
rm -f "$CHECKLIST_BIN"

echo ""
echo "==> Running negative / robustness tests..."
NEGATIVE_SRC="$SCRIPT_DIR/tests/test_negative.c"
NEGATIVE_BIN="$SCRIPT_DIR/test_negative_run"
cc -std=c89 -Wall -Wextra -pedantic -O2 -o "$NEGATIVE_BIN" "$NEGATIVE_SRC" -lm
"$NEGATIVE_BIN"
rm -f "$NEGATIVE_BIN"

echo "==> Done. Output in: $OUTPUT_DIR"
