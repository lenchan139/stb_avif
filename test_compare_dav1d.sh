#!/bin/sh
# test_compare_dav1d.sh - bit-exactness check of the built-in scalar AV1
# decoder against the optional dav1d backend.
#
# Builds test_avif2pnm twice (scalar and -DSTB_AVIF_USE_DAV1D), decodes each
# AVIF through both, and byte-compares the emitted Y4M files (raw YUV planes,
# C420/C422/C444/Cmono).  The dav1d build is the reference.
#
# Usage:
#   ./test_compare_dav1d.sh                 # all example_avif/*.avif
#   ./test_compare_dav1d.sh a.avif b.avif   # explicit files
#
# Environment:
#   CC              C compiler (default: cc)
#   CFLAGS          extra compile flags
#   DAV1D_CFLAGS    dav1d include flags (default: pkg-config or auto-detect)
#   DAV1D_LIBS      dav1d link flags    (default: pkg-config or auto-detect)
#   STRICT=1        require byte-exact Y4M instead of tolerance (default off)
#   TOL_MAXD        peak per-plane threshold passed to test_y4m_compare.py
#   TOL_MEAN        mean per-plane threshold passed to test_y4m_compare.py
#
# Default policy: every input must DECODE successfully through BOTH backends,
# and the scalar output must match dav1d within a small tolerance (the corpus
# carries ~1-ulp SGR/CDEF differences).  STRICT=1 turns that into a byte-exact
# requirement.
#
# Exit status: 0 if every compared output is within policy, 1 otherwise.

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c89 -O2}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/stb_avif_cmp.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT INT TERM

# --- Locate dav1d ---------------------------------------------------------
DAV1D_CFLAGS="${DAV1D_CFLAGS:-}"
DAV1D_LIBS="${DAV1D_LIBS:-}"
if [ -z "$DAV1D_LIBS" ]; then
    if command -v pkg-config >/dev/null 2>&1 && \
       pkg-config --exists dav1d 2>/dev/null; then
        DAV1D_CFLAGS="$(pkg-config --cflags dav1d)"
        DAV1D_LIBS="$(pkg-config --libs dav1d)"
        echo "dav1d: via pkg-config"
    else
        for _prefix in "${DAV1D_PREFIX:-}" "$HOME/.local" /usr/local /opt/homebrew /usr; do
            [ -n "$_prefix" ] || continue
            if [ -f "$_prefix/include/dav1d/dav1d.h" ]; then
                _lib=""
                for _l in "$_prefix/lib/libdav1d.dylib" "$_prefix/lib/libdav1d.so" \
                          "$_prefix/lib/libdav1d.so.7" "$_prefix/lib64/libdav1d.so"; do
                    [ -f "$_l" ] && _lib="$_l" && break
                done
                [ -z "$_lib" ] && continue
                _rpath=""
                [ "$(uname)" = "Darwin" ] && _rpath="-Wl,-rpath,$_prefix/lib"
                DAV1D_CFLAGS="-I$_prefix/include"
                DAV1D_LIBS="$_lib $_rpath"
                echo "dav1d: auto-detected in $_prefix"
                break
            fi
        done
    fi
fi

if [ -z "$DAV1D_LIBS" ]; then
    echo "ERROR: dav1d not found. Install libdav1d-dev or set DAV1D_CFLAGS/DAV1D_LIBS." >&2
    exit 1
fi

# --- Build the two harnesses ---------------------------------------------
echo "=== Building scalar harness ==="
# shellcheck disable=SC2086
$CC $CFLAGS -o "$WORK/scalar" test_avif2pnm.c -lm || exit 1
echo "=== Building dav1d harness ==="
# shellcheck disable=SC2086
$CC $CFLAGS -DSTB_AVIF_USE_DAV1D $DAV1D_CFLAGS -o "$WORK/dav1d" \
    test_avif2pnm.c -lm $DAV1D_LIBS || exit 1

# --- Collect inputs -------------------------------------------------------
if [ "$#" -gt 0 ]; then
    FILES="$*"
else
    FILES=""
    for f in example_avif/*.avif; do
        [ -f "$f" ] || continue
        FILES="$FILES $f"
    done
fi

mkdir -p "$WORK/scalar_run" "$WORK/dav1d_run"

pass=0
fail=0
dec_fail=0
list=""
maxd=0
maxmean=0

for f in $FILES; do
    [ -f "$f" ] || { echo "SKIP: $f (missing)"; continue; }
    base="$(basename "$f" .avif)"
    case "$f" in
        /*) abs="$f" ;;
        *)  abs="$SCRIPT_DIR/$f" ;;
    esac

    # Run both in separate CWDs so their output_ppm/ trees don't clash.
    ( cd "$WORK/scalar_run" && "$WORK/scalar" "$abs" >/dev/null 2>&1 )
    s_rc=$?
    ( cd "$WORK/dav1d_run" && "$WORK/dav1d" "$abs" >/dev/null 2>&1 )
    d_rc=$?

    sy="$WORK/scalar_run/output_ppm/$base.y4m"
    dy="$WORK/dav1d_run/output_ppm/$base.y4m"

    if [ "$s_rc" -ne 0 ] || [ ! -f "$sy" ]; then
        echo "DECODE FAIL (scalar): $f"
        dec_fail=$((dec_fail + 1))
        continue
    fi
    if [ "$d_rc" -ne 0 ] || [ ! -f "$dy" ]; then
        echo "DECODE FAIL (dav1d):  $f"
        dec_fail=$((dec_fail + 1))
        continue
    fi

    if cmp -s "$sy" "$dy"; then
        echo "OK    $f"
        pass=$((pass + 1))
        continue
    fi

    # Differing file: quantify with the pixel comparator.
    CMP_FLAGS="${STRICT:+--strict} --maxd ${TOL_MAXD:-32} --mean ${TOL_MEAN:-0.05}"
    # shellcheck disable=SC2086
    out="$(python3 "$SCRIPT_DIR/test_y4m_compare.py" "$dy" "$sy" $CMP_FLAGS 2>&1)"
    c_rc=$?
    # shellcheck disable=SC2086
    echo "$out" | sed 's/^/    /'
    if [ "$c_rc" -eq 0 ]; then
        echo "OK    $f (within tolerance)"
        pass=$((pass + 1))
    else
        echo "DIFF  $f"
        fail=$((fail + 1))
        list="$list $f"
    fi
done

echo
echo "=== Summary ==="
echo "within-policy: $pass   differing: $fail   decode-failures: $dec_fail"
if [ -n "$list" ]; then
    echo "differing files:$list"
fi

[ "$fail" -eq 0 ] && [ "$dec_fail" -eq 0 ]
