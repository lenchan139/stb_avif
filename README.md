# stb_avif

A pure C89, libc-only AVIF decoder in stb-style single-header form.

## Current Implementation Status

**Working end-to-end pipeline:** AVIF container → AV1 intra-frame decode → YUV→RGBA → PNG output.

### Implemented

- Single-header API in `stb_avif.h` (define `STB_AVIF_IMPLEMENTATION` in one translation unit)
- ISOBMFF container parsing (`ftyp`, `meta`, `pitm`, `iprp`, `iloc`, `ispe`, `av1C`)
- AV1 OBU parsing with both **reduced** and **full (non-reduced)** sequence/frame headers
- AV1 intra-frame decode: superblock partition tree, all intra prediction modes (directional, DC, smooth, paeth, CFL, filter-intra, palette), arithmetic range coder, inverse transforms (DCT/ADST/identity 4×4–32×32), dequantization
- YUV→RGBA conversion: BT.601 / BT.709 / BT.2020 (full-range and limited-range), identity matrix
- 8-bit and 10-bit (down-shifted to 8-bit output) support
- Monochrome (grayscale) image support
- `desired_channels` parameter: request 1 (grayscale), 3 (RGB), or 4 (RGBA) channel output

### Not Yet Implemented

- CDEF (Constrained Directional Enhancement Filter) — parameters parsed but filter not applied; may cause visible blocking artifacts
- Loop restoration filter (Wiener / self-guided) — parameters parsed but not applied
- Alpha plane support (auxiliary `auxl` items for transparency)
- Film grain synthesis — files with film grain parameters are rejected
- YUV 4:2:2 chroma subsampling
- Animation / multi-frame sequences

### Current v1 Target Subset

- Static AVIF only (still pictures)
- 8-bit and 10-bit content (output is always 8-bit per channel)
- No animation
- No film grain
- No embedded alpha support
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

## Next Implementation Milestones

1. CDEF filter — major visual quality improvement for lossy encodes
2. Loop restoration filter (Wiener / self-guided) — quality improvement for high-quality encodes
3. Alpha plane support — web-essential feature for transparent images
4. Film grain synthesis — completeness for encoder-emitted grain parameters