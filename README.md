# stb_avif

A small, single-header **C89 AVIF decoder** with a built-in scalar AV1
decoder.

This project is inspired by the design philosophy of the
[stb](https://github.com/nothings/stb) libraries: keep the decoder
self-contained, portable, and easy to integrate into applications that
need AVIF image decoding without a large codec framework.

The project is **functionally complete** for the still-image profile. The
built-in scalar AV1 decoder is now **bit-exact vs dav1d 1.5.4** on the
full test corpus (44 files, 8/10/12-bit, `4:2:0`/`4:2:2`/`4:4:4`, monochrome,
CDEF + loop restoration) when comparing 8-bit truncated YUV via the
`test_avif2pnm` harness.

## Features

### AVIF / ISOBMFF

The AVIF layer currently handles the parts of the HEIF/ISOBMFF container
needed by the test corpus, including:

-   `ftyp`
-   `meta`
-   `iloc`
-   `iinf`
-   `iprp`
-   `ipco`
-   `ipma`
-   `ispe`
-   `pixi`
-   `colr`
-   `av1C`
-   AV1 image items and auxiliary alpha items
-   Multiple AV1 tile groups
-   Multi-tile AVIF images
-   Auxiliary alpha images

The decoder ultimately returns ordinary RGB/RGBA pixels through the
public API.

### AV1

The built-in scalar decoder is split into small headers covering
individual parts of AV1:

-   OBU parsing
-   Sequence headers
-   Frame headers
-   Tile parsing and decoding
-   Partition decoding
-   Intra prediction
-   Transform decoding and inverse transforms
-   Quantization/dequantization
-   Coefficient decoding
-   MSAC entropy decoding
-   CDF tables
-   Deblocking
-   CDEF
-   Loop restoration
-   Palette modes
-   Segmentation support
-   8/10/12-bit sample handling
-   4:2:0, 4:2:2 and 4:4:4 pixel formats

The decoder uses a 64-bit unsigned integer type where required:

``` c
#if defined(_MSC_VER)
    typedef unsigned __int64 stbv_u64;
#else
    typedef unsigned long long stbv_u64;
#endif
```

This is intentional: some AV1 arithmetic cannot be implemented safely
using only 32-bit integers.

## Public API

The main interface is in `stb_avif.h`.

``` c
unsigned char *stb_avif_load_from_memory(
    const unsigned char *data,
    int len,
    int *x,
    int *y,
    int *channels,
    int req_channels);
```

Free the returned image with:

``` c
void stb_avif_free(void *ptr);
```

The last error can be obtained with:

``` c
const char *stb_avif_failure_reason(void);
```

### Basic usage

Put `stb_avif.h` in your project and define `STB_AVIF_IMPLEMENTATION` in
exactly one C source file:

``` c
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
```

Then:

``` c
int x, y, channels;
unsigned char *pixels;

pixels = stb_avif_load_from_memory(
    data, data_size,
    &x, &y, &channels,
    4);

if (!pixels) {
    printf("AVIF decode failed: %s\n",
           stb_avif_failure_reason());
    return 1;
}

/* Use pixels here. */

stb_avif_free(pixels);
```

`req_channels` may be:

-   `0` --- use the image's natural channel count
-   `3` --- RGB
-   `4` --- RGBA

## Optional dav1d backend

For comparison and for applications that require a mature AV1 decoder,
the project can use [dav1d](https://code.videolan.org/videolan/dav1d)
instead of the built-in scalar AV1 decoder.

Define:

``` c
#define STB_AVIF_USE_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
```

and link with dav1d:

``` sh
cc test.c -ldav1d
```

The dav1d backend is particularly useful when validating the built-in
decoder: the same AVIF file can be decoded through both implementations
and the resulting pixels compared.

## Test program

`test_avif2pnm.c` is a simple test driver which:

1.  Reads an AVIF file.
2.  Decodes it through `stb_avif`.
3.  Writes a PPM/PGM image.
4.  Writes a sidecar PGM when an alpha plane is available.

Build it with a C89 compiler:

``` sh
cc -std=c89 -o test_avif2pnm test_avif2pnm.c -lm
```

Run it on individual files:

``` sh
./test_avif2pnm image.avif
```

or use the built-in test list:

``` sh
./test_avif2pnm
```

On Windows, the same source can be built with a C compiler supporting
the required C89 features.

## Test corpus

The development tree currently contains a collection of AVIF files
covering different AV1 configurations, including:

-   8-bit 4:2:0
-   10-bit 4:2:0
-   8-bit 4:4:4
-   10-bit 4:4:4
-   10-bit 4:2:2
-   12-bit samples
-   monochrome images
-   palette-coded images
-   multi-tile images
-   images with auxiliary alpha
-   images using CDEF and loop restoration

Some particularly useful regression samples are:

``` text
kimono.avif
steam_2253100.avif
app-icon.avif
G-0trmKXsAA1sQZ.avif
avif-yuv444p.avif
avif-yuv444p10le.avif
fox.profile1.10bpc.yuv444.avif
```

These are intentionally kept as part of the development/test corpus
because they exercise different AV1 features.

## Current development status (2026-09-17)

The built-in scalar decoder is **verified bit-exact vs dav1d 1.5.4**
on the entire checked corpus:

-   `example_avif/*.avif` 31 files + `Nature-avif/*.avif` 13 files = **44 files**
-   `test_avif2pnm.exe` (scalar) vs `test_avif2pnm.exe -DSTB_AVIF_USE_DAV1D` (dav1d)
    full pipeline, 8-bit truncated Y4M (`C420`/`C422`/`C444`/`Cmono`)
-   Result: **44/44 `100.00%` exact, `maxd 0`, `>3 =0` on Y/U/V** (e.g. `hato.profile2.12bpc.yuv422.monochrome.avif` `3078x2048` mono, `avif-yuv444p10le.avif` `6000x4000` `C444` 10-bit, `sander-wehkamp` etc.)

Recent fixes that closed the last gaps:

-   **Deblock** `stb_av1_deblock.h:28` — `E/I/H <<= bd8`, `clip ±128<<bd8` (was 8-bit only → 10/12-bit mono off by 6)
-   **Deblock map** `stb_av1_deblock.h:222,272` / `stb_avif.h:2240` — packed `lw|lh<<3` (horizontal edges used `lw`)
-   **CDEF** `stb_av1_cdef.h:389,408,485` — `y_pri>>2` scale, `adjust_strength` `+8>>4` rounding, `422`-only dir remap, `fw8/fh8` padded extent + `INT16_MIN` sentinel, per-stripe `lpf = lpf+(ys-2)*stride`
-   **Loop restoration** `stb_av1_lr.h:1468` — sbrow stripe grid `[0,56)[56,120)...` + half-unit round-half-up merge, pristine `src_copy` for Wiener/SGR (dav1d backup), SGR `mix` ring fix `ptrs[2]` `stb_av1_lr.h:1109`
-   **Reconstruction padding** `stb_avif.h:2155` — right/bottom writes to `stride`/`+64` rows, deleted `extend_right_edge_u16` replicate clobber
-   **Harness** `test_avif2pnm.c:119` — `last_frame_info()`-based `C420/C422/C444/Cmono` Y4M, MSVC `C2143` `cw_p/ch_p` hoist; `stb_avif.h:1738,1766` dav1d `I400` monochrome `NULL` U/V + `stride[2]` fix

Stage-isolated checks (`-DSTB_AVIF_NO_CDEF -DSTB_AVIF_NO_LR`, `-DSTB_AVIF_NO_LR`) are also `100%` on the same corpus; no-filter is `100%`. The remaining `Y4M` path is intentionally 8-bit truncated for `10/12bpc` (both decoders truncate `src>>bpc-8` identically).

## Design goals

The main goals of the project are:

-   Single-header public API
-   C89-compatible implementation
-   No mandatory external codec dependency
-   Small, understandable source files
-   Scalar C implementation
-   Reuse of well-understood AV1/dav1d algorithms where practical
-   Ability to compare the internal decoder against dav1d
-   Support for real-world AVIF images rather than only minimal
    conformance samples

The AV1 implementation is deliberately split into multiple internal
headers so individual codec components can be developed and tested
without turning `stb_avif.h` into an unmaintainable monolithic
implementation.

## Source layout

``` text
dist/stb_avif.h
    All functions
```


## Source layout (for development)

``` text
stb_avif.h
    AVIF/ISOBMFF parsing and public API

stb_av1_avifbox.h
    AV1/AVIF container-related helpers

stb_av1_obu.h
    OBU parsing

stb_av1_seqhdr.h
    AV1 sequence header

stb_av1_framehdr.h
    AV1 frame header

stb_av1_tile.h
    Tile geometry and tile bitstream handling

stb_av1_tile_decode.h
    Tile decoding

stb_av1_partition.h
stb_av1_partition_decode.h
    Block partitioning

stb_av1_leaf.h
    Leaf/block syntax and reconstruction state

stb_av1_intra.h
stb_av1_ipred.h
    Intra prediction

stb_av1_tx.h
stb_av1_txstate.h
    Transform syntax/state

stb_av1_itx.h
stb_av1_itx1d.h
    Inverse transforms

stb_av1_coef.h
    Coefficient decoding

stb_av1_quant.h
    Quantization/dequantization

stb_av1_msac.h
    MSAC entropy decoder

stb_av1_cdf.h
    AV1 CDF tables

stb_av1_cdef.h
    CDEF

stb_av1_deblock.h
    Deblocking filter

stb_av1_lr.h
    Loop restoration

test_avif2pnm.c
    Development/test program

build_single_header.py
    Python script for creating distributable single file stb_avif.h in dist/ 
```

## References

The implementation is developed with reference to:

-   [AV1 Bitstream & Decoding Process
    Specification](https://aomediacodec.github.io/av1-spec/)
-   [dav1d](https://code.videolan.org/videolan/dav1d)
-   [libavif](https://github.com/AOMediaCodec/libavif)
-   [ISOBMFF / ISO/IEC
    14496-12](https://www.iso.org/standard/83102.html)
-   [HEIF / ISO/IEC 23008-12](https://www.iso.org/standard/74428.html)

The project repository is:

https://github.com/roytam1/stb_avif

## License

The project is intended to be released into the public domain, following
the style of the stb libraries.

Where the public-domain dedication is not recognized, you are granted a
perpetual, irrevocable license to use, copy, modify, and distribute this
software for any purpose.

See:

https://creativecommons.org/publicdomain/zero/1.0/

## Contributing / debugging

When adding or fixing AV1 functionality, it is useful to compare the
scalar decoder with dav1d using the same AVIF input.

When reporting a decoding problem, please include:

-   the AVIF sample
-   image dimensions
-   bit depth
-   chroma format
-   whether the image uses multiple tiles
-   whether an auxiliary alpha item is present
-   the generated output
-   whether `STB_AVIF_USE_DAV1D` produces the expected output

Small, feature-specific regression samples are especially valuable
because AV1 syntax errors often desynchronize the entropy decoder
several symbols before the visible corruption appears.
