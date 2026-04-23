# stb_avif

A pure C89, libc-only AVIF decoder in stb-style single-header form.

## Current Implementation Status

**Working end-to-end pipeline:** AVIF container → AV1 intra-frame decode → YUV→RGBA → PNG output.

### Implemented

- Single-header API in `stb_avif.h` (define `STB_AVIF_IMPLEMENTATION` in one translation unit)
- ISOBMFF container parsing (`ftyp`, `meta`, `pitm`, `iprp`, `iloc`, `ispe`, `av1C`, `colr`)
- **Color profile parsing** — `colr` box support for ICC profiles (`prof`/`rICC`) and `nclx` color info (primaries, transfer characteristics, matrix coefficients, full range flag)
- AV1 OBU parsing with both **reduced** and **full (non-reduced)** sequence/frame headers
- AV1 intra-frame decode: superblock partition tree, all intra prediction modes (directional, DC, smooth, paeth, CFL, filter-intra, palette), arithmetic range coder, inverse transforms (DCT/ADST/identity 4×4–64×64), dequantization
- **Segmentation-based quantization** — per-block segment_id entropy decoding with SEG_LVL_ALT_Q delta application. Supports `SegIdPreSkip` for correct bitstream ordering per AV1 spec.
- **Deblocking (Loop) Filter** — edge-adaptive filtering on Y/U/V planes using parsed loop filter levels, sharpness, and ref/mode deltas. Applied before CDEF per AV1 spec pipeline order.
- **CDEF (Constrained Directional Enhancement Filter)** — direction finding, primary/secondary tap filtering with constrained damping on Y, U, V planes.
- **Loop Restoration Filter** — Wiener (7-tap symmetric separable convolution) and Sgrproj (self-guided box filter with projection). Applied per-plane after CDEF with per-unit parameters from the tile bitstream.
- YUV→RGBA conversion: BT.601 / BT.709 / BT.2020 (full-range and limited-range), identity matrix
- **Interpolated chroma upsampling** — bilinear averaging of adjacent chroma samples during YUV→RGBA conversion for 4:2:0 and 4:2:2 content (replaces nearest-neighbor fetch)
- 8-bit, 10-bit, and 12-bit AV1 support (output is 8-bit per channel)
- Monochrome (grayscale) image support; `channels_in_file` reports 1 for monochrome AVIFs
- `desired_channels` parameter: request 1 (grayscale), 3 (RGB), or 4 (RGBA) channel output
- YUV 4:2:0 and 4:2:2 chroma subsampling
- Alpha plane support (via `iref`/`auxl` auxiliary items — decoded as separate monochrome AV1 frame)
- **Film Grain Synthesis** — auto-regressive grain template generation, piecewise-linear intensity scaling, per-block application on Y/Cb/Cr planes
- **Superres upscaling** — AV1 8-tap interpolation filter for horizontal upscaling per spec
- **Optional PNG Writer** — `stbi_avif_write_png()` and `stbi_avif_write_png_to_memory()` APIs behind `STB_AVIF_WRITE_PNG`. Supports grayscale (color_type=0), RGB (color_type=2), and RGBA (color_type=6) output.
- **`avif2png` CLI converter** — `tools/avif2png.c`, a minimal command-line tool built on the public API (no dependencies beyond `stb_avif.h`).
- **Hardened container parser** — overflow guards on multi-extent size accumulation and 8-byte extended box sizes on 32-bit platforms; malformed/truncated AVIF inputs produce clean error messages via `stbi_avif_failure_reason()`.

### Not Yet Implemented

- Animation / multi-frame sequences
- Inter-frame prediction (P/B frames)
- Intra block copy (IntraBC) for screen content

### Current v1 Target Subset

- Static AVIF only (still pictures)
- 8-bit, 10-bit, and 12-bit content (output is always 8-bit per channel)
- No animation
- Pure C89 (uses `long long` for range decoder only)
- No external dependencies except libc

## Example

```c
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

int w, h, n;
unsigned char *rgba = stbi_avif_load("image.avif", &w, &h, &n, 4);
if (!rgba) {
    printf("error: %s\n", stbi_avif_failure_reason());
}
```

### PNG Writer Example

```c
#define STB_AVIF_IMPLEMENTATION
#define STB_AVIF_WRITE_PNG
#include "stb_avif.h"

int w, h, n;
unsigned char *rgba = stbi_avif_load("image.avif", &w, &h, &n, 4);
if (rgba) {
    stbi_avif_write_png("output.png", rgba, w, h, 4);
    stbi_avif_image_free(rgba);
}
```

## Build and Test

Compile the test harness (strict C89):

```sh
cc -std=c89 -Wall -Wextra -pedantic tests/test_decode.c -o tests/test_decode -lm
```

Compile the negative / robustness test suite (strict C89):

```sh
cc -std=c89 -Wall -Wextra -pedantic tests/test_negative.c -o tests/test_negative -lm
```

Run the full test suite (conversion regression + negative tests + CLI smoke test):

```sh
bash test_run.sh
```

This compiles three binaries, converts all sample AVIF files in `example_avif/` to PNG
in `output_png/`, runs the negative tests, and smoke-tests the `avif2png` CLI tool.

### Debug tracing

To trace internal container / AV1 bitstream / reconstruction state line-by-line,
compile with `-DSTBI_AVIF_DEBUG_TRACE`:

```sh
cc -O2 -DSTBI_AVIF_DEBUG_TRACE tests/test_decode.c -o /tmp/td -lm
/tmp/td input.avif out.png 2>&1 | grep '^\[stb_avif\]'
```

Traces are emitted at every major stage (`ftyp`, `pitm`, `iloc`, container
summary, each OBU, sequence header, frame header, tile group, RGBA output).
You can also `#define STBI_AVIF_TRACE(...)` before including `stb_avif.h` to
route trace output to a custom sink. With the macro undefined (default) the
trace calls expand to nothing.

For entropy-symbol diffing against a reference decoder, `-DSTBI_AVIF_TRACE_SYMBOLS`
prints per-symbol range-decoder state. Because this can be very large, you can
limit it with compile-time filters:

```sh
# trace only one adaptive symbol callsite (line number in stb_avif.h)
cc -O2 -DSTBI_AVIF_TRACE_SYMBOLS -DSTBI_AVIF_TRACE_SYMBOLS_LINE=11467 tests/test_decode.c -o /tmp/td -lm

# or stop after N symbol decodes
cc -O2 -DSTBI_AVIF_TRACE_SYMBOLS -DSTBI_AVIF_TRACE_SYMBOLS_MAX_EVENTS=2000 tests/test_decode.c -o /tmp/td -lm
```

## `avif2png` CLI Converter

A minimal, self-contained command-line converter is provided in `tools/avif2png.c`.
It uses only the public `stb_avif.h` API — no logic is duplicated from the test harness.

Build:

```sh
cc -O2 tools/avif2png.c -o avif2png -lm
```

Usage:

```sh
avif2png input.avif output.png
```

Exit codes: `0` success, `1` wrong usage, `2` decode error, `3` write error.

## Post-Processing Pipeline Order

Per the AV1 specification, the post-processing pipeline applies in this order:

1. **Deblocking Filter** — edge-adaptive loop filter on block boundaries
2. **CDEF** — constrained directional enhancement
3. **Loop Restoration** — Wiener / Sgrproj per-unit filtering
4. **Superres** — horizontal upscaling (if enabled)
5. **Film Grain** — grain synthesis overlay
6. **YUV→RGBA** — color space conversion + alpha merge

## Next Implementation Milestones

1. Animation / multi-frame sequence support
2. Intra block copy (IntraBC) for screen content
