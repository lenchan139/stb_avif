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
- 8-bit, 10-bit, and 12-bit AV1 support (output is 8-bit per channel)
- Monochrome (grayscale) image support
- `desired_channels` parameter: request 1 (grayscale), 3 (RGB), or 4 (RGBA) channel output
- YUV 4:2:0 and 4:2:2 chroma subsampling
- Alpha plane support (via `iref`/`auxl` auxiliary items — decoded as separate monochrome AV1 frame)
- **Film Grain Synthesis** — auto-regressive grain template generation, piecewise-linear intensity scaling, per-block application on Y/Cb/Cr planes
- **Superres upscaling** — AV1 8-tap interpolation filter for horizontal upscaling per spec
- **Optional PNG Writer** — `stbi_avif_write_png()` and `stbi_avif_write_png_to_memory()` APIs behind `STB_AVIF_WRITE_PNG`. Supports grayscale (color_type=0), RGB (color_type=2), and RGBA (color_type=6) output.

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

Compile the test harness:

```sh
cc -std=c89 -Wall -Wextra -pedantic tests/test_decode.c -o tests/test_decode
```

Run the conversion test suite:

```sh
bash test_run.sh
```

This converts all sample AVIF files in `example_avif/` to PNG in `output_png/`.

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