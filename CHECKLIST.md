# AVIF Decoder Validation Checklist

A structured checklist for debugging stb_avif, ordered the same way a real
AVIF decoder validates a file: container → item properties → AV1 bitstream →
YUV-to-RGBA conversion.

---

## 1 · Confirm the container

| # | Check | Notes |
|---|-------|-------|
| 1.1 | `ftyp` includes an AVIF-compatible brand (`avif`, `avis`, `MA1B`, …) | |
| 1.2 | `meta` box exists and contains a primary item (`pitm`) | |
| 1.3 | Primary item type is `av01` **or** a derived image (`grid`/`iovl`) whose tiles are `av01` | Primary items are not always directly `av01` |
| 1.4 | `iloc` points to a non-empty extent that is within `mdat` **or** `idat` | Data may be in `idat`, not only `mdat` |
| 1.5 | `iinf`/`infe` item IDs match the `iloc` item IDs for coded items | Derived items may not have direct payload extents |
| 1.6 | `iprp`/`ipco`/`ipma` exist when the file uses item properties | |

> If any of these fail the decoder has not reached AV1 at all; the bug is in
> BMFF/HEIF parsing.

---

## 2 · Check required AVIF item properties

| # | Check | Notes |
|---|-------|-------|
| 2.1 | `av1C` exists for the `av01` item and is parsed correctly | |
| 2.2 | `ispe` width/height match the final decoded image size | |
| 2.3 | `pixi` channel count and bit depth match the actual pixel format | |
| 2.4 | `colr` is handled when present (CICP / ICC profile / range signalling) | |
| 2.5 | Alpha items are parsed separately and linked to the color item (`iref/auxl`) | |

> "Wrong image" bugs are often property mismatches or missing property
> application, not bitstream bugs.

---

## 3 · Validate the AV1 payload

| # | Check | Notes |
|---|-------|-------|
| 3.1 | Dump every OBU type in the payload | |
| 3.2 | A Sequence Header OBU appears before any Frame OBU (a Temporal Delimiter may precede it) | First OBU is **not** always the sequence header |
| 3.3 | OBU header fields are sane: no forbidden bit, valid size field, no truncated payload | |
| 3.4 | Sequence header profile matches the AVIF profile | |
| 3.5 | Sequence header coded dimensions are reasonable | |
| 3.6 | Unknown/unrecognised OBU types are **skipped**, not rejected; only malformed/forbidden ones are rejected | Blindly rejecting unknown OBUs breaks forward compatibility |

> If the payload is malformed here, the decoder may produce corrupted output
> even when the container parser is correct.

---

## 4 · Compare against libavif

Use libavif as a reference decoder and compare:

- parse result
- width / height
- bit depth
- YUV format
- range (full vs. limited)
- alpha presence
- decode success/failure reason

libavif exposes useful error categories: BMFF parse failure, missing image
item, decode color failure, decode alpha failure, color/alpha size mismatch,
and `ispe` size mismatch.

---

## 5 · Check YUV plane reconstruction

| # | Check | Notes |
|---|-------|-------|
| 5.1 | Y plane byte size = `stride × height` (stride ≥ width; depends on bit depth) | Not simply `width × height` for high bit depths or padded strides |
| 5.2 | U/V plane sizes match chroma subsampling (e.g. `⌈width/2⌉ × ⌈height/2⌉` for 4:2:0) | |
| 5.3 | Alpha plane dimensions match the color plane dimensions | |
| 5.4 | Row stride is not accidentally treated as tight packing | |
| 5.5 | 4:2:0 chroma indexing uses the correct subsampled coordinates | |
| 5.6 | Monochrome files do not read invalid chroma pointers | |

> Strip / wrong-size artefacts are usually a plane-size or stride bug, not a
> bitstream bug.

---

## 6 · Check color conversion

| # | Check | Notes |
|---|-------|-------|
| 6.1 | Correct YUV matrix for the file's CICP metadata (BT.601, BT.709, BT.2020, …) | |
| 6.2 | Full-range vs. limited-range is respected | |
| 6.3 | 10-bit or 12-bit samples are converted with bit-depth-aware arithmetic | Conversion must be bit-depth-aware; not necessarily expand-then-convert |
| 6.4 | Alpha and premultiplication handled consistently | |
| 6.5 | HDR / wide-gamut files are not assumed to be sRGB | |

> Recognisable but wrong-colored pictures are almost always a
> range/matrix/chroma conversion bug.

---

## 7 · Add targeted debug output

For each decode, print:

- file name
- `ftyp` brands
- `pitm` item ID
- `iloc` extents
- `av1C` fields
- `ispe` / `pixi`
- OBU types and sizes
- decoded plane dimensions
- RGBA output dimensions

This lets you compare your decoder line-by-line against libavif and isolate
the first point where the two diverge.

---

## 8 · Fast triage by symptom

| Symptom | Most likely bug |
|---------|----------------|
| Total garbage image | AV1 decode or OBU parsing |
| Correct image, wrong colors | Range / matrix / chroma conversion |
| Correct colors, wrong size | `ispe` / `iloc` / stride |
| Some files work, others fail | Unsupported profile, bit depth, alpha, or color metadata |

---

## Validation results — `example_avif/` test corpus

Run with `tests/test_checklist.c` (checks C1–C8 via the public API):

```
=== AVIF Decoder Validation Checklist ===

File: example_avif/G-0trmKXsAA1sQZ-thumb.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=89
  [PASS] C3-height-positive : height=100
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 89x100 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/G-0trmKXsAA1sQZ.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=1429
  [PASS] C3-height-positive : height=1623
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 1429x1623 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/Gb5RU6RWoAAQQ1n.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=1461
  [PASS] C3-height-positive : height=1530
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 1461x1530 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/fox.profile0.10bpc.yuv420.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=1204
  [PASS] C3-height-positive : height=800
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 1204x800 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/fox.profile0.8bpc.yuv420.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=1204
  [PASS] C3-height-positive : height=800
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 1204x800 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/kimono.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=722
  [PASS] C3-height-positive : height=1024
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 722x1024 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/red-at-12-oclock-with-color-profile-10bpc.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=800
  [PASS] C3-height-positive : height=800
  [PASS] C4-channels-valid  : channels=3 (RGB)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 800x800 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : no alpha channel (RGB image)

File: example_avif/steam_2253100.avif
  [PASS] C1-container-parse : stbi_avif_info succeeded
  [PASS] C2-width-positive  : width=1024
  [PASS] C3-height-positive : height=772
  [PASS] C4-channels-valid  : channels=4 (RGBA)
  [PASS] C5-av1-decode      : stbi_avif_load succeeded
  [PASS] C6-dims-match      : 1024x772 matches info
  [PASS] C7-pixel-variance  : pixels contain variation
  [PASS] C8-alpha-consistency : alpha plane contains variation - semi-transparent image

=== Summary: 8/8 files fully decoded ===
```

All 8 files in the test corpus pass every check.
