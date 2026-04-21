#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SCRIPT_DIR/tests/test_decode.c"
BIN="$SCRIPT_DIR/test_decode_run"
NEG_SRC="$SCRIPT_DIR/tests/test_negative.c"
NEG_BIN="$SCRIPT_DIR/test_negative_run"
CLI_SRC="$SCRIPT_DIR/tools/avif2png.c"
CLI_BIN="$SCRIPT_DIR/avif2png_run"
INPUT_DIR="$SCRIPT_DIR/example_avif"
OUTPUT_DIR="$SCRIPT_DIR/output_png"

# Compile main decode harness
echo "==> Compiling test_decode..."
cc -O2 -o "$BIN" "$SRC" -lm
echo "==> Build complete: $BIN"

# Compile negative test suite
echo "==> Compiling test_negative..."
cc -O2 -o "$NEG_BIN" "$NEG_SRC" -lm
echo "==> Build complete: $NEG_BIN"

# Compile CLI converter
echo "==> Compiling avif2png..."
cc -O2 -o "$CLI_BIN" "$CLI_SRC" -lm
echo "==> Build complete: $CLI_BIN"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Convert each .avif to .png via the main test harness
echo "==> Converting AVIF files to PNG (test_decode)..."
for avif in "$INPUT_DIR"/*.avif; do
    [ -f "$avif" ] || continue
    base="$(basename "${avif%.avif}")"
    out="$OUTPUT_DIR/${base}.png"
    echo "  $base.avif -> $base.png"
    "$BIN" "$avif" "$out"
done

# Also convert a representative file via the CLI converter as a smoke test
echo "==> Smoke-testing avif2png CLI..."
for avif in "$INPUT_DIR"/*.avif; do
    [ -f "$avif" ] || continue
    base="$(basename "${avif%.avif}")"
    out="$OUTPUT_DIR/${base}_cli.png"
    "$CLI_BIN" "$avif" "$out"
    break  # one file is enough for a smoke test
done

# Run negative / robustness tests
echo "==> Running negative tests..."
"$NEG_BIN"

# Cleanup build artifacts
echo "==> Cleaning up build files..."
rm -f "$BIN" "$NEG_BIN" "$CLI_BIN"

echo "==> Done. Output in: $OUTPUT_DIR"
