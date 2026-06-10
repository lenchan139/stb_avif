# stb_avif.h — Development Plan

## Project Overview
A single-header C89 AVIF decoder. The header parses ISOBMFF/HEIF containers correctly.
For actual AV1 pixel decoding, there are two paths:
1. **External dav1d** (`-D STB_AVIF_USE_DAV1D -ldav1d`) — works, produces correct output
2. **Internal C89 decoder** (`-D STB_AVIF_USE_C89_DAV1D`) — work in progress, not yet working

## Current State (June 2025)

### Working
- ISOBMFF/HEIF container parsing: ftyp, meta, hdlr, pitm, iloc, iinf, iprp, ipco, av1C, ispe, pixi, mdat
- av1C codec configuration (bit depth, chroma subsampling, monochrome)
- AV1 OBU parsing (sequence header, frame header, tile group)
- Frame dimensions from ISPE box + fallback from AV1 bitstream
- 10-bit → 8-bit downsampling for internal planes
- YUV→RGB conversion with correct matrix selection (BT.601, BT.709, BT.2020)
- Limited/full color range handling
- Progress reporting with ETA during decode
- dav1d external backend integration (compile with `-D STB_AVIF_USE_DAV1D -ldav1d`)

### C89 Internal Decoder — Key Lesson
**You cannot mix the old simple Boolean decoder with the new MSAC.**
They are fundamentally incompatible:

| Aspect | Old Bool Decoder | New MSAC (dav1d) |
|---|---|---|
| Byte reading | Raw bytes (no XOR) | XOR-inverted (`byte ^ 0xFF`) |
| Bit precision | 8-bit range (128-255) | 15-bit range (32768-65535) |
| Symbol decoding | Binary only (0/1) | Multi-symbol CDF (up to 16) |
| Init state | `range=128, value=0` | `rng=0x8000, dif=0` |

Attempting a compatibility layer (`#define` wrappers) failed because:
1. The init processes produce completely different decoder states
2. The XOR inversion flips all bit interpretations
3. Mid-stream switching corrupts the arithmetic decoder state

**The MSAC must be used from the FIRST bit read** — it must replace the entire
old decoder, not just the coefficient/intra decoding. This requires porting
the full dav1d OBU parsing + header parsing + tile decoding to use the MSAC.

### What a Complete C89 Port Would Need
| Component | Lines | C89 Status |
|---|---|---|
| **MSAC** (msac.h + msac.c) | 330 | ✅ Ported in `msac_c89.h` |
| **CDF tables** (cdf.c) | 4,065 | ❌ |
| **OBU parser** (obu.c) | 1,695 | ❌ |
| **Main decoder** (decode.c) | 3,746 | ❌ |
| **Reconstruction** (recon_tmpl.c) | 2,310 | ❌ |
| **Transforms** (itx_1d.c + itx_tmpl.c) | 1,393 | ❌ |
| **Intra prediction** (ipred_tmpl.c + intra_edge.c) | ~500 | ❌ |
| **CDEF filter** (cdef_apply_tmpl.c) | 308 | ❌ |
| **Loop restoration** (looprestoration_tmpl.c) | ~500 | ❌ |
| **Context/env** (ctx.c + env.h) | ~600 | ❌ |
| **Total** | **~19,000** | ❌ |

These files are in `dav1d_ref/` ready for C89 porting.

### C89-ification Rules
- Replace `// comments` with `/* */`
- Move all declarations to top of blocks
- Replace `uint8_t` → `unsigned char` (or `stbv_u8`)
- Replace `uint16_t` → `unsigned short` (or `stbv_u16`)
- Replace `uint32_t` → `unsigned int` (or `stbv_u32`)
- Replace `int64_t` → `signed long long` (or `stbv_s64`)
- Remove `inline` keyword
- Remove `restrict` keyword
- Remove function pointer casts between incompatible types
- Replace `for (int i = 0; ...)` → `int i; for (i = 0; ...)`
- Remove ASM-specific code paths
- Compound literals → manual struct init
- Designated initializers → sequential initializers
- `static_assert` → runtime assert or remove

## Architecture

```
stb_avif_load_from_memory()
  ├── ISOBMFF/HEIF parser (always active)
  │   ├── ftyp → verify AVIF brand
  │   ├── meta → scan sub-boxes
  │   │   ├── hdlr → verify picture handler
  │   │   ├── pitm → get primary item ID
  │   │   ├── iloc → get data offsets (version 0/1/2)
  │   │   ├── iprp → ipco → av1C, ispe, pixi
  │   │   └── ipma → property associations
  │   └── mdat → extract compressed AV1 data
  │
  ├── AV1 OBU parsing (old decoder, works but wrong bit reading)
  │   ├── Temporal delimiter
  │   ├── Sequence header → dimensions, color config
  │   ├── Frame header → quantization, filters
  │   └── Tile group / Frame → tile data position
  │
  ├── Decode path selection:
  │   ├── STB_AVIF_USE_DAV1D → dav1d library → real pixels
  │   └── default → old internal decoder → garbage ("snow")
  │
  └── YUV→RGB conversion (always active)
      ├── Matrix: BT.601 / BT.709 / BT.2020 based on sequence header
      ├── Range: limited (16-235) or full (0-255)
      └── Output: RGBA (4 channels)
```

## File Structure
```
stb_avif/
├── stb_avif.h          — Main library (3,287 lines, C89)
├── test_avif2png.c     — Test: decode AVIF → PPM
├── test_run.sh          — Build + test + convert to PNG
├── test_avif.c          — Basic test harness
├── msac_c89.h           — C89 port of dav1d's MSAC (reference, 392 lines)
├── dav1d_ref/           — dav1d source files for reference
│   ├── msac.h, msac.c
│   ├── cdf.c, cdf.h
│   ├── decode.c
│   ├── obu.c, obu.h
│   ├── ctx.c, ctx.h
│   ├── env.h
│   ├── internal.h
│   └── ... (26 files total)
├── example_avif/        — Test AVIF images (8 files)
├── output_ppm/          — Decoded PPM output
├── output_png/          — Converted PNG output
└── .gitignore
```

## Build Commands
```sh
# Without dav1d (garbage output)
cc -std=c89 -o avif2ppm test_avif2png.c -lm
./avif2ppm

# With dav1d (correct output)
cc -std=c89 -D STB_AVIF_USE_DAV1D $(pkg-config --cflags dav1d) \
   -o avif2ppm test_avif2png.c -lm $(pkg-config --libs dav1d)
./avif2ppm

# Auto-detect (via test_run.sh)
./test_run.sh
```

## Next Steps for C89 Internal Decoder
1. Port remaining dav1d source files to C89 (in dav1d_ref/)
2. Insert ported code into stb_avif.h behind `STB_AVIF_USE_C89_DAV1D` flag
3. The MSAC must handle ALL bit reading (not mixed with old decoder)
4. Remove old Boolean decoder entirely when C89 decoder is complete
5. Validate against all 8 test AVIF images

## Key Technical Notes
- AV1 bitstream uses XOR-inverted bytes (`byte ^ 0xFF` in storage)
- The old Boolean decoder reads raw bytes — WRONG but accidentally works for 50/50 coded headers
- The MSAC (dav1d's arithmetic coder) correctly XORs bytes back
- CDF tables use Q15 format (0-32768) with adaptation counters
- The `struct stb_av1_msac` uses `stbv_u64 dif` (64-bit window), `unsigned rng` (16-bit range)
- ec_win = sizeof(size_t) × 8 = 64 bits on 64-bit platforms
