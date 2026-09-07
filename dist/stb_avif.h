/* stb_avif.h - v0.01 - AVIF image decoder - public domain
 *                                                  - http://github.com/nothings/stb
 *
 * A single-header C89 library for decoding AVIF images.
 *
 * REFERENCES
 *   libavif - https://github.com/AOMediaCodec/libavif
 *   dav1d   - https://code.videolan.org/videolan/dav1d
 *   AV1     - https://aomediacodec.github.io/av1-spec/
 *   ISOBMFF - ISO 14496-12
 *   HEIF    - ISO 23000-22
 *
 * LIBRARY OVERVIEW
 *
 *   stb_avif.h is a single-header library for decoding AVIF images.
 *   To use it, #define STB_AVIF_IMPLEMENTATION in exactly one C file
 *   that includes this header.
 *
 *   Example (without dav1d, internal decoder produces garbage/snow):
 *      #define STB_AVIF_IMPLEMENTATION
 *      #include "stb_avif.h"
 *      ...
 *      int x, y, c;
 *      unsigned char *img = stb_avif_load_from_memory(data, len, &x, &y, &c, 4);
 *      // ... use img ...
 *      stb_avif_free(img);
 *
 *   Example (with dav1d - correct output):
 *      #define STB_AVIF_USE_DAV1D
 *      #define STB_AVIF_IMPLEMENTATION
 *      #include "stb_avif.h"
 *      ...
 *      // Compile: cc ... -D STB_AVIF_USE_DAV1D -ldav1d
 *      int x, y, c;
 *      unsigned char *img = stb_avif_load_from_memory(data, len, &x, &y, &c, 4);
 *      // ... use img ...
 *      stb_avif_free(img);
 *
 *   The library decodes AVIF images down to plain RGBA pixels.
 *   With STB_AVIF_USE_DAV1D, it uses libdav1d for correct AV1 decoding.
 *   Without it, the built-in AV1 decoder will be used.
 *
 *   Supported formats:
 *     - Profile 0 (Main): 8-bit, 4:2:0, 4:2:2, 4:4:4
 *     - Profile 1 (High): 8/10-bit, 4:2:0, 4:2:2, 4:4:4
 *     - Monochrome (limited)
 *     - Still images only (no sequences)
 *
 * LICENSE
 *
 *   This software is in the public domain. Where that dedication is not
 *   recognized, you are granted a perpetual, irrevocable license to use,
 *   copy, modify, and distribute this software for any purpose.
 *
 *   See http://creativecommons.org/publicdomain/zero/1.0/ for details.
 */

#ifndef STB_AVIF_H
#define STB_AVIF_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* PUBLIC API                                                                 */
/* -------------------------------------------------------------------------- */

/* Load an AVIF image from memory.
 *
 *  data    - pointer to the complete AVIF file contents
 *  len     - length of the data buffer
 *  x, y    - output: image dimensions (in pixels)
 *  channels - output: number of color channels in the returned data
 *  req_channels - desired number of output channels (0 = use image default,
 *                 3 = RGB, 4 = RGBA)
 *
 *  Returns a pointer to decoded pixels (row-major, top-left first) or NULL
 *  on failure.
 *
 *  The returned buffer is req_channels bytes per pixel (or channels if 0).
 *  Free it with stb_avif_free().
 *
 *  When STB_AVIF_USE_DAV1D is defined, uses libdav1d for correct output.
 *  Link with -ldav1d. Without dav1d, the internal decoder will be used.
 */
unsigned char *stb_avif_load_from_memory(const unsigned char *data, int len,
                                          int *x, int *y, int *channels,
                                          int req_channels);

/* Free an image buffer previously returned by stb_avif_load_from_memory(). */
void stb_avif_free(void *ptr);

/* Returns a string describing the last error. */
static unsigned char *stb_avif_g_last_alpha;
static int stb_avif_g_last_alpha_stride;
static unsigned char *stb_avif_g_last_yuv_y;
static unsigned char *stb_avif_g_last_yuv_u;
static unsigned char *stb_avif_g_last_yuv_v;
static int stb_avif_g_last_yuv_stride_y;
static int stb_avif_g_last_yuv_stride_u;
static int stb_avif_g_last_yuv_stride_v;

/* Returns the 8-bit alpha plane (w-strided) decoded from the AVIF
 * auxiliary alpha item of the most recent load, or NULL. */
static unsigned char *stb_avif_last_alpha(int *stride)
{
    if (stride) *stride = stb_avif_g_last_alpha_stride;
    return stb_avif_g_last_alpha;
}

/* Returns the 8-bit YUV planes from the most recent load, or NULL.
 * Pointers are owned by the library and freed on the next stb_avif_load()
 * or stb_avif_close(); do NOT call stb_avif_free() on them. */
static void stb_avif_last_yuv(unsigned char **y, unsigned char **u, unsigned char **v,
                               int *stride_y, int *stride_u, int *stride_v)
{
    if (y) *y = stb_avif_g_last_yuv_y;
    if (u) *u = stb_avif_g_last_yuv_u;
    if (v) *v = stb_avif_g_last_yuv_v;
    if (stride_y) *stride_y = stb_avif_g_last_yuv_stride_y;
    if (stride_u) *stride_u = stb_avif_g_last_yuv_stride_u;
    if (stride_v) *stride_v = stb_avif_g_last_yuv_stride_v;
}

const char *stb_avif_failure_reason(void);

/* Load an AVIF from a file path.  Returns an 8-bit RGB/RGBA pixel buffer
 * (freed with stb_avif_free()) or NULL on failure.
 * req_channels: 0 = keep original (3 or 4), 3 = force RGB, 4 = force RGBA.
 * w/h/channels receive image dimensions and actual channel count. */
unsigned char *stb_avif_load_from_file(const char *filePath,
                                       int *w, int *h, int *channels,
                                       int req_channels);

/* -------------------------------------------------------------------------- */
/* PRIVATE TYPES (exposed for implementation)                                 */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* STB_AVIF_H */

/* -------------------------------------------------------------------------- */
/* IMPLEMENTATION                                                             */
/* -------------------------------------------------------------------------- */

#ifdef STB_AVIF_IMPLEMENTATION

#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* memset, memcpy, memmove */
#include <setjmp.h>     /* setjmp, longjmp */
#include <math.h>       /* cos, sin, sqrt */
#include <time.h>       /* clock, time */
#include <stdio.h>      /* fprintf, stderr */

/* Optional dav1d backend for correct AV1 decoding.
   Define STB_AVIF_USE_DAV1D and link with -ldav1d */
#ifdef STB_AVIF_USE_DAV1D
#include <dav1d/dav1d.h>
#endif


/* ===== stb_av1_scalar.h ===== */
/*
 * stb_av1_scalar.h - aggregate include for the scalar AV1 path.
 */
#ifndef STB_AV1_SCALAR_H
#define STB_AV1_SCALAR_H

#include <stddef.h>

#ifndef STBV_I8_DEFINED
typedef char stbv_i8;
#define STBV_I8_DEFINED 1
#endif
#ifndef STBV_U8_DEFINED
typedef unsigned char stbv_u8;
#define STBV_U8_DEFINED 1
#endif
#ifndef STBV_I16_DEFINED
typedef  short stbv_i16;
#define STBV_I16_DEFINED 1
#endif
#ifndef STBV_U16_DEFINED
typedef unsigned short stbv_u16;
#define STBV_U16_DEFINED 1
#endif
#ifndef STBV_U32_DEFINED
typedef unsigned int stbv_u32;
#define STBV_U32_DEFINED 1
#endif
#ifndef STBV_I32_DEFINED
typedef signed int stbv_i32;
#define STBV_I32_DEFINED 1
#endif
#ifndef STBV_U64_DEFINED
# if defined(_MSC_VER)
typedef unsigned __int64 stbv_u64;
# else
typedef unsigned long long stbv_u64;
# endif
#define STBV_U64_DEFINED 1
#endif


/* ===== stb_av1_getbits.h ===== */
/*
 * stb_av1_getbits.h - scalar AV1 uncompressed-bitstream reader
 *
 * Portions are derived from dav1d 1.5.4 src/getbits.c / src/getbits.h.
 * Copyright (C) 2018, VideoLAN and dav1d authors
 * Copyright (C) 2018, Two Orioles, LLC
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_GETBITS_H
#define STB_AV1_GETBITS_H

/* The including file must provide stbv_u8, stbv_u32, stbv_u64 and size_t. */
struct stb_av1_getbits {
    stbv_u64 state;
    int bits_left;
    int error;
    const stbv_u8 *ptr;
    const stbv_u8 *ptr_start;
    const stbv_u8 *ptr_end;
};

static void stb_av1_getbits_init(struct stb_av1_getbits *c,
                                 const stbv_u8 *data, size_t size)
{
    c->ptr = data;
    c->ptr_start = data;
    c->ptr_end = data + size;
    c->state = 0;
    c->bits_left = 0;
    c->error = 0;
}

static unsigned int stb_av1_get_bit(struct stb_av1_getbits *c)
{
    stbv_u64 state;
    if (!c->bits_left) {
        if (c->ptr >= c->ptr_end) {
            c->error = 1;
            return 0;
        }
        state = *c->ptr++;
        c->bits_left = 7;
        c->state = (stbv_u64)state << 57;
        return (unsigned int)(state >> 7);
    }
    state = c->state;
    c->bits_left--;
    c->state = state << 1;
    return (unsigned int)(state >> 63);
}

static void stb_av1_getbits_refill(struct stb_av1_getbits *c, int n)
{
    unsigned int state = 0;
    do {
        if (c->ptr >= c->ptr_end) {
            c->error = 1;
            if (state)
                break;
            return;
        }
        state = (state << 8) | *c->ptr++;
        c->bits_left += 8;
    } while (n > c->bits_left);
    c->state |= (stbv_u64)state << (64 - c->bits_left);
}

static unsigned int stb_av1_get_bits(struct stb_av1_getbits *c, int n)
{
    stbv_u64 state;
    unsigned int v;
    if (n <= 0 || n > 32) {
        c->error = 1;
        return 0;
    }
    if ((unsigned int)n > (unsigned int)c->bits_left)
        stb_av1_getbits_refill(c, n);
    state = c->state;
    c->bits_left -= n;
    c->state = state << n;
    v = (unsigned int)(state >> (64 - n));
    return v;
}

static int stb_av1_get_sbits(struct stb_av1_getbits *c, int n)
{
    unsigned int v;
    if (n <= 0 || n > 31) {
        c->error = 1;
        return 0;
    }
    v = stb_av1_get_bits(c, n);
    if (v & ((unsigned int)1 << (n - 1)))
        return (int)(v - ((unsigned int)1 << n));
    return (int)v;
}

static unsigned int stb_av1_get_uleb128(struct stb_av1_getbits *c)
{
    stbv_u64 val = 0;
    unsigned int i = 0;
    unsigned int more;
    do {
        unsigned int v = stb_av1_get_bits(c, 8);
        more = v & 0x80U;
        val |= (stbv_u64)(v & 0x7fU) << i;
        i += 7;
    } while (more && i < 56);
    if (val > 0xffffffffU || more) {
        c->error = 1;
        return 0;
    }
    return (unsigned int)val;
}

static unsigned int stb_av1_get_uniform(struct stb_av1_getbits *c,
                                         unsigned int max)
{
    unsigned int l = 0;
    unsigned int m;
    unsigned int v;
    if (max <= 1)
        return 0;
    while (((unsigned int)1 << l) < max)
        l++;
    m = ((unsigned int)1 << l) - max;
    v = stb_av1_get_bits(c, (int)l - 1);
    if (v < m)
        return v;
    return (v << 1) - m + stb_av1_get_bit(c);
}

static unsigned int stb_av1_get_vlc(struct stb_av1_getbits *c)
{
    unsigned int n_bits = 0;
    if (stb_av1_get_bit(c))
        return 0;
    for (;;) {
        if (++n_bits == 32)
            return 0xffffffffU;
        if (stb_av1_get_bit(c))
            break;
    }
    return (((unsigned int)1 << n_bits) - 1U) +
           stb_av1_get_bits(c, (int)n_bits);
}

/* AV1's inv_recenter() helper. */
static unsigned int stb_av1_inv_recenter(unsigned int r, unsigned int v)
{
    if (v > 2 * r)
        return v;
    if (v & 1U)
        return r - ((v + 1U) >> 1);
    return r + (v >> 1);
}

static unsigned int stb_av1_get_bits_subexp(struct stb_av1_getbits *c,
                                             unsigned int ref,
                                             unsigned int n)
{
    unsigned int v = 0;
    unsigned int i;
    for (i = 0;; i++) {
        unsigned int b = i ? 3U + i - 1U : 3U;
        unsigned int range = 3U * ((unsigned int)1 << b);
        if (n < v + range) {
            v += stb_av1_get_uniform(c, n - v + 1U);
            break;
        }
        if (!stb_av1_get_bit(c)) {
            v += stb_av1_get_bits(c, (int)b);
            break;
        }
        v += (unsigned int)1 << b;
    }
    if (ref * 2U <= n)
        return stb_av1_inv_recenter(ref, v);
    return n - stb_av1_inv_recenter(n - ref, v);
}

static unsigned int stb_av1_get_bits_pos(const struct stb_av1_getbits *c)
{
    return (unsigned int)((c->ptr - c->ptr_start) * 8 - c->bits_left);
}

static void stb_av1_getbits_bytealign(struct stb_av1_getbits *c)
{
    c->bits_left = 0;
    c->state = 0;
}

/* ---- Byte-level helpers (ISOBMFF / OBU byte-oriented parsing) ---- */

static int stb_av1_getbits_read_byte(struct stb_av1_getbits *c)
{
    return (int)stb_av1_get_bits(c, 8);
}

static int stb_av1_getbits_peek_byte(struct stb_av1_getbits *c)
{
    if (c->bits_left >= 8)
        return (int)(c->state >> (64 - 8));
    if (c->ptr < c->ptr_end)
        return *c->ptr;
    c->error = 1;
    return -1;
}

static stbv_u16 stb_av1_getbits_read_be16(struct stb_av1_getbits *c)
{
    return (stbv_u16)stb_av1_get_bits(c, 16);
}

static stbv_u32 stb_av1_getbits_read_be32(struct stb_av1_getbits *c)
{
    return stb_av1_get_bits(c, 32);
}

static stbv_u64 stb_av1_getbits_read_be64(struct stb_av1_getbits *c)
{
    stbv_u64 hi = stb_av1_get_bits(c, 32);
    stbv_u64 lo = stb_av1_get_bits(c, 32);
    return (hi << 32) | lo;
}

static unsigned int stb_av1_getbits_read_uleb128(struct stb_av1_getbits *c)
{
    return stb_av1_get_uleb128(c);
}

static void stb_av1_getbits_skip(struct stb_av1_getbits *c, size_t n)
{
    while (n > 0 && !c->error) {
        size_t chunk = n > 4 ? 4 : n;
        stb_av1_get_bits(c, (int)(chunk * 8));
        n -= chunk;
    }
}

/* Current byte position in the stream (valid after byte-align or when
 * bits_left == 0). */
static size_t stb_av1_getbits_bytepos(const struct stb_av1_getbits *c)
{
    return (size_t)(c->ptr - c->ptr_start);
}

static void stb_av1_getbits_seek(struct stb_av1_getbits *c, size_t byte_pos)
{
    c->bits_left = 0;
    c->state = 0;
    c->ptr = c->ptr_start + byte_pos;
}

static size_t stb_av1_getbits_size(const struct stb_av1_getbits *c)
{
    return (size_t)(c->ptr_end - c->ptr_start);
}

#endif /* STB_AV1_GETBITS_H */

/* ===== stb_av1_msac.h ===== */
/* stb_av1_msac.h - scalar AV1 MSAC entropy decoder.
 * Verbatim port of dav1d 1.5.4 src/msac.c|h (BSD-2-Clause) onto the
 * stbv_* typedefs; clz replaced by a shift loop for C89/MSVC6. */
#ifndef STB_AV1_MSAC_H
#define STB_AV1_MSAC_H

struct stb_av1_msac {
    const stbv_u8 *buf_pos;
    const stbv_u8 *buf_end;
    stbv_u64 dif;
    stbv_u32 rng;
    int cnt;
    int allow_update_cdf;
};

#define STB_AV1_MSAC_EC_PROB_SHIFT 6
#define STB_AV1_MSAC_EC_MIN_PROB 4
#define STB_AV1_MSAC_EC_WIN_SIZE ((int)(sizeof(stbv_u64) * 8))

static int stb_av1_msac_clz(stbv_u32 m)
{
    int b = 0;
    while (!(m & 0x80000000U)) {
        m <<= 1;
        b++;
        if (b >= 32) break;
    }
    return b;
}

static void stb_av1_msac_refill(struct stb_av1_msac *s)
{
    const stbv_u8 *buf_pos = s->buf_pos;
    const stbv_u8 *buf_end = s->buf_end;
    int c = STB_AV1_MSAC_EC_WIN_SIZE - s->cnt - 24;
    stbv_u64 dif = s->dif;
    do {
        if (buf_pos >= buf_end) {
            dif |= ~(~(stbv_u64)0xff << c);
            break;
        }
        dif |= (stbv_u64)(*buf_pos++ ^ 0xff) << c;
        c -= 8;
    } while (c >= 0);
    s->dif = dif;
    s->cnt = STB_AV1_MSAC_EC_WIN_SIZE - c - 24;
    s->buf_pos = buf_pos;
}

static void stb_av1_msac_norm(struct stb_av1_msac *s,
                              stbv_u64 dif, stbv_u32 rng)
{
    const int d = 15 ^ (31 ^ stb_av1_msac_clz(rng));
    const int cnt = s->cnt;
    s->dif = dif << d;
    s->rng = rng << d;
    s->cnt = cnt - d;
    /* unsigned compare avoids redundant refills at eob */
    if ((stbv_u32)cnt < (stbv_u32)d)
        stb_av1_msac_refill(s);
}

static unsigned int stb_av1_msac_bool_equi(struct stb_av1_msac *s)
{
    const stbv_u32 r = s->rng;
    stbv_u64 dif = s->dif;
    stbv_u32 v = ((r >> 8) << 7) + STB_AV1_MSAC_EC_MIN_PROB;
    stbv_u64 vw = (stbv_u64)v << (STB_AV1_MSAC_EC_WIN_SIZE - 16);
    const stbv_u32 ret = dif >= vw;
    dif -= (stbv_u64)ret * vw;
    v += ret * (r - 2 * v);
    stb_av1_msac_norm(s, dif, v);
    return !ret;
}

static unsigned int stb_av1_msac_bool(struct stb_av1_msac *s,
                                      unsigned int f)
{
    const stbv_u32 r = s->rng;
    stbv_u64 dif = s->dif;
    stbv_u32 v = ((r >> 8) * (f >> STB_AV1_MSAC_EC_PROB_SHIFT)
                  >> (7 - STB_AV1_MSAC_EC_PROB_SHIFT)) +
                 STB_AV1_MSAC_EC_MIN_PROB;
    stbv_u64 vw = (stbv_u64)v << (STB_AV1_MSAC_EC_WIN_SIZE - 16);
    const stbv_u32 ret = dif >= vw;
    dif -= (stbv_u64)ret * vw;
    v += ret * (r - 2 * v);
    stb_av1_msac_norm(s, dif, v);
    return !ret;
}

static unsigned int stb_av1_msac_symbol(struct stb_av1_msac *s,
                                        stbv_u16 *cdf,
                                        size_t n_symbols)
{
    const stbv_u32 c = (stbv_u32)(s->dif >>
                        (STB_AV1_MSAC_EC_WIN_SIZE - 16));
    const stbv_u32 r = s->rng >> 8;
    stbv_u32 u, v = s->rng;
    unsigned int val = (unsigned int)-1;

    /* Match dav1d exactly: val starts at -1, increments first, then
     * computes v and checks c < v.  The previous do-while rewrite
     * broke the u/v tracking when all symbols were exhausted. */
    do {
        val++;
        u = v;
        v = r * (cdf[val] >> STB_AV1_MSAC_EC_PROB_SHIFT);
        v >>= 7 - STB_AV1_MSAC_EC_PROB_SHIFT;
        v += STB_AV1_MSAC_EC_MIN_PROB * ((unsigned int)n_symbols - val);
    } while (c < v);

    stb_av1_msac_norm(s,
        s->dif - ((stbv_u64)v << (STB_AV1_MSAC_EC_WIN_SIZE - 16)),
        u - v);

    if (s->allow_update_cdf) {
        const stbv_u32 count = cdf[n_symbols];
        const stbv_u32 rate = 4 + (count >> 4) + (n_symbols > 2);
        stbv_u32 i;
        for (i = 0; i < val; i++)
            cdf[i] += (stbv_u16)((32768U - cdf[i]) >> rate);
        for (; i < (stbv_u32)n_symbols; i++)
            cdf[i] -= (stbv_u16)(cdf[i] >> rate);
        cdf[n_symbols] = (stbv_u16)(count + (count < 32));
    }

    return val;
}

static unsigned int stb_av1_msac_bool_adapt(struct stb_av1_msac *s,
                                            stbv_u16 *cdf)
{
    const unsigned int bit = stb_av1_msac_bool(s, cdf[0]);
    if (s->allow_update_cdf) {
        const stbv_u32 count = cdf[1];
        const stbv_u32 rate = 4 + (count >> 4);
        if (bit)
            cdf[0] += (stbv_u16)((32768U - cdf[0]) >> rate);
        else
            cdf[0] -= (stbv_u16)(cdf[0] >> rate);
        cdf[1] = (stbv_u16)(count + (count < 32));
    }
    return bit;
}

static unsigned int stb_av1_msac_bools(struct stb_av1_msac *s,
                                       unsigned int n)
{
    unsigned int v = 0;
    while (n--)
        v = (v << 1) | stb_av1_msac_bool_equi(s);
    return v;
}

static unsigned int stb_av1_msac_uniform(struct stb_av1_msac *s,
                                         unsigned int n)
{
    unsigned int l = 0, m, v;
    if (n <= 1)
        return 0;
    while (((unsigned int)1 << l) < n)
        l++;
    m = ((unsigned int)1 << l) - n;
    v = stb_av1_msac_bools(s, l - 1);
    if (v < m)
        return v;
    return (v << 1) - m + stb_av1_msac_bool_equi(s);
}

static unsigned int stbv_av1_inv_recenter(unsigned int r, unsigned int v)
{
    if (v > (r << 1))
        return v;
    else if ((v & 1) == 0)
        return (v >> 1) + r;
    else
        return r - ((v + 1) >> 1);
}

static int stb_av1_msac_subexp(struct stb_av1_msac *s, int ref,
                                int n, unsigned int k)
{
    unsigned int a = 0, v;
    if (stb_av1_msac_bool_equi(s)) {
        if (stb_av1_msac_bool_equi(s))
            k += stb_av1_msac_bool_equi(s) + 1;
        a = 1U << k;
    }
    v = stb_av1_msac_bools(s, k) + a;
    return (unsigned int)ref * 2 <= (unsigned int)n
        ? (int)stbv_av1_inv_recenter((unsigned int)ref, v)
        : n - 1 - (int)stbv_av1_inv_recenter((unsigned int)(n - 1 - ref), v);
}

static void stb_av1_msac_init(struct stb_av1_msac *s,
                              const stbv_u8 *data, size_t size,
                              int disable_cdf_update)
{
    s->buf_pos = data;
    s->buf_end = data + size;
    s->dif = 0;
    s->rng = 0x8000U;
    s->cnt = -15;
    s->allow_update_cdf = !disable_cdf_update;
    stb_av1_msac_refill(s);
}

#endif /* STB_AV1_MSAC_H */

/* ===== stb_av1_cdf.h ===== */
/*
 * AV1 CDF defaults derived from dav1d 1.5.4 src/cdf.c.
 *
 * Copyright © 2018-2021, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#ifndef STB_AV1_CDF_H
#define STB_AV1_CDF_H

#include <string.h>

#ifndef STBV_U16_DEFINED
#error "stb_av1_cdf.h requires stbv_u16 from stb_avif.h"
#endif

static const stbv_u16 stb_av1_cdf_y_mode[64] = {
    22801, 23489, 24293, 24756, 25601, 26123, 26606, 27418, 27945, 29228, 29685, 30349,
    0, 0, 0, 0, 18673, 19845, 22631, 23318, 23950, 24649, 25527, 27364,
    28152, 29701, 29984, 30852, 0, 0, 0, 0, 19770, 20979, 23396, 23939,
    24241, 24654, 25136, 27073, 27830, 29360, 29730, 30659, 0, 0, 0, 0,
    20155, 21301, 22838, 23178, 23261, 23533, 23703, 24804, 25352, 26575, 27016, 28049,
    0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_uv_mode[416] = {
    22631, 24152, 25378, 25661, 25986, 26520, 27055, 27923, 28244, 30059, 30941, 31961,
    0, 0, 0, 0, 9513, 26881, 26973, 27046, 27118, 27664, 27739, 27824,
    28359, 29505, 29800, 31796, 0, 0, 0, 0, 9845, 9915, 28663, 28704,
    28757, 28780, 29198, 29822, 29854, 30764, 31777, 32029, 0, 0, 0, 0,
    13639, 13897, 14171, 25331, 25606, 25727, 25953, 27148, 28577, 30612, 31355, 32493,
    0, 0, 0, 0, 9764, 9835, 9930, 9954, 25386, 27053, 27958, 28148,
    28243, 31101, 31744, 32363, 0, 0, 0, 0, 11825, 13589, 13677, 13720,
    15048, 29213, 29301, 29458, 29711, 31161, 31441, 32550, 0, 0, 0, 0,
    14175, 14399, 16608, 16821, 17718, 17775, 28551, 30200, 30245, 31837, 32342, 32667,
    0, 0, 0, 0, 12885, 13038, 14978, 15590, 15673, 15748, 16176, 29128,
    29267, 30643, 31961, 32461, 0, 0, 0, 0, 12026, 13661, 13874, 15305,
    15490, 15726, 15995, 16273, 28443, 30388, 30767, 32416, 0, 0, 0, 0,
    19052, 19840, 20579, 20916, 21150, 21467, 21885, 22719, 23174, 28861, 30379, 32175,
    0, 0, 0, 0, 18627, 19649, 20974, 21219, 21492, 21816, 22199, 23119,
    23527, 27053, 31397, 32148, 0, 0, 0, 0, 17026, 19004, 19997, 20339,
    20586, 21103, 21349, 21907, 22482, 25896, 26541, 31819, 0, 0, 0, 0,
    12124, 13759, 14959, 14992, 15007, 15051, 15078, 15166, 15255, 15753, 16039, 16606,
    0, 0, 0, 0, 10407, 11208, 12900, 13181, 13823, 14175, 14899, 15656,
    15986, 20086, 20995, 22455, 24212, 0, 0, 0, 4532, 19780, 20057, 20215,
    20428, 21071, 21199, 21451, 22099, 24228, 24693, 27032, 29472, 0, 0, 0,
    5273, 5379, 20177, 20270, 20385, 20439, 20949, 21695, 21774, 23138, 24256, 24703,
    26679, 0, 0, 0, 6740, 7167, 7662, 14152, 14536, 14785, 15034, 16741,
    18371, 21520, 22206, 23389, 24182, 0, 0, 0, 4987, 5368, 5928, 6068,
    19114, 20315, 21857, 22253, 22411, 24911, 25380, 26027, 26376, 0, 0, 0,
    5370, 6889, 7247, 7393, 9498, 21114, 21402, 21753, 21981, 24780, 25386, 26517,
    27176, 0, 0, 0, 4816, 4961, 7204, 7326, 8765, 8930, 20169, 20682,
    20803, 23188, 23763, 24455, 24940, 0, 0, 0, 6608, 6740, 8529, 9049,
    9257, 9356, 9735, 18827, 19059, 22336, 23204, 23964, 24793, 0, 0, 0,
    5998, 7419, 7781, 8933, 9255, 9549, 9753, 10417, 18898, 22494, 23139, 24764,
    25989, 0, 0, 0, 10660, 11298, 12550, 12957, 13322, 13624, 14040, 15004,
    15534, 20714, 21789, 23443, 24861, 0, 0, 0, 10522, 11530, 12552, 12963,
    13378, 13779, 14245, 15235, 15902, 20102, 22696, 23774, 25838, 0, 0, 0,
    10099, 10691, 12639, 13049, 13386, 13665, 14125, 15163, 15636, 19676, 20474, 23519,
    25208, 0, 0, 0, 3144, 5087, 7382, 7504, 7593, 7690, 7801, 8064,
    8232, 9248, 9875, 10521, 29048, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_kfym[400] = {
    15588, 17027, 19338, 20218, 20682, 21110, 21825, 23244, 24189, 28165, 29093, 30466,
    0, 0, 0, 0, 12016, 18066, 19516, 20303, 20719, 21444, 21888, 23032,
    24434, 28658, 30172, 31409, 0, 0, 0, 0, 10052, 10771, 22296, 22788,
    23055, 23239, 24133, 25620, 26160, 29336, 29929, 31567, 0, 0, 0, 0,
    14091, 15406, 16442, 18808, 19136, 19546, 19998, 22096, 24746, 29585, 30958, 32462,
    0, 0, 0, 0, 12122, 13265, 15603, 16501, 18609, 20033, 22391, 25583,
    26437, 30261, 31073, 32475, 0, 0, 0, 0, 10023, 19585, 20848, 21440,
    21832, 22760, 23089, 24023, 25381, 29014, 30482, 31436, 0, 0, 0, 0,
    5983, 24099, 24560, 24886, 25066, 25795, 25913, 26423, 27610, 29905, 31276, 31794,
    0, 0, 0, 0, 7444, 12781, 20177, 20728, 21077, 21607, 22170, 23405,
    24469, 27915, 29090, 30492, 0, 0, 0, 0, 8537, 14689, 15432, 17087,
    17408, 18172, 18408, 19825, 24649, 29153, 31096, 32210, 0, 0, 0, 0,
    7543, 14231, 15496, 16195, 17905, 20717, 21984, 24516, 26001, 29675, 30981, 31994,
    0, 0, 0, 0, 12613, 13591, 21383, 22004, 22312, 22577, 23401, 25055,
    25729, 29538, 30305, 32077, 0, 0, 0, 0, 9687, 13470, 18506, 19230,
    19604, 20147, 20695, 22062, 23219, 27743, 29211, 30907, 0, 0, 0, 0,
    6183, 6505, 26024, 26252, 26366, 26434, 27082, 28354, 28555, 30467, 30794, 32086,
    0, 0, 0, 0, 10718, 11734, 14954, 17224, 17565, 17924, 18561, 21523,
    23878, 28975, 30287, 32252, 0, 0, 0, 0, 9194, 9858, 16501, 17263,
    18424, 19171, 21563, 25961, 26561, 30072, 30737, 32463, 0, 0, 0, 0,
    12602, 14399, 15488, 18381, 18778, 19315, 19724, 21419, 25060, 29696, 30917, 32409,
    0, 0, 0, 0, 8203, 13821, 14524, 17105, 17439, 18131, 18404, 19468,
    25225, 29485, 31158, 32342, 0, 0, 0, 0, 8451, 9731, 15004, 17643,
    18012, 18425, 19070, 21538, 24605, 29118, 30078, 32018, 0, 0, 0, 0,
    7714, 9048, 9516, 16667, 16817, 16994, 17153, 18767, 26743, 30389, 31536, 32528,
    0, 0, 0, 0, 8843, 10280, 11496, 15317, 16652, 17943, 19108, 22718,
    25769, 29953, 30983, 32485, 0, 0, 0, 0, 12578, 13671, 15979, 16834,
    19075, 20913, 22989, 25449, 26219, 30214, 31150, 32477, 0, 0, 0, 0,
    9563, 13626, 15080, 15892, 17756, 20863, 22207, 24236, 25380, 29653, 31143, 32277,
    0, 0, 0, 0, 8356, 8901, 17616, 18256, 19350, 20106, 22598, 25947,
    26466, 29900, 30523, 32261, 0, 0, 0, 0, 10835, 11815, 13124, 16042,
    17018, 18039, 18947, 22753, 24615, 29489, 30883, 32482, 0, 0, 0, 0,
    7618, 8288, 9859, 10509, 15386, 18657, 22903, 28776, 29180, 31355, 31802, 32593,
    0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_partition[320] = {
    27899, 28219, 28529, 32484, 32539, 32619, 32639, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 6607, 6990, 8268, 32060, 32219, 32338, 32371, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 5429, 6676, 7122, 32027,
    32227, 32531, 32582, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    711, 966, 1172, 32448, 32538, 32617, 32664, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 20137, 21547, 23078, 29566, 29837, 30261, 30524, 30892,
    31724, 0, 0, 0, 0, 0, 0, 0, 6732, 7490, 9497, 27944,
    28250, 28515, 28969, 29630, 30104, 0, 0, 0, 0, 0, 0, 0,
    5945, 7663, 8348, 28683, 29117, 29749, 30064, 30298, 32238, 0, 0, 0,
    0, 0, 0, 0, 870, 1212, 1487, 31198, 31394, 31574, 31743, 31881,
    32332, 0, 0, 0, 0, 0, 0, 0, 18462, 20920, 23124, 27647,
    28227, 29049, 29519, 30178, 31544, 0, 0, 0, 0, 0, 0, 0,
    7689, 9060, 12056, 24992, 25660, 26182, 26951, 28041, 29052, 0, 0, 0,
    0, 0, 0, 0, 6015, 9009, 10062, 24544, 25409, 26545, 27071, 27526,
    32047, 0, 0, 0, 0, 0, 0, 0, 1394, 2208, 2796, 28614,
    29061, 29466, 29840, 30185, 31899, 0, 0, 0, 0, 0, 0, 0,
    15597, 20929, 24571, 26706, 27664, 28821, 29601, 30571, 31902, 0, 0, 0,
    0, 0, 0, 0, 7925, 11043, 16785, 22470, 23971, 25043, 26651, 28701,
    29834, 0, 0, 0, 0, 0, 0, 0, 5414, 13269, 15111, 20488,
    22360, 24500, 25537, 26336, 32117, 0, 0, 0, 0, 0, 0, 0,
    2662, 6362, 8614, 20860, 23053, 24778, 26436, 27829, 31171, 0, 0, 0,
    0, 0, 0, 0, 19132, 25510, 30392, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 13928, 19855, 28540, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    12522, 23679, 28629, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 9896, 18783, 25853, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_angle_delta[64] = {
    2180, 5032, 7567, 22776, 26989, 30217, 0, 0, 2301, 5608, 8801, 23487,
    26974, 30330, 0, 0, 3780, 11018, 13699, 19354, 23083, 31286, 0, 0,
    4581, 11226, 15147, 17138, 21834, 28397, 0, 0, 1737, 10927, 14509, 19588,
    22745, 28823, 0, 0, 2664, 10176, 12485, 17650, 21600, 30495, 0, 0,
    2240, 11096, 15453, 20341, 22561, 28917, 0, 0, 3605, 10428, 12459, 17676,
    21244, 30655, 0, 0,
};

static const stbv_u16 stb_av1_cdf_filter_intra[8] = {
    8949, 12776, 17211, 29558, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_use_filter_intra[44] = {
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 22343, 0, 12756, 0, 18101, 0, 16384, 0, 14301, 0,
    12408, 0, 9394, 0, 10368, 0, 20229, 0, 12551, 0, 7866, 0,
    5893, 0, 12770, 0, 6743, 0, 4621, 0,
};

static const stbv_u16 stb_av1_cdf_txsz[48] = {
    19968, 0, 0, 0, 19968, 0, 0, 0, 24320, 0, 0, 0,
    12272, 30172, 0, 0, 12272, 30172, 0, 0, 18677, 30848, 0, 0,
    12986, 15180, 0, 0, 12986, 15180, 0, 0, 24302, 25602, 0, 0,
    5782, 11475, 0, 0, 5782, 11475, 0, 0, 16803, 22759, 0, 0,
};

static const stbv_u16 stb_av1_cdf_txpart[42] = {
    28581, 0, 23846, 0, 20847, 0, 24315, 0, 18196, 0, 12133, 0,
    18791, 0, 10887, 0, 11005, 0, 27179, 0, 20004, 0, 11281, 0,
    26549, 0, 19308, 0, 14224, 0, 28015, 0, 21546, 0, 14400, 0,
    28165, 0, 22401, 0, 16088, 0,
};

static const stbv_u16 stb_av1_cdf_skip[6] = {
    31671, 0, 16515, 0, 4576, 0,
};

/* dav1d default seg_id CDF: 3 contexts x 8 entries each (7 symbols + count) */
static const stbv_u16 stb_av1_cdf_seg_id[24] = {
    5622, 7893, 16093, 18233, 27809, 28373, 32533, 0,
    14274, 18230, 22557, 24935, 29980, 30851, 32344, 0,
    27527, 28487, 28723, 28890, 32397, 32647, 32679, 0,
};
/* dav1d default seg_pred CDF: 4 contexts x 2 entries each (1 symbol + count) */
static const stbv_u16 stb_av1_cdf_seg_pred[8] = {
    16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_cfl_sign[8] = {
    1418, 2123, 13340, 18405, 26972, 28343, 32294, 0,
};

static const stbv_u16 stb_av1_cdf_cfl_alpha[96] = {
    7637, 20719, 31401, 32481, 32657, 32688, 32692, 32696, 32700, 32704, 32708, 32712,
    32716, 32720, 32724, 0, 14365, 23603, 28135, 31168, 32167, 32395, 32487, 32573,
    32620, 32647, 32668, 32672, 32676, 32680, 32684, 0, 11532, 22380, 28445, 31360,
    32349, 32523, 32584, 32649, 32673, 32677, 32681, 32685, 32689, 32693, 32697, 0,
    26990, 31402, 32282, 32571, 32692, 32696, 32700, 32704, 32708, 32712, 32716, 32720,
    32724, 32728, 32732, 0, 17248, 26058, 28904, 30608, 31305, 31877, 32126, 32321,
    32394, 32464, 32516, 32560, 32576, 32593, 32622, 0, 14738, 21678, 25779, 27901,
    29024, 30302, 30980, 31843, 32144, 32413, 32520, 32594, 32622, 32656, 32660, 0,
};

static const stbv_u16 stb_av1_cdf_txtp_intra1[208] = {
    1535, 8035, 9461, 12751, 23467, 27825, 0, 0, 564, 3335, 9709, 10870,
    18143, 28094, 0, 0, 672, 3247, 3676, 11982, 19415, 23127, 0, 0,
    5279, 13885, 15487, 18044, 23527, 30252, 0, 0, 4423, 6074, 7985, 10416,
    25693, 29298, 0, 0, 1486, 4241, 9460, 10662, 16456, 27694, 0, 0,
    439, 2838, 3522, 6737, 18058, 23754, 0, 0, 1190, 4233, 4855, 11670,
    20281, 24377, 0, 0, 1045, 4312, 8647, 10159, 18644, 29335, 0, 0,
    202, 3734, 4747, 7298, 17127, 24016, 0, 0, 447, 4312, 6819, 8884,
    16010, 23858, 0, 0, 277, 4369, 5255, 8905, 16465, 22271, 0, 0,
    3409, 5436, 10599, 15599, 19687, 24040, 0, 0, 1870, 13742, 14530, 16498,
    23770, 27698, 0, 0, 326, 8796, 14632, 15079, 19272, 27486, 0, 0,
    484, 7576, 7712, 14443, 19159, 22591, 0, 0, 1126, 15340, 15895, 17023,
    20896, 30279, 0, 0, 655, 4854, 5249, 5913, 22099, 27138, 0, 0,
    1299, 6458, 8885, 9290, 14851, 25497, 0, 0, 311, 5295, 5552, 6885,
    16107, 22672, 0, 0, 883, 8059, 8270, 11258, 17289, 21549, 0, 0,
    741, 7580, 9318, 10345, 16688, 29046, 0, 0, 110, 7406, 7915, 9195,
    16041, 23329, 0, 0, 363, 7974, 9357, 10673, 15629, 24474, 0, 0,
    153, 7647, 8112, 9936, 15307, 19996, 0, 0, 3511, 6332, 11165, 15335,
    19323, 23594, 0, 0,
};

static const stbv_u16 stb_av1_cdf_txtp_intra2[312] = {
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 6554, 13107, 19661, 26214, 0, 0, 0, 0,
    6554, 13107, 19661, 26214, 0, 0, 0, 0, 6554, 13107, 19661, 26214,
    0, 0, 0, 0, 1127, 12814, 22772, 27483, 0, 0, 0, 0,
    145, 6761, 11980, 26667, 0, 0, 0, 0, 362, 5887, 11678, 16725,
    0, 0, 0, 0, 385, 15213, 18587, 30693, 0, 0, 0, 0,
    25, 2914, 23134, 27903, 0, 0, 0, 0, 60, 4470, 11749, 23991,
    0, 0, 0, 0, 37, 3332, 14511, 21448, 0, 0, 0, 0,
    157, 6320, 13036, 17439, 0, 0, 0, 0, 119, 6719, 12906, 29396,
    0, 0, 0, 0, 47, 5537, 12576, 21499, 0, 0, 0, 0,
    269, 6076, 11258, 23115, 0, 0, 0, 0, 83, 5615, 12001, 17228,
    0, 0, 0, 0, 1968, 5556, 12023, 18547, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_skip[130] = {
    31849, 0, 5892, 0, 12112, 0, 21935, 0, 20289, 0, 27473, 0,
    32487, 0, 7654, 0, 19473, 0, 29984, 0, 9961, 0, 30242, 0,
    32117, 0, 31548, 0, 1549, 0, 10130, 0, 16656, 0, 18591, 0,
    26308, 0, 32537, 0, 5403, 0, 18096, 0, 30003, 0, 16384, 0,
    16384, 0, 16384, 0, 29957, 0, 5391, 0, 18039, 0, 23566, 0,
    22431, 0, 25822, 0, 32197, 0, 3778, 0, 15336, 0, 28981, 0,
    16384, 0, 16384, 0, 16384, 0, 17920, 0, 1818, 0, 7282, 0,
    25273, 0, 10923, 0, 31554, 0, 32624, 0, 1366, 0, 15628, 0,
    30462, 0, 146, 0, 5132, 0, 31657, 0, 6308, 0, 117, 0,
    1638, 0, 2161, 0, 16384, 0, 10923, 0, 30247, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_16[32] = {
    840, 1039, 1980, 4895, 0, 0, 0, 0, 370, 671, 1883, 4471,
    0, 0, 0, 0, 3247, 4950, 9688, 14563, 0, 0, 0, 0,
    1904, 3354, 7763, 14647, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_32[32] = {
    400, 520, 977, 2102, 6542, 0, 0, 0, 210, 405, 1315, 3326,
    7537, 0, 0, 0, 2636, 4273, 7588, 11794, 20401, 0, 0, 0,
    1786, 3179, 6902, 11357, 19054, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_64[32] = {
    329, 498, 1101, 1784, 3265, 7758, 0, 0, 335, 730, 1459, 5494,
    8755, 12997, 0, 0, 3505, 5304, 10086, 13814, 17684, 23370, 0, 0,
    1563, 2700, 4876, 10911, 14706, 22480, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_128[32] = {
    219, 482, 1140, 2091, 3680, 6028, 12586, 0, 371, 699, 1254, 4830,
    9479, 12562, 17497, 0, 5245, 7456, 12880, 15852, 20033, 23932, 27608, 0,
    2054, 3472, 5869, 14232, 18242, 20590, 26752, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_256[64] = {
    310, 584, 1887, 3589, 6168, 8611, 11352, 15652, 0, 0, 0, 0,
    0, 0, 0, 0, 998, 1850, 2998, 5604, 17341, 19888, 22899, 25583,
    0, 0, 0, 0, 0, 0, 0, 0, 2520, 3240, 5952, 8870,
    12577, 17558, 19954, 24168, 0, 0, 0, 0, 0, 0, 0, 0,
    2203, 4130, 7435, 10739, 20652, 23681, 25609, 27261, 0, 0, 0, 0,
    0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_512[32] = {
    641, 983, 3707, 5430, 10234, 14958, 18788, 23412, 26061, 0, 0, 0,
    0, 0, 0, 0, 5095, 6446, 9996, 13354, 16017, 17986, 20919, 26129,
    29140, 0, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_bin_1024[32] = {
    393, 421, 751, 1623, 3160, 6352, 13345, 18047, 22571, 25830, 0, 0,
    0, 0, 0, 0, 1865, 1988, 2930, 4242, 10533, 16538, 21354, 27255,
    28546, 31784, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_base_tok[160] = {
    17837, 29055, 0, 0, 29600, 31446, 0, 0, 30844, 31878, 0, 0,
    24926, 28948, 0, 0, 21365, 30026, 0, 0, 30512, 32423, 0, 0,
    31658, 32621, 0, 0, 29630, 31881, 0, 0, 5717, 26477, 0, 0,
    30491, 31703, 0, 0, 31550, 32158, 0, 0, 29648, 31491, 0, 0,
    12608, 27820, 0, 0, 30680, 32225, 0, 0, 30809, 32335, 0, 0,
    31299, 32423, 0, 0, 1786, 12612, 0, 0, 30663, 31625, 0, 0,
    32339, 32468, 0, 0, 31148, 31833, 0, 0, 18857, 23865, 0, 0,
    31428, 32428, 0, 0, 31744, 32373, 0, 0, 31775, 32526, 0, 0,
    1787, 2532, 0, 0, 30832, 31662, 0, 0, 31824, 32682, 0, 0,
    32133, 32569, 0, 0, 13751, 22235, 0, 0, 32089, 32409, 0, 0,
    27084, 27920, 0, 0, 29291, 32594, 0, 0, 1725, 3449, 0, 0,
    31102, 31935, 0, 0, 32457, 32613, 0, 0, 32412, 32649, 0, 0,
    10923, 21845, 0, 0, 10923, 21845, 0, 0, 10923, 21845, 0, 0,
    10923, 21845, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_base_tok[1640] = {
    4034, 8930, 12727, 0, 18082, 29741, 31877, 0, 12596, 26124, 30493, 0,
    9446, 21118, 27005, 0, 6308, 15141, 21279, 0, 2463, 6357, 9783, 0,
    20667, 30546, 31929, 0, 13043, 26123, 30134, 0, 8151, 18757, 24778, 0,
    5255, 12839, 18632, 0, 2820, 7206, 11161, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    15736, 27553, 30604, 0, 11210, 23794, 28787, 0, 5947, 13874, 19701, 0,
    4215, 9323, 13891, 0, 2833, 6462, 10059, 0, 19605, 30393, 31582, 0,
    13523, 26252, 30248, 0, 8446, 18622, 24512, 0, 3818, 10343, 15974, 0,
    1481, 4117, 6796, 0, 22649, 31302, 32190, 0, 14829, 27127, 30449, 0,
    8313, 17702, 23304, 0, 3022, 8301, 12786, 0, 1536, 4412, 7184, 0,
    22354, 29774, 31372, 0, 14723, 25472, 29214, 0, 6673, 13745, 18662, 0,
    2068, 5766, 9322, 0, 8192, 16384, 24576, 0, 6302, 16444, 21761, 0,
    23040, 31538, 32475, 0, 15196, 28452, 31496, 0, 10020, 22946, 28514, 0,
    6533, 16862, 23501, 0, 3538, 9816, 15076, 0, 24444, 31875, 32525, 0,
    15881, 28924, 31635, 0, 9922, 22873, 28466, 0, 6527, 16966, 23691, 0,
    4114, 11303, 17220, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 20201, 30770, 32209, 0,
    14754, 28071, 31258, 0, 8378, 20186, 26517, 0, 5916, 15299, 21978, 0,
    4268, 11583, 17901, 0, 24361, 32025, 32581, 0, 18673, 30105, 31943, 0,
    10196, 22244, 27576, 0, 5495, 14349, 20417, 0, 2676, 7415, 11498, 0,
    24678, 31958, 32585, 0, 18629, 29906, 31831, 0, 9364, 20724, 26315, 0,
    4641, 12318, 18094, 0, 2758, 7387, 11579, 0, 25433, 31842, 32469, 0,
    18795, 29289, 31411, 0, 7644, 17584, 23592, 0, 3408, 9014, 15047, 0,
    8192, 16384, 24576, 0, 4536, 10072, 14001, 0, 25459, 31416, 32206, 0,
    16605, 28048, 30818, 0, 11008, 22857, 27719, 0, 6915, 16268, 22315, 0,
    2625, 6812, 10537, 0, 24257, 31788, 32499, 0, 16880, 29454, 31879, 0,
    11958, 25054, 29778, 0, 7916, 18718, 25084, 0, 3383, 8777, 13446, 0,
    22720, 31603, 32393, 0, 14960, 28125, 31335, 0, 9731, 22210, 27928, 0,
    6304, 15832, 22277, 0, 2910, 7818, 12166, 0, 20375, 30627, 32131, 0,
    13904, 27284, 30887, 0, 9368, 21558, 27144, 0, 5937, 14966, 21119, 0,
    2667, 7225, 11319, 0, 23970, 31470, 32378, 0, 17173, 29734, 32018, 0,
    12795, 25441, 29965, 0, 8981, 19680, 25893, 0, 4728, 11372, 16902, 0,
    24287, 31797, 32439, 0, 16703, 29145, 31696, 0, 10833, 23554, 28725, 0,
    6468, 16566, 23057, 0, 2415, 6562, 10278, 0, 26610, 32395, 32659, 0,
    18590, 30498, 32117, 0, 12420, 25756, 29950, 0, 7639, 18746, 24710, 0,
    3001, 8086, 12347, 0, 25076, 32064, 32580, 0, 17946, 30128, 32028, 0,
    12024, 24985, 29378, 0, 7517, 18390, 24304, 0, 3243, 8781, 13331, 0,
    6037, 16771, 21957, 0, 24774, 31704, 32426, 0, 16830, 28589, 31056, 0,
    10602, 22828, 27760, 0, 6733, 16829, 23071, 0, 3250, 8914, 13556, 0,
    25582, 32220, 32668, 0, 18659, 30342, 32223, 0, 12546, 26149, 30515, 0,
    8420, 20451, 26801, 0, 4636, 12420, 18344, 0, 27581, 32362, 32639, 0,
    18987, 30083, 31978, 0, 11327, 24248, 29084, 0, 7264, 17719, 24120, 0,
    3995, 10768, 16169, 0, 25893, 31831, 32487, 0, 16577, 28587, 31379, 0,
    10189, 22748, 28182, 0, 6832, 17094, 23556, 0, 3708, 10110, 15334, 0,
    25904, 32282, 32656, 0, 19721, 30792, 32276, 0, 12819, 26243, 30411, 0,
    8572, 20614, 26891, 0, 5364, 14059, 20467, 0, 26580, 32438, 32677, 0,
    20852, 31225, 32340, 0, 12435, 25700, 29967, 0, 8691, 20825, 26976, 0,
    4446, 12209, 17269, 0, 27350, 32429, 32696, 0, 21372, 30977, 32272, 0,
    12673, 25270, 29853, 0, 9208, 20925, 26640, 0, 5018, 13351, 18732, 0,
    27351, 32479, 32713, 0, 21398, 31209, 32387, 0, 12162, 25047, 29842, 0,
    7896, 18691, 25319, 0, 4670, 12882, 18881, 0, 5487, 10460, 13708, 0,
    21597, 28303, 30674, 0, 11037, 21953, 26476, 0, 8147, 17962, 22952, 0,
    5242, 13061, 18532, 0, 1889, 5208, 8182, 0, 26774, 32133, 32590, 0,
    17844, 29564, 31767, 0, 11690, 24438, 29171, 0, 7542, 18215, 24459, 0,
    2993, 8050, 12319, 0, 28023, 32328, 32591, 0, 18651, 30126, 31954, 0,
    12164, 25146, 29589, 0, 7762, 18530, 24771, 0, 3492, 9183, 13920, 0,
    27591, 32008, 32491, 0, 17149, 28853, 31510, 0, 11485, 24003, 28860, 0,
    7697, 18086, 24210, 0, 3075, 7999, 12218, 0, 28268, 32482, 32654, 0,
    19631, 31051, 32404, 0, 13860, 27260, 31020, 0, 9605, 21613, 27594, 0,
    4876, 12162, 17908, 0, 27248, 32316, 32576, 0, 18955, 30457, 32075, 0,
    11824, 23997, 28795, 0, 7346, 18196, 24647, 0, 3403, 9247, 14111, 0,
    29711, 32655, 32735, 0, 21169, 31394, 32417, 0, 13487, 27198, 30957, 0,
    8828, 21683, 27614, 0, 4270, 11451, 17038, 0, 28708, 32578, 32731, 0,
    20120, 31241, 32482, 0, 13692, 27550, 31321, 0, 9418, 22514, 28439, 0,
    4999, 13283, 19462, 0, 5673, 14302, 19711, 0, 26251, 30701, 31834, 0,
    12782, 23783, 27803, 0, 9127, 20657, 25808, 0, 6368, 16208, 21462, 0,
    2465, 7177, 10822, 0, 29961, 32563, 32719, 0, 18318, 29891, 31949, 0,
    11361, 24514, 29357, 0, 7900, 19603, 25607, 0, 4002, 10590, 15546, 0,
    29637, 32310, 32595, 0, 18296, 29913, 31809, 0, 10144, 21515, 26871, 0,
    5358, 14322, 20394, 0, 3067, 8362, 13346, 0, 28652, 32470, 32676, 0,
    17538, 30771, 32209, 0, 13924, 26882, 30494, 0, 10496, 22837, 27869, 0,
    7236, 16396, 21621, 0, 30743, 32687, 32746, 0, 23006, 31676, 32489, 0,
    14494, 27828, 31120, 0, 10174, 22801, 28352, 0, 6242, 15281, 21043, 0,
    25817, 32243, 32720, 0, 18618, 31367, 32325, 0, 13997, 28318, 31878, 0,
    12255, 26534, 31383, 0, 9561, 21588, 28450, 0, 28188, 32635, 32724, 0,
    22060, 32365, 32728, 0, 18102, 30690, 32528, 0, 14196, 28864, 31999, 0,
    12262, 25792, 30865, 0, 24176, 32109, 32628, 0, 18280, 29681, 31963, 0,
    10205, 23703, 29664, 0, 7889, 20025, 27676, 0, 6060, 16743, 23970, 0,
    5141, 7096, 8260, 0, 27186, 29022, 29789, 0, 6668, 12568, 15682, 0,
    2172, 6181, 8638, 0, 1126, 3379, 4531, 0, 443, 1361, 2254, 0,
    26083, 31153, 32436, 0, 13486, 24603, 28483, 0, 6508, 14840, 19910, 0,
    3386, 8800, 13286, 0, 1530, 4322, 7054, 0, 29639, 32080, 32548, 0,
    15897, 27552, 30290, 0, 8588, 20047, 25383, 0, 4889, 13339, 19269, 0,
    2240, 6871, 10498, 0, 28165, 32197, 32517, 0, 20735, 30427, 31568, 0,
    14325, 24671, 27692, 0, 5119, 12554, 17805, 0, 1810, 5441, 8261, 0,
    31212, 32724, 32748, 0, 23352, 31766, 32545, 0, 14669, 27570, 31059, 0,
    8492, 20894, 27272, 0, 3644, 10194, 15204, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 2461, 7013, 9371, 0,
    24749, 29600, 30986, 0, 9466, 19037, 22417, 0, 3584, 9280, 14400, 0,
    1505, 3929, 5433, 0, 677, 1500, 2736, 0, 23987, 30702, 32117, 0,
    13554, 24571, 29263, 0, 6211, 14556, 21155, 0, 3135, 10972, 15625, 0,
    2435, 7127, 11427, 0, 31300, 32532, 32550, 0, 14757, 30365, 31954, 0,
    4405, 11612, 18553, 0, 580, 4132, 7322, 0, 1695, 10169, 14124, 0,
    30008, 32282, 32591, 0, 19244, 30108, 31748, 0, 11180, 24158, 29555, 0,
    5650, 14972, 19209, 0, 2114, 5109, 8456, 0, 31856, 32716, 32748, 0,
    23012, 31664, 32572, 0, 13694, 26656, 30636, 0, 8142, 19508, 26093, 0,
    4253, 10955, 16724, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 601, 983, 1311, 0, 18725, 23406, 28087, 0,
    5461, 8192, 10923, 0, 3781, 15124, 21425, 0, 2587, 7761, 12072, 0,
    106, 458, 810, 0, 22282, 29710, 31894, 0, 8508, 20926, 25984, 0,
    3726, 12713, 18083, 0, 1620, 7112, 10893, 0, 729, 2236, 3495, 0,
    30163, 32474, 32684, 0, 18304, 30464, 32000, 0, 11443, 26526, 29647, 0,
    6007, 15292, 21299, 0, 2234, 6703, 8937, 0, 30954, 32177, 32571, 0,
    17363, 29562, 31076, 0, 9686, 22464, 27410, 0, 8192, 16384, 21390, 0,
    1755, 8046, 11264, 0, 31168, 32734, 32748, 0, 22486, 31441, 32471, 0,
    12833, 25627, 29738, 0, 6980, 17379, 23122, 0, 3111, 8887, 13479, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_br_tok[672] = {
    14298, 20718, 24174, 0, 12536, 19601, 23789, 0, 8712, 15051, 19503, 0,
    6170, 11327, 15434, 0, 4742, 8926, 12538, 0, 3803, 7317, 10546, 0,
    1696, 3317, 4871, 0, 14392, 19951, 22756, 0, 15978, 23218, 26818, 0,
    12187, 19474, 23889, 0, 9176, 15640, 20259, 0, 7068, 12655, 17028, 0,
    5656, 10442, 14472, 0, 2580, 4992, 7244, 0, 12136, 18049, 21426, 0,
    13784, 20721, 24481, 0, 10836, 17621, 21900, 0, 8372, 14444, 18847, 0,
    6523, 11779, 16000, 0, 5337, 9898, 13760, 0, 3034, 5860, 8462, 0,
    15967, 22905, 26286, 0, 13534, 20654, 24579, 0, 9504, 16092, 20535, 0,
    6975, 12568, 16903, 0, 5364, 10091, 14020, 0, 4357, 8370, 11857, 0,
    2506, 4934, 7218, 0, 23032, 28815, 30936, 0, 19540, 26704, 29719, 0,
    15158, 22969, 27097, 0, 11408, 18865, 23650, 0, 8885, 15448, 20250, 0,
    7108, 12853, 17416, 0, 4231, 8041, 11480, 0, 19823, 26490, 29156, 0,
    18890, 25929, 28932, 0, 15660, 23491, 27433, 0, 12147, 19776, 24488, 0,
    9728, 16774, 21649, 0, 7919, 14277, 19066, 0, 5440, 10170, 14185, 0,
    14406, 20862, 24414, 0, 11824, 18907, 23109, 0, 8257, 14393, 18803, 0,
    5860, 10747, 14778, 0, 4475, 8486, 11984, 0, 3606, 6954, 10043, 0,
    1736, 3410, 5048, 0, 14430, 20046, 22882, 0, 15593, 22899, 26709, 0,
    12102, 19368, 23811, 0, 9059, 15584, 20262, 0, 6999, 12603, 17048, 0,
    5684, 10497, 14553, 0, 2822, 5438, 7862, 0, 15785, 21585, 24359, 0,
    18347, 25229, 28266, 0, 14974, 22487, 26389, 0, 11423, 18681, 23271, 0,
    8863, 15350, 20008, 0, 7153, 12852, 17278, 0, 3707, 7036, 9982, 0,
    15460, 21696, 25469, 0, 12170, 19249, 23191, 0, 8723, 15027, 19332, 0,
    6428, 11704, 15874, 0, 4922, 9292, 13052, 0, 4139, 7695, 11010, 0,
    2291, 4508, 6598, 0, 19856, 26920, 29828, 0, 17923, 25289, 28792, 0,
    14278, 21968, 26297, 0, 10910, 18136, 22950, 0, 8423, 14815, 19627, 0,
    6771, 12283, 16774, 0, 4074, 7750, 11081, 0, 19852, 26074, 28672, 0,
    19371, 26110, 28989, 0, 16265, 23873, 27663, 0, 12758, 20378, 24952, 0,
    10095, 17098, 21961, 0, 8250, 14628, 19451, 0, 5205, 9745, 13622, 0,
    10563, 16233, 19763, 0, 9794, 16022, 19804, 0, 6750, 11945, 15759, 0,
    4963, 9186, 12752, 0, 3845, 7435, 10627, 0, 3051, 6085, 8834, 0,
    1311, 2596, 3830, 0, 11246, 16404, 19689, 0, 12315, 18911, 22731, 0,
    10557, 17095, 21289, 0, 8136, 14006, 18249, 0, 6348, 11474, 15565, 0,
    5196, 9655, 13400, 0, 2349, 4526, 6587, 0, 13337, 18730, 21569, 0,
    19306, 26071, 28882, 0, 15952, 23540, 27254, 0, 12409, 19934, 24430, 0,
    9760, 16706, 21389, 0, 8004, 14220, 18818, 0, 4138, 7794, 10961, 0,
    10870, 16684, 20949, 0, 9664, 15230, 18680, 0, 6886, 12109, 15408, 0,
    4825, 8900, 12305, 0, 3630, 7162, 10314, 0, 3036, 6429, 9387, 0,
    1671, 3296, 4940, 0, 13819, 19159, 23026, 0, 11984, 19108, 23120, 0,
    10690, 17210, 21663, 0, 7984, 14154, 18333, 0, 6868, 12294, 16124, 0,
    5274, 8994, 12868, 0, 2988, 5771, 8424, 0, 19736, 26647, 29141, 0,
    18933, 26070, 28984, 0, 15779, 23048, 27200, 0, 12638, 20061, 24532, 0,
    10692, 17545, 22220, 0, 9217, 15251, 20054, 0, 5078, 9284, 12594, 0,
    2331, 3662, 5244, 0, 2891, 4771, 6145, 0, 4598, 7623, 9729, 0,
    3520, 6845, 9199, 0, 3417, 6119, 9324, 0, 2601, 5412, 7385, 0,
    600, 1173, 1744, 0, 7672, 13286, 17469, 0, 4232, 7792, 10793, 0,
    2915, 5317, 7397, 0, 2318, 4356, 6152, 0, 2127, 4000, 5554, 0,
    1850, 3478, 5275, 0, 977, 1933, 2843, 0, 18280, 24387, 27989, 0,
    15852, 22671, 26185, 0, 13845, 20951, 24789, 0, 11055, 17966, 22129, 0,
    9138, 15422, 19801, 0, 7454, 13145, 17456, 0, 3370, 6393, 9013, 0,
    5842, 9229, 10838, 0, 2313, 3491, 4276, 0, 2998, 6104, 7496, 0,
    2420, 7447, 9868, 0, 3034, 8495, 10923, 0, 4076, 8937, 10975, 0,
    1086, 2370, 3299, 0, 9714, 17254, 20444, 0, 8543, 13698, 17123, 0,
    4918, 9007, 11910, 0, 4129, 7532, 10553, 0, 2364, 5533, 8058, 0,
    1834, 3546, 5563, 0, 1473, 2908, 4133, 0, 15405, 21193, 25619, 0,
    15691, 21952, 26561, 0, 12962, 19194, 24165, 0, 10272, 17855, 22129, 0,
    8588, 15270, 20718, 0, 8682, 14669, 19500, 0, 4870, 9636, 13205, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_eob_hi_bit[180] = {
    16961, 0, 17223, 0, 7621, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 19069, 0, 22525, 0, 13377, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    20401, 0, 17025, 0, 12845, 0, 12873, 0, 14094, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 20681, 0, 20701, 0, 15250, 0,
    15017, 0, 14928, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    23905, 0, 17194, 0, 16170, 0, 17695, 0, 13826, 0, 15810, 0,
    12036, 0, 16384, 0, 16384, 0, 23959, 0, 20799, 0, 19021, 0,
    16203, 0, 17886, 0, 14144, 0, 12010, 0, 16384, 0, 16384, 0,
    27399, 0, 16327, 0, 18071, 0, 19584, 0, 20721, 0, 18432, 0,
    19560, 0, 10150, 0, 8805, 0, 24932, 0, 20833, 0, 12027, 0,
    16670, 0, 19914, 0, 15106, 0, 17662, 0, 13783, 0, 28756, 0,
    23406, 0, 21845, 0, 18432, 0, 16384, 0, 17096, 0, 12561, 0,
    17320, 0, 22395, 0, 21370, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef0_dc_sign[12] = {
    16000, 0, 13056, 0, 18816, 0, 15232, 0, 12928, 0, 17280, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_skip[130] = {
    30371, 0, 7570, 0, 13155, 0, 20751, 0, 20969, 0, 27067, 0,
    32013, 0, 5495, 0, 17942, 0, 28280, 0, 16384, 0, 16384, 0,
    16384, 0, 31782, 0, 1836, 0, 10689, 0, 17604, 0, 21622, 0,
    27518, 0, 32399, 0, 4419, 0, 16294, 0, 28345, 0, 16384, 0,
    16384, 0, 16384, 0, 31901, 0, 10311, 0, 18047, 0, 24806, 0,
    23288, 0, 27914, 0, 32296, 0, 4215, 0, 15756, 0, 28341, 0,
    16384, 0, 16384, 0, 16384, 0, 26726, 0, 1045, 0, 11703, 0,
    20590, 0, 18554, 0, 25970, 0, 31938, 0, 5583, 0, 21313, 0,
    29390, 0, 641, 0, 22265, 0, 31452, 0, 26584, 0, 188, 0,
    8847, 0, 24519, 0, 22938, 0, 30583, 0, 32608, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_16[32] = {
    2125, 2551, 5165, 8946, 0, 0, 0, 0, 513, 765, 1859, 6339,
    0, 0, 0, 0, 7637, 9498, 14259, 19108, 0, 0, 0, 0,
    2497, 4096, 8866, 16993, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_32[32] = {
    989, 1249, 2019, 4151, 10785, 0, 0, 0, 313, 441, 1099, 2917,
    8562, 0, 0, 0, 8394, 10352, 13932, 18855, 26014, 0, 0, 0,
    2578, 4124, 8181, 13670, 24234, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_64[32] = {
    1260, 1446, 2253, 3712, 6652, 13369, 0, 0, 401, 605, 1029, 2563,
    5845, 12626, 0, 0, 8609, 10612, 14624, 18714, 22614, 29024, 0, 0,
    1923, 3127, 5867, 9703, 14277, 27100, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_128[32] = {
    685, 933, 1488, 2714, 4766, 8562, 19254, 0, 217, 352, 618, 2303,
    5261, 9969, 17472, 0, 8045, 11200, 15497, 19595, 23948, 27408, 30938, 0,
    2310, 4160, 7471, 14997, 17931, 20768, 30240, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_256[64] = {
    1448, 2109, 4151, 6263, 9329, 13260, 17944, 23300, 0, 0, 0, 0,
    0, 0, 0, 0, 399, 1019, 1749, 3038, 10444, 15546, 22739, 27294,
    0, 0, 0, 0, 0, 0, 0, 0, 6402, 8148, 12623, 15072,
    18728, 22847, 26447, 29377, 0, 0, 0, 0, 0, 0, 0, 0,
    1674, 3252, 5734, 10159, 22397, 23802, 24821, 30940, 0, 0, 0, 0,
    0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_512[32] = {
    1230, 2278, 5035, 7776, 11871, 15346, 19590, 24584, 28749, 0, 0, 0,
    0, 0, 0, 0, 7265, 9979, 15819, 19250, 21780, 23846, 26478, 28396,
    31811, 0, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_bin_1024[32] = {
    696, 948, 3145, 5702, 9706, 13217, 17851, 21856, 25692, 28034, 0, 0,
    0, 0, 0, 0, 2672, 3591, 9330, 17084, 22725, 24284, 26527, 28027,
    28377, 30876, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_base_tok[160] = {
    17560, 29888, 0, 0, 29671, 31549, 0, 0, 31007, 32056, 0, 0,
    27286, 30006, 0, 0, 26594, 31212, 0, 0, 31208, 32582, 0, 0,
    31835, 32637, 0, 0, 30595, 32206, 0, 0, 15239, 29932, 0, 0,
    31315, 32095, 0, 0, 32130, 32434, 0, 0, 30864, 31996, 0, 0,
    26279, 30968, 0, 0, 31142, 32495, 0, 0, 31713, 32540, 0, 0,
    31929, 32594, 0, 0, 2644, 25198, 0, 0, 32038, 32451, 0, 0,
    32639, 32695, 0, 0, 32166, 32518, 0, 0, 17187, 27668, 0, 0,
    31714, 32550, 0, 0, 32283, 32678, 0, 0, 31930, 32563, 0, 0,
    1044, 2257, 0, 0, 30755, 31923, 0, 0, 32208, 32693, 0, 0,
    32244, 32615, 0, 0, 21317, 26207, 0, 0, 29133, 30868, 0, 0,
    29311, 31231, 0, 0, 29657, 31087, 0, 0, 478, 1834, 0, 0,
    31005, 31987, 0, 0, 32317, 32724, 0, 0, 30865, 32648, 0, 0,
    10923, 21845, 0, 0, 10923, 21845, 0, 0, 10923, 21845, 0, 0,
    10923, 21845, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_base_tok[1640] = {
    6041, 11854, 15927, 0, 20326, 30905, 32251, 0, 14164, 26831, 30725, 0,
    9760, 20647, 26585, 0, 6416, 14953, 21219, 0, 2966, 7151, 10891, 0,
    23567, 31374, 32254, 0, 14978, 27416, 30946, 0, 9434, 20225, 26254, 0,
    6658, 14558, 20535, 0, 3916, 8677, 12989, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    18088, 29545, 31587, 0, 13062, 25843, 30073, 0, 8940, 16827, 22251, 0,
    7654, 13220, 17973, 0, 5733, 10316, 14456, 0, 22879, 31388, 32114, 0,
    15215, 27993, 30955, 0, 9397, 19445, 24978, 0, 3442, 9813, 15344, 0,
    1368, 3936, 6532, 0, 25494, 32033, 32406, 0, 16772, 27963, 30718, 0,
    9419, 18165, 23260, 0, 2677, 7501, 11797, 0, 1516, 4344, 7170, 0,
    26556, 31454, 32101, 0, 17128, 27035, 30108, 0, 8324, 15344, 20249, 0,
    1903, 5696, 9469, 0, 8192, 16384, 24576, 0, 8455, 19003, 24368, 0,
    23563, 32021, 32604, 0, 16237, 29446, 31935, 0, 10724, 23999, 29358, 0,
    6725, 17528, 24416, 0, 3927, 10927, 16825, 0, 26313, 32288, 32634, 0,
    17430, 30095, 32095, 0, 11116, 24606, 29679, 0, 7195, 18384, 25269, 0,
    4726, 12852, 19315, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 22822, 31648, 32483, 0,
    16724, 29633, 31929, 0, 10261, 23033, 28725, 0, 7029, 17840, 24528, 0,
    4867, 13886, 21502, 0, 25298, 31892, 32491, 0, 17809, 29330, 31512, 0,
    9668, 21329, 26579, 0, 4774, 12956, 18976, 0, 2322, 7030, 11540, 0,
    25472, 31920, 32543, 0, 17957, 29387, 31632, 0, 9196, 20593, 26400, 0,
    4680, 12705, 19202, 0, 2917, 8456, 13436, 0, 26471, 32059, 32574, 0,
    18458, 29783, 31909, 0, 8400, 19464, 25956, 0, 3812, 10973, 17206, 0,
    8192, 16384, 24576, 0, 6779, 13743, 17678, 0, 24806, 31797, 32457, 0,
    17616, 29047, 31372, 0, 11063, 23175, 28003, 0, 6521, 16110, 22324, 0,
    2764, 7504, 11654, 0, 25266, 32367, 32637, 0, 19054, 30553, 32175, 0,
    12139, 25212, 29807, 0, 7311, 18162, 24704, 0, 3397, 9164, 14074, 0,
    25988, 32208, 32522, 0, 16253, 28912, 31526, 0, 9151, 21387, 27372, 0,
    5688, 14915, 21496, 0, 2717, 7627, 12004, 0, 23144, 31855, 32443, 0,
    16070, 28491, 31325, 0, 8702, 20467, 26517, 0, 5243, 13956, 20367, 0,
    2621, 7335, 11567, 0, 26636, 32340, 32630, 0, 19990, 31050, 32341, 0,
    13243, 26105, 30315, 0, 8588, 19521, 25918, 0, 4717, 11585, 17304, 0,
    25844, 32292, 32582, 0, 19090, 30635, 32097, 0, 11963, 24546, 28939, 0,
    6218, 16087, 22354, 0, 2340, 6608, 10426, 0, 28046, 32576, 32694, 0,
    21178, 31313, 32296, 0, 13486, 26184, 29870, 0, 7149, 17871, 23723, 0,
    2833, 7958, 12259, 0, 27710, 32528, 32686, 0, 20674, 31076, 32268, 0,
    12413, 24955, 29243, 0, 6676, 16927, 23097, 0, 2966, 8333, 12919, 0,
    8639, 19339, 24429, 0, 24404, 31837, 32525, 0, 16997, 29425, 31784, 0,
    11253, 24234, 29149, 0, 6751, 17394, 24028, 0, 3490, 9830, 15191, 0,
    26283, 32471, 32714, 0, 19599, 31168, 32442, 0, 13146, 26954, 30893, 0,
    8214, 20588, 26890, 0, 4699, 13081, 19300, 0, 28212, 32458, 32669, 0,
    18594, 30316, 32100, 0, 11219, 24408, 29234, 0, 6865, 17656, 24149, 0,
    3678, 10362, 16006, 0, 25825, 32136, 32616, 0, 17313, 29853, 32021, 0,
    11197, 24471, 29472, 0, 6947, 17781, 24405, 0, 3768, 10660, 16261, 0,
    27352, 32500, 32706, 0, 20850, 31468, 32469, 0, 14021, 27707, 31133, 0,
    8964, 21748, 27838, 0, 5437, 14665, 21187, 0, 26304, 32492, 32698, 0,
    20409, 31380, 32385, 0, 13682, 27222, 30632, 0, 8974, 21236, 26685, 0,
    4234, 11665, 16934, 0, 26273, 32357, 32711, 0, 20672, 31242, 32441, 0,
    14172, 27254, 30902, 0, 9870, 21898, 27275, 0, 5164, 13506, 19270, 0,
    26725, 32459, 32728, 0, 20991, 31442, 32527, 0, 13071, 26434, 30811, 0,
    8184, 20090, 26742, 0, 4803, 13255, 19895, 0, 7555, 14942, 18501, 0,
    24410, 31178, 32287, 0, 14394, 26738, 30253, 0, 8413, 19554, 25195, 0,
    4766, 12924, 18785, 0, 2029, 5806, 9207, 0, 26776, 32364, 32663, 0,
    18732, 29967, 31931, 0, 11005, 23786, 28852, 0, 6466, 16909, 23510, 0,
    3044, 8638, 13419, 0, 29208, 32582, 32704, 0, 20068, 30857, 32208, 0,
    12003, 25085, 29595, 0, 6947, 17750, 24189, 0, 3245, 9103, 14007, 0,
    27359, 32465, 32669, 0, 19421, 30614, 32174, 0, 11915, 25010, 29579, 0,
    6950, 17676, 24074, 0, 3007, 8473, 13096, 0, 29002, 32676, 32735, 0,
    22102, 31849, 32576, 0, 14408, 28009, 31405, 0, 9027, 21679, 27931, 0,
    4694, 12678, 18748, 0, 28216, 32528, 32682, 0, 20849, 31264, 32318, 0,
    12756, 25815, 29751, 0, 7565, 18801, 24923, 0, 3509, 9533, 14477, 0,
    30133, 32687, 32739, 0, 23063, 31910, 32515, 0, 14588, 28051, 31132, 0,
    9085, 21649, 27457, 0, 4261, 11654, 17264, 0, 29518, 32691, 32748, 0,
    22451, 31959, 32613, 0, 14864, 28722, 31700, 0, 9695, 22964, 28716, 0,
    4932, 13358, 19502, 0, 6465, 16958, 21688, 0, 25199, 31514, 32360, 0,
    14774, 27149, 30607, 0, 9257, 21438, 26972, 0, 5723, 15183, 21882, 0,
    3150, 8879, 13731, 0, 26989, 32262, 32682, 0, 17396, 29937, 32085, 0,
    11387, 24901, 29784, 0, 7289, 18821, 25548, 0, 3734, 10577, 16086, 0,
    29728, 32501, 32695, 0, 17431, 29701, 31903, 0, 9921, 22826, 28300, 0,
    5896, 15434, 22068, 0, 3430, 9646, 14757, 0, 28614, 32511, 32705, 0,
    19364, 30638, 32263, 0, 13129, 26254, 30402, 0, 8754, 20484, 26440, 0,
    4378, 11607, 17110, 0, 30292, 32671, 32744, 0, 21780, 31603, 32501, 0,
    14314, 27829, 31291, 0, 9611, 22327, 28263, 0, 4890, 13087, 19065, 0,
    25862, 32567, 32733, 0, 20794, 32050, 32567, 0, 17243, 30625, 32254, 0,
    13283, 27628, 31474, 0, 9669, 22532, 28918, 0, 27435, 32697, 32748, 0,
    24922, 32390, 32714, 0, 21449, 31504, 32536, 0, 16392, 29729, 31832, 0,
    11692, 24884, 29076, 0, 24193, 32290, 32735, 0, 18909, 31104, 32563, 0,
    12236, 26841, 31403, 0, 8171, 21840, 29082, 0, 7224, 17280, 25275, 0,
    3078, 6839, 9890, 0, 13837, 20450, 24479, 0, 5914, 14222, 19328, 0,
    3866, 10267, 14762, 0, 2612, 7208, 11042, 0, 1067, 2991, 4776, 0,
    25817, 31646, 32529, 0, 13708, 26338, 30385, 0, 7328, 18585, 24870, 0,
    4691, 13080, 19276, 0, 1825, 5253, 8352, 0, 29386, 32315, 32624, 0,
    17160, 29001, 31360, 0, 9602, 21862, 27396, 0, 5915, 15772, 22148, 0,
    2786, 7779, 12047, 0, 29246, 32450, 32663, 0, 18696, 29929, 31818, 0,
    10510, 23369, 28560, 0, 6229, 16499, 23125, 0, 2608, 7448, 11705, 0,
    30753, 32710, 32748, 0, 21638, 31487, 32503, 0, 12937, 26854, 30870, 0,
    8182, 20596, 26970, 0, 3637, 10269, 15497, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 5244, 12150, 16906, 0,
    20486, 26858, 29701, 0, 7756, 18317, 23735, 0, 3452, 9256, 13146, 0,
    2020, 5206, 8229, 0, 1801, 4993, 7903, 0, 27051, 31858, 32531, 0,
    15988, 27531, 30619, 0, 9188, 21484, 26719, 0, 6273, 17186, 23800, 0,
    3108, 9355, 14764, 0, 31076, 32520, 32680, 0, 18119, 30037, 31850, 0,
    10244, 22969, 27472, 0, 4692, 14077, 19273, 0, 3694, 11677, 17556, 0,
    30060, 32581, 32720, 0, 21011, 30775, 32120, 0, 11931, 24820, 29289, 0,
    7119, 17662, 24356, 0, 3833, 10706, 16304, 0, 31954, 32731, 32748, 0,
    23913, 31724, 32489, 0, 15520, 28060, 31286, 0, 11517, 23008, 28571, 0,
    6193, 14508, 20629, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 1035, 2807, 4156, 0, 13162, 18138, 20939, 0,
    2696, 6633, 8755, 0, 1373, 4161, 6853, 0, 1099, 2746, 4716, 0,
    340, 1021, 1599, 0, 22826, 30419, 32135, 0, 10395, 21762, 26942, 0,
    4726, 12407, 17361, 0, 2447, 7080, 10593, 0, 1227, 3717, 6011, 0,
    28156, 31424, 31934, 0, 16915, 27754, 30373, 0, 9148, 20990, 26431, 0,
    5950, 15515, 21148, 0, 2492, 7327, 11526, 0, 30602, 32477, 32670, 0,
    20026, 29955, 31568, 0, 11220, 23628, 28105, 0, 6652, 17019, 22973, 0,
    3064, 8536, 13043, 0, 31769, 32724, 32748, 0, 22230, 30887, 32373, 0,
    12234, 25079, 29731, 0, 7326, 18816, 25353, 0, 3933, 10907, 16616, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_br_tok[672] = {
    14995, 21341, 24749, 0, 13158, 20289, 24601, 0, 8941, 15326, 19876, 0,
    6297, 11541, 15807, 0, 4817, 9029, 12776, 0, 3731, 7273, 10627, 0,
    1847, 3617, 5354, 0, 14472, 19659, 22343, 0, 16806, 24162, 27533, 0,
    12900, 20404, 24713, 0, 9411, 16112, 20797, 0, 7056, 12697, 17148, 0,
    5544, 10339, 14460, 0, 2954, 5704, 8319, 0, 12464, 18071, 21354, 0,
    15482, 22528, 26034, 0, 12070, 19269, 23624, 0, 8953, 15406, 20106, 0,
    7027, 12730, 17220, 0, 5887, 10913, 15140, 0, 3793, 7278, 10447, 0,
    15571, 22232, 25749, 0, 14506, 21575, 25374, 0, 10189, 17089, 21569, 0,
    7316, 13301, 17915, 0, 5783, 10912, 15190, 0, 4760, 9155, 13088, 0,
    2993, 5966, 8774, 0, 23424, 28903, 30778, 0, 20775, 27666, 30290, 0,
    16474, 24410, 28299, 0, 12471, 20180, 24987, 0, 9410, 16487, 21439, 0,
    7536, 13614, 18529, 0, 5048, 9586, 13549, 0, 21090, 27290, 29756, 0,
    20796, 27402, 30026, 0, 17819, 25485, 28969, 0, 13860, 21909, 26462, 0,
    11002, 18494, 23529, 0, 8953, 15929, 20897, 0, 6448, 11918, 16454, 0,
    15999, 22208, 25449, 0, 13050, 19988, 24122, 0, 8594, 14864, 19378, 0,
    6033, 11079, 15238, 0, 4554, 8683, 12347, 0, 3672, 7139, 10337, 0,
    1900, 3771, 5576, 0, 15788, 21340, 23949, 0, 16825, 24235, 27758, 0,
    12873, 20402, 24810, 0, 9590, 16363, 21094, 0, 7352, 13209, 17733, 0,
    5960, 10989, 15184, 0, 3232, 6234, 9007, 0, 15761, 20716, 23224, 0,
    19318, 25989, 28759, 0, 15529, 23094, 26929, 0, 11662, 18989, 23641, 0,
    8955, 15568, 20366, 0, 7281, 13106, 17708, 0, 4248, 8059, 11440, 0,
    14899, 21217, 24503, 0, 13519, 20283, 24047, 0, 9429, 15966, 20365, 0,
    6700, 12355, 16652, 0, 5088, 9704, 13716, 0, 4243, 8154, 11731, 0,
    2702, 5364, 7861, 0, 22745, 28388, 30454, 0, 20235, 27146, 29922, 0,
    15896, 23715, 27637, 0, 11840, 19350, 24131, 0, 9122, 15932, 20880, 0,
    7488, 13581, 18362, 0, 5114, 9568, 13370, 0, 20845, 26553, 28932, 0,
    20981, 27372, 29884, 0, 17781, 25335, 28785, 0, 13760, 21708, 26297, 0,
    10975, 18415, 23365, 0, 9045, 15789, 20686, 0, 6130, 11199, 15423, 0,
    13549, 19724, 23158, 0, 11844, 18382, 22246, 0, 7919, 13619, 17773, 0,
    5486, 10143, 13946, 0, 4166, 7983, 11324, 0, 3364, 6506, 9427, 0,
    1598, 3160, 4674, 0, 15281, 20979, 23781, 0, 14939, 22119, 25952, 0,
    11363, 18407, 22812, 0, 8609, 14857, 19370, 0, 6737, 12184, 16480, 0,
    5506, 10263, 14262, 0, 2990, 5786, 8380, 0, 20249, 25253, 27417, 0,
    21070, 27518, 30001, 0, 16854, 24469, 28074, 0, 12864, 20486, 25000, 0,
    9962, 16978, 21778, 0, 8074, 14338, 19048, 0, 4494, 8479, 11906, 0,
    13960, 19617, 22829, 0, 11150, 17341, 21228, 0, 7150, 12964, 17190, 0,
    5331, 10002, 13867, 0, 4167, 7744, 11057, 0, 3480, 6629, 9646, 0,
    1883, 3784, 5686, 0, 18752, 25660, 28912, 0, 16968, 24586, 28030, 0,
    13520, 21055, 25313, 0, 10453, 17626, 22280, 0, 8386, 14505, 19116, 0,
    6742, 12595, 17008, 0, 4273, 8140, 11499, 0, 22120, 27827, 30233, 0,
    20563, 27358, 29895, 0, 17076, 24644, 28153, 0, 13362, 20942, 25309, 0,
    10794, 17965, 22695, 0, 9014, 15652, 20319, 0, 5708, 10512, 14497, 0,
    5705, 10930, 15725, 0, 7946, 12765, 16115, 0, 6801, 12123, 16226, 0,
    5462, 10135, 14200, 0, 4189, 8011, 11507, 0, 3191, 6229, 9408, 0,
    1057, 2137, 3212, 0, 10018, 17067, 21491, 0, 7380, 12582, 16453, 0,
    6068, 10845, 14339, 0, 5098, 9198, 12555, 0, 4312, 8010, 11119, 0,
    3700, 6966, 9781, 0, 1693, 3326, 4887, 0, 18757, 24930, 27774, 0,
    17648, 24596, 27817, 0, 14707, 22052, 26026, 0, 11720, 18852, 23292, 0,
    9357, 15952, 20525, 0, 7810, 13753, 18210, 0, 3879, 7333, 10328, 0,
    8278, 13242, 15922, 0, 10547, 15867, 18919, 0, 9106, 15842, 20609, 0,
    6833, 13007, 17218, 0, 4811, 9712, 13923, 0, 3985, 7352, 11128, 0,
    1688, 3458, 5262, 0, 12951, 21861, 26510, 0, 9788, 16044, 20276, 0,
    6309, 11244, 14870, 0, 5183, 9349, 12566, 0, 4389, 8229, 11492, 0,
    3633, 6945, 10620, 0, 3600, 6847, 9907, 0, 21748, 28137, 30255, 0,
    19436, 26581, 29560, 0, 16359, 24201, 27953, 0, 13961, 21693, 25871, 0,
    11544, 18686, 23322, 0, 9372, 16462, 20952, 0, 6138, 11210, 15390, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_eob_hi_bit[180] = {
    17471, 0, 20223, 0, 11357, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 20335, 0, 21667, 0, 14818, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    20430, 0, 20662, 0, 15367, 0, 16970, 0, 14657, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 22117, 0, 22028, 0, 18650, 0,
    16042, 0, 15885, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    22409, 0, 21012, 0, 15650, 0, 17395, 0, 15469, 0, 20205, 0,
    19511, 0, 16384, 0, 16384, 0, 24220, 0, 22480, 0, 17737, 0,
    18916, 0, 19268, 0, 18412, 0, 18844, 0, 16384, 0, 16384, 0,
    25991, 0, 20314, 0, 17731, 0, 19678, 0, 18649, 0, 17307, 0,
    21798, 0, 17549, 0, 15630, 0, 26585, 0, 21469, 0, 20432, 0,
    17735, 0, 19280, 0, 15235, 0, 20297, 0, 22471, 0, 28997, 0,
    26605, 0, 11304, 0, 16726, 0, 16560, 0, 20866, 0, 23524, 0,
    19878, 0, 13469, 0, 23084, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef1_dc_sign[12] = {
    16000, 0, 13056, 0, 18816, 0, 15232, 0, 12928, 0, 17280, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_skip[130] = {
    29614, 0, 9068, 0, 12924, 0, 19538, 0, 17737, 0, 24619, 0,
    30642, 0, 4119, 0, 16026, 0, 25657, 0, 16384, 0, 16384, 0,
    16384, 0, 31957, 0, 3230, 0, 11153, 0, 18123, 0, 20143, 0,
    26536, 0, 31986, 0, 3050, 0, 14603, 0, 25155, 0, 16384, 0,
    16384, 0, 16384, 0, 32363, 0, 10692, 0, 19090, 0, 24357, 0,
    24442, 0, 28312, 0, 32169, 0, 3648, 0, 15690, 0, 26815, 0,
    16384, 0, 16384, 0, 16384, 0, 30669, 0, 3832, 0, 11663, 0,
    18889, 0, 19782, 0, 23313, 0, 31330, 0, 5124, 0, 18719, 0,
    28468, 0, 3082, 0, 20982, 0, 29443, 0, 28573, 0, 3183, 0,
    17802, 0, 25977, 0, 26677, 0, 27832, 0, 32387, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_16[32] = {
    4016, 4897, 8881, 14968, 0, 0, 0, 0, 716, 1105, 2646, 10056,
    0, 0, 0, 0, 11139, 13270, 18241, 23566, 0, 0, 0, 0,
    3192, 5032, 10297, 19755, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_32[32] = {
    2515, 3003, 4452, 8162, 16041, 0, 0, 0, 574, 821, 1836, 5089,
    13128, 0, 0, 0, 13468, 16303, 20361, 25105, 29281, 0, 0, 0,
    3542, 5502, 10415, 16760, 25644, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_64[32] = {
    2374, 2772, 4583, 7276, 12288, 19706, 0, 0, 497, 810, 1315, 3000,
    7004, 15641, 0, 0, 15050, 17126, 21410, 24886, 28156, 30726, 0, 0,
    4034, 6290, 10235, 14982, 21214, 28491, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_128[32] = {
    1366, 1738, 2527, 5016, 9355, 15797, 24643, 0, 354, 558, 944, 2760,
    7287, 14037, 21779, 0, 13627, 16246, 20173, 24429, 27948, 30415, 31863, 0,
    6275, 9889, 14769, 23164, 27988, 30493, 32272, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_256[64] = {
    3089, 3920, 6038, 9460, 14266, 19881, 25766, 29176, 0, 0, 0, 0,
    0, 0, 0, 0, 1084, 2358, 3488, 5122, 11483, 18103, 26023, 29799,
    0, 0, 0, 0, 0, 0, 0, 0, 11514, 13794, 17480, 20754,
    24361, 27378, 29492, 31277, 0, 0, 0, 0, 0, 0, 0, 0,
    6571, 9610, 15516, 21826, 29092, 30829, 31842, 32708, 0, 0, 0, 0,
    0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_512[32] = {
    2624, 3936, 6480, 9686, 13979, 17726, 23267, 28410, 31078, 0, 0, 0,
    0, 0, 0, 0, 12015, 14769, 19588, 22052, 24222, 25812, 27300, 29219,
    32114, 0, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_bin_1024[32] = {
    2784, 3831, 7041, 10521, 14847, 18844, 23155, 26682, 29229, 31045, 0, 0,
    0, 0, 0, 0, 9577, 12466, 17739, 20750, 22061, 23215, 24601, 25483,
    25843, 32056, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_base_tok[160] = {
    20092, 30774, 0, 0, 30695, 32020, 0, 0, 31131, 32103, 0, 0,
    28666, 30870, 0, 0, 27258, 31095, 0, 0, 31804, 32623, 0, 0,
    31763, 32528, 0, 0, 31438, 32506, 0, 0, 18049, 30489, 0, 0,
    31706, 32286, 0, 0, 32163, 32473, 0, 0, 31550, 32184, 0, 0,
    27116, 30842, 0, 0, 31971, 32598, 0, 0, 32088, 32576, 0, 0,
    32067, 32664, 0, 0, 12854, 29093, 0, 0, 32272, 32558, 0, 0,
    32667, 32729, 0, 0, 32306, 32585, 0, 0, 25476, 30366, 0, 0,
    32169, 32687, 0, 0, 32479, 32689, 0, 0, 31673, 32634, 0, 0,
    2809, 19301, 0, 0, 32205, 32622, 0, 0, 32338, 32730, 0, 0,
    31786, 32616, 0, 0, 22737, 29105, 0, 0, 30810, 32362, 0, 0,
    30014, 32627, 0, 0, 30528, 32574, 0, 0, 935, 3382, 0, 0,
    30789, 31909, 0, 0, 32466, 32756, 0, 0, 30860, 32513, 0, 0,
    10923, 21845, 0, 0, 10923, 21845, 0, 0, 10923, 21845, 0, 0,
    10923, 21845, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_base_tok[1640] = {
    8896, 16227, 20630, 0, 23629, 31782, 32527, 0, 15173, 27755, 31321, 0,
    10158, 21233, 27382, 0, 6420, 14857, 21558, 0, 3269, 8155, 12646, 0,
    24835, 32009, 32496, 0, 16509, 28421, 31579, 0, 10957, 21514, 27418, 0,
    7881, 15930, 22096, 0, 5388, 10960, 15918, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    20745, 30773, 32093, 0, 15200, 27221, 30861, 0, 13032, 20873, 25667, 0,
    12285, 18663, 23494, 0, 11563, 17481, 21489, 0, 26260, 31982, 32320, 0,
    15397, 28083, 31100, 0, 9742, 19217, 24824, 0, 3261, 9629, 15362, 0,
    1480, 4322, 7499, 0, 27599, 32256, 32460, 0, 16857, 27659, 30774, 0,
    9551, 18290, 23748, 0, 3052, 8933, 14103, 0, 2021, 5910, 9787, 0,
    29005, 32015, 32392, 0, 17677, 27694, 30863, 0, 9204, 17356, 23219, 0,
    2403, 7516, 12814, 0, 8192, 16384, 24576, 0, 10808, 22056, 26896, 0,
    25739, 32313, 32676, 0, 17288, 30203, 32221, 0, 11359, 24878, 29896, 0,
    6949, 17767, 24893, 0, 4287, 11796, 18071, 0, 27880, 32521, 32705, 0,
    19038, 31004, 32414, 0, 12564, 26345, 30768, 0, 8269, 19947, 26779, 0,
    5674, 14657, 21674, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 25742, 32319, 32671, 0,
    19557, 31164, 32454, 0, 13381, 26381, 30755, 0, 10101, 21466, 26722, 0,
    9209, 19650, 26825, 0, 27107, 31917, 32432, 0, 18056, 28893, 31203, 0,
    10200, 21434, 26764, 0, 4660, 12913, 19502, 0, 2368, 6930, 12504, 0,
    26960, 32158, 32613, 0, 18628, 30005, 32031, 0, 10233, 22442, 28232, 0,
    5471, 14630, 21516, 0, 3235, 10767, 17109, 0, 27696, 32440, 32692, 0,
    20032, 31167, 32438, 0, 8700, 21341, 28442, 0, 5662, 14831, 21795, 0,
    8192, 16384, 24576, 0, 9704, 17294, 21132, 0, 26762, 32278, 32633, 0,
    18382, 29620, 31819, 0, 10891, 23475, 28723, 0, 6358, 16583, 23309, 0,
    3248, 9118, 14141, 0, 27204, 32573, 32699, 0, 19818, 30824, 32329, 0,
    11772, 25120, 30041, 0, 6995, 18033, 25039, 0, 3752, 10442, 16098, 0,
    27222, 32256, 32559, 0, 15356, 28399, 31475, 0, 8821, 20635, 27057, 0,
    5511, 14404, 21239, 0, 2935, 8222, 13051, 0, 24875, 32120, 32529, 0,
    15233, 28265, 31445, 0, 8605, 20570, 26932, 0, 5431, 14413, 21196, 0,
    2994, 8341, 13223, 0, 28201, 32604, 32700, 0, 21041, 31446, 32456, 0,
    13221, 26213, 30475, 0, 8255, 19385, 26037, 0, 4930, 12585, 18830, 0,
    28768, 32448, 32627, 0, 19705, 30561, 32021, 0, 11572, 23589, 28220, 0,
    5532, 15034, 21446, 0, 2460, 7150, 11456, 0, 29874, 32619, 32699, 0,
    21621, 31071, 32201, 0, 12511, 24747, 28992, 0, 6281, 16395, 22748, 0,
    3246, 9278, 14497, 0, 29715, 32625, 32712, 0, 20958, 31011, 32283, 0,
    11233, 23671, 28806, 0, 6012, 16128, 22868, 0, 3427, 9851, 15414, 0,
    11016, 22111, 26794, 0, 25946, 32357, 32677, 0, 17890, 30452, 32252, 0,
    11678, 25142, 29816, 0, 6720, 17534, 24584, 0, 4230, 11665, 17820, 0,
    28400, 32623, 32747, 0, 21164, 31668, 32575, 0, 13572, 27388, 31182, 0,
    8234, 20750, 27358, 0, 5065, 14055, 20897, 0, 28981, 32547, 32705, 0,
    18681, 30543, 32239, 0, 10919, 24075, 29286, 0, 6431, 17199, 24077, 0,
    3819, 10464, 16618, 0, 26870, 32467, 32693, 0, 19041, 30831, 32347, 0,
    11794, 25211, 30016, 0, 6888, 18019, 24970, 0, 4370, 12363, 18992, 0,
    29578, 32670, 32744, 0, 23159, 32007, 32613, 0, 15315, 28669, 31676, 0,
    9298, 22607, 28782, 0, 6144, 15913, 22968, 0, 28110, 32499, 32669, 0,
    21574, 30937, 32015, 0, 12759, 24818, 28727, 0, 6545, 16761, 23042, 0,
    3649, 10597, 16833, 0, 28163, 32552, 32728, 0, 22101, 31469, 32464, 0,
    13160, 25472, 30143, 0, 7303, 18684, 25468, 0, 5241, 13975, 20955, 0,
    28400, 32631, 32744, 0, 22104, 31793, 32603, 0, 13557, 26571, 30846, 0,
    7749, 19861, 26675, 0, 4873, 14030, 21234, 0, 9800, 17635, 21073, 0,
    26153, 31885, 32527, 0, 15038, 27852, 31006, 0, 8718, 20564, 26486, 0,
    5128, 14076, 20514, 0, 2636, 7566, 11925, 0, 27551, 32504, 32701, 0,
    18310, 30054, 32100, 0, 10211, 23420, 29082, 0, 6222, 16876, 23916, 0,
    3462, 9954, 15498, 0, 29991, 32633, 32721, 0, 19883, 30751, 32201, 0,
    11141, 24184, 29285, 0, 6420, 16940, 23774, 0, 3392, 9753, 15118, 0,
    28465, 32616, 32712, 0, 19850, 30702, 32244, 0, 10983, 24024, 29223, 0,
    6294, 16770, 23582, 0, 3244, 9283, 14509, 0, 30023, 32717, 32748, 0,
    22940, 32032, 32626, 0, 14282, 27928, 31473, 0, 8562, 21327, 27914, 0,
    4846, 13393, 19919, 0, 29981, 32590, 32695, 0, 20465, 30963, 32166, 0,
    11479, 23579, 28195, 0, 5916, 15648, 22073, 0, 3031, 8605, 13398, 0,
    31146, 32691, 32739, 0, 23106, 31724, 32444, 0, 13783, 26738, 30439, 0,
    7852, 19468, 25807, 0, 3860, 11124, 16853, 0, 31014, 32724, 32748, 0,
    23629, 32109, 32628, 0, 14747, 28115, 31403, 0, 8545, 21242, 27478, 0,
    4574, 12781, 19067, 0, 9185, 19694, 24688, 0, 26081, 31985, 32621, 0,
    16015, 29000, 31787, 0, 10542, 23690, 29206, 0, 6732, 17945, 24677, 0,
    3916, 11039, 16722, 0, 28224, 32566, 32744, 0, 19100, 31138, 32485, 0,
    12528, 26620, 30879, 0, 7741, 20277, 26885, 0, 4566, 12845, 18990, 0,
    29933, 32593, 32718, 0, 17670, 30333, 32155, 0, 10385, 23600, 28909, 0,
    6243, 16236, 22407, 0, 3976, 10389, 16017, 0, 28377, 32561, 32738, 0,
    19366, 31175, 32482, 0, 13327, 27175, 31094, 0, 8258, 20769, 27143, 0,
    4703, 13198, 19527, 0, 31086, 32706, 32748, 0, 22853, 31902, 32583, 0,
    14759, 28186, 31419, 0, 9284, 22382, 28348, 0, 5585, 15192, 21868, 0,
    28291, 32652, 32746, 0, 19849, 32107, 32571, 0, 14834, 26818, 29214, 0,
    10306, 22594, 28672, 0, 6615, 17384, 23384, 0, 28947, 32604, 32745, 0,
    25625, 32289, 32646, 0, 18758, 28672, 31403, 0, 10017, 23430, 28523, 0,
    6862, 15269, 22131, 0, 23933, 32509, 32739, 0, 19927, 31495, 32631, 0,
    11903, 26023, 30621, 0, 7026, 20094, 27252, 0, 5998, 18106, 24437, 0,
    4456, 11274, 15533, 0, 21219, 29079, 31616, 0, 11173, 23774, 28567, 0,
    7282, 18293, 24263, 0, 4890, 13286, 19115, 0, 1890, 5508, 8659, 0,
    26651, 32136, 32647, 0, 14630, 28254, 31455, 0, 8716, 21287, 27395, 0,
    5615, 15331, 22008, 0, 2675, 7700, 12150, 0, 29954, 32526, 32690, 0,
    16126, 28982, 31633, 0, 9030, 21361, 27352, 0, 5411, 14793, 21271, 0,
    2943, 8422, 13163, 0, 29539, 32601, 32730, 0, 18125, 30385, 32201, 0,
    10422, 24090, 29468, 0, 6468, 17487, 24438, 0, 2970, 8653, 13531, 0,
    30912, 32715, 32748, 0, 20666, 31373, 32497, 0, 12509, 26640, 30917, 0,
    8058, 20629, 27290, 0, 4231, 12006, 18052, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 10202, 20633, 25484, 0,
    27336, 31445, 32352, 0, 12420, 24384, 28552, 0, 7648, 18115, 23856, 0,
    5662, 14341, 19902, 0, 3611, 10328, 15390, 0, 30945, 32616, 32736, 0,
    18682, 30505, 32253, 0, 11513, 25336, 30203, 0, 7449, 19452, 26148, 0,
    4482, 13051, 18886, 0, 32022, 32690, 32747, 0, 18578, 30501, 32146, 0,
    11249, 23368, 28631, 0, 5645, 16958, 22158, 0, 5009, 11444, 16637, 0,
    31357, 32710, 32748, 0, 21552, 31494, 32504, 0, 13891, 27677, 31340, 0,
    9051, 22098, 28172, 0, 5190, 13377, 19486, 0, 32364, 32740, 32748, 0,
    24839, 31907, 32551, 0, 17160, 28779, 31696, 0, 12452, 24137, 29602, 0,
    6165, 15389, 22477, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 2575, 7281, 11077, 0, 14002, 20866, 25402, 0,
    6343, 15056, 19658, 0, 4474, 11858, 17041, 0, 2865, 8299, 12534, 0,
    1344, 3949, 6391, 0, 24720, 31239, 32459, 0, 12585, 25356, 29968, 0,
    7181, 18246, 24444, 0, 5025, 13667, 19885, 0, 2521, 7304, 11605, 0,
    29908, 32252, 32584, 0, 17421, 29156, 31575, 0, 9889, 22188, 27782, 0,
    5878, 15647, 22123, 0, 2814, 8665, 13323, 0, 30183, 32568, 32713, 0,
    18528, 30195, 32049, 0, 10982, 24606, 29657, 0, 6957, 18165, 25231, 0,
    3508, 10118, 15468, 0, 31761, 32736, 32748, 0, 21041, 31328, 32546, 0,
    12568, 26732, 31166, 0, 8052, 20720, 27733, 0, 4336, 12192, 18396, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_br_tok[672] = {
    16138, 22223, 25509, 0, 15347, 22430, 26332, 0, 9614, 16736, 21332, 0,
    6600, 12275, 16907, 0, 4811, 9424, 13547, 0, 3748, 7809, 11420, 0,
    2254, 4587, 6890, 0, 15196, 20284, 23177, 0, 18317, 25469, 28451, 0,
    13918, 21651, 25842, 0, 10052, 17150, 21995, 0, 7499, 13630, 18587, 0,
    6158, 11417, 16003, 0, 4014, 7785, 11252, 0, 15048, 21067, 24384, 0,
    18202, 25346, 28553, 0, 14302, 22019, 26356, 0, 10839, 18139, 23166, 0,
    8715, 15744, 20806, 0, 7536, 13576, 18544, 0, 5413, 10335, 14498, 0,
    17394, 24501, 27895, 0, 15889, 23420, 27185, 0, 11561, 19133, 23870, 0,
    8285, 14812, 19844, 0, 6496, 12043, 16550, 0, 4771, 9574, 13677, 0,
    3603, 6830, 10144, 0, 21656, 27704, 30200, 0, 21324, 27915, 30511, 0,
    17327, 25336, 28997, 0, 13417, 21381, 26033, 0, 10132, 17425, 22338, 0,
    8580, 15016, 19633, 0, 5694, 11477, 16411, 0, 24116, 29780, 31450, 0,
    23853, 29695, 31591, 0, 20085, 27614, 30428, 0, 15326, 24335, 28575, 0,
    11814, 19472, 24810, 0, 10221, 18611, 24767, 0, 7689, 14558, 20321, 0,
    16214, 22380, 25770, 0, 14213, 21304, 25295, 0, 9213, 15823, 20455, 0,
    6395, 11758, 16139, 0, 4779, 9187, 13066, 0, 3821, 7501, 10953, 0,
    2293, 4567, 6795, 0, 15859, 21283, 23820, 0, 18404, 25602, 28726, 0,
    14325, 21980, 26206, 0, 10669, 17937, 22720, 0, 8297, 14642, 19447, 0,
    6746, 12389, 16893, 0, 4324, 8251, 11770, 0, 16532, 21631, 24475, 0,
    20667, 27150, 29668, 0, 16728, 24510, 28175, 0, 12861, 20645, 25332, 0,
    10076, 17361, 22417, 0, 8395, 14940, 19963, 0, 5731, 10683, 14912, 0,
    14433, 21155, 24938, 0, 14658, 21716, 25545, 0, 9923, 16824, 21557, 0,
    6982, 13052, 17721, 0, 5419, 10503, 15050, 0, 4852, 9162, 13014, 0,
    3271, 6395, 9630, 0, 22210, 27833, 30109, 0, 20750, 27368, 29821, 0,
    16894, 24828, 28573, 0, 13247, 21276, 25757, 0, 10038, 17265, 22563, 0,
    8587, 14947, 20327, 0, 5645, 11371, 15252, 0, 22027, 27526, 29714, 0,
    23098, 29146, 31221, 0, 19886, 27341, 30272, 0, 15609, 23747, 28046, 0,
    11993, 20065, 24939, 0, 9637, 18267, 23671, 0, 7625, 13801, 19144, 0,
    14438, 20798, 24089, 0, 12621, 19203, 23097, 0, 8177, 14125, 18402, 0,
    5674, 10501, 14456, 0, 4236, 8239, 11733, 0, 3447, 6750, 9806, 0,
    1986, 3950, 5864, 0, 16208, 22099, 24930, 0, 16537, 24025, 27585, 0,
    12780, 20381, 24867, 0, 9767, 16612, 21416, 0, 7686, 13738, 18398, 0,
    6333, 11614, 15964, 0, 3941, 7571, 10836, 0, 22819, 27422, 29202, 0,
    22224, 28514, 30721, 0, 17660, 25433, 28913, 0, 13574, 21482, 26002, 0,
    10629, 17977, 22938, 0, 8612, 15298, 20265, 0, 5607, 10491, 14596, 0,
    13569, 19800, 23206, 0, 13128, 19924, 23869, 0, 8329, 14841, 19403, 0,
    6130, 10976, 15057, 0, 4682, 8839, 12518, 0, 3656, 7409, 10588, 0,
    2577, 5099, 7412, 0, 22427, 28684, 30585, 0, 20913, 27750, 30139, 0,
    15840, 24109, 27834, 0, 12308, 20029, 24569, 0, 10216, 16785, 21458, 0,
    8309, 14203, 19113, 0, 6043, 11168, 15307, 0, 23166, 28901, 30998, 0,
    21899, 28405, 30751, 0, 18413, 26091, 29443, 0, 15233, 23114, 27352, 0,
    12683, 20472, 25288, 0, 10702, 18259, 23409, 0, 8125, 14464, 19226, 0,
    9040, 14786, 18360, 0, 9979, 15718, 19415, 0, 7913, 13918, 18311, 0,
    5859, 10889, 15184, 0, 4593, 8677, 12510, 0, 3820, 7396, 10791, 0,
    1730, 3471, 5192, 0, 11803, 18365, 22709, 0, 11419, 18058, 22225, 0,
    9418, 15774, 20243, 0, 7539, 13325, 17657, 0, 6233, 11317, 15384, 0,
    5137, 9656, 13545, 0, 2977, 5774, 8349, 0, 21207, 27246, 29640, 0,
    19547, 26578, 29497, 0, 16169, 23871, 27690, 0, 12820, 20458, 25018, 0,
    10224, 17332, 22214, 0, 8526, 15048, 19884, 0, 5037, 9410, 13118, 0,
    12339, 17329, 20140, 0, 13505, 19895, 23225, 0, 9847, 16944, 21564, 0,
    7280, 13256, 18348, 0, 4712, 10009, 14454, 0, 4361, 7914, 12477, 0,
    2870, 5628, 7995, 0, 20061, 25504, 28526, 0, 15235, 22878, 26145, 0,
    12985, 19958, 24155, 0, 9782, 16641, 21403, 0, 9456, 16360, 20760, 0,
    6855, 12940, 18557, 0, 5661, 10564, 15002, 0, 25656, 30602, 31894, 0,
    22570, 29107, 31092, 0, 18917, 26423, 29541, 0, 15940, 23649, 27754, 0,
    12803, 20581, 25219, 0, 11082, 18695, 23376, 0, 7939, 14373, 19005, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_eob_hi_bit[180] = {
    18983, 0, 20512, 0, 14885, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 20090, 0, 19444, 0, 17286, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    19139, 0, 21487, 0, 18959, 0, 20910, 0, 19089, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 20536, 0, 20664, 0, 20625, 0,
    19123, 0, 14862, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    19833, 0, 21502, 0, 17485, 0, 20267, 0, 18353, 0, 23329, 0,
    21478, 0, 16384, 0, 16384, 0, 22041, 0, 23434, 0, 20001, 0,
    20554, 0, 20951, 0, 20145, 0, 15562, 0, 16384, 0, 16384, 0,
    23312, 0, 21607, 0, 16526, 0, 18957, 0, 18034, 0, 18934, 0,
    24247, 0, 16921, 0, 17080, 0, 26579, 0, 24910, 0, 18637, 0,
    19800, 0, 20388, 0, 9887, 0, 15642, 0, 30198, 0, 24721, 0,
    26998, 0, 16737, 0, 17838, 0, 18922, 0, 19515, 0, 18636, 0,
    17333, 0, 15776, 0, 22658, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef2_dc_sign[12] = {
    16000, 0, 13056, 0, 18816, 0, 15232, 0, 12928, 0, 17280, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_skip[130] = {
    26887, 0, 6729, 0, 10361, 0, 17442, 0, 15045, 0, 22478, 0,
    29072, 0, 2713, 0, 11861, 0, 20773, 0, 16384, 0, 16384, 0,
    16384, 0, 31903, 0, 2044, 0, 7528, 0, 14618, 0, 16182, 0,
    24168, 0, 31037, 0, 2786, 0, 11194, 0, 20155, 0, 16384, 0,
    16384, 0, 16384, 0, 32510, 0, 8430, 0, 17318, 0, 24154, 0,
    23674, 0, 28789, 0, 32139, 0, 3440, 0, 13117, 0, 22702, 0,
    16384, 0, 16384, 0, 16384, 0, 31671, 0, 2056, 0, 11746, 0,
    16852, 0, 18635, 0, 24715, 0, 31484, 0, 4656, 0, 16074, 0,
    24704, 0, 1806, 0, 14645, 0, 25336, 0, 31539, 0, 8433, 0,
    20576, 0, 27904, 0, 27852, 0, 30026, 0, 32441, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_16[32] = {
    6708, 8958, 14746, 22133, 0, 0, 0, 0, 1222, 2074, 4783, 15410,
    0, 0, 0, 0, 19575, 21766, 26044, 29709, 0, 0, 0, 0,
    7297, 10767, 19273, 28194, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_32[32] = {
    4617, 5709, 8446, 13584, 23135, 0, 0, 0, 1156, 1702, 3675, 9274,
    20539, 0, 0, 0, 22086, 24282, 27010, 29770, 31743, 0, 0, 0,
    7699, 10897, 20891, 26926, 31628, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_64[32] = {
    6307, 7541, 12060, 16358, 22553, 27865, 0, 0, 1289, 2320, 3971, 7926,
    14153, 24291, 0, 0, 24212, 25708, 28268, 30035, 31307, 32049, 0, 0,
    8726, 12378, 19409, 26450, 30038, 32462, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_128[32] = {
    3472, 4885, 7489, 12481, 18517, 24536, 29635, 0, 886, 1731, 3271, 8469,
    15569, 22126, 28383, 0, 24313, 26062, 28385, 30107, 31217, 31898, 32345, 0,
    9165, 13282, 21150, 30286, 31894, 32571, 32712, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_256[64] = {
    5348, 7113, 11820, 15924, 22106, 26777, 30334, 31757, 0, 0, 0, 0,
    0, 0, 0, 0, 2453, 4474, 6307, 8777, 16474, 22975, 29000, 31547,
    0, 0, 0, 0, 0, 0, 0, 0, 23110, 24597, 27140, 28894,
    30167, 30927, 31392, 32094, 0, 0, 0, 0, 0, 0, 0, 0,
    9998, 17661, 25178, 28097, 31308, 32038, 32403, 32695, 0, 0, 0, 0,
    0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_512[32] = {
    5927, 7809, 10923, 14597, 19439, 24135, 28456, 31142, 32060, 0, 0, 0,
    0, 0, 0, 0, 21093, 23043, 25742, 27658, 29097, 29716, 30073, 30820,
    31956, 0, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_bin_1024[32] = {
    6698, 8334, 11961, 15762, 20186, 23862, 27434, 29326, 31082, 32050, 0, 0,
    0, 0, 0, 0, 20569, 22426, 25569, 26859, 28053, 28913, 29486, 29724,
    29807, 32570, 0, 0, 0, 0, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_base_tok[160] = {
    22497, 31198, 0, 0, 31715, 32495, 0, 0, 31606, 32337, 0, 0,
    30388, 31990, 0, 0, 27877, 31584, 0, 0, 32170, 32728, 0, 0,
    32155, 32688, 0, 0, 32219, 32702, 0, 0, 21457, 31043, 0, 0,
    31951, 32483, 0, 0, 32153, 32562, 0, 0, 31473, 32215, 0, 0,
    27558, 31151, 0, 0, 32020, 32640, 0, 0, 32097, 32575, 0, 0,
    32242, 32719, 0, 0, 19980, 30591, 0, 0, 32219, 32597, 0, 0,
    32581, 32706, 0, 0, 31803, 32287, 0, 0, 26473, 30507, 0, 0,
    32431, 32723, 0, 0, 32196, 32611, 0, 0, 31588, 32528, 0, 0,
    24647, 30463, 0, 0, 32412, 32695, 0, 0, 32468, 32720, 0, 0,
    31269, 32523, 0, 0, 28482, 31505, 0, 0, 32152, 32701, 0, 0,
    31732, 32598, 0, 0, 31767, 32712, 0, 0, 12358, 24977, 0, 0,
    31331, 32385, 0, 0, 32634, 32756, 0, 0, 30411, 32548, 0, 0,
    10923, 21845, 0, 0, 10923, 21845, 0, 0, 10923, 21845, 0, 0,
    10923, 21845, 0, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_base_tok[1640] = {
    7062, 16472, 22319, 0, 24538, 32261, 32674, 0, 13675, 28041, 31779, 0,
    8590, 20674, 27631, 0, 5685, 14675, 22013, 0, 3655, 9898, 15731, 0,
    26493, 32418, 32658, 0, 16376, 29342, 32090, 0, 10594, 22649, 28970, 0,
    8176, 17170, 24303, 0, 5605, 12694, 19139, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    23888, 31902, 32542, 0, 18612, 29687, 31987, 0, 16245, 24852, 29249, 0,
    15765, 22608, 27559, 0, 19895, 24699, 27510, 0, 28401, 32212, 32457, 0,
    15274, 27825, 30980, 0, 9364, 18128, 24332, 0, 2283, 8193, 15082, 0,
    1228, 3972, 7881, 0, 29455, 32469, 32620, 0, 17981, 28245, 31388, 0,
    10921, 20098, 26240, 0, 3743, 11829, 18657, 0, 2374, 9593, 15715, 0,
    31068, 32466, 32635, 0, 20321, 29572, 31971, 0, 10771, 20255, 27119, 0,
    2795, 10410, 17361, 0, 8192, 16384, 24576, 0, 9320, 22102, 27840, 0,
    27057, 32464, 32724, 0, 16331, 30268, 32309, 0, 10319, 23935, 29720, 0,
    6189, 16448, 24106, 0, 3589, 10884, 18808, 0, 29026, 32624, 32748, 0,
    19226, 31507, 32587, 0, 12692, 26921, 31203, 0, 7049, 19532, 27635, 0,
    7727, 15669, 23252, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 28056, 32625, 32748, 0,
    22383, 32075, 32669, 0, 15417, 27098, 31749, 0, 18127, 26493, 27190, 0,
    5461, 16384, 21845, 0, 27982, 32091, 32584, 0, 19045, 29868, 31972, 0,
    10397, 22266, 27932, 0, 5990, 13697, 21500, 0, 1792, 6912, 15104, 0,
    28198, 32501, 32718, 0, 21534, 31521, 32569, 0, 11109, 25217, 30017, 0,
    5671, 15124, 26151, 0, 4681, 14043, 18725, 0, 28688, 32580, 32741, 0,
    22576, 32079, 32661, 0, 10627, 22141, 28340, 0, 9362, 14043, 28087, 0,
    8192, 16384, 24576, 0, 7754, 16948, 22142, 0, 25670, 32330, 32691, 0,
    15663, 29225, 31994, 0, 9878, 23288, 29158, 0, 6419, 17088, 24336, 0,
    3859, 11003, 17039, 0, 27562, 32595, 32725, 0, 17575, 30588, 32399, 0,
    10819, 24838, 30309, 0, 7124, 18686, 25916, 0, 4479, 12688, 19340, 0,
    28385, 32476, 32673, 0, 15306, 29005, 31938, 0, 8937, 21615, 28322, 0,
    5982, 15603, 22786, 0, 3620, 10267, 16136, 0, 27280, 32464, 32667, 0,
    15607, 29160, 32004, 0, 9091, 22135, 28740, 0, 6232, 16632, 24020, 0,
    4047, 11377, 17672, 0, 29220, 32630, 32718, 0, 19650, 31220, 32462, 0,
    13050, 26312, 30827, 0, 9228, 20870, 27468, 0, 6146, 15149, 21971, 0,
    30169, 32481, 32623, 0, 17212, 29311, 31554, 0, 9911, 21311, 26882, 0,
    4487, 13314, 20372, 0, 2570, 7772, 12889, 0, 30924, 32613, 32708, 0,
    19490, 30206, 32107, 0, 11232, 23998, 29276, 0, 6769, 17955, 25035, 0,
    4398, 12623, 19214, 0, 30609, 32627, 32722, 0, 19370, 30582, 32287, 0,
    10457, 23619, 29409, 0, 6443, 17637, 24834, 0, 4645, 13236, 20106, 0,
    8626, 20271, 26216, 0, 26707, 32406, 32711, 0, 16999, 30329, 32286, 0,
    11445, 25123, 30286, 0, 6411, 18828, 25601, 0, 6801, 12458, 20248, 0,
    29918, 32682, 32748, 0, 20649, 31739, 32618, 0, 12879, 27773, 31581, 0,
    7896, 21751, 28244, 0, 5260, 14870, 23698, 0, 29252, 32593, 32731, 0,
    17072, 30460, 32294, 0, 10653, 24143, 29365, 0, 6536, 17490, 23983, 0,
    4929, 13170, 20085, 0, 28137, 32518, 32715, 0, 18171, 30784, 32407, 0,
    11437, 25436, 30459, 0, 7252, 18534, 26176, 0, 4126, 13353, 20978, 0,
    31162, 32726, 32748, 0, 23017, 32222, 32701, 0, 15629, 29233, 32046, 0,
    9387, 22621, 29480, 0, 6922, 17616, 25010, 0, 28838, 32265, 32614, 0,
    19701, 30206, 31920, 0, 11214, 22410, 27933, 0, 5320, 14177, 23034, 0,
    5049, 12881, 17827, 0, 27484, 32471, 32734, 0, 21076, 31526, 32561, 0,
    12707, 26303, 31211, 0, 8169, 21722, 28219, 0, 6045, 19406, 27042, 0,
    27753, 32572, 32745, 0, 20832, 31878, 32653, 0, 13250, 27356, 31674, 0,
    7718, 21508, 29858, 0, 7209, 18350, 25559, 0, 7876, 16901, 21741, 0,
    24001, 31898, 32625, 0, 14529, 27959, 31451, 0, 8273, 20818, 27258, 0,
    5278, 14673, 21510, 0, 2983, 8843, 14039, 0, 28016, 32574, 32732, 0,
    17471, 30306, 32301, 0, 10224, 24063, 29728, 0, 6602, 17954, 25052, 0,
    4002, 11585, 17759, 0, 30190, 32634, 32739, 0, 17497, 30282, 32270, 0,
    10229, 23729, 29538, 0, 6344, 17211, 24440, 0, 3849, 11189, 17108, 0,
    28570, 32583, 32726, 0, 17521, 30161, 32238, 0, 10153, 23565, 29378, 0,
    6455, 17341, 24443, 0, 3907, 11042, 17024, 0, 30689, 32715, 32748, 0,
    21546, 31840, 32610, 0, 13547, 27581, 31459, 0, 8912, 21757, 28309, 0,
    5548, 15080, 22046, 0, 30783, 32540, 32685, 0, 17540, 29528, 31668, 0,
    10160, 21468, 26783, 0, 4724, 13393, 20054, 0, 2702, 8174, 13102, 0,
    31648, 32686, 32742, 0, 20954, 31094, 32337, 0, 12420, 25698, 30179, 0,
    7304, 19320, 26248, 0, 4366, 12261, 18864, 0, 31581, 32723, 32748, 0,
    21373, 31586, 32525, 0, 12744, 26625, 30885, 0, 7431, 20322, 26950, 0,
    4692, 13323, 20111, 0, 7833, 18369, 24095, 0, 26650, 32273, 32702, 0,
    16371, 29961, 32191, 0, 11055, 24082, 29629, 0, 6892, 18644, 25400, 0,
    5006, 13057, 19240, 0, 29834, 32666, 32748, 0, 19577, 31335, 32570, 0,
    12253, 26509, 31122, 0, 7991, 20772, 27711, 0, 5677, 15910, 23059, 0,
    30109, 32532, 32720, 0, 16747, 30166, 32252, 0, 10134, 23542, 29184, 0,
    5791, 16176, 23556, 0, 4362, 10414, 17284, 0, 29492, 32626, 32748, 0,
    19894, 31402, 32525, 0, 12942, 27071, 30869, 0, 8346, 21216, 27405, 0,
    6572, 17087, 23859, 0, 32035, 32735, 32748, 0, 22957, 31838, 32618, 0,
    14724, 28572, 31772, 0, 10364, 23999, 29553, 0, 7004, 18433, 25655, 0,
    27528, 32277, 32681, 0, 16959, 31171, 32096, 0, 10486, 23593, 27962, 0,
    8192, 16384, 23211, 0, 8937, 17873, 20852, 0, 27715, 32002, 32615, 0,
    15073, 29491, 31676, 0, 11264, 24576, 28672, 0, 2341, 18725, 23406, 0,
    7282, 18204, 25486, 0, 28547, 32213, 32657, 0, 20788, 29773, 32239, 0,
    6780, 21469, 30508, 0, 5958, 14895, 23831, 0, 16384, 21845, 27307, 0,
    5992, 14304, 19765, 0, 22612, 31238, 32456, 0, 13456, 27162, 31087, 0,
    8001, 20062, 26504, 0, 5168, 14105, 20764, 0, 2632, 7771, 12385, 0,
    27034, 32344, 32709, 0, 15850, 29415, 31997, 0, 9494, 22776, 28841, 0,
    6151, 16830, 23969, 0, 3461, 10039, 15722, 0, 30134, 32569, 32731, 0,
    15638, 29422, 31945, 0, 9150, 21865, 28218, 0, 5647, 15719, 22676, 0,
    3402, 9772, 15477, 0, 28530, 32586, 32735, 0, 17139, 30298, 32292, 0,
    10200, 24039, 29685, 0, 6419, 17674, 24786, 0, 3544, 10225, 15824, 0,
    31333, 32726, 32748, 0, 20618, 31487, 32544, 0, 12901, 27217, 31232, 0,
    8624, 21734, 28171, 0, 5104, 14191, 20748, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 11206, 21090, 26561, 0,
    28759, 32279, 32671, 0, 14171, 27952, 31569, 0, 9743, 22907, 29141, 0,
    6871, 17886, 24868, 0, 4960, 13152, 19315, 0, 31077, 32661, 32748, 0,
    19400, 31195, 32515, 0, 12752, 26858, 31040, 0, 8370, 22098, 28591, 0,
    5457, 15373, 22298, 0, 31697, 32706, 32748, 0, 17860, 30657, 32333, 0,
    12510, 24812, 29261, 0, 6180, 19124, 24722, 0, 5041, 13548, 17959, 0,
    31552, 32716, 32748, 0, 21908, 31769, 32623, 0, 14470, 28201, 31565, 0,
    9493, 22982, 28608, 0, 6858, 17240, 24137, 0, 32543, 32752, 32756, 0,
    24286, 32097, 32666, 0, 15958, 29217, 32024, 0, 10207, 24234, 29958, 0,
    6929, 18305, 25652, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 4137, 10847, 15682, 0, 17824, 27001, 30058, 0,
    10204, 22796, 28291, 0, 6076, 15935, 22125, 0, 3852, 10937, 16816, 0,
    2252, 6324, 10131, 0, 25840, 32016, 32662, 0, 15109, 28268, 31531, 0,
    9385, 22231, 28340, 0, 6082, 16672, 23479, 0, 3318, 9427, 14681, 0,
    30594, 32574, 32718, 0, 16836, 29552, 31859, 0, 9556, 22542, 28356, 0,
    6305, 16725, 23540, 0, 3376, 9895, 15184, 0, 29383, 32617, 32745, 0,
    18891, 30809, 32401, 0, 11688, 25942, 30687, 0, 7468, 19469, 26651, 0,
    3909, 11358, 17012, 0, 31564, 32736, 32748, 0, 20906, 31611, 32600, 0,
    13191, 27621, 31537, 0, 8768, 22029, 28676, 0, 5079, 14109, 20906, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
    8192, 16384, 24576, 0, 8192, 16384, 24576, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_br_tok[672] = {
    18315, 24289, 27551, 0, 16854, 24068, 27835, 0, 10140, 17927, 23173, 0,
    6722, 12982, 18267, 0, 4661, 9826, 14706, 0, 3832, 8165, 12294, 0,
    2795, 6098, 9245, 0, 17145, 23326, 26672, 0, 20733, 27680, 30308, 0,
    16032, 24461, 28546, 0, 11653, 20093, 25081, 0, 9290, 16429, 22086, 0,
    7796, 14598, 19982, 0, 6502, 12378, 17441, 0, 21681, 27732, 30320, 0,
    22389, 29044, 31261, 0, 19027, 26731, 30087, 0, 14739, 23755, 28624, 0,
    11358, 20778, 25511, 0, 10995, 18073, 24190, 0, 9162, 14990, 20617, 0,
    21425, 27952, 30388, 0, 18062, 25838, 29034, 0, 11956, 19881, 24808, 0,
    7718, 15000, 20980, 0, 5702, 11254, 16143, 0, 4898, 9088, 16864, 0,
    3679, 6776, 11907, 0, 23294, 30160, 31663, 0, 24397, 29896, 31836, 0,
    19245, 27128, 30593, 0, 13202, 19825, 26404, 0, 11578, 19297, 23957, 0,
    8073, 13297, 21370, 0, 5461, 10923, 19745, 0, 27367, 30521, 31934, 0,
    24904, 30671, 31940, 0, 23075, 28460, 31299, 0, 14400, 23658, 30417, 0,
    13885, 23882, 28325, 0, 14746, 22938, 27853, 0, 5461, 16384, 27307, 0,
    18274, 24813, 27890, 0, 15537, 23149, 27003, 0, 9449, 16740, 21827, 0,
    6700, 12498, 17261, 0, 4988, 9866, 14198, 0, 4236, 8147, 11902, 0,
    2867, 5860, 8654, 0, 17124, 23171, 26101, 0, 20396, 27477, 30148, 0,
    16573, 24629, 28492, 0, 12749, 20846, 25674, 0, 10233, 17878, 22818, 0,
    8525, 15332, 20363, 0, 6283, 11632, 16255, 0, 20466, 26511, 29286, 0,
    23059, 29174, 31191, 0, 19481, 27263, 30241, 0, 15458, 23631, 28137, 0,
    12416, 20608, 25693, 0, 10261, 18011, 23261, 0, 8016, 14655, 19666, 0,
    17616, 24586, 28112, 0, 15809, 23299, 27155, 0, 10767, 18890, 23793, 0,
    7727, 14255, 18865, 0, 6129, 11926, 16882, 0, 4482, 9704, 14861, 0,
    3277, 7452, 11522, 0, 22956, 28551, 30730, 0, 22724, 28937, 30961, 0,
    18467, 26324, 29580, 0, 13234, 20713, 25649, 0, 11181, 17592, 22481, 0,
    8291, 18358, 24576, 0, 7568, 11881, 14984, 0, 24948, 29001, 31147, 0,
    25674, 30619, 32151, 0, 20841, 26793, 29603, 0, 14669, 24356, 28666, 0,
    11334, 23593, 28219, 0, 8922, 14762, 22873, 0, 8301, 13544, 20535, 0,
    17113, 23733, 27081, 0, 14139, 21406, 25452, 0, 8552, 15002, 19776, 0,
    5871, 11120, 15378, 0, 4455, 8616, 12253, 0, 3469, 6910, 10386, 0,
    2255, 4553, 6782, 0, 18224, 24376, 27053, 0, 19290, 26710, 29614, 0,
    14936, 22991, 27184, 0, 11238, 18951, 23762, 0, 8786, 15617, 20588, 0,
    7317, 13228, 18003, 0, 5101, 9512, 13493, 0, 22639, 28222, 30210, 0,
    23216, 29331, 31307, 0, 19075, 26762, 29895, 0, 15014, 23113, 27457, 0,
    11938, 19857, 24752, 0, 9942, 17280, 22282, 0, 7167, 13144, 17752, 0,
    15820, 22738, 26488, 0, 13530, 20885, 25216, 0, 8395, 15530, 20452, 0,
    6574, 12321, 16380, 0, 5353, 10419, 14568, 0, 4613, 8446, 12381, 0,
    3440, 7158, 9903, 0, 24247, 29051, 31224, 0, 22118, 28058, 30369, 0,
    16498, 24768, 28389, 0, 12920, 21175, 26137, 0, 10730, 18619, 25352, 0,
    10187, 16279, 22791, 0, 9310, 14631, 22127, 0, 24970, 30558, 32057, 0,
    24801, 29942, 31698, 0, 22432, 28453, 30855, 0, 19054, 25680, 29580, 0,
    14392, 23036, 28109, 0, 12495, 20947, 26650, 0, 12442, 20326, 26214, 0,
    12162, 18785, 22648, 0, 12749, 19697, 23806, 0, 8580, 15297, 20346, 0,
    6169, 11749, 16543, 0, 4836, 9391, 13448, 0, 3821, 7711, 11613, 0,
    2228, 4601, 7070, 0, 16319, 24725, 28280, 0, 15698, 23277, 27168, 0,
    12726, 20368, 25047, 0, 9912, 17015, 21976, 0, 7888, 14220, 19179, 0,
    6777, 12284, 17018, 0, 4492, 8590, 12252, 0, 23249, 28904, 30947, 0,
    21050, 27908, 30512, 0, 17440, 25340, 28949, 0, 14059, 22018, 26541, 0,
    11288, 18903, 23898, 0, 9411, 16342, 21428, 0, 6278, 11588, 15944, 0,
    13981, 20067, 23226, 0, 16922, 23580, 26783, 0, 11005, 19039, 24487, 0,
    7389, 14218, 19798, 0, 5598, 11505, 17206, 0, 6090, 11213, 15659, 0,
    3820, 7371, 10119, 0, 21082, 26925, 29675, 0, 21262, 28627, 31128, 0,
    18392, 26454, 30437, 0, 14870, 22910, 27096, 0, 12620, 19484, 24908, 0,
    9290, 16553, 22802, 0, 6668, 14288, 20004, 0, 27704, 31055, 31949, 0,
    24709, 29978, 31788, 0, 21668, 29264, 31657, 0, 18295, 26968, 30074, 0,
    16399, 24422, 29313, 0, 14347, 23026, 28104, 0, 12370, 19806, 24477, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_eob_hi_bit[180] = {
    20177, 0, 20789, 0, 20262, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 21416, 0, 20855, 0, 23410, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    20238, 0, 21057, 0, 19159, 0, 22337, 0, 20159, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 20125, 0, 20559, 0, 21707, 0,
    22296, 0, 17333, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
    19941, 0, 20527, 0, 21470, 0, 22487, 0, 19558, 0, 22354, 0,
    20331, 0, 16384, 0, 16384, 0, 22752, 0, 25006, 0, 22075, 0,
    21576, 0, 17740, 0, 21690, 0, 19211, 0, 16384, 0, 16384, 0,
    21442, 0, 22358, 0, 18503, 0, 20291, 0, 19945, 0, 21294, 0,
    21178, 0, 19400, 0, 10556, 0, 24648, 0, 24949, 0, 20708, 0,
    23905, 0, 20501, 0, 9558, 0, 9423, 0, 30365, 0, 19253, 0,
    26064, 0, 22098, 0, 19613, 0, 20525, 0, 17595, 0, 16618, 0,
    20497, 0, 18989, 0, 15513, 0, 16384, 0, 16384, 0, 16384, 0,
    16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0, 16384, 0,
};

static const stbv_u16 stb_av1_cdf_coef3_dc_sign[12] = {
    16000, 0, 13056, 0, 18816, 0, 15232, 0, 12928, 0, 17280, 0,
};

/* dav1d pal_y[7][3][2]: 7 size contexts x 3 palette contexts, each a 2-entry
 * CDF1.  The leaf syntax only uses pal_ctx 0 for now, so the stride into the
 * table is sz_ctx * 6 entries. */
static const stbv_u16 stb_av1_cdf_pal_y[42] = {
    31676, 0,  3419, 0,  1261, 0,
    31912, 0,  2859, 0,   980, 0,
    31823, 0,  3400, 0,   781, 0,
    32030, 0,  3561, 0,   904, 0,
    32309, 0,  7337, 0,  1462, 0,
    32265, 0,  4015, 0,  1521, 0,
    32450, 0,  7946, 0,   129, 0,
};

/* dav1d pal_uv[2][2]: 2 palette contexts, each a 2-entry CDF1. */
static const stbv_u16 stb_av1_cdf_pal_uv[4] = {
    32461, 0, 21488, 0,
};

/* dav1d pal_sz[2][7][8]: 2 planes x 7 size contexts, each a CDF6 group of
 * 6 thresholds + count slot + pad (spec-form values; inverted at init). */
static const stbv_u16 stb_av1_cdf_pal_sz[112] = {
    7952, 13000, 18149, 21478, 25527, 29241, 0, 0,
    7139, 11421, 16195, 19544, 23666, 28073, 0, 0,
    7788, 12741, 17325, 20500, 24315, 28530, 0, 0,
    8271, 14064, 18246, 21564, 25071, 28533, 0, 0,
    12725, 19180, 21863, 24839, 27535, 30120, 0, 0,
    9711, 14888, 16923, 21052, 25661, 27875, 0, 0,
    14940, 20797, 21678, 24186, 27033, 28999, 0, 0,
    8713, 19979, 27128, 29609, 31331, 32272, 0, 0,
    5839, 15573, 23581, 26947, 29848, 31700, 0, 0,
    4426, 11260, 17999, 21483, 25863, 29430, 0, 0,
    3228, 9464, 14993, 18089, 22523, 27420, 0, 0,
    3768, 8886, 13091, 17852, 22495, 27207, 0, 0,
    2464, 8451, 12861, 21632, 25525, 28555, 0, 0,
    1269, 5435, 10433, 18963, 21700, 25865, 0, 0,
};

/* dav1d color_map[2][7][5][8]: 2 planes x 7 palette sizes (pal_sz-2) x
 * 5 contexts; group k has k+1 thresholds + count slot + pad. */
static const stbv_u16 stb_av1_cdf_color_map[560] = {
    28710, 0, 0, 0, 0, 0, 0, 0,
    16384, 0, 0, 0, 0, 0, 0, 0,
    10553, 0, 0, 0, 0, 0, 0, 0,
    27036, 0, 0, 0, 0, 0, 0, 0,
    31603, 0, 0, 0, 0, 0, 0, 0,
    27877, 30490, 0, 0, 0, 0, 0, 0,
    11532, 25697, 0, 0, 0, 0, 0, 0,
    6544, 30234, 0, 0, 0, 0, 0, 0,
    23018, 28072, 0, 0, 0, 0, 0, 0,
    31915, 32385, 0, 0, 0, 0, 0, 0,
    25572, 28046, 30045, 0, 0, 0, 0, 0,
    9478, 21590, 27256, 0, 0, 0, 0, 0,
    7248, 26837, 29824, 0, 0, 0, 0, 0,
    19167, 24486, 28349, 0, 0, 0, 0, 0,
    31400, 31825, 32250, 0, 0, 0, 0, 0,
    24779, 26955, 28576, 30282, 0, 0, 0, 0,
    8669, 20364, 24073, 28093, 0, 0, 0, 0,
    4255, 27565, 29377, 31067, 0, 0, 0, 0,
    19864, 23674, 26716, 29530, 0, 0, 0, 0,
    31646, 31893, 32147, 32426, 0, 0, 0, 0,
    23132, 25407, 26970, 28435, 30073, 0, 0, 0,
    7443, 17242, 20717, 24762, 27982, 0, 0, 0,
    6300, 24862, 26944, 28784, 30671, 0, 0, 0,
    18916, 22895, 25267, 27435, 29652, 0, 0, 0,
    31270, 31550, 31808, 32059, 32353, 0, 0, 0,
    23105, 25199, 26464, 27684, 28931, 30318, 0, 0,
    6950, 15447, 18952, 22681, 25567, 28563, 0, 0,
    7560, 23474, 25490, 27203, 28921, 30708, 0, 0,
    18544, 22373, 24457, 26195, 28119, 30045, 0, 0,
    31198, 31451, 31670, 31882, 32123, 32391, 0, 0,
    21689, 23883, 25163, 26352, 27506, 28827, 30195, 0,
    6892, 15385, 17840, 21606, 24287, 26753, 29204, 0,
    5651, 23182, 25042, 26518, 27982, 29392, 30900, 0,
    19349, 22578, 24418, 25994, 27524, 29031, 30448, 0,
    31028, 31270, 31504, 31705, 31927, 32153, 32392, 0,
    29089, 0, 0, 0, 0, 0, 0, 0,
    16384, 0, 0, 0, 0, 0, 0, 0,
    8713, 0, 0, 0, 0, 0, 0, 0,
    29257, 0, 0, 0, 0, 0, 0, 0,
    31610, 0, 0, 0, 0, 0, 0, 0,
    25257, 29145, 0, 0, 0, 0, 0, 0,
    12287, 27293, 0, 0, 0, 0, 0, 0,
    7033, 27960, 0, 0, 0, 0, 0, 0,
    20145, 25405, 0, 0, 0, 0, 0, 0,
    30608, 31639, 0, 0, 0, 0, 0, 0,
    24210, 27175, 29903, 0, 0, 0, 0, 0,
    9888, 22386, 27214, 0, 0, 0, 0, 0,
    5901, 26053, 29293, 0, 0, 0, 0, 0,
    18318, 22152, 28333, 0, 0, 0, 0, 0,
    30459, 31136, 31926, 0, 0, 0, 0, 0,
    22980, 25479, 27781, 29986, 0, 0, 0, 0,
    8413, 21408, 24859, 28874, 0, 0, 0, 0,
    2257, 29449, 30594, 31598, 0, 0, 0, 0,
    19189, 21202, 25915, 28620, 0, 0, 0, 0,
    31844, 32044, 32281, 32518, 0, 0, 0, 0,
    22217, 24567, 26637, 28683, 30548, 0, 0, 0,
    7307, 16406, 19636, 24632, 28424, 0, 0, 0,
    4441, 25064, 26879, 28942, 30919, 0, 0, 0,
    17210, 20528, 23319, 26750, 29582, 0, 0, 0,
    30674, 30953, 31396, 31735, 32207, 0, 0, 0,
    21239, 23168, 25044, 26962, 28705, 30506, 0, 0,
    6545, 15012, 18004, 21817, 25503, 28701, 0, 0,
    3448, 26295, 27437, 28704, 30126, 31442, 0, 0,
    15889, 18323, 21704, 24698, 26976, 29690, 0, 0,
    30988, 31204, 31479, 31734, 31983, 32325, 0, 0,
    21442, 23288, 24758, 26246, 27649, 28980, 30563, 0,
    5863, 14933, 17552, 20668, 23683, 26411, 29273, 0,
    3415, 25810, 26877, 27990, 29223, 30394, 31618, 0,
    17965, 20084, 22232, 23974, 26274, 28402, 30390, 0,
    31190, 31329, 31516, 31679, 31825, 32026, 32322, 0,
};

static const stbv_u16 stb_av1_cdf_restore_switchable[4] = {
    9413, 22581, 0, 0,
};
static const stbv_u16 stb_av1_cdf_restore_wiener[2] = {
    11570, 0,
};
static const stbv_u16 stb_av1_cdf_restore_sgrproj[2] = {
    16855, 0,
};

typedef struct stbv_av1_cdf {
    stbv_u16 y_mode[64];
    stbv_u16 uv_mode[416];
    stbv_u16 kfym[400];
    stbv_u16 partition[320];
    stbv_u16 angle_delta[64];
    stbv_u16 filter_intra[8];
    stbv_u16 use_filter_intra[44];
    stbv_u16 txsz[48];
    stbv_u16 txpart[42];
    stbv_u16 skip[6];
    stbv_u16 cfl_sign[8];
    stbv_u16 cfl_alpha[96];
    stbv_u16 txtp_intra1[208];
    stbv_u16 txtp_intra2[312];
    stbv_u16 pal_y[42];
    stbv_u16 pal_uv[4];
    stbv_u16 pal_sz[112];
    stbv_u16 color_map[560];
    stbv_u16 coef[3050];
    stbv_u16 seg_id[24]; /* 3 contexts x 8 symbols each */
    stbv_u16 seg_pred[8];
    stbv_u16 restore_switchable[4];
    stbv_u16 restore_wiener[2];
    stbv_u16 restore_sgrproj[2];
    stbv_u16 intrabc[2];
    stbv_u16 delta_q[8];   /* 4 symbols x 2 ctx */
    stbv_u16 delta_lf[16]; /* 4 symbols x 4 ctx */
    /* MV CDFs for IBC (intra block copy) */
    stbv_u16 mv_joint[4];     /* 4 symbols (ZERO/H/V/HV) */
    stbv_u16 mv_sign[2];      /* 2 symbols (Y component) */
    stbv_u16 mv_classes[11];  /* 11 symbols (Y component) */
    stbv_u16 mv_class0[2];    /* 2 symbols (Y component) */
    stbv_u16 mv_classN[10][2]; /* 10 entries, each 2 symbols (Y component) */
    stbv_u16 mv_sign_x[2];      /* 2 symbols (X component) */
    stbv_u16 mv_classes_x[11];  /* 11 symbols (X component) */
    stbv_u16 mv_class0_x[2];    /* 2 symbols (X component) */
    stbv_u16 mv_classN_x[10][2]; /* 10 entries, each 2 symbols (X component) */
    /* Inter txtp CDFs */
    stbv_u16 txtp_inter1[2][16]; /* 2 entries, 15 symbols each + count */
    stbv_u16 txtp_inter2[12];    /* 11 symbols + count */
    stbv_u16 txtp_inter3[8];     /* 4 bools, each 2 (val + count) */
} stbv_av1_cdf;

#define STBV_AV1_COEF_SKIP_OFF       0
#define STBV_AV1_COEF_EOB16_OFF      130
#define STBV_AV1_COEF_EOB32_OFF      162
#define STBV_AV1_COEF_EOB64_OFF      194
#define STBV_AV1_COEF_EOB128_OFF     226
#define STBV_AV1_COEF_EOB256_OFF     258
#define STBV_AV1_COEF_EOB512_OFF     322
#define STBV_AV1_COEF_EOB1024_OFF    354
#define STBV_AV1_COEF_EOBBASE_OFF    386
#define STBV_AV1_COEF_BASE_OFF       546
#define STBV_AV1_COEF_BR_OFF         2186
#define STBV_AV1_COEF_EOBHI_OFF      2858
#define STBV_AV1_COEF_DCSIGN_OFF     3038

/* dav1d stores its CDFs in inverted form: each entry is 32768 minus the
 * spec probability, i.e. the tail probability P(symbol > i).  The static
 * tables above are spec-form, so every group is inverted at init time.
 * Groups hold n symbol values followed by an adaptation count slot (0) and
 * padding; the count and pad entries must not be inverted. */
static void stbv_av1_cdf_inv(stbv_u16 *d, const stbv_u16 *s,
                             int groups, int stride, int n)
{
    int g, i;
    for (g = 0; g < groups; g++)
        for (i = 0; i < n; i++)
            d[g * stride + i] = (stbv_u16)(32768U - s[g * stride + i]);
}

static void stbv_av1_cdf_copy_coef(stbv_u16 *d, unsigned q)
{
    const stbv_u16 *s;
#define STBV_SRC(field) (q == 0 ? stb_av1_cdf_coef0_##field : \
                         q == 1 ? stb_av1_cdf_coef1_##field : \
                         q == 2 ? stb_av1_cdf_coef2_##field : \
                                  stb_av1_cdf_coef3_##field)
#define STBV_CPY(field,off,count,groups,stride,n) do { \
        s = STBV_SRC(field); \
        memcpy(d + (off), s, (count) * sizeof(stbv_u16)); \
        stbv_av1_cdf_inv(d + (off), s, (groups), (stride), (n)); \
    } while (0)
    STBV_CPY(skip,0,130,65,2,1);
    STBV_CPY(eob_bin_16,130,32,4,8,4);
    STBV_CPY(eob_bin_32,162,32,4,8,5);
    STBV_CPY(eob_bin_64,194,32,4,8,6);
    STBV_CPY(eob_bin_128,226,32,4,8,7);
    STBV_CPY(eob_bin_256,258,64,4,16,8);
    STBV_CPY(eob_bin_512,322,32,2,16,9);
    STBV_CPY(eob_bin_1024,354,32,2,16,10);
    STBV_CPY(eob_base_tok,386,160,40,4,2);
    STBV_CPY(base_tok,546,1640,410,4,3);
    STBV_CPY(br_tok,2186,672,168,4,3);
    STBV_CPY(eob_hi_bit,2858,180,90,2,1);
    STBV_CPY(dc_sign,3038,12,6,2,1);
#undef STBV_CPY
#undef STBV_SRC
}

static void stbv_av1_cdf_init(stbv_av1_cdf *c, unsigned qcat)
{
    static const int part_n[5] = { 7, 9, 9, 9, 3 };
    int g, i;

    memcpy(c->y_mode, stb_av1_cdf_y_mode, sizeof(c->y_mode));
    stbv_av1_cdf_inv(c->y_mode, stb_av1_cdf_y_mode, 4, 16, 12);
    memcpy(c->uv_mode, stb_av1_cdf_uv_mode, sizeof(c->uv_mode));
    /* uv_mode: 13 non-CFL groups (12 probs) then 13 CFL groups (13 probs). */
    stbv_av1_cdf_inv(c->uv_mode, stb_av1_cdf_uv_mode, 13, 16, 12);
    stbv_av1_cdf_inv(c->uv_mode + 208, stb_av1_cdf_uv_mode + 208, 13, 16, 13);
    memcpy(c->kfym, stb_av1_cdf_kfym, sizeof(c->kfym));
    stbv_av1_cdf_inv(c->kfym, stb_av1_cdf_kfym, 25, 16, 12);
    memcpy(c->partition, stb_av1_cdf_partition, sizeof(c->partition));
    for (g = 0; g < 20; g++)
        for (i = 0; i < part_n[g / 4]; i++)
            c->partition[g * 16 + i] =
                (stbv_u16)(32768U - stb_av1_cdf_partition[g * 16 + i]);
    memcpy(c->angle_delta, stb_av1_cdf_angle_delta, sizeof(c->angle_delta));
    stbv_av1_cdf_inv(c->angle_delta, stb_av1_cdf_angle_delta, 8, 8, 6);
    memcpy(c->filter_intra, stb_av1_cdf_filter_intra, sizeof(c->filter_intra));
    stbv_av1_cdf_inv(c->filter_intra, stb_av1_cdf_filter_intra, 1, 8, 4);
    memcpy(c->use_filter_intra, stb_av1_cdf_use_filter_intra, sizeof(c->use_filter_intra));
    stbv_av1_cdf_inv(c->use_filter_intra, stb_av1_cdf_use_filter_intra, 22, 2, 1);
    memcpy(c->txsz, stb_av1_cdf_txsz, sizeof(c->txsz));
    /* txsz groups: max=1 rows are CDF1 (3 groups), the rest CDF2 (9 groups);
     * invert only the symbol entries so the count slots stay 0. */
    stbv_av1_cdf_inv(c->txsz, stb_av1_cdf_txsz, 3, 4, 1);
    stbv_av1_cdf_inv(c->txsz + 12, stb_av1_cdf_txsz + 12, 9, 4, 2);
    memcpy(c->txpart, stb_av1_cdf_txpart, sizeof(c->txpart));
    stbv_av1_cdf_inv(c->txpart, stb_av1_cdf_txpart, 21, 2, 1);
    memcpy(c->skip, stb_av1_cdf_skip, sizeof(c->skip));
    stbv_av1_cdf_inv(c->skip, stb_av1_cdf_skip, 3, 2, 1);
    memcpy(c->cfl_sign, stb_av1_cdf_cfl_sign, sizeof(c->cfl_sign));
    stbv_av1_cdf_inv(c->cfl_sign, stb_av1_cdf_cfl_sign, 1, 8, 7);
    memcpy(c->cfl_alpha, stb_av1_cdf_cfl_alpha, sizeof(c->cfl_alpha));
    stbv_av1_cdf_inv(c->cfl_alpha, stb_av1_cdf_cfl_alpha, 6, 16, 15);
    memcpy(c->txtp_intra1, stb_av1_cdf_txtp_intra1, sizeof(c->txtp_intra1));
    stbv_av1_cdf_inv(c->txtp_intra1, stb_av1_cdf_txtp_intra1, 26, 8, 6);
    memcpy(c->txtp_intra2, stb_av1_cdf_txtp_intra2, sizeof(c->txtp_intra2));
    stbv_av1_cdf_inv(c->txtp_intra2, stb_av1_cdf_txtp_intra2, 39, 8, 4);
    memcpy(c->pal_y, stb_av1_cdf_pal_y, sizeof(c->pal_y));
    stbv_av1_cdf_inv(c->pal_y, stb_av1_cdf_pal_y, 21, 2, 1);
    memcpy(c->pal_uv, stb_av1_cdf_pal_uv, sizeof(c->pal_uv));
    stbv_av1_cdf_inv(c->pal_uv, stb_av1_cdf_pal_uv, 2, 2, 1);
    memcpy(c->pal_sz, stb_av1_cdf_pal_sz, sizeof(c->pal_sz));
    stbv_av1_cdf_inv(c->pal_sz, stb_av1_cdf_pal_sz, 14, 8, 6);
    memcpy(c->color_map, stb_av1_cdf_color_map, sizeof(c->color_map));
    for (g = 0; g < 70; g++) {
        int n = 1 + ((g / 5) % 7);
        for (i = 0; i < n; i++)
            c->color_map[g * 8 + i] =
                (stbv_u16)(32768U - stb_av1_cdf_color_map[g * 8 + i]);
    }
    memcpy(c->seg_id, stb_av1_cdf_seg_id, sizeof(c->seg_id));
    stbv_av1_cdf_inv(c->seg_id, stb_av1_cdf_seg_id, 3, 8, 7);
    memcpy(c->seg_pred, stb_av1_cdf_seg_pred, sizeof(c->seg_pred));
    stbv_av1_cdf_inv(c->seg_pred, stb_av1_cdf_seg_pred, 4, 2, 1);
    memcpy(c->restore_switchable, stb_av1_cdf_restore_switchable, sizeof(c->restore_switchable));
    stbv_av1_cdf_inv(c->restore_switchable, stb_av1_cdf_restore_switchable, 1, 4, 2);
    memcpy(c->restore_wiener, stb_av1_cdf_restore_wiener, sizeof(c->restore_wiener));
    stbv_av1_cdf_inv(c->restore_wiener, stb_av1_cdf_restore_wiener, 1, 2, 1);
    memcpy(c->restore_sgrproj, stb_av1_cdf_restore_sgrproj, sizeof(c->restore_sgrproj));
    stbv_av1_cdf_inv(c->restore_sgrproj, stb_av1_cdf_restore_sgrproj, 1, 2, 1);
    /* intrabc: 2-symbol CDF, default = { 30531, 32768 } */
    c->intrabc[0] = 32768U - 30531;
    c->intrabc[1] = 0;
    /* delta_q: 4-symbol CDF, default = { 28160, 32120, 32677, 32768 } */
    c->delta_q[0] = 32768U - 28160;
    c->delta_q[1] = 32768U - 32120;
    c->delta_q[2] = 32768U - 32677;
    c->delta_q[3] = 0;
    /* delta_lf: same default as delta_q, 4 contexts x 4 symbols */
    for (i = 0; i < 4; i++) {
        c->delta_lf[i*4+0] = 32768U - 28160;
        c->delta_lf[i*4+1] = 32768U - 32120;
        c->delta_lf[i*4+2] = 32768U - 32677;
        c->delta_lf[i*4+3] = 0;
    }
    /* MV CDFs: joint CDF3(4096,11264,19328), sign CDF1(16384),
     * classes CDF10(28672,30976,31858,32320,32551,32656,32740,32757,32762,32767),
     * class0 CDF1(27648),
     * classN[0..9] = CDF1(17408..30720). All inverted (32768-val). */
    {
        /* Raw args to CDF macros; stored = 32768 - arg */
        static const stbv_u16 mv_joint_def[3] = { 4096, 11264, 19328 };
        static const stbv_u16 mv_classes_def[10] = {
            28672, 30976, 31858, 32320, 32551, 32656, 32740, 32757, 32762, 32767
        };
        static const stbv_u16 mv_classN_def[10] = {
            17408, 17920, 18944, 20480, 22528, 24576, 28672, 29952, 29952, 30720
        };
        for (i = 0; i < 3; i++)
            c->mv_joint[i] = 32768U - mv_joint_def[i];
        c->mv_joint[3] = 0;
        c->mv_sign[0] = 32768U - 16384;
        c->mv_sign[1] = 0;
        for (i = 0; i < 10; i++)
            c->mv_classes[i] = 32768U - mv_classes_def[i];
        c->mv_classes[10] = 0;
        c->mv_class0[0] = 32768U - 27648;
        c->mv_class0[1] = 0;
        for (i = 0; i < 10; i++) {
            c->mv_classN[i][0] = 32768U - mv_classN_def[i];
            c->mv_classN[i][1] = 0;
        }
        c->mv_sign_x[0] = 32768U - 16384;
        c->mv_sign_x[1] = 0;
        for (i = 0; i < 10; i++)
            c->mv_classes_x[i] = 32768U - mv_classes_def[i];
        c->mv_classes_x[10] = 0;
        c->mv_class0_x[0] = 32768U - 27648;
        c->mv_class0_x[1] = 0;
        for (i = 0; i < 10; i++) {
            c->mv_classN_x[i][0] = 32768U - mv_classN_def[i];
            c->mv_classN_x[i][1] = 0;
        }
    }
    /* TX partition CDF: 7 categories x 3 contexts (each bool CDF = 2 entries) */
    {
        static const stbv_u16 txpart_def[7][3] = {
            { 28581, 23846, 20847 },
            { 24315, 18196, 12133 },
            { 18791, 10887, 11005 },
            { 27179, 20004, 11281 },
            { 26549, 19308, 14224 },
            { 28015, 21546, 14400 },
            { 28165, 22401, 16088 }
        };
        int r, cl;
        for (r = 0; r < 7; r++)
            for (cl = 0; cl < 3; cl++) {
                c->txpart[(r * 3 + cl) * 2] = 32768U - txpart_def[r][cl];
                c->txpart[(r * 3 + cl) * 2 + 1] = 0;
            }
    }
    /* Inter txtp CDFs: dav1d default values inverted. */
    {
        /* txtp_inter1: 2 entries x 15 symbols + count each = 32 entries */
        static const stbv_u16 inter1_def[2][15] = {
            { 4458, 5560, 7695, 9709, 13330, 14789, 17537, 20266,
              21504, 22848, 23934, 25474, 27727, 28915, 30631 },
            { 1645, 2573, 4778, 5711, 7807, 8622, 10522, 15357,
              17674, 20408, 22517, 25010, 27116, 28856, 30749 }
        };
        /* txtp_inter2: 11 symbols + count = 12 entries */
        static const stbv_u16 inter2_def[11] = {
            770, 2421, 5225, 12907, 15819, 18927,
            21561, 24089, 26595, 28526, 30529
        };
        /* txtp_inter3: 4 bool CDFs (val + count each) */
        static const stbv_u16 inter3_def[4] = {
            16384, 4167, 1998, 748
        };
        int j;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 15; j++)
                c->txtp_inter1[i][j] = 32768U - inter1_def[i][j];
            c->txtp_inter1[i][15] = 0; /* count */
        }
        for (j = 0; j < 11; j++)
            c->txtp_inter2[j] = 32768U - inter2_def[j];
        c->txtp_inter2[11] = 0; /* count */
        for (i = 0; i < 4; i++) {
            c->txtp_inter3[i * 2] = 32768U - inter3_def[i];
            c->txtp_inter3[i * 2 + 1] = 0; /* count */
        }
    }
    if (qcat > 3) qcat = 3;
    switch (qcat) {
    case 0: stbv_av1_cdf_copy_coef(c->coef, 0); break;
    case 1: stbv_av1_cdf_copy_coef(c->coef, 1); break;
    case 2: stbv_av1_cdf_copy_coef(c->coef, 2); break;
    default: stbv_av1_cdf_copy_coef(c->coef, 3); break;
    }
}

#endif

/* ===== stb_av1_partition.h ===== */
/*
 * Minimal AV1 partition/block geometry helpers derived from dav1d 1.5.4.
 *
 * Copyright (c) 2018, VideoLAN and dav1d authors
 * Copyright (c) 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * This file is intended for use with stb_avif's scalar AV1 decoder.
 */
#ifndef STB_AV1_PARTITION_H
#define STB_AV1_PARTITION_H

#ifndef STBV_U8_DEFINED
#error "stb_av1_partition.h requires stbv_u8 from stb_avif.h"
#endif

/* AV1 block partition types.  The numeric order is significant: it is the
 * order used by the partition CDFs in dav1d. */
enum stbv_av1_partition_type {
    STBV_AV1_PARTITION_NONE = 0,
    STBV_AV1_PARTITION_H,
    STBV_AV1_PARTITION_V,
    STBV_AV1_PARTITION_SPLIT,
    STBV_AV1_PARTITION_T_TOP_SPLIT,
    STBV_AV1_PARTITION_T_BOTTOM_SPLIT,
    STBV_AV1_PARTITION_T_LEFT_SPLIT,
    STBV_AV1_PARTITION_T_RIGHT_SPLIT,
    STBV_AV1_PARTITION_H4,
    STBV_AV1_PARTITION_V4,
    STBV_AV1_N_PARTITIONS
};

enum stbv_av1_block_level {
    STBV_AV1_BL_128X128 = 0,
    STBV_AV1_BL_64X64,
    STBV_AV1_BL_32X32,
    STBV_AV1_BL_16X16,
    STBV_AV1_BL_8X8,
    STBV_AV1_N_BL_LEVELS
};

enum stbv_av1_block_size {
    STBV_AV1_BS_128x128 = 0,
    STBV_AV1_BS_128x64,
    STBV_AV1_BS_64x128,
    STBV_AV1_BS_64x64,
    STBV_AV1_BS_64x32,
    STBV_AV1_BS_64x16,
    STBV_AV1_BS_32x64,
    STBV_AV1_BS_32x32,
    STBV_AV1_BS_32x16,
    STBV_AV1_BS_32x8,
    STBV_AV1_BS_16x64,
    STBV_AV1_BS_16x32,
    STBV_AV1_BS_16x16,
    STBV_AV1_BS_16x8,
    STBV_AV1_BS_16x4,
    STBV_AV1_BS_8x32,
    STBV_AV1_BS_8x16,
    STBV_AV1_BS_8x8,
    STBV_AV1_BS_8x4,
    STBV_AV1_BS_4x16,
    STBV_AV1_BS_4x8,
    STBV_AV1_BS_4x4,
    STBV_AV1_N_BS_SIZES
};

/* Number of 4x4 units in width/height and log2(width/height) for each block
 * size.  This is the useful subset of dav1d_block_dimensions[]. */
static const stbv_u8 stbv_av1_block_dimensions[STBV_AV1_N_BS_SIZES][4] = {
    {32,32,5,5}, {32,16,5,4}, {16,32,4,5}, {16,16,4,4},
    {16,8,4,3}, {16,4,4,2}, {8,16,3,4}, {8,8,3,3},
    {8,4,3,2}, {8,2,3,1}, {4,16,2,4}, {4,8,2,3},
    {4,4,2,2}, {4,2,2,1}, {4,1,2,0}, {2,8,1,3},
    {2,4,1,2}, {2,2,1,1}, {2,1,1,0}, {1,4,0,2},
    {1,2,0,1}, {1,1,0,0}
};

/* For each block level and partition, the first and (where applicable)
 * second child block sizes.  0xff means that the partition is not legal at
 * that level. */
static const stbv_u8 stbv_av1_block_sizes[STBV_AV1_N_BL_LEVELS]
                                      [STBV_AV1_N_PARTITIONS][2] = {
    /* 128x128 */
    {
        {STBV_AV1_BS_128x128, 0xff},
        {STBV_AV1_BS_128x64,  0xff},
        {STBV_AV1_BS_64x128,  0xff},
        {0xff, 0xff},
        {STBV_AV1_BS_64x64, STBV_AV1_BS_128x64},
        {STBV_AV1_BS_128x64, STBV_AV1_BS_64x64},
        {STBV_AV1_BS_64x64, STBV_AV1_BS_64x128},
        {STBV_AV1_BS_64x128, STBV_AV1_BS_64x64},
        {0xff, 0xff}, {0xff, 0xff}
    },
    /* 64x64 */
    {
        {STBV_AV1_BS_64x64, 0xff},
        {STBV_AV1_BS_64x32, 0xff},
        {STBV_AV1_BS_32x64, 0xff},
        {0xff, 0xff},
        {STBV_AV1_BS_32x32, STBV_AV1_BS_64x32},
        {STBV_AV1_BS_64x32, STBV_AV1_BS_32x32},
        {STBV_AV1_BS_32x32, STBV_AV1_BS_32x64},
        {STBV_AV1_BS_32x64, STBV_AV1_BS_32x32},
        {STBV_AV1_BS_64x16, 0xff},
        {STBV_AV1_BS_16x64, 0xff}
    },
    /* 32x32 */
    {
        {STBV_AV1_BS_32x32, 0xff},
        {STBV_AV1_BS_32x16, 0xff},
        {STBV_AV1_BS_16x32, 0xff},
        {0xff, 0xff},
        {STBV_AV1_BS_16x16, STBV_AV1_BS_32x16},
        {STBV_AV1_BS_32x16, STBV_AV1_BS_16x16},
        {STBV_AV1_BS_16x16, STBV_AV1_BS_16x32},
        {STBV_AV1_BS_16x32, STBV_AV1_BS_16x16},
        {STBV_AV1_BS_32x8, 0xff},
        {STBV_AV1_BS_8x32, 0xff}
    },
    /* 16x16 */
    {
        {STBV_AV1_BS_16x16, 0xff},
        {STBV_AV1_BS_16x8, 0xff},
        {STBV_AV1_BS_8x16, 0xff},
        {0xff, 0xff},
        {STBV_AV1_BS_8x8, STBV_AV1_BS_16x8},
        {STBV_AV1_BS_16x8, STBV_AV1_BS_8x8},
        {STBV_AV1_BS_8x8, STBV_AV1_BS_8x16},
        {STBV_AV1_BS_8x16, STBV_AV1_BS_8x8},
        {STBV_AV1_BS_16x4, 0xff},
        {STBV_AV1_BS_4x16, 0xff}
    },
    /* 8x8 */
    {
        {STBV_AV1_BS_8x8, 0xff},
        {STBV_AV1_BS_8x4, 0xff},
        {STBV_AV1_BS_4x8, 0xff},
        {STBV_AV1_BS_4x4, 0xff},
        {0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff}, {0xff, 0xff},
        {0xff, 0xff}, {0xff, 0xff}
    }
};

static const stbv_u8 stbv_av1_partition_type_count[STBV_AV1_N_BL_LEVELS] = {
    7, 9, 9, 9, 3
};

/* Convert a partition-tree level to the corresponding CDF slice. */
static stbv_u16 *stbv_av1_partition_cdf(stbv_u16 *cdf,
                                        int level, int ctx)
{
    return cdf + level * 64 + ctx * 16;
}

/* This is the same context derivation used by dav1d's get_partition_ctx().
 * The caller stores the partition-depth mask in an 8x8-unit grid. */
static int stbv_av1_partition_ctx(const stbv_u8 *above,
                                  const stbv_u8 *left,
                                  int stride,
                                  int xb8, int yb8, int level)
{
    int a = (above[xb8] >> (4 - level)) & 1;
    int l = (left[yb8 * stride] >> (4 - level)) & 1;
    return a + (l << 1);
}

/* Set the partition-depth mask for the 8x8 cells covered by a block. */
static void stbv_av1_partition_mark(stbv_u8 *grid, int stride,
                                    int x8, int y8, int w8, int h8,
                                    int level)
{
    int y, x;
    stbv_u8 bit = (stbv_u8)(1 << (4 - level));
    for (y = 0; y < h8; y++) {
        for (x = 0; x < w8; x++)
            grid[(y8 + y) * stride + x8 + x] = bit;
    }
}

#endif /* STB_AV1_PARTITION_H */

/* ===== stb_av1_partition_decode.h ===== */
/*
 * AV1 partition-tree decoder derived from dav1d 1.5.4 src/decode.c.
 * Copyright (c) 2018-2024, VideoLAN and dav1d authors; BSD-2-Clause.
 */
#ifndef STB_AV1_PARTITION_DECODE_H
#define STB_AV1_PARTITION_DECODE_H

#ifndef STB_AV1_PARTITION_H
#error "include stb_av1_partition.h first"
#endif
#ifndef STB_AV1_MSAC_H
#error "include stb_av1_msac.h first"
#endif
#ifndef STB_AV1_CDF_H
#error "include stb_av1_cdf.h first"
#endif

static const stbv_u8 stbv_av1_al_part_ctx[2][STBV_AV1_N_BL_LEVELS]
                                          [STBV_AV1_N_PARTITIONS] = {
    {
        { 0x00, 0x00, 0x10, 0xff, 0x00, 0x10, 0x10, 0x10, 0xff, 0xff },
        { 0x10, 0x10, 0x18, 0xff, 0x10, 0x18, 0x18, 0x18, 0x10, 0x1c },
        { 0x18, 0x18, 0x1c, 0xff, 0x18, 0x1c, 0x1c, 0x1c, 0x18, 0x1e },
        { 0x1c, 0x1c, 0x1e, 0xff, 0x1c, 0x1e, 0x1e, 0x1e, 0x1c, 0x1f },
        { 0x1e, 0x1e, 0x1f, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
    }, {
        { 0x00, 0x10, 0x00, 0xff, 0x10, 0x10, 0x00, 0x10, 0xff, 0xff },
        { 0x10, 0x18, 0x10, 0xff, 0x18, 0x18, 0x10, 0x18, 0x1c, 0x10 },
        { 0x18, 0x1c, 0x18, 0xff, 0x1c, 0x1c, 0x18, 0x1c, 0x1e, 0x18 },
        { 0x1c, 0x1e, 0x1c, 0xff, 0x1e, 0x1e, 0x1c, 0x1e, 0x1f, 0x1c },
        { 0x1e, 0x1f, 0x1e, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
    }
};

/* case_set_upto16(): dav1d writes min(pow2(hsz),16) cells. */
static void stbv_av1_partition_set_context(stbv_u8 *above, stbv_u8 *left,
                                            int above_n, int left_n,
                                            int bx8, int by8, int hsz,
                                            int bl, int bp)
{
    int n, i;
    stbv_u8 av = stbv_av1_al_part_ctx[0][bl][bp];
    stbv_u8 lv = stbv_av1_al_part_ctx[1][bl][bp];

    n = 1;
    while (n < hsz && n < 16)
        n <<= 1;

    for (i = 0; i < n; i++) {
        if (bx8 + i >= 0 && bx8 + i < above_n)
            above[bx8 + i] = av;
        if (by8 + i >= 0 && by8 + i < left_n)
            left[by8 + i] = lv;
    }
}

static unsigned int stbv_av1_gather_top_partition_prob(const stbv_u16 *pc,
                                                        int bl)
{
    unsigned int out;
    out = (unsigned int)pc[STBV_AV1_PARTITION_V - 1] - pc[STBV_AV1_PARTITION_T_TOP_SPLIT];
    out += pc[STBV_AV1_PARTITION_T_LEFT_SPLIT - 1];
    if (bl != STBV_AV1_BL_128X128)
        out += (unsigned int)pc[STBV_AV1_PARTITION_V4 - 1] -
               pc[STBV_AV1_PARTITION_T_RIGHT_SPLIT];
    return out;
}

static unsigned int stbv_av1_gather_left_partition_prob(const stbv_u16 *pc,
                                                          int bl)
{
    unsigned int out;
    out = (unsigned int)pc[STBV_AV1_PARTITION_H - 1] - pc[STBV_AV1_PARTITION_H];
    out += (unsigned int)pc[STBV_AV1_PARTITION_SPLIT - 1] -
           pc[STBV_AV1_PARTITION_T_LEFT_SPLIT];
    if (bl != STBV_AV1_BL_128X128)
        out += (unsigned int)pc[STBV_AV1_PARTITION_H4 - 1] -
               pc[STBV_AV1_PARTITION_H4];
    return out;
}

typedef struct stbv_av1_partition_decoder stbv_av1_partition_decoder;

typedef int (*stbv_av1_partition_leaf_fn)(
    stbv_av1_partition_decoder *d, int bl, int bs, int bp,
    int bx, int by, void *opaque);

struct stbv_av1_partition_decoder {
    struct stb_av1_msac *msac;
    stbv_av1_cdf *cdf;
    int frame_w4;
    int frame_h4;
    stbv_u8 *above;
    stbv_u8 *left;
    int above_n;
    int left_n;
    int ctx_x4;
    int ctx_y4;
    stbv_av1_partition_leaf_fn leaf;
    void *opaque;
    int leaf_count;
};

static int stbv_av1_partition_emit(stbv_av1_partition_decoder *d,
                                    int bl, int bs, int bp,
                                    int bx, int by)
{
    int r;
    d->leaf_count++;
    r = d->leaf(d, bl, bs, bp, bx, by, d->opaque);
    return r;
}

static int stbv_av1_partition_decode_sb(stbv_av1_partition_decoder *d,
                                        int bl, int bx, int by)
{
    int hsz;
    int have_h_split, have_v_split;
    int bx8, by8, ctx;
    stbv_u16 *pc;
    int bp = 0;
    int bs;

    hsz = 16 >> bl;
    have_h_split = d->frame_w4 > bx + hsz;
    have_v_split = d->frame_h4 > by + hsz;

    if (!have_h_split && !have_v_split) {
        if (bl >= STBV_AV1_BL_8X8)
            return stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4,
                                           STBV_AV1_PARTITION_SPLIT, bx, by);
        return stbv_av1_partition_decode_sb(d, bl + 1, bx, by);
    }

    bx8 = (bx - d->ctx_x4) >> 1;
    by8 = (by - d->ctx_y4) >> 1;
    if (bx8 < 0 || bx8 >= d->above_n || by8 < 0 || by8 >= d->left_n)
        return -1;
    ctx = ((d->above[bx8] >> (4 - bl)) & 1) |
          (((d->left[by8] >> (4 - bl)) & 1) << 1);
    pc = stbv_av1_partition_cdf(d->cdf->partition, bl, ctx);

    if (have_h_split && have_v_split) {
        bp = (int)stb_av1_msac_symbol(d->msac, pc,
                                       stbv_av1_partition_type_count[bl]);
        if (bp < 0 || bp >= STBV_AV1_N_PARTITIONS)
            return -1;

        if (bp == STBV_AV1_PARTITION_SPLIT) {
            if (bl == STBV_AV1_BL_8X8) {
                int r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx + 1, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx, by + 1);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp,
                                            bx + 1, by + 1);
                if (r) return r;
            } else {
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx, by)) return -1;
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx + hsz, by)) return -1;
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx, by + hsz)) return -1;
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx + hsz, by + hsz)) return -1;
            }
        } else {
            bs = stbv_av1_block_sizes[bl][bp][0];
            if (bs == 0xff)
                return -1;
            if (bp == STBV_AV1_PARTITION_NONE) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_H) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by + hsz);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_V) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, bs, bp, bx + hsz, by);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_T_TOP_SPLIT) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, bs, bp, bx + hsz, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, stbv_av1_block_sizes[bl][bp][1], bp,
                                            bx, by + hsz);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_T_BOTTOM_SPLIT) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, stbv_av1_block_sizes[bl][bp][1], bp,
                                            bx, by + hsz);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, stbv_av1_block_sizes[bl][bp][1], bp,
                                            bx + hsz, by + hsz);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_T_LEFT_SPLIT) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by + hsz);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, stbv_av1_block_sizes[bl][bp][1], bp,
                                            bx + hsz, by);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_T_RIGHT_SPLIT) {
                int r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, stbv_av1_block_sizes[bl][bp][1], bp,
                                            bx + hsz, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, stbv_av1_block_sizes[bl][bp][1], bp,
                                            bx + hsz, by + hsz);
                if (r) return r;
            } else if (bp == STBV_AV1_PARTITION_H4) {
                int i, r;
                int step = hsz >> 1;
                for (i = 0; i < 4; i++) {
                    if (by + i * step >= d->frame_h4) break;
                    r = stbv_av1_partition_emit(d, bl, bs, bp, bx, by + i * step);
                    if (r) return r;
                }
            } else if (bp == STBV_AV1_PARTITION_V4) {
                int i, r;
                int step = hsz >> 1;
                for (i = 0; i < 4; i++) {
                    if (bx + i * step >= d->frame_w4) break;
                    r = stbv_av1_partition_emit(d, bl, bs, bp, bx + i * step, by);
                    if (r) return r;
                }
            } else {
                return -1;
            }
        }

        /* dav1d decode.c:2414: once per level over the parent span
         * (case_set_upto16(ulog2(hsz))), skipping un-split interior SPLTs. */
        if (bp != STBV_AV1_PARTITION_SPLIT || bl == STBV_AV1_BL_8X8)
            stbv_av1_partition_set_context(d->above, d->left, d->above_n,
                                           d->left_n, bx8, by8,
                                           hsz, bl, bp);
    } else if (have_h_split) {
        unsigned int is_split;
        is_split = stb_av1_msac_bool(d->msac,
                                     stbv_av1_gather_top_partition_prob(pc, bl));
        bp = is_split ? (int)STBV_AV1_PARTITION_SPLIT : (int)STBV_AV1_PARTITION_H;
        if (is_split) {
            if (bl >= STBV_AV1_BL_8X8) {
                int r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx + 1, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx, by + 1);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx + 1, by + 1);
                if (r) return r;
            } else {
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx, by)) return -1;
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx + hsz, by)) return -1;
            }
        } else {
            int r;
            bs = stbv_av1_block_sizes[bl][STBV_AV1_PARTITION_H][0];
            if (bs == 0xff)
                return -1;
            r = stbv_av1_partition_emit(d, bl, bs, STBV_AV1_PARTITION_H, bx, by);
            if (r) return r;
        }
        /* dav1d sets bp before reaching the shared write below. */
        if (bp != STBV_AV1_PARTITION_SPLIT || bl == STBV_AV1_BL_8X8)
            stbv_av1_partition_set_context(d->above, d->left, d->above_n,
                                           d->left_n, bx8, by8,
                                           hsz, bl, bp);
    } else {
        unsigned int is_split;
        is_split = stb_av1_msac_bool(d->msac,
                                     stbv_av1_gather_left_partition_prob(pc, bl));
        bp = is_split ? (int)STBV_AV1_PARTITION_SPLIT : (int)STBV_AV1_PARTITION_V;
        if (is_split) {
            if (bl >= STBV_AV1_BL_8X8) {
                int r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx + 1, by);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx, by + 1);
                if (r) return r;
                r = stbv_av1_partition_emit(d, bl, STBV_AV1_BS_4x4, bp, bx + 1, by + 1);
                if (r) return r;
            } else {
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx, by)) return -1;
                if (stbv_av1_partition_decode_sb(d, bl + 1, bx, by + hsz)) return -1;
            }
        } else {
            int r;
            bs = stbv_av1_block_sizes[bl][STBV_AV1_PARTITION_V][0];
            if (bs == 0xff)
                return -1;
            r = stbv_av1_partition_emit(d, bl, bs, STBV_AV1_PARTITION_V, bx, by);
            if (r) return r;
        }
        if (bp != STBV_AV1_PARTITION_SPLIT || bl == STBV_AV1_BL_8X8)
            stbv_av1_partition_set_context(d->above, d->left, d->above_n,
                                           d->left_n, bx8, by8,
                                           hsz, bl, bp);
    }

    return 0;
}

static void stbv_av1_partition_decoder_init(stbv_av1_partition_decoder *d,
                                             struct stb_av1_msac *msac,
                                             stbv_av1_cdf *cdf,
                                             int frame_w4, int frame_h4,
                                             stbv_u8 *above, int above_n,
                                             stbv_u8 *left, int left_n,
                                             stbv_av1_partition_leaf_fn leaf,
                                             void *opaque)
{
    d->msac = msac;
    d->cdf = cdf;
    d->frame_w4 = frame_w4;
    d->frame_h4 = frame_h4;
    d->above = above;
    d->left = left;
    d->above_n = above_n;
    d->left_n = left_n;
    d->ctx_x4 = 0;
    d->ctx_y4 = 0;
    d->leaf = leaf;
    d->opaque = opaque;
}

#endif /* STB_AV1_PARTITION_DECODE_H */

/* ===== stb_av1_seqhdr.h ===== */
/*
 * stb_av1_seqhdr.h - AV1 sequence header parser
 *
 * Portions are adapted from dav1d 1.5.4 src/obu.c parse_seq_hdr().
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_SEQHDR_H
#define STB_AV1_SEQHDR_H

#define STB_AV1_MAX_OPERATING_POINTS 32

enum stb_av1_seq_layout {
    STB_AV1_LAYOUT_I400 = 0,
    STB_AV1_LAYOUT_I420 = 1,
    STB_AV1_LAYOUT_I422 = 2,
    STB_AV1_LAYOUT_I444 = 3
};

enum stb_av1_seq_chr {
    STB_AV1_CHR_UNKNOWN = 0,
    STB_AV1_CHR_420 = 1,
    STB_AV1_CHR_422 = 2,
    STB_AV1_CHR_444 = 3
};

struct stb_av1_seq_op {
    unsigned int idc;
    unsigned int major_level;
    unsigned int minor_level;
    unsigned int tier;
    unsigned int decoder_model_param_present;
    unsigned int display_model_param_present;
    unsigned int initial_display_delay;
    unsigned int decoder_buffer_delay;
    unsigned int encoder_buffer_delay;
    unsigned int low_delay_mode;
};

struct stb_av1_seqhdr {
    unsigned int profile;
    unsigned int still_picture;
    unsigned int reduced_still_picture_header;

    unsigned int timing_info_present;
    unsigned int num_units_in_tick;
    unsigned int time_scale;
    unsigned int equal_picture_interval;
    unsigned int num_ticks_per_picture;
    unsigned int decoder_model_info_present;
    unsigned int encoder_decoder_buffer_delay_length;
    unsigned int num_units_in_decoding_tick;
    unsigned int buffer_removal_delay_length;
    unsigned int frame_presentation_delay_length;
    unsigned int display_model_info_present;
    unsigned int num_operating_points;
    struct stb_av1_seq_op operating_points[STB_AV1_MAX_OPERATING_POINTS];

    unsigned int width_n_bits;
    unsigned int height_n_bits;
    unsigned int max_width;
    unsigned int max_height;

    unsigned int frame_id_numbers_present;
    unsigned int delta_frame_id_n_bits;
    unsigned int frame_id_n_bits;

    unsigned int sb128;
    unsigned int filter_intra;
    unsigned int intra_edge_filter;
    unsigned int inter_intra;
    unsigned int masked_compound;
    unsigned int warped_motion;
    unsigned int dual_filter;
    unsigned int order_hint;
    unsigned int jnt_comp;
    unsigned int ref_frame_mvs;
    unsigned int screen_content_tools;
    unsigned int force_integer_mv;
    unsigned int order_hint_n_bits;

    unsigned int super_res;
    unsigned int cdef;
    unsigned int restoration;

    unsigned int hbd;
    unsigned int monochrome;
    unsigned int color_description_present;
    unsigned int pri;
    unsigned int trc;
    unsigned int mtrx;
    unsigned int color_range;
    unsigned int layout;
    unsigned int ss_hor;
    unsigned int ss_ver;
    unsigned int chr;
    unsigned int separate_uv_delta_q;
    unsigned int film_grain_present;
};

static unsigned int stb_av1_seq_vlc(struct stb_av1_getbits *gb)
{
    unsigned int leading = 0;
    unsigned int v;
    while (!stb_av1_get_bit(gb)) {
        leading++;
        if (leading >= 32) {
            gb->error = 1;
            return 0;
        }
    }
    if (!leading)
        return 0;
    v = stb_av1_get_bits(gb, (int)leading);
    return ((1U << leading) - 1U) + v;
}

/* Returns 0 on success, -1 on malformed input. */
static int stb_av1_parse_seqhdr(struct stb_av1_seqhdr *h,
                                struct stb_av1_getbits *gb)
{
    unsigned int i;

    /* dav1d starts with memset(). Do it explicitly for C89 portability. */
    unsigned char *p = (unsigned char *)h;
    size_t n = sizeof(*h);
    while (n--)
        *p++ = 0;

    h->profile = stb_av1_get_bits(gb, 3);
    if (h->profile > 2)
        return -1;

    h->still_picture = stb_av1_get_bit(gb);
    h->reduced_still_picture_header = stb_av1_get_bit(gb);
    if (h->reduced_still_picture_header && !h->still_picture)
        return -1;

    if (h->reduced_still_picture_header) {
        h->num_operating_points = 1;
        h->operating_points[0].major_level = stb_av1_get_bits(gb, 3);
        h->operating_points[0].minor_level = stb_av1_get_bits(gb, 2);
        h->operating_points[0].initial_display_delay = 10;
    } else {
        h->timing_info_present = stb_av1_get_bit(gb);
        if (h->timing_info_present) {
            h->num_units_in_tick = stb_av1_get_bits(gb, 32);
            h->time_scale = stb_av1_get_bits(gb, 32);
            h->equal_picture_interval = stb_av1_get_bit(gb);
            if (h->equal_picture_interval) {
                h->num_ticks_per_picture = stb_av1_seq_vlc(gb) + 1;
                if (gb->error)
                    return -1;
            }
            h->decoder_model_info_present = stb_av1_get_bit(gb);
            if (h->decoder_model_info_present) {
                h->encoder_decoder_buffer_delay_length =
                    stb_av1_get_bits(gb, 5) + 1;
                h->num_units_in_decoding_tick = stb_av1_get_bits(gb, 32);
                h->buffer_removal_delay_length = stb_av1_get_bits(gb, 5) + 1;
                h->frame_presentation_delay_length = stb_av1_get_bits(gb, 5) + 1;
            }
        }

        h->display_model_info_present = stb_av1_get_bit(gb);
        h->num_operating_points = stb_av1_get_bits(gb, 5) + 1;
        if (h->num_operating_points > STB_AV1_MAX_OPERATING_POINTS)
            return -1;

        for (i = 0; i < h->num_operating_points; i++) {
            struct stb_av1_seq_op *op = &h->operating_points[i];
            op->idc = stb_av1_get_bits(gb, 12);
            if (op->idc && (!(op->idc & 0xffU) || !(op->idc & 0xf00U)))
                return -1;
            op->major_level = 2 + stb_av1_get_bits(gb, 3);
            op->minor_level = stb_av1_get_bits(gb, 2);
            if (op->major_level > 3)
                op->tier = stb_av1_get_bit(gb);
            if (h->decoder_model_info_present) {
                op->decoder_model_param_present = stb_av1_get_bit(gb);
                if (op->decoder_model_param_present) {
                    op->decoder_buffer_delay = stb_av1_get_bits(
                        gb, (int)h->encoder_decoder_buffer_delay_length);
                    op->encoder_buffer_delay = stb_av1_get_bits(
                        gb, (int)h->encoder_decoder_buffer_delay_length);
                    op->low_delay_mode = stb_av1_get_bit(gb);
                }
            }
            if (h->display_model_info_present)
                op->display_model_param_present = stb_av1_get_bit(gb);
            op->initial_display_delay = op->display_model_param_present ?
                stb_av1_get_bits(gb, 4) + 1 : 10;
        }
    }

    h->width_n_bits = stb_av1_get_bits(gb, 4) + 1;
    h->height_n_bits = stb_av1_get_bits(gb, 4) + 1;
    h->max_width = stb_av1_get_bits(gb, (int)h->width_n_bits) + 1;
    h->max_height = stb_av1_get_bits(gb, (int)h->height_n_bits) + 1;

    if (!h->reduced_still_picture_header) {
        h->frame_id_numbers_present = stb_av1_get_bit(gb);
        if (h->frame_id_numbers_present) {
            h->delta_frame_id_n_bits = stb_av1_get_bits(gb, 4) + 2;
            h->frame_id_n_bits = stb_av1_get_bits(gb, 3) +
                                 h->delta_frame_id_n_bits + 1;
        }
    }

    h->sb128 = stb_av1_get_bit(gb);
    h->filter_intra = stb_av1_get_bit(gb);
    h->intra_edge_filter = stb_av1_get_bit(gb);

    if (h->reduced_still_picture_header) {
        h->screen_content_tools = 2; /* DAV1D_ADAPTIVE */
        h->force_integer_mv = 2;     /* DAV1D_ADAPTIVE */
    } else {
        h->inter_intra = stb_av1_get_bit(gb);
        h->masked_compound = stb_av1_get_bit(gb);
        h->warped_motion = stb_av1_get_bit(gb);
        h->dual_filter = stb_av1_get_bit(gb);
        h->order_hint = stb_av1_get_bit(gb);
        if (h->order_hint) {
            h->jnt_comp = stb_av1_get_bit(gb);
            h->ref_frame_mvs = stb_av1_get_bit(gb);
        }
        h->screen_content_tools = stb_av1_get_bit(gb) ?
            2 : stb_av1_get_bit(gb);
        h->force_integer_mv = h->screen_content_tools ?
            (stb_av1_get_bit(gb) ? 2 : stb_av1_get_bit(gb)) : 2;
        if (h->order_hint)
            h->order_hint_n_bits = stb_av1_get_bits(gb, 3) + 1;
    }

    h->super_res = stb_av1_get_bit(gb);
    h->cdef = stb_av1_get_bit(gb);
    h->restoration = stb_av1_get_bit(gb);

    h->hbd = stb_av1_get_bit(gb);
    if (h->profile == 2 && h->hbd)
        h->hbd += stb_av1_get_bit(gb);
    if (h->profile != 1)
        h->monochrome = stb_av1_get_bit(gb);

    h->color_description_present = stb_av1_get_bit(gb);
    if (h->color_description_present) {
        h->pri = stb_av1_get_bits(gb, 8);
        h->trc = stb_av1_get_bits(gb, 8);
        h->mtrx = stb_av1_get_bits(gb, 8);
    } else {
        h->pri = 2;  /* UNKNOWN */
        h->trc = 2;  /* UNKNOWN */
        h->mtrx = 2; /* UNKNOWN */
    }

    if (h->monochrome) {
        h->color_range = stb_av1_get_bit(gb);
        h->layout = STB_AV1_LAYOUT_I400;
        h->ss_hor = h->ss_ver = 1;
        h->chr = STB_AV1_CHR_UNKNOWN;
    } else if (h->pri == 1 && h->trc == 13 && h->mtrx == 0
               && (h->profile == 1
                   || (h->profile == 2 && h->hbd == 2))) {
        /* BT.709 / sRGB / identity matrix special case.
         * Only valid for profile 1 or profile 2 with hbd==2 per spec.
         * For other profiles (e.g. profile 0), fall through to the
         * normal path so we read color_range and subsampling correctly. */
        h->layout = STB_AV1_LAYOUT_I444;
        h->color_range = 1;
    } else {
        h->color_range = stb_av1_get_bit(gb);
        switch (h->profile) {
        case 0:
            h->layout = STB_AV1_LAYOUT_I420;
            h->ss_hor = h->ss_ver = 1;
            break;
        case 1:
            h->layout = STB_AV1_LAYOUT_I444;
            break;
        case 2:
            if (h->hbd == 2) {
                h->ss_hor = stb_av1_get_bit(gb);
                if (h->ss_hor)
                    h->ss_ver = stb_av1_get_bit(gb);
            } else {
                h->ss_hor = 1;
            }
            h->layout = h->ss_hor ?
                (h->ss_ver ? STB_AV1_LAYOUT_I420 : STB_AV1_LAYOUT_I422) :
                STB_AV1_LAYOUT_I444;
            break;
        default:
            return -1;
        }
        h->chr = (h->ss_hor && h->ss_ver) ?
            stb_av1_get_bits(gb, 2) : STB_AV1_CHR_UNKNOWN;
    }

    if (!h->monochrome)
        h->separate_uv_delta_q = stb_av1_get_bit(gb);

    h->film_grain_present = stb_av1_get_bit(gb);

    if (gb->error)
        return -1;
    return 0;
}

#endif /* STB_AV1_SEQHDR_H */

/* ===== stb_av1_framehdr.h ===== */
/*
 * stb_av1_framehdr.h - reduced scalar AV1 frame-header parser
 *
 * Portions adapted from dav1d 1.5.4 src/obu.c (parse_frame_hdr/read_frame_size).
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * First-stage target: still/intra frames, scalar C89 implementation.
 */
#ifndef STB_AV1_FRAMEHDR_H
#define STB_AV1_FRAMEHDR_H

#define STB_AV1_MAX_TILE_COLS 64
#define STB_AV1_MAX_TILE_ROWS 64
#define STB_AV1_MAX_SEGMENTS 8

#define STB_AV1_FRAME_KEY        0
#define STB_AV1_FRAME_INTER      1
#define STB_AV1_FRAME_INTRA_ONLY 2
#define STB_AV1_FRAME_SWITCH     3

struct stb_av1_frame_quant {
    unsigned int yac;
    int ydc_delta, udc_delta, uac_delta, vdc_delta, vac_delta;
    unsigned int qm;
    unsigned int qm_y, qm_u, qm_v;
};

struct stb_av1_seg_data {
    int delta_q;
    int delta_lf_y_v;
    int delta_lf_y_h;
    int delta_lf_u;
    int delta_lf_v;
    int ref;
    unsigned int skip;
    unsigned int globalmv;
};

struct stb_av1_frame_seg {
    unsigned int enabled;
    unsigned int update_map;
    unsigned int temporal;
    unsigned int update_data;
    unsigned int preskip;
    int last_active_segid;
    struct stb_av1_seg_data d[STB_AV1_MAX_SEGMENTS];
    unsigned int qidx[STB_AV1_MAX_SEGMENTS];
    unsigned int lossless[STB_AV1_MAX_SEGMENTS];
};

struct stb_av1_frame_lf {
    unsigned int level_y[2];
    unsigned int level_u, level_v;
    unsigned int sharpness;
    unsigned int mode_ref_delta_enabled;
    unsigned int mode_ref_delta_update;
    int ref_delta[8];
    int mode_delta[2];
};

struct stb_av1_frame_cdef {
    unsigned int damping;
    unsigned int n_bits;
    unsigned int y_strength[8];
    unsigned int uv_strength[8];
};

struct stb_av1_frame_restoration {
    unsigned int type[3];
    unsigned int unit_size[2];
};

struct stb_av1_tiling {
    unsigned int uniform;
    unsigned int cols, rows;
    unsigned int log2_cols, log2_rows;
    unsigned int min_log2_cols, max_log2_cols;
    unsigned int min_log2_rows, max_log2_rows;
    unsigned int col_start_sb[STB_AV1_MAX_TILE_COLS + 1];
    unsigned int row_start_sb[STB_AV1_MAX_TILE_ROWS + 1];
    unsigned int update;
    unsigned int n_bytes;
};

struct stb_av1_framehdr {
    unsigned int show_existing_frame;
    unsigned int existing_frame_idx;
    unsigned int frame_type;
    unsigned int show_frame;
    unsigned int showable_frame;
    unsigned int error_resilient_mode;
    unsigned int disable_cdf_update;
    unsigned int allow_screen_content_tools;
    unsigned int force_integer_mv;
    unsigned int frame_id;
    unsigned int frame_size_override;
    unsigned int frame_offset;
    unsigned int refresh_frame_flags;
    unsigned int allow_intrabc;
    unsigned int refresh_context;

    unsigned int width[2];
    unsigned int height;
    unsigned int render_width, render_height;
    unsigned int have_render_size;

    unsigned int superres_enabled;
    unsigned int superres_den;

    struct stb_av1_tiling tiling;
    struct stb_av1_frame_quant quant;
    struct stb_av1_frame_seg segmentation;
    struct stb_av1_frame_lf loopfilter;
    struct stb_av1_frame_cdef cdef;
    struct stb_av1_frame_restoration restoration;

    unsigned int delta_q_present;
    unsigned int delta_q_res_log2;
    unsigned int delta_lf_present;
    unsigned int delta_lf_res_log2;
    unsigned int delta_lf_multi;

    unsigned int all_lossless;
    unsigned int txfm_mode;
    unsigned int reduced_txtp_set;
};

static unsigned int stb_av1_imax_u(unsigned int a, unsigned int b)
{
    return a > b ? a : b;
}

static unsigned int stb_av1_imin_u(unsigned int a, unsigned int b)
{
    return a < b ? a : b;
}

static unsigned int stb_av1_tile_log2(unsigned int sz, unsigned int tgt)
{
    unsigned int k = 0;
    while ((sz << k) < tgt)
        k++;
    return k;
}

static unsigned int stb_av1_clip_u8_int(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned int)v;
}

static int stb_av1_read_frame_size(struct stb_av1_framehdr *h,
                                   const struct stb_av1_seqhdr *s,
                                   struct stb_av1_getbits *gb)
{
    if (h->frame_size_override) {
        h->width[1] = stb_av1_get_bits(gb, (int)s->width_n_bits) + 1;
        h->height = stb_av1_get_bits(gb, (int)s->height_n_bits) + 1;
    } else {
        h->width[1] = s->max_width;
        h->height = s->max_height;
    }

    h->superres_enabled = s->super_res ? stb_av1_get_bit(gb) : 0;
    if (h->superres_enabled) {
        h->superres_den = 9 + stb_av1_get_bits(gb, 3);
        h->width[0] = (h->width[1] * 8 + (h->superres_den >> 1)) /
                      h->superres_den;
        if (h->width[0] < 16)
            h->width[0] = 16 < h->width[1] ? 16 : h->width[1];
    } else {
        h->superres_den = 8;
        h->width[0] = h->width[1];
    }

    h->have_render_size = stb_av1_get_bit(gb);
    if (h->have_render_size) {
        h->render_width = stb_av1_get_bits(gb, 16) + 1;
        h->render_height = stb_av1_get_bits(gb, 16) + 1;
    } else {
        h->render_width = h->width[1];
        h->render_height = h->height;
    }
    return gb->error ? -1 : 0;
}

static int stb_av1_parse_tiling(struct stb_av1_framehdr *h,
                                const struct stb_av1_seqhdr *s,
                                struct stb_av1_getbits *gb)
{
    unsigned int sbsz_log2 = 6 + s->sb128;
    unsigned int sbsz_min1 = (64U << s->sb128) - 1U;
    unsigned int sbw = (h->width[0] + sbsz_min1) >> sbsz_log2;
    unsigned int sbh = (h->height + sbsz_min1) >> sbsz_log2;
    unsigned int max_tile_width_sb = 4096U >> sbsz_log2;
    unsigned int max_tile_area_sb = (4096U * 2304U) >> (2 * sbsz_log2);
    unsigned int min_log2_tiles;
    unsigned int tile_w, tile_h, sbx, sby;

    h->tiling.uniform = stb_av1_get_bit(gb);
    h->tiling.min_log2_cols = stb_av1_tile_log2(max_tile_width_sb, sbw);
    h->tiling.max_log2_cols = stb_av1_tile_log2(1, stb_av1_imin_u(sbw, STB_AV1_MAX_TILE_COLS));
    h->tiling.max_log2_rows = stb_av1_tile_log2(1, stb_av1_imin_u(sbh, STB_AV1_MAX_TILE_ROWS));
    min_log2_tiles = stb_av1_imax_u(
        stb_av1_tile_log2(max_tile_area_sb, sbw * sbh),
        h->tiling.min_log2_cols);

    if (h->tiling.uniform) {
        h->tiling.log2_cols = h->tiling.min_log2_cols;
        while (h->tiling.log2_cols < h->tiling.max_log2_cols &&
               stb_av1_get_bit(gb))
            h->tiling.log2_cols++;
        tile_w = 1U + ((sbw - 1U) >> h->tiling.log2_cols);
        h->tiling.cols = 0;
        for (sbx = 0; sbx < sbw; sbx += tile_w) {
            if (h->tiling.cols >= STB_AV1_MAX_TILE_COLS) return -1;
            h->tiling.col_start_sb[h->tiling.cols++] = sbx;
        }

        h->tiling.min_log2_rows = min_log2_tiles > h->tiling.log2_cols ?
            min_log2_tiles - h->tiling.log2_cols : 0;
        h->tiling.log2_rows = h->tiling.min_log2_rows;
        while (h->tiling.log2_rows < h->tiling.max_log2_rows &&
               stb_av1_get_bit(gb))
            h->tiling.log2_rows++;
        tile_h = 1U + ((sbh - 1U) >> h->tiling.log2_rows);
        h->tiling.rows = 0;
        for (sby = 0; sby < sbh; sby += tile_h) {
            if (h->tiling.rows >= STB_AV1_MAX_TILE_ROWS) return -1;
            h->tiling.row_start_sb[h->tiling.rows++] = sby;
        }
    } else {
        /* The first decoder milestone is intentionally conservative. */
        return -1;
    }

    h->tiling.col_start_sb[h->tiling.cols] = sbw;
    h->tiling.row_start_sb[h->tiling.rows] = sbh;

    if (h->tiling.log2_cols || h->tiling.log2_rows) {
        unsigned int n = h->tiling.log2_cols + h->tiling.log2_rows;
        h->tiling.update = stb_av1_get_bits(gb, (int)n);
        if (h->tiling.update >= h->tiling.cols * h->tiling.rows)
            return -1;
        h->tiling.n_bytes = stb_av1_get_bits(gb, 2) + 1;
    }
    return gb->error ? -1 : 0;
}

/*
 * Parse the frame header for the first implementation target:
 * key/intra still pictures. Inter frames are rejected deliberately.
 */
static int stb_av1_parse_framehdr(struct stb_av1_framehdr *h,
                                  const struct stb_av1_seqhdr *s,
                                  struct stb_av1_getbits *gb)
{
    unsigned int i;
    int delta_lossless;

    {
        unsigned char *p = (unsigned char *)h;
        size_t n = sizeof(*h);
        while (n--) *p++ = 0;
    }

    if (!s->reduced_still_picture_header)
        h->show_existing_frame = stb_av1_get_bit(gb);
    if (h->show_existing_frame)
        return -1; /* no reference-frame machinery in stage 1 */

    if (s->reduced_still_picture_header) {
        h->frame_type = STB_AV1_FRAME_KEY;
        h->show_frame = 1;
    } else {
        h->frame_type = stb_av1_get_bits(gb, 2);
        h->show_frame = stb_av1_get_bit(gb);
        if (h->frame_type == STB_AV1_FRAME_INTER ||
            h->frame_type == STB_AV1_FRAME_SWITCH)
            return -1;
    }

    if (h->show_frame) {
        if (s->decoder_model_info_present && !s->equal_picture_interval)
            (void)stb_av1_get_bits(gb, (int)s->frame_presentation_delay_length);
        h->showable_frame = h->frame_type != STB_AV1_FRAME_KEY;
    } else {
        h->showable_frame = stb_av1_get_bit(gb);
    }

    h->error_resilient_mode =
        (h->frame_type == STB_AV1_FRAME_KEY && h->show_frame) ||
        s->reduced_still_picture_header || stb_av1_get_bit(gb);

    h->disable_cdf_update = stb_av1_get_bit(gb);
    h->allow_screen_content_tools = s->screen_content_tools == 2 ?
        stb_av1_get_bit(gb) : s->screen_content_tools;
    if (h->allow_screen_content_tools)
        h->force_integer_mv = s->force_integer_mv == 2 ?
            stb_av1_get_bit(gb) : s->force_integer_mv;
    if (h->frame_type == STB_AV1_FRAME_KEY ||
        h->frame_type == STB_AV1_FRAME_INTRA_ONLY)
        h->force_integer_mv = 1;

    if (s->frame_id_numbers_present)
        h->frame_id = stb_av1_get_bits(gb, (int)s->frame_id_n_bits);

    if (!s->reduced_still_picture_header)
        h->frame_size_override = stb_av1_get_bit(gb);

    if (s->order_hint)
        h->frame_offset = stb_av1_get_bits(gb, (int)s->order_hint_n_bits);

    /* primary_ref_frame is NONE for key/intra frames. */

    if (h->frame_type == STB_AV1_FRAME_KEY ||
        h->frame_type == STB_AV1_FRAME_INTRA_ONLY) {
        if (h->frame_type == STB_AV1_FRAME_KEY && h->show_frame)
            h->refresh_frame_flags = 0xff;
        else
            h->refresh_frame_flags = stb_av1_get_bits(gb, 8);

        if (stb_av1_read_frame_size(h, s, gb) < 0)
            return -1;
        if (h->allow_screen_content_tools && !h->superres_enabled)
            h->allow_intrabc = stb_av1_get_bit(gb);
    }

    if (!s->reduced_still_picture_header && !h->disable_cdf_update)
        h->refresh_context = !stb_av1_get_bit(gb);

    /* Tiling params - between refresh_context and quantization (per dav1d obu.c). */
    if (stb_av1_parse_tiling(h, s, gb) < 0)
        return -1;

    /* Quantization parameters. */
    h->quant.yac = stb_av1_get_bits(gb, 8);
    if (stb_av1_get_bit(gb))
        h->quant.ydc_delta = stb_av1_get_sbits(gb, 7);

    if (!s->monochrome) {
        unsigned int diff_uv_delta = s->separate_uv_delta_q ?
            stb_av1_get_bit(gb) : 0;
        if (stb_av1_get_bit(gb))
            h->quant.udc_delta = stb_av1_get_sbits(gb, 7);
        if (stb_av1_get_bit(gb))
            h->quant.uac_delta = stb_av1_get_sbits(gb, 7);
        if (diff_uv_delta) {
            if (stb_av1_get_bit(gb))
                h->quant.vdc_delta = stb_av1_get_sbits(gb, 7);
            if (stb_av1_get_bit(gb))
                h->quant.vac_delta = stb_av1_get_sbits(gb, 7);
        } else {
            h->quant.vdc_delta = h->quant.udc_delta;
            h->quant.vac_delta = h->quant.uac_delta;
        }
    }

    h->quant.qm = stb_av1_get_bit(gb);
    if (h->quant.qm) {
        h->quant.qm_y = stb_av1_get_bits(gb, 4);
        h->quant.qm_u = stb_av1_get_bits(gb, 4);
        h->quant.qm_v = s->separate_uv_delta_q ?
            stb_av1_get_bits(gb, 4) : h->quant.qm_u;
    }

    /* Segmentation. */
    h->segmentation.enabled = stb_av1_get_bit(gb);
    h->segmentation.last_active_segid = -1;
    if (h->segmentation.enabled) {
        h->segmentation.update_map = 1;
        h->segmentation.update_data = 1;
        h->segmentation.preskip = 0;
        for (i = 0; i < STB_AV1_MAX_SEGMENTS; i++) {
            struct stb_av1_seg_data *seg = &h->segmentation.d[i];
            seg->ref = -1;
            if (stb_av1_get_bit(gb)) {
                seg->delta_q = stb_av1_get_sbits(gb, 9);
                h->segmentation.last_active_segid = (int)i;
            }
            if (stb_av1_get_bit(gb)) {
                seg->delta_lf_y_v = stb_av1_get_sbits(gb, 7);
                h->segmentation.last_active_segid = (int)i;
            }
            if (stb_av1_get_bit(gb)) {
                seg->delta_lf_y_h = stb_av1_get_sbits(gb, 7);
                h->segmentation.last_active_segid = (int)i;
            }
            if (stb_av1_get_bit(gb)) {
                seg->delta_lf_u = stb_av1_get_sbits(gb, 7);
                h->segmentation.last_active_segid = (int)i;
            }
            if (stb_av1_get_bit(gb)) {
                seg->delta_lf_v = stb_av1_get_sbits(gb, 7);
                h->segmentation.last_active_segid = (int)i;
            }
            if (stb_av1_get_bit(gb)) {
                seg->ref = (int)stb_av1_get_bits(gb, 3);
                h->segmentation.last_active_segid = (int)i;
                h->segmentation.preskip = 1;
            }
            if ((seg->skip = stb_av1_get_bit(gb))) {
                h->segmentation.last_active_segid = (int)i;
                h->segmentation.preskip = 1;
            }
            if ((seg->globalmv = stb_av1_get_bit(gb))) {
                h->segmentation.last_active_segid = (int)i;
                h->segmentation.preskip = 1;
            }
        }
    } else {
        for (i = 0; i < STB_AV1_MAX_SEGMENTS; i++)
            h->segmentation.d[i].ref = -1;
    }

    /* Delta-Q / delta-loop-filter flags. */
    if (h->quant.yac) {
        h->delta_q_present = stb_av1_get_bit(gb);
        if (h->delta_q_present) {
            h->delta_q_res_log2 = stb_av1_get_bits(gb, 2);
            if (!h->allow_intrabc) {
                h->delta_lf_present = stb_av1_get_bit(gb);
                if (h->delta_lf_present) {
                    h->delta_lf_res_log2 = stb_av1_get_bits(gb, 2);
                    h->delta_lf_multi = stb_av1_get_bit(gb);
                }
            }
        }
    }

    delta_lossless = !h->quant.ydc_delta && !h->quant.udc_delta &&
                     !h->quant.uac_delta && !h->quant.vdc_delta &&
                     !h->quant.vac_delta;
    h->all_lossless = 1;
    for (i = 0; i < STB_AV1_MAX_SEGMENTS; i++) {
        h->segmentation.qidx[i] = h->segmentation.enabled ?
            stb_av1_clip_u8_int((int)h->quant.yac + h->segmentation.d[i].delta_q) :
            h->quant.yac;
        h->segmentation.lossless[i] =
            !h->segmentation.qidx[i] && delta_lossless;
        if (!h->segmentation.lossless[i])
            h->all_lossless = 0;
    }

    /* Loop filter. */
    if (h->all_lossless || h->allow_intrabc) {
        h->loopfilter.mode_ref_delta_enabled = 1;
        h->loopfilter.mode_ref_delta_update = 1;
        h->loopfilter.ref_delta[0] = 1;
        h->loopfilter.ref_delta[1] = 0;
        h->loopfilter.ref_delta[2] = 0;
        h->loopfilter.ref_delta[3] = 0;
        h->loopfilter.ref_delta[4] = -1;
        h->loopfilter.ref_delta[5] = 0;
        h->loopfilter.ref_delta[6] = -1;
        h->loopfilter.ref_delta[7] = -1;
    } else {
        h->loopfilter.level_y[0] = stb_av1_get_bits(gb, 6);
        h->loopfilter.level_y[1] = stb_av1_get_bits(gb, 6);
        if (!s->monochrome &&
            (h->loopfilter.level_y[0] || h->loopfilter.level_y[1])) {
            h->loopfilter.level_u = stb_av1_get_bits(gb, 6);
            h->loopfilter.level_v = stb_av1_get_bits(gb, 6);
        }
        h->loopfilter.sharpness = stb_av1_get_bits(gb, 3);
        h->loopfilter.ref_delta[0] = 1;
        h->loopfilter.ref_delta[1] = 0;
        h->loopfilter.ref_delta[2] = 0;
        h->loopfilter.ref_delta[3] = 0;
        h->loopfilter.ref_delta[4] = -1;
        h->loopfilter.ref_delta[5] = 0;
        h->loopfilter.ref_delta[6] = -1;
        h->loopfilter.ref_delta[7] = -1;
        h->loopfilter.mode_ref_delta_enabled = stb_av1_get_bit(gb);
        if (h->loopfilter.mode_ref_delta_enabled) {
            h->loopfilter.mode_ref_delta_update = stb_av1_get_bit(gb);
            if (h->loopfilter.mode_ref_delta_update) {
                for (i = 0; i < 8; i++)
                    if (stb_av1_get_bit(gb))
                        h->loopfilter.ref_delta[i] = stb_av1_get_sbits(gb, 7);
                for (i = 0; i < 2; i++)
                    if (stb_av1_get_bit(gb))
                        h->loopfilter.mode_delta[i] = stb_av1_get_sbits(gb, 7);
            }
        }
    }

    /* CDEF. */
    if (!h->all_lossless && s->cdef && !h->allow_intrabc) {
        h->cdef.damping = stb_av1_get_bits(gb, 2) + 3;
        h->cdef.n_bits = stb_av1_get_bits(gb, 2);
        for (i = 0; i < (1U << h->cdef.n_bits); i++) {
            h->cdef.y_strength[i] = stb_av1_get_bits(gb, 6);
            if (!s->monochrome)
                h->cdef.uv_strength[i] = stb_av1_get_bits(gb, 6);
        }
    }

    /* Restoration. */
    if ((!h->all_lossless || h->superres_enabled) &&
        s->restoration && !h->allow_intrabc) {
        h->restoration.type[0] = stb_av1_get_bits(gb, 2);
        if (!s->monochrome) {
            h->restoration.type[1] = stb_av1_get_bits(gb, 2);
            h->restoration.type[2] = stb_av1_get_bits(gb, 2);
        }
        if (h->restoration.type[0] || h->restoration.type[1] ||
            h->restoration.type[2]) {
            h->restoration.unit_size[0] = 6 + s->sb128;
            if (stb_av1_get_bit(gb)) {
                h->restoration.unit_size[0]++;
                if (!s->sb128)
                    h->restoration.unit_size[0] += stb_av1_get_bit(gb);
            }
            h->restoration.unit_size[1] = h->restoration.unit_size[0];
            if ((h->restoration.type[1] || h->restoration.type[2]) &&
                s->ss_hor == 1 && s->ss_ver == 1)
                h->restoration.unit_size[1] -= stb_av1_get_bit(gb);
        } else {
            h->restoration.unit_size[0] = 8;
        }
    }

    if (!h->all_lossless)
        h->txfm_mode = stb_av1_get_bit(gb) ? 1U : 0U; /* SWITCHABLE/LARGEST */

    /* No inter-only syntax in stage 1. */
    h->reduced_txtp_set = stb_av1_get_bit(gb);

    if (gb->error)
        return -1;
    return 0;
}

#endif /* STB_AV1_FRAMEHDR_H */

/* ===== stb_av1_tx.h ===== */
/*
 * Minimal AV1 transform-size/type decoding derived from dav1d 1.5.4.
 *
 * This file is intended for the scalar C89-oriented stb_avif decoder.
 * The transform syntax here deliberately covers the intra still-picture
 * path first; inter-only transform selection is not included.
 *
 * Copyright © 2018-2021, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */
#ifndef STB_AV1_TX_H
#define STB_AV1_TX_H


#define STBV_AV1_TX_4X4   0
#define STBV_AV1_TX_8X8   1
#define STBV_AV1_TX_16X16 2
#define STBV_AV1_TX_32X32 3
#define STBV_AV1_TX_64X64 4
#define STBV_AV1_TX_4X8   5
#define STBV_AV1_TX_8X4   6
#define STBV_AV1_TX_8X16  7
#define STBV_AV1_TX_16X8  8
#define STBV_AV1_TX_16X32 9
#define STBV_AV1_TX_32X16 10
#define STBV_AV1_TX_32X64 11
#define STBV_AV1_TX_64X32 12
#define STBV_AV1_TX_4X16  13
#define STBV_AV1_TX_16X4  14
#define STBV_AV1_TX_8X32  15
#define STBV_AV1_TX_32X8  16
#define STBV_AV1_TX_16X64 17
#define STBV_AV1_TX_64X16 18
#define STBV_AV1_N_TX_SIZES 19

#define STBV_AV1_TX_DCT_DCT               0
#define STBV_AV1_TX_ADST_DCT              1
#define STBV_AV1_TX_DCT_ADST              2
#define STBV_AV1_TX_ADST_ADST             3
#define STBV_AV1_TX_FLIPADST_DCT          4
#define STBV_AV1_TX_DCT_FLIPADST          5
#define STBV_AV1_TX_FLIPADST_FLIPADST     6
#define STBV_AV1_TX_ADST_FLIPADST         7
#define STBV_AV1_TX_FLIPADST_ADST         8
#define STBV_AV1_TX_IDTX                  9
#define STBV_AV1_TX_V_DCT                 10
#define STBV_AV1_TX_H_DCT                 11
#define STBV_AV1_TX_V_ADST                12
#define STBV_AV1_TX_H_ADST                13
#define STBV_AV1_TX_V_FLIPADST            14
#define STBV_AV1_TX_H_FLIPADST            15
#define STBV_AV1_TX_WHT_WHT               16

#define STBV_AV1_TX_CLASS_2D              0
#define STBV_AV1_TX_CLASS_H               1
#define STBV_AV1_TX_CLASS_V               2

static const unsigned char stbv_av1_tx_set_intra2[5] = {
    STBV_AV1_TX_IDTX, STBV_AV1_TX_DCT_DCT, STBV_AV1_TX_ADST_ADST,
    STBV_AV1_TX_ADST_DCT, STBV_AV1_TX_DCT_ADST
};

static const unsigned char stbv_av1_tx_set_intra1[7] = {
    STBV_AV1_TX_IDTX, STBV_AV1_TX_DCT_DCT, STBV_AV1_TX_V_DCT,
    STBV_AV1_TX_H_DCT, STBV_AV1_TX_ADST_ADST, STBV_AV1_TX_ADST_DCT,
    STBV_AV1_TX_DCT_ADST
};

/* Inter tx type mapping tables from dav1d tables.c tx_types_per_set[].
 * Tables have n+1 entries; entry n is the fallback when all n CDF symbols
 * are exhausted (msac_symbol returns n). dav1d always uses FLIPADST_ADST. */
static const unsigned char stbv_av1_tx_set_inter1[16] = {
    STBV_AV1_TX_IDTX, STBV_AV1_TX_V_DCT, STBV_AV1_TX_H_DCT,
    STBV_AV1_TX_V_ADST, STBV_AV1_TX_H_ADST, STBV_AV1_TX_V_FLIPADST,
    STBV_AV1_TX_H_FLIPADST, STBV_AV1_TX_DCT_DCT, STBV_AV1_TX_ADST_DCT,
    STBV_AV1_TX_DCT_ADST, STBV_AV1_TX_FLIPADST_DCT,
    STBV_AV1_TX_DCT_FLIPADST, STBV_AV1_TX_ADST_ADST,
    STBV_AV1_TX_FLIPADST_FLIPADST, STBV_AV1_TX_ADST_FLIPADST,
    STBV_AV1_TX_FLIPADST_ADST
};
static const unsigned char stbv_av1_tx_set_inter2[12] = {
    STBV_AV1_TX_IDTX, STBV_AV1_TX_V_DCT, STBV_AV1_TX_H_DCT,
    STBV_AV1_TX_DCT_DCT, STBV_AV1_TX_ADST_DCT, STBV_AV1_TX_DCT_ADST,
    STBV_AV1_TX_FLIPADST_DCT, STBV_AV1_TX_DCT_FLIPADST,
    STBV_AV1_TX_ADST_ADST, STBV_AV1_TX_FLIPADST_FLIPADST,
    STBV_AV1_TX_ADST_FLIPADST, STBV_AV1_TX_FLIPADST_ADST
};
static const unsigned char stbv_av1_tx_set_inter3[2] = {
    STBV_AV1_TX_IDTX, STBV_AV1_TX_DCT_DCT
};

/* Transform dimensions use 4x4 units. */
typedef struct stbv_av1_tx_dim {
    unsigned char w;
    unsigned char h;
    unsigned char lw;
    unsigned char lh;
    unsigned char min;
    unsigned char max;
    unsigned char sub;
    unsigned char ctx;
} stbv_av1_tx_dim;

static const stbv_av1_tx_dim stbv_av1_tx_dims[STBV_AV1_N_TX_SIZES] = {
    /* dav1d_txfm_dimensions[], in dav1d enum order. */
    { 1,  1, 0, 0, 0, 0, STBV_AV1_TX_4X4,   0 },
    { 2,  2, 1, 1, 1, 1, STBV_AV1_TX_4X4,   1 },
    { 4,  4, 2, 2, 2, 2, STBV_AV1_TX_8X8,   2 },
    { 8,  8, 3, 3, 3, 3, STBV_AV1_TX_16X16, 3 },
    {16, 16, 4, 4, 4, 4, STBV_AV1_TX_32X32, 4 },
    { 1,  2, 0, 1, 0, 1, STBV_AV1_TX_4X4,   1 },
    { 2,  1, 1, 0, 0, 1, STBV_AV1_TX_4X4,   1 },
    { 2,  4, 1, 2, 1, 2, STBV_AV1_TX_8X8,   2 },
    { 4,  2, 2, 1, 1, 2, STBV_AV1_TX_8X8,   2 },
    { 4,  8, 2, 3, 2, 3, STBV_AV1_TX_16X16, 3 },
    { 8,  4, 3, 2, 2, 3, STBV_AV1_TX_16X16, 3 },
    { 8, 16, 3, 4, 3, 4, STBV_AV1_TX_32X32, 4 },
    {16,  8, 4, 3, 3, 4, STBV_AV1_TX_32X32, 4 },
    { 1,  4, 0, 2, 0, 2, STBV_AV1_TX_4X8,   1 },
    { 4,  1, 2, 0, 0, 2, STBV_AV1_TX_8X4,   1 },
    { 2,  8, 1, 3, 1, 3, STBV_AV1_TX_8X16,  2 },
    { 8,  2, 3, 1, 1, 3, STBV_AV1_TX_16X8,  2 },
    { 4, 16, 2, 4, 2, 4, STBV_AV1_TX_16X32, 3 },
    {16,  4, 4, 2, 2, 4, STBV_AV1_TX_32X16, 3 }
};

/* dav1d_max_txfm_size_for_bs[bs][plane]: y, 420, 422, 444. */
static const stbv_u8 stbv_av1_max_tx_for_bs[STBV_AV1_N_BS_SIZES][4] = {
    {STBV_AV1_TX_64X64, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32},
    {STBV_AV1_TX_64X64, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32},
    {STBV_AV1_TX_64X64, STBV_AV1_TX_32X32, STBV_AV1_TX_4X4,   STBV_AV1_TX_32X32},
    {STBV_AV1_TX_64X64, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32},
    {STBV_AV1_TX_64X32, STBV_AV1_TX_32X16, STBV_AV1_TX_32X32, STBV_AV1_TX_32X32},
    {STBV_AV1_TX_64X16, STBV_AV1_TX_32X8,  STBV_AV1_TX_32X16, STBV_AV1_TX_32X16},
    {STBV_AV1_TX_32X64, STBV_AV1_TX_16X32, STBV_AV1_TX_4X4,   STBV_AV1_TX_32X32},
    {STBV_AV1_TX_32X32, STBV_AV1_TX_16X16, STBV_AV1_TX_16X32, STBV_AV1_TX_32X32},
    {STBV_AV1_TX_32X16, STBV_AV1_TX_16X8,  STBV_AV1_TX_16X16, STBV_AV1_TX_32X16},
    {STBV_AV1_TX_32X8,  STBV_AV1_TX_16X4,  STBV_AV1_TX_16X8,  STBV_AV1_TX_32X8},
    {STBV_AV1_TX_16X64, STBV_AV1_TX_8X32,  STBV_AV1_TX_4X4,   STBV_AV1_TX_16X32},
    {STBV_AV1_TX_16X32, STBV_AV1_TX_8X16,  STBV_AV1_TX_4X4,   STBV_AV1_TX_16X32},
    {STBV_AV1_TX_16X16, STBV_AV1_TX_8X8,   STBV_AV1_TX_8X16,  STBV_AV1_TX_16X16},
    {STBV_AV1_TX_16X8,  STBV_AV1_TX_8X4,   STBV_AV1_TX_8X8,   STBV_AV1_TX_16X8},
    {STBV_AV1_TX_16X4,  STBV_AV1_TX_8X4,   STBV_AV1_TX_8X4,   STBV_AV1_TX_16X4},
    {STBV_AV1_TX_8X32,  STBV_AV1_TX_4X16,  STBV_AV1_TX_4X4,   STBV_AV1_TX_8X32},
    {STBV_AV1_TX_8X16,  STBV_AV1_TX_4X8,   STBV_AV1_TX_4X4,   STBV_AV1_TX_8X16},
    {STBV_AV1_TX_8X8,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X8,   STBV_AV1_TX_8X8},
    {STBV_AV1_TX_8X4,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X4,   STBV_AV1_TX_8X4},
    {STBV_AV1_TX_4X16,  STBV_AV1_TX_4X8,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X16},
    {STBV_AV1_TX_4X8,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X8},
    {STBV_AV1_TX_4X4,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X4,   STBV_AV1_TX_4X4}
};

static int stbv_av1_tx_class(int tx_type)
{
    switch (tx_type) {
    case STBV_AV1_TX_V_DCT:
    case STBV_AV1_TX_V_ADST:
    case STBV_AV1_TX_V_FLIPADST:
        return STBV_AV1_TX_CLASS_V;
    case STBV_AV1_TX_H_DCT:
    case STBV_AV1_TX_H_ADST:
    case STBV_AV1_TX_H_FLIPADST:
        return STBV_AV1_TX_CLASS_H;
    default:
        return STBV_AV1_TX_CLASS_2D;
    }
}

/* dav1d_txtp_from_uvmode: chroma txtp derived from the intra UV mode.
 * Indexed by UV intra mode (DC,V,H,DDL,DDR,VR,HD,HU,VL,SMOOTH,
 * SMOOTH_V,SMOOTH_H,PAETH,CFL). */
static const unsigned char stbv_av1_txtp_from_uvmode[14] = {
    /* DC */        STBV_AV1_TX_DCT_DCT,
    /* VERT */      STBV_AV1_TX_ADST_DCT,
    /* HOR */       STBV_AV1_TX_DCT_ADST,
    /* DDL(45) */   STBV_AV1_TX_DCT_DCT,
    /* DDR(135) */  STBV_AV1_TX_ADST_ADST,
    /* VR(113) */   STBV_AV1_TX_ADST_DCT,
    /* HD(157) */   STBV_AV1_TX_DCT_ADST,
    /* HU(203) */   STBV_AV1_TX_DCT_ADST,
    /* VL(67) */    STBV_AV1_TX_ADST_DCT,
    /* SMOOTH */    STBV_AV1_TX_ADST_ADST,
    /* SMOOTH_V */  STBV_AV1_TX_ADST_DCT,
    /* SMOOTH_H */  STBV_AV1_TX_DCT_ADST,
    /* PAETH */     STBV_AV1_TX_ADST_ADST,
    /* CFL */       STBV_AV1_TX_DCT_DCT
};

/*
 * Decode one transform-size choice from dav1d's txsz CDF.
 *
 * max_tx is the maximum transform size for the current block.  tctx is the
 * transform-size context obtained from the neighbouring transform map.  The
 * returned value is the first transform size selected by the variable-tx
 * syntax; callers performing a full var-tx tree should repeat this operation
 * for each sub-transform using the txpart syntax below.
 */
static int stbv_av1_decode_tx_size(struct stb_av1_msac *msac,
                                   stbv_av1_cdf *cdf,
                                   int max_tx,
                                   int tctx)
{
    unsigned int depth;
    int n, max2;

    if (max_tx < 0 || max_tx >= STBV_AV1_N_TX_SIZES)
        return STBV_AV1_TX_4X4;
    /* dav1d: txsz[t_dim->max - 1][tctx], where t_dim->max is the square
     * maximum of the block's largest transform (1..4). */
    max2 = stbv_av1_tx_dims[max_tx].max;
    if (max2 <= STBV_AV1_TX_4X4)
        return STBV_AV1_TX_4X4;
    if (tctx < 0) tctx = 0;
    if (tctx > 2) tctx = 2;

    n = max2 < STBV_AV1_TX_16X16 ? max2 : 2;
    depth = stb_av1_msac_symbol(msac,
             &cdf->txsz[(max2 - 1) * 12 + tctx * 4],
             (size_t)n);

    while (depth-- > 0 && max_tx > STBV_AV1_TX_4X4)
        max_tx = stbv_av1_tx_dims[max_tx].sub;
    return max_tx;
}

/*
 * Decode one variable-transform partition decision.  This is the direct
 * scalar equivalent of dav1d's read_tx_tree() decision at one node.
 *
 * cat follows dav1d exactly:
 *     2 * (TX_64X64 - t_dim->max) - depth
 *
 * txpart contains [cat][above_smaller + left_smaller], two CDFs per cat.
 */
static int stbv_av1_decode_tx_split(struct stb_av1_msac *msac,
                                    stbv_av1_cdf *cdf,
                                    int tx,
                                    int depth,
                                    int above_smaller,
                                    int left_smaller)
{
    int cat;
    int ctx;

    if (depth >= 2 || tx <= STBV_AV1_TX_4X4)
        return 0;

    cat = 2 * (STBV_AV1_TX_64X64 - tx) - depth;
    if (cat < 0) cat = 0;
    if (cat > 2) cat = 2;

    ctx = (above_smaller ? 1 : 0) + (left_smaller ? 1 : 0);
    return (int)stb_av1_msac_bool_adapt(msac,
                &cdf->txpart[(cat * 3 + ctx) * 2]);
}

/*
 * Select the leaf transform type for an intra block.
 *
 * y_mode_nofilt is the ordinary directional/DC intra mode (FILTER_PRED has
 * already been converted to its underlying directional mode by the caller).
 * reduced_txtp_set selects dav1d's four-entry Intra2 set.
 *
 * The caller is responsible for dav1d's gating: no symbol is coded when the
 * block is lossless, when t_dim->max + 1 >= TX_64X64, when qidx == 0, or for
 * chroma planes (txtp is derived via stbv_av1_txtp_from_uvmode).
 */
static int stbv_av1_decode_intra_txtp(struct stb_av1_msac *msac,
                                      stbv_av1_cdf *cdf,
                                      int tx_min,
                                      int y_mode_nofilt,
                                      int reduced_txtp_set)
{
    unsigned int idx;
    int min2;

    if (y_mode_nofilt < 0 || y_mode_nofilt > 12) y_mode_nofilt = 0; /* DC */
    if (reduced_txtp_set || tx_min == STBV_AV1_TX_16X16) {
        /* txtp_intra2[min][y_mode], min in 0..2 */
        min2 = tx_min > 2 ? 2 : (tx_min < 0 ? 0 : tx_min);
        idx = stb_av1_msac_symbol(msac,
              &cdf->txtp_intra2[min2 * 104 + y_mode_nofilt * 8], 4);
        if (idx < 5)
            return stbv_av1_tx_set_intra2[idx];
        return STBV_AV1_TX_DCT_DCT;
    }

    /* txtp_intra1[min][y_mode], min in 0..1 */
    min2 = tx_min > 1 ? 1 : (tx_min < 0 ? 0 : tx_min);
    idx = stb_av1_msac_symbol(msac,
          &cdf->txtp_intra1[min2 * 104 + y_mode_nofilt * 8], 6);
    if (idx < 7)
        return stbv_av1_tx_set_intra1[idx];
    return STBV_AV1_TX_DCT_DCT;
}

/*
 * Decode the chroma txtp for inter blocks (including IBC).
 * uvt_dim is the chroma transform dimension; ytxtp is the already-decoded
 * luma txtp.  No MSAC consumption.
 */
static int stbv_av1_get_uv_inter_txtp(int uvt_dim_min, int uvt_dim_max,
                                      int ytxtp)
{
    if (uvt_dim_max == 3) /* TX_32X32 */
        return ytxtp == STBV_AV1_TX_IDTX ? STBV_AV1_TX_IDTX
                                          : STBV_AV1_TX_DCT_DCT;
    if (uvt_dim_min == 2) { /* TX_16X16 */
        /* H_FLIPADST=15, V_FLIPADST=14, H_ADST=13, V_ADST=12 */
        if ((1 << ytxtp) & ((1 << 15) | (1 << 14) | (1 << 13) | (1 << 12)))
            return STBV_AV1_TX_DCT_DCT;
    }
    return ytxtp;
}

/*
 * Decode the transform type for an inter block (including IBC).
 *
 * t_dim_min is the smaller dimension of the luma transform.
 * t_dim_max is the larger dimension.
 * reduced_txtp_set selects the restricted inter set.
 *
 * Returns the decoded transform type.  The caller must NOT be in an intra
 * block (use stbv_av1_decode_intra_txtp for intra).
 */
static int stbv_av1_decode_inter_txtp(struct stb_av1_msac *msac,
                                      stbv_av1_cdf *cdf,
                                      int t_dim_min, int t_dim_max,
                                      int reduced_txtp_set)
{
    unsigned int idx;

    if (reduced_txtp_set || t_dim_max == 3 /* TX_32X32 */) {
        /* txtp_inter3[t_dim_min]: single bool CDF, DCT_DCT vs IDTX */
        int min2 = t_dim_min > 3 ? 3 : (t_dim_min < 0 ? 0 : t_dim_min);
        idx = stb_av1_msac_bool_adapt(msac,
                  &cdf->txtp_inter3[min2 * 2]);
        return stbv_av1_tx_set_inter3[idx];
    } else if (t_dim_min == 2 /* TX_16X16 */) {
        idx = stb_av1_msac_symbol(msac, cdf->txtp_inter2, 11);
        return stbv_av1_tx_set_inter2[idx];
    } else {
        /* t_dim_min is 0 or 1 */
        int min2 = t_dim_min > 1 ? 1 : (t_dim_min < 0 ? 0 : t_dim_min);
        idx = stb_av1_msac_symbol(msac, cdf->txtp_inter1[min2], 15);
        return stbv_av1_tx_set_inter1[idx];
    }
}

#endif /* STB_AV1_TX_H */

/* ===== stb_av1_txstate.h ===== */
/*
 * stb_av1_txstate.h - AV1 transform-neighbour state
 *
 * The layout follows the part of dav1d's BlockContext used by read_tx_tree():
 * one transform-size value per 4x4 column on the above edge and one per 4x4
 * row on the left edge.  Values are expressed as transform log2-minus-2
 * dimensions (TX_4X4 == 0, TX_8X8 == 1, ...), just like dav1d's tx context.
 */
#ifndef STB_AV1_TXSTATE_H
#define STB_AV1_TXSTATE_H

#include <stddef.h>
#include <string.h>

#ifndef STB_AV1_TX_H
#error "include stb_av1_tx.h first"
#endif

typedef struct stbv_av1_tx_state {
    stbv_u8 *above_tx;
    stbv_u8 *left_tx;
    unsigned int above_n;
    unsigned int left_n;
    /* dav1d's tx_intra: stores the MAX transform size (lw/lh) per 4x4
     * position, used by get_tx_ctx() for the next block's TX context.
     * above_tx/left_tx store the decoded TX (updated by read_tx_tree at
     * leaf nodes) and are used by the tree split decision. */
    stbv_u8 *above_tx_intra;
    stbv_u8 *left_tx_intra;
} stbv_av1_tx_state;

static void stbv_av1_tx_state_init(stbv_av1_tx_state *s,
                                   stbv_u8 *above_tx, unsigned int above_n,
                                   stbv_u8 *left_tx, unsigned int left_n,
                                   stbv_u8 *above_tx_intra,
                                   stbv_u8 *left_tx_intra)
{
    if (!s) return;
    s->above_tx = above_tx;
    s->left_tx = left_tx;
    s->above_n = above_n;
    s->left_n = left_n;
    s->above_tx_intra = above_tx_intra;
    s->left_tx_intra = left_tx_intra;
    /* dav1d resets the above contexts once per frame (reset_context(&f->a));
     * a missing neighbour reads 0xff, compares "larger or equal" to any real
     * transform, and contributes 1 to the tx-size context.  The left context
     * is also reset at frame start; per row only the left is re-reset. */
    if (above_tx) memset(above_tx, 0xff, above_n);
    if (left_tx) memset(left_tx, 0xff, left_n);
    if (above_tx_intra) memset(above_tx_intra, 0xff, above_n);
    if (left_tx_intra) memset(left_tx_intra, 0xff, left_n);
}

static int stbv_av1_tx_is_smaller(const stbv_u8 *edge, int pos4, int tx_dim,
                                  unsigned int n)
{
    if (!edge || pos4 < 0 || (unsigned int)pos4 >= n)
        return 0;
    return edge[pos4] < tx_dim;
}

static int stbv_av1_tx_is_large(const stbv_u8 *edge, int pos4, int tx_dim,
                                unsigned int n)
{
    /* dav1d's tx_intra is int8_t and reset to -1: a missing neighbour is
     * never "larger or equal" than any real transform (log2 0..4).  The
     * 0xff sentinel must therefore compare as false. */
    if (!edge || pos4 < 0 || (unsigned int)pos4 >= n)
        return 0;
    return edge[pos4] != 0xff && edge[pos4] >= tx_dim;
}

/* dav1d resets only the LEFT transform context at each superblock row;
 * the above contexts persist across rows (reset once per frame). */
static void stbv_av1_tx_state_reset_row(stbv_av1_tx_state *s)
{
    if (!s) return;
    if (s->left_tx) memset(s->left_tx, 0xff, s->left_n);
    if (s->left_tx_intra) memset(s->left_tx_intra, 0xff, s->left_n);
}

/*
 * Decode and write one variable-transform tree.  This is the scalar form of
 * dav1d's read_tx_tree().  x4/y4 are frame-local 4x4 coordinates; the edge
 * arrays are tile/superblock-local and are therefore indexed modulo 32.
 *
 * The callback is called for every transform leaf.  For each leaf, *tx_out
 * is the selected transform size.  The callback can then decode transform
 * type and coefficients at that exact location.
 */
typedef int (*stbv_av1_tx_leaf_fn)(int x4, int y4, int tx,
                                   void *opaque);

/*
 * Phase 1: Read split bools and store in masks.
 * Matches dav1d's read_tx_tree() - only reads split decisions from MSAC,
 * does NOT decode coefficients.  Updates above/left TX context at leaves.
 * x4/y4 are frame-local 4x4 coordinates for context array indexing.
 * x_off/y_off are 0-based offsets within the TX hierarchy for mask indexing.
 */
static void stbv_av1_tx_tree_read_splits(struct stb_av1_msac *msac,
                                         stbv_av1_cdf *cdf,
                                         stbv_av1_tx_state *s,
                                         int from, int depth,
                                         stbv_u16 *masks,
                                         int x4, int y4,
                                         int x_off, int y_off)
{
    const int txw = stbv_av1_tx_dims[from].lw;
    const int txh = stbv_av1_tx_dims[from].lh;
    int is_split = 0;

    if (depth < 2 && from > STBV_AV1_TX_4X4) {
        const int cat = 2 * (STBV_AV1_TX_64X64 - stbv_av1_tx_dims[from].max) - depth;
        const int a = stbv_av1_tx_is_smaller(s->above_tx, x4, txw, s->above_n);
        const int l = stbv_av1_tx_is_smaller(s->left_tx,  y4, txh, s->left_n);
        int cat_clamp = cat < 0 ? 0 : (cat > 6 ? 6 : cat);
        is_split = stb_av1_msac_bool_adapt(msac,
                    &cdf->txpart[(cat_clamp * 3 + a + l) * 2]);
        if (is_split)
            masks[depth] |= 1 << (y_off * 4 + x_off);
    } else {
        is_split = 0;
    }

    if (is_split && stbv_av1_tx_dims[from].max > STBV_AV1_TX_8X8) {
        const int sub = stbv_av1_tx_dims[from].sub;
        const int subw4 = stbv_av1_tx_dims[sub].w;
        const int subh4 = stbv_av1_tx_dims[sub].h;

        stbv_av1_tx_tree_read_splits(msac, cdf, s, sub, depth + 1,
                                     masks, x4, y4,
                                     x_off * 2 + 0, y_off * 2 + 0);
        if (txw >= txh)
            stbv_av1_tx_tree_read_splits(msac, cdf, s, sub, depth + 1,
                                         masks, x4 + subw4, y4,
                                         x_off * 2 + 1, y_off * 2 + 0);
        if (txh >= txw) {
            stbv_av1_tx_tree_read_splits(msac, cdf, s, sub, depth + 1,
                                         masks, x4, y4 + subh4,
                                         x_off * 2 + 0, y_off * 2 + 1);
            if (txw >= txh)
                stbv_av1_tx_tree_read_splits(msac, cdf, s, sub, depth + 1,
                                             masks, x4 + subw4, y4 + subh4,
                                             x_off * 2 + 1, y_off * 2 + 1);
        }
    } else {
        /* Leaf: update above/left tx context.
         * dav1d stores the log2 TX dimension (txw/txh) and writes
         * 1<<txw bytes (dav1d_memset_pow2[lw]). */
        int i;
        for (i = 0; i < (1 << txw) && (unsigned int)(x4 + i) < s->above_n; i++)
            s->above_tx[x4 + i] = (stbv_u8)(is_split ? STBV_AV1_TX_4X4 : txw);
        for (i = 0; i < (1 << txh) && (unsigned int)(y4 + i) < s->left_n; i++)
            s->left_tx[y4 + i] = (stbv_u8)(is_split ? STBV_AV1_TX_4X4 : txh);
    }
}

/*
 * Phase 2: Traverse the split masks and decode coefficients at leaves.
 * Matches dav1d's read_coef_tree() - uses the pre-computed tx_split masks
 * to traverse the tree, calling the leaf callback at each leaf.
 * x4/y4 are frame-local 4x4 coordinates for the leaf callback.
 * x_off/y_off are 0-based offsets within the TX hierarchy for mask indexing.
 */
static int stbv_av1_tx_tree_read_coefs(struct stb_av1_msac *msac,
                                       stbv_av1_cdf *cdf,
                                       stbv_av1_tx_state *s,
                                       int from, int depth,
                                       const stbv_u16 *masks,
                                       int x4, int y4,
                                       int x_off, int y_off,
                                       stbv_av1_tx_leaf_fn leaf,
                                       void *opaque)
{
    const int txw = stbv_av1_tx_dims[from].w;
    const int txh = stbv_av1_tx_dims[from].h;

    if (depth < 2 && masks[depth] &&
        (masks[depth] & (1 << (y_off * 4 + x_off))))
    {
        const int sub = stbv_av1_tx_dims[from].sub;
        const int subw4 = stbv_av1_tx_dims[sub].w;
        const int subh4 = stbv_av1_tx_dims[sub].h;

        if (stbv_av1_tx_tree_read_coefs(msac, cdf, s, sub, depth + 1, masks,
                                        x4, y4,
                                        x_off * 2 + 0, y_off * 2 + 0,
                                        leaf, opaque)) return -1;
        if (txw >= txh) {
            if (stbv_av1_tx_tree_read_coefs(msac, cdf, s, sub, depth + 1, masks,
                                            x4 + subw4, y4,
                                            x_off * 2 + 1, y_off * 2 + 0,
                                            leaf, opaque)) return -2;
        }
        if (txh >= txw) {
            if (stbv_av1_tx_tree_read_coefs(msac, cdf, s, sub, depth + 1, masks,
                                            x4, y4 + subh4,
                                            x_off * 2 + 0, y_off * 2 + 1,
                                            leaf, opaque)) return -3;
            if (txw >= txh) {
                if (stbv_av1_tx_tree_read_coefs(msac, cdf, s, sub, depth + 1, masks,
                                                x4 + subw4, y4 + subh4,
                                                x_off * 2 + 1, y_off * 2 + 1,
                                                leaf, opaque)) return -4;
            }
        }
        return 0;
    }

    if (leaf) {
        return leaf(x4, y4, from, opaque);
    }
    return 0;
}

/*
 * Two-pass TX tree decode matching dav1d's architecture:
 *   Pass 1: read_tx_tree - read all split bools, store in masks,
 *            update above/left TX context at leaves.
 *   Pass 2: read_coef_tree - traverse masks, decode coefficients at leaves.
 *
 * This ensures MSAC consumption order matches dav1d exactly.
 */
static int stbv_av1_decode_tx_tree(struct stb_av1_msac *msac,
                                   stbv_av1_cdf *cdf,
                                   stbv_av1_tx_state *s,
                                   int max_tx,
                                   int x4,
                                   int y4,
                                   stbv_av1_tx_leaf_fn leaf,
                                   void *opaque)
{
    stbv_u16 tx_split[2] = { 0, 0 };
    const stbv_av1_tx_dim *const ytx = &stbv_av1_tx_dims[max_tx];
    const int bw4 = ytx->w;
    const int bh4 = ytx->h;
    int x, y, x_off, y_off;

    if (!msac || !cdf || !s || max_tx < STBV_AV1_TX_4X4 ||
        max_tx >= STBV_AV1_N_TX_SIZES)
        return -1;

    /* Pass 1: Read all split bools (dav1d read_tx_tree) */
    for (y = 0, y_off = 0; y < bh4; y += ytx->h, y_off++) {
        for (x = 0, x_off = 0; x < bw4; x += ytx->w, x_off++) {
            stbv_av1_tx_tree_read_splits(msac, cdf, s, max_tx, 0,
                                         tx_split,
                                         x4 + x, y4 + y,
                                         x_off, y_off);
        }
    }

    /* Pass 2: Decode coefficients at leaves (dav1d read_coef_tree) */
    for (y = 0, y_off = 0; y < bh4; y += ytx->h, y_off++) {
        for (x = 0, x_off = 0; x < bw4; x += ytx->w, x_off++) {
            int r = stbv_av1_tx_tree_read_coefs(msac, cdf, s, max_tx, 0,
                                                tx_split,
                                                x4 + x, y4 + y,
                                                x_off, y_off,
                                                leaf, opaque);
            if (r) return r;
        }
    }

    return 0;
}

#endif /* STB_AV1_TXSTATE_H */

/* ===== stb_av1_intra.h ===== */
/*
 * stb_av1_intra.h - scalar AV1 intra-mode syntax decoder
 *
 * Intra syntax adapted from dav1d 1.5.4 src/decode.c.
 * BSD-2-Clause; see dav1d COPYING for attribution/license details.
 */
#ifndef STB_AV1_INTRA_H
#define STB_AV1_INTRA_H

#ifndef STB_AV1_MSAC_H
#error "include stb_av1_msac.h first"
#endif
#ifndef STB_AV1_CDF_H
#error "include stb_av1_cdf.h first"
#endif

#define STBV_AV1_INTRA_DC          0
#define STBV_AV1_INTRA_VERT        1
#define STBV_AV1_INTRA_HOR         2
#define STBV_AV1_INTRA_DDL         3
#define STBV_AV1_INTRA_DDR         4
#define STBV_AV1_INTRA_VR          5
#define STBV_AV1_INTRA_HD          6
#define STBV_AV1_INTRA_HU          7
#define STBV_AV1_INTRA_VL          8
#define STBV_AV1_INTRA_SMOOTH      9
#define STBV_AV1_INTRA_SMOOTH_V   10
#define STBV_AV1_INTRA_SMOOTH_H   11
#define STBV_AV1_INTRA_PAETH      12
#define STBV_AV1_INTRA_CFL        13
#define STBV_AV1_INTRA_FILTER     14

/* Note: dav1d_filter_mode_to_y_mode is defined in stb_av1_leaf.h as
 * stb_filter_mode_to_y_mode (the version actually used during decode). */

/* dav1d_cfl_allowed_mask: cfl is allowed for blocks no larger than 32x32
 * (bits for BS_32x32 .. BS_4x4, dav1d tables.h). */
/* dav1d_cfl_allowed_mask: cfl allowed for blocks <= 32x32 except rect
 * 16x64/64x16/32x64/64x32 etc: bits {BS_32x32,32x16,32x8,16x32,16x16,
 * 16x8,16x4,8x32,8x16,8x8,8x4,4x16,4x8,4x4} == {7,8,9,11..21}. */
#define STBV_AV1_CFL_ALLOWED_MASK 0x3FFB80u

/* dav1d_intra_mode_context[], in the same order as N_INTRA_PRED_MODES. */
static const stbv_u8 stbv_av1_intra_mode_ctx[13] = {
    0, 1, 2, 3, 4, 4, 4, 4, 3, 0, 1, 2, 0
};

struct stb_av1_intra_block {
    int y_mode;
    int y_angle;
    int uv_mode;
    int uv_angle;
    int cfl_alpha_u;
    int cfl_alpha_v;
};

static int stbv_av1_decode_intra_mode(struct stb_av1_msac *msac,
                                      stbv_av1_cdf *cdf,
                                      int above_mode, int left_mode,
                                      int cbw4, int cbh4,
                                      int cfl_allowed,
                                      int has_chroma,
                                      struct stb_av1_intra_block *b)
{
    int ac, lc, mode;
    stbv_u16 *ycdf;
    stbv_u16 *uvcdf;
    unsigned sym;
    int sign, sign_u, sign_v, ctx;

    if (!b) return -1;
    if (above_mode < 0 || above_mode > 12) above_mode = STBV_AV1_INTRA_DC;
    if (left_mode < 0 || left_mode > 12) left_mode = STBV_AV1_INTRA_DC;

    ac = stbv_av1_intra_mode_ctx[above_mode];
    lc = stbv_av1_intra_mode_ctx[left_mode];
    ycdf = cdf->kfym + (ac * 5 + lc) * 16;
    sym = stb_av1_msac_symbol(msac, ycdf, 12);
    if (sym > 12) return -2;
    mode = (int)sym;
    b->y_mode = mode;
    b->y_angle = 0;
    b->uv_mode = STBV_AV1_INTRA_DC;
    b->uv_angle = 0;
    b->cfl_alpha_u = 0;
    b->cfl_alpha_v = 0;

    /* dav1d uses b_dim[2] + b_dim[3] >= 2 which equals log2(bw4)+log2(bh4)>=2 */
    if (cbw4 * cbh4 >= 4 && mode >= STBV_AV1_INTRA_VERT &&
        mode <= STBV_AV1_INTRA_VL) {
        sym = stb_av1_msac_symbol(msac,
                                  cdf->angle_delta + (mode - STBV_AV1_INTRA_VERT) * 8,
                                  6);
        b->y_angle = (int)sym - 3;
    }

    /* Chroma syntax: only decoded when the block covers chroma samples.
     * For subsampled formats with small blocks at even positions,
     * has_chroma is false and NO chroma symbols are consumed. */
    if (!has_chroma)
        return 0;

    uvcdf = cdf->uv_mode + ((cfl_allowed ? 1 : 0) * 13 + mode) * 16;
    sym = stb_av1_msac_symbol(msac, uvcdf, cfl_allowed ? 13 : 12);
    if (sym > (unsigned)(cfl_allowed ? 13 : 12)) return -3;
    b->uv_mode = (int)sym;

    if (b->uv_mode == STBV_AV1_INTRA_CFL) {
        sym = stb_av1_msac_symbol(msac, cdf->cfl_sign, 7);
        sign = (int)sym + 1;
        sign_u = sign / 3;
        sign_v = sign - sign_u * 3;

        if (sign_u) {
            ctx = (sign_u == 2) * 3 + sign_v;
            sym = stb_av1_msac_symbol(msac, cdf->cfl_alpha + ctx * 16, 15);
            b->cfl_alpha_u = (int)sym + 1;
            if (sign_u == 1) b->cfl_alpha_u = -b->cfl_alpha_u;
        }
        if (sign_v) {
            ctx = (sign_v == 2) * 3 + sign_u;
            sym = stb_av1_msac_symbol(msac, cdf->cfl_alpha + ctx * 16, 15);
            b->cfl_alpha_v = (int)sym + 1;
            if (sign_v == 1) b->cfl_alpha_v = -b->cfl_alpha_v;
        }
    } else if (cbw4 * cbh4 >= 4 && b->uv_mode >= STBV_AV1_INTRA_VERT &&
               b->uv_mode <= STBV_AV1_INTRA_VL) {
        sym = stb_av1_msac_symbol(msac,
                                  cdf->angle_delta + (b->uv_mode - STBV_AV1_INTRA_VERT) * 8,
                                  6);
        b->uv_angle = (int)sym - 3;
    }

    return 0;
}

#endif

/* ===== stb_av1_state.h ===== */
/*
 * stb_av1_state.h - scalar intra tile neighbor state
 *
 * The state here mirrors the small part of dav1d's TileState that is needed
 * before reconstruction: per-4x4 above/left intra modes.  It is deliberately
 * separate from pixel storage so the entropy syntax can be validated first.
 */
#ifndef STB_AV1_STATE_H
#define STB_AV1_STATE_H

#include <stddef.h>
#include <string.h>

#ifndef STB_AV1_INTRA_H
#error "include stb_av1_intra.h first"
#endif
#ifndef STB_AV1_PARTITION_H
#error "include stb_av1_partition.h first"
#endif

struct stb_av1_intra_state {
    stbv_u8 *above_mode;
    stbv_u8 *left_mode;
    unsigned int above_count;
    unsigned int left_count;
    /* Chroma neighbour modes (dav1d BlockContext uvmode maps), indexed by
     * chroma 4x4 position; only written for blocks with chroma. */
    stbv_u8 *above_uvmode;
    stbv_u8 *left_uvmode;
    unsigned int above_uv_count;
    unsigned int left_uv_count;
};

static void stb_av1_intra_state_init(struct stb_av1_intra_state *s,
                                     stbv_u8 *above_mode,
                                     unsigned int above_count,
                                     stbv_u8 *left_mode,
                                     unsigned int left_count)
{
    s->above_mode = above_mode;
    s->left_mode = left_mode;
    s->above_count = above_count;
    s->left_count = left_count;
    s->above_uvmode = 0;
    s->left_uvmode = 0;
    s->above_uv_count = 0;
    s->left_uv_count = 0;
    if (above_mode) memset(above_mode, STBV_AV1_INTRA_DC, above_count);
    if (left_mode) memset(left_mode, STBV_AV1_INTRA_DC, left_count);
}

static void stb_av1_intra_state_set_uv(struct stb_av1_intra_state *s,
                                       stbv_u8 *above_uvmode,
                                       unsigned int above_uv_count,
                                       stbv_u8 *left_uvmode,
                                       unsigned int left_uv_count)
{
    s->above_uvmode = above_uvmode;
    s->left_uvmode = left_uvmode;
    s->above_uv_count = above_uv_count;
    s->left_uv_count = left_uv_count;
    if (above_uvmode) memset(above_uvmode, STBV_AV1_INTRA_DC, above_uv_count);
    if (left_uvmode) memset(left_uvmode, STBV_AV1_INTRA_DC, left_uv_count);
}

static int stb_av1_intra_state_decode_leaf(
    struct stb_av1_msac *msac, stbv_av1_cdf *cdf,
    struct stb_av1_intra_state *s,
    int bx4, int by4, int bs,
    int cfl_allowed, int has_chroma,
    struct stb_av1_intra_block *out)
{
    int bw4, bh4, above, left;
    if (!s || !out || bs < 0 || bs >= STBV_AV1_N_BS_SIZES)
        return -1;
    bw4 = stbv_av1_block_dimensions[bs][0];
    bh4 = stbv_av1_block_dimensions[bs][1];
    if (bw4 <= 0 || bh4 <= 0)
        return -1;

    above = (s->above_mode && (unsigned)bx4 < s->above_count) ?
        s->above_mode[bx4] : STBV_AV1_INTRA_DC;
    left = (s->left_mode && (unsigned)by4 < s->left_count) ?
        s->left_mode[by4] : STBV_AV1_INTRA_DC;

    if (stbv_av1_decode_intra_mode(msac, cdf, above, left,
                                   bw4, bh4, cfl_allowed, has_chroma, out))
        return -2;

    return 0;
}

#endif

/* ===== stb_av1_coef.h ===== */
/*
 * stb_av1_coef.h - scalar AV1 coefficient decoder
 *
 * Coefficient syntax derived from dav1d 1.5.4 src/recon_tmpl.c.
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_COEF_H
#define STB_AV1_COEF_H

#include <string.h>

#ifndef STBV_U16_DEFINED
#error "stb_av1_coef.h requires stbv_u16"
#endif
#ifndef STBV_U32_DEFINED
#error "stb_av1_coef.h requires stbv_u32"
#endif
#ifndef STBV_I32_DEFINED
#error "stb_av1_coef.h requires stbv_i32"
#endif

/* tx_class: 0 = 2D, 1 = horizontal, 2 = vertical. */

static unsigned stbv_av1_coef_hi_tok(struct stb_av1_msac *s, stbv_u16 *cdf)
{
    unsigned t;
    unsigned v;
    t = stb_av1_msac_symbol(s, cdf, 3);
    v = 3 + t;
    if (t == 3) {
        t = stb_av1_msac_symbol(s, cdf, 3);
        v = 6 + t;
        if (t == 3) {
            t = stb_av1_msac_symbol(s, cdf, 3);
            v = 9 + t;
            if (t == 3)
                v = 12 + stb_av1_msac_symbol(s, cdf, 3);
        }
    }
    return v;
}

static unsigned stbv_av1_coef_golomb(struct stb_av1_msac *s)
{
    unsigned len = 0;
    unsigned v = 1;
    while (!stb_av1_msac_bool_equi(s) && len < 32U)
        len++;
    while (len--)
        v = (v << 1) | stb_av1_msac_bool_equi(s);
    return v - 1U;
}

/* dav1d_lo_ctx_offsets[3][5][5]: 0 = square, 1 = w > h, 2 = w < h.  Indexed
 * with dav1d's nonsquare + (tx & nonsquare), nonsquare = tx >= RTX_4X8. */
static const unsigned char stbv_av1_lo_ctx_offsets[3][5][5] = {
    {
        {  0,  1,  6,  6, 21 },
        {  1,  6,  6, 21, 21 },
        {  6,  6, 21, 21, 21 },
        {  6, 21, 21, 21, 21 },
        { 21, 21, 21, 21, 21 }
    }, {
        {  0, 16,  6,  6, 21 },
        { 16, 16,  6, 21, 21 },
        { 16, 16, 21, 21, 21 },
        { 16, 16, 21, 21, 21 },
        { 16, 16, 21, 21, 21 }
    }, {
        {  0, 11, 11, 11, 11 },
        { 11, 11, 11, 11, 11 },
        {  6,  6, 21, 21, 21 },
        {  6, 21, 21, 21, 21 },
        { 21, 21, 21, 21, 21 }
    }
};

static unsigned stbv_av1_coef_lo_ctx(const unsigned char *levels,
                                      unsigned *hi_mag,
                                      unsigned x, unsigned y,
                                      unsigned stride, int tx_class,
                                      const unsigned char (*ctx_offsets)[5])
{
    unsigned mag = levels[stride + 0] + levels[1];
    unsigned off;
    if (tx_class == 0) {
        mag += levels[stride + 1];
        *hi_mag = mag;
        mag += levels[2 * stride] + levels[2];
        off = ctx_offsets[y > 4 ? 4 : y][x > 4 ? 4 : x];
    } else {
        mag += levels[2];
        *hi_mag = mag;
        mag += levels[3] + levels[4];
        off = 26U + (y > 1U ? 10U : y * 5U);
    }
    return off + (mag > 512U ? 4U : (mag + 64U) >> 7);
}

static const stbv_u16 stbv_av1_scan_4x4[16] = {
    0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15
};

static const stbv_u16 stbv_av1_scan_8x8[64] = {
    0, 8, 1, 2, 9, 16, 24, 17, 10, 3, 4, 11, 18, 25, 32, 40,
    33, 26, 19, 12, 5, 6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35,
    28, 21, 14, 7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30,
    23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63
};

static const stbv_u16 stbv_av1_scan_16x16[256] = {
    0, 16, 1, 2, 17, 32, 48, 33, 18, 3, 4, 19, 34, 49, 64, 80,
    65, 50, 35, 20, 5, 6, 21, 36, 51, 66, 81, 96, 112, 97, 82, 67,
    52, 37, 22, 7, 8, 23, 38, 53, 68, 83, 98, 113, 128, 144, 129, 114,
    99, 84, 69, 54, 39, 24, 9, 10, 25, 40, 55, 70, 85, 100, 115, 130,
    145, 160, 176, 161, 146, 131, 116, 101, 86, 71, 56, 41, 26, 11, 12, 27,
    42, 57, 72, 87, 102, 117, 132, 147, 162, 177, 192, 208, 193, 178, 163, 148,
    133, 118, 103, 88, 73, 58, 43, 28, 13, 14, 29, 44, 59, 74, 89, 104,
    119, 134, 149, 164, 179, 194, 209, 224, 240, 225, 210, 195, 180, 165, 150, 135,
    120, 105, 90, 75, 60, 45, 30, 15, 31, 46, 61, 76, 91, 106, 121, 136,
    151, 166, 181, 196, 211, 226, 241, 242, 227, 212, 197, 182, 167, 152, 137, 122,
    107, 92, 77, 62, 47, 63, 78, 93, 108, 123, 138, 153, 168, 183, 198, 213,
    228, 243, 244, 229, 214, 199, 184, 169, 154, 139, 124, 109, 94, 79, 95, 110,
    125, 140, 155, 170, 185, 200, 215, 230, 245, 246, 231, 216, 201, 186, 171, 156,
    141, 126, 111, 127, 142, 157, 172, 187, 202, 217, 232, 247, 248, 233, 218, 203,
    188, 173, 158, 143, 159, 174, 189, 204, 219, 234, 249, 250, 235, 220, 205, 190,
    175, 191, 206, 221, 236, 251, 252, 237, 222, 207, 223, 238, 253, 254, 239, 255
};

static const stbv_u16 stbv_av1_scan_32x32[1024] = {
    0, 32, 1, 2, 33, 64, 96, 65, 34, 3, 4, 35, 66, 97, 128, 160,
    129, 98, 67, 36, 5, 6, 37, 68, 99, 130, 161, 192, 224, 193, 162, 131,
    100, 69, 38, 7, 8, 39, 70, 101, 132, 163, 194, 225, 256, 288, 257, 226,
    195, 164, 133, 102, 71, 40, 9, 10, 41, 72, 103, 134, 165, 196, 227, 258,
    289, 320, 352, 321, 290, 259, 228, 197, 166, 135, 104, 73, 42, 11, 12, 43,
    74, 105, 136, 167, 198, 229, 260, 291, 322, 353, 384, 416, 385, 354, 323, 292,
    261, 230, 199, 168, 137, 106, 75, 44, 13, 14, 45, 76, 107, 138, 169, 200,
    231, 262, 293, 324, 355, 386, 417, 448, 480, 449, 418, 387, 356, 325, 294, 263,
    232, 201, 170, 139, 108, 77, 46, 15, 16, 47, 78, 109, 140, 171, 202, 233,
    264, 295, 326, 357, 388, 419, 450, 481, 512, 544, 513, 482, 451, 420, 389, 358,
    327, 296, 265, 234, 203, 172, 141, 110, 79, 48, 17, 18, 49, 80, 111, 142,
    173, 204, 235, 266, 297, 328, 359, 390, 421, 452, 483, 514, 545, 576, 608, 577,
    546, 515, 484, 453, 422, 391, 360, 329, 298, 267, 236, 205, 174, 143, 112, 81,
    50, 19, 20, 51, 82, 113, 144, 175, 206, 237, 268, 299, 330, 361, 392, 423,
    454, 485, 516, 547, 578, 609, 640, 672, 641, 610, 579, 548, 517, 486, 455, 424,
    393, 362, 331, 300, 269, 238, 207, 176, 145, 114, 83, 52, 21, 22, 53, 84,
    115, 146, 177, 208, 239, 270, 301, 332, 363, 394, 425, 456, 487, 518, 549, 580,
    611, 642, 673, 704, 736, 705, 674, 643, 612, 581, 550, 519, 488, 457, 426, 395,
    364, 333, 302, 271, 240, 209, 178, 147, 116, 85, 54, 23, 24, 55, 86, 117,
    148, 179, 210, 241, 272, 303, 334, 365, 396, 427, 458, 489, 520, 551, 582, 613,
    644, 675, 706, 737, 768, 800, 769, 738, 707, 676, 645, 614, 583, 552, 521, 490,
    459, 428, 397, 366, 335, 304, 273, 242, 211, 180, 149, 118, 87, 56, 25, 26,
    57, 88, 119, 150, 181, 212, 243, 274, 305, 336, 367, 398, 429, 460, 491, 522,
    553, 584, 615, 646, 677, 708, 739, 770, 801, 832, 864, 833, 802, 771, 740, 709,
    678, 647, 616, 585, 554, 523, 492, 461, 430, 399, 368, 337, 306, 275, 244, 213,
    182, 151, 120, 89, 58, 27, 28, 59, 90, 121, 152, 183, 214, 245, 276, 307,
    338, 369, 400, 431, 462, 493, 524, 555, 586, 617, 648, 679, 710, 741, 772, 803,
    834, 865, 896, 928, 897, 866, 835, 804, 773, 742, 711, 680, 649, 618, 587, 556,
    525, 494, 463, 432, 401, 370, 339, 308, 277, 246, 215, 184, 153, 122, 91, 60,
    29, 30, 61, 92, 123, 154, 185, 216, 247, 278, 309, 340, 371, 402, 433, 464,
    495, 526, 557, 588, 619, 650, 681, 712, 743, 774, 805, 836, 867, 898, 929, 960,
    992, 961, 930, 899, 868, 837, 806, 775, 744, 713, 682, 651, 620, 589, 558, 527,
    496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124, 93, 62, 31,
    63, 94, 125, 156, 187, 218, 249, 280, 311, 342, 373, 404, 435, 466, 497, 528,
    559, 590, 621, 652, 683, 714, 745, 776, 807, 838, 869, 900, 931, 962, 993, 994,
    963, 932, 901, 870, 839, 808, 777, 746, 715, 684, 653, 622, 591, 560, 529, 498,
    467, 436, 405, 374, 343, 312, 281, 250, 219, 188, 157, 126, 95, 127, 158, 189,
    220, 251, 282, 313, 344, 375, 406, 437, 468, 499, 530, 561, 592, 623, 654, 685,
    716, 747, 778, 809, 840, 871, 902, 933, 964, 995, 996, 965, 934, 903, 872, 841,
    810, 779, 748, 717, 686, 655, 624, 593, 562, 531, 500, 469, 438, 407, 376, 345,
    314, 283, 252, 221, 190, 159, 191, 222, 253, 284, 315, 346, 377, 408, 439, 470,
    501, 532, 563, 594, 625, 656, 687, 718, 749, 780, 811, 842, 873, 904, 935, 966,
    997, 998, 967, 936, 905, 874, 843, 812, 781, 750, 719, 688, 657, 626, 595, 564,
    533, 502, 471, 440, 409, 378, 347, 316, 285, 254, 223, 255, 286, 317, 348, 379,
    410, 441, 472, 503, 534, 565, 596, 627, 658, 689, 720, 751, 782, 813, 844, 875,
    906, 937, 968, 999, 1000, 969, 938, 907, 876, 845, 814, 783, 752, 721, 690, 659,
    628, 597, 566, 535, 504, 473, 442, 411, 380, 349, 318, 287, 319, 350, 381, 412,
    443, 474, 505, 536, 567, 598, 629, 660, 691, 722, 753, 784, 815, 846, 877, 908,
    939, 970, 1001, 1002, 971, 940, 909, 878, 847, 816, 785, 754, 723, 692, 661, 630,
    599, 568, 537, 506, 475, 444, 413, 382, 351, 383, 414, 445, 476, 507, 538, 569,
    600, 631, 662, 693, 724, 755, 786, 817, 848, 879, 910, 941, 972, 1003, 1004, 973,
    942, 911, 880, 849, 818, 787, 756, 725, 694, 663, 632, 601, 570, 539, 508, 477,
    446, 415, 447, 478, 509, 540, 571, 602, 633, 664, 695, 726, 757, 788, 819, 850,
    881, 912, 943, 974, 1005, 1006, 975, 944, 913, 882, 851, 820, 789, 758, 727, 696,
    665, 634, 603, 572, 541, 510, 479, 511, 542, 573, 604, 635, 666, 697, 728, 759,
    790, 821, 852, 883, 914, 945, 976, 1007, 1008, 977, 946, 915, 884, 853, 822, 791,
    760, 729, 698, 667, 636, 605, 574, 543, 575, 606, 637, 668, 699, 730, 761, 792,
    823, 854, 885, 916, 947, 978, 1009, 1010, 979, 948, 917, 886, 855, 824, 793, 762,
    731, 700, 669, 638, 607, 639, 670, 701, 732, 763, 794, 825, 856, 887, 918, 949,
    980, 1011, 1012, 981, 950, 919, 888, 857, 826, 795, 764, 733, 702, 671, 703, 734,
    765, 796, 827, 858, 889, 920, 951, 982, 1013, 1014, 983, 952, 921, 890, 859, 828,
    797, 766, 735, 767, 798, 829, 860, 891, 922, 953, 984, 1015, 1016, 985, 954, 923,
    892, 861, 830, 799, 831, 862, 893, 924, 955, 986, 1017, 1018, 987, 956, 925, 894,
    863, 895, 926, 957, 988, 1019, 1020, 989, 958, 927, 959, 990, 1021, 1022, 991, 1023
};

static const stbv_u16 stbv_av1_scan_4x8[32] = {
    0, 8, 1, 16, 9, 2, 24, 17, 10, 3, 25, 18, 11, 4, 26, 19, 12, 5, 27, 20, 13, 6, 28, 21, 
    14, 7, 29, 22, 15, 30, 23, 31
};

static const stbv_u16 stbv_av1_scan_8x4[32] = {
    0, 1, 4, 2, 5, 8, 3, 6, 9, 12, 7, 10, 13, 16, 11, 14, 17, 20, 15, 18, 21, 24, 19, 22, 25, 
    28, 23, 26, 29, 27, 30, 31
};

static const stbv_u16 stbv_av1_scan_8x16[128] = {
    0, 16, 1, 32, 17, 2, 48, 33, 18, 3, 64, 49, 34, 19, 4, 80, 65, 50, 35, 20, 5, 96, 81, 66, 
    51, 36, 21, 6, 112, 97, 82, 67, 52, 37, 22, 7, 113, 98, 83, 68, 53, 38, 23, 8, 114, 99, 
    84, 69, 54, 39, 24, 9, 115, 100, 85, 70, 55, 40, 25, 10, 116, 101, 86, 71, 56, 41, 26, 
    11, 117, 102, 87, 72, 57, 42, 27, 12, 118, 103, 88, 73, 58, 43, 28, 13, 119, 104, 89, 74, 
    59, 44, 29, 14, 120, 105, 90, 75, 60, 45, 30, 15, 121, 106, 91, 76, 61, 46, 31, 122, 107, 
    92, 77, 62, 47, 123, 108, 93, 78, 63, 124, 109, 94, 79, 125, 110, 95, 126, 111, 127
};

static const stbv_u16 stbv_av1_scan_16x8[128] = {
    0, 1, 8, 2, 9, 16, 3, 10, 17, 24, 4, 11, 18, 25, 32, 5, 12, 19, 26, 33, 40, 6, 13, 20, 
    27, 34, 41, 48, 7, 14, 21, 28, 35, 42, 49, 56, 15, 22, 29, 36, 43, 50, 57, 64, 23, 30, 
    37, 44, 51, 58, 65, 72, 31, 38, 45, 52, 59, 66, 73, 80, 39, 46, 53, 60, 67, 74, 81, 88, 
    47, 54, 61, 68, 75, 82, 89, 96, 55, 62, 69, 76, 83, 90, 97, 104, 63, 70, 77, 84, 91, 98, 
    105, 112, 71, 78, 85, 92, 99, 106, 113, 120, 79, 86, 93, 100, 107, 114, 121, 87, 94, 101, 
    108, 115, 122, 95, 102, 109, 116, 123, 103, 110, 117, 124, 111, 118, 125, 119, 126, 127
};

static const stbv_u16 stbv_av1_scan_16x32[512] = {
    0, 32, 1, 64, 33, 2, 96, 65, 34, 3, 128, 97, 66, 35, 4, 160, 129, 98, 67, 36, 5, 192, 161, 
    130, 99, 68, 37, 6, 224, 193, 162, 131, 100, 69, 38, 7, 256, 225, 194, 163, 132, 101, 70, 
    39, 8, 288, 257, 226, 195, 164, 133, 102, 71, 40, 9, 320, 289, 258, 227, 196, 165, 134, 
    103, 72, 41, 10, 352, 321, 290, 259, 228, 197, 166, 135, 104, 73, 42, 11, 384, 353, 322, 
    291, 260, 229, 198, 167, 136, 105, 74, 43, 12, 416, 385, 354, 323, 292, 261, 230, 199, 
    168, 137, 106, 75, 44, 13, 448, 417, 386, 355, 324, 293, 262, 231, 200, 169, 138, 107, 
    76, 45, 14, 480, 449, 418, 387, 356, 325, 294, 263, 232, 201, 170, 139, 108, 77, 46, 15, 
    481, 450, 419, 388, 357, 326, 295, 264, 233, 202, 171, 140, 109, 78, 47, 16, 482, 451, 
    420, 389, 358, 327, 296, 265, 234, 203, 172, 141, 110, 79, 48, 17, 483, 452, 421, 390, 
    359, 328, 297, 266, 235, 204, 173, 142, 111, 80, 49, 18, 484, 453, 422, 391, 360, 329, 
    298, 267, 236, 205, 174, 143, 112, 81, 50, 19, 485, 454, 423, 392, 361, 330, 299, 268, 
    237, 206, 175, 144, 113, 82, 51, 20, 486, 455, 424, 393, 362, 331, 300, 269, 238, 207, 
    176, 145, 114, 83, 52, 21, 487, 456, 425, 394, 363, 332, 301, 270, 239, 208, 177, 146, 
    115, 84, 53, 22, 488, 457, 426, 395, 364, 333, 302, 271, 240, 209, 178, 147, 116, 85, 54, 
    23, 489, 458, 427, 396, 365, 334, 303, 272, 241, 210, 179, 148, 117, 86, 55, 24, 490, 459, 
    428, 397, 366, 335, 304, 273, 242, 211, 180, 149, 118, 87, 56, 25, 491, 460, 429, 398, 
    367, 336, 305, 274, 243, 212, 181, 150, 119, 88, 57, 26, 492, 461, 430, 399, 368, 337, 
    306, 275, 244, 213, 182, 151, 120, 89, 58, 27, 493, 462, 431, 400, 369, 338, 307, 276, 
    245, 214, 183, 152, 121, 90, 59, 28, 494, 463, 432, 401, 370, 339, 308, 277, 246, 215, 
    184, 153, 122, 91, 60, 29, 495, 464, 433, 402, 371, 340, 309, 278, 247, 216, 185, 154, 
    123, 92, 61, 30, 496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124, 93, 62, 
    31, 497, 466, 435, 404, 373, 342, 311, 280, 249, 218, 187, 156, 125, 94, 63, 498, 467, 
    436, 405, 374, 343, 312, 281, 250, 219, 188, 157, 126, 95, 499, 468, 437, 406, 375, 344, 
    313, 282, 251, 220, 189, 158, 127, 500, 469, 438, 407, 376, 345, 314, 283, 252, 221, 190, 
    159, 501, 470, 439, 408, 377, 346, 315, 284, 253, 222, 191, 502, 471, 440, 409, 378, 347, 
    316, 285, 254, 223, 503, 472, 441, 410, 379, 348, 317, 286, 255, 504, 473, 442, 411, 380, 
    349, 318, 287, 505, 474, 443, 412, 381, 350, 319, 506, 475, 444, 413, 382, 351, 507, 476, 
    445, 414, 383, 508, 477, 446, 415, 509, 478, 447, 510, 479, 511
};

static const stbv_u16 stbv_av1_scan_32x16[512] = {
    0, 1, 16, 2, 17, 32, 3, 18, 33, 48, 4, 19, 34, 49, 64, 5, 20, 35, 50, 65, 80, 6, 21, 36, 
    51, 66, 81, 96, 7, 22, 37, 52, 67, 82, 97, 112, 8, 23, 38, 53, 68, 83, 98, 113, 128, 9, 
    24, 39, 54, 69, 84, 99, 114, 129, 144, 10, 25, 40, 55, 70, 85, 100, 115, 130, 145, 160, 
    11, 26, 41, 56, 71, 86, 101, 116, 131, 146, 161, 176, 12, 27, 42, 57, 72, 87, 102, 117, 
    132, 147, 162, 177, 192, 13, 28, 43, 58, 73, 88, 103, 118, 133, 148, 163, 178, 193, 208, 
    14, 29, 44, 59, 74, 89, 104, 119, 134, 149, 164, 179, 194, 209, 224, 15, 30, 45, 60, 75, 
    90, 105, 120, 135, 150, 165, 180, 195, 210, 225, 240, 31, 46, 61, 76, 91, 106, 121, 136, 
    151, 166, 181, 196, 211, 226, 241, 256, 47, 62, 77, 92, 107, 122, 137, 152, 167, 182, 197, 
    212, 227, 242, 257, 272, 63, 78, 93, 108, 123, 138, 153, 168, 183, 198, 213, 228, 243, 
    258, 273, 288, 79, 94, 109, 124, 139, 154, 169, 184, 199, 214, 229, 244, 259, 274, 289, 
    304, 95, 110, 125, 140, 155, 170, 185, 200, 215, 230, 245, 260, 275, 290, 305, 320, 111, 
    126, 141, 156, 171, 186, 201, 216, 231, 246, 261, 276, 291, 306, 321, 336, 127, 142, 157, 
    172, 187, 202, 217, 232, 247, 262, 277, 292, 307, 322, 337, 352, 143, 158, 173, 188, 203, 
    218, 233, 248, 263, 278, 293, 308, 323, 338, 353, 368, 159, 174, 189, 204, 219, 234, 249, 
    264, 279, 294, 309, 324, 339, 354, 369, 384, 175, 190, 205, 220, 235, 250, 265, 280, 295, 
    310, 325, 340, 355, 370, 385, 400, 191, 206, 221, 236, 251, 266, 281, 296, 311, 326, 341, 
    356, 371, 386, 401, 416, 207, 222, 237, 252, 267, 282, 297, 312, 327, 342, 357, 372, 387, 
    402, 417, 432, 223, 238, 253, 268, 283, 298, 313, 328, 343, 358, 373, 388, 403, 418, 433, 
    448, 239, 254, 269, 284, 299, 314, 329, 344, 359, 374, 389, 404, 419, 434, 449, 464, 255, 
    270, 285, 300, 315, 330, 345, 360, 375, 390, 405, 420, 435, 450, 465, 480, 271, 286, 301, 
    316, 331, 346, 361, 376, 391, 406, 421, 436, 451, 466, 481, 496, 287, 302, 317, 332, 347, 
    362, 377, 392, 407, 422, 437, 452, 467, 482, 497, 303, 318, 333, 348, 363, 378, 393, 408, 
    423, 438, 453, 468, 483, 498, 319, 334, 349, 364, 379, 394, 409, 424, 439, 454, 469, 484, 
    499, 335, 350, 365, 380, 395, 410, 425, 440, 455, 470, 485, 500, 351, 366, 381, 396, 411, 
    426, 441, 456, 471, 486, 501, 367, 382, 397, 412, 427, 442, 457, 472, 487, 502, 383, 398, 
    413, 428, 443, 458, 473, 488, 503, 399, 414, 429, 444, 459, 474, 489, 504, 415, 430, 445, 
    460, 475, 490, 505, 431, 446, 461, 476, 491, 506, 447, 462, 477, 492, 507, 463, 478, 493, 
    508, 479, 494, 509, 495, 510, 511
};

static const stbv_u16 stbv_av1_scan_4x16[64] = {
    0, 16, 1, 32, 17, 2, 48, 33, 18, 3, 49, 34, 19, 4, 50, 35, 20, 5, 51, 36, 21, 6, 52, 37, 
    22, 7, 53, 38, 23, 8, 54, 39, 24, 9, 55, 40, 25, 10, 56, 41, 26, 11, 57, 42, 27, 12, 58, 
    43, 28, 13, 59, 44, 29, 14, 60, 45, 30, 15, 61, 46, 31, 62, 47, 63
};

static const stbv_u16 stbv_av1_scan_16x4[64] = {
    0, 1, 4, 2, 5, 8, 3, 6, 9, 12, 7, 10, 13, 16, 11, 14, 17, 20, 15, 18, 21, 24, 19, 22, 25, 
    28, 23, 26, 29, 32, 27, 30, 33, 36, 31, 34, 37, 40, 35, 38, 41, 44, 39, 42, 45, 48, 43, 
    46, 49, 52, 47, 50, 53, 56, 51, 54, 57, 60, 55, 58, 61, 59, 62, 63
};

static const stbv_u16 stbv_av1_scan_8x32[256] = {
    0, 32, 1, 64, 33, 2, 96, 65, 34, 3, 128, 97, 66, 35, 4, 160, 129, 98, 67, 36, 5, 192, 161, 
    130, 99, 68, 37, 6, 224, 193, 162, 131, 100, 69, 38, 7, 225, 194, 163, 132, 101, 70, 39, 
    8, 226, 195, 164, 133, 102, 71, 40, 9, 227, 196, 165, 134, 103, 72, 41, 10, 228, 197, 166, 
    135, 104, 73, 42, 11, 229, 198, 167, 136, 105, 74, 43, 12, 230, 199, 168, 137, 106, 75, 
    44, 13, 231, 200, 169, 138, 107, 76, 45, 14, 232, 201, 170, 139, 108, 77, 46, 15, 233, 
    202, 171, 140, 109, 78, 47, 16, 234, 203, 172, 141, 110, 79, 48, 17, 235, 204, 173, 142, 
    111, 80, 49, 18, 236, 205, 174, 143, 112, 81, 50, 19, 237, 206, 175, 144, 113, 82, 51, 
    20, 238, 207, 176, 145, 114, 83, 52, 21, 239, 208, 177, 146, 115, 84, 53, 22, 240, 209, 
    178, 147, 116, 85, 54, 23, 241, 210, 179, 148, 117, 86, 55, 24, 242, 211, 180, 149, 118, 
    87, 56, 25, 243, 212, 181, 150, 119, 88, 57, 26, 244, 213, 182, 151, 120, 89, 58, 27, 245, 
    214, 183, 152, 121, 90, 59, 28, 246, 215, 184, 153, 122, 91, 60, 29, 247, 216, 185, 154, 
    123, 92, 61, 30, 248, 217, 186, 155, 124, 93, 62, 31, 249, 218, 187, 156, 125, 94, 63, 
    250, 219, 188, 157, 126, 95, 251, 220, 189, 158, 127, 252, 221, 190, 159, 253, 222, 191, 
    254, 223, 255
};

static const stbv_u16 stbv_av1_scan_32x8[256] = {
    0, 1, 8, 2, 9, 16, 3, 10, 17, 24, 4, 11, 18, 25, 32, 5, 12, 19, 26, 33, 40, 6, 13, 20, 
    27, 34, 41, 48, 7, 14, 21, 28, 35, 42, 49, 56, 15, 22, 29, 36, 43, 50, 57, 64, 23, 30, 
    37, 44, 51, 58, 65, 72, 31, 38, 45, 52, 59, 66, 73, 80, 39, 46, 53, 60, 67, 74, 81, 88, 
    47, 54, 61, 68, 75, 82, 89, 96, 55, 62, 69, 76, 83, 90, 97, 104, 63, 70, 77, 84, 91, 98, 
    105, 112, 71, 78, 85, 92, 99, 106, 113, 120, 79, 86, 93, 100, 107, 114, 121, 128, 87, 94, 
    101, 108, 115, 122, 129, 136, 95, 102, 109, 116, 123, 130, 137, 144, 103, 110, 117, 124, 
    131, 138, 145, 152, 111, 118, 125, 132, 139, 146, 153, 160, 119, 126, 133, 140, 147, 154, 
    161, 168, 127, 134, 141, 148, 155, 162, 169, 176, 135, 142, 149, 156, 163, 170, 177, 184, 
    143, 150, 157, 164, 171, 178, 185, 192, 151, 158, 165, 172, 179, 186, 193, 200, 159, 166, 
    173, 180, 187, 194, 201, 208, 167, 174, 181, 188, 195, 202, 209, 216, 175, 182, 189, 196, 
    203, 210, 217, 224, 183, 190, 197, 204, 211, 218, 225, 232, 191, 198, 205, 212, 219, 226, 
    233, 240, 199, 206, 213, 220, 227, 234, 241, 248, 207, 214, 221, 228, 235, 242, 249, 215, 
    222, 229, 236, 243, 250, 223, 230, 237, 244, 251, 231, 238, 245, 252, 239, 246, 253, 247, 
    254, 255
};

static const stbv_u16 *const stbv_av1_scan_rect[STBV_AV1_N_TX_SIZES] = {
    stbv_av1_scan_4x4,  stbv_av1_scan_8x8,    stbv_av1_scan_16x16,
    stbv_av1_scan_32x32, stbv_av1_scan_32x32,
    stbv_av1_scan_4x8,  stbv_av1_scan_8x4,    stbv_av1_scan_8x16,
    stbv_av1_scan_16x8, stbv_av1_scan_16x32,  stbv_av1_scan_32x16,
    stbv_av1_scan_32x32, stbv_av1_scan_32x32,
    stbv_av1_scan_4x16, stbv_av1_scan_16x4,   stbv_av1_scan_8x32,
    stbv_av1_scan_32x8, stbv_av1_scan_16x32,  stbv_av1_scan_32x16
};

/* Decode one 4/8/16/32 (square or rectangular) transform.
 *
 * This follows dav1d's decode_coefs() representation closely.  cf[] is used
 * as a temporary linked list: bits 11.. carry the coefficient token and the
 * low 10 bits carry the next non-zero scan position.  After the syntax has
 * been consumed the list is walked again to read signs and dequantize.
 *
 * Quantization matrices are intentionally not handled here yet; the caller
 * supplies the scalar DC/AC dequantizers and dq_shift.
 *
 * tx is a dav1d RectTxfmSize value (stbv_av1_tx_dims index), tx_class is
 * STBV_AV1_TX_CLASS_* and chroma is 0 for luma, 1 for a chroma plane.
 */
static int stbv_av1_decode_coeffs_square(struct stb_av1_msac *msac,
                                          stbv_av1_cdf *cdf,
                                          int tx, int chroma, int tx_class,
                                          int dq_dc, int dq_ac,
                                          int dq_shift, int skip_ctx, int dc_sign_ctx,
                                          int bpc,
                                          stbv_i32 *cf,
                                          stbv_u8 *res_ctx_out)
{
    const stbv_u16 *scan;
    stbv_u8 levels[34 * 34];
    unsigned area, slw, slh, szctx, eob, eob_bin, is1d;
    unsigned x, y, rc, i, ctx, tok, mag;
    unsigned cul_level, dc_sign_level;
    int dc_tok, dc_sign, dc_dq, txctx, nonsquare;
    const unsigned char (*lo_ctx)[5];
    stbv_u16 *eob_bin_cdf;
    stbv_u16 *eob_hi_cdf;
    stbv_u16 *eob_cdf;
    stbv_u16 *lo_cdf;
    stbv_u16 *hi_cdf;
    stbv_u16 *dc_sign_cdf;
unsigned stride, shift, shift2, mask;
unsigned char *level;
/* Large enough for any bit depth; final pixel clipping happens
 * in itxfm_add (iclip_pixel). */
int cf_max = ~(~127U << bpc);

    if (tx < 0 || tx >= STBV_AV1_N_TX_SIZES)
        return -1;

    nonsquare = tx >= STBV_AV1_TX_4X8;
    lo_ctx = stbv_av1_lo_ctx_offsets[nonsquare + (tx & nonsquare)];
    txctx = stbv_av1_tx_dims[tx].ctx;
    slw = (unsigned)stbv_av1_tx_dims[tx].lw;
    slh = (unsigned)stbv_av1_tx_dims[tx].lh;
    if (slw > 3U) slw = 3U;
    if (slh > 3U) slh = 3U;

    switch (tx) {
    case STBV_AV1_TX_4X4:   scan = stbv_av1_scan_4x4;   break;
    case STBV_AV1_TX_8X8:   scan = stbv_av1_scan_8x8;   break;
    case STBV_AV1_TX_16X16: scan = stbv_av1_scan_16x16; break;
    case STBV_AV1_TX_32X32: scan = stbv_av1_scan_32x32; break;
    case STBV_AV1_TX_64X64: scan = stbv_av1_scan_32x32; break;
    default:
        scan = stbv_av1_scan_rect[tx];
        break;
    }

    if (txctx < 0) txctx = 0;
    if (txctx > 4) txctx = 4;
    if (skip_ctx < 0) skip_ctx = 0;
    if (skip_ctx > 12) skip_ctx = 12;
    if (dc_sign_ctx < 0) dc_sign_ctx = 0;
    if (dc_sign_ctx > 2) dc_sign_ctx = 2;

area = (4U << slw) * (4U << slh);
szctx = slw + slh;
is1d = tx_class != 0;
    memset(cf, 0, area * sizeof(*cf));
    memset(levels, 0, sizeof(levels));

    /* Coefficient skip is decoded by the caller.  Do not consume it here. */
    (void)skip_ctx;

    /* eob_bin_{16..1024}.  The first two dimensions are chroma and 1-D. */
    switch (szctx) {
    case 0:
        eob_bin_cdf = cdf->coef + 130U +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 1:
        eob_bin_cdf = cdf->coef + 162U +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 2:
        eob_bin_cdf = cdf->coef + 194U +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 3:
        eob_bin_cdf = cdf->coef + 226U +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 4:
        eob_bin_cdf = cdf->coef + 258U +
                      ((unsigned)chroma * 2U + is1d) * 16U;
        break;
    case 5:
        eob_bin_cdf = cdf->coef + 322U + (unsigned)chroma * 16U;
        break;
    default:
        eob_bin_cdf = cdf->coef + 354U + (unsigned)chroma * 16U;
        break;
    }

    eob = stb_av1_msac_symbol(msac, eob_bin_cdf, 4U + szctx);
    if (eob > 1U) {
        eob_bin = eob - 2U;
        /* eob_hi_bit[N_TX_SIZES][2][9][2] */
        eob_hi_cdf = cdf->coef + 2858U + (unsigned)txctx * 36U +
                     (unsigned)chroma * 18U + eob_bin * 2U;
        eob = ((stb_av1_msac_bool_adapt(msac, eob_hi_cdf) | 2U) << eob_bin) |
              stb_av1_msac_bools(msac, eob_bin);
    }
    if (eob > area)
        return -2;

    /* eob_base_tok[N_TX_SIZES][2][4][4] */
    eob_cdf = cdf->coef + 386U + (unsigned)txctx * 32U +
              (unsigned)chroma * 16U;
    /* base_tok[N_TX_SIZES][2][41][4] */
    lo_cdf = cdf->coef + 546U + (unsigned)txctx * 328U +
             (unsigned)chroma * 164U;
    /* br_tok[min(txctx,3)][2][21][4] */
    hi_cdf = cdf->coef + 2186U + (unsigned)(txctx > 3 ? 3 : txctx) * 168U +
             (unsigned)chroma * 84U;

    /* The level scratch layout is exactly the one used by dav1d: for 2-D
     * transforms stride is the coefficient-grid width (4 << slh), which for
     * TX_64X64 is 32 while the real transform is 64 samples wide; H/V use
     * stride 16. */
    if (tx_class == 0) {
        stride = 4U << slh;
        shift = slh + 2U;
        shift2 = 0;
        mask = (4U << slh) - 1U;
    } else if (tx_class == 1) {
        stride = 16U;
        shift = slh + 2U;
        shift2 = 0;
        mask = (4U << slh) - 1U;
    } else {
        stride = 16U;
        shift = slw + 2U;
        shift2 = slh + 2U;
        mask = (4U << slw) - 1U;
    }
    memset(levels, 0, (size_t)(stride * ((4U << slw) + 2U)));

    /* EOB coefficient and descending AC scan, only when eob > 0. */
    if (eob) {
        ctx = 1U + (eob > (2U << szctx)) + (eob > (4U << szctx));
        tok = 1U + stb_av1_msac_symbol(msac, eob_cdf + ctx * 4U, 2U);
        dc_tok = (int)tok;

        if (tx_class == 0) {
            rc = scan[eob];
            x = rc >> shift;
            y = rc & mask;
        } else if (tx_class == 1) {
            x = eob & mask;
            y = eob >> shift;
            rc = eob;
        } else {
            x = eob & mask;
            y = eob >> shift;
            rc = (x << shift2) | y;
        }

        if (tok == 3U) {
            ctx = (tx_class == 0 ? ((x | y) > 1U) : (y != 0U)) ? 14U : 7U;
            tok = stbv_av1_coef_hi_tok(msac, hi_cdf + ctx * 4U);
            dc_tok = (int)tok;
            cf[rc] = tok << 11;
            level = levels + x * stride + y;
            *level = (stbv_u8)(tok + (3U << 6));
        } else {
            cf[rc] = tok << 11;
            level = levels + x * stride + y;
            *level = (stbv_u8)(tok * 0x41U);
        }

        /* Descending AC scan.  The linked-list encoding below is the important
         * detail: it lets the later sign/dequant pass visit only non-zero
         * values. */
        for (i = eob - 1U; i > 0U; i--) {
            unsigned rc_i;

            if (tx_class == 0) {
                rc_i = scan[i];
                x = rc_i >> shift;
                y = rc_i & mask;
            } else if (tx_class == 1) {
                x = i & mask;
                y = i >> shift;
                rc_i = i;
            } else {
                x = i & mask;
                y = i >> shift;
                rc_i = (x << shift2) | y;
            }

            level = levels + x * stride + y;
            ctx = stbv_av1_coef_lo_ctx(levels + x * stride + y,
                                        &mag, x, y, stride, tx_class, lo_ctx);
            tok = stb_av1_msac_symbol(msac, lo_cdf + ctx * 4U, 3U);

            if (tok == 3U) {
                /* dav1d ORs x into y for 2-D and compares y > 1; H/V compares
                 * y != 0.  mag is masked to 63 before the binarization
                 * context. */
                mag &= 63U;
                ctx = (tx_class == 0 ? ((x | y) > 1U) : (y != 0U)) ? 14U : 7U;
                ctx += mag > 12U ? 6U : (mag + 1U) >> 1;
                tok = stbv_av1_coef_hi_tok(msac, hi_cdf + ctx * 4U);
                *level = (stbv_u8)(tok + (3U << 6));
                cf[rc_i] = (tok << 11) | rc;
                rc = rc_i;
            } else {
                /* dav1d's packed expression is equivalent to storing zero or
                 * the token in bits 11+, with rc in the low ten bits when
                 * non-zero. */
                unsigned packed = tok * 0x17ff41U;
                *level = (stbv_u8)packed;
                tok = (packed >> 9) & (rc + ~0x7ffU);
                if (tok)
                    rc = rc_i;
                cf[rc_i] = tok;
            }
        }
    } else {
        rc = 0;
    }

    /* DC token.  For eob > 0 the DC token was already decoded by the EOB
     * section above (eob_base_tok with the eob-derived context).  For
     * dc-only (eob == 0) blocks dav1d reads it from eob_base_tok[0]. */
    if (eob) {
        /* DC token after the AC scan: dav1d decodes it from base_tok with
         * ctx = 0 for 2-D (or a lo_ctx-derived context for H/V), and a hi
         * token when the symbol is 3, using the magnitude of the
         * neighbours of the DC position. */
        ctx = (tx_class == 0U) ? 0U : stbv_av1_coef_lo_ctx(
            levels, &mag, 0, 0, stride, tx_class, lo_ctx);
        tok = stb_av1_msac_symbol(msac, lo_cdf + ctx * 4U, 3U);
        dc_tok = (int)tok;
        if (tok == 3U) {
            if (tx_class == 0U)
                mag = levels[0 * stride + 1U] + levels[1 * stride + 0U]
                    + levels[1 * stride + 1U];
            mag &= 63U;
            ctx = mag > 12U ? 6U : (mag + 1U) >> 1U;
            dc_tok = (int)stbv_av1_coef_hi_tok(msac, hi_cdf + ctx * 4U);
        }
    } else {
        /* dc-only (eob_bin == 0): dav1d evaluates the eob context with
         * eob == 1 (base_tok row 1), and escapes via the br context of
         * the DC position (x == y == 0 -> row 7).  One coefficient, the
         * DC, is present. */
        tok = stb_av1_msac_symbol(msac, eob_cdf, 2U);
        dc_tok = (int)(1U + tok);
        if (tok == 2U)
            dc_tok = (int)stbv_av1_coef_hi_tok(msac, hi_cdf);
        eob = 1; /* DC coefficient is present */
    }

    /* The final rc is the first non-zero coefficient in scan order.  dav1d's
     * residual pass follows the linked list encoded above. */
    cul_level = 0;
    dc_sign_level = 1U << 6;

    if (!dc_tok) {
        dc_sign_level = 1U << 6;
    } else {
        dc_sign_cdf = cdf->coef + 3038U + (chroma != 0) * 6U +
                      (unsigned)dc_sign_ctx * 2U;
        dc_sign = (int)stb_av1_msac_bool_adapt(msac, dc_sign_cdf);
        dc_sign_level = (dc_sign - 1) & (2 << 6);

        dc_dq = dq_dc;
        {
        int dc_tok_orig = dc_tok;
        if (dc_tok == 15) {
            dc_tok = (int)stbv_av1_coef_golomb(msac) + 15;
            dc_tok &= 0xfffff;
            dc_dq = (int)(((stbv_u32)dc_dq * (stbv_u32)dc_tok) & 0xffffffU);
        } else {
            dc_dq *= dc_tok;
        }
        dc_dq >>= dq_shift;
        if (dc_dq > cf_max + dc_sign)
            dc_dq = cf_max + dc_sign;
        cf[0] = dc_sign ? -dc_dq : dc_dq;
        cul_level = (unsigned)dc_tok;
        }
    }

    if (rc) {
        do {
            int sign = (int)stb_av1_msac_bool_equi(msac);
            unsigned rc_tok = (unsigned)cf[rc];
            unsigned vtok;
            int dq;

            if (rc_tok >= (15U << 11)) {
                vtok = stbv_av1_coef_golomb(msac) + 15U;
                vtok &= 0xfffffU;
                /* Escape residuals wrap the product to 24 bits before the
                 * right-shift, exactly like dav1d's ac_noqm path. */
                dq = (int)((((stbv_u32)dq_ac * (stbv_u32)vtok) & 0xffffffU) >>
                           (unsigned)dq_shift);
            } else {
                vtok = rc_tok >> 11;
                dq = (int)(((stbv_u32)dq_ac * (stbv_u32)vtok) >>
                           (unsigned)dq_shift);
            }
            if (dq > cf_max + sign)
                dq = cf_max + sign;
            cul_level += vtok;
            cf[rc] = sign ? -dq : dq;
            rc = rc_tok & 0x3ffU;
        } while (rc);
    }

    /* res_ctx = min(cul_level,63) | dc_sign_level, exactly like dav1d's
     * decode_coefs() tail. */
    if (res_ctx_out)
        *res_ctx_out = (stbv_u8)((cul_level < 63U ? cul_level : 63U) |
                                 dc_sign_level);
    return (int)eob;
}

#endif /* STB_AV1_COEF_H */

/* ===== stb_av1_itx.h ===== */
/*
 * Scalar AV1 inverse transforms (all rect sizes and tx types) for the
 * minimal intra still-picture decoder, following dav1d 1.5.4's
 * inv_txfm_add_c() exactly.
 *
 * Copyright © 2018-2019, VideoLAN and dav1d authors
 * Copyright © 2018-2019, Two Orioles, LLC
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_ITX_H
#define STB_AV1_ITX_H

#ifndef STBV_I32_DEFINED
#error "stb_av1_itx.h requires stbv_i32"
#endif

#include <string.h>

/* ===== stb_av1_itx1d.h ===== */
/* Scalar inverse-transform routines adapted from dav1d 1.5.4.
 * Copyright © 2018-2019, VideoLAN and dav1d authors
 * Copyright © 2018-2019, Two Orioles, LLC
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_ITX1D_H
#define STB_AV1_ITX1D_H
#include <assert.h>
#ifndef STBV_I32_DEFINED
#error "stb_av1_itx1d.h requires stbv_i32"
#endif
#define STBV_AV1_CLIP(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
#define CLIP(v) STBV_AV1_CLIP((v),min,max)
static void
inv_dct4_1d_internal_c(stbv_i32 * c, const int stride,
                       const int min, const int max, const int tx64)
{
    const int in0 = c[0 * stride], in1 = c[1 * stride];
    int t0, t1, t2, t3;

    assert(stride > 0);
    if (tx64) {
        t0 = t1 = (in0 * 181 + 128) >> 8;
        t2 = (in1 * 1567 + 2048) >> 12;
        t3 = (in1 * 3784 + 2048) >> 12;
    } else {
        const int in2 = c[2 * stride], in3 = c[3 * stride];

        t0 = ((in0 + in2) * 181 + 128) >> 8;
        t1 = ((in0 - in2) * 181 + 128) >> 8;
        t2 = ((in1 *  1567         - in3 * (3784 - 4096) + 2048) >> 12) - in3;
        t3 = ((in1 * (3784 - 4096) + in3 *  1567         + 2048) >> 12) + in1;
    }

    c[0 * stride] = CLIP(t0 + t3);
    c[1 * stride] = CLIP(t1 + t2);
    c[2 * stride] = CLIP(t1 - t2);
    c[3 * stride] = CLIP(t0 - t3);
}

static void inv_dct4_1d_c(stbv_i32 * c, const int stride,
                          const int min, const int max)
{
    inv_dct4_1d_internal_c(c, stride, min, max, 0);
}

static void
inv_dct8_1d_internal_c(stbv_i32 * c, const int stride,
                       const int min, const int max, const int tx64)
{
    const int in1 = c[1 * stride], in3 = c[3 * stride];
    int t4a, t5a, t6a, t7a;

    assert(stride > 0);
    inv_dct4_1d_internal_c(c, stride << 1, min, max, tx64);

    if (tx64) {
        t4a = (in1 *   799 + 2048) >> 12;
        t5a = (in3 * -2276 + 2048) >> 12;
        t6a = (in3 *  3406 + 2048) >> 12;
        t7a = (in1 *  4017 + 2048) >> 12;
    } else     {
        const int in5 = c[5 * stride], in7 = c[7 * stride];

        t4a = ((in1 *   799         - in7 * (4017 - 4096) + 2048) >> 12) - in7;
        t5a =  (in5 *  1703         - in3 *  1138         + 1024) >> 11;
        t6a =  (in5 *  1138         + in3 *  1703         + 1024) >> 11;
        t7a = ((in1 * (4017 - 4096) + in7 *  799          + 2048) >> 12) + in1;
    }

    {
        /* C90: all declarations first; t4/t7 must read the pre-update
         * t5a/t6a, so compute them before the in-place updates below. */
        const int t0 = c[0 * stride];
        const int t1 = c[2 * stride];
        const int t2 = c[4 * stride];
        const int t3 = c[6 * stride];
        const int t4 = CLIP(t4a + t5a);
        const int t7 = CLIP(t7a + t6a);
        int t5, t6;

        t5a = CLIP(t4a - t5a);
        t6a = CLIP(t7a - t6a);
        t5  = ((t6a - t5a) * 181 + 128) >> 8;
        t6  = ((t6a + t5a) * 181 + 128) >> 8;

        c[0 * stride] = CLIP(t0 + t7);
        c[1 * stride] = CLIP(t1 + t6);
        c[2 * stride] = CLIP(t2 + t5);
        c[3 * stride] = CLIP(t3 + t4);
        c[4 * stride] = CLIP(t3 - t4);
        c[5 * stride] = CLIP(t2 - t5);
        c[6 * stride] = CLIP(t1 - t6);
        c[7 * stride] = CLIP(t0 - t7);
    }
}

static void inv_dct8_1d_c(stbv_i32 * c, const int stride,
                          const int min, const int max)
{
    inv_dct8_1d_internal_c(c, stride, min, max, 0);
}

static void
inv_dct16_1d_internal_c(stbv_i32 * c, const int stride,
                        const int min, const int max, int tx64)
{
    const int in1 = c[1 * stride], in3 = c[3 * stride];
    const int in5 = c[5 * stride], in7 = c[7 * stride];
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t8, t9, t10, t11, t12, t13, t14, t15;
    int t8a, t9a, t10a, t11a, t12a, t13a, t14a, t15a;

    assert(stride > 0);
    inv_dct8_1d_internal_c(c, stride << 1, min, max, tx64);
    if (tx64) {
        t8a  = (in1 *   401 + 2048) >> 12;
        t9a  = (in7 * -2598 + 2048) >> 12;
        t10a = (in5 *  1931 + 2048) >> 12;
        t11a = (in3 * -1189 + 2048) >> 12;
        t12a = (in3 *  3920 + 2048) >> 12;
        t13a = (in5 *  3612 + 2048) >> 12;
        t14a = (in7 *  3166 + 2048) >> 12;
        t15a = (in1 *  4076 + 2048) >> 12;
    } else {
        const int in9  = c[ 9 * stride], in11 = c[11 * stride];
        const int in13 = c[13 * stride], in15 = c[15 * stride];

        t8a  = ((in1  *   401         - in15 * (4076 - 4096) + 2048) >> 12) - in15;
        t9a  =  (in9  *  1583         - in7  *  1299         + 1024) >> 11;
        t10a = ((in5  *  1931         - in11 * (3612 - 4096) + 2048) >> 12) - in11;
        t11a = ((in13 * (3920 - 4096) - in3  *  1189         + 2048) >> 12) + in13;
        t12a = ((in13 *  1189         + in3  * (3920 - 4096) + 2048) >> 12) + in3;
        t13a = ((in5  * (3612 - 4096) + in11 *  1931         + 2048) >> 12) + in5;
        t14a =  (in9  *  1299         + in7  *  1583         + 1024) >> 11;
        t15a = ((in1  * (4076 - 4096) + in15 *   401         + 2048) >> 12) + in1;
    }

    t8  = CLIP(t8a  + t9a);
    t9  = CLIP(t8a  - t9a);
    t10 = CLIP(t11a - t10a);
    t11 = CLIP(t11a + t10a);
    t12 = CLIP(t12a + t13a);
    t13 = CLIP(t12a - t13a);
    t14 = CLIP(t15a - t14a);
    t15 = CLIP(t15a + t14a);

    t9a  = ((  t14 *  1567         - t9  * (3784 - 4096)  + 2048) >> 12) - t9;
    t14a = ((  t14 * (3784 - 4096) + t9  *  1567          + 2048) >> 12) + t14;
    t10a = ((-(t13 * (3784 - 4096) + t10 *  1567)         + 2048) >> 12) - t13;
    t13a = ((  t13 *  1567         - t10 * (3784 - 4096)  + 2048) >> 12) - t10;

    t8a  = CLIP(t8   + t11);
    t9   = CLIP(t9a  + t10a);
    t10  = CLIP(t9a  - t10a);
    t11a = CLIP(t8   - t11);
    t12a = CLIP(t15  - t12);
    t13  = CLIP(t14a - t13a);
    t14  = CLIP(t14a + t13a);
    t15a = CLIP(t15  + t12);

    t10a = ((t13  - t10)  * 181 + 128) >> 8;
    t13a = ((t13  + t10)  * 181 + 128) >> 8;
    t11  = ((t12a - t11a) * 181 + 128) >> 8;
    t12  = ((t12a + t11a) * 181 + 128) >> 8;

    t0 = c[ 0 * stride];
    t1 = c[ 2 * stride];
    t2 = c[ 4 * stride];
    t3 = c[ 6 * stride];
    t4 = c[ 8 * stride];
    t5 = c[10 * stride];
    t6 = c[12 * stride];
    t7 = c[14 * stride];

    c[ 0 * stride] = CLIP(t0 + t15a);
    c[ 1 * stride] = CLIP(t1 + t14);
    c[ 2 * stride] = CLIP(t2 + t13a);
    c[ 3 * stride] = CLIP(t3 + t12);
    c[ 4 * stride] = CLIP(t4 + t11);
    c[ 5 * stride] = CLIP(t5 + t10a);
    c[ 6 * stride] = CLIP(t6 + t9);
    c[ 7 * stride] = CLIP(t7 + t8a);
    c[ 8 * stride] = CLIP(t7 - t8a);
    c[ 9 * stride] = CLIP(t6 - t9);
    c[10 * stride] = CLIP(t5 - t10a);
    c[11 * stride] = CLIP(t4 - t11);
    c[12 * stride] = CLIP(t3 - t12);
    c[13 * stride] = CLIP(t2 - t13a);
    c[14 * stride] = CLIP(t1 - t14);
    c[15 * stride] = CLIP(t0 - t15a);
}

static void inv_dct16_1d_c(stbv_i32 * c, const int stride,
                           const int min, const int max)
{
    inv_dct16_1d_internal_c(c, stride, min, max, 0);
}

static void
inv_dct32_1d_internal_c(stbv_i32 * c, const int stride,
                        const int min, const int max, const int tx64)
{
    const int in1  = c[ 1 * stride], in3  = c[ 3 * stride];
    const int in5  = c[ 5 * stride], in7  = c[ 7 * stride];
    const int in9  = c[ 9 * stride], in11 = c[11 * stride];
    const int in13 = c[13 * stride], in15 = c[15 * stride];
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t8, t9, t10, t11, t12, t13, t14, t15;
    int t16, t17, t18, t19, t20, t21, t22, t23;
    int t24, t25, t26, t27, t28, t29, t30, t31;
    int t16a, t17a, t18a, t19a, t20a, t21a, t22a, t23a;
    int t24a, t25a, t26a, t27a, t28a, t29a, t30a, t31a;

    assert(stride > 0);
    inv_dct16_1d_internal_c(c, stride << 1, min, max, tx64);
    if (tx64) {
        t16a = (in1  *   201 + 2048) >> 12;
        t17a = (in15 * -2751 + 2048) >> 12;
        t18a = (in9  *  1751 + 2048) >> 12;
        t19a = (in7  * -1380 + 2048) >> 12;
        t20a = (in5  *   995 + 2048) >> 12;
        t21a = (in11 * -2106 + 2048) >> 12;
        t22a = (in13 *  2440 + 2048) >> 12;
        t23a = (in3  *  -601 + 2048) >> 12;
        t24a = (in3  *  4052 + 2048) >> 12;
        t25a = (in13 *  3290 + 2048) >> 12;
        t26a = (in11 *  3513 + 2048) >> 12;
        t27a = (in5  *  3973 + 2048) >> 12;
        t28a = (in7  *  3857 + 2048) >> 12;
        t29a = (in9  *  3703 + 2048) >> 12;
        t30a = (in15 *  3035 + 2048) >> 12;
        t31a = (in1  *  4091 + 2048) >> 12;
    } else {
        const int in17 = c[17 * stride], in19 = c[19 * stride];
        const int in21 = c[21 * stride], in23 = c[23 * stride];
        const int in25 = c[25 * stride], in27 = c[27 * stride];
        const int in29 = c[29 * stride], in31 = c[31 * stride];

        t16a = ((in1  *   201         - in31 * (4091 - 4096) + 2048) >> 12) - in31;
        t17a = ((in17 * (3035 - 4096) - in15 *  2751         + 2048) >> 12) + in17;
        t18a = ((in9  *  1751         - in23 * (3703 - 4096) + 2048) >> 12) - in23;
        t19a = ((in25 * (3857 - 4096) - in7  *  1380         + 2048) >> 12) + in25;
        t20a = ((in5  *   995         - in27 * (3973 - 4096) + 2048) >> 12) - in27;
        t21a = ((in21 * (3513 - 4096) - in11 *  2106         + 2048) >> 12) + in21;
        t22a =  (in13 *  1220         - in19 *  1645         + 1024) >> 11;
        t23a = ((in29 * (4052 - 4096) - in3  *   601         + 2048) >> 12) + in29;
        t24a = ((in29 *   601         + in3  * (4052 - 4096) + 2048) >> 12) + in3;
        t25a =  (in13 *  1645         + in19 *  1220         + 1024) >> 11;
        t26a = ((in21 *  2106         + in11 * (3513 - 4096) + 2048) >> 12) + in11;
        t27a = ((in5  * (3973 - 4096) + in27 *   995         + 2048) >> 12) + in5;
        t28a = ((in25 *  1380         + in7  * (3857 - 4096) + 2048) >> 12) + in7;
        t29a = ((in9  * (3703 - 4096) + in23 *  1751         + 2048) >> 12) + in9;
        t30a = ((in17 *  2751         + in15 * (3035 - 4096) + 2048) >> 12) + in15;
        t31a = ((in1  * (4091 - 4096) + in31 *   201         + 2048) >> 12) + in1;
    }

    t16 = CLIP(t16a + t17a);
    t17 = CLIP(t16a - t17a);
    t18 = CLIP(t19a - t18a);
    t19 = CLIP(t19a + t18a);
    t20 = CLIP(t20a + t21a);
    t21 = CLIP(t20a - t21a);
    t22 = CLIP(t23a - t22a);
    t23 = CLIP(t23a + t22a);
    t24 = CLIP(t24a + t25a);
    t25 = CLIP(t24a - t25a);
    t26 = CLIP(t27a - t26a);
    t27 = CLIP(t27a + t26a);
    t28 = CLIP(t28a + t29a);
    t29 = CLIP(t28a - t29a);
    t30 = CLIP(t31a - t30a);
    t31 = CLIP(t31a + t30a);

    t17a = ((  t30 *   799         - t17 * (4017 - 4096)  + 2048) >> 12) - t17;
    t30a = ((  t30 * (4017 - 4096) + t17 *   799          + 2048) >> 12) + t30;
    t18a = ((-(t29 * (4017 - 4096) + t18 *   799)         + 2048) >> 12) - t29;
    t29a = ((  t29 *   799         - t18 * (4017 - 4096)  + 2048) >> 12) - t18;
    t21a =  (  t26 *  1703         - t21 *  1138          + 1024) >> 11;
    t26a =  (  t26 *  1138         + t21 *  1703          + 1024) >> 11;
    t22a =  (-(t25 *  1138         + t22 *  1703        ) + 1024) >> 11;
    t25a =  (  t25 *  1703         - t22 *  1138          + 1024) >> 11;

    t16a = CLIP(t16  + t19);
    t17  = CLIP(t17a + t18a);
    t18  = CLIP(t17a - t18a);
    t19a = CLIP(t16  - t19);
    t20a = CLIP(t23  - t20);
    t21  = CLIP(t22a - t21a);
    t22  = CLIP(t22a + t21a);
    t23a = CLIP(t23  + t20);
    t24a = CLIP(t24  + t27);
    t25  = CLIP(t25a + t26a);
    t26  = CLIP(t25a - t26a);
    t27a = CLIP(t24  - t27);
    t28a = CLIP(t31  - t28);
    t29  = CLIP(t30a - t29a);
    t30  = CLIP(t30a + t29a);
    t31a = CLIP(t31  + t28);

    t18a = ((  t29  *  1567         - t18  * (3784 - 4096)  + 2048) >> 12) - t18;
    t29a = ((  t29  * (3784 - 4096) + t18  *  1567          + 2048) >> 12) + t29;
    t19  = ((  t28a *  1567         - t19a * (3784 - 4096)  + 2048) >> 12) - t19a;
    t28  = ((  t28a * (3784 - 4096) + t19a *  1567          + 2048) >> 12) + t28a;
    t20  = ((-(t27a * (3784 - 4096) + t20a *  1567)         + 2048) >> 12) - t27a;
    t27  = ((  t27a *  1567         - t20a * (3784 - 4096)  + 2048) >> 12) - t20a;
    t21a = ((-(t26  * (3784 - 4096) + t21  *  1567)         + 2048) >> 12) - t26;
    t26a = ((  t26  *  1567         - t21  * (3784 - 4096)  + 2048) >> 12) - t21;

    t16  = CLIP(t16a + t23a);
    t17a = CLIP(t17  + t22);
    t18  = CLIP(t18a + t21a);
    t19a = CLIP(t19  + t20);
    t20a = CLIP(t19  - t20);
    t21  = CLIP(t18a - t21a);
    t22a = CLIP(t17  - t22);
    t23  = CLIP(t16a - t23a);
    t24  = CLIP(t31a - t24a);
    t25a = CLIP(t30  - t25);
    t26  = CLIP(t29a - t26a);
    t27a = CLIP(t28  - t27);
    t28a = CLIP(t28  + t27);
    t29  = CLIP(t29a + t26a);
    t30a = CLIP(t30  + t25);
    t31  = CLIP(t31a + t24a);

    t20  = ((t27a - t20a) * 181 + 128) >> 8;
    t27  = ((t27a + t20a) * 181 + 128) >> 8;
    t21a = ((t26  - t21 ) * 181 + 128) >> 8;
    t26a = ((t26  + t21 ) * 181 + 128) >> 8;
    t22  = ((t25a - t22a) * 181 + 128) >> 8;
    t25  = ((t25a + t22a) * 181 + 128) >> 8;
    t23a = ((t24  - t23 ) * 181 + 128) >> 8;
    t24a = ((t24  + t23 ) * 181 + 128) >> 8;

    t0  = c[ 0 * stride];
    t1  = c[ 2 * stride];
    t2  = c[ 4 * stride];
    t3  = c[ 6 * stride];
    t4  = c[ 8 * stride];
    t5  = c[10 * stride];
    t6  = c[12 * stride];
    t7  = c[14 * stride];
    t8  = c[16 * stride];
    t9  = c[18 * stride];
    t10 = c[20 * stride];
    t11 = c[22 * stride];
    t12 = c[24 * stride];
    t13 = c[26 * stride];
    t14 = c[28 * stride];
    t15 = c[30 * stride];

    c[ 0 * stride] = CLIP(t0  + t31);
    c[ 1 * stride] = CLIP(t1  + t30a);
    c[ 2 * stride] = CLIP(t2  + t29);
    c[ 3 * stride] = CLIP(t3  + t28a);
    c[ 4 * stride] = CLIP(t4  + t27);
    c[ 5 * stride] = CLIP(t5  + t26a);
    c[ 6 * stride] = CLIP(t6  + t25);
    c[ 7 * stride] = CLIP(t7  + t24a);
    c[ 8 * stride] = CLIP(t8  + t23a);
    c[ 9 * stride] = CLIP(t9  + t22);
    c[10 * stride] = CLIP(t10 + t21a);
    c[11 * stride] = CLIP(t11 + t20);
    c[12 * stride] = CLIP(t12 + t19a);
    c[13 * stride] = CLIP(t13 + t18);
    c[14 * stride] = CLIP(t14 + t17a);
    c[15 * stride] = CLIP(t15 + t16);
    c[16 * stride] = CLIP(t15 - t16);
    c[17 * stride] = CLIP(t14 - t17a);
    c[18 * stride] = CLIP(t13 - t18);
    c[19 * stride] = CLIP(t12 - t19a);
    c[20 * stride] = CLIP(t11 - t20);
    c[21 * stride] = CLIP(t10 - t21a);
    c[22 * stride] = CLIP(t9  - t22);
    c[23 * stride] = CLIP(t8  - t23a);
    c[24 * stride] = CLIP(t7  - t24a);
    c[25 * stride] = CLIP(t6  - t25);
    c[26 * stride] = CLIP(t5  - t26a);
    c[27 * stride] = CLIP(t4  - t27);
    c[28 * stride] = CLIP(t3  - t28a);
    c[29 * stride] = CLIP(t2  - t29);
    c[30 * stride] = CLIP(t1  - t30a);
    c[31 * stride] = CLIP(t0  - t31);
}

static void inv_dct32_1d_c(stbv_i32 * c, const int stride,
                           const int min, const int max)
{
    inv_dct32_1d_internal_c(c, stride, min, max, 0);
}

static void inv_dct64_1d_c(stbv_i32 * c, const int stride,
                           const int min, const int max)
{
    int t32a, t33a, t34a, t35a, t36a, t37a, t38a, t39a;
    int t40a, t41a, t42a, t43a, t44a, t45a, t46a, t47a;
    int t48a, t49a, t50a, t51a, t52a, t53a, t54a, t55a;
    int t56a, t57a, t58a, t59a, t60a, t61a, t62a, t63a;
    int t32, t33, t34, t35, t36, t37, t38, t39;
    int t40, t41, t42, t43, t44, t45, t46, t47;
    int t48, t49, t50, t51, t52, t53, t54, t55;
    int t56, t57, t58, t59, t60, t61, t62, t63;
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t8, t9, t10, t11, t12, t13, t14, t15;
    int t16, t17, t18, t19, t20, t21, t22, t23;
    int t24, t25, t26, t27, t28, t29, t30, t31;
    const int in1  = c[ 1 * stride], in3  = c[ 3 * stride];
    const int in5  = c[ 5 * stride], in7  = c[ 7 * stride];
    const int in9  = c[ 9 * stride], in11 = c[11 * stride];
    const int in13 = c[13 * stride], in15 = c[15 * stride];
    const int in17 = c[17 * stride], in19 = c[19 * stride];
    const int in21 = c[21 * stride], in23 = c[23 * stride];
    const int in25 = c[25 * stride], in27 = c[27 * stride];
    const int in29 = c[29 * stride], in31 = c[31 * stride];

    assert(stride > 0);
    inv_dct32_1d_internal_c(c, stride << 1, min, max, 1);

    t32a = (in1  *   101 + 2048) >> 12;
    t33a = (in31 * -2824 + 2048) >> 12;
    t34a = (in17 *  1660 + 2048) >> 12;
    t35a = (in15 * -1474 + 2048) >> 12;
    t36a = (in9  *   897 + 2048) >> 12;
    t37a = (in23 * -2191 + 2048) >> 12;
    t38a = (in25 *  2359 + 2048) >> 12;
    t39a = (in7  *  -700 + 2048) >> 12;
    t40a = (in5  *   501 + 2048) >> 12;
    t41a = (in27 * -2520 + 2048) >> 12;
    t42a = (in21 *  2019 + 2048) >> 12;
    t43a = (in11 * -1092 + 2048) >> 12;
    t44a = (in13 *  1285 + 2048) >> 12;
    t45a = (in19 * -1842 + 2048) >> 12;
    t46a = (in29 *  2675 + 2048) >> 12;
    t47a = (in3  *  -301 + 2048) >> 12;
    t48a = (in3  *  4085 + 2048) >> 12;
    t49a = (in29 *  3102 + 2048) >> 12;
    t50a = (in19 *  3659 + 2048) >> 12;
    t51a = (in13 *  3889 + 2048) >> 12;
    t52a = (in11 *  3948 + 2048) >> 12;
    t53a = (in21 *  3564 + 2048) >> 12;
    t54a = (in27 *  3229 + 2048) >> 12;
    t55a = (in5  *  4065 + 2048) >> 12;
    t56a = (in7  *  4036 + 2048) >> 12;
    t57a = (in25 *  3349 + 2048) >> 12;
    t58a = (in23 *  3461 + 2048) >> 12;
    t59a = (in9  *  3996 + 2048) >> 12;
    t60a = (in15 *  3822 + 2048) >> 12;
    t61a = (in17 *  3745 + 2048) >> 12;
    t62a = (in31 *  2967 + 2048) >> 12;
    t63a = (in1  *  4095 + 2048) >> 12;

    t32 = CLIP(t32a + t33a);
    t33 = CLIP(t32a - t33a);
    t34 = CLIP(t35a - t34a);
    t35 = CLIP(t35a + t34a);
    t36 = CLIP(t36a + t37a);
    t37 = CLIP(t36a - t37a);
    t38 = CLIP(t39a - t38a);
    t39 = CLIP(t39a + t38a);
    t40 = CLIP(t40a + t41a);
    t41 = CLIP(t40a - t41a);
    t42 = CLIP(t43a - t42a);
    t43 = CLIP(t43a + t42a);
    t44 = CLIP(t44a + t45a);
    t45 = CLIP(t44a - t45a);
    t46 = CLIP(t47a - t46a);
    t47 = CLIP(t47a + t46a);
    t48 = CLIP(t48a + t49a);
    t49 = CLIP(t48a - t49a);
    t50 = CLIP(t51a - t50a);
    t51 = CLIP(t51a + t50a);
    t52 = CLIP(t52a + t53a);
    t53 = CLIP(t52a - t53a);
    t54 = CLIP(t55a - t54a);
    t55 = CLIP(t55a + t54a);
    t56 = CLIP(t56a + t57a);
    t57 = CLIP(t56a - t57a);
    t58 = CLIP(t59a - t58a);
    t59 = CLIP(t59a + t58a);
    t60 = CLIP(t60a + t61a);
    t61 = CLIP(t60a - t61a);
    t62 = CLIP(t63a - t62a);
    t63 = CLIP(t63a + t62a);

    t33a = ((t33 * (4096 - 4076) + t62 *   401         + 2048) >> 12) - t33;
    t34a = ((t34 *  -401         + t61 * (4096 - 4076) + 2048) >> 12) - t61;
    t37a =  (t37 * -1299         + t58 *  1583         + 1024) >> 11;
    t38a =  (t38 * -1583         + t57 * -1299         + 1024) >> 11;
    t41a = ((t41 * (4096 - 3612) + t54 *  1931         + 2048) >> 12) - t41;
    t42a = ((t42 * -1931         + t53 * (4096 - 3612) + 2048) >> 12) - t53;
    t45a = ((t45 * -1189         + t50 * (3920 - 4096) + 2048) >> 12) + t50;
    t46a = ((t46 * (4096 - 3920) + t49 * -1189         + 2048) >> 12) - t46;
    t49a = ((t46 * -1189         + t49 * (3920 - 4096) + 2048) >> 12) + t49;
    t50a = ((t45 * (3920 - 4096) + t50 *  1189         + 2048) >> 12) + t45;
    t53a = ((t42 * (4096 - 3612) + t53 *  1931         + 2048) >> 12) - t42;
    t54a = ((t41 *  1931         + t54 * (3612 - 4096) + 2048) >> 12) + t54;
    t57a =  (t38 * -1299         + t57 *  1583         + 1024) >> 11;
    t58a =  (t37 *  1583         + t58 *  1299         + 1024) >> 11;
    t61a = ((t34 * (4096 - 4076) + t61 *   401         + 2048) >> 12) - t34;
    t62a = ((t33 *   401         + t62 * (4076 - 4096) + 2048) >> 12) + t62;

    t32a = CLIP(t32  + t35);
    t33  = CLIP(t33a + t34a);
    t34  = CLIP(t33a - t34a);
    t35a = CLIP(t32  - t35);
    t36a = CLIP(t39  - t36);
    t37  = CLIP(t38a - t37a);
    t38  = CLIP(t38a + t37a);
    t39a = CLIP(t39  + t36);
    t40a = CLIP(t40  + t43);
    t41  = CLIP(t41a + t42a);
    t42  = CLIP(t41a - t42a);
    t43a = CLIP(t40  - t43);
    t44a = CLIP(t47  - t44);
    t45  = CLIP(t46a - t45a);
    t46  = CLIP(t46a + t45a);
    t47a = CLIP(t47  + t44);
    t48a = CLIP(t48  + t51);
    t49  = CLIP(t49a + t50a);
    t50  = CLIP(t49a - t50a);
    t51a = CLIP(t48  - t51);
    t52a = CLIP(t55  - t52);
    t53  = CLIP(t54a - t53a);
    t54  = CLIP(t54a + t53a);
    t55a = CLIP(t55  + t52);
    t56a = CLIP(t56  + t59);
    t57  = CLIP(t57a + t58a);
    t58  = CLIP(t57a - t58a);
    t59a = CLIP(t56  - t59);
    t60a = CLIP(t63  - t60);
    t61  = CLIP(t62a - t61a);
    t62  = CLIP(t62a + t61a);
    t63a = CLIP(t63  + t60);

    t34a = ((t34  * (4096 - 4017) + t61  *   799         + 2048) >> 12) - t34;
    t35  = ((t35a * (4096 - 4017) + t60a *   799         + 2048) >> 12) - t35a;
    t36  = ((t36a *  -799         + t59a * (4096 - 4017) + 2048) >> 12) - t59a;
    t37a = ((t37  *  -799         + t58  * (4096 - 4017) + 2048) >> 12) - t58;
    t42a =  (t42  * -1138         + t53  *  1703         + 1024) >> 11;
    t43  =  (t43a * -1138         + t52a *  1703         + 1024) >> 11;
    t44  =  (t44a * -1703         + t51a * -1138         + 1024) >> 11;
    t45a =  (t45  * -1703         + t50  * -1138         + 1024) >> 11;
    t50a =  (t45  * -1138         + t50  *  1703         + 1024) >> 11;
    t51  =  (t44a * -1138         + t51a *  1703         + 1024) >> 11;
    t52  =  (t43a *  1703         + t52a *  1138         + 1024) >> 11;
    t53a =  (t42  *  1703         + t53  *  1138         + 1024) >> 11;
    t58a = ((t37  * (4096 - 4017) + t58  *   799         + 2048) >> 12) - t37;
    t59  = ((t36a * (4096 - 4017) + t59a *   799         + 2048) >> 12) - t36a;
    t60  = ((t35a *   799         + t60a * (4017 - 4096) + 2048) >> 12) + t60a;
    t61a = ((t34  *   799         + t61  * (4017 - 4096) + 2048) >> 12) + t61;

    t32  = CLIP(t32a + t39a);
    t33a = CLIP(t33  + t38);
    t34  = CLIP(t34a + t37a);
    t35a = CLIP(t35  + t36);
    t36a = CLIP(t35  - t36);
    t37  = CLIP(t34a - t37a);
    t38a = CLIP(t33  - t38);
    t39  = CLIP(t32a - t39a);
    t40  = CLIP(t47a - t40a);
    t41a = CLIP(t46  - t41);
    t42  = CLIP(t45a - t42a);
    t43a = CLIP(t44  - t43);
    t44a = CLIP(t44  + t43);
    t45  = CLIP(t45a + t42a);
    t46a = CLIP(t46  + t41);
    t47  = CLIP(t47a + t40a);
    t48  = CLIP(t48a + t55a);
    t49a = CLIP(t49  + t54);
    t50  = CLIP(t50a + t53a);
    t51a = CLIP(t51  + t52);
    t52a = CLIP(t51  - t52);
    t53  = CLIP(t50a - t53a);
    t54a = CLIP(t49  - t54);
    t55  = CLIP(t48a - t55a);
    t56  = CLIP(t63a - t56a);
    t57a = CLIP(t62  - t57);
    t58  = CLIP(t61a - t58a);
    t59a = CLIP(t60  - t59);
    t60a = CLIP(t60  + t59);
    t61  = CLIP(t61a + t58a);
    t62a = CLIP(t62  + t57);
    t63  = CLIP(t63a + t56a);

    t36  = ((t36a * (4096 - 3784) + t59a *  1567         + 2048) >> 12) - t36a;
    t37a = ((t37  * (4096 - 3784) + t58  *  1567         + 2048) >> 12) - t37;
    t38  = ((t38a * (4096 - 3784) + t57a *  1567         + 2048) >> 12) - t38a;
    t39a = ((t39  * (4096 - 3784) + t56  *  1567         + 2048) >> 12) - t39;
    t40a = ((t40  * -1567         + t55  * (4096 - 3784) + 2048) >> 12) - t55;
    t41  = ((t41a * -1567         + t54a * (4096 - 3784) + 2048) >> 12) - t54a;
    t42a = ((t42  * -1567         + t53  * (4096 - 3784) + 2048) >> 12) - t53;
    t43  = ((t43a * -1567         + t52a * (4096 - 3784) + 2048) >> 12) - t52a;
    t52  = ((t43a * (4096 - 3784) + t52a *  1567         + 2048) >> 12) - t43a;
    t53a = ((t42  * (4096 - 3784) + t53  *  1567         + 2048) >> 12) - t42;
    t54  = ((t41a * (4096 - 3784) + t54a *  1567         + 2048) >> 12) - t41a;
    t55a = ((t40  * (4096 - 3784) + t55  *  1567         + 2048) >> 12) - t40;
    t56a = ((t39  *  1567         + t56  * (3784 - 4096) + 2048) >> 12) + t56;
    t57  = ((t38a *  1567         + t57a * (3784 - 4096) + 2048) >> 12) + t57a;
    t58a = ((t37  *  1567         + t58  * (3784 - 4096) + 2048) >> 12) + t58;
    t59  = ((t36a *  1567         + t59a * (3784 - 4096) + 2048) >> 12) + t59a;

    t32a = CLIP(t32  + t47);
    t33  = CLIP(t33a + t46a);
    t34a = CLIP(t34  + t45);
    t35  = CLIP(t35a + t44a);
    t36a = CLIP(t36  + t43);
    t37  = CLIP(t37a + t42a);
    t38a = CLIP(t38  + t41);
    t39  = CLIP(t39a + t40a);
    t40  = CLIP(t39a - t40a);
    t41a = CLIP(t38  - t41);
    t42  = CLIP(t37a - t42a);
    t43a = CLIP(t36  - t43);
    t44  = CLIP(t35a - t44a);
    t45a = CLIP(t34  - t45);
    t46  = CLIP(t33a - t46a);
    t47a = CLIP(t32  - t47);
    t48a = CLIP(t63  - t48);
    t49  = CLIP(t62a - t49a);
    t50a = CLIP(t61  - t50);
    t51  = CLIP(t60a - t51a);
    t52a = CLIP(t59  - t52);
    t53  = CLIP(t58a - t53a);
    t54a = CLIP(t57  - t54);
    t55  = CLIP(t56a - t55a);
    t56  = CLIP(t56a + t55a);
    t57a = CLIP(t57  + t54);
    t58  = CLIP(t58a + t53a);
    t59a = CLIP(t59  + t52);
    t60  = CLIP(t60a + t51a);
    t61a = CLIP(t61  + t50);
    t62  = CLIP(t62a + t49a);
    t63a = CLIP(t63  + t48);

    t40a = ((t55  - t40 ) * 181 + 128) >> 8;
    t41  = ((t54a - t41a) * 181 + 128) >> 8;
    t42a = ((t53  - t42 ) * 181 + 128) >> 8;
    t43  = ((t52a - t43a) * 181 + 128) >> 8;
    t44a = ((t51  - t44 ) * 181 + 128) >> 8;
    t45  = ((t50a - t45a) * 181 + 128) >> 8;
    t46a = ((t49  - t46 ) * 181 + 128) >> 8;
    t47  = ((t48a - t47a) * 181 + 128) >> 8;
    t48  = ((t47a + t48a) * 181 + 128) >> 8;
    t49a = ((t46  + t49 ) * 181 + 128) >> 8;
    t50  = ((t45a + t50a) * 181 + 128) >> 8;
    t51a = ((t44  + t51 ) * 181 + 128) >> 8;
    t52  = ((t43a + t52a) * 181 + 128) >> 8;
    t53a = ((t42  + t53 ) * 181 + 128) >> 8;
    t54  = ((t41a + t54a) * 181 + 128) >> 8;
    t55a = ((t40  + t55 ) * 181 + 128) >> 8;

    {
        t0  = c[ 0 * stride];
        t1  = c[ 2 * stride];
        t2  = c[ 4 * stride];
        t3  = c[ 6 * stride];
        t4  = c[ 8 * stride];
        t5  = c[10 * stride];
        t6  = c[12 * stride];
        t7  = c[14 * stride];
        t8  = c[16 * stride];
        t9  = c[18 * stride];
        t10 = c[20 * stride];
        t11 = c[22 * stride];
        t12 = c[24 * stride];
        t13 = c[26 * stride];
        t14 = c[28 * stride];
        t15 = c[30 * stride];
        t16 = c[32 * stride];
        t17 = c[34 * stride];
        t18 = c[36 * stride];
        t19 = c[38 * stride];
        t20 = c[40 * stride];
        t21 = c[42 * stride];
        t22 = c[44 * stride];
        t23 = c[46 * stride];
        t24 = c[48 * stride];
        t25 = c[50 * stride];
        t26 = c[52 * stride];
        t27 = c[54 * stride];
        t28 = c[56 * stride];
        t29 = c[58 * stride];
        t30 = c[60 * stride];
        t31 = c[62 * stride];

        c[ 0 * stride] = CLIP(t0  + t63a);
        c[ 1 * stride] = CLIP(t1  + t62);
        c[ 2 * stride] = CLIP(t2  + t61a);
        c[ 3 * stride] = CLIP(t3  + t60);
        c[ 4 * stride] = CLIP(t4  + t59a);
        c[ 5 * stride] = CLIP(t5  + t58);
        c[ 6 * stride] = CLIP(t6  + t57a);
        c[ 7 * stride] = CLIP(t7  + t56);
        c[ 8 * stride] = CLIP(t8  + t55a);
        c[ 9 * stride] = CLIP(t9  + t54);
        c[10 * stride] = CLIP(t10 + t53a);
        c[11 * stride] = CLIP(t11 + t52);
        c[12 * stride] = CLIP(t12 + t51a);
        c[13 * stride] = CLIP(t13 + t50);
        c[14 * stride] = CLIP(t14 + t49a);
        c[15 * stride] = CLIP(t15 + t48);
        c[16 * stride] = CLIP(t16 + t47);
        c[17 * stride] = CLIP(t17 + t46a);
        c[18 * stride] = CLIP(t18 + t45);
        c[19 * stride] = CLIP(t19 + t44a);
        c[20 * stride] = CLIP(t20 + t43);
        c[21 * stride] = CLIP(t21 + t42a);
        c[22 * stride] = CLIP(t22 + t41);
        c[23 * stride] = CLIP(t23 + t40a);
        c[24 * stride] = CLIP(t24 + t39);
        c[25 * stride] = CLIP(t25 + t38a);
        c[26 * stride] = CLIP(t26 + t37);
        c[27 * stride] = CLIP(t27 + t36a);
        c[28 * stride] = CLIP(t28 + t35);
        c[29 * stride] = CLIP(t29 + t34a);
        c[30 * stride] = CLIP(t30 + t33);
        c[31 * stride] = CLIP(t31 + t32a);
        c[32 * stride] = CLIP(t31 - t32a);
        c[33 * stride] = CLIP(t30 - t33);
        c[34 * stride] = CLIP(t29 - t34a);
        c[35 * stride] = CLIP(t28 - t35);
        c[36 * stride] = CLIP(t27 - t36a);
        c[37 * stride] = CLIP(t26 - t37);
        c[38 * stride] = CLIP(t25 - t38a);
        c[39 * stride] = CLIP(t24 - t39);
        c[40 * stride] = CLIP(t23 - t40a);
        c[41 * stride] = CLIP(t22 - t41);
        c[42 * stride] = CLIP(t21 - t42a);
        c[43 * stride] = CLIP(t20 - t43);
        c[44 * stride] = CLIP(t19 - t44a);
        c[45 * stride] = CLIP(t18 - t45);
        c[46 * stride] = CLIP(t17 - t46a);
        c[47 * stride] = CLIP(t16 - t47);
        c[48 * stride] = CLIP(t15 - t48);
        c[49 * stride] = CLIP(t14 - t49a);
        c[50 * stride] = CLIP(t13 - t50);
        c[51 * stride] = CLIP(t12 - t51a);
        c[52 * stride] = CLIP(t11 - t52);
        c[53 * stride] = CLIP(t10 - t53a);
        c[54 * stride] = CLIP(t9  - t54);
        c[55 * stride] = CLIP(t8  - t55a);
        c[56 * stride] = CLIP(t7  - t56);
        c[57 * stride] = CLIP(t6  - t57a);
        c[58 * stride] = CLIP(t5  - t58);
        c[59 * stride] = CLIP(t4  - t59a);
        c[60 * stride] = CLIP(t3  - t60);
        c[61 * stride] = CLIP(t2  - t61a);
        c[62 * stride] = CLIP(t1  - t62);
        c[63 * stride] = CLIP(t0  - t63a);
    }
}

static void
inv_adst4_1d_internal_c(const stbv_i32 * in, const int in_s,
                        const int min, const int max,
                        stbv_i32 * out, const int out_s)
{
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];

    assert(in_s > 0 && out_s != 0);
    out[0 * out_s] = (( 1321         * in0 + (3803 - 4096) * in2 +
                       (2482 - 4096) * in3 + (3344 - 4096) * in1 + 2048) >> 12) +
                     in2 + in3 + in1;
    out[1 * out_s] = (((2482 - 4096) * in0 -  1321         * in2 -
                       (3803 - 4096) * in3 + (3344 - 4096) * in1 + 2048) >> 12) +
                     in0 - in3 + in1;
    out[2 * out_s] = (209 * (in0 - in2 + in3) + 128) >> 8;
    out[3 * out_s] = (((3803 - 4096) * in0 + (2482 - 4096) * in2 -
                        1321         * in3 - (3344 - 4096) * in1 + 2048) >> 12) +
                     in0 + in2 - in1;
}

static void
inv_adst8_1d_internal_c(const stbv_i32 * in, const int in_s,
                        const int min, const int max,
                        stbv_i32 * out, const int out_s)
{
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];
    const int in4 = in[4 * in_s], in5 = in[5 * in_s];
    const int in6 = in[6 * in_s], in7 = in[7 * in_s];

    /* note: in_s > 0 and out_s != 0 are guaranteed by callers (assert
     * omitted here so every declaration stays before any statement). */
    const int t0a = (((4076 - 4096) * in7 +   401         * in0 + 2048) >> 12) + in7;
    const int t1a = ((  401         * in7 - (4076 - 4096) * in0 + 2048) >> 12) - in0;
    const int t2a = (((3612 - 4096) * in5 +  1931         * in2 + 2048) >> 12) + in5;
    const int t3a = (( 1931         * in5 - (3612 - 4096) * in2 + 2048) >> 12) - in2;
          int t4a =  ( 1299         * in3 +  1583         * in4 + 1024) >> 11;
          int t5a =  ( 1583         * in3 -  1299         * in4 + 1024) >> 11;
          int t6a = (( 1189         * in1 + (3920 - 4096) * in6 + 2048) >> 12) + in6;
          int t7a = (((3920 - 4096) * in1 -  1189         * in6 + 2048) >> 12) + in1;

    const int t0 = CLIP(t0a + t4a);
    const int t1 = CLIP(t1a + t5a);
          int t2 = CLIP(t2a + t6a);
          int t3 = CLIP(t3a + t7a);
    const int t4 = CLIP(t0a - t4a);
    const int t5 = CLIP(t1a - t5a);
          int t6 = CLIP(t2a - t6a);
          int t7 = CLIP(t3a - t7a);

    t4a = (((3784 - 4096) * t4 +  1567         * t5 + 2048) >> 12) + t4;
    t5a = (( 1567         * t4 - (3784 - 4096) * t5 + 2048) >> 12) - t5;
    t6a = (((3784 - 4096) * t7 -  1567         * t6 + 2048) >> 12) + t7;
    t7a = (( 1567         * t7 + (3784 - 4096) * t6 + 2048) >> 12) + t6;

    out[0 * out_s] =  CLIP(t0  + t2 );
    out[7 * out_s] = -CLIP(t1  + t3 );
    t2             =  CLIP(t0  - t2 );
    t3             =  CLIP(t1  - t3 );
    out[1 * out_s] = -CLIP(t4a + t6a);
    out[6 * out_s] =  CLIP(t5a + t7a);
    t6             =  CLIP(t4a - t6a);
    t7             =  CLIP(t5a - t7a);

    out[3 * out_s] = -(((t2 + t3) * 181 + 128) >> 8);
    out[4 * out_s] =   ((t2 - t3) * 181 + 128) >> 8;
    out[2 * out_s] =   ((t6 + t7) * 181 + 128) >> 8;
    out[5 * out_s] = -(((t6 - t7) * 181 + 128) >> 8);
}

static void
inv_adst16_1d_internal_c(const stbv_i32 * in, const int in_s,
                         const int min, const int max,
                         stbv_i32 * out, const int out_s)
{
    const int in0  = in[ 0 * in_s], in1  = in[ 1 * in_s];
    const int in2  = in[ 2 * in_s], in3  = in[ 3 * in_s];
    const int in4  = in[ 4 * in_s], in5  = in[ 5 * in_s];
    const int in6  = in[ 6 * in_s], in7  = in[ 7 * in_s];
    const int in8  = in[ 8 * in_s], in9  = in[ 9 * in_s];
    const int in10 = in[10 * in_s], in11 = in[11 * in_s];
    const int in12 = in[12 * in_s], in13 = in[13 * in_s];
    const int in14 = in[14 * in_s], in15 = in[15 * in_s];

    /* note: in_s > 0 and out_s != 0 are guaranteed by callers (assert
     * omitted here so every declaration stays before any statement). */
    int t0  = ((in15 * (4091 - 4096) + in0  *   201         + 2048) >> 12) + in15;
    int t1  = ((in15 *   201         - in0  * (4091 - 4096) + 2048) >> 12) - in0;
    int t2  = ((in13 * (3973 - 4096) + in2  *   995         + 2048) >> 12) + in13;
    int t3  = ((in13 *   995         - in2  * (3973 - 4096) + 2048) >> 12) - in2;
    int t4  = ((in11 * (3703 - 4096) + in4  *  1751         + 2048) >> 12) + in11;
    int t5  = ((in11 *  1751         - in4  * (3703 - 4096) + 2048) >> 12) - in4;
    int t6  =  (in9  *  1645         + in6  *  1220         + 1024) >> 11;
    int t7  =  (in9  *  1220         - in6  *  1645         + 1024) >> 11;
    int t8  = ((in7  *  2751         + in8  * (3035 - 4096) + 2048) >> 12) + in8;
    int t9  = ((in7  * (3035 - 4096) - in8  *  2751         + 2048) >> 12) + in7;
    int t10 = ((in5  *  2106         + in10 * (3513 - 4096) + 2048) >> 12) + in10;
    int t11 = ((in5  * (3513 - 4096) - in10 *  2106         + 2048) >> 12) + in5;
    int t12 = ((in3  *  1380         + in12 * (3857 - 4096) + 2048) >> 12) + in12;
    int t13 = ((in3  * (3857 - 4096) - in12 *  1380         + 2048) >> 12) + in3;
    int t14 = ((in1  *   601         + in14 * (4052 - 4096) + 2048) >> 12) + in14;
    int t15 = ((in1  * (4052 - 4096) - in14 *   601         + 2048) >> 12) + in1;

    int t0a  = CLIP(t0 + t8 );
    int t1a  = CLIP(t1 + t9 );
    int t2a  = CLIP(t2 + t10);
    int t3a  = CLIP(t3 + t11);
    int t4a  = CLIP(t4 + t12);
    int t5a  = CLIP(t5 + t13);
    int t6a  = CLIP(t6 + t14);
    int t7a  = CLIP(t7 + t15);
    int t8a  = CLIP(t0 - t8 );
    int t9a  = CLIP(t1 - t9 );
    int t10a = CLIP(t2 - t10);
    int t11a = CLIP(t3 - t11);
    int t12a = CLIP(t4 - t12);
    int t13a = CLIP(t5 - t13);
    int t14a = CLIP(t6 - t14);
    int t15a = CLIP(t7 - t15);

    t8   = ((t8a  * (4017 - 4096) + t9a  *   799         + 2048) >> 12) + t8a;
    t9   = ((t8a  *   799         - t9a  * (4017 - 4096) + 2048) >> 12) - t9a;
    t10  = ((t10a *  2276         + t11a * (3406 - 4096) + 2048) >> 12) + t11a;
    t11  = ((t10a * (3406 - 4096) - t11a *  2276         + 2048) >> 12) + t10a;
    t12  = ((t13a * (4017 - 4096) - t12a *   799         + 2048) >> 12) + t13a;
    t13  = ((t13a *   799         + t12a * (4017 - 4096) + 2048) >> 12) + t12a;
    t14  = ((t15a *  2276         - t14a * (3406 - 4096) + 2048) >> 12) - t14a;
    t15  = ((t15a * (3406 - 4096) + t14a *  2276         + 2048) >> 12) + t15a;

    t0   = CLIP(t0a + t4a);
    t1   = CLIP(t1a + t5a);
    t2   = CLIP(t2a + t6a);
    t3   = CLIP(t3a + t7a);
    t4   = CLIP(t0a - t4a);
    t5   = CLIP(t1a - t5a);
    t6   = CLIP(t2a - t6a);
    t7   = CLIP(t3a - t7a);
    t8a  = CLIP(t8  + t12);
    t9a  = CLIP(t9  + t13);
    t10a = CLIP(t10 + t14);
    t11a = CLIP(t11 + t15);
    t12a = CLIP(t8  - t12);
    t13a = CLIP(t9  - t13);
    t14a = CLIP(t10 - t14);
    t15a = CLIP(t11 - t15);

    t4a  = ((t4   * (3784 - 4096) + t5   *  1567         + 2048) >> 12) + t4;
    t5a  = ((t4   *  1567         - t5   * (3784 - 4096) + 2048) >> 12) - t5;
    t6a  = ((t7   * (3784 - 4096) - t6   *  1567         + 2048) >> 12) + t7;
    t7a  = ((t7   *  1567         + t6   * (3784 - 4096) + 2048) >> 12) + t6;
    t12  = ((t12a * (3784 - 4096) + t13a *  1567         + 2048) >> 12) + t12a;
    t13  = ((t12a *  1567         - t13a * (3784 - 4096) + 2048) >> 12) - t13a;
    t14  = ((t15a * (3784 - 4096) - t14a *  1567         + 2048) >> 12) + t15a;
    t15  = ((t15a *  1567         + t14a * (3784 - 4096) + 2048) >> 12) + t14a;

    out[ 0 * out_s] =  CLIP(t0  + t2  );
    out[15 * out_s] = -CLIP(t1  + t3  );
    t2a             =  CLIP(t0  - t2  );
    t3a             =  CLIP(t1  - t3  );
    out[ 3 * out_s] = -CLIP(t4a + t6a );
    out[12 * out_s] =  CLIP(t5a + t7a );
    t6              =  CLIP(t4a - t6a );
    t7              =  CLIP(t5a - t7a );
    out[ 1 * out_s] = -CLIP(t8a + t10a);
    out[14 * out_s] =  CLIP(t9a + t11a);
    t10             =  CLIP(t8a - t10a);
    t11             =  CLIP(t9a - t11a);
    out[ 2 * out_s] =  CLIP(t12 + t14 );
    out[13 * out_s] = -CLIP(t13 + t15 );
    t14a            =  CLIP(t12 - t14 );
    t15a            =  CLIP(t13 - t15 );

    out[ 7 * out_s] = -(((t2a  + t3a)  * 181 + 128) >> 8);
    out[ 8 * out_s] =   ((t2a  - t3a)  * 181 + 128) >> 8;
    out[ 4 * out_s] =   ((t6   + t7)   * 181 + 128) >> 8;
    out[11 * out_s] = -(((t6   - t7)   * 181 + 128) >> 8);
    out[ 6 * out_s] =   ((t10  + t11)  * 181 + 128) >> 8;
    out[ 9 * out_s] = -(((t10  - t11)  * 181 + 128) >> 8);
    out[ 5 * out_s] = -(((t14a + t15a) * 181 + 128) >> 8);
    out[10 * out_s] =   ((t14a - t15a) * 181 + 128) >> 8;
}

#define inv_adst_1d(sz) \
static void inv_adst##sz##_1d_c(stbv_i32 * c, const int stride, \
                                const int min, const int max) \
{ \
    inv_adst##sz##_1d_internal_c(c, stride, min, max, c, stride); \
} \
static void inv_flipadst##sz##_1d_c(stbv_i32 * c, const int stride, \
                                          const int min, const int max) \
{ \
    inv_adst##sz##_1d_internal_c(c, stride, min, max, \
                                 &c[(sz - 1) * stride], -stride); \
}

inv_adst_1d( 4)
inv_adst_1d( 8)
inv_adst_1d(16)

#undef inv_adst_1d

static void inv_identity4_1d_c(stbv_i32 * c, const int stride,
                               const int min, const int max)
{
    assert(stride > 0);
    { int i; for (i = 0; i < 4; i++) {
        const int in = c[stride * i];
        c[stride * i] = in + ((in * 1697 + 2048) >> 12);
    }
    }
}

static void inv_identity8_1d_c(stbv_i32 * c, const int stride,
                               const int min, const int max)
{
    assert(stride > 0);
    { int i; for (i = 0; i < 8; i++)
        c[stride * i] *= 2;
    }
}

static void inv_identity16_1d_c(stbv_i32 * c, const int stride,
                                const int min, const int max)
{
    assert(stride > 0);
    { int i; for (i = 0; i < 16; i++) {
        const int in = c[stride * i];
        c[stride * i] = 2 * in + ((in * 1697 + 1024) >> 11);
    }
    }
}

static void inv_identity32_1d_c(stbv_i32 * c, const int stride,
                                const int min, const int max)
{
    assert(stride > 0);
    { int i; for (i = 0; i < 32; i++)
        c[stride * i] *= 4;
    }
}


#undef CLIP
#undef STBV_AV1_CLIP
#endif

/* dav1d enum Tx1dType ordering: DCT, ADST, FLIPADST, IDENTITY. */
typedef void (*stbv_av1_itx1d_fn)(stbv_i32 *, const int, const int, const int);

static const stbv_av1_itx1d_fn stbv_av1_tx1d_fns[5][4] = {
    { inv_dct4_1d_c,  inv_adst4_1d_c,  inv_flipadst4_1d_c,  inv_identity4_1d_c  },
    { inv_dct8_1d_c,  inv_adst8_1d_c,  inv_flipadst8_1d_c,  inv_identity8_1d_c  },
    { inv_dct16_1d_c, inv_adst16_1d_c, inv_flipadst16_1d_c, inv_identity16_1d_c },
    { inv_dct32_1d_c, NULL,            NULL,                inv_identity32_1d_c },
    { inv_dct64_1d_c, NULL,            NULL,                NULL                }
};

/* dav1d_tx1d_types[]: {first (width-axis), second (height-axis)} per txtp. */
/* Effective 1D type pairs.  NOTE: dav1d assigns the itxfm_add slots
 * cross-wise ([ADST_DCT] = fn_dct_adst etc.) because its intermediate
 * buffer is transposed; replicating the EFFECTIVE mapping here:
 *   e.g. decoded ADST_DCT runs first=DCT (columns), second=ADST (rows). */
static const stbv_u8 stbv_av1_tx1d_types[STBV_AV1_TX_WHT_WHT + 1][2] = {
    /* pair = { horizontal 1-D type, vertical 1-D type }; 1-D types are
     * 0=DCT, 1=ADST, 2=FLIPADST, 3=IDENTITY (see stbv_av1_tx1d_fns).
     * Semantics per dav1d levels.h TxfmType (e.g. H_DCT == identity
     * vertically, DCT horizontally). */
    { 0, 0 }, /* DCT_DCT           */
    { 0, 1 }, /* ADST_DCT          */
    { 1, 0 }, /* DCT_ADST          */
    { 1, 1 }, /* ADST_ADST         */
    { 0, 2 }, /* FLIPADST_DCT      */
    { 2, 0 }, /* DCT_FLIPADST      */
    { 2, 2 }, /* FLIPADST_FLIPADST */
    { 2, 1 }, /* ADST_FLIPADST     */
    { 1, 2 }, /* FLIPADST_ADST     */
    { 3, 3 }, /* IDTX              */
    { 3, 0 }, /* V_DCT             */
    { 0, 3 }, /* H_DCT             */
    { 3, 1 }, /* V_ADST            */
    { 1, 3 }, /* H_ADST            */
    { 3, 2 }, /* V_FLIPADST        */
    { 2, 3 }, /* H_FLIPADST        */
    { 0, 0 }  /* WHT_WHT (unused)  */
};

/* Intermediate rounding shift per transform size, in tx enum order
 * (dav1d inv_txfm_fn*() instantiation list). */
static const unsigned char stbv_av1_itx_shifts[STBV_AV1_N_TX_SIZES] = {
    0, 1, 2, 2, 2, /* 4x4, 8x8, 16x16, 32x32, 64x64 */
    0, 0,          /* 4x8, 8x4  */
    1, 1, 1, 1, 1, 1, /* 8x16, 16x8, 16x32, 32x16, 32x64, 64x32 */
    1, 1,          /* 4x16, 16x4 */
    2, 2, 2, 2     /* 8x32, 32x8, 16x64, 64x16 */
};

#define STBV_AV1_ITX_CLIP(v, lo, hi) \
    ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/*
 * Compute the 2-D inverse transform into tmp[] (w*h entries, layout
 * tmp[y*w+x], values still needing the final (v+8)>>4 rounding).
 *
 * Returns 0 and sets *dc_out for the DC-only shortcut (eob == 0 and
 * DCT_DCT); returns 1 when tmp[] was filled.  Mirrors dav1d
 * inv_txfm_add_c(); unlike dav1d's C reference, the coefficient grid is
 * explicitly zero-padded from its clipped sw x sh extent up to the full
 * w x h transform input, which is the semantics the SIMD paths implement
 * and what the spec requires (coefficients outside the coded 32x32
 * sub-grid are implicitly zero).
 */
static int stbv_av1_inv_txfm_core(stbv_i32 *coeff, const int eob,
                                  const int tx, const int txtp,
                                  const int bd, stbv_i32 *tmp, int *dc_out)
{
    const stbv_av1_tx_dim *t_dim = &stbv_av1_tx_dims[tx];
    const int w = 4 * t_dim->w, h = 4 * t_dim->h;
    const int has_dconly = txtp == STBV_AV1_TX_DCT_DCT;
    const int is_rect2 = w * 2 == h || h * 2 == w;
    const int shift = stbv_av1_itx_shifts[tx];
    const int rnd = (1 << shift) >> 1;
    const int sh = h < 32 ? h : 32, sw = w < 32 ? w : 32;
    const stbv_u8 *txtps;
    stbv_av1_itx1d_fn first_fn, second_fn;
    int cf_dbg_hit = 0;
    int row_clip_min, row_clip_max, col_clip_min, col_clip_max;
    stbv_i32 *c;
    int y, x, i;

    if (eob < has_dconly) {
        int dc = coeff[0];
        coeff[0] = 0;
        if (is_rect2)
            dc = (dc * 181 + 128) >> 8;
        dc = (dc * 181 + 128) >> 8;
        dc = (dc + rnd) >> shift;
        *dc_out = (dc * 181 + 128 + 2048) >> 12;
        return 0;
    }

    if (bd == 8) {
        row_clip_min = -32768;
        col_clip_min = -32768;
    } else {
        const unsigned max = (1U << bd) - 1;
        row_clip_min = (int)((unsigned)~max << 7);
        col_clip_min = (int)((unsigned)~max << 5);
    }
    row_clip_max = ~row_clip_min;
    col_clip_max = ~col_clip_min;

    txtps = stbv_av1_tx1d_types[txtp];
    first_fn = stbv_av1_tx1d_fns[t_dim->lw][txtps[0]];
    second_fn = stbv_av1_tx1d_fns[t_dim->lh][txtps[1]];
    if (!first_fn) first_fn = stbv_av1_tx1d_fns[t_dim->lw][0];
    if (!second_fn) second_fn = stbv_av1_tx1d_fns[t_dim->lh][0];

    /* dav1d last_nonzero_col_from_eob: coefficient rows beyond this txb's
     * last decoded scan position hold stale scratch data, so only rows up
     * to the maximum touched column may be read; the rest must be zeroed.
     * Skipping this turned every deep 64x64 txb into reconstruction
     * garbage once the stack scratch stopped being zero-filled. */
    {
        int lnc = 0;
        if (txtps[1] == 3U && txtps[0] != 3U) {
            lnc = sh - 1 < eob ? sh - 1 : eob;
        } else if (txtps[0] == 3U && txtps[1] != 3U) {
            lnc = eob >> (t_dim->lw + 2);
            if (lnc > sh - 1) lnc = sh - 1;
        } else {
            const stbv_u16 *sc;
            switch (tx) {
            case STBV_AV1_TX_4X4:   sc = stbv_av1_scan_4x4;   break;
            case STBV_AV1_TX_8X8:   sc = stbv_av1_scan_8x8;   break;
            case STBV_AV1_TX_16X16: sc = stbv_av1_scan_16x16; break;
            case STBV_AV1_TX_32X32: sc = stbv_av1_scan_32x32; break;
            case STBV_AV1_TX_64X64: sc = stbv_av1_scan_32x32; break;
            default:                sc = stbv_av1_scan_rect[tx]; break;
            }
            for (i = 0; i <= eob; i++) {
                const unsigned rcx = (unsigned)sc[i] & (unsigned)(sh - 1);
                if ((int)rcx > lnc) lnc = (int)rcx;
            }
        }
        c = tmp;
        for (y = 0; y <= lnc; y++, c += w) {
            if (is_rect2) {
                for (x = 0; x < sw; x++)
                    c[x] = (coeff[y + x * sh] * 181 + 128) >> 8;
            } else {
                for (x = 0; x < sw; x++)
                    c[x] = coeff[y + x * sh];
            }
            for (x = sw; x < w; x++)
                c[x] = 0;
            first_fn(c, 1, row_clip_min, row_clip_max);
        }
        if (lnc + 1 < sh)
            memset(c, 0, (size_t)(sh - lnc - 1) * w * sizeof(stbv_i32));
    }
    if (sh < h)
        memset(tmp + sh * w, 0, (size_t)((h - sh) * w) * sizeof(stbv_i32));

    memset(coeff, 0, (size_t)(sw * sh) * sizeof(stbv_i32));
    for (i = 0; i < w * h; i++)
        tmp[i] = STBV_AV1_ITX_CLIP((tmp[i] + rnd) >> shift,
                                   col_clip_min, col_clip_max);

    for (x = 0; x < w; x++)
        second_fn(&tmp[x], w, col_clip_min, col_clip_max);
    return 1;
}

static void stbv_av1_inv_wht4_1d(stbv_i32 *c, const int stride)
{
    const int in0 = c[0 * stride], in1 = c[1 * stride];
    const int in2 = c[2 * stride], in3 = c[3 * stride];

    const int t0 = in0 + in1;
    const int t2 = in2 - in3;
    const int t4 = (t0 - t2) >> 1;
    const int t3 = t4 - in3;
    const int t1 = t4 - in1;

    c[0 * stride] = t0 - t3;
    c[1 * stride] = t3;
    c[2 * stride] = t1;
    c[3 * stride] = t2 + t1;
}

/* WHT_WHT (lossless) 4x4; dav1d inv_txfm_add_wht_wht_4x4_c(). */
static int stbv_av1_inv_wht_core(stbv_i32 *coeff, stbv_i32 *tmp)
{
    int x, y;
    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++)
            tmp[y * 4 + x] = coeff[y + x * 4] >> 2;
        stbv_av1_inv_wht4_1d(tmp + y * 4, 1);
    }
    memset(coeff, 0, (size_t)(4 * 4) * sizeof(stbv_i32));
    for (x = 0; x < 4; x++)
        stbv_av1_inv_wht4_1d(&tmp[x], 4);
    return 1;
}

/* 8-bit output: dst has stride bytes; pixel range 0..255. */
static void stbv_av1_inv_txfm_add8(stbv_u8 *dst, const int stride,
                                   stbv_i32 *coeff, const int eob,
                                   const int tx, const int txtp)
{
    stbv_i32 tmp[64 * 64];
    int dc = 0;
    int x, y;

    if (txtp == STBV_AV1_TX_WHT_WHT) {
        if (stbv_av1_inv_wht_core(coeff, tmp)) {
            for (y = 0; y < 4; y++, dst += stride)
                for (x = 0; x < 4; x++) {
                    int v = dst[x] + tmp[y * 4 + x];
                    dst[x] = (stbv_u8)STBV_AV1_ITX_CLIP(v, 0, 255);
                }
        }
        return;
    }

    if (!stbv_av1_inv_txfm_core(coeff, eob, tx, txtp, 8, tmp, &dc)) {
        for (y = 0; y < 4 * stbv_av1_tx_dims[tx].h; y++, dst += stride)
            for (x = 0; x < 4 * stbv_av1_tx_dims[tx].w; x++) {
                int v = dst[x] + dc;
                dst[x] = (stbv_u8)STBV_AV1_ITX_CLIP(v, 0, 255);
            }
        return;
    }

    {
        const int w = 4 * stbv_av1_tx_dims[tx].w;
        const int h = 4 * stbv_av1_tx_dims[tx].h;
        const stbv_i32 *t = tmp;
        for (y = 0; y < h; y++, dst += stride)
            for (x = 0; x < w; x++) {
                int v = dst[x] + ((*t++ + 8) >> 4);
                dst[x] = (stbv_u8)STBV_AV1_ITX_CLIP(v, 0, 255);
            }
    }
}

/* High-bit-depth output: dst is u16 with stride units; pixel range
 * 0..(1<<bd)-1. */
static void stbv_av1_inv_txfm_add16(stbv_u16 *dst, const int stride,
                                    stbv_i32 *coeff, const int eob,
                                    const int tx, const int txtp, const int bd)
{
    stbv_i32 tmp[64 * 64];
    const int max_pix = (1 << bd) - 1;
    int dc = 0;
    int x, y;

    if (txtp == STBV_AV1_TX_WHT_WHT) {
        if (stbv_av1_inv_wht_core(coeff, tmp)) {
            for (y = 0; y < 4; y++, dst += stride)
                for (x = 0; x < 4; x++) {
                    int v = dst[x] + tmp[y * 4 + x];
                    dst[x] = (stbv_u16)STBV_AV1_ITX_CLIP(v, 0, max_pix);
                }
        }
        return;
    }

    if (!stbv_av1_inv_txfm_core(coeff, eob, tx, txtp, bd, tmp, &dc)) {
        for (y = 0; y < 4 * stbv_av1_tx_dims[tx].h; y++, dst += stride)
            for (x = 0; x < 4 * stbv_av1_tx_dims[tx].w; x++) {
                int v = dst[x] + dc;
                dst[x] = (stbv_u16)STBV_AV1_ITX_CLIP(v, 0, max_pix);
            }
        return;
    }

    {
        const int w = 4 * stbv_av1_tx_dims[tx].w;
        const int h = 4 * stbv_av1_tx_dims[tx].h;
        const stbv_i32 *t = tmp;
        for (y = 0; y < h; y++, dst += stride)
            for (x = 0; x < w; x++) {
                int v = dst[x] + ((*t++ + 8) >> 4);
                dst[x] = (stbv_u16)STBV_AV1_ITX_CLIP(v, 0, max_pix);
            }
    }
}

#undef STBV_AV1_ITX_CLIP
#endif

/* ===== stb_av1_ipred.h ===== */
/*
 * stb_av1_ipred.h - scalar AV1 intra prediction (8-bit and 16-bit pixels)
 *
 * Faithful port of dav1d 1.5.4 src/ipred_tmpl.c, src/ipred_prepare_tmpl.c
 * and the corresponding tables from src/tables.c.
 *
 * Copyright (C) 2018-2021, VideoLAN and dav1d authors
 * Copyright (C) 2018, Two Orioles, LLC
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_IPRED_H
#define STB_AV1_IPRED_H

#ifndef STBV_U16_DEFINED
typedef unsigned short stbv_u16;
#define STBV_U16_DEFINED 1
#endif
#ifndef STBV_U8_DEFINED
typedef unsigned char stbv_u8;
#define STBV_U8_DEFINED 1
#endif
#ifndef STBV_I16_DEFINED
typedef signed short stbv_i16;
#define STBV_I16_DEFINED 1
#endif

#if defined(__GNUC__)
#define STBV_AV1_IPRED_UNUSED __attribute__((unused))
#else
#define STBV_AV1_IPRED_UNUSED
#endif

/*
 * Implemented prediction modes (dav1d enum IntraPredMode ordering).
 * The regular modes 0..12 match the syntax layer's y_mode/uv_mode values.
 */
#define STBV_AV1_IPRED_DC        0
#define STBV_AV1_IPRED_VERT      1
#define STBV_AV1_IPRED_HOR       2
#define STBV_AV1_IPRED_DDL       3
#define STBV_AV1_IPRED_DDR       4
#define STBV_AV1_IPRED_VR        5
#define STBV_AV1_IPRED_HD        6
#define STBV_AV1_IPRED_HU        7
#define STBV_AV1_IPRED_VL        8
#define STBV_AV1_IPRED_SMOOTH    9
#define STBV_AV1_IPRED_SMOOTH_V 10
#define STBV_AV1_IPRED_SMOOTH_H 11
#define STBV_AV1_IPRED_PAETH    12
#define STBV_AV1_IPRED_LEFT_DC  13
#define STBV_AV1_IPRED_TOP_DC   14
#define STBV_AV1_IPRED_DC_128   15
#define STBV_AV1_IPRED_FILTER   16
#define STBV_AV1_IPRED_Z1       17
#define STBV_AV1_IPRED_Z2       18
#define STBV_AV1_IPRED_Z3       19

/* Edge availability flags (dav1d EdgeFlags, I444 positions). */
#define STBV_AV1_EDGE_I444_TOP_HAS_RIGHT   1
#define STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM 2

/* Needs-left/top/topleft/topright/bottomleft bit masks. */
#define STBV_AV1_IPRED_NL 1
#define STBV_AV1_IPRED_NT 2
#define STBV_AV1_IPRED_NTL 4
#define STBV_AV1_IPRED_NTR 8
#define STBV_AV1_IPRED_NBL 16

STBV_AV1_IPRED_UNUSED static const unsigned char stbv_av1_sm_weights[128] = {
      0,   0,
    255, 128,
    255, 149,  85,  64,
    255, 197, 146, 105,  73,  50,  37,  32,
    255, 225, 196, 170, 145, 123, 102,  84,
     68,  54,  43,  33,  26,  20,  17,  16,
    255, 240, 225, 210, 196, 182, 169, 157,
    145, 133, 122, 111, 101,  92,  83,  74,
     66,  59,  52,  45,  39,  34,  29,  25,
     21,  17,  14,  12,  10,   9,   8,   8,
    255, 248, 240, 233, 225, 218, 210, 203,
    196, 189, 182, 176, 169, 163, 156, 150,
    144, 138, 133, 127, 121, 116, 111, 106,
    101,  96,  91,  86,  82,  77,  73,  69,
     65,  61,  57,  54,  50,  47,  44,  41,
     38,  35,  32,  29,  27,  25,  22,  20,
     18,  16,  15,  13,  12,  10,   9,   8,
      7,   6,   6,   5,   5,   4,   4,   4
};

/* dav1d_dr_intra_derivative[44]; index = angle >> 1 for the three zones. */
STBV_AV1_IPRED_UNUSED static const unsigned short stbv_av1_dr_deriv[44] = {
          0,
    1023,   0,
     547,
     372,   0,   0,
     273,
     215,   0,
     178,
     151,   0,
     132,
     116,   0,
     102,   0,
      90,
      80,   0,
      71,
      64,   0,
      57,
      51,   0,
      45,   0,
      40,
      35,   0,
      31,
      27,   0,
      23,
      19,   0,
      15,   0,
      11,   0,
       7,
       3
};

/*
 * Filter-intra taps, dav1d_filter_intra_taps[5][64].  Tap t of output
 * pixel k = yy*4+xx lives at [k + 8*t].
 */
STBV_AV1_IPRED_UNUSED static const signed char
    stbv_av1_filter_intra_taps[5][64] = {
    {
         -6,  -5,  -3,  -3,  -4,  -3,  -3,  -3,
         10,   2,   1,   1,   6,   2,   2,   1,
          0,  10,   1,   1,   0,   6,   2,   2,
          0,   0,  10,   2,   0,   0,   6,   2,
          0,   0,   0,  10,   0,   0,   0,   6,
         12,   9,   7,   5,   2,   2,   2,   3,
          0,   0,   0,   0,  12,   9,   7,   5,
          0,   0,   0,   0,   0,   0,   0,   0
    }, {
        -10,  -6,  -4,  -2, -10,  -6,  -4,  -2,
         16,   0,   0,   0,  16,   0,   0,   0,
          0,  16,   0,   0,   0,  16,   0,   0,
          0,   0,  16,   0,   0,   0,  16,   0,
          0,   0,   0,  16,   0,   0,   0,  16,
         10,   6,   4,   2,   0,   0,   0,   0,
          0,   0,   0,   0,  10,   6,   4,   2,
          0,   0,   0,   0,   0,   0,   0,   0
    }, {
         -8,  -8,  -8,  -8,  -4,  -4,  -4,  -4,
          8,   0,   0,   0,   4,   0,   0,   0,
          0,   8,   0,   0,   0,   4,   0,   0,
          0,   0,   8,   0,   0,   0,   4,   0,
          0,   0,   0,   8,   0,   0,   0,   4,
         16,  16,  16,  16,   0,   0,   0,   0,
          0,   0,   0,   0,  16,  16,  16,  16,
          0,   0,   0,   0,   0,   0,   0,   0
    }, {
         -2,  -1,  -1,   0,  -1,  -1,  -1,  -1,
          8,   3,   2,   1,   4,   3,   2,   2,
          0,   8,   3,   2,   0,   4,   3,   2,
          0,   0,   8,   3,   0,   0,   4,   3,
          0,   0,   0,   8,   0,   0,   0,   4,
         10,   6,   4,   2,   3,   4,   4,   3,
          0,   0,   0,   0,  10,   6,   4,   3,
          0,   0,   0,   0,   0,   0,   0,   0
    }, {
        -12, -10,  -9,  -8, -10,  -9,  -8,  -7,
         14,   0,   0,   0,  12,   1,   0,   0,
          0,  14,   0,   0,   0,  12,   0,   0,
          0,   0,  14,   0,   0,   0,  12,   1,
          0,   0,   0,  14,   0,   0,   0,  12,
         14,  12,  11,  10,   0,   0,   1,   1,
          0,   0,   0,   0,  14,  12,  11,   9,
          0,   0,   0,   0,   0,   0,   0,   0
    }
};

static int stbv_av1_ipred_iclip(int v, int min, int max)
{
    return v < min ? min : v > max ? max : v;
}

static int stbv_av1_ipred_imin(int a, int b)
{
    return a < b ? a : b;
}

static int stbv_av1_ipred_imax(int a, int b)
{
    return a > b ? a : b;
}

static int stbv_av1_ipred_iabs(int a)
{
    return a < 0 ? -a : a;
}

static int stbv_av1_apply_sign(int a, int b)
{
    return b < 0 ? -a : a;
}

static int stbv_av1_ipred_ctz(unsigned v)
{
    int n = 0;
    while (!(v & 1u)) {
        v >>= 1;
        n++;
    }
    return n;
}

/* dav1d get_upsample(). */
static int stbv_av1_get_upsample(int wh, int angle, int is_sm)
{
    return angle < 40 && wh <= (16 >> is_sm);
}

/* dav1d get_filter_strength(). */
static int stbv_av1_get_filter_strength(int wh, int angle, int is_sm)
{
    if (is_sm) {
        if (wh <= 8) {
            if (angle >= 64) return 2;
            if (angle >= 40) return 1;
        } else if (wh <= 16) {
            if (angle >= 48) return 2;
            if (angle >= 20) return 1;
        } else if (wh <= 24) {
            if (angle >=  4) return 3;
        } else {
            return 3;
        }
    } else {
        if (wh <= 8) {
            if (angle >= 56) return 1;
        } else if (wh <= 16) {
            if (angle >= 40) return 1;
        } else if (wh <= 24) {
            if (angle >= 32) return 3;
            if (angle >= 16) return 2;
            if (angle >=  8) return 1;
        } else if (wh <= 32) {
            if (angle >= 32) return 3;
            if (angle >=  4) return 2;
            return 1;
        } else {
            return 3;
        }
    }
    return 0;
}

/*
 * Pixel-generic definitions.  Every function exists twice, with suffix
 * _8 over stbv_u8 and _16 over stbv_u16.  bd is the bitdepth (8 or 10);
 * clipping always uses (1 << bd) - 1 so both variants behave exactly like
 * the corresponding dav1d BITDEPTH instantiation.
 */
#define STBV_AV1_IPRED_DEF_DC(px, sfx) \
STBV_AV1_IPRED_UNUSED static void stbv_av1_splat_dc_##sfx( \
    px *dst, int stride, int w, int h, int dc) \
{ \
    int x, y; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x++) dst[x] = (px)dc; \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_cfl_pred_fill_##sfx( \
    px *dst, int stride, int w, int h, int dc, const stbv_i16 *ac, \
    int alpha, int bd) \
{ \
    int x, y; \
    const int maxv = (1 << bd) - 1; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x++) { \
            const int diff = alpha * ac[x]; \
            dst[x] = (px)stbv_av1_ipred_iclip( \
                dc + stbv_av1_apply_sign((stbv_av1_ipred_iabs(diff) + 32) >> 6, \
                                         diff), 0, maxv); \
        } \
        ac += w; \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static unsigned stbv_av1_dc_gen_top_##sfx( \
    const px *tl, int w) \
{ \
    unsigned dc = (unsigned)(w >> 1); \
    int i; \
    for (i = 0; i < w; i++) dc += tl[1 + i]; \
    return dc >> stbv_av1_ipred_ctz((unsigned)w); \
} \
STBV_AV1_IPRED_UNUSED static unsigned stbv_av1_dc_gen_left_##sfx( \
    const px *tl, int h) \
{ \
    unsigned dc = (unsigned)(h >> 1); \
    int i; \
    for (i = 0; i < h; i++) dc += tl[-(1 + i)]; \
    return dc >> stbv_av1_ipred_ctz((unsigned)h); \
} \
STBV_AV1_IPRED_UNUSED static unsigned stbv_av1_dc_gen_##sfx( \
    const px *tl, int w, int h, int bd) \
{ \
    unsigned dc = (unsigned)((w + h) >> 1); \
    int i; \
    for (i = 0; i < w; i++) dc += tl[i + 1]; \
    for (i = 0; i < h; i++) dc += tl[-(i + 1)]; \
    dc >>= stbv_av1_ipred_ctz((unsigned)(w + h)); \
    if (w != h) { \
        /* After the power-of-two shift above, the residual divisor is 3
         * when the sides differ by 2x (w+h = 3*2^k) and 5 when they
         * differ by 4x (w+h = 5*2^k).  dav1d MULTIPLIER_1x2 == 1/3
         * (0x5556@16 / 0xAAAB@17), MULTIPLIER_1x4 == 1/5
         * (0x3334@16 / 0x6667@17); match the ratio to the right one. */ \
        if (bd == 8) { \
            dc *= (unsigned)((w > h * 2 || h > w * 2) ? 0x3334 : 0x5556); \
            dc >>= 16; \
        } else { \
            dc *= (unsigned)((w > h * 2 || h > w * 2) ? 0x6667 : 0xAAAB); \
            dc >>= 17; \
        } \
    } \
    return dc; \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_dc_top_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    stbv_av1_splat_dc_##sfx(dst, stride, w, h, \
                            (int)stbv_av1_dc_gen_top_##sfx(tl, w)); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_dc_left_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    stbv_av1_splat_dc_##sfx(dst, stride, w, h, \
                            (int)stbv_av1_dc_gen_left_##sfx(tl, h)); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_dc_##sfx( \
    px *dst, int stride, const px *tl, int w, int h, int bd) \
{ \
    stbv_av1_splat_dc_##sfx(dst, stride, w, h, \
                            (int)stbv_av1_dc_gen_##sfx(tl, w, h, bd)); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_dc_128_##sfx( \
    px *dst, int stride, int w, int h, int bd) \
{ \
    stbv_av1_splat_dc_##sfx(dst, stride, w, h, 1 << (bd - 1)); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_cfl_##sfx( \
    px *dst, int stride, const px *tl, int w, int h, const stbv_i16 *ac, \
    int alpha, int bd) \
{ \
    stbv_av1_cfl_pred_fill_##sfx(dst, stride, w, h, \
                                 (int)stbv_av1_dc_gen_##sfx(tl, w, h, bd), \
                                 ac, alpha, bd); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_cfl_top_##sfx( \
    px *dst, int stride, const px *tl, int w, int h, const stbv_i16 *ac, \
    int alpha, int bd) \
{ \
    stbv_av1_cfl_pred_fill_##sfx(dst, stride, w, h, \
                                 (int)stbv_av1_dc_gen_top_##sfx(tl, w), \
                                 ac, alpha, bd); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_cfl_left_##sfx( \
    px *dst, int stride, const px *tl, int w, int h, const stbv_i16 *ac, \
    int alpha, int bd) \
{ \
    stbv_av1_cfl_pred_fill_##sfx(dst, stride, w, h, \
                                 (int)stbv_av1_dc_gen_left_##sfx(tl, h), \
                                 ac, alpha, bd); \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_cfl_128_##sfx( \
    px *dst, int stride, int w, int h, const stbv_i16 *ac, int alpha, \
    int bd) \
{ \
    stbv_av1_cfl_pred_fill_##sfx(dst, stride, w, h, 1 << (bd - 1), \
                                 ac, alpha, bd); \
}

#define STBV_AV1_IPRED_DEF_DIR(px, sfx) \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_v_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    int x, y; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x++) dst[x] = tl[1 + x]; \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_h_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    int x, y; \
    for (y = 0; y < h; y++) { \
        const px v = tl[-(1 + y)]; \
        for (x = 0; x < w; x++) dst[x] = v; \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_paeth_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    const int topleft = tl[0]; \
    int x, y; \
    for (y = 0; y < h; y++) { \
        const int left = tl[-(y + 1)]; \
        for (x = 0; x < w; x++) { \
            const int top = tl[1 + x]; \
            const int base = left + top - topleft; \
            const int ldiff = stbv_av1_ipred_iabs(left - base); \
            const int tdiff = stbv_av1_ipred_iabs(top - base); \
            const int tldiff = stbv_av1_ipred_iabs(topleft - base); \
            dst[x] = (px)(ldiff <= tdiff && ldiff <= tldiff ? left : \
                          tdiff <= tldiff ? top : topleft); \
        } \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_smooth_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    const unsigned char *const weights_hor = &stbv_av1_sm_weights[w]; \
    const unsigned char *const weights_ver = &stbv_av1_sm_weights[h]; \
    const int right = tl[w], bottom = tl[-h]; \
    int x, y; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x++) { \
            const int pred = weights_ver[y] * tl[1 + x] + \
                             (256 - weights_ver[y]) * bottom + \
                             weights_hor[x] * tl[-(1 + y)] + \
                             (256 - weights_hor[x]) * right; \
            dst[x] = (px)((pred + 256) >> 9); \
        } \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_smooth_v_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    const unsigned char *const weights_ver = &stbv_av1_sm_weights[h]; \
    const int bottom = tl[-h]; \
    int x, y; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x++) { \
            const int pred = weights_ver[y] * tl[1 + x] + \
                             (256 - weights_ver[y]) * bottom; \
            dst[x] = (px)((pred + 128) >> 8); \
        } \
        dst += stride; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_smooth_h_##sfx( \
    px *dst, int stride, const px *tl, int w, int h) \
{ \
    const unsigned char *const weights_hor = &stbv_av1_sm_weights[w]; \
    const int right = tl[w]; \
    int x, y; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x++) { \
            const int pred = weights_hor[x] * tl[-(y + 1)] + \
                             (256 - weights_hor[x]) * right; \
            dst[x] = (px)((pred + 128) >> 8); \
        } \
        dst += stride; \
    } \
}

#define STBV_AV1_IPRED_DEF_EDGEFN(px, sfx) \
STBV_AV1_IPRED_UNUSED static void stbv_av1_filter_edge_##sfx( \
    px *out, int sz, int lim_from, int lim_to, const px *in, int from, \
    int to, int strength) \
{ \
    static const int kernel[3][5] = { \
        { 0, 4, 8, 4, 0 }, \
        { 0, 5, 6, 5, 0 }, \
        { 2, 4, 4, 4, 2 } \
    }; \
    const int lim1 = sz < lim_from ? sz : lim_from; \
    const int lim2 = sz < lim_to ? sz : lim_to; \
    int i, j; \
    for (i = 0; i < lim1; i++) \
        out[i] = in[stbv_av1_ipred_iclip(i, from, to - 1)]; \
    for (; i < lim2; i++) { \
        int s = 0; \
        for (j = 0; j < 5; j++) \
            s += in[stbv_av1_ipred_iclip(i - 2 + j, from, to - 1)] * \
                 kernel[strength - 1][j]; \
        out[i] = (px)((s + 8) >> 4); \
    } \
    for (; i < sz; i++) \
        out[i] = in[stbv_av1_ipred_iclip(i, from, to - 1)]; \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_upsample_edge_##sfx( \
    px *out, int hsz, const px *in, int from, int to, int bd) \
{ \
    static const int kernel[4] = { -1, 9, 9, -1 }; \
    const int maxv = (1 << bd) - 1; \
    int i, j; \
    for (i = 0; i < hsz - 1; i++) { \
        int s = 0; \
        out[i * 2] = in[stbv_av1_ipred_iclip(i, from, to - 1)]; \
        for (j = 0; j < 4; j++) \
            s += in[stbv_av1_ipred_iclip(i + j - 1, from, to - 1)] * \
                 kernel[j]; \
        out[i * 2 + 1] = (px)stbv_av1_ipred_iclip((s + 8) >> 4, 0, maxv); \
    } \
    out[i * 2] = in[stbv_av1_ipred_iclip(i, from, to - 1)]; \
}

#define STBV_AV1_IPRED_DEF_Z(px, sfx) \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_z1_##sfx( \
    px *dst, int stride, const px *topleft_in, int width, int height, \
    int angle, int bd) \
{ \
    const int is_sm = (angle >> 9) & 0x1; \
    const int enable_intra_edge_filter = angle >> 10; \
    px top_out[256]; \
    const px *top; \
    int max_wh, upsample_above, base_inc, dx, max_base_x, y, x, xpos; \
    angle &= 511; \
    max_wh = width + height; \
    upsample_above = enable_intra_edge_filter ? \
        stbv_av1_get_upsample(max_wh, 90 - angle, is_sm) : 0; \
    base_inc = 1 + upsample_above; \
    dx = stbv_av1_dr_deriv[angle >> 1]; \
    if (upsample_above) { \
        stbv_av1_upsample_edge_##sfx(top_out, max_wh, &topleft_in[1], -1, \
                                     width + stbv_av1_ipred_imin(width, \
                                     height), bd); \
        top = top_out; \
        max_base_x = 2 * max_wh - 2; \
        dx <<= 1; \
    } else { \
        const int fs = enable_intra_edge_filter ? \
            stbv_av1_get_filter_strength(max_wh, 90 - angle, is_sm) : 0; \
        if (fs) { \
            stbv_av1_filter_edge_##sfx(top_out, max_wh, 0, max_wh, \
                                       &topleft_in[1], -1, \
                                       width + stbv_av1_ipred_imin(width, \
                                       height), fs); \
            top = top_out; \
            max_base_x = max_wh - 1; \
        } else { \
            top = &topleft_in[1]; \
            max_base_x = width + stbv_av1_ipred_imin(width, height) - 1; \
        } \
    } \
    for (y = 0, xpos = dx; y < height; y++, xpos += dx, dst += stride) { \
        const int frac = xpos & 0x3E; \
        int base = xpos >> 6; \
        for (x = 0; x < width; x++, base += base_inc) { \
            if (base < max_base_x) { \
                const int v = top[base] * (64 - frac) + \
                              top[base + 1] * frac; \
                dst[x] = (px)((v + 32) >> 6); \
            } else { \
                const px last = top[max_base_x]; \
                for (; x < width; x++) dst[x] = last; \
                break; \
            } \
        } \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_z2_##sfx( \
    px *dst, int stride, const px *topleft_in, int width, int height, \
    int angle, int max_width, int max_height, int bd) \
{ \
    const int is_sm = (angle >> 9) & 0x1; \
    const int enable_intra_edge_filter = angle >> 10; \
    px edge[512]; \
    px *const topleft = &edge[256]; \
    const px *left; \
    int max_wh, upsample_left, upsample_above, base_inc_x; \
    int dy, dx, y, x, ypos, xpos; \
    angle &= 511; \
    max_wh = width + height; \
    upsample_left = enable_intra_edge_filter ? \
        stbv_av1_get_upsample(max_wh, 180 - angle, is_sm) : 0; \
    upsample_above = enable_intra_edge_filter ? \
        stbv_av1_get_upsample(max_wh, angle - 90, is_sm) : 0; \
    base_inc_x = 1 + upsample_above; \
    dy = stbv_av1_dr_deriv[(angle - 90) >> 1]; \
    dx = stbv_av1_dr_deriv[(180 - angle) >> 1]; \
    if (upsample_above) { \
        stbv_av1_upsample_edge_##sfx(topleft, width + 1, topleft_in, 0, \
                                     width + 1, bd); \
        dx <<= 1; \
    } else { \
        const int fs = enable_intra_edge_filter ? \
            stbv_av1_get_filter_strength(max_wh, angle - 90, is_sm) : 0; \
        if (fs) { \
            stbv_av1_filter_edge_##sfx(&topleft[1], width, 0, max_width, \
                                       &topleft_in[1], -1, width, fs); \
        } else { \
            for (y = 0; y < width; y++) \
                topleft[1 + y] = topleft_in[1 + y]; \
        } \
    } \
    if (upsample_left) { \
        stbv_av1_upsample_edge_##sfx(&topleft[-height * 2], height + 1, \
                                     &topleft_in[-height], 0, height + 1, \
                                     bd); \
        dy <<= 1; \
    } else { \
        const int fs = enable_intra_edge_filter ? \
            stbv_av1_get_filter_strength(max_wh, 180 - angle, is_sm) : 0; \
        if (fs) { \
            stbv_av1_filter_edge_##sfx(&topleft[-height], height, \
                                       height - max_height, height, \
                                       &topleft_in[-height], 0, height + 1, \
                                       fs); \
        } else { \
            for (y = 0; y < height; y++) \
                topleft[-height + y] = topleft_in[-height + y]; \
        } \
    } \
    *topleft = *topleft_in; \
    left = &topleft[-(1 + upsample_left)]; \
    for (y = 0, xpos = ((1 + upsample_above) << 6) - dx; y < height; \
         y++, xpos -= dx, dst += stride) { \
        int base_x = xpos >> 6; \
        const int frac_x = xpos & 0x3E; \
        ypos = (y << (6 + upsample_left)) - dy; \
        for (x = 0; x < width; x++, base_x += base_inc_x, ypos -= dy) { \
            int v; \
            if (base_x >= 0) { \
                v = topleft[base_x] * (64 - frac_x) + \
                    topleft[base_x + 1] * frac_x; \
            } else { \
                const int base_y = ypos >> 6; \
                const int frac_y = ypos & 0x3E; \
                v = left[-base_y] * (64 - frac_y) + \
                    left[-(base_y + 1)] * frac_y; \
            } \
            dst[x] = (px)((v + 32) >> 6); \
        } \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_z3_##sfx( \
    px *dst, int stride, const px *topleft_in, int width, int height, \
    int angle, int bd) \
{ \
    const int is_sm = (angle >> 9) & 0x1; \
    const int enable_intra_edge_filter = angle >> 10; \
    px left_out[256]; \
    const px *left; \
    int max_wh, upsample_left, base_inc, dy, max_base_y, x, y, ypos; \
    angle &= 511; \
    max_wh = width + height; \
    upsample_left = enable_intra_edge_filter ? \
        stbv_av1_get_upsample(max_wh, angle - 180, is_sm) : 0; \
    base_inc = 1 + upsample_left; \
    dy = stbv_av1_dr_deriv[(270 - angle) >> 1]; \
    if (upsample_left) { \
        stbv_av1_upsample_edge_##sfx(left_out, max_wh, \
                                     &topleft_in[-max_wh], \
                                     stbv_av1_ipred_imax(width - height, 0), \
                                     max_wh + 1, bd); \
        left = &left_out[2 * max_wh - 2]; \
        max_base_y = 2 * max_wh - 2; \
        dy <<= 1; \
    } else { \
        const int fs = enable_intra_edge_filter ? \
            stbv_av1_get_filter_strength(max_wh, angle - 180, is_sm) : 0; \
        if (fs) { \
            stbv_av1_filter_edge_##sfx(left_out, max_wh, 0, max_wh, \
                                       &topleft_in[-max_wh], \
                                       stbv_av1_ipred_imax(width - height, \
                                       0), max_wh + 1, fs); \
            left = &left_out[max_wh - 1]; \
            max_base_y = max_wh - 1; \
        } else { \
            left = &topleft_in[-1]; \
            max_base_y = height + stbv_av1_ipred_imin(width, height) - 1; \
        } \
    } \
    for (x = 0, ypos = dy; x < width; x++, ypos += dy) { \
        const int frac = ypos & 0x3E; \
        int base = ypos >> 6; \
        for (y = 0; y < height; y++, base += base_inc) { \
            if (base < max_base_y) { \
                const int v = left[-base] * (64 - frac) + \
                              left[-(base + 1)] * frac; \
                dst[y * stride + x] = (px)((v + 32) >> 6); \
            } else { \
                const px last = left[-max_base_y]; \
                for (; y < height; y++) dst[y * stride + x] = last; \
                break; \
            } \
        } \
    } \
}

#define STBV_AV1_IPRED_DEF_FILTER(px, sfx) \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_filter_##sfx( \
    px *dst, int stride, const px *topleft_in, int width, int height, \
    int filt_idx, int bd) \
{ \
    const signed char *filter = \
        stbv_av1_filter_intra_taps[filt_idx & 511]; \
    const px *top = &topleft_in[1]; \
    const int maxv = (1 << bd) - 1; \
    int y, x, yy, xx; \
    for (y = 0; y < height; y += 2) { \
        const px *topleft = &topleft_in[-y]; \
        const px *left = &topleft[-1]; \
        int left_stride = -1; \
        for (x = 0; x < width; x += 4) { \
            const int p0 = *topleft; \
            const int p1 = top[0], p2 = top[1], p3 = top[2], p4 = top[3]; \
            const int p5 = left[0], p6 = left[left_stride]; \
            px *ptr = &dst[x]; \
            for (yy = 0; yy < 2; yy++) { \
                for (xx = 0; xx < 4; xx++) { \
                    const int k = yy * 4 + xx; \
                    const int acc = filter[k] * p0 + \
                                    filter[k + 8] * p1 + \
                                    filter[k + 16] * p2 + \
                                    filter[k + 24] * p3 + \
                                    filter[k + 32] * p4 + \
                                    filter[k + 40] * p5 + \
                                    filter[k + 48] * p6; \
                    ptr[xx] = (px)stbv_av1_ipred_iclip((acc + 8) >> 4, \
                                                       0, maxv); \
                } \
                ptr += stride; \
            } \
            left = &dst[x + 4 - 1]; \
            left_stride = stride; \
            top += 4; \
            topleft = &top[-1]; \
        } \
        top = &dst[stride]; \
        dst = &dst[stride * 2]; \
    } \
}

#define STBV_AV1_IPRED_DEF_PREPARE(px, sfx) \
STBV_AV1_IPRED_UNUSED static int stbv_av1_prepare_intra_edges_##sfx( \
    int x, int have_left, int y, int have_top, int w, int h, \
    int edge_flags, const px *dst, int stride, const px *sb_edge, \
    int mode, int *angle, int tw, int th, int filter_edge, \
    px *topleft_out, int bd) \
{ \
    /* [mode][have_left][have_top], only DC and PAETH rows are used. */ \
    static const unsigned char mode_conv[13][2][2] = { \
        { { STBV_AV1_IPRED_DC_128, STBV_AV1_IPRED_TOP_DC }, \
          { STBV_AV1_IPRED_LEFT_DC, STBV_AV1_IPRED_DC } }, \
        { { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, \
        { { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, \
        { { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, \
        { { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, \
        { { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, \
        { { 0, 0 }, { 0, 0 } }, \
        { { STBV_AV1_IPRED_DC_128, STBV_AV1_IPRED_VERT }, \
          { STBV_AV1_IPRED_HOR, STBV_AV1_IPRED_PAETH } } \
    }; \
    static const unsigned char mode_to_angle_map[8] = { \
        90, 180, 45, 135, 113, 157, 203, 67 \
    }; \
    static const unsigned char needs[20] = { \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT, \
        STBV_AV1_IPRED_NT, \
        STBV_AV1_IPRED_NL, \
        0, 0, 0, 0, 0, 0, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT | STBV_AV1_IPRED_NTL, \
        STBV_AV1_IPRED_NL, \
        STBV_AV1_IPRED_NT, \
        0, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT | STBV_AV1_IPRED_NTL, \
        STBV_AV1_IPRED_NT | STBV_AV1_IPRED_NTR | STBV_AV1_IPRED_NTL, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NT | STBV_AV1_IPRED_NTL, \
        STBV_AV1_IPRED_NL | STBV_AV1_IPRED_NBL | STBV_AV1_IPRED_NTL \
    }; \
    const int mid = (1 << bd) >> 1; \
    const px *dst_top = 0; \
    int sz, px_have, i; \
    if (mode >= STBV_AV1_IPRED_VERT && mode <= STBV_AV1_IPRED_VL) { \
        *angle = mode_to_angle_map[mode - STBV_AV1_IPRED_VERT] + 3 * *angle; \
        if (*angle <= 90) \
            mode = (*angle < 90 && have_top) ? STBV_AV1_IPRED_Z1 \
                                             : STBV_AV1_IPRED_VERT; \
        else if (*angle < 180) \
            mode = STBV_AV1_IPRED_Z2; \
        else \
            mode = (*angle > 180 && have_left) ? STBV_AV1_IPRED_Z3 \
                                               : STBV_AV1_IPRED_HOR; \
    } else if (mode == STBV_AV1_IPRED_DC || mode == STBV_AV1_IPRED_PAETH) { \
        mode = mode_conv[mode][have_left][have_top]; \
    } \
    if (have_top && \
        ((needs[mode] & (STBV_AV1_IPRED_NT | STBV_AV1_IPRED_NTL)) || \
         ((needs[mode] & STBV_AV1_IPRED_NL) && !have_left))) \
    { \
        if (sb_edge) \
            dst_top = &sb_edge[x * 4]; \
        else \
            dst_top = &dst[-stride]; \
    } \
    if (needs[mode] & STBV_AV1_IPRED_NL) { \
        px *const left = &topleft_out[-(th << 2)]; \
        sz = th << 2; \
        if (have_left) { \
            px_have = stbv_av1_ipred_imin(sz, (h - y) << 2); \
            for (i = 0; i < px_have; i++) \
                left[sz - 1 - i] = dst[stride * i - 1]; \
            if (px_have < sz) { \
                const px v = left[sz - px_have]; \
                for (i = 0; i < sz - px_have; i++) left[i] = v; \
            } \
        } else { \
            const px v = have_top ? *dst_top : (px)(mid + 1); \
            for (i = 0; i < sz; i++) left[i] = v; \
        } \
        if (needs[mode] & STBV_AV1_IPRED_NBL) { \
            const int have_bottomleft = \
                (!have_left || y + th >= h) ? 0 : \
                (edge_flags & STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM); \
            if (have_bottomleft) { \
                px_have = stbv_av1_ipred_imin(sz, (h - y - th) << 2); \
                for (i = 0; i < px_have; i++) \
                    left[-(i + 1)] = dst[stride * (sz + i) - 1]; \
                if (px_have < sz) { \
                    const px v = left[-px_have]; \
                    for (i = 0; i < sz - px_have; i++) left[-sz + i] = v; \
                } \
            } else { \
                const px v = left[0]; \
                for (i = 0; i < sz; i++) left[-sz + i] = v; \
            } \
        } \
    } \
    if (needs[mode] & STBV_AV1_IPRED_NT) { \
        px *const top = &topleft_out[1]; \
        sz = tw << 2; \
        if (have_top) { \
            px_have = stbv_av1_ipred_imin(sz, (w - x) << 2); \
            for (i = 0; i < px_have; i++) top[i] = dst_top[i]; \
            if (px_have < sz) { \
                const px v = top[px_have - 1]; \
                for (i = px_have; i < sz; i++) top[i] = v; \
            } \
        } else { \
            const px v = have_left ? dst[-1] : (px)(mid - 1); \
            for (i = 0; i < sz; i++) top[i] = v; \
        } \
        if (needs[mode] & STBV_AV1_IPRED_NTR) { \
            const int have_topright = \
                (!have_top || x + tw >= w) ? 0 : \
                (edge_flags & STBV_AV1_EDGE_I444_TOP_HAS_RIGHT); \
            if (have_topright) { \
                px_have = stbv_av1_ipred_imin(sz, (w - x - tw) << 2); \
                for (i = 0; i < px_have; i++) top[sz + i] = dst_top[sz + i]; \
                if (px_have < sz) { \
                    const px v = top[sz + px_have - 1]; \
                    for (i = px_have; i < sz; i++) top[sz + i] = v; \
                } \
            } else { \
                const px v = top[sz - 1]; \
                for (i = 0; i < sz; i++) top[sz + i] = v; \
            } \
        } \
    } \
    if (needs[mode] & STBV_AV1_IPRED_NTL) { \
        if (have_left) \
            topleft_out[0] = have_top ? dst_top[-1] : dst[-1]; \
        else \
            topleft_out[0] = have_top ? *dst_top : (px)mid; \
        if (mode == STBV_AV1_IPRED_Z2 && tw + th >= 6 && filter_edge) \
            topleft_out[0] = (px)(((topleft_out[-1] + topleft_out[1]) * 5 + \
                                   topleft_out[0] * 6 + 8) >> 4); \
    } \
    return mode; \
}

#define STBV_AV1_IPRED_DEF_RUN(px, sfx) \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_run_##sfx( \
    int mode, px *dst, int stride, const px *tl, int w, int h, int angle, \
    int filt_idx, int max_w, int max_h, int bd) \
{ \
    switch (mode) { \
    case STBV_AV1_IPRED_DC: \
        stbv_av1_ipred_dc_##sfx(dst, stride, tl, w, h, bd); \
        break; \
    case STBV_AV1_IPRED_VERT: \
        stbv_av1_ipred_v_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_HOR: \
        stbv_av1_ipred_h_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_SMOOTH: \
        stbv_av1_ipred_smooth_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_SMOOTH_V: \
        stbv_av1_ipred_smooth_v_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_SMOOTH_H: \
        stbv_av1_ipred_smooth_h_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_PAETH: \
        stbv_av1_ipred_paeth_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_LEFT_DC: \
        stbv_av1_ipred_dc_left_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_TOP_DC: \
        stbv_av1_ipred_dc_top_##sfx(dst, stride, tl, w, h); \
        break; \
    case STBV_AV1_IPRED_DC_128: \
        stbv_av1_ipred_dc_128_##sfx(dst, stride, w, h, bd); \
        break; \
    case STBV_AV1_IPRED_FILTER: \
        stbv_av1_ipred_filter_##sfx(dst, stride, tl, w, h, filt_idx, bd); \
        break; \
    case STBV_AV1_IPRED_Z1: \
        stbv_av1_ipred_z1_##sfx(dst, stride, tl, w, h, angle, bd); \
        break; \
    case STBV_AV1_IPRED_Z2: \
        stbv_av1_ipred_z2_##sfx(dst, stride, tl, w, h, angle, max_w, \
                                max_h, bd); \
        break; \
    case STBV_AV1_IPRED_Z3: \
        stbv_av1_ipred_z3_##sfx(dst, stride, tl, w, h, angle, bd); \
        break; \
    default: \
        break; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_ipred_cfl_run_##sfx( \
    int mode, px *dst, int stride, const px *tl, int w, int h, \
    const stbv_i16 *ac, int alpha, int bd) \
{ \
    switch (mode) { \
    case STBV_AV1_IPRED_DC: \
        stbv_av1_ipred_cfl_##sfx(dst, stride, tl, w, h, ac, alpha, bd); \
        break; \
    case STBV_AV1_IPRED_TOP_DC: \
        stbv_av1_ipred_cfl_top_##sfx(dst, stride, tl, w, h, ac, alpha, bd); \
        break; \
    case STBV_AV1_IPRED_LEFT_DC: \
        stbv_av1_ipred_cfl_left_##sfx(dst, stride, tl, w, h, ac, alpha, bd); \
        break; \
    case STBV_AV1_IPRED_DC_128: \
        stbv_av1_ipred_cfl_128_##sfx(dst, stride, w, h, ac, alpha, bd); \
        break; \
    default: \
        break; \
    } \
} \
STBV_AV1_IPRED_UNUSED static void stbv_av1_pal_pred_##sfx( \
    px *dst, int stride, const px *pal, const stbv_u8 *idx, int w, int h) \
{ \
    int x, y; \
    for (y = 0; y < h; y++) { \
        for (x = 0; x < w; x += 2) { \
            const int i = *idx++; \
            dst[x] = pal[i & 7]; \
            dst[x + 1] = pal[i >> 4]; \
        } \
        dst += stride; \
    } \
}

STBV_AV1_IPRED_DEF_DC(stbv_u8, 8)
STBV_AV1_IPRED_DEF_DIR(stbv_u8, 8)
STBV_AV1_IPRED_DEF_EDGEFN(stbv_u8, 8)
STBV_AV1_IPRED_DEF_Z(stbv_u8, 8)
STBV_AV1_IPRED_DEF_FILTER(stbv_u8, 8)
STBV_AV1_IPRED_DEF_PREPARE(stbv_u8, 8)
STBV_AV1_IPRED_DEF_RUN(stbv_u8, 8)

STBV_AV1_IPRED_DEF_DC(stbv_u16, 16)
STBV_AV1_IPRED_DEF_DIR(stbv_u16, 16)
STBV_AV1_IPRED_DEF_EDGEFN(stbv_u16, 16)
STBV_AV1_IPRED_DEF_Z(stbv_u16, 16)
STBV_AV1_IPRED_DEF_FILTER(stbv_u16, 16)
STBV_AV1_IPRED_DEF_PREPARE(stbv_u16, 16)
STBV_AV1_IPRED_DEF_RUN(stbv_u16, 16)

#endif

/* ===== stb_av1_leaf.h ===== */
/*
 * stb_av1_leaf.h - first scalar intra leaf syntax integration
 *
 * The ordering follows dav1d 1.5.4 read_b()/decode_coefs(): after intra
 * syntax, select the maximum transform, optionally decode tx-size, then at
 * each transform leaf decode coefficient skip and (when required) transform
 * type.  Reconstruction is intentionally left to the next layer.
 */
#ifndef STB_AV1_LEAF_H
#define STB_AV1_LEAF_H

#include <string.h>

#ifndef STB_AV1_TXSTATE_H
#error "include stb_av1_txstate.h first"
#endif
#ifndef STB_AV1_STATE_H
#error "include stb_av1_state.h first"
#endif

/* neg_deinterleave: decode segment ID diff (dav1d decode.c) */
static int stb_neg_deinterleave(int diff, int ref, int max)
{
    if (!ref) return diff;
    if (ref >= (max - 1)) return max - diff - 1;
    if (2 * ref < max) {
        if (diff <= 2 * ref) {
            if (diff & 1) return ref + ((diff + 1) >> 1);
            else return ref - (diff >> 1);
        }
        return diff;
    } else {
        if (diff <= 2 * (max - ref - 1)) {
            if (diff & 1) return ref + ((diff + 1) >> 1);
            else return ref - (diff >> 1);
        }
        return max - (diff + 1);
    }
}
#ifndef STB_AV1_COEF_H
#error "include stb_av1_coef.h first"
#endif
#ifndef STB_AV1_QUANT_H

/* ===== stb_av1_quant.h ===== */
/* stb_av1_quant.h - AV1 dequantization table (dc, ac) per qindex.
 * Values transcribed from dav1d 1.5.4 src/dequant_tables.c
 * dav1d_dq_tbl[3][QINDEX_RANGE][2].  Index 0 = 8-bit, 1 = 10-bit,
 * 2 = 12-bit. */
#ifndef STB_AV1_QUANT_H
#define STB_AV1_QUANT_H

#define STBV_AV1_QINDEX_RANGE 256

static const unsigned short stbv_av1_dq_tbl[3][STBV_AV1_QINDEX_RANGE][2] = {
    {
        {    4,    4 }, {    8,    8 }, {    8,    9 }, {    9,   10 },
        {   10,   11 }, {   11,   12 }, {   12,   13 }, {   12,   14 },
        {   13,   15 }, {   14,   16 }, {   15,   17 }, {   16,   18 },
        {   17,   19 }, {   18,   20 }, {   19,   21 }, {   19,   22 },
        {   20,   23 }, {   21,   24 }, {   22,   25 }, {   23,   26 },
        {   24,   27 }, {   25,   28 }, {   26,   29 }, {   26,   30 },
        {   27,   31 }, {   28,   32 }, {   29,   33 }, {   30,   34 },
        {   31,   35 }, {   32,   36 }, {   32,   37 }, {   33,   38 },
        {   34,   39 }, {   35,   40 }, {   36,   41 }, {   37,   42 },
        {   38,   43 }, {   38,   44 }, {   39,   45 }, {   40,   46 },
        {   41,   47 }, {   42,   48 }, {   43,   49 }, {   43,   50 },
        {   44,   51 }, {   45,   52 }, {   46,   53 }, {   47,   54 },
        {   48,   55 }, {   48,   56 }, {   49,   57 }, {   50,   58 },
        {   51,   59 }, {   52,   60 }, {   53,   61 }, {   53,   62 },
        {   54,   63 }, {   55,   64 }, {   56,   65 }, {   57,   66 },
        {   57,   67 }, {   58,   68 }, {   59,   69 }, {   60,   70 },
        {   61,   71 }, {   62,   72 }, {   62,   73 }, {   63,   74 },
        {   64,   75 }, {   65,   76 }, {   66,   77 }, {   66,   78 },
        {   67,   79 }, {   68,   80 }, {   69,   81 }, {   70,   82 },
        {   70,   83 }, {   71,   84 }, {   72,   85 }, {   73,   86 },
        {   74,   87 }, {   74,   88 }, {   75,   89 }, {   76,   90 },
        {   77,   91 }, {   78,   92 }, {   78,   93 }, {   79,   94 },
        {   80,   95 }, {   81,   96 }, {   81,   97 }, {   82,   98 },
        {   83,   99 }, {   84,  100 }, {   85,  101 }, {   85,  102 },
        {   87,  104 }, {   88,  106 }, {   90,  108 }, {   92,  110 },
        {   93,  112 }, {   95,  114 }, {   96,  116 }, {   98,  118 },
        {   99,  120 }, {  101,  122 }, {  102,  124 }, {  104,  126 },
        {  105,  128 }, {  107,  130 }, {  108,  132 }, {  110,  134 },
        {  111,  136 }, {  113,  138 }, {  114,  140 }, {  116,  142 },
        {  117,  144 }, {  118,  146 }, {  120,  148 }, {  121,  150 },
        {  123,  152 }, {  125,  155 }, {  127,  158 }, {  129,  161 },
        {  131,  164 }, {  134,  167 }, {  136,  170 }, {  138,  173 },
        {  140,  176 }, {  142,  179 }, {  144,  182 }, {  146,  185 },
        {  148,  188 }, {  150,  191 }, {  152,  194 }, {  154,  197 },
        {  156,  200 }, {  158,  203 }, {  161,  207 }, {  164,  211 },
        {  166,  215 }, {  169,  219 }, {  172,  223 }, {  174,  227 },
        {  177,  231 }, {  180,  235 }, {  182,  239 }, {  185,  243 },
        {  187,  247 }, {  190,  251 }, {  192,  255 }, {  195,  260 },
        {  199,  265 }, {  202,  270 }, {  205,  275 }, {  208,  280 },
        {  211,  285 }, {  214,  290 }, {  217,  295 }, {  220,  300 },
        {  223,  305 }, {  226,  311 }, {  230,  317 }, {  233,  323 },
        {  237,  329 }, {  240,  335 }, {  243,  341 }, {  247,  347 },
        {  250,  353 }, {  253,  359 }, {  257,  366 }, {  261,  373 },
        {  265,  380 }, {  269,  387 }, {  272,  394 }, {  276,  401 },
        {  280,  408 }, {  284,  416 }, {  288,  424 }, {  292,  432 },
        {  296,  440 }, {  300,  448 }, {  304,  456 }, {  309,  465 },
        {  313,  474 }, {  317,  483 }, {  322,  492 }, {  326,  501 },
        {  330,  510 }, {  335,  520 }, {  340,  530 }, {  344,  540 },
        {  349,  550 }, {  354,  560 }, {  359,  571 }, {  364,  582 },
        {  369,  593 }, {  374,  604 }, {  379,  615 }, {  384,  627 },
        {  389,  639 }, {  395,  651 }, {  400,  663 }, {  406,  676 },
        {  411,  689 }, {  417,  702 }, {  423,  715 }, {  429,  729 },
        {  435,  743 }, {  441,  757 }, {  447,  771 }, {  454,  786 },
        {  461,  801 }, {  467,  816 }, {  475,  832 }, {  482,  848 },
        {  489,  864 }, {  497,  881 }, {  505,  898 }, {  513,  915 },
        {  522,  933 }, {  530,  951 }, {  539,  969 }, {  549,  988 },
        {  559, 1007 }, {  569, 1026 }, {  579, 1046 }, {  590, 1066 },
        {  602, 1087 }, {  614, 1108 }, {  626, 1129 }, {  640, 1151 },
        {  654, 1173 }, {  668, 1196 }, {  684, 1219 }, {  700, 1243 },
        {  717, 1267 }, {  736, 1292 }, {  755, 1317 }, {  775, 1343 },
        {  796, 1369 }, {  819, 1396 }, {  843, 1423 }, {  869, 1451 },
        {  896, 1479 }, {  925, 1508 }, {  955, 1537 }, {  988, 1567 },
        { 1022, 1597 }, { 1058, 1628 }, { 1098, 1660 }, { 1139, 1692 },
        { 1184, 1725 }, { 1232, 1759 }, { 1282, 1793 }, { 1336, 1828 },
    },
    {
        {    4,    4 }, {    9,    9 }, {   10,   11 }, {   13,   13 },
        {   15,   16 }, {   17,   18 }, {   20,   21 }, {   22,   24 },
        {   25,   27 }, {   28,   30 }, {   31,   33 }, {   34,   37 },
        {   37,   40 }, {   40,   44 }, {   43,   48 }, {   47,   51 },
        {   50,   55 }, {   53,   59 }, {   57,   63 }, {   60,   67 },
        {   64,   71 }, {   68,   75 }, {   71,   79 }, {   75,   83 },
        {   78,   88 }, {   82,   92 }, {   86,   96 }, {   90,  100 },
        {   93,  105 }, {   97,  109 }, {  101,  114 }, {  105,  118 },
        {  109,  122 }, {  113,  127 }, {  116,  131 }, {  120,  136 },
        {  124,  140 }, {  128,  145 }, {  132,  149 }, {  136,  154 },
        {  140,  158 }, {  143,  163 }, {  147,  168 }, {  151,  172 },
        {  155,  177 }, {  159,  181 }, {  163,  186 }, {  166,  190 },
        {  170,  195 }, {  174,  199 }, {  178,  204 }, {  182,  208 },
        {  185,  213 }, {  189,  217 }, {  193,  222 }, {  197,  226 },
        {  200,  231 }, {  204,  235 }, {  208,  240 }, {  212,  244 },
        {  215,  249 }, {  219,  253 }, {  223,  258 }, {  226,  262 },
        {  230,  267 }, {  233,  271 }, {  237,  275 }, {  241,  280 },
        {  244,  284 }, {  248,  289 }, {  251,  293 }, {  255,  297 },
        {  259,  302 }, {  262,  306 }, {  266,  311 }, {  269,  315 },
        {  273,  319 }, {  276,  324 }, {  280,  328 }, {  283,  332 },
        {  287,  337 }, {  290,  341 }, {  293,  345 }, {  297,  349 },
        {  300,  354 }, {  304,  358 }, {  307,  362 }, {  310,  367 },
        {  314,  371 }, {  317,  375 }, {  321,  379 }, {  324,  384 },
        {  327,  388 }, {  331,  392 }, {  334,  396 }, {  337,  401 },
        {  343,  409 }, {  350,  417 }, {  356,  425 }, {  362,  433 },
        {  369,  441 }, {  375,  449 }, {  381,  458 }, {  387,  466 },
        {  394,  474 }, {  400,  482 }, {  406,  490 }, {  412,  498 },
        {  418,  506 }, {  424,  514 }, {  430,  523 }, {  436,  531 },
        {  442,  539 }, {  448,  547 }, {  454,  555 }, {  460,  563 },
        {  466,  571 }, {  472,  579 }, {  478,  588 }, {  484,  596 },
        {  490,  604 }, {  499,  616 }, {  507,  628 }, {  516,  640 },
        {  525,  652 }, {  533,  664 }, {  542,  676 }, {  550,  688 },
        {  559,  700 }, {  567,  713 }, {  576,  725 }, {  584,  737 },
        {  592,  749 }, {  601,  761 }, {  609,  773 }, {  617,  785 },
        {  625,  797 }, {  634,  809 }, {  644,  825 }, {  655,  841 },
        {  666,  857 }, {  676,  873 }, {  687,  889 }, {  698,  905 },
        {  708,  922 }, {  718,  938 }, {  729,  954 }, {  739,  970 },
        {  749,  986 }, {  759, 1002 }, {  770, 1018 }, {  782, 1038 },
        {  795, 1058 }, {  807, 1078 }, {  819, 1098 }, {  831, 1118 },
        {  844, 1138 }, {  856, 1158 }, {  868, 1178 }, {  880, 1198 },
        {  891, 1218 }, {  906, 1242 }, {  920, 1266 }, {  933, 1290 },
        {  947, 1314 }, {  961, 1338 }, {  975, 1362 }, {  988, 1386 },
        { 1001, 1411 }, { 1015, 1435 }, { 1030, 1463 }, { 1045, 1491 },
        { 1061, 1519 }, { 1076, 1547 }, { 1090, 1575 }, { 1105, 1603 },
        { 1120, 1631 }, { 1137, 1663 }, { 1153, 1695 }, { 1170, 1727 },
        { 1186, 1759 }, { 1202, 1791 }, { 1218, 1823 }, { 1236, 1859 },
        { 1253, 1895 }, { 1271, 1931 }, { 1288, 1967 }, { 1306, 2003 },
        { 1323, 2039 }, { 1342, 2079 }, { 1361, 2119 }, { 1379, 2159 },
        { 1398, 2199 }, { 1416, 2239 }, { 1436, 2283 }, { 1456, 2327 },
        { 1476, 2371 }, { 1496, 2415 }, { 1516, 2459 }, { 1537, 2507 },
        { 1559, 2555 }, { 1580, 2603 }, { 1601, 2651 }, { 1624, 2703 },
        { 1647, 2755 }, { 1670, 2807 }, { 1692, 2859 }, { 1717, 2915 },
        { 1741, 2971 }, { 1766, 3027 }, { 1791, 3083 }, { 1817, 3143 },
        { 1844, 3203 }, { 1871, 3263 }, { 1900, 3327 }, { 1929, 3391 },
        { 1958, 3455 }, { 1990, 3523 }, { 2021, 3591 }, { 2054, 3659 },
        { 2088, 3731 }, { 2123, 3803 }, { 2159, 3876 }, { 2197, 3952 },
        { 2236, 4028 }, { 2276, 4104 }, { 2319, 4184 }, { 2363, 4264 },
        { 2410, 4348 }, { 2458, 4432 }, { 2508, 4516 }, { 2561, 4604 },
        { 2616, 4692 }, { 2675, 4784 }, { 2737, 4876 }, { 2802, 4972 },
        { 2871, 5068 }, { 2944, 5168 }, { 3020, 5268 }, { 3102, 5372 },
        { 3188, 5476 }, { 3280, 5584 }, { 3375, 5692 }, { 3478, 5804 },
        { 3586, 5916 }, { 3702, 6032 }, { 3823, 6148 }, { 3953, 6268 },
        { 4089, 6388 }, { 4236, 6512 }, { 4394, 6640 }, { 4559, 6768 },
        { 4737, 6900 }, { 4929, 7036 }, { 5130, 7172 }, { 5347, 7312 },
    },
    {
        {    4,    4 }, {   12,   13 }, {   18,   19 }, {   25,   27 },
        {   33,   35 }, {   41,   44 }, {   50,   54 }, {   60,   64 },
        {   70,   75 }, {   80,   87 }, {   91,   99 }, {  103,  112 },
        {  115,  126 }, {  127,  139 }, {  140,  154 }, {  153,  168 },
        {  166,  183 }, {  180,  199 }, {  194,  214 }, {  208,  230 },
        {  222,  247 }, {  237,  263 }, {  251,  280 }, {  266,  297 },
        {  281,  314 }, {  296,  331 }, {  312,  349 }, {  327,  366 },
        {  343,  384 }, {  358,  402 }, {  374,  420 }, {  390,  438 },
        {  405,  456 }, {  421,  475 }, {  437,  493 }, {  453,  511 },
        {  469,  530 }, {  484,  548 }, {  500,  567 }, {  516,  586 },
        {  532,  604 }, {  548,  623 }, {  564,  642 }, {  580,  660 },
        {  596,  679 }, {  611,  698 }, {  627,  716 }, {  643,  735 },
        {  659,  753 }, {  674,  772 }, {  690,  791 }, {  706,  809 },
        {  721,  828 }, {  737,  846 }, {  752,  865 }, {  768,  884 },
        {  783,  902 }, {  798,  920 }, {  814,  939 }, {  829,  957 },
        {  844,  976 }, {  859,  994 }, {  874, 1012 }, {  889, 1030 },
        {  904, 1049 }, {  919, 1067 }, {  934, 1085 }, {  949, 1103 },
        {  964, 1121 }, {  978, 1139 }, {  993, 1157 }, { 1008, 1175 },
        { 1022, 1193 }, { 1037, 1211 }, { 1051, 1229 }, { 1065, 1246 },
        { 1080, 1264 }, { 1094, 1282 }, { 1108, 1299 }, { 1122, 1317 },
        { 1136, 1335 }, { 1151, 1352 }, { 1165, 1370 }, { 1179, 1387 },
        { 1192, 1405 }, { 1206, 1422 }, { 1220, 1440 }, { 1234, 1457 },
        { 1248, 1474 }, { 1261, 1491 }, { 1275, 1509 }, { 1288, 1526 },
        { 1302, 1543 }, { 1315, 1560 }, { 1329, 1577 }, { 1342, 1595 },
        { 1368, 1627 }, { 1393, 1660 }, { 1419, 1693 }, { 1444, 1725 },
        { 1469, 1758 }, { 1494, 1791 }, { 1519, 1824 }, { 1544, 1856 },
        { 1569, 1889 }, { 1594, 1922 }, { 1618, 1954 }, { 1643, 1987 },
        { 1668, 2020 }, { 1692, 2052 }, { 1717, 2085 }, { 1741, 2118 },
        { 1765, 2150 }, { 1789, 2183 }, { 1814, 2216 }, { 1838, 2248 },
        { 1862, 2281 }, { 1885, 2313 }, { 1909, 2346 }, { 1933, 2378 },
        { 1957, 2411 }, { 1992, 2459 }, { 2027, 2508 }, { 2061, 2556 },
        { 2096, 2605 }, { 2130, 2653 }, { 2165, 2701 }, { 2199, 2750 },
        { 2233, 2798 }, { 2267, 2847 }, { 2300, 2895 }, { 2334, 2943 },
        { 2367, 2992 }, { 2400, 3040 }, { 2434, 3088 }, { 2467, 3137 },
        { 2499, 3185 }, { 2532, 3234 }, { 2575, 3298 }, { 2618, 3362 },
        { 2661, 3426 }, { 2704, 3491 }, { 2746, 3555 }, { 2788, 3619 },
        { 2830, 3684 }, { 2872, 3748 }, { 2913, 3812 }, { 2954, 3876 },
        { 2995, 3941 }, { 3036, 4005 }, { 3076, 4069 }, { 3127, 4149 },
        { 3177, 4230 }, { 3226, 4310 }, { 3275, 4390 }, { 3324, 4470 },
        { 3373, 4550 }, { 3421, 4631 }, { 3469, 4711 }, { 3517, 4791 },
        { 3565, 4871 }, { 3621, 4967 }, { 3677, 5064 }, { 3733, 5160 },
        { 3788, 5256 }, { 3843, 5352 }, { 3897, 5448 }, { 3951, 5544 },
        { 4005, 5641 }, { 4058, 5737 }, { 4119, 5849 }, { 4181, 5961 },
        { 4241, 6073 }, { 4301, 6185 }, { 4361, 6297 }, { 4420, 6410 },
        { 4479, 6522 }, { 4546, 6650 }, { 4612, 6778 }, { 4677, 6906 },
        { 4742, 7034 }, { 4807, 7162 }, { 4871, 7290 }, { 4942, 7435 },
        { 5013, 7579 }, { 5083, 7723 }, { 5153, 7867 }, { 5222, 8011 },
        { 5291, 8155 }, { 5367, 8315 }, { 5442, 8475 }, { 5517, 8635 },
        { 5591, 8795 }, { 5665, 8956 }, { 5745, 9132 }, { 5825, 9308 },
        { 5905, 9484 }, { 5984, 9660 }, { 6063, 9836 }, { 6149, 10028 },
        { 6234, 10220 }, { 6319, 10412 }, { 6404, 10604 }, { 6495, 10812 },
        { 6587, 11020 }, { 6678, 11228 }, { 6769, 11437 }, { 6867, 11661 },
        { 6966, 11885 }, { 7064, 12109 }, { 7163, 12333 }, { 7269, 12573 },
        { 7376, 12813 }, { 7483, 13053 }, { 7599, 13309 }, { 7715, 13565 },
        { 7832, 13821 }, { 7958, 14093 }, { 8085, 14365 }, { 8214, 14637 },
        { 8352, 14925 }, { 8492, 15213 }, { 8635, 15502 }, { 8788, 15806 },
        { 8945, 16110 }, { 9104, 16414 }, { 9275, 16734 }, { 9450, 17054 },
        { 9639, 17390 }, { 9832, 17726 }, { 10031, 18062 }, { 10245, 18414 },
        { 10465, 18766 }, { 10702, 19134 }, { 10946, 19502 }, { 11210, 19886 },
        { 11482, 20270 }, { 11776, 20670 }, { 12081, 21070 }, { 12409, 21486 },
        { 12750, 21902 }, { 13118, 22334 }, { 13501, 22766 }, { 13913, 23214 },
        { 14343, 23662 }, { 14807, 24126 }, { 15290, 24590 }, { 15812, 25070 },
        { 16356, 25551 }, { 16943, 26047 }, { 17575, 26559 }, { 18237, 27071 },
        { 18949, 27599 }, { 19718, 28143 }, { 20521, 28687 }, { 21387, 29247 },
    }
};

#endif /* STB_AV1_QUANT_H */
#endif

/* Reconstruction callback interface (NULL-safe, for stb_avif integration). */
typedef struct stbv_av1_leaf_recon {
    void *ud;
    stbv_i32 *cf;
    void (*block_info)(void *ud, int intra, int bs, int bx4, int by4, int has_chroma, int cbw4, int cbh4, int uv_tx, int tx0, int pal_sz_y, int pal_sz_uv, int skip, int y_mode, int y_angle, int uv_mode, int uv_angle, int cfl_alpha_u, int cfl_alpha_v, int ibc_mv_y, int ibc_mv_x);
    void (*luma_txb)(void *ud, int x4, int y4, int tx, int txtp, int eob, stbv_i32 *cf);
    void (*chroma_txb)(void *ud, int pl, int x4, int y4, int tx, int txtp, int eob, stbv_i32 *cf);
    void (*luma_pal)(void *ud, const stbv_u8 *idx, int sz, int bw4, int bh4, const stbv_u16 *pal);
    void (*chroma_pal)(void *ud, int pl, const stbv_u8 *idx, int sz, int cbw4, int cbh4, const stbv_u16 *pal);
} stbv_av1_leaf_recon;

static const stbv_u8 stbv_av1_skip_ctx[5][5] = {
    { 1, 2, 2, 2, 3 },
    { 2, 4, 4, 4, 5 },
    { 2, 4, 4, 4, 5 },
    { 2, 4, 4, 4, 5 },
    { 3, 5, 5, 5, 6 }
};

/* Residual context is cul_level in bits 0..5 and dc-sign in bit 6.  Arrays
 * are frame-wide, indexed in 4x4 units. */
typedef struct stbv_av1_res_state {
    stbv_u8 *above;
    stbv_u8 *left;
    unsigned int above_n;
    unsigned int left_n;
    /* dav1d clips residual-context WRITES to the true plane extent
     * (imin(txw, f->bw - bx)); reads fall through into 0x40 padding.
     * 0 means "same as above_n/left_n". */
    unsigned int above_mark_n;
    unsigned int left_mark_n;
} stbv_av1_res_state;

static void stbv_av1_res_state_init(stbv_av1_res_state *s,
                                    stbv_u8 *above, unsigned int above_n,
                                    stbv_u8 *left, unsigned int left_n)
{
    if (!s) return;
    s->above = above;
    s->left = left;
    s->above_n = above_n;
    s->left_n = left_n;
    s->above_mark_n = above_n;
    s->left_mark_n = left_n;
    if (above) memset(above, 0x40, above_n);
    if (left) memset(left, 0x40, left_n);
}

static unsigned stbv_av1_res_merge(const stbv_u8 *p, int n)
{
    unsigned v = 0;
    int i;
    for (i = 0; i < n; i++)
        v |= p[i];
    return v;
}

/* Skip context, following dav1d's get_skip_ctx().  chroma == 0: luma branch
 * (equal block/transform dims give ctx 0, otherwise merge the residual
 * contexts over the transform width/height and look up the table).  chroma
 * != 0: the caller passes the chroma block/transform dims in chroma 4x4
 * units and gets dav1d's 7 + not_one_blk*3 + ca + cl.  Coordinates are
 * absolute (frame-wide arrays). */
static int stbv_av1_get_skip_ctx(const stbv_av1_res_state *s,
                                 int bx4, int by4,
                                 int bw4, int bh4,
                                 int txw4, int txh4,
                                 int chroma)
{
    stbv_u64 la, ll;
    int i;

    if (!s) return 0;
    if (chroma) {
        int not_one_blk, ca, cl;
        la = 0;
        ll = 0;
        for (i = 0; i < txw4 && (unsigned int)(bx4 + i) < s->above_n; i++)
            la |= s->above[bx4 + i];
        for (i = 0; i < txh4 && (unsigned int)(by4 + i) < s->left_n; i++)
            ll |= s->left[by4 + i];
        not_one_blk = (bw4 != txw4) || (bh4 != txh4);
        ca = (int)(la != 0x40);
        cl = (int)(ll != 0x40);
        ca = (int)(la != 0x40);
        cl = (int)(ll != 0x40);
        return 7 + not_one_blk * 3 + ca + cl;
    }
    if (bw4 == txw4 && bh4 == txh4)
        return 0;

    la = 0;
    ll = 0;
    for (i = 0; i < txw4 && (unsigned int)(bx4 + i) < s->above_n; i++)
        la |= s->above[bx4 + i];
    for (i = 0; i < txh4 && (unsigned int)(by4 + i) < s->left_n; i++)
        ll |= s->left[by4 + i];

    /* Collapse every context byte into the low byte, exactly like dav1d's
       MERGE_CTX (read N bytes, then OR-fold with >>16/>>8). */
    la |= la >> 32;
    la |= la >> 16;
    la |= la >> 8;
    ll |= ll >> 32;
    ll |= ll >> 16;
    ll |= ll >> 8;

    /* bit 6 is the DC-sign flag; skip context uses magnitude only. */
    la &= 0x3fU;
    ll &= 0x3fU;
    la = la > 4 ? 4 : la;
    ll = ll > 4 ? 4 : ll;
    return stbv_av1_skip_ctx[(int)la][(int)ll];
}

static void stbv_av1_res_mark(stbv_av1_res_state *s,
                              int bx4, int by4, int txw4, int txh4,
                              stbv_u8 res_ctx)
{
    int i;
    unsigned int n;
    if (!s) return;
    n = s->above_mark_n ? s->above_mark_n : s->above_n;
    for (i = 0; i < txw4 && (unsigned int)(bx4 + i) < n; i++)
        s->above[bx4 + i] = res_ctx;
    n = s->left_mark_n ? s->left_mark_n : s->left_n;
    for (i = 0; i < txh4 && (unsigned int)(by4 + i) < n; i++)
        s->left[by4 + i] = res_ctx;
}

/* dav1d's SKIP-block marking (memset_pow2) is NOT clipped to the
 * frame extent, unlike its coded-coefficient marking. */
static void stbv_av1_res_mark_unc(stbv_av1_res_state *s,
                                  int bx4, int by4, int txw4, int txh4,
                                  stbv_u8 res_ctx)
{
    int i;
    if (!s) return;
    for (i = 0; i < txw4 && (unsigned int)(bx4 + i) < s->above_n; i++)
        s->above[bx4 + i] = res_ctx;
    for (i = 0; i < txh4 && (unsigned int)(by4 + i) < s->left_n; i++)
        s->left[by4 + i] = res_ctx;
}

typedef struct stbv_av1_leaf_state_arrays {
    stbv_u8 *above_mode;
    unsigned int above_mode_n;
    stbv_u8 *left_mode;
    unsigned int left_mode_n;
    stbv_u8 *above_tx;
    unsigned int above_tx_n;
    stbv_u8 *left_tx;
    unsigned int left_tx_n;
    stbv_u8 *above_tx_intra;
    stbv_u8 *left_tx_intra;
    stbv_u8 *above_res;
    unsigned int above_res_n;
    stbv_u8 *left_res;
    unsigned int left_res_n;
    unsigned int above_res_mark_n;
    unsigned int left_res_mark_n;
    unsigned int above_cre_mark_n[2];
    unsigned int left_cre_mark_n[2];
    stbv_u8 *above_skip;
    unsigned int above_skip_n;
    stbv_u8 *left_skip;
    unsigned int left_skip_n;
    stbv_u8 *above_cre[2];
    unsigned int above_cre_n[2];
    stbv_u8 *left_cre[2];
    unsigned int left_cre_n[2];
    stbv_u8 *above_pal_sz;
    unsigned int above_pal_sz_n;
    stbv_u8 *left_pal_sz;
    unsigned int left_pal_sz_n;
    stbv_u8 *above_pal_uv;
    unsigned int above_pal_uv_n;
    stbv_u8 *left_pal_uv;
    unsigned int left_pal_uv_n;
    stbv_u16 *above_pal[2];
    unsigned int above_pal_n;
    stbv_u16 *left_pal[2];
    unsigned int left_pal_n;
    /* segment id context */
    stbv_u8 *above_seg_id;
    unsigned int above_seg_id_n;
    stbv_u8 *left_seg_id;
    unsigned int left_seg_id_n;
    /* IBC MV neighbour arrays for MV prediction (dav1d refmvs_find). */
    int *above_ibc_mv_y;
    int *above_ibc_mv_x;
    stbv_u8 *above_ibc_valid;
    unsigned int above_ibc_mv_n;
    int *left_ibc_mv_y;
    int *left_ibc_mv_x;
    stbv_u8 *left_ibc_valid;
    unsigned int left_ibc_mv_n;
    /* 2D refmvs block array for dav1d-compatible spatial MV prediction. */
    /* Each 4x4 position stores: mv_y, mv_x (1/8-pel), bs (block size enum),
     * and valid (1 = intra/IBC block coded at this position). */
    stbv_u8 *refmvs_r;       /* flat [frame_h4 * frame_w4] of stbv_refmvs_cell */
    unsigned int refmvs_stride;
    unsigned int refmvs_h4;
    unsigned int refmvs_w4;
} stbv_av1_leaf_state_arrays;

/* Minimal refmvs cell: MV + block size + validity, stored per 4x4 position.
 * Matches dav1d refmvs_block semantics for spatial candidate search. */
typedef struct stbv_refmvs_cell {
    int mv_y;       /* 1/8-pel luma units */
    int mv_x;       /* 1/8-pel luma units */
    stbv_u8 bs;     /* block size enum (STBV_AV1_BS_*) */
    stbv_u8 valid;  /* 1 = intra/IBC block */
    signed char ref;    /* reference frame: -1=intra, 0=current (IBC), >0=ref frame */
} stbv_refmvs_cell;

typedef struct stbv_av1_leaf_state {
    struct stb_av1_intra_state intra;
    stbv_av1_tx_state tx;
    stbv_av1_res_state res;
    stbv_av1_res_state cres[2];
    stbv_u8 *above_skip;
    stbv_u8 *left_skip;
    unsigned int above_skip_n;
    unsigned int left_skip_n;
    int cdef_sb_x;
    int cdef_sb_y;
    int cdef_idx[4];
    stbv_u8 *above_pal_sz;
    stbv_u8 *left_pal_sz;
    unsigned int above_pal_sz_n;
    unsigned int left_pal_sz_n;
    stbv_u8 *above_pal_uv;
    stbv_u8 *left_pal_uv;
    unsigned int above_pal_uv_n;
    unsigned int left_pal_uv_n;
    stbv_u16 *above_pal[2];
    stbv_u16 *left_pal[2];
    unsigned int above_pal_n;
    unsigned int left_pal_n;
    stbv_u16 pal_y[8];
    stbv_u16 pal_u[8];
    stbv_u16 pal_v[8];
    stbv_u16 cache[16];
    stbv_u16 used_cache[8];
    stbv_u8 pal_tmp[64 * 64];
    stbv_u8 pal_tmp_y[64 * 64];
    stbv_u8 pal_order[64][8];
    stbv_u8 pal_ctxs[64];
    int pal_sz_y;
    int pal_sz_uv;
    /* segment id context */
    stbv_u8 *above_seg_id;
    stbv_u8 *left_seg_id;
    unsigned int above_seg_id_n;
    unsigned int left_seg_id_n;
    /* CDEF index output grid (per-64x64 block) */
    int *cdef_idx_grid;
    int cdef_grid_stride;
    /* Per-SB quantizer/lf state (persists across leaf callbacks within a tile) */
    int last_qidx;
    int last_delta_lf[4];
    /* IBC MV neighbour arrays for MV prediction. */
    int *above_ibc_mv_y;
    int *above_ibc_mv_x;
    stbv_u8 *above_ibc_valid;
    unsigned int above_ibc_mv_n;
    int *left_ibc_mv_y;
    int *left_ibc_mv_x;
    stbv_u8 *left_ibc_valid;
    unsigned int left_ibc_mv_n;
    /* 2D refmvs block array for dav1d-compatible spatial MV prediction. */
    stbv_refmvs_cell *refmvs_r;
    unsigned int refmvs_stride;
    unsigned int refmvs_h4;
    unsigned int refmvs_w4;
} stbv_av1_leaf_state;

static void stbv_av1_leaf_state_init(stbv_av1_leaf_state *s,
                                     const stbv_av1_leaf_state_arrays *a)
{
    int pl;
    if (!s) return;
    if (!a) {
        memset(s, 0, sizeof(*s));
        s->cdef_sb_x = s->cdef_sb_y = -1;
        s->cdef_idx[0] = s->cdef_idx[1] = -1;
        s->cdef_idx[2] = s->cdef_idx[3] = -1;
        return;
    }
    stb_av1_intra_state_init(&s->intra, a->above_mode, a->above_mode_n,
                             a->left_mode, a->left_mode_n);
    stbv_av1_tx_state_init(&s->tx, a->above_tx, a->above_tx_n,
                           a->left_tx, a->left_tx_n,
                           a->above_tx_intra, a->left_tx_intra);
    stbv_av1_res_state_init(&s->res, a->above_res, a->above_res_n,
                            a->left_res, a->left_res_n);
    for (pl = 0; pl < 2; pl++)
        stbv_av1_res_state_init(&s->cres[pl], a->above_cre[pl],
                                a->above_cre_n[pl], a->left_cre[pl],
                                a->left_cre_n[pl]);
    s->res.above_mark_n = a->above_res_mark_n;
    s->res.left_mark_n = a->left_res_mark_n;
    for (pl = 0; pl < 2; pl++) {
        s->cres[pl].above_mark_n = a->above_cre_mark_n[pl];
        s->cres[pl].left_mark_n = a->left_cre_mark_n[pl];
    }
    s->above_skip = a->above_skip;
    s->left_skip = a->left_skip;
    s->above_skip_n = a->above_skip_n;
    s->left_skip_n = a->left_skip_n;
    s->above_pal_sz = a->above_pal_sz;
    s->left_pal_sz = a->left_pal_sz;
    s->above_pal_sz_n = a->above_pal_sz_n;
    s->left_pal_sz_n = a->left_pal_sz_n;
    s->above_pal_uv = a->above_pal_uv;
    s->left_pal_uv = a->left_pal_uv;
    s->above_pal_uv_n = a->above_pal_uv_n;
    s->left_pal_uv_n = a->left_pal_uv_n;
    s->above_pal[0] = a->above_pal[0];
    s->above_pal[1] = a->above_pal[1];
    s->left_pal[0] = a->left_pal[0];
    s->left_pal[1] = a->left_pal[1];
    s->above_pal_n = a->above_pal_n;
    s->left_pal_n = a->left_pal_n;
    s->above_seg_id = a->above_seg_id;
    s->left_seg_id = a->left_seg_id;
    s->above_seg_id_n = a->above_seg_id_n;
    s->left_seg_id_n = a->left_seg_id_n;
    s->above_ibc_mv_y = a->above_ibc_mv_y;
    s->above_ibc_mv_x = a->above_ibc_mv_x;
    s->above_ibc_valid = a->above_ibc_valid;
    s->above_ibc_mv_n = a->above_ibc_mv_n;
    s->left_ibc_mv_y = a->left_ibc_mv_y;
    s->left_ibc_mv_x = a->left_ibc_mv_x;
    s->left_ibc_valid = a->left_ibc_valid;
    s->left_ibc_mv_n = a->left_ibc_mv_n;
    s->refmvs_r = (stbv_refmvs_cell *)a->refmvs_r;
    s->refmvs_stride = a->refmvs_stride;
    s->refmvs_h4 = a->refmvs_h4;
    s->refmvs_w4 = a->refmvs_w4;
    if (a->refmvs_r) memset(a->refmvs_r, 0,
        (size_t)a->refmvs_h4 * a->refmvs_stride * sizeof(stbv_refmvs_cell));
    if (a->above_pal_sz) memset(a->above_pal_sz, 0, a->above_pal_sz_n);
    if (a->left_pal_sz) memset(a->left_pal_sz, 0, a->left_pal_sz_n);
    if (a->above_pal_uv) memset(a->above_pal_uv, 0, a->above_pal_uv_n);
    if (a->left_pal_uv) memset(a->left_pal_uv, 0, a->left_pal_uv_n);
    if (a->above_pal[0])
        memset(a->above_pal[0], 0, a->above_pal_n * 8U * sizeof(stbv_u16));
    if (a->above_pal[1])
        memset(a->above_pal[1], 0, a->above_pal_n * 8U * sizeof(stbv_u16));
    if (a->left_pal[0])
        memset(a->left_pal[0], 0, a->left_pal_n * 8U * sizeof(stbv_u16));
    if (a->left_pal[1])
        memset(a->left_pal[1], 0, a->left_pal_n * 8U * sizeof(stbv_u16));
    s->cdef_sb_x = s->cdef_sb_y = -1;
    s->cdef_idx[0] = s->cdef_idx[1] = -1;
    s->cdef_idx[2] = s->cdef_idx[3] = -1;
    if (a->above_skip) memset(a->above_skip, 0, a->above_skip_n);
    if (a->left_skip) memset(a->left_skip, 0, a->left_skip_n);
    if (a->above_seg_id) memset(a->above_seg_id, 0, a->above_seg_id_n);
    if (a->above_ibc_valid) memset(a->above_ibc_valid, 0, a->above_ibc_mv_n);
    if (a->left_ibc_valid) memset(a->left_ibc_valid, 0, a->left_ibc_mv_n);
}

/* dav1d reset_context() resets only the LEFT contexts at the start of each
 * superblock row; the above contexts persist across rows (they are reset
 * once per frame). */
static void stbv_av1_leaf_state_reset_row(stbv_av1_leaf_state *s)
{
    int pl;
    if (!s) return;
    stbv_av1_tx_state_reset_row(&s->tx);
    if (s->res.left) memset(s->res.left, 0x40, s->res.left_n);
    for (pl = 0; pl < 2; pl++)
        if (s->cres[pl].left) memset(s->cres[pl].left, 0x40, s->cres[pl].left_n);
    if (s->left_skip) memset(s->left_skip, 0, s->left_skip_n);
    if (s->left_seg_id) memset(s->left_seg_id, 0, s->left_seg_id_n);
    if (s->left_ibc_valid) memset(s->left_ibc_valid, 0, s->left_ibc_mv_n);
    if (s->intra.left_mode)
        memset(s->intra.left_mode, STBV_AV1_INTRA_DC,
               (size_t)s->intra.left_count);
    /* dav1d reset_context() also clears uvmode to DC_PRED each superblock
     * row; leaving stale SMOOTH modes here made sm_uv_flag fire on rows
     * where dav1d saw a clean left edge (diffuse chroma fog). */
    if (s->intra.left_uvmode)
        memset(s->intra.left_uvmode, STBV_AV1_INTRA_DC,
               (size_t)s->intra.left_uv_count);
    if (s->left_pal_sz) memset(s->left_pal_sz, 0, s->left_pal_sz_n);
    if (s->left_pal_uv) memset(s->left_pal_uv, 0, s->left_pal_uv_n);
    if (s->left_pal[0])
        memset(s->left_pal[0], 0, s->left_pal_n * 8U * sizeof(stbv_u16));
    if (s->left_pal[1])
        memset(s->left_pal[1], 0, s->left_pal_n * 8U * sizeof(stbv_u16));
}

typedef struct stbv_av1_leaf_tx_result {
    int x4, y4, tx;
    int skipped;
    int txtp;
    int eob;
    int skip_ctx;
} stbv_av1_leaf_tx_result;

typedef struct stbv_av1_leaf_decode_ctx {
    struct stb_av1_msac *msac;
    stbv_av1_cdf *cdf;
    stbv_av1_leaf_state *state;
    const struct stb_av1_framehdr *frame;
    const struct stb_av1_intra_block *intra;
    int bs;
    int bw4, bh4;
    /* Unclipped luma block dims (dav1d uses full b_dim for skip ctx). */
    int bw4_unc, bh4_unc;
    int cbw4, cbh4;
    /* Unclipped chroma block dims (dav1d uses full b_dim for skip ctx). */
    int cbw4_unc, cbh4_unc;
    int ss_hor, ss_ver;  /* chroma subsampling for txtp_map lookup */
    int lossless;
    int qidx;
    int y_mode_nofilt;
    int y_mode_txtp;
    int block_skip;
    int reduced_txtp_set;
    int hbd;
    int is_intra;  /* 1 = intra block, 0 = IBC block */
    int luma_txtp; /* stored luma txtp for inter/IBC chroma derivation */
    /* Per-position luma txtp map (SB-local 32x32), matching dav1d's
     * t->scratch.txtp_map.  Populated during luma coefficient decode,
     * read during chroma coefficient decode for inter/IBC blocks. */
    stbv_u8 luma_txtp_map[32 * 32];
    int ibc_mv_y;  /* decoded IBC MV, 1/8-pel luma units */
    int ibc_mv_x;
    const stbv_av1_leaf_recon *recon;
} stbv_av1_leaf_decode_ctx;

/* Per-transform coefficient syntax for one plane (dav1d decode_coefs +
 * read_coef_blocks): per-transform skip, then transform type (gated), then
 * coefficients.  x4/y4 are plane-local 4x4 coordinates and bw4/bh4 the
 * plane-local block dims in 4x4 units.  When out is non-NULL the first
 * (luma) transform's results are recorded there. */
static int stbv_av1_leaf_tx_plane(struct stb_av1_msac *msac,
                                  stbv_av1_cdf *cdf,
                                  stbv_av1_leaf_decode_ctx *c,
                                  int x4, int y4, int tx, int chroma,
                                  stbv_av1_res_state *rs,
                                  int bw4, int bh4,
                                  stbv_av1_leaf_tx_result *out)
{
    int txw4 = stbv_av1_tx_dims[tx].w;
    int txh4 = stbv_av1_tx_dims[tx].h;
    int sctx, txtp, max;
    unsigned skip;
    int is_chroma = chroma != 0; /* dav1d: chroma = !!plane */
    sctx = stbv_av1_get_skip_ctx(rs, x4, y4,
                                 is_chroma ? c->cbw4_unc : c->bw4_unc,
                                 is_chroma ? c->cbh4_unc : c->bh4_unc,
                                 txw4, txh4, is_chroma);
    {
        stbv_u16 *_csk = cdf->coef + stbv_av1_tx_dims[tx].ctx * 26 + sctx * 2;
        skip = stb_av1_msac_bool_adapt(msac, _csk);
    }
    if (!skip) {
        max = stbv_av1_tx_dims[tx].max;
        if (c->lossless)
            txtp = STBV_AV1_TX_WHT_WHT;
        else if (max + c->is_intra >= STBV_AV1_TX_64X64) /* max + intra >= TX_64X64 */
            txtp = STBV_AV1_TX_DCT_DCT;
        else if (is_chroma) {
            if (c->is_intra)
                txtp = stbv_av1_txtp_from_uvmode[c->intra ? c->intra->uv_mode : 0];
            else {
                /* dav1d read_coef_blocks: txtp = t->scratch.txtp_map[by4*32+bx4].
                 * Look up the luma txtp at the chroma position from the
                 * per-position txtp map, matching dav1d's read_coef_tree. */
                int lumax = x4 << c->ss_hor;
                int lumay = y4 << c->ss_ver;
                int map_txtp = c->luma_txtp_map[(lumay & 31) * 32 + (lumax & 31)];
                txtp = stbv_av1_get_uv_inter_txtp(
                    stbv_av1_tx_dims[tx].min, stbv_av1_tx_dims[tx].max,
                    map_txtp);
            }
        } else if (!c->qidx)
            txtp = STBV_AV1_TX_DCT_DCT;
        else if (c->is_intra)
            txtp = stbv_av1_decode_intra_txtp(msac, cdf,
                stbv_av1_tx_dims[tx].min, c->y_mode_txtp,
                c->reduced_txtp_set);
        else
            txtp = stbv_av1_decode_inter_txtp(msac, cdf,
                stbv_av1_tx_dims[tx].min, stbv_av1_tx_dims[tx].max,
                c->reduced_txtp_set);
    } else {
        /* dav1d: *txtp = lossless * WHT_WHT */
        txtp = c->lossless ? STBV_AV1_TX_WHT_WHT : STBV_AV1_TX_DCT_DCT;
    }

    /* Store luma txtp for inter/IBC chroma derivation */
    if (!is_chroma) {
        c->luma_txtp = txtp;
        /* Store in per-position txtp map (dav1d read_coef_tree: txtp_map).
         * SB-local coordinates: (x4 & 31, y4 & 31).
         * Fill all 4x4 positions covered by this TX leaf. */
        {
            int sbx = x4 & 31;
            int sby = y4 & 31;
            int dx, dy;
            for (dy = 0; dy < txh4 && (sby + dy) < 32; dy++) {
                for (dx = 0; dx < txw4 && (sbx + dx) < 32; dx++) {
                    c->luma_txtp_map[(sby + dy) * 32 + (sbx + dx)] = (stbv_u8)txtp;
                }
            }
        }
    }

    if (out) {
        out->x4 = x4;
        out->y4 = y4;
        out->tx = tx;
        out->skipped = (int)skip;
        out->txtp = txtp;
        out->eob = 0;
        out->skip_ctx = sctx;
    }

    if (skip) {
        /* A skipped transform has the fixed residual context 0x40. */
        stbv_av1_res_mark_unc(rs, x4, y4, txw4, txh4, (stbv_u8)0x40);
        if (c->recon && c->recon->cf) {
            /* coefficient count is (4w)*(4h): dims are in 4x4 units */
            int n = stbv_av1_tx_dims[tx].w * stbv_av1_tx_dims[tx].h * 16;
            int i;
            for (i = 0; i < n; i++) c->recon->cf[i] = 0;
            if (is_chroma) {
                if (c->recon->chroma_txb)
                    c->recon->chroma_txb(c->recon->ud, chroma - 1, x4, y4,
                                         tx, txtp, 0, c->recon->cf);
            } else {
                if (c->recon->luma_txb)
                    c->recon->luma_txb(c->recon->ud, x4, y4,
                                       tx, txtp, 0, c->recon->cf);
        }
    }
    return 0;
}

    {
        stbv_i32 cf[64 * 64];
        int txclass = stbv_av1_tx_class(txtp);
        int eob;
        stbv_u8 res_ctx;
        int dc_sign_ctx = 0;
        int s = 0;
        int i;

        /* dav1d get_dc_sign_ctx: sum res_ctx >> 6 over the transform width
         * for the above row and the transform HEIGHT for the left column,
         * then subtract w4 and h4 and map to 0..2 via (s != 0) + (s > 0). */
        for (i = 0; i < txw4; i++) {
            if ((unsigned int)(x4 + i) < rs->above_n)
                s += rs->above[x4 + i] >> 6;
        }
        for (i = 0; i < txh4; i++) {
            if ((unsigned int)(y4 + i) < rs->left_n)
                s += rs->left[y4 + i] >> 6;
        }
        s -= txw4;
        s -= txh4;
        dc_sign_ctx = (s != 0) + (s > 0);

        /* This first integration pass validates coefficient syntax and MSAC
           consumption.  Quantization/reconstruction is still supplied by
           the caller in the block layer. */
        {
            /* Real dequantization: dav1d_dq_tbl[hbd][qidx][{dc,ac}] with the
             * per-plane qidx deltas; dq_shift = imax(0, t_dim->ctx - 2). */
            const int hbd_i = c->hbd;
            int base_q = c->qidx ? c->qidx : (c->frame ? (int)c->frame->quant.yac : 0);
            int qdc, qac;
            int dq_dc, dq_ac, dq_shift;
            if (base_q < 0) base_q = 0;
            if (base_q > 255) base_q = 255;
            if (is_chroma) {
                int udc = base_q + (c->frame ? c->frame->quant.udc_delta : 0);
                int uac = base_q + (c->frame ? c->frame->quant.uac_delta : 0);
                if (chroma == 2) { /* V plane */
                    udc += c->frame ? c->frame->quant.vdc_delta - c->frame->quant.udc_delta : 0;
                    uac += c->frame ? c->frame->quant.vac_delta - c->frame->quant.uac_delta : 0;
                }
                udc = udc < 0 ? 0 : udc > 255 ? 255 : udc;
                uac = uac < 0 ? 0 : uac > 255 ? 255 : uac;
                qdc = udc; qac = uac;
            } else {
                int ydc = base_q + (c->frame ? c->frame->quant.ydc_delta : 0);
                ydc = ydc < 0 ? 0 : ydc > 255 ? 255 : ydc;
                qdc = ydc; qac = base_q;
            }
            dq_dc = stbv_av1_dq_tbl[hbd_i][qdc][0];
            dq_ac = stbv_av1_dq_tbl[hbd_i][qac][1];
            dq_shift = stbv_av1_tx_dims[tx].ctx - 2;
            if (dq_shift < 0) dq_shift = 0;

            eob = stbv_av1_decode_coeffs_square(msac, cdf, tx, is_chroma,
                                    txclass,
                                    dq_dc, dq_ac, dq_shift,
                                    sctx, dc_sign_ctx, 8 + c->hbd * 2, cf,
                                    &res_ctx);
        }
        if (eob < 0)
            return -2;
        if (out)
            out->eob = eob;
        if (c->recon && c->recon->cf) {
            /* coefficient count is (4w)*(4h): dims are in 4x4 units */
            int n = stbv_av1_tx_dims[tx].w * stbv_av1_tx_dims[tx].h * 16;
            int i;
            for (i = 0; i < n; i++) c->recon->cf[i] = cf[i];
            if (is_chroma) {
                if (c->recon->chroma_txb) c->recon->chroma_txb(c->recon->ud, chroma-1, x4, y4, tx, txtp, eob, c->recon->cf);
            } else {
                if (c->recon->luma_txb) c->recon->luma_txb(c->recon->ud, x4, y4, tx, txtp, eob, c->recon->cf);
            }
        }
    stbv_av1_res_mark(rs, x4, y4, txw4, txh4, res_ctx);
    }
    return 0;
}

static int stbv_av1_ulog2(unsigned int v)
{
    int n = 0;
    while (v >>= 1) n++;
    return n;
}

/* Palette size + colors for one plane (dav1d dav1d_read_pal_plane).
 * The cache is the merge of the above/left neighbor palettes; entries are
 * reused via equi-probability flags, the rest is delta coded. */
static int stbv_av1_palette_read_plane(struct stb_av1_msac *msac,
                                       stbv_av1_cdf *cdf,
                                       stbv_av1_leaf_state *state,
                                       int pl, int sz_ctx, int bx4, int by4,
                                       int bpc, stbv_u16 *pal_out,
                                       int *pal_sz_out)
{
    int pal_sz, i, l_cache, a_cache, n_cache = 0, n_used = 0, prev;
    int bits, max;
    stbv_u16 *l, *a;
    stbv_u16 *cache = state->cache;
    stbv_u16 *used = state->used_cache;

    pal_sz = (int)stb_av1_msac_symbol(msac,
                                      cdf->pal_sz + (pl * 7 + sz_ctx) * 8,
                                      6) + 2;
    if (pal_sz > 8) return -1;
    l_cache = pl ? (state->left_pal_uv &&
                    (unsigned)by4 < state->left_pal_uv_n ?
                    state->left_pal_uv[by4] : 0)
                 : (state->left_pal_sz &&
                    (unsigned)by4 < state->left_pal_sz_n ?
                    state->left_pal_sz[by4] : 0);
    a_cache = (by4 & 15) ? (pl ? (state->above_pal_uv &&
                                  (unsigned)bx4 < state->above_pal_uv_n ?
                                  state->above_pal_uv[bx4] : 0)
                               : (state->above_pal_sz &&
                                  (unsigned)bx4 < state->above_pal_sz_n ?
                                  state->above_pal_sz[bx4] : 0))
                         : 0;
    l = (state->left_pal[pl] && (unsigned)by4 < state->left_pal_n) ?
        state->left_pal[pl] + by4 * 8 : NULL;
    a = (state->above_pal[pl] && (unsigned)bx4 < state->above_pal_n) ?
        state->above_pal[pl] + bx4 * 8 : NULL;

    while (l_cache && a_cache) {
        if (*l < *a) {
            if (!n_cache || cache[n_cache - 1] != *l)
                cache[n_cache++] = *l;
            l++;
            l_cache--;
        } else {
            if (*a == *l) {
                l++;
                l_cache--;
            }
            if (!n_cache || cache[n_cache - 1] != *a)
                cache[n_cache++] = *a;
            a++;
            a_cache--;
        }
    }
    if (l_cache) {
        do {
            if (!n_cache || cache[n_cache - 1] != *l)
                cache[n_cache++] = *l;
            l++;
        } while (--l_cache > 0);
    } else if (a_cache) {
        do {
            if (!n_cache || cache[n_cache - 1] != *a)
                cache[n_cache++] = *a;
            a++;
        } while (--a_cache > 0);
    }

    i = 0;
    {
        int n;
        for (n = 0; n < n_cache && i < pal_sz; n++)
            if (stb_av1_msac_bool_equi(msac)) {
                used[i++] = cache[n];
            }
    }
    n_used = i;

    if (i < pal_sz) {
        int n, m;
        prev = (int)stb_av1_msac_bools(msac, (unsigned)bpc);
        pal_out[i++] = (stbv_u16)prev;
        if (i < pal_sz) {
            bits = bpc - 3 + (int)stb_av1_msac_bools(msac, 2);
            max = (1 << bpc) - 1;
            do {
                int delta = (int)stb_av1_msac_bools(msac, (unsigned)bits);
                prev += delta + (pl ? 0 : 1);
                if (prev > max) prev = max;
                pal_out[i++] = (stbv_u16)prev;
                if (prev + (pl ? 0 : 1) >= max) {
                    for (; i < pal_sz; i++)
                        pal_out[i] = (stbv_u16)max;
                    break;
                }
                bits = bits < 1 + stbv_av1_ulog2((unsigned)(max - prev -
                                                  (pl ? 0 : 1))) ?
                       bits : 1 + stbv_av1_ulog2((unsigned)(max - prev -
                                                  (pl ? 0 : 1)));
            } while (i < pal_sz);
        }
        n = 0;
        m = n_used;
        for (i = 0; i < pal_sz; i++) {
            if (n < n_used && (m >= pal_sz || used[n] <= pal_out[m]))
                pal_out[i] = used[n++];
            else
                pal_out[i] = pal_out[m++];
        }
    } else {
        for (i = 0; i < n_used; i++)
            pal_out[i] = used[i];
    }

    if (pal_sz_out) *pal_sz_out = pal_sz;
    return 0;
}

/* V plane of the UV palette (dav1d read_pal_uv's V pal coding). */
static void stbv_av1_palette_read_uv_v(struct stb_av1_msac *msac, int bpc,
                                       int pal_sz, stbv_u16 *pal_v)
{
    int i, bits, prev, delta, max;
    if (stb_av1_msac_bool_equi(msac)) {
        bits = bpc - 4 + (int)stb_av1_msac_bools(msac, 2);
        prev = (int)stb_av1_msac_bools(msac, (unsigned)bpc);
        pal_v[0] = (stbv_u16)prev;
        max = (1 << bpc) - 1;
        for (i = 1; i < pal_sz; i++) {
            delta = (int)stb_av1_msac_bools(msac, (unsigned)bits);
            if (delta && stb_av1_msac_bool_equi(msac))
                delta = -delta;
            prev = (prev + delta) & max;
            pal_v[i] = (stbv_u16)prev;
        }
    } else {
        for (i = 0; i < pal_sz; i++)
            pal_v[i] = stb_av1_msac_bools(msac, (unsigned)bpc);
    }
}

/* Per-cell palette order/context for one wave-front diagonal (dav1d
 * order_palette). */
static void stbv_av1_palette_order(const stbv_u8 *pal_idx, int stride,
                                   int i, int first, int last,
                                   stbv_u8 (*order)[8], stbv_u8 *ctx)
{
    int have_top = i > first;
    int j, n;

    pal_idx += first + (i - first) * stride;
    for (j = first, n = 0; j >= last; have_top = 1, j--, n++,
         pal_idx += stride - 1) {
        int have_left = j > 0;
        unsigned mask = 0;
        int o_idx = 0;
#define STBV_PAL_ADD(v_in) do { \
            int v = (v_in); \
            order[n][o_idx++] = (stbv_u8)v; \
            mask |= 1U << v; \
        } while (0)

        if (!have_left) {
            ctx[n] = 0;
            STBV_PAL_ADD(pal_idx[-stride]);
        } else if (!have_top) {
            ctx[n] = 0;
            STBV_PAL_ADD(pal_idx[-1]);
        } else {
            int l = pal_idx[-1], t = pal_idx[-stride];
            int tl = pal_idx[-(stride + 1)];
            int same_t_l = t == l;
            int same_t_tl = t == tl;
            int same_l_tl = l == tl;
            int same_all = same_t_l & same_t_tl & same_l_tl;

            if (same_all) {
                ctx[n] = 4;
                STBV_PAL_ADD(t);
            } else if (same_t_l) {
                ctx[n] = 3;
                STBV_PAL_ADD(t);
                STBV_PAL_ADD(tl);
            } else if (same_t_tl | same_l_tl) {
                ctx[n] = 2;
                STBV_PAL_ADD(tl);
                STBV_PAL_ADD(same_t_tl ? l : t);
            } else {
                ctx[n] = 1;
                STBV_PAL_ADD(t < l ? t : l);
                STBV_PAL_ADD(t < l ? l : t);
                STBV_PAL_ADD(tl);
            }
        }
        {
            unsigned m;
            int bit;
            for (m = 1, bit = 0; m < 0x100; m <<= 1, bit++)
                if (!(mask & m))
                    order[n][o_idx++] = (stbv_u8)bit;
        }
#undef STBV_PAL_ADD
    }
}

/* Palette index map (dav1d read_pal_indices). */
static int stbv_av1_palette_indices(struct stb_av1_msac *msac,
                                    stbv_av1_cdf *cdf,
                                    int pl, int pal_sz,
                                    int bw4, int bh4,
                                    stbv_u8 *pal_tmp,
                                    stbv_u8 (*order)[8], stbv_u8 *ctx)
{
    int stride = bw4 * 4;
    int wpx = bw4 * 4, hpx = bh4 * 4;
    int i, j, m, first, last;
    stbv_u16 *color_map_cdf;

    pal_tmp[0] = stb_av1_msac_uniform(msac, (unsigned)pal_sz);
    color_map_cdf = cdf->color_map + (pl * 7 + (pal_sz - 2)) * 40;
    for (i = 1; i < wpx + hpx - 1; i++) {
        first = i < wpx - 1 ? i : wpx - 1;
        last = i - hpx + 1 > 0 ? i - hpx + 1 : 0;
        stbv_av1_palette_order(pal_tmp, stride, i, first, last, order, ctx);
        for (j = first, m = 0; j >= last; j--, m++) {
            int color_idx = (int)stb_av1_msac_symbol(
                msac, color_map_cdf + ctx[m] * 8, (size_t)(pal_sz - 1));
            pal_tmp[(i - j) * stride + j] = order[m][color_idx];
        }
    }
    return 0;
}

/* ---- MV residual decode for IBC (dav1d decode.c:76-117) ---- */

/* Decode the diff for one MV component. For IBC, mv_prec=-1 so fp and hp
 * are never decoded (only sign + class + class bits). */
static int stbv_av1_read_mv_component_diff(struct stb_av1_msac *msac,
                                            stbv_u16 *mv_comp_sign,
                                            stbv_u16 *mv_comp_classes,
                                            stbv_u16 *mv_comp_class0,
                                            stbv_u16 mv_comp_classN[10][2],
                                            int mv_prec)
{
    int sign, cl, up, fp = 3, hp = 1;
    sign = (int)stb_av1_msac_bool_adapt(msac, mv_comp_sign);
    cl = (int)stb_av1_msac_symbol(msac, mv_comp_classes, 10);
    if (!cl) {
        up = (int)stb_av1_msac_bool_adapt(msac, mv_comp_class0);
        if (mv_prec >= 0) {
            fp = (int)stb_av1_msac_symbol(msac,
                mv_comp_class0 + up * 4, 3);
            if (mv_prec > 0)
                hp = (int)stb_av1_msac_bool_adapt(msac,
                    mv_comp_class0 + up * 4 + 2);
        }
    } else {
        up = 1 << cl;
        { int n; for (n = 0; n < cl; n++)
            up |= (int)stb_av1_msac_bool_adapt(msac, mv_comp_classN[n]) << n;
        }
        if (mv_prec >= 0) {
            fp = (int)stb_av1_msac_symbol(msac, mv_comp_classN[0] + 10, 3);
            if (mv_prec > 0)
                hp = (int)stb_av1_msac_bool_adapt(msac, mv_comp_classN[0] + 12);
        }
    }
    { int diff = ((up << 3) | (fp << 1) | hp) + 1;
      return sign ? -diff : diff;
    }
}

/* Decode MV joint + component residuals. mv_prec=-1 for IBC. */
static void stbv_av1_read_mv_residual(struct stb_av1_msac *msac,
                                       stbv_av1_cdf *cdf,
                                       int *mv_y, int *mv_x,
                                       int mv_prec,
                                       int bx4, int by4)
{
    int joint;
    joint = (int)stb_av1_msac_symbol(msac, cdf->mv_joint, 3);
    if (joint & 2) /* MV_JOINT_V */
        *mv_y += stbv_av1_read_mv_component_diff(msac,
            cdf->mv_sign, cdf->mv_classes, cdf->mv_class0,
            cdf->mv_classN, mv_prec);
    if (joint & 1) /* MV_JOINT_H */
        *mv_x += stbv_av1_read_mv_component_diff(msac,
            cdf->mv_sign_x, cdf->mv_classes_x, cdf->mv_class0_x,
            cdf->mv_classN_x, mv_prec);
}

/* ---- dav1d-compatible refmvs spatial candidate search for IBC ----
 * This implements a simplified version of dav1d's refmvs_find() using a
 * 2D refmvs_cell array.  For IBC, ref={0,-1} (intra, single reference),
 * so we only need spatial candidate search (no temporal MVs). */

/* Add a spatial candidate to the mvstack.  Returns 1 if the candidate was
 * added (or merged with an existing duplicate). */
static int stbv_refmvs_add_candidate(
    int mvstack_mv_y[8], int mvstack_mv_x[8], int mvstack_w[8], int *cnt,
    int mv_y, int mv_x, int weight)
{
    int n;
    /* Check for duplicate MV and merge weights. */
    for (n = 0; n < *cnt; n++) {
        if (mvstack_mv_y[n] == mv_y && mvstack_mv_x[n] == mv_x) {
            mvstack_w[n] += weight;
            return 1;
        }
    }
    if (*cnt < 8) {
        mvstack_mv_y[*cnt] = mv_y;
        mvstack_mv_x[*cnt] = mv_x;
        mvstack_w[*cnt] = weight;
        *cnt = *cnt + 1;
        return 1;
    }
    return 0;
}

/* Scan a single row of blocks (matching dav1d scan_row).
 * b points to the first block in the row at the starting column.
 * bw4 = current block width in 4x4 units.
 * max_rows = used only for weight computation (2*max_rows cap).
 * step = minimum column advance (1 for primary, 2 for secondary).
 * Returns weight>>1 for wide-candidate case, 1 for loop case. */
static int stbv_refmvs_scan_row(
    const stbv_refmvs_cell *r, unsigned int stride, unsigned int rw4,
    int mvstack_mv_y[8], int mvstack_mv_x[8], int mvstack_w[8], int *cnt,
    const stbv_refmvs_cell *b, int bw4, int w4, int max_rows, int step,
    signed char filter_ref)
{
    const stbv_refmvs_cell *cand_b = b;
    int first_cand_bw4 = stbv_av1_block_dimensions[cand_b->bs][0];
    int first_cand_bh4 = stbv_av1_block_dimensions[cand_b->bs][1];
    int len, x, cand_bw4;
    { int _min = bw4 < first_cand_bw4 ? bw4 : first_cand_bw4;
      len = step > _min ? step : _min; }

    (void)stride; (void)rw4;

    if (bw4 <= first_cand_bw4) {
        int weight = bw4 == 1 ? 2 :
                     (first_cand_bh4 > 2 * max_rows ? 2 * max_rows :
                      (first_cand_bh4 < 2 ? 2 : first_cand_bh4));
        if (cand_b->valid && (filter_ref < 0 || cand_b->ref == filter_ref)) {
            stbv_refmvs_add_candidate(mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                       cnt, cand_b->mv_y, cand_b->mv_x,
                                       len * weight);
        }
        return weight >> 1;
    }

    for (x = 0;;) {
        if (cand_b->valid && (filter_ref < 0 || cand_b->ref == filter_ref)) {
            stbv_refmvs_add_candidate(mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                       cnt, cand_b->mv_y, cand_b->mv_x,
                                       len * 2);
        }
        x += len;
        if (x >= w4) return 1;
        cand_b = &b[x];
        cand_bw4 = stbv_av1_block_dimensions[cand_b->bs][0];
        len = step > cand_bw4 ? step : cand_bw4;
    }
}

/* Scan a single column vertically (matching dav1d scan_col).
 * bx4_col = column index to scan.
 * start_y = starting row (by4 for primary, by4|1 for secondary).
 * h4 = scan depth (imin(bh4, 16)).
 * Returns weight>>1 for single-block case, 1 for loop case. */
static int stbv_refmvs_scan_col1(
    const stbv_refmvs_cell *r, unsigned int stride,
    int mvstack_mv_y[8], int mvstack_mv_x[8], int mvstack_w[8],
    int *cnt, int *have_col_mvs,
    int bx4_col, int start_y, int bh4, int h4, int max_cols, int step,
    signed char filter_ref)
{
    const stbv_refmvs_cell *cand = &r[start_y * (int)stride + bx4_col];
    int cand_bh4 = stbv_av1_block_dimensions[cand->bs][1];
    int len, y;
    { int _min = bh4 < cand_bh4 ? bh4 : cand_bh4;
      len = step > _min ? step : _min; }

    if (bh4 <= cand_bh4) {
        int cand_bw4 = stbv_av1_block_dimensions[cand->bs][0];
        int weight = bh4 == 1 ? 2 :
                     (cand_bw4 > 2 * max_cols ? 2 * max_cols :
                      (cand_bw4 < 2 ? 2 : cand_bw4));
        if (cand->valid && (filter_ref < 0 || cand->ref == filter_ref)) {
            stbv_refmvs_add_candidate(mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                       cnt, cand->mv_y, cand->mv_x,
                                       len * weight);
            *have_col_mvs = 1;
        }
        return weight >> 1;
    }

    for (y = 0;;) {
        if (cand->valid && (filter_ref < 0 || cand->ref == filter_ref)) {
            stbv_refmvs_add_candidate(mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                       cnt, cand->mv_y, cand->mv_x,
                                       len * 2);
            *have_col_mvs = 1;
        }
        y += len;
        if (y >= h4) break;
        cand = &r[(start_y + y) * (int)stride + bx4_col];
        cand_bh4 = stbv_av1_block_dimensions[cand->bs][1];
        len = step > cand_bh4 ? step : cand_bh4;
    }
    return 1;
}

/* Splat MV into the 2D refmvs array for all 4x4 positions covered by the
 * current block.  Matches dav1d splat_mv_c. */
static void stbv_refmvs_splat(stbv_refmvs_cell *r, unsigned int stride,
                               int bx4, int by4, int bw4, int bh4, int bs,
                               int mv_y, int mv_x, int valid, signed char ref)
{
    int y;
    for (y = 0; y < bh4; y++) {
        stbv_refmvs_cell *row = &r[(by4 + y) * (int)stride];
        int x;
        for (x = 0; x < bw4; x++) {
            row[bx4 + x].mv_y = mv_y;
            row[bx4 + x].mv_x = mv_x;
            row[bx4 + x].bs = (stbv_u8)bs;
            row[bx4 + x].valid = (stbv_u8)valid;
            row[bx4 + x].ref = ref;
        }
    }
}

/* Splat "intra but not IBC" (clears valid flag) into the 2D refmvs array.
 * Matches dav1d storing an intra block with INVALID_MV in the r array. */
static void stbv_refmvs_splat_intra(stbv_refmvs_cell *r, unsigned int stride,
                                     int bx4, int by4, int bw4, int bh4, int bs)
{
    stbv_refmvs_splat(r, stride, bx4, by4, bw4, bh4, bs, 0, 0, 0, -1);
}

/* Find IBC MV prediction using dav1d-compatible refmvs_find with spatial
 * candidate search (ref={0,-1}, IBC/intra only).
 *
 * Implements the key parts of dav1d refmvs_find():
 *   1. scan_row above (with block-size-aware weight computation)
 *   2. scan_col left
 *   3. above-right (spatial candidate)
 *   4. above-left (spatial candidate)
 *   5. Sort by weight, select highest
 *   6. +640 "nearest" boost for row/col candidates
 *
 * Returns the best MV prediction.  If no candidates found, returns
 * dav1d's default MV. */
static void stbv_av1_find_ibc_mv_pred(const stbv_av1_leaf_state *s,
                                       int bx4, int by4, int bw4, int bh4,
                                       int frame_top4, int sb128,
                                       int *pred_y, int *pred_x)
{
    int mvstack_mv_y[8], mvstack_mv_x[8], mvstack_w[8];
    int cnt = 0;
    int have_row_mvs = 0, have_col_mvs = 0;
    unsigned n_rows = ~0U, n_cols = ~0U;
    int nearest_cnt;
    int max_rows, max_cols;
    int n, best;
    (void)frame_top4;

    *pred_y = 0;
    *pred_x = 0;

    if (!s->refmvs_r) goto default_mv;

    /* max_rows/max_cols: same formula as dav1d refmvs_find.
     * max_rows = imin((by4 + 1) >> 1, 2 + (bh4 > 1)) */
    max_rows = ((by4 + 1) >> 1);
    { int cap = 2 + (bh4 > 1); if (max_rows > cap) max_rows = cap; }
    max_cols = ((bx4 + 1) >> 1);
    { int cap = 2 + (bw4 > 1); if (max_cols > cap) max_cols = cap; }

    /* 1. Scan above row. IBC ref=0 (current frame).
     * Match dav1d: primary row by4-1, secondary ((by4-3)|1) and ((by4-5)|1).
     * scan_row now scans a single row, matching dav1d. */
    n_rows = ~0U;
    if (by4 > 0) {
        int check_y = by4 - 1;
        int w4 = bw4 < 16 ? bw4 : 16;
        const stbv_refmvs_cell *b_top = &s->refmvs_r[check_y * (int)s->refmvs_stride + bx4];
        n_rows = stbv_refmvs_scan_row(s->refmvs_r, s->refmvs_stride,
                                       s->refmvs_w4,
                                       mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                       &cnt, b_top, bw4, w4, max_rows,
                                       bw4 >= 16 ? 4 : 1, 0);
        have_row_mvs = (n_rows != ~0U) ? 1 : 0;
    }

    /* 2. Scan left column. IBC ref=0 (current frame).
     * Match dav1d: primary column bx4-1, secondary (bx4-3)|1, (bx4-5)|1. */
    n_cols = ~0U;
    if (bx4 > 0) {
        int h4 = bh4 < 16 ? bh4 : 16;
        /* Primary: column bx4-1, start at by4, step = bh4>=16 ? 4 : 1 */
        n_cols = stbv_refmvs_scan_col1(s->refmvs_r, s->refmvs_stride,
                                        mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                        &cnt, &have_col_mvs,
                                        bx4 - 1, by4, bh4, h4,
                                        max_cols, bh4 >= 16 ? 4 : 1, 0);
    }

    /* 3. Above-right: add as spatial candidate with weight=4. */
    if (n_rows != ~0U && bw4 + bx4 < (int)s->refmvs_w4 &&
        (bw4 <= 16 && bh4 <= 16))
    {
        int ar_x = bx4 + bw4;
        int ar_y = by4 - 1;
        if (ar_x >= 0 && (unsigned)ar_x < s->refmvs_w4 && ar_y >= 0) {
            const stbv_refmvs_cell *cand = &s->refmvs_r[ar_y * (int)s->refmvs_stride + ar_x];
            if (cand->valid && cand->ref == 0) {
                stbv_refmvs_add_candidate(mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                          &cnt, cand->mv_y, cand->mv_x, 4);
            }
        }
    }

    nearest_cnt = cnt;
    /* +640 "nearest" boost: matches dav1d refmvs_find line 413-414.
     * All spatial candidates from row/col get this boost. */
    for (n = 0; n < nearest_cnt; n++)
        mvstack_w[n] += 640;

    /* 4. Above-left: add as secondary candidate with weight=4. */
    if ((n_rows | n_cols) != ~0U && bx4 > 0 && by4 > 0) {
        int al_x = bx4 - 1;
        int al_y = by4 - 1;
        if ((unsigned)al_x < s->refmvs_w4 && (unsigned)al_y < s->refmvs_h4) {
            const stbv_refmvs_cell *cand = &s->refmvs_r[al_y * (int)s->refmvs_stride + al_x];
            if (cand->valid && cand->ref == 0) {
                stbv_refmvs_add_candidate(mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                          &cnt, cand->mv_y, cand->mv_x, 4);
            }
        }
    }

    /* 5. Secondary rows/columns (8x8-aligned): match dav1d refmvs_find lines 464-478. */
    { int n2;
    for (n2 = 2; n2 <= 3; n2++) {
        if ((unsigned)n2 > n_rows && (unsigned)n2 <= (unsigned)max_rows) {
            int sec_y = ((by4 - 2 * n2 + 1) | 1);
            int w4 = bw4 < 16 ? bw4 : 16;
            if (sec_y >= 0) {
                const stbv_refmvs_cell *b_sec = &s->refmvs_r[sec_y * (int)s->refmvs_stride + (bx4 | 1)];
                n_rows += stbv_refmvs_scan_row(s->refmvs_r, s->refmvs_stride,
                                                s->refmvs_w4,
                                                mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                                &cnt, b_sec, bw4, w4,
                                                1 + max_rows - n2,
                                                bw4 >= 16 ? 4 : 2, 0);
            }
        }
        if ((unsigned)n2 > n_cols && (unsigned)n2 <= (unsigned)max_cols) {
            int h4 = bh4 < 16 ? bh4 : 16;
            n_cols += stbv_refmvs_scan_col1(s->refmvs_r, s->refmvs_stride,
                                             mvstack_mv_y, mvstack_mv_x, mvstack_w,
                                             &cnt, &have_col_mvs,
                                             (bx4 - n2 * 2 + 1) | 1, by4 | 1,
                                             bh4, h4,
                                             1 + max_cols - n2, bh4 >= 16 ? 4 : 2, 0);
        }
    }
    }

    /* Sort by weight (descending): bubble sort, matches dav1d refmvs_find
     * lines 500-524).  We sort the entire stack since all entries have
     * the nearest boost applied. */
    {
        int len = cnt;
        int did_swap;
        do {
            did_swap = 0;
            for (n = 1; n < len; n++) {
                if (mvstack_w[n - 1] < mvstack_w[n]) {
                    int tmp_y = mvstack_mv_y[n - 1];
                    int tmp_x = mvstack_mv_x[n - 1];
                    int tmp_w = mvstack_w[n - 1];
                    mvstack_mv_y[n - 1] = mvstack_mv_y[n];
                    mvstack_mv_x[n - 1] = mvstack_mv_x[n];
                    mvstack_w[n - 1] = mvstack_w[n];
                    mvstack_mv_y[n] = tmp_y;
                    mvstack_mv_x[n] = tmp_x;
                    mvstack_w[n] = tmp_w;
                    did_swap = 1;
                }
            }
            len--;
        } while (did_swap && len > 1);
    }

    /* Select the highest-weight candidate. */
    if (cnt > 0) {
        *pred_y = mvstack_mv_y[0];
        *pred_x = mvstack_mv_x[0];
        return;
    }

default_mv:
    /* No spatial candidate found: use dav1d default MV. */
    {
        int sb_step = 16 << sb128;
        if (by4 - sb_step < frame_top4) {
            *pred_y = 0;
            *pred_x = -(512 << sb128) - 2048;
        } else {
            *pred_y = -(512 << sb128);
            *pred_x = 0;
        }
    }
}

/* ---- IBC luma TX tree leaf callback ---- */
/* Called by stbv_av1_decode_tx_tree for each luma TX leaf in an IBC block.
 * Decodes coefficients at the leaf's TX size. */
static int stbv_av1_ibc_luma_leaf(int x4, int y4, int tx, void *opaque)
{
    stbv_av1_leaf_decode_ctx *c = (stbv_av1_leaf_decode_ctx *)opaque;
    int r;
    if (!c) return -1;
    r = stbv_av1_leaf_tx_plane(c->msac, c->cdf, c, x4, y4, tx, 0,
                               &c->state->res, c->bw4, c->bh4, NULL);
    return r ? -1 : 0;
}

static int stbv_av1_decode_leaf_syntax(struct stb_av1_msac *msac,
                                       stbv_av1_cdf *cdf,
                                       stbv_av1_leaf_state *state,
                                       const struct stb_av1_seqhdr *seq,
                                       const struct stb_av1_framehdr *frame,
                                       int bs, int bx4, int by4,
                                       stbv_av1_leaf_tx_result *out,
                                       const stbv_av1_leaf_recon *recon)
{
    struct stb_av1_intra_block intra;
    stbv_av1_leaf_decode_ctx c;
    int bw4, bh4, bw4_unc, bh4_unc, max_tx, uv_tx, tx0;
    int layout, ss_hor, ss_ver, sb_step;
    int cfl_allowed, cbw4, cbh4, has_chroma;
    int cbw4_unc, cbh4_unc;
    int lossless, qidx;
    int y_mode_nofilt, i;
    int seg_id = 0, seg_pred = 0;
    unsigned block_skip = 0;
    unsigned int n;
    int intra_flag = 1; /* 1 = intra, 0 = IBC */
    c.recon = recon;
    c.ss_hor = 0;
    c.ss_ver = 0;
    memset(c.luma_txtp_map, 0, sizeof(c.luma_txtp_map));
    if (!msac || !cdf || !state || bs < 0 || bs >= STBV_AV1_N_BS_SIZES)
        return -1;
    bw4 = stbv_av1_block_dimensions[bs][0];
    bh4 = stbv_av1_block_dimensions[bs][1];
    if (!bw4 || !bh4)
        return -2;
    layout = seq ? (int)seq->layout : STB_AV1_LAYOUT_I444;
    ss_hor = layout == STB_AV1_LAYOUT_I420 || layout == STB_AV1_LAYOUT_I422;
    ss_ver = layout == STB_AV1_LAYOUT_I420;
    c.ss_hor = ss_hor;
    c.ss_ver = ss_ver;
    sb_step = (seq && seq->sb128) ? 32 : 16;
    lossless = frame ? (int)frame->segmentation.lossless[0] : 0;
    qidx = state->last_qidx + (frame ? (int)frame->segmentation.d[seg_id].delta_q : 0);
    if (qidx < 0) qidx = 0;
    if (qidx > 255) qidx = 255;
    /* dav1d gates chroma presence on the UNCLIPPED block dims, then clips
     * the coefficient grids to the padded frame area ((w+7)&~7)>>2. */
    has_chroma = layout != STB_AV1_LAYOUT_I400 &&
                 (bw4 > ss_hor || (bx4 & 1)) && (bh4 > ss_ver || (by4 & 1));
    cbw4_unc = (bw4 + ss_hor) >> ss_hor;
    cbh4_unc = (bh4 + ss_ver) >> ss_ver;
    if (frame && frame->width[0] > 0 && frame->height > 0) {
        int fw4 = (((int)frame->width[0] + 7) & ~7) >> 2;
        int fh4 = (((int)frame->height + 7) & ~7) >> 2;
        /* Syntax decisions (skip ctx, tx size, palette/filter-intra gates)
         * must use the block's own dimensions; only coefficient-grid
         * extents may be clipped to the padded frame (dav1d keeps b_dim
         * unclipped and clips during recon). */
        /* Clip coefficient-loop dims to the 8-aligned frame extent
         * (dav1d read_coef_blocks: w4 = imin(bw4, f->bw - t->bx)).
         * bw4_unc/bh4_unc stay unclipped for syntax decisions. */
        bw4_unc = bw4;
        bh4_unc = bh4;
        if (fw4 - bx4 < bw4) bw4 = fw4 - bx4;
        if (fh4 - by4 < bh4) bh4 = fh4 - by4;
        if (bw4 <= 0 || bh4 <= 0)
            return 0;
    } else {
        bw4_unc = bw4;
        bh4_unc = bh4;
    }
    cbw4 = (bw4 + ss_hor) >> ss_hor;
    cbh4 = (bh4 + ss_ver) >> ss_ver;

    /* Segment ID decoding (dav1d decode_b segment_id section). */
    seg_id = 0;
    seg_pred = 0;
    if (frame && frame->segmentation.enabled && frame->segmentation.update_map) {
        int have_top = (state->above_seg_id && (unsigned)bx4 < state->above_seg_id_n);
        int have_left = (state->left_seg_id && (unsigned)by4 < state->left_seg_id_n);
        if (frame->segmentation.preskip) {
            /* preskip: decode segment_id before skip */
            if (!frame->segmentation.temporal) {
                /* Spatial prediction: get predicted seg_id from neighbours */
                int seg_ctx = 0;
                unsigned pred_seg_id = 0;
                if (have_left && have_top) {
                    int l = state->left_seg_id[by4];
                    int a = state->above_seg_id[bx4];
                    int al = (bx4 > 0 && by4 > 0) ? state->above_seg_id[bx4 - 1] : a;
                    if (l == a && al == l) seg_ctx = 2;
                    else if (l == a || al == l || a == al) seg_ctx = 1;
                    else seg_ctx = 0;
                    pred_seg_id = (unsigned)(a == al ? a : l);
                } else {
                    pred_seg_id = have_left ? (unsigned)state->left_seg_id[by4] :
                                  have_top ? (unsigned)state->above_seg_id[bx4] : 0;
                }
                if (block_skip) {
                    seg_id = (int)pred_seg_id;
                } else {
                    unsigned diff = (unsigned)stb_av1_msac_symbol(msac,
                        cdf->seg_id + seg_ctx * 8, 7);
                    int last_active = frame->segmentation.last_active_segid;
                    seg_id = stb_neg_deinterleave((int)diff, (int)pred_seg_id,
                                              last_active + 1);
                    if (seg_id > last_active) seg_id = 0;
                }
                if (seg_id < 0 || seg_id >= 8) seg_id = 0;
            }
        }
        /* Apply per-segment features: skip */
        if (frame->segmentation.d[seg_id].skip)
            block_skip = 1;
        /* Store segment_id in context arrays */
        if (state->above_seg_id && (unsigned)bx4 < state->above_seg_id_n) {
            for (i = 0; i < bw4_unc && (unsigned)(bx4 + i) < state->above_seg_id_n; i++)
                state->above_seg_id[bx4 + i] = (stbv_u8)seg_id;
        }
        if (state->left_seg_id && (unsigned)by4 < state->left_seg_id_n) {
            for (i = 0; i < bh4_unc && (unsigned)(by4 + i) < state->left_seg_id_n; i++)
                state->left_seg_id[by4 + i] = (stbv_u8)seg_id;
        }
    }

    /* Block-level skip, decoded before intra modes (dav1d decode_b).
     * When skip_mode or segment skip is already set, skip=1 without
     * reading from MSAC (dav1d decode.c:888-895). */
    {
        int sctx = 0;
        if (state->above_skip && (unsigned int)bx4 < state->above_skip_n &&
            state->above_skip[bx4])
            sctx += 1;
        if (state->left_skip && (unsigned int)by4 < state->left_skip_n &&
            state->left_skip[by4])
            sctx += 1;
        if (!block_skip) {
            block_skip = stb_av1_msac_bool_adapt(msac, cdf->skip + sctx * 2);
        }
    
        for (i = 0; i < bw4 && (unsigned int)(bx4 + i) < state->above_skip_n; i++)
            state->above_skip[bx4 + i] = (stbv_u8)block_skip;
        for (i = 0; i < bh4 && (unsigned int)(by4 + i) < state->left_skip_n; i++)
            state->left_skip[by4 + i] = (stbv_u8)block_skip;
    }

    /* cdef index, once per superblock, only when the block is not skipped.
     * The slot state resets at each superblock start (dav1d decode_sb). */
    if (frame) {
        int sbx = bx4 & ~(sb_step - 1);
        int sby = by4 & ~(sb_step - 1);
        int idx;
        if (state->cdef_sb_x != sbx || state->cdef_sb_y != sby) {
            state->cdef_sb_x = sbx;
            state->cdef_sb_y = sby;
            state->cdef_idx[0] = state->cdef_idx[1] = -1;
            state->cdef_idx[2] = state->cdef_idx[3] = -1;
        }
        idx = seq && seq->sb128 ? ((bx4 & 16) >> 4) + ((by4 & 16) >> 3) : 0;
        if (!block_skip && state->cdef_idx[idx] == -1) {
            int v;
            v = stb_av1_msac_bools(msac, frame->cdef.n_bits);
            state->cdef_idx[idx] = v;
            if (bw4 > 16) state->cdef_idx[idx + 1] = v;
            if (bh4 > 16) state->cdef_idx[idx + 2] = v;
            if (bw4 == 32 && bh4 == 32) state->cdef_idx[idx + 3] = v;
            /* Write to cdef_idx output grid for post-decode CDEF filtering. */
            if (state->cdef_idx_grid && state->cdef_grid_stride > 0) {
                int gx = sbx / 16 + (idx & 1);
                int gy = sby / 16 + (idx >> 1);
                if (gx >= 0 && gx < state->cdef_grid_stride &&
                    gy >= 0)
                    state->cdef_idx_grid[gy * state->cdef_grid_stride + gx] = v;
            }
        }
        /* Also write cdef_idx for skipped blocks - use 0 as default. */
        if (block_skip && state->cdef_idx[idx] == -1) {
            state->cdef_idx[idx] = 0;
            if (bw4 > 16) state->cdef_idx[idx + 1] = 0;
            if (bh4 > 16) state->cdef_idx[idx + 2] = 0;
            if (bw4 == 32 && bh4 == 32) state->cdef_idx[idx + 3] = 0;
            if (state->cdef_idx_grid && state->cdef_grid_stride > 0) {
                int gx = sbx / 16 + (idx & 1);
                int gy = sby / 16 + (idx >> 1);
                if (gx >= 0 && gx < state->cdef_grid_stride &&
                    gy >= 0)
                    state->cdef_idx_grid[gy * state->cdef_grid_stride + gx] = 0;
            }
        }
    }

    /* delta-q/lf at superblock origin (dav1d decode.c:962-1028). */
    if (frame && frame->delta_q_present &&
        !((bx4 | by4) & (sb_step - 1)))
    {
        int have_delta_q = (bs != (int)(seq && seq->sb128 ? STBV_AV1_BS_128x128 : STBV_AV1_BS_64x64) || !block_skip);
        if (have_delta_q) {
            int dq = (int)stb_av1_msac_symbol(msac, cdf->delta_q, 3);
            if (dq == 3) {
                int nb = 1 + (int)stb_av1_msac_bools(msac, 3);
                dq = (int)stb_av1_msac_bools(msac, (unsigned)nb) + 1 + (1 << nb);
            }
            if (dq) {
                if (stb_av1_msac_bool_equi(msac)) dq = -dq;
                dq *= 1 << frame->delta_q_res_log2;
            }
            state->last_qidx = dq + state->last_qidx;
            if (state->last_qidx < 0) state->last_qidx = 0;
            if (state->last_qidx > 255) state->last_qidx = 255;
            if (frame->delta_lf_present) {
                int nlfs = frame->delta_lf_multi ?
                    (seq && seq->layout != STB_AV1_LAYOUT_I400 ? 4 : 2) : 1;
                int i;
                for (i = 0; i < nlfs; i++) {
                    int dl = (int)stb_av1_msac_symbol(msac,
                        cdf->delta_lf + (i + (int)frame->delta_lf_multi) * 4, 3);
                    if (dl == 3) {
                        int nb = 1 + (int)stb_av1_msac_bools(msac, 3);
                        dl = (int)stb_av1_msac_bools(msac, (unsigned)nb) + 1 + (1 << nb);
                    }
                    if (dl) {
                        if (stb_av1_msac_bool_equi(msac)) dl = -dl;
                        dl *= 1 << frame->delta_lf_res_log2;
                    }
                    state->last_delta_lf[i] += dl;
                    if (state->last_delta_lf[i] < -63) state->last_delta_lf[i] = -63;
                    if (state->last_delta_lf[i] > 63) state->last_delta_lf[i] = 63;
                }
            }
        }
        qidx = state->last_qidx;
    }

    /* Intra flag: for key frames with allow_intrabc, decode the intrabc
     * flag (dav1d decode.c:1043-1044).  For key frames without intrabc,
     * all blocks are implicitly intra. */
    if (frame && frame->allow_intrabc) {
        intra_flag = !stb_av1_msac_bool_adapt(msac, cdf->intrabc);
    } else {
        /* IBC: no intra mode decode; set defaults for ctx. */
        memset(&intra, 0, sizeof(intra));
        intra.y_mode = STBV_AV1_INTRA_DC;
        intra.uv_mode = STBV_AV1_INTRA_DC;
    }

    /* Recompute qidx using the SB-level last_qidx (may have been updated
     * by delta_q above). */
    qidx = state->last_qidx + (frame ? (int)frame->segmentation.d[seg_id].delta_q : 0);
    if (qidx < 0) qidx = 0;
    if (qidx > 255) qidx = 255;

    /* IBC MV residual decode (dav1d decode.c:1267-1340).
     * Find spatial MV prediction from above/left IBC neighbours, then
     * decode residual relative to that prediction. */
    if (!intra_flag) {
        int pred_mv_y = 0, pred_mv_x = 0;
        int mv_y, mv_x;
        int sb128 = (seq && seq->sb128) ? 1 : 0;
        int frame_top4 = 0;
        stbv_av1_find_ibc_mv_pred(state, bx4, by4, bw4, bh4,
                                   frame_top4, sb128,
                                   &pred_mv_y, &pred_mv_x);
        mv_y = pred_mv_y;
        mv_x = pred_mv_x;
    
        stbv_av1_read_mv_residual(msac, cdf, &mv_y, &mv_x, -1, bx4, by4);

        /* Clip IBC MV to decoded parts of the current tile/SB
         * (dav1d decode.c:1292-1346).  All values in pixel units. */
        {
            int fw = frame ? (int)frame->width[0] : 0;
            int border_left  = 0;
            int border_top   = 0;
            int border_right, src_left, src_top, src_right, src_bottom;
            int sbx, sby, sb_size;

            if (has_chroma) {
                if (bw4 < 2 && ss_hor) border_left += 4;
                if (bh4 < 2 && ss_ver) border_top  += 4;
            }

            src_left   = bx4 * 4 + (mv_x >> 3);
            src_top    = by4 * 4 + (mv_y >> 3);
            src_right  = src_left + bw4 * 4;
            src_bottom = src_top  + bh4 * 4;

            /* Single-tile: border_right = frame width rounded up to bw4 */
            border_right = ((fw + (bw4 * 4 - 1)) & ~(bw4 * 4 - 1));

            /* Clip to left/right tile boundary */
            if (src_left < border_left) {
                src_right += border_left - src_left;
                src_left  += border_left - src_left;
            } else if (src_right > border_right) {
                src_left  -= src_right - border_right;
                src_right -= src_right - border_right;
            }
            /* Clip to top tile boundary */
            if (src_top < border_top) {
                src_bottom += border_top - src_top;
                src_top    += border_top - src_top;
            }

            /* SB position and size in pixel units */
            sb_size = 1 << (6 + sb128);
            sbx = (bx4 >> (4 + sb128)) << (6 + sb128);
            sby = (by4 >> (4 + sb128)) << (6 + sb128);

            /* Avoid overlap with current superblock */
            if (src_bottom > sby && src_right > sbx) {
                if (src_top - border_top >= src_bottom - sby) {
                    src_top    -= src_bottom - sby;
                    src_bottom -= src_bottom - sby;
                } else if (src_left - border_left >= src_right - sbx) {
                    src_left  -= src_right - sbx;
                    src_right -= src_right - sbx;
                }
            }
            /* Move src up if below current SB row */
            if (src_bottom > sby + sb_size) {
                src_top    -= src_bottom - (sby + sb_size);
                src_bottom -= src_bottom - (sby + sb_size);
            }

            /* Write back clipped MV in 1/8-pel luma units */
            mv_x = (src_left - bx4 * 4) * 8;
            mv_y = (src_top  - by4 * 4) * 8;
        }

        c.ibc_mv_y = mv_y;
        c.ibc_mv_x = mv_x;
    } else {
        c.ibc_mv_y = 0;
        c.ibc_mv_x = 0;
    }

    cfl_allowed = lossless ? (cbw4 == 1 && cbh4 == 1) :
        !!(STBV_AV1_CFL_ALLOWED_MASK & (1U << bs));
    if (intra_flag) {
        if (stb_av1_intra_state_decode_leaf(msac, cdf, &state->intra,
                                             bx4, by4, bs, cfl_allowed,
                                              has_chroma, &intra))
            return -3;
    } else {
        /* IBC: no intra mode decode; set defaults for ctx. */
        memset(&intra, 0, sizeof(intra));
        intra.y_mode = STBV_AV1_INTRA_DC;
        intra.uv_mode = STBV_AV1_INTRA_DC;
    }
    /* Palette, filter-intra, and palette indices: intra-only.
     * IBC blocks skip all of these (dav1d decode.c:1267). */
    state->pal_sz_y = 0;
    state->pal_sz_uv = 0;
    if (intra_flag) {
        if (frame && frame->allow_screen_content_tools &&
            (bw4 > bh4 ? bw4 : bh4) <= 16 && bw4 + bh4 >= 4) {
            int sz_ctx = stbv_av1_block_dimensions[bs][2] +
                         stbv_av1_block_dimensions[bs][3] - 2;
            int bpc = 8 + (seq ? seq->hbd : 0) * 2;
            if (intra.y_mode == STBV_AV1_INTRA_DC) {
                int pal_ctx = 0;
                int above_palsz = 0, left_palsz = 0;
                if (state->above_pal_sz && (unsigned)bx4 < state->above_pal_sz_n &&
                    state->above_pal_sz[bx4] > 0) {
                    pal_ctx++;
                    above_palsz = state->above_pal_sz[bx4];
                }
                if (state->left_pal_sz && (unsigned)by4 < state->left_pal_sz_n &&
                    state->left_pal_sz[by4] > 0) {
                    pal_ctx++;
                    left_palsz = state->left_pal_sz[by4];
                }
                {
                    int pal_result = stb_av1_msac_bool_adapt(msac,
                                                cdf->pal_y + sz_ctx * 6 + pal_ctx * 2);
                    if (pal_result) {
                        if (stbv_av1_palette_read_plane(msac, cdf, state, 0, sz_ctx,
                                                        bx4, by4, bpc, state->pal_y,
                                                        &state->pal_sz_y))
                            return -7;
                    }
                }
            }
            if (has_chroma && intra.uv_mode == STBV_AV1_INTRA_DC) {
                int pal_ctx = state->pal_sz_y > 0;
                int pal_bool = stb_av1_msac_bool_adapt(msac, cdf->pal_uv + pal_ctx * 2);
                if (pal_bool) {
                    if (stbv_av1_palette_read_plane(msac, cdf, state, 1, sz_ctx,
                                                    bx4, by4, bpc, state->pal_u,
                                                    &state->pal_sz_uv))
                        return -8;
                    stbv_av1_palette_read_uv_v(msac, bpc, state->pal_sz_uv,
                                               state->pal_v);
                }
            }
        }

        /* Filter-intra bool (dav1d decode.c, after the palette bools). */
        if (seq && seq->filter_intra && intra.y_mode == STBV_AV1_INTRA_DC &&
            !state->pal_sz_y &&
            stbv_av1_block_dimensions[bs][2] <= 3 &&
            stbv_av1_block_dimensions[bs][3] <= 3) {
            if (stb_av1_msac_bool_adapt(msac, cdf->use_filter_intra + bs * 2)) {
                intra.y_mode = STBV_AV1_INTRA_FILTER;
                intra.y_angle = (int)stb_av1_msac_symbol(msac, cdf->filter_intra, 4);
            }
        }

        /* Palette index maps come after filter-intra. */
        if (state->pal_sz_y) {
            if (stbv_av1_palette_indices(msac, cdf, 0, state->pal_sz_y,
                                         bw4, bh4, state->pal_tmp_y,
                                         state->pal_order, state->pal_ctxs))
                return -7;
        }
        if (state->pal_sz_uv) {
            if (stbv_av1_palette_indices(msac, cdf, 1, state->pal_sz_uv,
                                         cbw4, cbh4, state->pal_tmp,
                                         state->pal_order, state->pal_ctxs))
                return -8;
        }
    } /* end intra-only palette/filter-intra */

    /* block_info hook: fires AFTER all mode decisions are final (including
     * filter_intra override), so reconstruction uses the correct mode.
     * For palette blocks this still fires but luma_pal/chroma_pal will
     * overwrite the prediction afterwards. */
    if (c.recon && c.recon->block_info) {
        c.recon->block_info(c.recon->ud, intra_flag, bs, bx4, by4,
                            has_chroma, cbw4, cbh4, 0, 0,
                            state->pal_sz_y, state->pal_sz_uv,
                            (int)block_skip,
                            intra.y_mode, intra.y_angle, intra.uv_mode,
                            intra.uv_angle,
                            intra.cfl_alpha_u, intra.cfl_alpha_v,
                             c.ibc_mv_y, c.ibc_mv_x);
    }

    /* Palette pixel application must run AFTER block_info (the callbacks
     * read the recon context's current block position) and before the
     * coefficient loop; txb prediction is suppressed for palette blocks
     * so nothing overwrites these pixels. */
    if (state->pal_sz_y && c.recon && c.recon->luma_pal)
        c.recon->luma_pal(c.recon->ud, state->pal_tmp_y, state->pal_sz_y,
                          bw4, bh4, state->pal_y);
    if (state->pal_sz_uv && c.recon && c.recon->chroma_pal) {
        c.recon->chroma_pal(c.recon->ud, 0, state->pal_tmp, state->pal_sz_uv, cbw4, cbh4, state->pal_u);
        c.recon->chroma_pal(c.recon->ud, 1, state->pal_tmp, state->pal_sz_uv, cbw4, cbh4, state->pal_v);
    }

    /* NOTE: neighbour-mode / palette context writes happen AFTER the
     * reconstruction loop below (dav1d calls set_ctx after recon), so
     * prediction reads the PRE-BLOCK neighbour state. */

    /* Transform size.  dav1d: lossless blocks are forced to TX_4X4; the
     * maximum otherwise comes from max_txfm_size_for_bs[bs][plane]; with
     * TX_SWITCHABLE a tx-size symbol is coded when max > TX_4X4.
     *
     * For IBC blocks, the TX tree bools are decoded separately via
     * read_vartx_tree/read_tx_tree (dav1d decode.c:1352).  No single
     * tx-size symbol is decoded for IBC. */
    if (lossless) {
        tx0 = STBV_AV1_TX_4X4;
        uv_tx = STBV_AV1_TX_4X4;
        max_tx = STBV_AV1_TX_4X4;
    } else {
        tx0 = stbv_av1_max_tx_for_bs[bs][0];
        uv_tx = stbv_av1_max_tx_for_bs[bs][layout];
        max_tx = tx0;
        if (intra_flag &&
            frame && frame->txfm_mode == 1 &&
            stbv_av1_tx_dims[max_tx].max > STBV_AV1_TX_4X4) {
            tx0 = stbv_av1_decode_tx_size(msac, cdf, max_tx,
                                          stbv_av1_tx_is_large(state->tx.above_tx_intra, bx4,
                                                               stbv_av1_tx_dims[max_tx].lw,
                                                               state->tx.above_n) +
                                           stbv_av1_tx_is_large(state->tx.left_tx_intra, by4,
                                                                stbv_av1_tx_dims[max_tx].lh,
                                                                state->tx.left_n));
        }
    }
    c.msac = msac;
    c.cdf = cdf;
    c.state = state;
    c.frame = frame;
    c.intra = &intra;
    c.bs = bs;
    c.bw4 = bw4;
    c.bw4_unc = bw4_unc;
    c.bh4_unc = bh4_unc;
    c.bh4 = bh4;
    c.cbw4 = cbw4;
    c.cbh4 = cbh4;
    c.cbw4_unc = cbw4_unc;
    c.cbh4_unc = cbh4_unc;
    c.lossless = lossless;
    c.qidx = qidx;
    /* dav1d stores y_mode_nofilt in the neighbour mode maps (set_ctx):
     * FILTER_PRED maps to DC_PRED, NOT to the filter angle's mode. */
    y_mode_nofilt = intra.y_mode == STBV_AV1_INTRA_FILTER ?
        STBV_AV1_INTRA_DC : intra.y_mode;
    c.y_mode_nofilt = y_mode_nofilt;
    c.reduced_txtp_set = frame ? (int)frame->reduced_txtp_set : 0;
    c.hbd = seq ? (int)seq->hbd : 0;
    c.block_skip = (int)block_skip;
    c.is_intra = intra_flag;
    /* TXTP mode: dav1d maps FILTER_PRED to the filter angle's directional
     * mode (dav1d_filter_mode_to_y_mode), unlike the neighbour-mode map
     * which uses DC_PRED. */
    {
        static const int stb_filter_mode_to_y_mode[5] =
            { STBV_AV1_INTRA_DC, STBV_AV1_INTRA_VERT, STBV_AV1_INTRA_HOR,
              STBV_AV1_INTRA_HD, STBV_AV1_INTRA_DC };
        int ym = intra.y_mode;
        if (ym == STBV_AV1_INTRA_FILTER) {
            ym = stb_filter_mode_to_y_mode[intra.y_angle < 0 ? 0 :
                 (intra.y_angle > 4 ? 4 : intra.y_angle)];
        }
        c.y_mode_txtp = ym;
    }

    /* Coefficients: intra blocks use one transform size across the whole
     * block.  IBC blocks use a variable TX tree for luma and fixed uv_tx
     * for chroma (dav1d decode.c:1352 read_vartx_tree). */
    {
        int txw4 = stbv_av1_tx_dims[tx0].w;
        int txh4 = stbv_av1_tx_dims[tx0].h;
        int y4, x4, cx4, cy4, pl, r;
        int uv_txw4 = stbv_av1_tx_dims[uv_tx].w;
        int uv_txh4 = stbv_av1_tx_dims[uv_tx].h;
        int first = 1;
        int qy4, qx4, qh4, qw4, sch4, scw4;

        if (!block_skip) {
            for (qy4 = by4; qy4 < by4 + bh4; qy4 += 16) {
                qh4 = by4 + bh4 - qy4;
                if (qh4 > 16) qh4 = 16;
                sch4 = (qh4 + ss_ver) >> ss_ver;
                for (qx4 = bx4; qx4 < bx4 + bw4; qx4 += 16) {
                    qw4 = bx4 + bw4 - qx4;
                    if (qw4 > 16) qw4 = 16;
                    scw4 = (qw4 + ss_hor) >> ss_hor;

                    if (intra_flag) {
                        /* Intra: fixed tx0 luma + fixed uv_tx chroma */
                        for (y4 = qy4; y4 < qy4 + qh4; y4 += txh4) {
                            for (x4 = qx4; x4 < qx4 + qw4; x4 += txw4) {
                                r = stbv_av1_leaf_tx_plane(msac, cdf, &c,
                                                           x4, y4,
                                                           tx0, 0, &state->res,
                                                           bw4, bh4,
                                                           first ? out : NULL);
                                first = 0;
                                if (r) return -4;
                            }
                        }
                    } else {
                        /* IBC: variable TX tree for luma (dav1d read_vartx_tree +
                         * read_coef_tree).  Chroma uses fixed uv_tx.
                         * dav1d reads ALL split bools first (read_vartx_tree),
                         * then decodes ALL coefficients (read_coef_tree/recon_b_inter).
                         * We must match this order exactly. */
                        int ytxw = stbv_av1_tx_dims[max_tx].w;
                        int ytxh = stbv_av1_tx_dims[max_tx].h;
                        if (!block_skip &&
                            stbv_av1_tx_dims[max_tx].max > STBV_AV1_TX_4X4 &&
                            frame && frame->txfm_mode == 1) {
                            /* Pass 1: Read all split bools (dav1d read_vartx_tree).
                             * Collect into a shared tx_split array indexed by
                             * y_off*4+x_off, matching dav1d's mask layout. */
                            stbv_u16 tx_split[2] = { 0, 0 };
                            int ty4, tx4, y_off, x_off;
                            for (ty4 = qy4, y_off = 0; ty4 < qy4 + qh4; ty4 += ytxh, y_off++) {
                                for (tx4 = qx4, x_off = 0; tx4 < qx4 + qw4; tx4 += ytxw, x_off++) {
                                    stbv_av1_tx_tree_read_splits(msac, cdf,
                                        &state->tx, max_tx, 0,
                                        tx_split, tx4, ty4,
                                        x_off, y_off);
                                }
                            }
                            /* Pass 2: Decode coefficients at leaves (dav1d read_coef_tree). */
                            for (ty4 = qy4, y_off = 0; ty4 < qy4 + qh4; ty4 += ytxh, y_off++) {
                                for (tx4 = qx4, x_off = 0; tx4 < qx4 + qw4; tx4 += ytxw, x_off++) {
                                    r = stbv_av1_tx_tree_read_coefs(msac, cdf,
                                        &state->tx, max_tx, 0,
                                        tx_split, tx4, ty4,
                                        x_off, y_off,
                                        stbv_av1_ibc_luma_leaf, &c);
                                    if (r) return -4;
                                }
                            }
                        } else {
                            /* Fixed max_tx luma (non-switchable or lossless).
                             * dav1d read_vartx_tree path 1: when max_ytx==TX_4X4
                             * and txfm_mode==SWITCHABLE, sets edge->tx to TX_4X4.
                             * We must do the same. */
                            if (!block_skip && frame && frame->txfm_mode == 1) {
                                int ii;
                                for (ii = 0; ii < bw4 && (unsigned int)(bx4 + ii) < state->tx.above_n; ii++)
                                    state->tx.above_tx[bx4 + ii] = STBV_AV1_TX_4X4;
                                for (ii = 0; ii < bh4 && (unsigned int)(by4 + ii) < state->tx.left_n; ii++)
                                    state->tx.left_tx[by4 + ii] = STBV_AV1_TX_4X4;
                            }
                            for (y4 = qy4; y4 < qy4 + qh4; y4 += txh4) {
                                for (x4 = qx4; x4 < qx4 + qw4; x4 += txw4) {
                                    r = stbv_av1_leaf_tx_plane(msac, cdf, &c,
                                                               x4, y4,
                                                               max_tx, 0,
                                                               &state->res,
                                                               bw4, bh4,
                                                               first ? out : NULL);
                                    first = 0;
                                    if (r) {
                                        return -4;
                                    }
                                }
                            }
                        }
                    }

                    /* Chroma: fixed uv_tx (both intra and IBC). */
                    if (!has_chroma) continue;
                    {
                        int cbx4 = qx4 >> ss_hor;
                        int cby4 = qy4 >> ss_ver;
                        for (pl = 0; pl < 2; pl++) {
                            for (cy4 = cby4; cy4 < cby4 + sch4;
                                 cy4 += uv_txh4) {
                                for (cx4 = cbx4; cx4 < cbx4 + scw4;
                                     cx4 += uv_txw4) {
                                    r = stbv_av1_leaf_tx_plane(msac, cdf,
                                        &c, cx4, cy4, uv_tx, pl + 1,
                                        &state->cres[pl], cbw4, cbh4,
                                        NULL);
                                     if (r) return -4;
                                }
                            }
                        }
                    }
                }
            }

        } else {
            /* dav1d read_coef_blocks marks the full block edges 0x40. */
            /* dav1d memsets context with UNCLIPPED b_dim; clipping here
             * left unit 383 unmarked for boundary blocks (bh4=7 case),
             * corrupting skip_ctx for every later block in the SB row. */
            stbv_av1_res_mark_unc(&state->res, bx4, by4, bw4_unc, bh4_unc,
                                  (stbv_u8)0x40);
            if (has_chroma) {
                int cbx4 = bx4 >> ss_hor;
                int cby4 = by4 >> ss_ver;
                for (pl = 0; pl < 2; pl++)
                    stbv_av1_res_mark_unc(&state->cres[pl], cbx4, cby4,
                                          (bw4_unc + ss_hor) >> ss_hor,
                                          (bh4_unc + ss_ver) >> ss_ver,
                                          (stbv_u8)0x40);
    }

    if (out) {
                out->x4 = bx4;
                out->y4 = by4;
                out->tx = tx0;
                out->skipped = 1;
                out->txtp = lossless ? STBV_AV1_TX_WHT_WHT : STBV_AV1_TX_DCT_DCT;
                out->eob = 0;
                out->skip_ctx = 0;
            }
            /* Skip blocks have no coefficients, but intra prediction must
             * still be written (dav1d recon_b_intra: prediction always runs,
             * skip suppresses only the residual).  Without this, skip-block
             * chroma planes remain zero (calloc), producing green output. */
            if (c.recon) {
                int txw4 = stbv_av1_tx_dims[tx0].w;
                int txh4 = stbv_av1_tx_dims[tx0].h;
                int uv_txw4 = stbv_av1_tx_dims[uv_tx].w;
                int uv_txh4 = stbv_av1_tx_dims[uv_tx].h;
                int qy4, qx4, qh4, qw4, sch4, scw4;
                int txtp_skip = lossless ? STBV_AV1_TX_WHT_WHT
                                         : STBV_AV1_TX_DCT_DCT;
                for (qy4 = by4; qy4 < by4 + bh4; qy4 += 16) {
                    qh4 = by4 + bh4 - qy4;
                    if (qh4 > 16) qh4 = 16;
                    sch4 = (qh4 + ss_ver) >> ss_ver;
                    for (qx4 = bx4; qx4 < bx4 + bw4; qx4 += 16) {
                        int y4, x4, cy4, cx4;
                        qw4 = bx4 + bw4 - qx4;
                        if (qw4 > 16) qw4 = 16;
                        scw4 = (qw4 + ss_hor) >> ss_hor;
                        if (c.recon->luma_txb) {
                            for (y4 = qy4; y4 < qy4 + qh4; y4 += txh4)
                                for (x4 = qx4; x4 < qx4 + qw4; x4 += txw4)
                                    c.recon->luma_txb(c.recon->ud, x4, y4,
                                        tx0, txtp_skip, -1, NULL);
                        }
                        if (has_chroma && c.recon->chroma_txb) {
                            int cbx4 = qx4 >> ss_hor;
                            int cby4 = qy4 >> ss_ver;
                            for (pl = 0; pl < 2; pl++)
                                for (cy4 = cby4; cy4 < cby4 + sch4;
                                     cy4 += uv_txh4)
                                    for (cx4 = cbx4; cx4 < cbx4 + scw4;
                                         cx4 += uv_txw4)
                                        c.recon->chroma_txb(c.recon->ud,
                                            pl, cx4, cy4, uv_tx, txtp_skip,
                                            -1, NULL);
                        }
                    }
                }
            }
        }

        /* Tx neighbour map (edge->tx): dav1d intra set_ctx writes decoded TX
         * lw/lh; IBC set_ctx does NOT write edge->tx (read_tx_tree leaf
         * path already set it). Skipped blocks: dav1d read_vartx_tree writes
         * b_dim[2]/b_dim[3] (max TX) directly. */
        if (intra_flag || block_skip) {
            int txm = (int)block_skip ? max_tx : tx0;
            for (i = 0; i < bw4 && (unsigned int)(bx4 + i) < state->tx.above_n; i++)
                state->tx.above_tx[bx4 + i] =
                    (stbv_u8)stbv_av1_tx_dims[txm].lw;
            for (i = 0; i < bh4 && (unsigned int)(by4 + i) < state->tx.left_n; i++)
                state->tx.left_tx[by4 + i] =
                    (stbv_u8)stbv_av1_tx_dims[txm].lh;
        }
        /* tx_intra: for intra blocks store decoded TX lw/lh; for IBC
         * blocks store max TX (dav1d: intra set_ctx uses t_dim->lw/lh
         * which is the decoded TX; IBC set_ctx uses b_dim[2+i] which
         * is the max TX). get_tx_ctx() compares this against the next
         * block's max TX to form the TX size context. */
        {
            int lw, lh;
            if (intra_flag) {
                lw = stbv_av1_tx_dims[tx0].lw;
                lh = stbv_av1_tx_dims[tx0].lh;
            } else {
                lw = stbv_av1_tx_dims[max_tx].lw;
                lh = stbv_av1_tx_dims[max_tx].lh;
            }
            if (state->tx.above_tx_intra) {
                for (i = 0; i < bw4 && (unsigned int)(bx4 + i) < state->tx.above_n; i++)
                    state->tx.above_tx_intra[bx4 + i] = (stbv_u8)lw;
            }
            if (state->tx.left_tx_intra) {
                for (i = 0; i < bh4 && (unsigned int)(by4 + i) < state->tx.left_n; i++)
                    state->tx.left_tx_intra[by4 + i] = (stbv_u8)lh;
            }
        }
    }

    /* Neighbour context writes: dav1d set_ctx runs AFTER reconstruction,
     * so prediction inside the loop above sees the pre-block state. */
    if (state->intra.above_mode) {
        for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->intra.above_count; i++)
            state->intra.above_mode[bx4 + i] = (stbv_u8)y_mode_nofilt;
    }
    if (state->intra.left_mode) {
        for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->intra.left_count; i++)
            state->intra.left_mode[by4 + i] = (stbv_u8)y_mode_nofilt;
    }
    if (has_chroma && state->intra.above_uvmode) {
        const int cbx4u = bx4 >> ss_hor;
        for (i = 0; i < cbw4_unc && (unsigned)(cbx4u + i) < state->intra.above_uv_count; i++)
            state->intra.above_uvmode[cbx4u + i] = (stbv_u8)intra.uv_mode;
    }
    if (has_chroma && state->intra.left_uvmode) {
        const int cby4u = by4 >> ss_ver;
        for (i = 0; i < cbh4_unc && (unsigned)(cby4u + i) < state->intra.left_uv_count; i++)
            state->intra.left_uvmode[cby4u + i] = (stbv_u8)intra.uv_mode;
    }

    /* Palette neighbour state (dav1d set_ctx: pal_sz maps + al_pal copies;
     * the UV palette sizes use luma coordinates). */
    if (state->above_pal_sz) {
        for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->above_pal_sz_n; i++)
            state->above_pal_sz[bx4 + i] = (stbv_u8)state->pal_sz_y;
    }
    if (state->left_pal_sz) {
        for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->left_pal_sz_n; i++)
            state->left_pal_sz[by4 + i] = (stbv_u8)state->pal_sz_y;
    }
    if (state->above_pal_uv) {
        for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->above_pal_uv_n; i++)
            state->above_pal_uv[bx4 + i] =
                (stbv_u8)(has_chroma ? state->pal_sz_uv : 0);
    }
    if (state->left_pal_uv) {
        for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->left_pal_uv_n; i++)
            state->left_pal_uv[by4 + i] =
                (stbv_u8)(has_chroma ? state->pal_sz_uv : 0);
    }
    if (state->pal_sz_y && state->above_pal[0]) {
        for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->above_pal_n; i++)
            memcpy(state->above_pal[0] + (bx4 + i) * 8, state->pal_y,
                   8 * sizeof(stbv_u16));
    }
    if (state->pal_sz_y && state->left_pal[0]) {
        for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->left_pal_n; i++)
            memcpy(state->left_pal[0] + (by4 + i) * 8, state->pal_y,
                   8 * sizeof(stbv_u16));
    }
    if (has_chroma && state->pal_sz_uv && state->above_pal[1]) {
        for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->above_pal_n; i++)
            memcpy(state->above_pal[1] + (bx4 + i) * 8, state->pal_u,
                   8 * sizeof(stbv_u16));
    }
    if (has_chroma && state->pal_sz_uv && state->left_pal[1]) {
        for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->left_pal_n; i++)
            memcpy(state->left_pal[1] + (by4 + i) * 8, state->pal_u,
                   8 * sizeof(stbv_u16));
    }

    /* IBC MV neighbour splat (dav1d decode.c splat_intrabc_mv + set_ctx).
     * For IBC blocks, store the decoded MV in the above/left arrays so
     * subsequent IBC blocks can use it as a prediction candidate.
     * For regular intra blocks, clear the IBC validity flags so stale
     * MV data from a previous IBC block does not leak.
     * Also splat to the 2D refmvs array for dav1d-compatible scanning. */
    if (!intra_flag) {
        if (state->above_ibc_mv_y && state->above_ibc_valid) {
            for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->above_ibc_mv_n; i++) {
                state->above_ibc_mv_y[bx4 + i] = c.ibc_mv_y;
                state->above_ibc_mv_x[bx4 + i] = c.ibc_mv_x;
                state->above_ibc_valid[bx4 + i] = 1;
            }
        }
        if (state->left_ibc_mv_y && state->left_ibc_valid) {
            for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->left_ibc_mv_n; i++) {
                state->left_ibc_mv_y[by4 + i] = c.ibc_mv_y;
                state->left_ibc_mv_x[by4 + i] = c.ibc_mv_x;
                state->left_ibc_valid[by4 + i] = 1;
            }
        }
        /* Splat to 2D refmvs array with valid=1 (IBC block). */
        if (state->refmvs_r) {
            stbv_refmvs_splat(state->refmvs_r, state->refmvs_stride,
                               bx4, by4, bw4, bh4, bs,
                               c.ibc_mv_y, c.ibc_mv_x, 1, 0);
        }
    } else {
        if (state->above_ibc_valid) {
            for (i = 0; i < bw4 && (unsigned)(bx4 + i) < state->above_ibc_mv_n; i++)
                state->above_ibc_valid[bx4 + i] = 0;
        }
        if (state->left_ibc_valid) {
            for (i = 0; i < bh4 && (unsigned)(by4 + i) < state->left_ibc_mv_n; i++)
                state->left_ibc_valid[by4 + i] = 0;
        }
        /* Splat to 2D refmvs array with valid=0 (regular intra, not IBC).
         * dav1d stores an INVALID_MV entry for non-IBC intra blocks. */
        if (state->refmvs_r) {
            stbv_refmvs_splat_intra(state->refmvs_r, state->refmvs_stride,
                                     bx4, by4, bw4, bh4, bs);
        }
    }
    return 0;
}

#endif /* STB_AV1_LEAF_H */

/* ===== stb_av1_tile.h ===== */
/*
 * stb_av1_tile.h - AV1 tile-group bridge for the scalar decoder.
 *
 * The tile-group header handling follows dav1d 1.5.4 parse_tile_hdr().
 * This first bridge deliberately supports one tile only.  That is enough for
 * the common reduced-still AVIF case and, more importantly, gives the new
 * MSAC/CDF decoder the exact tile byte range instead of the legacy Boolean
 * reader's byte stream.
 */
#ifndef STB_AV1_TILE_H
#define STB_AV1_TILE_H

#include <stddef.h>

struct stb_av1_tile_span {
    const stbv_u8 *data;
    size_t size;
    unsigned int start;
    unsigned int end;
};

/* AV1 byte_alignment(): one 1 bit followed by zero bits until aligned. */
static int stb_av1_byte_align(struct stb_av1_getbits *gb)
{
    if (!gb)
        return -1;
    while (gb->bits_left)
        (void)stb_av1_get_bit(gb);
    return gb->error ? -1 : 0;
}

/* Parse the tile-group header and return the remaining bytes as the tile.
 * For a single tile, dav1d consumes no tile-group header bits at all. */
static int stb_av1_parse_tile_group(const struct stb_av1_framehdr *fh,
                                    struct stb_av1_getbits *gb,
                                    struct stb_av1_tile_span *tiles,
                                    unsigned int max_tiles,
                                    unsigned int *tile_count,
                                    unsigned int *tile_start,
                                    unsigned int *tile_end)
{
    unsigned int n_tiles, start, end, i, nbits, tile_size_bytes;
    const stbv_u8 *p, *pend;
    if (!fh || !gb || !tiles || !tile_count) return -1;
    n_tiles = fh->tiling.cols * fh->tiling.rows;
    if (!n_tiles || n_tiles > max_tiles) return -1;
    stb_av1_getbits_bytealign(gb);
    if (gb->error) { fprintf(stderr, "TG_FAIL: bytealign error\n"); return -1; }
    start = 0; end = n_tiles - 1;
    if (n_tiles > 1 && stb_av1_get_bit(gb)) {
        nbits = fh->tiling.log2_cols + fh->tiling.log2_rows;
        start = stb_av1_get_bits(gb, (int)nbits);
        end = stb_av1_get_bits(gb, (int)nbits);
        if (gb->error || start > end || end >= n_tiles) {
            fprintf(stderr, "TG_FAIL: tile header error gb_err=%d start=%u end=%u n_tiles=%u\n", gb->error, start, end, n_tiles);
            return -1;
        }
    }
    stb_av1_getbits_bytealign(gb);
    if (gb->error) { fprintf(stderr, "TG_FAIL: bytealign2 error\n"); return -1; }
    tile_size_bytes = fh->tiling.n_bytes;
    if (n_tiles > 1 && tile_size_bytes == 0) { fprintf(stderr, "TG_FAIL: n_bytes=0 multi-tile\n"); return -1; }
    p = gb->ptr; pend = gb->ptr_end; *tile_count = 0;
    for (i = start; i <= end; i++) {
        size_t sz; unsigned int k;
        if (i != end) {
            sz = 0;
            if ((size_t)(pend - p) < tile_size_bytes) {
                fprintf(stderr, "TG_FAIL: tile %u not enough bytes for size (%zu < %u)\n", i, (size_t)(pend - p), tile_size_bytes);
                return -1;
            }
            for (k = 0; k < tile_size_bytes; k++) sz |= (size_t)p[k] << (k * 8);
            sz += 1; p += tile_size_bytes;
            if (sz > (size_t)(pend - p)) {
                fprintf(stderr, "TG_FAIL: tile %u size %zu > remaining %zu\n", i, sz, (size_t)(pend - p));
                return -1;
            }
        } else sz = (size_t)(pend - p);
        tiles[*tile_count].data = p;
        tiles[*tile_count].size = sz;
        tiles[*tile_count].start = i; tiles[*tile_count].end = i;
        (*tile_count)++; p += sz;
    }
    if (p != pend) {
        fprintf(stderr, "TG_FAIL: p!=pend (%d != %d)\n", (int)(p - gb->ptr_start), (int)(pend - gb->ptr_start));
        return -1;
    }
    if (tile_start) *tile_start = start;
    if (tile_end) *tile_end = end;
    return 0;
}

#endif

/* ===== stb_av1_obu.h ===== */
/*
 * stb_av1_obu.h - AV1 OBU plumbing for the scalar decoder
 *
 * The OBU framing rules follow dav1d 1.5.4 src/obu.c.  This layer is
 * intentionally independent of the legacy Boolean reader in stb_avif.h.
 *
 * First target:
 *   - sequence header OBU
 *   - key/intra frame header OBU
 *   - combined FRAME OBU
 *   - TILE_GROUP OBU
 *   - one or more tile groups, but only tile 0 is exposed to the first
 *     reconstruction stage
 */
#ifndef STB_AV1_OBU_H
#define STB_AV1_OBU_H

#include <stddef.h>

#define STB_AV1_OBU_FLAG_FORBIDDEN 0x80
#define STB_AV1_OBU_FLAG_EXTENSION 0x04
#define STB_AV1_OBU_FLAG_SIZE      0x02

#ifndef STB_AV1_OBU_SEQUENCE_HEADER
#define STB_AV1_OBU_SEQUENCE_HEADER 1
#define STB_AV1_OBU_TEMPORAL_DELIMITER 2
#define STB_AV1_OBU_FRAME_HEADER 3
#define STB_AV1_OBU_TILE_GROUP 4
#define STB_AV1_OBU_METADATA 5
#define STB_AV1_OBU_FRAME 6
#define STB_AV1_OBU_REDUNDANT_FRAME_HEADER 7
#define STB_AV1_OBU_TILE_LIST 8
#define STB_AV1_OBU_PADDING 15
#endif

struct stb_av1_obu {
    unsigned int type;
    unsigned int extension;
    unsigned int temporal_id;
    unsigned int spatial_id;
    const stbv_u8 *data;
    size_t size;
};

#define STB_AV1_MAX_TILES (STB_AV1_MAX_TILE_COLS * STB_AV1_MAX_TILE_ROWS)

struct stb_av1_internal_stream {
    struct stb_av1_seqhdr seq;
    struct stb_av1_framehdr frame;
    int have_seq;
    int have_frame;

    const stbv_u8 *tile_data;
    size_t tile_size;
    unsigned int tile_start;
    unsigned int tile_end;
    struct stb_av1_tile_span tiles[STB_AV1_MAX_TILES];
    unsigned char tile_seen[STB_AV1_MAX_TILES];
    unsigned int tile_count;
};

/* Read one OBU from an already byte-aligned stream. */
static int stb_av1_read_obu(struct stb_av1_getbits *gb,
                            struct stb_av1_obu *obu)
{
    unsigned int hdr;
    unsigned int has_extension;
    unsigned int has_size;
    unsigned int reserved;
    unsigned int v;
    size_t start;
    size_t payload_start;
    size_t payload_size;

    if (!gb || !obu || gb->error || gb->bits_left)
        return -1;

    start = (size_t)(gb->ptr - gb->ptr_start);
    if (start >= (size_t)(gb->ptr_end - gb->ptr_start))
        return 1; /* end of stream */

    hdr = stb_av1_get_bits(gb, 8);
    if (hdr & STB_AV1_OBU_FLAG_FORBIDDEN)
        return -1;

    obu->type = (hdr >> 3) & 15U;
    has_extension = (hdr & STB_AV1_OBU_FLAG_EXTENSION) != 0;
    has_size = (hdr & STB_AV1_OBU_FLAG_SIZE) != 0;
    reserved = hdr & 1U;
    if (reserved)
        return -1;

    obu->extension = 0;
    obu->temporal_id = 0;
    obu->spatial_id = 0;

    if (has_extension) {
        v = stb_av1_get_bits(gb, 8);
        obu->temporal_id = (v >> 5) & 7U;
        obu->spatial_id = (v >> 3) & 3U;
        if (v & 7U)
            return -1;
        obu->extension = 1;
    }

    if (has_size) {
        unsigned int size = stb_av1_get_uleb128(gb);
        if (gb->error)
            return -1;
        payload_start = (size_t)(gb->ptr - gb->ptr_start);
        payload_size = (size_t)size;
        if (payload_size > (size_t)(gb->ptr_end - gb->ptr_start) - payload_start)
            return -1;
    } else {
        payload_start = (size_t)(gb->ptr - gb->ptr_start);
        payload_size = (size_t)(gb->ptr_end - gb->ptr_start) - payload_start;
    }

    obu->data = gb->ptr_start + payload_start;
    obu->size = payload_size;

    /* Move to the end of this OBU.  The payload itself is consumed later by
       a separate GetBits instance, so the outer reader remains byte based. */
    gb->ptr = gb->ptr_start + payload_start + payload_size;
    gb->bits_left = 0;
    gb->state = 0;
    return 0;
}

static int stb_av1_parse_obu_payload_seq(struct stb_av1_internal_stream *st,
                                         const struct stb_av1_obu *obu)
{
    struct stb_av1_getbits gb;
    int res;

    stb_av1_getbits_init(&gb, obu->data, obu->size);
    res = stb_av1_parse_seqhdr(&st->seq, &gb);
    if (res < 0 || gb.error)
        return -1;
    st->have_seq = 1;
    return 0;
}

static int stb_av1_parse_obu_payload_frame(struct stb_av1_internal_stream *st,
                                           const struct stb_av1_obu *obu,
                                           int combined)
{
    struct stb_av1_getbits gb;
    struct stb_av1_tile_span tile[STB_AV1_MAX_TILES];
    unsigned int ntile, i;
    int res;

    if (!st->have_seq)
        return -1;

    stb_av1_getbits_init(&gb, obu->data, obu->size);
    res = stb_av1_parse_framehdr(&st->frame, &st->seq, &gb);
    if (res < 0 || gb.error)
        return -1;
    st->have_frame = 1;

    if (!combined)
        return 0;

    if (st->frame.show_existing_frame)
        return -1;

    /* A FRAME OBU contains frame-header bits followed immediately by the
       tile-group header.  parse_tile_group() consumes that header and leaves
       the reader at the first MSAC byte. */
    if (stb_av1_parse_tile_group(&st->frame, &gb, tile, STB_AV1_MAX_TILES,
                                 &ntile, &st->tile_start, &st->tile_end) < 0)
        return -1;
    for (i = 0; i < ntile; i++) {
        unsigned int ti = tile[i].start;
        if (st->tile_seen[ti]) return -1;
        st->tile_seen[ti] = 1; st->tiles[ti] = tile[i]; st->tile_count++;
    }
    st->tile_data = st->tiles[0].data;
    st->tile_size = st->tiles[0].size;
    return 0;
}

/*
 * Parse enough of an AV1 still-image stream to expose the first tile.
 * Returns 0 on success, -1 on malformed/unsupported input.
 */
static int stb_av1_parse_internal_stream(struct stb_av1_internal_stream *st,
                                         const stbv_u8 *data, size_t size)
{
    struct stb_av1_getbits outer;
    struct stb_av1_obu obu;
    int r;

    if (!st || !data || !size)
        return -1;

    {
        unsigned char *p = (unsigned char *)st;
        size_t n = sizeof(*st);
        while (n--) *p++ = 0;
    }

    stb_av1_getbits_init(&outer, data, size);

    while ((size_t)(outer.ptr - outer.ptr_start) < size) {
        r = stb_av1_read_obu(&outer, &obu);
        if (r == 1)
            break;
        if (r < 0)
            return -1;

        switch (obu.type) {
        case STB_AV1_OBU_SEQUENCE_HEADER:
            if (stb_av1_parse_obu_payload_seq(st, &obu) < 0)
                return -1;
            break;

        case STB_AV1_OBU_FRAME:
            if (stb_av1_parse_obu_payload_frame(st, &obu, 1) < 0)
                return -1;
            if (st->tile_data)
                return 0;
            break;

        case STB_AV1_OBU_FRAME_HEADER:
        case STB_AV1_OBU_REDUNDANT_FRAME_HEADER:
            if (stb_av1_parse_obu_payload_frame(st, &obu, 0) < 0)
                return -1;
            break;

        case STB_AV1_OBU_TILE_GROUP: {
            struct stb_av1_getbits gb;
            struct stb_av1_tile_span tile[STB_AV1_MAX_TILES];
            unsigned int ntile, ti;
            if (!st->have_frame)
                return -1;
            stb_av1_getbits_init(&gb, obu.data, obu.size);
            if (stb_av1_parse_tile_group(&st->frame, &gb, tile, STB_AV1_MAX_TILES,
                                         &ntile, &st->tile_start, &st->tile_end) < 0)
                return -1;
            for (ti = 0; ti < ntile; ti++) {
                unsigned int idx = tile[ti].start;
                if (st->tile_seen[idx]) return -1;
                st->tile_seen[idx] = 1; st->tiles[idx] = tile[ti]; st->tile_count++;
            }
            if (st->tile_count == st->frame.tiling.cols * st->frame.tiling.rows) {
                st->tile_data = st->tiles[0].data;
                st->tile_size = st->tiles[0].size;
                return 0;
            }
        }

        default:
            break;
        }
    }

    return st->have_seq && st->have_frame && st->tile_count ? 0 : -1;
}

#endif

/* ===== stb_av1_tile_decode.h ===== */
/*
 * stb_av1_tile_decode.h - first real scalar AV1 tile walker
 *
 * This is the first integration point between the OBU/tile plumbing and the
 * MSAC/CDF partition decoder.  It deliberately stops at partition leaves:
 * leaf syntax is kept separate until the neighbor/context state is wired in.
 */
#ifndef STB_AV1_TILE_DECODE_H
#define STB_AV1_TILE_DECODE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef STB_AV1_PARTITION_DECODE_H
#error "include stb_av1_partition_decode.h first"
#endif

/* Restoration types (AV1 spec Table 6.16) */
#define STBV_AV1_RESTORATION_NONE       0
#define STBV_AV1_RESTORATION_SWITCHABLE 1
#define STBV_AV1_RESTORATION_WIENER     2
#define STBV_AV1_RESTORATION_SGRPROJ    3

static const unsigned short stbv_av1_sgr_params[16][2] = {
    { 140, 3236 }, { 112, 2158 }, {  93, 1618 }, {  80, 1438 },
    {  70, 1295 }, {  58, 1177 }, {  47, 1079 }, {  37,  996 },
    {  30,  925 }, {  25,  863 }, {   0, 2589 }, {   0, 1618 },
    {   0, 1177 }, {   0,  925 }, {  56,    0 }, {  22,    0 },
};

/* Per-plane restoration reference state */
typedef struct stbv_av1_lr_ref {
    int filter_v[3];  /* 0=offset, 1=sharp, 2=dense */
    int filter_h[3];
    int sgr_weights[2];
    int type_val;     /* decoded restoration type for this unit */
} stbv_av1_lr_ref;

static void stb_av1_read_restoration_info(struct stb_av1_msac *msac,
                                           struct stbv_av1_cdf *cdf,
                                           stbv_av1_lr_ref *lr_ref,
                                           int plane,
                                           unsigned int frame_type)
{
    int lr_type;

    if (frame_type == STBV_AV1_RESTORATION_SWITCHABLE) {
        int filter = (int)stb_av1_msac_symbol(msac, cdf->restore_switchable, 2);
        lr_type = filter + (filter != 0); /* NONE=0, WIENER=2, SGRPROJ=3 */
    } else {
        stbv_u16 *cdf_ptr = frame_type == STBV_AV1_RESTORATION_WIENER ?
            cdf->restore_wiener : cdf->restore_sgrproj;
        int has_filter;
        has_filter = (int)stb_av1_msac_bool_adapt(msac, cdf_ptr);
        lr_type = has_filter ? (int)frame_type : STBV_AV1_RESTORATION_NONE;
    }

    if (lr_type == STBV_AV1_RESTORATION_WIENER) {
        lr_ref->filter_v[0] = plane ? 0 :
            stb_av1_msac_subexp(msac, lr_ref->filter_v[0] + 5, 16, 1) - 5;
        lr_ref->filter_v[1] =
            stb_av1_msac_subexp(msac, lr_ref->filter_v[1] + 23, 32, 2) - 23;
        lr_ref->filter_v[2] =
            stb_av1_msac_subexp(msac, lr_ref->filter_v[2] + 17, 64, 3) - 17;
        lr_ref->filter_h[0] = plane ? 0 :
            stb_av1_msac_subexp(msac, lr_ref->filter_h[0] + 5, 16, 1) - 5;
        lr_ref->filter_h[1] =
            stb_av1_msac_subexp(msac, lr_ref->filter_h[1] + 23, 32, 2) - 23;
        lr_ref->filter_h[2] =
            stb_av1_msac_subexp(msac, lr_ref->filter_h[2] + 17, 64, 3) - 17;
        /* Wiener: copy sgr_weights from ref for delta coding */
        lr_ref->sgr_weights[0] = lr_ref->sgr_weights[0];
        lr_ref->sgr_weights[1] = lr_ref->sgr_weights[1];
    } else if (lr_type == STBV_AV1_RESTORATION_SGRPROJ) {
        unsigned int idx = stb_av1_msac_bools(msac, 4);
        int w0, w1;
        /* SGR index encoded into type (matches dav1d: lr->type += idx) */
        lr_type += (int)idx;
        w0 = stbv_av1_sgr_params[idx][0] ?
            stb_av1_msac_subexp(msac, lr_ref->sgr_weights[0] + 96, 128, 4) - 96 : 0;
        w1 = stbv_av1_sgr_params[idx][1] ?
            stb_av1_msac_subexp(msac, lr_ref->sgr_weights[1] + 32, 128, 4) - 32 : 95;
        lr_ref->sgr_weights[0] = w0;
        lr_ref->sgr_weights[1] = w1;
        /* SGR: copy wiener filters from ref for delta coding */
        lr_ref->filter_v[0] = lr_ref->filter_v[0];
        lr_ref->filter_v[1] = lr_ref->filter_v[1];
        lr_ref->filter_v[2] = lr_ref->filter_v[2];
        lr_ref->filter_h[0] = lr_ref->filter_h[0];
        lr_ref->filter_h[1] = lr_ref->filter_h[1];
        lr_ref->filter_h[2] = lr_ref->filter_h[2];
    }
    lr_ref->type_val = lr_type;
}

/* Per-SB restoration unit storage (for LR filter application after decode) */
typedef struct stbv_av1_lr_unit {
    unsigned char type;          /* STBV_AV1_RESTORATION_NONE/WIENER/SGRPROJ */
    signed char filter_h[3];     /* Wiener horizontal */
    signed char filter_v[3];     /* Wiener vertical */
    signed char sgr_weights[2];  /* SGR weights */
    unsigned char sgr_idx;       /* SGR index into params table */
} stbv_av1_lr_unit;

typedef struct stbv_av1_lr_mask {
    stbv_av1_lr_unit *units[3]; /* per-plane flat array of LR units */
    int grid_stride[3];         /* stride in LR units per plane */
    int grid_rows[3];           /* rows in LR units per plane */
    int unit_size_log2[2];      /* [0]=luma, [1]=chroma */
} stbv_av1_lr_mask;

struct stb_av1_tile_decoder {
    struct stb_av1_msac msac;
    stbv_av1_cdf cdf;
    const struct stb_av1_seqhdr *seq;
    const struct stb_av1_framehdr *frame;
    unsigned int tile_col;
    unsigned int tile_row;
    unsigned int tile_w4;
    unsigned int tile_h4;
    unsigned int leaves;
    int error;
    /* LR mask for storing decoded restoration params (owned by caller) */
    stbv_av1_lr_mask *lr_mask;
};

struct stb_av1_tile_leaf_info {
    int bl;
    int bs;
    int bp;
    int bx;
    int by;
};

typedef int (*stb_av1_tile_leaf_cb)(struct stb_av1_tile_decoder *td,
                                    const struct stb_av1_tile_leaf_info *li,
                                    void *opaque);

struct stb_av1_tile_walk_ctx {
    struct stb_av1_tile_decoder *td;
    stb_av1_tile_leaf_cb cb;
    void *opaque;
};

typedef void (*stb_av1_tile_row_cb)(void *opaque);

static int stb_av1_tile_leaf_dispatch(stbv_av1_partition_decoder *pd,
                                       int bl, int bs, int bp,
                                       int bx, int by, void *opaque)
{
    struct stb_av1_tile_walk_ctx *w = (struct stb_av1_tile_walk_ctx *)opaque;
    struct stb_av1_tile_leaf_info li;
    li.bl = bl;
    li.bs = bs;
    li.bp = bp;
    li.bx = bx;
    li.by = by;
    w->td->leaves++;
    if (w->cb) {
        int rc = w->cb(w->td, &li, w->opaque);
        if (rc) w->td->error = rc;
        /* Continue decoding even on leaf errors: dav1d skips the block
         * and keeps the MSAC state for subsequent blocks. */
        return 0;
    }
    return 0;
}

/*
 * Decode a single tile. Coordinates in the partition decoder are 4x4 units.
 * The initial implementation supports tile 0 only; the frame-header tiling
 * parser still exposes the exact tile geometry so this function can be
 * extended without changing the MSAC interface.
 */
static int stb_av1_decode_tile_at(struct stb_av1_tile_decoder *td,
                                  const struct stb_av1_seqhdr *seq,
                                  const struct stb_av1_framehdr *frame,
                                  const stbv_u8 *data, size_t size,
                                  unsigned int tile_col, unsigned int tile_row,
                                  stb_av1_tile_leaf_cb cb, void *opaque,
                                  stb_av1_tile_row_cb row_cb)
{
    stbv_av1_partition_decoder pd; struct stb_av1_tile_walk_ctx w;
    unsigned int sb_log2, sb_size, sx0, sy0, sx1, sy1;
    unsigned int tile_x4, tile_y4, tile_w4, tile_h4;
    int qcat, above_n, left_n; stbv_u8 *above, *left; int rc = 0;
    if (!td || !seq || !frame || !data || !size) return -1;
    if (frame->frame_type != STB_AV1_FRAME_KEY && frame->frame_type != STB_AV1_FRAME_INTRA_ONLY) return -2;
    if (tile_col >= frame->tiling.cols || tile_row >= frame->tiling.rows) return -3;
    if (frame->superres_enabled) return -4;
    {
        stbv_av1_lr_mask *saved_lr_mask = td->lr_mask;
        memset(td, 0, sizeof(*td));
        td->lr_mask = saved_lr_mask;
    }
    td->seq = seq; td->frame = frame;
    td->tile_col = tile_col; td->tile_row = tile_row;
    qcat = (frame->quant.yac > 20) + (frame->quant.yac > 60) + (frame->quant.yac > 120);
    stbv_av1_cdf_init(&td->cdf, (unsigned)qcat);
    stb_av1_msac_init(&td->msac, data, size, (int)frame->disable_cdf_update);
    sb_log2 = 6U + seq->sb128; sb_size = 1U << sb_log2;
    sx0 = frame->tiling.col_start_sb[tile_col]; sx1 = frame->tiling.col_start_sb[tile_col + 1];
    sy0 = frame->tiling.row_start_sb[tile_row]; sy1 = frame->tiling.row_start_sb[tile_row + 1];
    if (sx1 <= sx0 || sy1 <= sy0) return -5;
    tile_x4 = sx0 * (sb_size >> 2); tile_y4 = sy0 * (sb_size >> 2);
    tile_w4 = (sx1 - sx0) * (sb_size >> 2); tile_h4 = (sy1 - sy0) * (sb_size >> 2);
    pd.msac = &td->msac; pd.cdf = &td->cdf;
    /* dav1d uses the full frame dimensions (f->bw, f->bh) for partition
       split decisions, NOT tile-local dimensions.  Using tile-local sizes
       causes wrong splitting at the right/bottom edge of the frame. */
     pd.frame_w4 = (int)(((frame->width[0] + 7U) >> 3) << 1);
     pd.frame_h4 = (int)(((frame->height + 7U) >> 3) << 1);
    pd.ctx_x4 = (int)tile_x4; pd.ctx_y4 = (int)tile_y4;
    above_n = (int)(((tile_w4 + 15U) >> 1) + 1U); left_n = (int)(((tile_h4 + 15U) >> 1) + 1U);
    above = (stbv_u8 *)malloc((size_t)above_n); left = (stbv_u8 *)malloc((size_t)left_n);
    if (!above || !left) { if (above) free(above); if (left) free(left); return -6; }
    memset(above, 0, (size_t)above_n); memset(left, 0, (size_t)left_n);
    pd.above = above; pd.left = left; pd.above_n = above_n; pd.left_n = left_n;
    w.td = td; w.cb = cb; w.opaque = opaque; pd.leaf = stb_av1_tile_leaf_dispatch; pd.opaque = &w;
    /* Loop restoration info: per-SB MSAC reads before partition decode.
       dav1d reads these in the tile superblock row loop before decode_sb(). */
    {
        unsigned int restore_planes = 0;
        unsigned int lr_unit_size_log2[2] = {0, 0};
        stbv_av1_lr_ref lr_ref[3];
        unsigned int sy;
        int p;

        if (seq->restoration && !frame->allow_intrabc) {
            if (frame->restoration.type[0] != STBV_AV1_RESTORATION_NONE)
                restore_planes |= 1U;
            if (!seq->monochrome && frame->restoration.type[1] != STBV_AV1_RESTORATION_NONE)
                restore_planes |= 2U;
            if (!seq->monochrome && frame->restoration.type[2] != STBV_AV1_RESTORATION_NONE)
                restore_planes |= 4U;
            lr_unit_size_log2[0] = frame->restoration.unit_size[0];
            lr_unit_size_log2[1] = frame->restoration.unit_size[1];
        }
        /* LR reference defaults: set once per tile (dav1d does this in
         * setup_tile, NOT per row). The subexponential delta coding in
         * read_restoration_info uses lr_ref as the reference for the next
         * SB's LR params. Resetting per row would lose the carryover from
         * the previous row, causing wrong LR filter values and MSAC desync. */
        for (p = 0; p < 3; p++) {
            lr_ref[p].filter_v[0] = 3; lr_ref[p].filter_v[1] = -7; lr_ref[p].filter_v[2] = 15;
            lr_ref[p].filter_h[0] = 3; lr_ref[p].filter_h[1] = -7; lr_ref[p].filter_h[2] = 15;
            lr_ref[p].sgr_weights[0] = -32; lr_ref[p].sgr_weights[1] = 31;
        }
        for (sy = sy0; sy < sy1; sy++) { unsigned int sx;
          memset(left, 0, (size_t)left_n); if (sy == sy0) memset(above, 0, (size_t)above_n);
          if (row_cb) row_cb(opaque);
          for (sx = sx0; sx < sx1; sx++) {
            int bl = seq->sb128 ? STBV_AV1_BL_128X128 : STBV_AV1_BL_64X64;
            int bx = (int)(sx * (sb_size >> 2)); int by = (int)(sy * (sb_size >> 2));
            if (restore_planes) {
                for (p = 0; p < 3; p++) {
                    int ss_ver, ss_hor, unit_size_log2, unit_size, mask, half_unit;
                    int y, x, w_px, h_px;
                    if (!((restore_planes >> p) & 1U))
                        continue;
                    ss_ver = (p != 0) ? (int)(seq->ss_ver != 0) : 0;
                    ss_hor = (p != 0) ? (int)(seq->ss_hor != 0) : 0;
                    unit_size_log2 = (int)lr_unit_size_log2[p > 0 ? 1 : 0];
                    unit_size = 1 << unit_size_log2;
                    mask = unit_size - 1;
                    half_unit = unit_size >> 1;
                    y = by * 4 >> ss_ver;
                    h_px = ((int)frame->height + ss_ver) >> ss_ver;
                    x = bx * 4 >> ss_hor;
                    w_px = ((int)frame->width[0] + ss_hor) >> ss_hor;
                    if (y & mask) continue;
                    if (y && y + half_unit > h_px) continue;
                    if (x & mask) continue;
                    if (x && x + half_unit > w_px) continue;
                    stb_av1_read_restoration_info(&td->msac, &td->cdf,
                                                   &lr_ref[p], p,
                                                   frame->restoration.type[p]);
                    /* Store decoded LR params into the frame-level mask */
                    if (td->lr_mask) {
                        int lr_x = x >> unit_size_log2;
                        int lr_y = y >> unit_size_log2;
                        int gw = td->lr_mask->grid_stride[p];
                        int gr = td->lr_mask->grid_rows[p];
                        if (lr_x >= 0 && lr_x < gw && lr_y >= 0 && lr_y < gr) {
                            stbv_av1_lr_unit *u = &td->lr_mask->units[p][lr_y * gw + lr_x];
                            u->type = (stbv_u8)lr_ref[p].type_val;
                            u->filter_h[0] = (signed char)lr_ref[p].filter_h[0];
                            u->filter_h[1] = (signed char)lr_ref[p].filter_h[1];
                            u->filter_h[2] = (signed char)lr_ref[p].filter_h[2];
                            u->filter_v[0] = (signed char)lr_ref[p].filter_v[0];
                            u->filter_v[1] = (signed char)lr_ref[p].filter_v[1];
                            u->filter_v[2] = (signed char)lr_ref[p].filter_v[2];
                            u->sgr_weights[0] = (signed char)lr_ref[p].sgr_weights[0];
                            u->sgr_weights[1] = (signed char)lr_ref[p].sgr_weights[1];
                            u->sgr_idx = (u->type >= STBV_AV1_RESTORATION_SGRPROJ) ?
                                (stbv_u8)(u->type - STBV_AV1_RESTORATION_SGRPROJ) : 0;
                        }
                    }
                }
            }
            {
                if (stbv_av1_partition_decode_sb(&pd, bl, bx, by)) { td->error = 1; rc = -1; goto done; }
            }
          }
        }
    }
    rc = td->msac.buf_pos <= td->msac.buf_end ? 0 : -1;
done:
    free(above); free(left); return rc;
}

static int stb_av1_decode_tile(struct stb_av1_tile_decoder *td,
                               const struct stb_av1_seqhdr *seq,
                               const struct stb_av1_framehdr *frame,
                               const stbv_u8 *data, size_t size,
                               stb_av1_tile_leaf_cb cb, void *opaque,
                               stb_av1_tile_row_cb row_cb)
{
    return stb_av1_decode_tile_at(td, seq, frame, data, size, 0, 0, cb, opaque, row_cb);
}

#endif

#endif

/* ===== stb_av1_deblock.h ===== */
/*
 * stb_av1_deblock.h - scalar AV1 in-loop deblocking filter
 *
 * Faithful port of dav1d's loopfilter_tmpl.c kernel plus the edge
 * selection rules: an 8-pixel-aligned edge segment (one 4-pixel band at
 * a time) is filtered when a transform-block or prediction-block
 * boundary crosses it; skip suppresses nothing on intra still frames
 * (single transform per intra block).  Filter width per edge =
 * 4 << min(lw_left, lw_right, cap), cap 2 for luma / 1 for chroma
 * (uv widths 4 or 6).  Levels come straight from the frame header
 * (segmentation / delta-lf are not supported here).
 */
#ifndef STB_AV1_DEBLOCK_H
#define STB_AV1_DEBLOCK_H

#include <stddef.h>

static int stb_av1_db_iclip(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* dav1d loop_filter() kernel on 16-bit samples.  dst points at q0;
 * stridea steps along the edge, strideb across it. */
static void stb_av1_loop_filter_edge(stbv_u16 *dst, ptrdiff_t stridea,
                                     ptrdiff_t strideb,
                                     int E, int I, int H, int wd, int maxv,
                                     int bd8)
{
    const int F = 1 << bd8;
    int i;

    for (i = 0; i < 4; i++, dst += stridea) {
        int p6, p5, p4, p3, p2;
        int p1 = dst[strideb * -2], p0 = dst[strideb * -1];
        int q0 = dst[strideb * +0], q1 = dst[strideb * +1];
        int q2, q3, q4, q5, q6;
        int fm, flat8out, flat8in;

        fm = abs(p1 - p0) <= I && abs(q1 - q0) <= I &&
             abs(p0 - q0) * 2 + (abs(p1 - q1) >> 1) <= E;

        if (wd > 4) {
            p2 = dst[strideb * -3];
            q2 = dst[strideb * +2];
            fm &= abs(p2 - p1) <= I && abs(q2 - q1) <= I;
            if (wd > 6) {
                p3 = dst[strideb * -4];
                q3 = dst[strideb * +3];
                fm &= abs(p3 - p2) <= I && abs(q3 - q2) <= I;
            }
        }
        if (!fm) continue;

        if (wd >= 16) {
            p6 = dst[strideb * -7];
            p5 = dst[strideb * -6];
            p4 = dst[strideb * -5];
            q4 = dst[strideb * +4];
            q5 = dst[strideb * +5];
            q6 = dst[strideb * +6];
            flat8out = abs(p6 - p0) <= F && abs(p5 - p0) <= F &&
                       abs(p4 - p0) <= F && abs(q4 - q0) <= F &&
                       abs(q5 - q0) <= F && abs(q6 - q0) <= F;
        } else {
            flat8out = 0;
        }

        if (wd >= 6)
            flat8in = abs(p2 - p0) <= F && abs(p1 - p0) <= F &&
                      abs(q1 - q0) <= F && abs(q2 - q0) <= F;
        else
            flat8in = 0;

        if (wd >= 8)
            flat8in &= abs(p3 - p0) <= F && abs(q3 - q0) <= F;

        if (wd >= 16 && (flat8out & flat8in)) {
            dst[strideb * -6] = (stbv_u16)stb_av1_db_iclip(
                (p6*6 + p5*2 + p4*2 + p3 + p2 + p1 + p0 + q0 + q1 + 8) >> 4, 0, maxv);
            dst[strideb * -5] = (stbv_u16)stb_av1_db_iclip(
                (p6*5 + p5*2 + p4*2 + p3*2 + p2 + p1 + p0 + q0 + q1 + 8) >> 4, 0, maxv);
            dst[strideb * -4] = (stbv_u16)stb_av1_db_iclip(
                (p6*4 + p5 + p4*2 + p3*2 + p2*2 + p1 + p0 + q0 + q1 + q2 + 8) >> 4, 0, maxv);
            dst[strideb * -3] = (stbv_u16)stb_av1_db_iclip(
                (p6*3 + p5 + p4 + p3*2 + p2*2 + p1*2 + p0 + q0 + q1 + q2 + q3 + 8) >> 4, 0, maxv);
            dst[strideb * -2] = (stbv_u16)stb_av1_db_iclip(
                (p6*2 + p5 + p4 + p3 + p2*2 + p1*2 + p0*2 + q0 + q1 + q2 + q3 + q4 + 8) >> 4, 0, maxv);
            dst[strideb * -1] = (stbv_u16)stb_av1_db_iclip(
                (p6 + p5 + p4 + p3 + p2 + p1*2 + p0*2 + q0*2 + q1 + q2 + q3 + q4 + q5 + 8) >> 4, 0, maxv);
            dst[strideb * +0] = (stbv_u16)stb_av1_db_iclip(
                (p5 + p4 + p3 + p2 + p1 + p0*2 + q0*2 + q1*2 + q2 + q3 + q4 + q5 + q6 + 8) >> 4, 0, maxv);
            dst[strideb * +1] = (stbv_u16)stb_av1_db_iclip(
                (p4 + p3 + p2 + p1 + p0 + q0*2 + q1*2 + q2*2 + q3 + q4 + q5 + q6*2 + 8) >> 4, 0, maxv);
            dst[strideb * +2] = (stbv_u16)stb_av1_db_iclip(
                (p3 + p2 + p1 + p0 + q0 + q1*2 + q2*2 + q3*2 + q4 + q5 + q6*3 + 8) >> 4, 0, maxv);
            dst[strideb * +3] = (stbv_u16)stb_av1_db_iclip(
                (p2 + p1 + p0 + q0 + q1 + q2*2 + q3*2 + q4*2 + q5 + q6*4 + 8) >> 4, 0, maxv);
            dst[strideb * +4] = (stbv_u16)stb_av1_db_iclip(
                (p1 + p0 + q0 + q1 + q2 + q3*2 + q4*2 + q5*2 + q6*5 + 8) >> 4, 0, maxv);
            dst[strideb * +5] = (stbv_u16)stb_av1_db_iclip(
                (p1 + p0 + q0 + q1 + q2 + q3 + q4*2 + q5*2 + q6*6 + 8) >> 4, 0, maxv);
        } else if (wd >= 8 && flat8in) {
            dst[strideb * -3] = (stbv_u16)stb_av1_db_iclip(
                (p3*3 + p2*2 + p1 + p0 + q0 + 4) >> 3, 0, maxv);
            dst[strideb * -2] = (stbv_u16)stb_av1_db_iclip(
                (p3*2 + p2 + p1*2 + p0 + q0 + q1 + 4) >> 3, 0, maxv);
            dst[strideb * -1] = (stbv_u16)stb_av1_db_iclip(
                (p3 + p2 + p1 + p0*2 + q0 + q1 + q2 + 4) >> 3, 0, maxv);
            dst[strideb * +0] = (stbv_u16)stb_av1_db_iclip(
                (p2 + p1 + p0 + q0*2 + q1 + q2 + q3 + 4) >> 3, 0, maxv);
            dst[strideb * +1] = (stbv_u16)stb_av1_db_iclip(
                (p1 + p0 + q0 + q1*2 + q2 + q3*2 + 4) >> 3, 0, maxv);
            dst[strideb * +2] = (stbv_u16)stb_av1_db_iclip(
                (p0 + q0 + q1 + q2*2 + q3*3 + 4) >> 3, 0, maxv);
        } else if (wd == 6 && flat8in) {
            dst[strideb * -2] = (stbv_u16)stb_av1_db_iclip(
                (p2*3 + p1*2 + p0*2 + q0 + 4) >> 3, 0, maxv);
            dst[strideb * -1] = (stbv_u16)stb_av1_db_iclip(
                (p2 + p1*2 + p0*2 + q0*2 + q1 + 4) >> 3, 0, maxv);
            dst[strideb * +0] = (stbv_u16)stb_av1_db_iclip(
                (p1 + p0*2 + q0*2 + q1*2 + q2 + 4) >> 3, 0, maxv);
            dst[strideb * +1] = (stbv_u16)stb_av1_db_iclip(
                (p0 + q0*2 + q1*2 + q2*3 + 4) >> 3, 0, maxv);
        } else {
            /* hev branch */
            int hev = abs(p1 - p0) > H || abs(q1 - q0) > H;
#define STB_DB_ICLIP_DIFF(v) stb_av1_db_iclip((v), -128, 127)
            if (hev) {
                int f = STB_DB_ICLIP_DIFF(p1 - q1);
                int f1, f2;
                f = STB_DB_ICLIP_DIFF(3 * (q0 - p0) + f);
                f1 = ((f + 4) > 127 ? 127 : (f + 4)) >> 3;
                f2 = ((f + 3) > 127 ? 127 : (f + 3)) >> 3;
                dst[strideb * -1] = (stbv_u16)stb_av1_db_iclip(p0 + f2, 0, maxv);
                dst[strideb * +0] = (stbv_u16)stb_av1_db_iclip(q0 - f1, 0, maxv);
            } else {
                int f = STB_DB_ICLIP_DIFF(3 * (q0 - p0));
                int f1 = ((f + 4) > 127 ? 127 : (f + 4)) >> 3;
                int f2 = ((f + 3) > 127 ? 127 : (f + 3)) >> 3;
                dst[strideb * -1] = (stbv_u16)stb_av1_db_iclip(p0 + f2, 0, maxv);
                dst[strideb * +0] = (stbv_u16)stb_av1_db_iclip(q0 - f1, 0, maxv);
                f = (f1 + 1) >> 1;
                dst[strideb * -2] = (stbv_u16)stb_av1_db_iclip(p1 + f, 0, maxv);
                dst[strideb * +1] = (stbv_u16)stb_av1_db_iclip(q1 - f, 0, maxv);
            }
#undef STB_DB_ICLIP_DIFF
        }
    }
}

/*
 * Deblock one plane.
 *   p, stride   - 16-bit plane
 *   w, h        - visible extent in pixels
 *   level       - base LF level for this plane (0 disables)
 *   sharpness   - frame sharpness
 *   is_chroma   - caps filter width at 6
 *   blkid       - per-4x4-unit block-identity map (any stable id per block)
 *   txlw        - per-4x4-unit log2-width of the covering transform
 *   b4stride    - row stride of the maps (4x4 units)
 *   maph, mapw  - map extent in 4x4 units
 *   ssx, ssy    - plane subsampling relative to the maps' grid
 */
static void stb_avif_deblock_plane_u16(stbv_u16 *p, ptrdiff_t stride,
                                       int w, int h,
                                       int level_v, int level_h,
                                       int sharpness, int is_chroma,
                                       int maxv, int bd8,
                                       const stbv_u32 *blkid,
                                       const stbv_u8 *txlw,
                                       ptrdiff_t b4stride,
                                       int mapw4, int maph4,
                                       int ssx, int ssy,
                                       const unsigned int *tile_col_start_sb,
                                       int tile_cols,
                                       const unsigned int *tile_row_start_sb,
                                       int tile_rows,
                                       int sb_size)
{
    int e_lim, lut_i[64], lut_e[64];
    int L, x, y, X, Y;

    if (!level_v && !level_h) return;

    /* Loop filtering must not cross a tile boundary.  The reconstruction
     * maps are frame-wide, so a plain blkid comparison would otherwise
     * make every tile boundary look like an ordinary block edge. */

    /* dav1d_calc_eih */
    for (L = 0; L < 64; L++) {
        int limit = L;
        if (sharpness > 0) {
            limit >>= (sharpness + 3) >> 2;
            limit = limit < (9 - sharpness) ? limit : (9 - sharpness);
        }
        if (limit < 1) limit = 1;
        lut_i[L] = limit;
        lut_e[L] = 2 * (L + 2) + limit;
    }

    /* ---- vertical edges (at px X = multiples of 4) ---- */
    if (level_v) {
        for (X = 4; X < w; X += 4) {
            for (Y = 0; Y < h; Y += 4) {
                int bx_r = (X << ssx) >> 2;          /* unit col right of edge */
                int by_a = (Y << ssy) >> 2;          /* first unit row of band */
                int band_rows = 4 >> ssy;
                int edge = 0, bucket = 99;
                int r;
                if (band_rows < 1) band_rows = 1;
                for (r = 0; r < band_rows; r++) {
                    int yy = by_a + r;
                    int yl = yy < maph4 ? yy : maph4 - 1;
                    int xl = bx_r - 1 < mapw4 ? bx_r - 1 : mapw4 - 1;
                    int xr = bx_r < mapw4 ? bx_r : mapw4 - 1;
                    stbv_u32 bl = blkid[(size_t)yl * b4stride + xl];
                    stbv_u32 br = blkid[(size_t)yl * b4stride + xr];
                    int ll = txlw[(size_t)yl * b4stride + xl];
                    int lr = txlw[(size_t)yl * b4stride + xr];
                    if (bl != br || ll != lr) {
                        edge = 1;
                        if (ll < bucket) bucket = ll;
                        if (lr < bucket) bucket = lr;
                    }
                }
                if (!edge) continue;
                /* No deblock across a tile-column boundary.  X is in this
                 * plane's pixel coordinates; tile starts are in SB units. */
                if (tile_col_start_sb && tile_cols > 1) {
                    int tc;
                    for (tc = 1; tc < tile_cols; tc++) {
                        int tbx = (int)((tile_col_start_sb[tc] * (unsigned int)sb_size) >> ssx);
                        if (X == tbx) { edge = 0; break; }
                    }
                }
                if (!edge) continue;
                if (bucket > (is_chroma ? 1 : 2)) bucket = is_chroma ? 1 : 2;
                if (bucket < 0) bucket = 0;
                L = level_v;
                {
                    ptrdiff_t sb = 1;                 /* across = x */
                    ptrdiff_t sa = stride;            /* along  = y */
                    stbv_u16 *q0 = p + (size_t)Y * stride + X;
                    int wd = 4 << bucket;
                    if (is_chroma) wd = 4 + 2 * bucket;
                    if (wd >= 16 && (X < 7 || w - X < 6)) wd = 8;
                    if (wd >= 8 && (X < 4 || w - X < 3)) wd = is_chroma ? 6 : 4;
                    if (wd >= 6 && (X < 3 || w - X < 2)) wd = 4;
                    if (wd >= 4 && (X < 2 || w - X < 1)) continue;
                    stb_av1_loop_filter_edge(q0, sa, sb, lut_e[L], lut_i[L],
                                             L >> 4, wd, maxv, bd8);
                }
            }
        }
    }

    /* ---- horizontal edges (at px Y = multiples of 4) ---- */
    if (level_h) {
        for (Y = 4; Y < h; Y += 4) {
            for (X = 0; X < w; X += 4) {
                int by_r = (Y << ssy) >> 2;
                int bx_a = (X << ssx) >> 2;
                int band_cols = 4 >> ssx;
                int edge = 0, bucket = 99;
                int c;
                if (band_cols < 1) band_cols = 1;
                for (c = 0; c < band_cols; c++) {
                    int xx = bx_a + c;
                    int xt = xx < mapw4 ? xx : mapw4 - 1;
                    int yt = by_r - 1 < maph4 ? by_r - 1 : maph4 - 1;
                    int yb = by_r < maph4 ? by_r : maph4 - 1;
                    stbv_u32 bu = blkid[(size_t)yt * b4stride + xt];
                    stbv_u32 bd = blkid[(size_t)yb * b4stride + xt];
                    int lu = txlw[(size_t)yt * b4stride + xt];
                    int ld = txlw[(size_t)yb * b4stride + xt];
                    if (bu != bd || lu != ld) {
                        edge = 1;
                        if (lu < bucket) bucket = lu;
                        if (ld < bucket) bucket = ld;
                    }
                }
                if (!edge) continue;
                /* No deblock across a tile-row boundary. */
                if (tile_row_start_sb && tile_rows > 1) {
                    int tr;
                    for (tr = 1; tr < tile_rows; tr++) {
                        int tby = (int)((tile_row_start_sb[tr] * (unsigned int)sb_size) >> ssy);
                        if (Y == tby) { edge = 0; break; }
                    }
                }
                if (!edge) continue;
                if (bucket > (is_chroma ? 1 : 2)) bucket = is_chroma ? 1 : 2;
                if (bucket < 0) bucket = 0;
                L = level_h;
                {
                    ptrdiff_t sb = stride;
                    ptrdiff_t sa = 1;
                    stbv_u16 *q0 = p + (size_t)Y * stride + X;
                    int wd = 4 << bucket;
                    if (is_chroma) wd = 4 + 2 * bucket;
                    if (wd >= 16 && (Y < 7 || h - Y < 6)) wd = 8;
                    if (wd >= 8 && (Y < 4 || h - Y < 3)) wd = is_chroma ? 6 : 4;
                    if (wd >= 6 && (Y < 3 || h - Y < 2)) wd = 4;
                    if (wd >= 4 && (Y < 2 || h - Y < 1)) continue;
                    stb_av1_loop_filter_edge(q0, sa, sb, lut_e[L], lut_i[L],
                                             L >> 4, wd, maxv, bd8);
                }
            }
        }
    }
}

#endif /* STB_AV1_DEBLOCK_H */

/* ===== stb_av1_cdef.h ===== */
/*
 * stb_av1_cdef.h - scalar AV1 CDEF (Constrained Directional Enhancement Filter)
 *
 * Faithful port of dav1d's cdef_tmpl.c C filter. CDEF operates on 64x64
 * superblocks, filtering each 8x8 block within. It uses directional analysis
 * to find the dominant edge direction and applies constrained smoothing along
 * that direction.
 */
#ifndef STB_AV1_CDEF_H
#define STB_AV1_CDEF_H

#include <stddef.h>

/* Edge flags for CDEF padding */
enum {
    CDEF_HAVE_TOP    = 1,
    CDEF_HAVE_BOTTOM = 2,
    CDEF_HAVE_LEFT   = 4,
    CDEF_HAVE_RIGHT  = 8
};

/* Direction offsets into a 12-wide tmp buffer (matches dav1d table).
 * Indexed as [dir + offset][pass], where dir is 0-7 and offset wraps. */
static const stbv_i8 stb_av1_cdef_directions[12][2] = {
    {  1 * 12 + 0,  2 * 12 + 0 }, /* dir 6 */
    {  1 * 12 + 0,  2 * 12 - 1 }, /* dir 7 */
    { -1 * 12 + 1, -2 * 12 + 2 }, /* dir 0 */
    {  0 * 12 + 1, -1 * 12 + 2 }, /* dir 1 */
    {  0 * 12 + 1,  0 * 12 + 2 }, /* dir 2 */
    {  0 * 12 + 1,  1 * 12 + 2 }, /* dir 3 */
    {  1 * 12 + 1,  2 * 12 + 2 }, /* dir 4 */
    {  1 * 12 + 0,  2 * 12 + 1 }, /* dir 5 */
    {  1 * 12 + 0,  2 * 12 + 0 }, /* dir 6 (wrap) */
    {  1 * 12 + 0,  2 * 12 - 1 }, /* dir 7 (wrap) */
    { -1 * 12 + 1, -2 * 12 + 2 }, /* dir 0 (wrap) */
    {  0 * 12 + 1, -1 * 12 + 2 }  /* dir 1 (wrap) */
};

/* Normalization divisors for direction cost computation */
static const unsigned stb_av1_cdef_div_table[7] = {
    840, 420, 280, 210, 168, 140, 120
};

/* --- Helper functions --- */

static int stb_av1_cdef_ulog2(unsigned v)
{
    int r = 0;
    if (v >= 1u << 16) { v >>= 16; r += 16; }
    if (v >= 1u << 8)  { v >>= 8;  r += 8; }
    if (v >= 1u << 4)  { v >>= 4;  r += 4; }
    if (v >= 1u << 2)  { v >>= 2;  r += 2; }
    if (v >= 1u << 1)  { r += 1; }
    return r;
}

static int stb_av1_cdef_constrain(int diff, int threshold, int shift)
{
    int adiff = diff < 0 ? -diff : diff;
    int t;
    if (shift >= 0)
        t = threshold - (adiff >> shift);
    else
        t = threshold - (adiff << (-shift));
    if (t < 0) t = 0;
    if (t > adiff) t = adiff;
    return diff < 0 ? -t : t;
}

static int stb_av1_cdef_iclip(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* --- Direction finding --- */

/* Find the dominant edge direction in an 8x8 block.
 * img: pointer to top-left pixel, stride: row stride in pixels.
 * Returns direction (0-7) and sets *var to directional variance. */
static int stb_av1_cdef_find_dir(const stbv_u16 *img, int stride,
                                   unsigned *var, int bitdepth_min_8)
{
    int partial_sum_hv[2][8] = {{0},{0}};
    int partial_sum_diag[2][15] = {{0},{0}};
    int partial_sum_alt[4][11] = {{0},{0}};
    unsigned cost[8] = {0};
    int best_dir = 0;
    unsigned best_cost;
    int y, x, n, m;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int px = ((int)img[x] >> bitdepth_min_8) - 128;
            partial_sum_diag[0][y + x] += px;
            partial_sum_alt[0][y + (x >> 1)] += px;
            partial_sum_hv[0][y] += px;
            partial_sum_alt[1][3 + y - (x >> 1)] += px;
            partial_sum_diag[1][7 + y - x] += px;
            partial_sum_alt[2][3 - (y >> 1) + x] += px;
            partial_sum_hv[1][x] += px;
            partial_sum_alt[3][(y >> 1) + x] += px;
        }
        img += stride;
    }

    for (n = 0; n < 8; n++) {
        cost[2] += (unsigned)partial_sum_hv[0][n] * (unsigned)partial_sum_hv[0][n];
        cost[6] += (unsigned)partial_sum_hv[1][n] * (unsigned)partial_sum_hv[1][n];
    }
    cost[2] *= 105;
    cost[6] *= 105;

    for (n = 0; n < 7; n++) {
        unsigned d = stb_av1_cdef_div_table[n];
        cost[0] += ((unsigned)partial_sum_diag[0][n]      * (unsigned)partial_sum_diag[0][n] +
                    (unsigned)partial_sum_diag[0][14 - n] * (unsigned)partial_sum_diag[0][14 - n]) * d;
        cost[4] += ((unsigned)partial_sum_diag[1][n]      * (unsigned)partial_sum_diag[1][n] +
                    (unsigned)partial_sum_diag[1][14 - n] * (unsigned)partial_sum_diag[1][14 - n]) * d;
    }
    cost[0] += (unsigned)partial_sum_diag[0][7] * (unsigned)partial_sum_diag[0][7] * 105;
    cost[4] += (unsigned)partial_sum_diag[1][7] * (unsigned)partial_sum_diag[1][7] * 105;

    for (n = 0; n < 4; n++) {
        unsigned *cost_ptr = &cost[n * 2 + 1];
        for (m = 0; m < 5; m++)
            *cost_ptr += (unsigned)partial_sum_alt[n][3 + m] * (unsigned)partial_sum_alt[n][3 + m];
        *cost_ptr *= 105;
        for (m = 0; m < 3; m++) {
            unsigned d = stb_av1_cdef_div_table[2 * m + 1];
            *cost_ptr += ((unsigned)partial_sum_alt[n][m]      * (unsigned)partial_sum_alt[n][m] +
                          (unsigned)partial_sum_alt[n][10 - m] * (unsigned)partial_sum_alt[n][10 - m]) * d;
        }
    }

    best_cost = cost[0];
    for (n = 1; n < 8; n++) {
        if (cost[n] > best_cost) {
            best_cost = cost[n];
            best_dir = n;
        }
    }

    *var = (best_cost - (cost[best_dir ^ 4])) >> 10;
    return best_dir;
}

/* --- Strength adjustment --- */

static int stb_av1_cdef_adjust_strength(int strength, unsigned var)
{
    int i;
    if (!var) return 0;
    i = var >> 6 ? stb_av1_cdef_ulog2(var >> 6) : 0;
    if (i > 12) i = 12;
    return (strength * (4 + i) + 8) >> 4;
}

/* --- Filter kernel --- */

/* Filter a w*h block (w,h in {4,8}).
 * dst points to the block in the frame buffer.
 * dst_stride, frame_w, frame_h: frame geometry.
 * bx, by: block position in pixels within the frame.
 * edges: CDEF_HAVE_* flags. */
static void stb_av1_cdef_filter_block(stbv_u16 *dst, int dst_stride,
                                       int bx, int by,
                                       int frame_w, int frame_h,
                                       int pri_strength, int sec_strength,
                                       int dir, int damping,
                                       int w, int h, int edges,
                                       int bitdepth_min_8)
{
    const int tmp_stride = 12;
    stbv_i16 tmp_buf[144];
    stbv_i16 *tmp = tmp_buf + 2 * tmp_stride + 2;
    const stbv_i8 (*cdef_dirs)[2] = &stb_av1_cdef_directions[dir];
    int x, y, k;

    /* Fill tmp with the block + 2-pixel padding on each side. */
    for (y = -2; y < h + 2; y++) {
        for (x = -2; x < w + 2; x++) {
            int fx = bx + x, fy = by + y;
            if (fx >= 0 && fx < frame_w && fy >= 0 && fy < frame_h)
                tmp[y * tmp_stride + x] = (stbv_i16)dst[fy * dst_stride + fx];
            else
                tmp[y * tmp_stride + x] = -32768;
        }
    }

    /* Apply the directional constrained filter. */
    if (pri_strength) {
        int pri_tap = 4 - ((pri_strength >> bitdepth_min_8) & 1);
        int pri_shift = damping - stb_av1_cdef_ulog2((unsigned)pri_strength);
        if (pri_shift < 0) pri_shift = 0;

        if (sec_strength) {
            int sec_shift = damping - stb_av1_cdef_ulog2((unsigned)sec_strength);
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    int px = dst[(by + y) * dst_stride + (bx + x)];
                    int sum = 0;
                    int max = px, min = px;
                    int pri_tap_k = pri_tap;
                    for (k = 0; k < 2; k++) {
                        int off1 = cdef_dirs[2][k];
                        int p0 = tmp[(y + 2) * tmp_stride + (x + 2) + off1];
                        int p1 = tmp[(y + 2) * tmp_stride + (x + 2) - off1];
                        sum += pri_tap_k * stb_av1_cdef_constrain(p0 - px, pri_strength, pri_shift);
                        sum += pri_tap_k * stb_av1_cdef_constrain(p1 - px, pri_strength, pri_shift);
                        pri_tap_k = (pri_tap_k & 3) | 2;
                        if (p0 < min) min = p0;
                        if (p0 > max) max = p0;
                        if (p1 < min) min = p1;
                        if (p1 > max) max = p1;
                        {
                            int off2 = cdef_dirs[4][k];
                            int off3 = cdef_dirs[0][k];
                            int s0 = tmp[(y + 2) * tmp_stride + (x + 2) + off2];
                            int s1 = tmp[(y + 2) * tmp_stride + (x + 2) - off2];
                            int s2 = tmp[(y + 2) * tmp_stride + (x + 2) + off3];
                            int s3 = tmp[(y + 2) * tmp_stride + (x + 2) - off3];
                            int sec_tap = 2 - k;
                            sum += sec_tap * stb_av1_cdef_constrain(s0 - px, sec_strength, sec_shift);
                            sum += sec_tap * stb_av1_cdef_constrain(s1 - px, sec_strength, sec_shift);
                            sum += sec_tap * stb_av1_cdef_constrain(s2 - px, sec_strength, sec_shift);
                            sum += sec_tap * stb_av1_cdef_constrain(s3 - px, sec_strength, sec_shift);
                            if (s0 < min) min = s0;
                            if (s0 > max) max = s0;
                            if (s1 < min) min = s1;
                            if (s1 > max) max = s1;
                            if (s2 < min) min = s2;
                            if (s2 > max) max = s2;
                            if (s3 < min) min = s3;
                            if (s3 > max) max = s3;
                        }
                    }
                    dst[(by + y) * dst_stride + (bx + x)] =
                        (stbv_u16)stb_av1_cdef_iclip(
                            px + ((sum - (sum < 0) + 8) >> 4), min, max);
                }
            }
        } else {
            /* Primary only, no secondary. */
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    int px = dst[(by + y) * dst_stride + (bx + x)];
                    int sum = 0;
                    int pri_tap_k = pri_tap;
                    for (k = 0; k < 2; k++) {
                        int off = cdef_dirs[2][k];
                        int p0 = tmp[(y + 2) * tmp_stride + (x + 2) + off];
                        int p1 = tmp[(y + 2) * tmp_stride + (x + 2) - off];
                        sum += pri_tap_k * stb_av1_cdef_constrain(p0 - px, pri_strength, pri_shift);
                        sum += pri_tap_k * stb_av1_cdef_constrain(p1 - px, pri_strength, pri_shift);
                        pri_tap_k = (pri_tap_k & 3) | 2;
                    }
                    dst[(by + y) * dst_stride + (bx + x)] =
                        (stbv_u16)(px + ((sum - (sum < 0) + 8) >> 4));
                }
            }
        }
    } else if (sec_strength) {
        /* Secondary only, no primary. */
        int sec_shift = damping - stb_av1_cdef_ulog2((unsigned)sec_strength);
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                int px = dst[(by + y) * dst_stride + (bx + x)];
                int sum = 0;
                for (k = 0; k < 2; k++) {
                    int off1 = cdef_dirs[4][k];
                    int off2 = cdef_dirs[0][k];
                    int s0 = tmp[(y + 2) * tmp_stride + (x + 2) + off1];
                    int s1 = tmp[(y + 2) * tmp_stride + (x + 2) - off1];
                    int s2 = tmp[(y + 2) * tmp_stride + (x + 2) + off2];
                    int s3 = tmp[(y + 2) * tmp_stride + (x + 2) - off2];
                    int sec_tap = 2 - k;
                    sum += sec_tap * stb_av1_cdef_constrain(s0 - px, sec_strength, sec_shift);
                    sum += sec_tap * stb_av1_cdef_constrain(s1 - px, sec_strength, sec_shift);
                    sum += sec_tap * stb_av1_cdef_constrain(s2 - px, sec_strength, sec_shift);
                    sum += sec_tap * stb_av1_cdef_constrain(s3 - px, sec_strength, sec_shift);
                }
                dst[(by + y) * dst_stride + (bx + x)] =
                    (stbv_u16)(px + ((sum - (sum < 0) + 8) >> 4));
            }
        }
    }
}

/* --- Frame-level CDEF application --- */

/* Apply CDEF to the entire frame. cdef_idx_grid must be pre-filled
 * with per-64x64-block CDEF indices (-1 for skip, 0..3 for parameter set).
 * y_strength[i] = (y_pri[i] << 2) | y_sec[i], same for uv.
 * In practice we take the already-separated pri/sec from the frame header. */
static void stb_av1_cdef_frame(stbv_u16 *plane_y, stbv_u16 *plane_u,
                                 stbv_u16 *plane_v,
                                 int stride_y, int stride_u, int stride_v,
                                 int frame_w, int frame_h,
                                 int ss_hor, int ss_ver,
                                 int bit_depth,
                                 const int *cdef_idx_grid, int cdef_grid_stride,
                                 const int y_pri[8], const int y_sec[8],
                                 const int uv_pri[8], const int uv_sec[8],
                                 int cdef_damping)
{
    int bitdepth_min_8 = bit_depth - 8;
    int damping = cdef_damping + bitdepth_min_8;
    int sb64_cols = (frame_w + 63) / 64;
    int sb64_rows = (frame_h + 63) / 64;
    int sb64_x, sb64_y;

    if (!cdef_idx_grid || !plane_y) return;

    for (sb64_y = 0; sb64_y < sb64_rows; sb64_y++) {
        for (sb64_x = 0; sb64_x < sb64_cols; sb64_x++) {
            int cdef_idx = cdef_idx_grid[sb64_y * cdef_grid_stride + sb64_x];
            int bx, by;
            int y_pri_lvl, y_sec_lvl, uv_pri_lvl, uv_sec_lvl;
            int dir = 0;
            unsigned variance = 0;
            int edges;
            int bw, bh;

            if (cdef_idx < 0) continue;

            /* Skip if both strengths are zero. */
            if (!y_pri[cdef_idx] && !y_sec[cdef_idx] &&
                !uv_pri[cdef_idx] && !uv_sec[cdef_idx])
                continue;

            bx = sb64_x * 64;
            by = sb64_y * 64;
            bw = (bx + 64 <= frame_w) ? 64 : frame_w - bx;
            bh = (by + 64 <= frame_h) ? 64 : frame_h - by;

            edges = 0;
            if (sb64_y > 0) edges |= CDEF_HAVE_TOP;
            if (sb64_y < sb64_rows - 1) edges |= CDEF_HAVE_BOTTOM;
            if (sb64_x > 0) edges |= CDEF_HAVE_LEFT;
            if (sb64_x < sb64_cols - 1) edges |= CDEF_HAVE_RIGHT;

            /* Compute adjusted strengths. */
            y_pri_lvl = stb_av1_cdef_adjust_strength(
                (y_pri[cdef_idx] << 2) << bitdepth_min_8, variance);
            y_sec_lvl = y_sec[cdef_idx];
            y_sec_lvl += (y_sec_lvl == 3);
            y_sec_lvl <<= bitdepth_min_8;

            uv_pri_lvl = (uv_pri[cdef_idx] << 2) << bitdepth_min_8;
            uv_sec_lvl = uv_sec[cdef_idx];
            uv_sec_lvl += (uv_sec_lvl == 3);
            uv_sec_lvl <<= bitdepth_min_8;

            /* Find direction for luma (needed if y_pri_lvl > 0). */
            if (y_pri_lvl || uv_pri_lvl) {
                dir = stb_av1_cdef_find_dir(
                    &plane_y[by * stride_y + bx], stride_y,
                    &variance, bitdepth_min_8);
            }

            /* Re-adjust y_pri_lvl now that we have variance. */
            y_pri_lvl = stb_av1_cdef_adjust_strength(
                (y_pri[cdef_idx] << 2) << bitdepth_min_8, variance);

            /* Filter luma: process 8x8 blocks within the 64x64 SB. */
            if (y_pri_lvl || y_sec_lvl) {
                int lx, ly;
                for (ly = 0; ly < bh; ly += 8) {
                    for (lx = 0; lx < bw; lx += 8) {
                        int bbw = (lx + 8 <= bw) ? 8 : bw - lx;
                        int bbh = (ly + 8 <= bh) ? 8 : bh - ly;
                        int bedges = edges;
                        if (ly > 0) bedges |= CDEF_HAVE_TOP;
                        if (ly + 8 < bh) bedges |= CDEF_HAVE_BOTTOM;
                        if (lx > 0) bedges |= CDEF_HAVE_LEFT;
                        if (lx + 8 < bw) bedges |= CDEF_HAVE_RIGHT;
                        stb_av1_cdef_filter_block(
                            plane_y, stride_y, bx + lx, by + ly,
                            frame_w, frame_h,
                            y_pri_lvl, y_sec_lvl,
                            dir, damping, bbw, bbh, bedges,
                            bitdepth_min_8);
                    }
                }
            }

            /* Filter chroma. */
            if (uv_pri_lvl || uv_sec_lvl) {
                static const stbv_u8 uv_dir_map[8] = {
                    0, 1, 2, 3, 4, 5, 6, 7
                };
                int uvdir = uv_pri_lvl ? uv_dir_map[dir] : 0;
                int cw = (frame_w + ss_hor) >> ss_hor;
                int ch = (frame_h + ss_ver) >> ss_ver;
                int cbx = bx >> ss_hor;
                int cby = by >> ss_ver;
                int cbw = bw >> ss_hor;
                int cbh = bh >> ss_ver;
                int pl;

                if (cbw < 1) cbw = 1;
                if (cbh < 1) cbh = 1;

                for (pl = 1; pl <= 2; pl++) {
                    stbv_u16 *plane = (pl == 1) ? plane_u : plane_v;
                    int stride = (pl == 1) ? stride_u : stride_v;
                    int clx, cly;
                    int uv_pri_adj = uv_pri_lvl;
                    int uv_sec_adj = uv_sec_lvl;
                    int bedges_c = edges;

                    /* For chroma, adjust damping by -1 per dav1d. */
                    int cdef_damping_c = damping - 1;
                    if (cdef_damping_c < 0) cdef_damping_c = 0;

                    if (!plane) continue;

                    for (cly = 0; cly < cbh; cly += 8) {
                        for (clx = 0; clx < cbw; clx += 8) {
                            int bbw = (clx + 8 <= cbw) ? 8 : cbw - clx;
                            int bbh = (cly + 8 <= cbh) ? 8 : cbh - cly;
                            int cbedges = bedges_c;
                            if (cly > 0) cbedges |= CDEF_HAVE_TOP;
                            if (cly + 8 < cbh) cbedges |= CDEF_HAVE_BOTTOM;
                            if (clx > 0) cbedges |= CDEF_HAVE_LEFT;
                            if (clx + 8 < cbw) cbedges |= CDEF_HAVE_RIGHT;
                            stb_av1_cdef_filter_block(
                                plane, stride, cbx + clx, cby + cly,
                                cw, ch,
                                uv_pri_adj, uv_sec_adj,
                                uvdir, cdef_damping_c, bbw, bbh, cbedges,
                                bitdepth_min_8);
                        }
                    }
                }
            }
        }
    }
}

#endif /* STB_AV1_CDEF_H */

/* ----------- CONFIGURATION ----------- */

#ifndef STB_AVIF_MAX_DIMENSION
#define STB_AVIF_MAX_DIMENSION 16384
#endif

#ifndef STB_AVIF_MAX_TILE_WIDTH
#define STB_AVIF_MAX_TILE_WIDTH 4096
#endif

#ifndef STB_AVIF_MAX_TILE_HEIGHT
#define STB_AVIF_MAX_TILE_HEIGHT 4096
#endif

/* ----------- C89 COMPATIBILITY HELPERS ----------- */

/* We avoid stdint.h for strict C89 compatibility.
   Define our own fixed-size types (guarded if scalar headers already included). */
#ifndef STB_AV1_SCALAR_H
typedef unsigned char  stbv_u8;
typedef signed char    stbv_s8;
typedef unsigned short stbv_u16;
typedef signed short   stbv_s16;
typedef unsigned int   stbv_u32;
typedef signed int     stbv_s32;
#ifndef STBV_I32_DEFINED
#define STBV_I32_DEFINED
typedef signed int     stbv_i32;
#endif
#endif
#ifndef STB_AV1_SCALAR_H
/* The AV1 decoder needs native 64-bit arithmetic for MSAC and bit reading.
   C89 has no standard 64-bit integer type, so use the compiler extensions
   available on the supported C89-era toolchains. */
#ifndef STB_AVIF_NO_64BIT
  #if defined(_MSC_VER)
    typedef unsigned __int64 stbv_u64;
    typedef __int64          stbv_s64;
  #else
    typedef unsigned long long stbv_u64;
    typedef long long          stbv_s64;
  #endif
#else
  #error "stb_avif requires 64-bit integer support"
#endif
#endif

/* ----------- ERROR HANDLING ----------- */

static const char *stb_avif_error_msg = "no error";
static jmp_buf stb_avif_jmp;

#define STB_AVIF_ERROR(msg) do { stb_avif_error_msg = msg; longjmp(stb_avif_jmp, 1); } while (0)
#define STB_AVIF_CHECK(cond, msg) do { if (!(cond)) STB_AVIF_ERROR(msg); } while (0)

/* ----------- MEMORY ALLOCATION ----------- */

static void *stb_avif_malloc(size_t size)
{
    return malloc(size);
}

static void *stb_avif_calloc(size_t count, size_t size)
{
    void *p;
    size_t total = count * size;
    p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

static void stb_avif_free_internal(void *ptr)
{
    free(ptr);
}


/* ===== stb_av1_lr.h ===== */
/*
 * stb_av1_lr.h - scalar AV1 loop restoration (Wiener + SGR projection)
 *
 * Faithful scalar-C port of dav1d's looprestoration_tmpl.c.
 * Operates on unsigned short planes (same as the rest of the decoder pipeline).
 */
#ifndef STB_AV1_LR_H
#define STB_AV1_LR_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- LR unit storage ---- */
/* stbv_av1_lr_unit and stbv_av1_lr_mask are defined in stb_av1_tile_decode.h */

/* Allocate LR mask for given frame dimensions and restoration params.
 * Returns 0 on success, -1 on error. */
static int stbv_av1_lr_mask_alloc(stbv_av1_lr_mask *m,
                                  int frame_w, int frame_h,
                                  const int unit_size_log2[2],
                                  int ss_hor, int ss_ver)
{
    int p;
    memset(m, 0, sizeof(*m));
    m->unit_size_log2[0] = unit_size_log2[0];
    m->unit_size_log2[1] = unit_size_log2[1];
    for (p = 0; p < 3; p++) {
        int chroma = p > 0;
        int ss_h = chroma ? ss_hor : 0;
        int ss_v = chroma ? ss_ver : 0;
        int w = (frame_w + ss_h) >> ss_h;
        int h = (frame_h + ss_v) >> ss_v;
        int usz = unit_size_log2[chroma ? 1 : 0];
        int unit_sz = 1 << usz;
        m->grid_stride[p] = (w + unit_sz - 1) / unit_sz;
        m->grid_rows[p] = (h + unit_sz - 1) / unit_sz;
        m->units[p] = (stbv_av1_lr_unit *)stb_avif_calloc(
            (size_t)m->grid_stride[p] * m->grid_rows[p],
            sizeof(stbv_av1_lr_unit));
        if (!m->units[p]) {
            int q;
            for (q = 0; q < p; q++) stb_avif_free_internal(m->units[q]);
            memset(m, 0, sizeof(*m));
            return -1;
        }
    }
    return 0;
}

static void stbv_av1_lr_mask_free(stbv_av1_lr_mask *m)
{
    int p;
    if (!m) return;
    for (p = 0; p < 3; p++) {
        if (m->units[p]) stb_avif_free_internal(m->units[p]);
    }
    memset(m, 0, sizeof(*m));
}

/* Store decoded LR unit params into the mask.
 * x, y are in LR-unit coordinates for the given plane. */
static void stbv_av1_lr_mask_store(stbv_av1_lr_mask *m, int plane,
                                   int lr_x, int lr_y,
                                   const stbv_av1_lr_ref *ref, int type)
{
    stbv_av1_lr_unit *u;
    if (!m || plane < 0 || plane > 2) return;
    if (lr_x < 0 || lr_x >= m->grid_stride[plane]) return;
    if (lr_y < 0 || lr_y >= m->grid_rows[plane]) return;
    u = &m->units[plane][lr_y * m->grid_stride[plane] + lr_x];
    u->type = (unsigned char)type;
    u->filter_h[0] = (signed char)ref->filter_h[0];
    u->filter_h[1] = (signed char)ref->filter_h[1];
    u->filter_h[2] = (signed char)ref->filter_h[2];
    u->filter_v[0] = (signed char)ref->filter_v[0];
    u->filter_v[1] = (signed char)ref->filter_v[1];
    u->filter_v[2] = (signed char)ref->filter_v[2];
    u->sgr_weights[0] = (signed char)ref->sgr_weights[0];
    u->sgr_weights[1] = (signed char)ref->sgr_weights[1];
    u->sgr_idx = 0; /* will be set from the tile decode */
}

/* ---- Pixel clip helper ---- */

static unsigned short stbv_av1_lr_clip16(int v, int maxv)
{
    return (unsigned short)(v < 0 ? 0 : v > maxv ? maxv : v);
}

/* ---- Wiener filter ---- */

#define STBV_LR_REST_UNIT_STRIDE 390

static void stbv_av1_wiener_filter_h(unsigned short *dst, const unsigned short *src,
                                     int src_stride, int w,
                                     const signed short *fh, int bit_depth)
{
    const int round_bits_h = 3 + (bit_depth == 12 ? 2 : 0);
    const int round_off_h = 1 << (round_bits_h - 1);
    const int round_offset = 1 << (bit_depth + 6);
    const int clip_limit = 1 << (bit_depth + 1 + 7 - round_bits_h);
    int x;
    for (x = 0; x < w; x++) {
        int sum = round_offset;
        int i;
        for (i = 0; i < 7; i++) {
            int idx = x + i - 3;
            int px;
            if (idx < 0) px = src[0];
            else if (idx >= w) px = src[w - 1];
            else px = src[idx];
            sum += px * fh[i];
        }
        dst[x] = (unsigned short)((sum + round_off_h) >> round_bits_h);
        if (dst[x] > (unsigned short)(clip_limit - 1))
            dst[x] = (unsigned short)(clip_limit - 1);
    }
}

static void stbv_av1_wiener_filter_v(unsigned short *p, const unsigned short *const *ptrs,
                                     const signed short *fv, int w, int bit_depth)
{
    const int round_bits_v = 11 - (bit_depth == 12 ? 2 : 0);
    const int round_off_v = 1 << (round_bits_v - 1);
    const int round_offset = 1 << (bit_depth + (round_bits_v - 1));
    const int maxv = (1 << bit_depth) - 1;
    int i;
    for (i = 0; i < w; i++) {
        int sum = -round_offset;
        int k;
        for (k = 0; k < 6; k++)
            sum += ptrs[k][i] * fv[k];
        sum += ptrs[6][i] * fv[6];
        p[i] = stbv_av1_lr_clip16((sum + round_off_v) >> round_bits_v, maxv);
    }
}

/* Apply Wiener filter to a rectangular region of a plane.
 * src points to the top-left of the LR unit; the region is [0..w) x [0..h)
 * within the full plane (stride = full frame stride).
 * Edge padding: clamp at frame boundaries. */
static void stbv_av1_wiener_plane(unsigned short *plane, int stride,
                                  int frame_w, int frame_h,
                                  int ux0, int uy0, int uw, int uh,
                                  const signed char *raw_fv, const signed char *raw_fh,
                                  int bit_depth)
{
    int maxv = (1 << bit_depth) - 1;
    unsigned short *tmp_buf;
    unsigned short *tmp_rows[7];
    signed short fh[7], fv[7];
    int y, i;
    int ew = uw + 6;

    if (uw <= 0 || uh <= 0) return;

    /* Build symmetric 7-tap filter from 3 parameters */
    fh[0] = fh[6] = raw_fh[0];
    fh[1] = fh[5] = raw_fh[1];
    fh[2] = fh[4] = raw_fh[2];
    fh[3] = (signed short)(-(fh[0] + fh[1] + fh[2]) * 2 + 128);
    fv[0] = fv[6] = raw_fv[0];
    fv[1] = fv[5] = raw_fv[1];
    fv[2] = fv[4] = raw_fv[2];
    fv[3] = (signed short)(128 - (fv[0] + fv[1] + fv[2]) * 2);

    tmp_buf = (unsigned short *)stb_avif_calloc((size_t)ew * 7, sizeof(unsigned short));
    if (!tmp_buf) return;
    for (i = 0; i < 7; i++)
        tmp_rows[i] = tmp_buf + i * ew;

    /* Initialize ring buffer: replicate clamped rows.
     * The horizontal source starts 3 pixels before the LR unit (ux0-3)
     * so the 7-tap filter centered at position x reads src[x-3..x+3]. */
    for (i = 0; i < 6; i++) {
        int src_y = uy0 + i - 3;
        int src_x0 = ux0 >= 3 ? ux0 - 3 : 0;
        if (src_y < 0) src_y = 0;
        if (src_y >= frame_h) src_y = frame_h - 1;
        stbv_av1_wiener_filter_h(tmp_rows[i], plane + src_y * stride + src_x0,
                                 stride, ew, fh, bit_depth);
    }

    /* Process uh rows of output.
     * The horizontal filter produces ew = uw + 6 elements starting from
     * ux0-3 (or 0). Positions 3..3+uw-1 are the valid LR unit pixels.
     * The vertical pass must only write uw elements to avoid corrupting
     * adjacent LR units. We offset the read pointers by +3 to skip the
     * left padding. */
    for (y = 0; y < uh; y++) {
        int src_y = uy0 + y + 3;
        int src_x0 = ux0 >= 3 ? ux0 - 3 : 0;
        unsigned short *row_dst;
        const unsigned short *vptrs[7];

        if (src_y >= frame_h) src_y = frame_h - 1;
        stbv_av1_wiener_filter_h(tmp_rows[(y + 6) % 7],
                                 plane + src_y * stride + src_x0,
                                 stride, ew, fh, bit_depth);

        for (i = 0; i < 7; i++)
            vptrs[i] = tmp_rows[(y + i) % 7] + 3;

        row_dst = plane + (uy0 + y) * stride + ux0;
        stbv_av1_wiener_filter_v(row_dst, vptrs, fv, uw, bit_depth);
    }

    stb_avif_free_internal(tmp_buf);
}

/* ---- SGR projection filter ---- */

static const unsigned short stbv_av1_sgr_tab[16][2] = {
    { 140, 3236 }, { 112, 2158 }, {  93, 1618 }, {  80, 1438 },
    {  70, 1295 }, {  58, 1177 }, {  47, 1079 }, {  37,  996 },
    {  30,  925 }, {  25,  863 }, {   0, 2589 }, {   0, 1618 },
    {   0, 1177 }, {   0,  925 }, {  56,    0 }, {  22,    0 },
};

static const unsigned char stbv_av1_sgr_x_by_x[256] = {
    255, 128,  85,  64,  51,  43,  37,  32,  28,  26,  23,  21,  20,  18,  17,
     16,  15,  14,  13,  13,  12,  12,  11,  11,  10,  10,   9,   9,   9,   9,
      8,   8,   8,   8,   7,   7,   7,   7,   7,   6,   6,   6,   6,   6,   6,
      6,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   4,   4,   4,   4,
      4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   3,   3,
      3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
      3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   2,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      2,   2,   2,   2,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      0
};

/* Box3 horizontal: sum and sumsq for 3-wide box at each x position */
static void stbv_av1_sgr_box3_row_h(int *sumsq, int *sum,
                                    const unsigned short *src, int w)
{
    /* x ranges from -1 to w inclusive; indices into sum/sumsq are offset by +1 */
    int a, b, c, x;
    sumsq++; sum++;
    a = src[0]; b = src[0];
    for (x = -1; x <= w; x++) {
        int px = x + 1;
        c = (px < w) ? src[px] : src[w - 1];
        sum[x] = a + b + c;
        sumsq[x] = a * a + b * b + c * c;
        a = b;
        b = c;
    }
}

/* Box5 horizontal: sum and sumsq for 5-wide box at each x position */
static void stbv_av1_sgr_box5_row_h(int *sumsq, int *sum,
                                    const unsigned short *src, int w)
{
    int a, b, c, d, x;
    sumsq++; sum++;
    a = src[0]; b = src[0]; c = src[0]; d = src[0];
    for (x = -1; x <= w; x++) {
        int px = x + 2;
        int e = (px < w) ? src[px] : src[w - 1];
        sum[x] = a + b + c + d + e;
        sumsq[x] = a*a + b*b + c*c + d*d + e*e;
        a = b; b = c; c = d; d = e;
    }
}

/* Vertical accumulation for box3 */
static void stbv_av1_sgr_box3_row_v(const int *const *sumsq_h,
                                    const int *const *sum_h,
                                    int *sumsq_out, int *sum_out, int w)
{
    int x;
    for (x = 0; x < w + 2; x++) {
        sumsq_out[x] = sumsq_h[0][x] + sumsq_h[1][x] + sumsq_h[2][x];
        sum_out[x] = sum_h[0][x] + sum_h[1][x] + sum_h[2][x];
    }
}

/* Vertical accumulation for box5 */
static void stbv_av1_sgr_box5_row_v(const int *const *sumsq_h,
                                    const int *const *sum_h,
                                    int *sumsq_out, int *sum_out, int w)
{
    int x;
    for (x = 0; x < w + 2; x++) {
        sumsq_out[x] = sumsq_h[0][x]+sumsq_h[1][x]+sumsq_h[2][x]
                       +sumsq_h[3][x]+sumsq_h[4][x];
        sum_out[x] = sum_h[0][x]+sum_h[1][x]+sum_h[2][x]
                     +sum_h[3][x]+sum_h[4][x];
    }
}

/* Compute A (inverse variance) and B (filtered value) per pixel */
static void stbv_av1_sgr_calc_ab(int *AA, int *BB, int w, int s,
                                 int n, int one_by_x)
{
    int i;
    for (i = 0; i < w + 2; i++) {
        int a = AA[i];
        int b = BB[i];
        unsigned int p = (unsigned int)(a * n - b * b);
        unsigned int z, x;
        if ((int)p < 0) p = 0;
        z = (p * (unsigned int)s + (1u << 19)) >> 20;
        if (z > 255) z = 255;
        x = stbv_av1_sgr_x_by_x[z];
        AA[i] = (int)((x * (unsigned int)b * (unsigned int)one_by_x + (1u << 11)) >> 12);
        BB[i] = (int)x;
    }
}

/* Rotate pointers: discard oldest, shift down */
static void stbv_av1_rotate3(int **ptrs)
{
    int *tmp = ptrs[0];
    ptrs[0] = ptrs[1];
    ptrs[1] = ptrs[2];
    ptrs[2] = tmp;
}

static void stbv_av1_rotate2(int **ptrs)
{
    int *tmp = ptrs[0];
    ptrs[0] = ptrs[1];
    ptrs[1] = tmp;
}

static void stbv_av1_rotate5(int **ptrs)
{
    int *tmp = ptrs[0];
    ptrs[0] = ptrs[2];
    ptrs[2] = ptrs[4];
    ptrs[4] = tmp;
    tmp = ptrs[1];
    ptrs[1] = ptrs[3];
    ptrs[3] = tmp;
}

/* Finish filter row for 3x3 SGR: 8-neighbor weighted sum */
static void stbv_av1_sgr_finish_filter_row1(signed short *tmp,
                                            const unsigned short *src,
                                            const int *const *A_ptrs,
                                            const int *const *B_ptrs,
                                            int w)
{
    int i;
    for (i = 0; i < w; i++) {
        int a = (B_ptrs[1][i+1]+B_ptrs[1][i]+B_ptrs[1][i+2]
                +B_ptrs[0][i+1]+B_ptrs[2][i+1]) * 4
               +(B_ptrs[0][i]+B_ptrs[2][i]+B_ptrs[0][i+2]+B_ptrs[2][i+2]) * 3;
        int b = (A_ptrs[1][i+1]+A_ptrs[1][i]+A_ptrs[1][i+2]
                +A_ptrs[0][i+1]+A_ptrs[2][i+1]) * 4
               +(A_ptrs[0][i]+A_ptrs[2][i]+A_ptrs[0][i+2]+A_ptrs[2][i+2]) * 3;
        tmp[i] = (signed short)((b - a * src[i] + (1 << 8)) >> 9);
    }
}

/* Finish filter row for 5x5 SGR: 6-neighbor weighted sum (2 rows at once) */
static void stbv_av1_sgr_finish_filter_row2(signed short *tmp,
                                            const unsigned short *src, int src_stride,
                                            const int *const *A_ptrs,
                                            const int *const *B_ptrs,
                                            int w, int h)
{
    int i;
    /* First row: full 6-neighbor */
    for (i = 0; i < w; i++) {
        int a = (B_ptrs[0][i+1]+B_ptrs[1][i+1])*6
               +(B_ptrs[0][i]+B_ptrs[1][i]+B_ptrs[0][i+2]+B_ptrs[1][i+2])*5;
        int b = (A_ptrs[0][i+1]+A_ptrs[1][i+1])*6
               +(A_ptrs[0][i]+A_ptrs[1][i]+A_ptrs[0][i+2]+A_ptrs[1][i+2])*5;
        tmp[i] = (signed short)((b - a * src[i] + (1 << 8)) >> 9);
    }
    if (h <= 1) return;
    /* Second row: simplified (using current A/B only) */
    tmp += 384;
    src += src_stride;
    for (i = 0; i < w; i++) {
        int B = B_ptrs[1][i+1], A = A_ptrs[1][i+1];
        int a = B*6 + (B_ptrs[1][i]+B_ptrs[1][i+2])*5;
        int b = A*6 + (A_ptrs[1][i]+A_ptrs[1][i+2])*5;
        tmp[i] = (signed short)((b - a * src[i] + (1 << 7)) >> 8);
    }
}

/* Apply weight for 3x3 SGR */
static void stbv_av1_sgr_weighted_row1(unsigned short *dst, const signed short *t1,
                                       int w, int w1)
{
    int i;
    for (i = 0; i < w; i++) {
        int v = w1 * t1[i];
        int r = dst[i] + ((v + (1 << 10)) >> 11);
        dst[i] = stbv_av1_lr_clip16(r, 255);
    }
}

/* Apply dual weights for mix SGR */
static void stbv_av1_sgr_weighted2(unsigned short *dst, int dst_stride,
                                   const signed short *t1, const signed short *t2,
                                   int w, int h, int w0, int w1)
{
    int j;
    for (j = 0; j < h; j++) {
        int i;
        for (i = 0; i < w; i++) {
            int v = w0 * t1[i] + w1 * t2[i];
            int r = dst[i] + ((v + (1 << 10)) >> 11);
            dst[i] = stbv_av1_lr_clip16(r, 255);
        }
        dst += dst_stride;
        t1 += 384;
        t2 += 384;
    }
}

/* ---- SGR 3x3 filter ---- */
static void stbv_av1_sgr_3x3(unsigned short *dst, int stride,
                              int frame_w, int frame_h,
                              int ux0, int uy0, int uw, int uh,
                              int s1, int w1)
{
    /* Allocate work buffers */
    int BUF = 384 + 16;
    int *sumsq_buf, *sum_buf;
    int *A_buf, *B_buf;
    int *sumsq_rows[3], *sum_rows[3];
    int *A_ptrs[3], *B_ptrs[3];
    int *sumsq_ptrs[3], *sum_ptrs[3];
    int y, i;
    int ex0 = ux0 - 1, ey0 = uy0 - 1;
    int ew = uw + 2, eh = uh + 2;
    int ey0_clamped, ey1_clamped;
    const unsigned short *src_row;
    signed short tmp[384];

    if (ew <= 0 || eh <= 0 || uw <= 0 || uh <= 0) return;

    sumsq_buf = (int*)stb_avif_calloc((size_t)BUF * 3, sizeof(int));
    sum_buf = (int*)stb_avif_calloc((size_t)BUF * 3, sizeof(int));
    A_buf = (int*)stb_avif_calloc((size_t)BUF * 3, sizeof(int));
    B_buf = (int*)stb_avif_calloc((size_t)BUF * 3, sizeof(int));
    if (!sumsq_buf || !sum_buf || !A_buf || !B_buf) {
        if (sumsq_buf) stb_avif_free_internal(sumsq_buf);
        if (sum_buf) stb_avif_free_internal(sum_buf);
        if (A_buf) stb_avif_free_internal(A_buf);
        if (B_buf) stb_avif_free_internal(B_buf);
        return;
    }

    for (i = 0; i < 3; i++) {
        sumsq_rows[i] = sumsq_buf + i * BUF;
        sum_rows[i] = sum_buf + i * BUF;
        sumsq_ptrs[i] = sumsq_rows[i];
        sum_ptrs[i] = sum_rows[i];
        A_ptrs[i] = A_buf + i * BUF;
        B_ptrs[i] = B_buf + i * BUF;
    }

    /* Initialize: replicate top row for rows before the LR unit */
    ey0_clamped = ey0 < 0 ? 0 : ey0;
    {
        const unsigned short *r0 = dst + ey0_clamped * stride + ex0;
        stbv_av1_sgr_box3_row_h(sumsq_ptrs[0], sum_ptrs[0], r0, ew);
    }

    /* Process eh rows */
    for (y = 0; y < eh; y++) {
        int row = (ey0 + y);
        int row_clamped;
        const unsigned short *src_ptr;
        int next_row;

        if (row < 0) row_clamped = 0;
        else if (row >= frame_h) row_clamped = frame_h - 1;
        else row_clamped = row;

        src_ptr = dst + row_clamped * stride + ex0;

        stbv_av1_sgr_box3_row_h(sumsq_ptrs[2], sum_ptrs[2], src_ptr, ew);
        stbv_av1_sgr_box3_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[2], B_ptrs[2], uw);
        stbv_av1_sgr_calc_ab(A_ptrs[2], B_ptrs[2], uw, s1, 9, 455);
        stbv_av1_rotate3(sumsq_ptrs);
        stbv_av1_rotate3(sum_ptrs);
        stbv_av1_rotate3(A_ptrs);
        stbv_av1_rotate3(B_ptrs);

        /* If we have 3+ rows accumulated, produce output */
        if (y >= 2) {
            int out_y = uy0 + (y - 2);
            if (out_y >= uy0 && out_y < uy0 + uh) {
                unsigned short *dst_row = dst + out_y * stride + ux0;
                stbv_av1_sgr_finish_filter_row1(tmp, dst_row,
                                                (const int *const *)A_ptrs,
                                                (const int *const *)B_ptrs,
                                                uw);
                stbv_av1_sgr_weighted_row1(dst_row, tmp, uw, w1);
            }
        }
    }

    /* Pad remaining rows */
    for (i = 0; i < 2; i++) {
        int out_y = uy0 + uh - 2 + i;
        if (out_y >= uy0 && out_y < uy0 + uh) {
            unsigned short *dst_row = dst + out_y * stride + ux0;
        stbv_av1_sgr_box3_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[2], B_ptrs[2], uw);
            stbv_av1_sgr_calc_ab(A_ptrs[2], B_ptrs[2], uw, s1, 9, 455);
            stbv_av1_rotate3(sumsq_ptrs);
            stbv_av1_rotate3(sum_ptrs);
            stbv_av1_rotate3(A_ptrs);
            stbv_av1_rotate3(B_ptrs);
            stbv_av1_sgr_finish_filter_row1(tmp, dst_row,
                                            (const int *const *)A_ptrs,
                                            (const int *const *)B_ptrs,
                                            uw);
            stbv_av1_sgr_weighted_row1(dst_row, tmp, uw, w1);
        }
    }

    stb_avif_free_internal(sumsq_buf);
    stb_avif_free_internal(sum_buf);
    stb_avif_free_internal(A_buf);
    stb_avif_free_internal(B_buf);
}

/* ---- SGR 5x5 filter ---- */
static void stbv_av1_sgr_5x5(unsigned short *dst, int stride,
                              int frame_w, int frame_h,
                              int ux0, int uy0, int uw, int uh,
                              int s0, int w0)
{
    int BUF = 384 + 16;
    int *sumsq_buf, *sum_buf;
    int *A_buf, *B_buf;
    int *sumsq_rows[5], *sum_rows[5];
    int *sumsq_ptrs[5], *sum_ptrs[5];
    int *A_ptrs[2], *B_ptrs[2];
    int y, i;
    int ex0 = ux0 - 2, ew = uw + 4;
    int ey0 = uy0 - 2, eh = uh + 4;
    int ey0_clamped;
    signed short tmp[768]; /* 2 * 384 */

    if (ew <= 0 || eh <= 0 || uw <= 0 || uh <= 0) return;

    sumsq_buf = (int*)stb_avif_calloc((size_t)BUF * 5, sizeof(int));
    sum_buf = (int*)stb_avif_calloc((size_t)BUF * 5, sizeof(int));
    A_buf = (int*)stb_avif_calloc((size_t)BUF * 2, sizeof(int));
    B_buf = (int*)stb_avif_calloc((size_t)BUF * 2, sizeof(int));
    if (!sumsq_buf || !sum_buf || !A_buf || !B_buf) {
        if (sumsq_buf) stb_avif_free_internal(sumsq_buf);
        if (sum_buf) stb_avif_free_internal(sum_buf);
        if (A_buf) stb_avif_free_internal(A_buf);
        if (B_buf) stb_avif_free_internal(B_buf);
        return;
    }

    for (i = 0; i < 5; i++) {
        sumsq_rows[i] = sumsq_buf + i * BUF;
        sum_rows[i] = sum_buf + i * BUF;
        sumsq_ptrs[i] = sumsq_rows[i];
        sum_ptrs[i] = sum_rows[i];
    }
    for (i = 0; i < 2; i++) {
        A_ptrs[i] = A_buf + i * BUF;
        B_ptrs[i] = B_buf + i * BUF;
    }

    ey0_clamped = ey0 < 0 ? 0 : ey0;
    {
        const unsigned short *r0 = dst + ey0_clamped * stride + ex0;
        stbv_av1_sgr_box5_row_h(sumsq_ptrs[0], sum_ptrs[0], r0, ew);
        stbv_av1_sgr_box5_row_h(sumsq_ptrs[1], sum_ptrs[1], r0, ew);
    }

    for (y = 0; y < eh; y++) {
        int row = ey0 + y;
        int row_clamped;
        const unsigned short *src_ptr;

        if (row < 0) row_clamped = 0;
        else if (row >= frame_h) row_clamped = frame_h - 1;
        else row_clamped = row;

        src_ptr = dst + row_clamped * stride + ex0;

        stbv_av1_sgr_box5_row_h(sumsq_ptrs[3], sum_ptrs[3], src_ptr, ew);

        /* Vertical accumulation when we have 5 rows */
        stbv_av1_sgr_box5_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[1], B_ptrs[1], uw);
        stbv_av1_sgr_calc_ab(A_ptrs[1], B_ptrs[1], uw, s0, 25, 164);
        stbv_av1_rotate5(sumsq_ptrs);
        stbv_av1_rotate5(sum_ptrs);
        stbv_av1_rotate2(A_ptrs);
        stbv_av1_rotate2(B_ptrs);

        /* Output: when we have 3+ valid rows, produce 1-2 output rows */
        if (y >= 3) {
            int out_y = uy0 + (y - 3);
            int out_h = (y < eh - 1) ? 2 : 1;
            unsigned short *dst_row;
            if (out_y + out_h > uy0 + uh) out_h = uy0 + uh - out_y;
            if (out_h > 0 && out_y >= uy0) {
                dst_row = dst + out_y * stride + ux0;
                stbv_av1_sgr_finish_filter_row2(tmp, dst_row, stride,
                                                (const int *const *)A_ptrs,
                                                (const int *const *)B_ptrs,
                                                uw, out_h);
                stbv_av1_sgr_weighted_row1(dst_row, tmp, uw, w0);
                if (out_h > 1)
                    stbv_av1_sgr_weighted_row1(dst_row + stride, tmp + 384, uw, w0);
            }
        }
    }

    stb_avif_free_internal(sumsq_buf);
    stb_avif_free_internal(sum_buf);
    stb_avif_free_internal(A_buf);
    stb_avif_free_internal(B_buf);
}

/* ---- SGR mix (5x5 + 3x3) filter ---- */
static void stbv_av1_sgr_mix(unsigned short *dst, int stride,
                              int frame_w, int frame_h,
                              int ux0, int uy0, int uw, int uh,
                              int s0, int s1, int w0, int w1)
{
    /* Simplified mix: apply 5x5 first, then 3x3 on the result */
    stbv_av1_sgr_5x5(dst, stride, frame_w, frame_h,
                      ux0, uy0, uw, uh, s0, w0);
    stbv_av1_sgr_3x3(dst, stride, frame_w, frame_h,
                      ux0, uy0, uw, uh, s1, w1);
}

/* ---- Frame-level LR application ---- */

/* Apply loop restoration to the entire frame.
 * Called after CDEF, before 8-bit conversion. */
static void stb_av1_lr_frame(unsigned short *plane_y, unsigned short *plane_u,
                             unsigned short *plane_v,
                             int stride_y, int stride_u, int stride_v,
                             int frame_w, int frame_h,
                             int ss_hor, int ss_ver, int bit_depth,
                             const stbv_av1_lr_mask *m)
{
    int p;
    if (!m) return;

    for (p = 0; p < 3; p++) {
        int chroma = p > 0;
        int ss_h = chroma ? ss_hor : 0;
        int ss_v = chroma ? ss_ver : 0;
        int w = (frame_w + ss_h) >> ss_h;
        int h = (frame_h + ss_v) >> ss_v;
        int stride = chroma ? (p == 1 ? stride_u : stride_v) : stride_y;
        unsigned short *plane = chroma ? (p == 1 ? plane_u : plane_v) : plane_y;
        int usz = m->unit_size_log2[chroma ? 1 : 0];
        int unit_sz = 1 << usz;
        int gw = m->grid_stride[p];
        int gr = m->grid_rows[p];
        int gy, gx;
        int any_non_none = 0;

        /* Quick check: any non-NONE types? */
        for (gy = 0; gy < gr && !any_non_none; gy++)
            for (gx = 0; gx < gw && !any_non_none; gx++)
                if (m->units[p][gy * gw + gx].type != STBV_AV1_RESTORATION_NONE)
                    any_non_none = 1;
        if (!any_non_none) continue;

        for (gy = 0; gy < gr; gy++) {
            for (gx = 0; gx < gw; gx++) {
                const stbv_av1_lr_unit *u = &m->units[p][gy * gw + gx];
                int ux0 = gx * unit_sz;
                int uy0 = gy * unit_sz;
                int uw = ux0 + unit_sz <= w ? unit_sz : w - ux0;
                int uh = uy0 + unit_sz <= h ? unit_sz : h - uy0;

                if (u->type == STBV_AV1_RESTORATION_WIENER) {
                    stbv_av1_wiener_plane(plane, stride, w, h,
                                          ux0, uy0, uw, uh,
                                          u->filter_v, u->filter_h,
                                          bit_depth);
                } else if (u->type == STBV_AV1_RESTORATION_SGRPROJ) {
                    int s0 = stbv_av1_sgr_tab[u->sgr_idx][0];
                    int s1 = stbv_av1_sgr_tab[u->sgr_idx][1];
                    int w0 = u->sgr_weights[0];
                    int w1_adj = 128 - (u->sgr_weights[0] + u->sgr_weights[1]);

                    if (s0 && s1)
                        stbv_av1_sgr_mix(plane, stride, w, h,
                                         ux0, uy0, uw, uh,
                                         s0, s1, w0, w1_adj);
                    else if (s0)
                        stbv_av1_sgr_5x5(plane, stride, w, h,
                                         ux0, uy0, uw, uh, s0, w0);
                    else if (s1)
                        stbv_av1_sgr_3x3(plane, stride, w, h,
                                         ux0, uy0, uw, uh, s1, w0);
                }
                /* NONE: no-op */
            }
        }
    }
}

#endif /* STB_AV1_LR_H */

/* ----------- BITSTREAM READER ----------- */
int sh_parsed_ok = 0;
int probe_seq_hbd = 0, probe_seq_mono = 0;

/* ----------- ISOBMFF/HEIF BOX PARSER ----------- */

/* Box header: size (4 or 8 bytes) + type (4 bytes) */
#define STB_AVIF_BOX_HEADER_SIZE 8
#define STB_AVIF_BOX_EXTENDED_SIZE 16

/* Known box types as 32-bit integers (big-endian ASCII) */
#define STB_AVIF_FOURCC(a,b,c,d) ((stbv_u32)((a)<<24|(b)<<16|(c)<<8|(d)))
#define STB_AVIF_BOX_FTYP   STB_AVIF_FOURCC('f','t','y','p')
#define STB_AVIF_BOX_META   STB_AVIF_FOURCC('m','e','t','a')
#define STB_AVIF_BOX_HDLR   STB_AVIF_FOURCC('h','d','l','r')
#define STB_AVIF_BOX_PITM   STB_AVIF_FOURCC('p','i','t','m')
#define STB_AVIF_BOX_ILOC   STB_AVIF_FOURCC('i','l','o','c')
#define STB_AVIF_BOX_IINF   STB_AVIF_FOURCC('i','i','n','f')
#define STB_AVIF_BOX_INFE   STB_AVIF_FOURCC('i','n','f','e')
#define STB_AVIF_BOX_IPRP   STB_AVIF_FOURCC('i','p','r','p')
#define STB_AVIF_BOX_IPCO   STB_AVIF_FOURCC('i','p','c','o')
#define STB_AVIF_BOX_IPMA   STB_AVIF_FOURCC('i','p','m','a')
#define STB_AVIF_BOX_ISPE   STB_AVIF_FOURCC('i','s','p','e')
#define STB_AVIF_BOX_PIXI   STB_AVIF_FOURCC('p','i','x','i')
#define STB_AVIF_BOX_AV1C   STB_AVIF_FOURCC('a','v','1','C')
#define STB_AVIF_BOX_IREF   STB_AVIF_FOURCC('i','r','e','f')
#define STB_AVIF_BOX_COLR   STB_AVIF_FOURCC('c','o','l','r')
#define STB_AVIF_BOX_MDAT   STB_AVIF_FOURCC('m','d','a','t')
#define STB_AVIF_BOX_MOOV   STB_AVIF_FOURCC('m','o','o','v')
#define STB_AVIF_BOX_MOOF   STB_AVIF_FOURCC('m','o','o','f')

struct stb_avif_box {
    stbv_u64 size;    /* total box size including header */
    stbv_u32 type;    /* 4-byte box type */
    stbv_u64 data_start; /* position of box content (after header) */
    stbv_u64 data_size;  /* size of box content */
};

/* Read a box header at current position and advance past it */
static void stb_avif_read_box_header(struct stb_av1_getbits *gb, struct stb_avif_box *box)
{
    stbv_u32 size32;
    stbv_u64 start = (stbv_u64)stb_av1_getbits_bytepos(gb);

    size32 = stb_av1_getbits_read_be32(gb);
    box->type = stb_av1_getbits_read_be32(gb);

    if (size32 == 1) {
        /* Extended size (64-bit) */
        box->size = stb_av1_getbits_read_be64(gb);
    } else if (size32 == 0) {
        /* Box extends to end of file */
        box->size = (stbv_u64)stb_av1_getbits_size(gb) - start;
    } else {
        box->size = (stbv_u64)size32;
    }

    box->data_start = (stbv_u64)stb_av1_getbits_bytepos(gb);
    if (box->size >= (stbv_u64)(stb_av1_getbits_bytepos(gb) - start)) {
        box->data_size = box->size - (stbv_u64)(stb_av1_getbits_bytepos(gb) - start);
    } else {
        box->data_size = 0;
    }
}

/* Skip to end of box */
static void stb_avif_skip_box(struct stb_av1_getbits *gb, const struct stb_avif_box *box)
{
    stbv_u64 end = box->data_start + box->data_size;
    if (end > (stbv_u64)stb_av1_getbits_size(gb))
        STB_AVIF_ERROR("Box extends beyond data");
    stb_av1_getbits_seek(gb, (size_t)end);
}

/* Enter a box: position at data start */
static void stb_avif_enter_box(struct stb_av1_getbits *gb, const struct stb_avif_box *box)
{
    if (box->data_start > (stbv_u64)stb_av1_getbits_size(gb))
        STB_AVIF_ERROR("Box position out of bounds");
    stb_av1_getbits_seek(gb, (size_t)box->data_start);
}

/* -------------------------------------------------------------------------- */
/* AVIF PARSER STATE                                                          */
/* -------------------------------------------------------------------------- */

struct stb_avif_avif_info {
    /* Image info */
    int width;
    int height;
    int bit_depth;
    int chroma_subsampling_x;
    int chroma_subsampling_y;
    int monochrome;

    /* AV1 codec config (from av1C box) */
    unsigned char av1c_data[32];
    int av1c_size;
    int av1c_seen;

    /* ipma / multi-av1C support */
    int av1c_prop_idx[4];         /* 1-based property indices of av1C in ipco */
    int av1c_prop_count;          /* number of av1C properties found */
    unsigned char av1c_all_data[4][32]; /* raw av1C OBU payload for each property */
    int av1c_all_size[4];         /* size of each av1C OBU payload */
    int av1c_all_bd[4];           /* bit_depth per av1C */
    int av1c_all_ssx[4];          /* chroma_subsampling_x per av1C */
    int av1c_all_ssy[4];          /* chroma_subsampling_y per av1C */
    int av1c_all_mono[4];         /* monochrome per av1C */
    int primary_av1c_prop_idx;    /* resolved 1-based index for primary item */
    /* ispe: store dimensions per property index so we can resolve after ipma */
    int ispe_prop_idx[8];         /* 1-based property indices of ispe in ipco */
    int ispe_all_w[8];            /* width per ispe property */
    int ispe_all_h[8];            /* height per ispe property */
    int ispe_prop_count;          /* number of ispe properties found */
    int primary_ispe_prop_idx;    /* resolved 1-based index for primary ispe */
    /* ipma: up to 16 items, each with up to 8 property associations */
    int ipma_item_count;
    int ipma_item_ids[16];
    int ipma_prop_count[16];
    int ipma_prop_idx[16][8];     /* 1-based property indices per item */

    /* Compressed AV1 data */
    const unsigned char *av1_data;
    size_t av1_size;
    void *ivf_concat_buf; /* non-NULL if av1_data was allocated for IVF concatenation */

    /* Output buffer */
    unsigned char *output;
    int output_channels;

    /* Input data */
    const unsigned char *input;
    int input_len;
    size_t meta_end_offset;

    /* Auxiliary alpha item (raw AV1 payload + decoded 8-bit plane) */
    int primary_item_id;
    int alpha_item_id;
    const unsigned char *alpha_av1;
    size_t alpha_size;
    unsigned char *alpha_plane;
    int alpha_stride;

    /* Decoded planes (8-bit) */
    unsigned char *plane_y;
    unsigned char *plane_u;
    unsigned char *plane_v;
    int stride_y;
    int stride_u;
    int stride_v;
};

/* ----------- ISOBMFF PARSER ----------- */

/* Find a box of given type within a container; recurses into sub-boxes if needed.
   Returns 1 if found, 0 if not. Does not modify gb position on return. */
static int stb_avif_find_box(struct stb_av1_getbits *gb, stbv_u32 type,
                              int deep_search, struct stb_avif_box *box_out)
{
    size_t saved_pos = stb_av1_getbits_bytepos(gb);

    while (stb_av1_getbits_bytepos(gb) + 8 <= stb_av1_getbits_size(gb)) {
        struct stb_avif_box box;
        size_t box_start = stb_av1_getbits_bytepos(gb);

        stb_avif_read_box_header(gb, &box);

        if (box.type == type) {
            stb_av1_getbits_seek(gb, (size_t)box.data_start);
            if (box_out) *box_out = box;
            return 1;
        }

        if (deep_search && (box.type == STB_AVIF_BOX_META ||
                            box.type == STB_AVIF_BOX_IPRP ||
                            box.type == STB_AVIF_BOX_IPCO ||
                            box.type == STB_AVIF_BOX_MOOV ||
                            box.type == STB_AVIF_BOX_MOOF))
        {
            stb_avif_enter_box(gb, &box);
            if (stb_avif_find_box(gb, type, deep_search, box_out)) {
                return 1;
            }
        }

        stb_av1_getbits_seek(gb, (size_t)(box_start + box.size));
    }

    stb_av1_getbits_seek(gb, saved_pos);
    return 0;
}

/* Parse ftyp box to verify this is an AVIF file */
static void stb_avif_parse_ftyp(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info)
{
    /* Skip major brand (4 bytes), minor version (4 bytes) */
    stb_av1_getbits_skip(gb, 8);

    /* Check for compatible brands */
    while (stb_av1_getbits_bytepos(gb) < stb_av1_getbits_size(gb)) {
        stbv_u32 brand = stb_av1_getbits_read_be32(gb);
        if (brand == STB_AVIF_FOURCC('a','v','i','f'))
            return; /* OK */
        /* We found avif brand; we're good */
    }

    /* Some files might not have avif brand but still be AVIF;
       check if we at least have an mif1 brand */
    /* no need to re-check; avif brand was found above */
}

/* Parse the av1C box (AV1 codec configuration) 
   box_data_size: remaining bytes in the av1C box (after box header) */
static void stb_avif_parse_av1c(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info,
                                 size_t box_data_size)
{
    int i;

    /* The av1C box contains an AV1CodecConfigurationBox */
    /* marker=1, version=1 */
    /* Actually the box just contains the AV1 config OBU data.
       From the ISOBMFF spec: the av1C box contains:
       unsigned int(1) marker = 1;
       unsigned int(7) version = 1;
       unsigned int(3) seq_profile;
       unsigned int(5) seq_level_idx_0;
       unsigned int(1) seq_tier_0;
       unsigned int(1) high_bitdepth;
       unsigned int(1) twelve_bit;
       unsigned int(1) monochrome;
       unsigned int(1) chroma_subsampling_x;
       unsigned int(1) chroma_subsampling_y;
       unsigned int(2) chroma_sample_position;
       unsigned int(3) reserved;
       unsigned int(1) initial_presentation_delay_present;
       if (initial_presentation_delay_present) {
           unsigned int(4) initial_presentation_delay_minus_one;
       } else {
           unsigned int(4) reserved;
       }
    */
    int seq_profile, seq_tier_0;
    int high_bitdepth, twelve_bit, monochrome;
    int chroma_subsampling_x, chroma_subsampling_y, chroma_sample_position;
    int initial_presentation_delay_present;

    /* Byte 0: marker(1)=1 + version(7)=1 */
    stb_av1_getbits_read_byte(gb);

    /* Byte 1: seq_profile(3) + seq_level_idx_0(5) */
    {
        int byte1 = stb_av1_getbits_read_byte(gb);
        seq_profile = (byte1 >> 5) & 7;
        /* seq_level_idx_0 = byte1 & 31; */
    }

    /* Byte 2: flags */
    {
        int byte2 = stb_av1_getbits_read_byte(gb);
        seq_tier_0                  = (byte2 >> 7) & 1;
        high_bitdepth               = (byte2 >> 6) & 1;
        twelve_bit                  = (byte2 >> 5) & 1;
        monochrome                  = (byte2 >> 4) & 1;
        chroma_subsampling_x        = (byte2 >> 3) & 1;
        chroma_subsampling_y        = (byte2 >> 2) & 1;
        chroma_sample_position      = byte2 & 3;

        info->monochrome = monochrome;
        info->chroma_subsampling_x = chroma_subsampling_x;
        info->chroma_subsampling_y = chroma_subsampling_y;

        if (high_bitdepth) {
            info->bit_depth = twelve_bit ? 12 : 10;
        } else {
            info->bit_depth = 8;
        }
    }

    /* Byte 3: reserved + initial_presentation_delay */
    {
        int byte3 = stb_av1_getbits_read_byte(gb);
        initial_presentation_delay_present = (byte3 >> 4) & 1;
    }

    /* Remaining bytes: config OBUs (sequence header OBU data) 
       box_data_size is the total av1C box content size; we've read 4 fixed bytes */
    info->av1c_size = (int)box_data_size - 4;
    if (info->av1c_size > (int)sizeof(info->av1c_data))
        info->av1c_size = (int)sizeof(info->av1c_data);
    if (info->av1c_size < 0) info->av1c_size = 0;

    for (i = 0; i < info->av1c_size && i < (int)box_data_size - 4; i++) {
        info->av1c_data[i] = (unsigned char)stb_av1_getbits_read_byte(gb);
    }

    STB_AVIF_CHECK(high_bitdepth == 0 || high_bitdepth == 1,
                   "Invalid bitdepth flag");
    (void)seq_profile;
    (void)seq_tier_0;
    (void)chroma_sample_position;
    (void)initial_presentation_delay_present;
}

/* Parse the iloc box (item location) to find where coded data is stored */
static void stb_avif_parse_iloc(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info,
                                 int primary_id,
                                 stbv_u32 *data_offset,
                                 stbv_u64 *data_size)
{
    int version;
    int offset_size, length_size, base_offset_size, index_size;
    int item_count, i_item;

    version = stb_av1_getbits_read_byte(gb);
    {
        /* fullbox: version(1) + flags(3), then the sizes byte */
        int fl;
        stb_av1_getbits_read_byte(gb);          /* version */
        stb_av1_getbits_read_byte(gb);
        stb_av1_getbits_read_byte(gb);
        fl       = stb_av1_getbits_read_byte(gb);
        /* ISO/AVIF: these 4-bit fields store SIZE-1 (0 = absent) */
        offset_size = (fl >> 4) & 0xF;
        length_size = fl & 0xF;
        /* base_offset_size / index_size byte is present in ALL versions */
        fl = stb_av1_getbits_read_byte(gb);
        base_offset_size = (fl >> 4) & 0xF;
        index_size = fl & 0xF;
        if (version < 2) {
            index_size = 0;
            (void)index_size;
        }
    }

    item_count = (int)stb_av1_getbits_read_be16(gb);
    *data_offset = 0;
    *data_size = 0;

    for (i_item = 0; i_item < item_count; i_item++) {
        int item_ID;
        int data_ref_index;
        int i_extent;
        int extent_count;

        if (version < 2) {
            item_ID = (int)stb_av1_getbits_read_be16(gb);
        } else {
            item_ID = (int)stb_av1_getbits_read_be16(gb);
            stb_av1_getbits_read_be16(gb);
        }

        if (version >= 1) {
            /* construction_method */
            /* 4 bytes: (12 reserved + 4 construction_method) or more depending on version */
            stb_av1_getbits_read_be16(gb); /* skip */
            data_ref_index = stb_av1_getbits_read_be16(gb);
        } else {
            data_ref_index = stb_av1_getbits_read_be16(gb);
        }
        (void)data_ref_index;

        /* base_offset */
        {
            int _off_sz;
            int j;
            stbv_u64 base_offset_val = 0;

            /* ISO/IEC 14496-12: per-item base_offset is base_offset_size
             * units wide in every iloc version. */
            _off_sz = base_offset_size;

            for (j = 0; j < _off_sz; j++) {
                base_offset_val = (base_offset_val << 8) | (stbv_u64)stb_av1_getbits_read_byte(gb);
            }

            extent_count = (int)stb_av1_getbits_read_be16(gb);

            for (i_extent = 0; i_extent < extent_count; i_extent++) {
                stbv_u64 extent_offset = 0;
                stbv_u64 extent_length = 0;
                int k;

                for (k = 0; k < offset_size; k++) {
                    extent_offset = (extent_offset << 8) | (stbv_u64)stb_av1_getbits_read_byte(gb);
                }
                for (k = 0; k < length_size; k++) {
                    extent_length = (extent_length << 8) | (stbv_u64)stb_av1_getbits_read_byte(gb);
                }

                if (item_ID == primary_id && i_extent == 0 &&
                    !*data_size) {
                    *data_offset = (stbv_u32)(base_offset_val + extent_offset);
                    *data_size = extent_length;
                }
            }
        }
    }
}

/* Parse pitm (Primary Item ID) */
static int stb_avif_parse_pitm(struct stb_av1_getbits *gb)
{
    int version = stb_av1_getbits_read_byte(gb);
    stb_av1_getbits_read_byte(gb); /* flags */
    stb_av1_getbits_read_byte(gb); /* flags */
    stb_av1_getbits_read_byte(gb); /* flags */
    if (version < 1) {
        return (int)stb_av1_getbits_read_be16(gb);
    } else {
        /* version >= 1 uses 32-bit */
        return (int)stb_av1_getbits_read_be32(gb);
    }
}

/* Parse the meta box to extract AVIF metadata */
static void stb_avif_parse_meta(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info)
{
    stbv_u32 data_offset = 0;
    stbv_u64 data_size = 0;
    struct stb_avif_box meta_box;
    size_t meta_end;

    /* Skip FullBox version+flags (4 bytes) */
    stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);
    stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);

    meta_box.data_start = (stbv_u64)stb_av1_getbits_bytepos(gb);
    meta_box.data_size = (stbv_u64)(info->meta_end_offset - stb_av1_getbits_bytepos(gb));

    meta_end = info->meta_end_offset;

    /* Scan sub-boxes within meta */
    while (stb_av1_getbits_bytepos(gb) < meta_end) {
        struct stb_avif_box sub;
        size_t sub_start = stb_av1_getbits_bytepos(gb);

        if (stb_av1_getbits_bytepos(gb) + 8 > stb_av1_getbits_size(gb)) break;

        stb_avif_read_box_header(gb, &sub);
        if (sub.type == STB_AVIF_BOX_HDLR) {
            /* handler box - verify picture handler */
        }
        else if (sub.type == STB_AVIF_BOX_PITM) {
            info->primary_item_id = stb_avif_parse_pitm(gb);
        }
        else if (sub.type == STB_AVIF_BOX_ILOC) {
            stb_avif_parse_iloc(gb, info, info->primary_item_id,
                                &data_offset, &data_size);
        }
        else if (sub.type == STB_AVIF_BOX_IREF) {
            /* iref: version/flags, then SUB-BOXES, one per reference
             * type: { u32 size; u32 type('auxl'); u16 from_item_ID;
             *         u16 reference_count; u16 to_item_ID[]; } */
            struct stb_avif_box ir = sub;
            stb_avif_enter_box(gb, &ir);
            stb_av1_getbits_read_byte(gb);
            stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);
            while (stb_av1_getbits_bytepos(gb) + 8 <= (size_t)(ir.data_start + ir.data_size)) {
                stbv_u32 esz = stb_av1_getbits_read_be32(gb);
                stbv_u32 ety = stb_av1_getbits_read_be32(gb);
                int from_id, ref_count, ri;
                size_t ebody_end;
                if (esz < 8) break;
                ebody_end = stb_av1_getbits_bytepos(gb) + esz - 8;
                if (ebody_end > (size_t)(ir.data_start + ir.data_size))
                    ebody_end = (size_t)(ir.data_start + ir.data_size);
                from_id = (int)stb_av1_getbits_read_be16(gb);
                ref_count = (int)stb_av1_getbits_read_be16(gb);
                for (ri = 0; ri < ref_count; ri++) {
                    int to_id;
                    if (stb_av1_getbits_bytepos(gb) + 2 > ebody_end) break;
                    to_id = (int)stb_av1_getbits_read_be16(gb);
                    if (ety == STB_AVIF_FOURCC('a','u','x','l') &&
                        to_id == info->primary_item_id)
                        info->alpha_item_id = from_id;
                }
                stb_av1_getbits_seek(gb, ebody_end);
            }
        }
        else if (sub.type == STB_AVIF_BOX_IPRP) {
            /* Item properties container */
            struct stb_avif_box iprp_box = sub;
            stb_avif_enter_box(gb, &iprp_box);

            while (stb_av1_getbits_bytepos(gb) < (size_t)(iprp_box.data_start + iprp_box.data_size)) {
                struct stb_avif_box iprp_sub;
                size_t iprp_sub_start = stb_av1_getbits_bytepos(gb);

                if (stb_av1_getbits_bytepos(gb) + 8 > stb_av1_getbits_size(gb)) break;
                stb_avif_read_box_header(gb, &iprp_sub);

                if (iprp_sub.type == STB_AVIF_BOX_IPCO) {
                    /* Item property container */
                    struct stb_avif_box ipco_box = iprp_sub;
                    stb_avif_enter_box(gb, &ipco_box);
                    {
                    int ipco_prop_pos = 0; /* 1-based ipco position counter */

                    while (stb_av1_getbits_bytepos(gb) < (size_t)(ipco_box.data_start + ipco_box.data_size)) {
                        struct stb_avif_box prop;
                        size_t prop_start = stb_av1_getbits_bytepos(gb);
                        ipco_prop_pos++;

                        if (stb_av1_getbits_bytepos(gb) + 8 > stb_av1_getbits_size(gb)) break;
                        stb_avif_read_box_header(gb, &prop);

                        if (prop.type == STB_AVIF_BOX_AV1C) {
                            /* Record 1-based property index for ipma lookup.
                             * Store raw av1C data; we'll select the right one
                             * after ipma is parsed. */
                            int idx = info->av1c_prop_count;
                            if (idx < 4) {
                                info->av1c_prop_idx[idx] = idx + 1;
                                /* Parse av1C into temporary storage */
                                {
                                    size_t saved_pos = stb_av1_getbits_bytepos(gb);
                                    stb_avif_parse_av1c(gb, info, (size_t)prop.data_size);
                                    /* Copy the just-parsed av1C data to all_data[idx] */
                                    if (info->av1c_size <= 32) {
                                        int bi;
                                        for (bi = 0; bi < info->av1c_size; bi++)
                                            info->av1c_all_data[idx][bi] = info->av1c_data[bi];
                                        info->av1c_all_size[idx] = info->av1c_size;
                                    }
                                    /* Cache parsed properties for this av1C */
                                    info->av1c_all_bd[idx] = info->bit_depth;
                                    info->av1c_all_ssx[idx] = info->chroma_subsampling_x;
                                    info->av1c_all_ssy[idx] = info->chroma_subsampling_y;
                                    info->av1c_all_mono[idx] = info->monochrome;
                                    (void)saved_pos;
                                }
                            }
                            info->av1c_prop_count++;
                            info->av1c_seen = 1;
                        }
                        else if (prop.type == STB_AVIF_BOX_ISPE) {
                            /* Image spatial extents: store per-property, resolve
                             * to primary item later via ipma. */
                            int si;
                            stb_av1_getbits_read_byte(gb); /* version */
                            stb_av1_getbits_read_byte(gb); /* flags */
                            stb_av1_getbits_read_byte(gb);
                            stb_av1_getbits_read_byte(gb);
                            {
                                int w = (int)stb_av1_getbits_read_be32(gb);
                                int h = (int)stb_av1_getbits_read_be32(gb);
                                si = info->ispe_prop_count;
                                if (si < 8) {
                                    info->ispe_prop_idx[si] = ipco_prop_pos;
                                    info->ispe_all_w[si] = w;
                                    info->ispe_all_h[si] = h;
                                }
                                info->ispe_prop_count++;
                            }
                        }
                        else if (prop.type == STB_AVIF_BOX_PIXI) {
                            /* Pixel information (bit depth per channel) */
                            stb_av1_getbits_read_byte(gb); /* version */
                            stb_av1_getbits_read_byte(gb); /* flags */
                            stb_av1_getbits_read_byte(gb);
                            stb_av1_getbits_read_byte(gb);
                            /* num_channels */
                            (void)stb_av1_getbits_read_byte(gb);
                        }

                        stb_av1_getbits_seek(gb, (size_t)(prop_start + prop.size));
                    }
                    } /* end ipco_prop_pos block */
                }
                else if (iprp_sub.type == STB_AVIF_BOX_IPMA) {
                    /* Item Property Association box: maps item IDs to property indices.
                     * Format: version(1) + flags(3) + entry_count(4)
                     *   each entry: item_ID(16|32) + association_count(8)
                     *     per assoc: property_index(16, 1-based, high bit = essential) */
                    struct stb_avif_box ipma_box = iprp_sub;
                    stb_avif_enter_box(gb, &ipma_box);
                    {
                        int ipma_version = stb_av1_getbits_read_byte(gb);
                        stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);
                        {
                            stbv_u32 entry_count = stb_av1_getbits_read_be32(gb);
                            stbv_u32 ei;
                            info->ipma_item_count = 0;
                            for (ei = 0; ei < entry_count && ei < 16; ei++) {
                                int item_id, acnt, ai;
                                if (ipma_version == 0)
                                    item_id = (int)stb_av1_getbits_read_be16(gb);
                                else
                                    item_id = (int)stb_av1_getbits_read_be32(gb);
                                acnt = (int)stb_av1_getbits_read_byte(gb);
                                info->ipma_item_ids[info->ipma_item_count] = item_id;
                                info->ipma_prop_count[info->ipma_item_count] = 0;
                                for (ai = 0; ai < acnt && ai < 8; ai++) {
                                    int prop_idx = (int)stb_av1_getbits_read_be16(gb);
                                    /* bit 15 = essential flag; property_index is bits 14..0 */
                                    prop_idx &= 0x7fff;
                                    if (info->ipma_prop_count[info->ipma_item_count] < 8)
                                        info->ipma_prop_idx[info->ipma_item_count][info->ipma_prop_count[info->ipma_item_count]++] = prop_idx;
                                }
                                info->ipma_item_count++;
            }
        }
    }
                }

                stb_av1_getbits_seek(gb, (size_t)(iprp_sub_start + iprp_sub.size));
            }
        }

        /* Resolve the correct av1C for the primary item using ipma.
         * ipma maps item_ID -> property indices (1-based) into ipco.
         * Find the primary item's av1C property index, then select it. */
        if (info->primary_item_id > 0 && info->av1c_prop_count > 1) {
            int pi, ai2;
            for (pi = 0; pi < info->ipma_item_count; pi++) {
                if (info->ipma_item_ids[pi] == info->primary_item_id) {
                    for (ai2 = 0; ai2 < info->ipma_prop_count[pi]; ai2++) {
                        int pidx = info->ipma_prop_idx[pi][ai2];
                        int k;
                        for (k = 0; k < info->av1c_prop_count; k++) {
                            if (info->av1c_prop_idx[k] == pidx) {
                                /* Found the primary item's av1C. Copy cached
                                 * properties directly instead of re-parsing. */
                                int bi;
                                info->av1c_size = info->av1c_all_size[k];
                                for (bi = 0; bi < info->av1c_size; bi++)
                                    info->av1c_data[bi] = info->av1c_all_data[k][bi];
                                info->primary_av1c_prop_idx = pidx;
                                info->bit_depth = info->av1c_all_bd[k];
                                info->chroma_subsampling_x = info->av1c_all_ssx[k];
                                info->chroma_subsampling_y = info->av1c_all_ssy[k];
                                info->monochrome = info->av1c_all_mono[k];
                                break;
                            }
                        }
                        if (info->primary_av1c_prop_idx) break;
                    }
                    break;
                }
            }
        }

        /* Resolve the correct ispe (width/height) for the primary item using ipma.
         * Multiple ispe properties may exist (main + thumbnails); we must pick
         * the one associated with the primary item. */
        if (info->primary_item_id > 0 && info->ispe_prop_count > 0) {
            int pi, ai2;
            for (pi = 0; pi < info->ipma_item_count; pi++) {
                if (info->ipma_item_ids[pi] == info->primary_item_id) {
                    for (ai2 = 0; ai2 < info->ipma_prop_count[pi]; ai2++) {
                        int pidx = info->ipma_prop_idx[pi][ai2];
                        int k;
                        for (k = 0; k < info->ispe_prop_count && k < 8; k++) {
                            if (info->ispe_prop_idx[k] == pidx) {
                                info->width = info->ispe_all_w[k];
                                info->height = info->ispe_all_h[k];
                                info->primary_ispe_prop_idx = pidx;
                                break;
                            }
                        }
                        if (info->primary_ispe_prop_idx) break;
                    }
                    break;
                }
            }
        }
        /* Fallback: if no primary ispe found via ipma, use the first ispe */
        if (!info->primary_ispe_prop_idx && info->ispe_prop_count > 0) {
            info->width = info->ispe_all_w[0];
            info->height = info->ispe_all_h[0];
        }

        stb_av1_getbits_seek(gb, (size_t)(sub_start + sub.size));
    }

    /* Now read the mdat data */
    {
        size_t saved = stb_av1_getbits_bytepos(gb);

        stb_av1_getbits_seek(gb, 0);
        if (stb_avif_find_box(gb, STB_AVIF_BOX_MDAT, 0, NULL)) {
                    info->av1_data = gb->ptr_start + stb_av1_getbits_bytepos(gb);
            info->av1_size = stb_av1_getbits_size(gb) - stb_av1_getbits_bytepos(gb); /* Rest of file is mdat content */

            /* If we have iloc info, use that offset instead */
            if (data_size > 0 && data_offset > 0) {
                info->av1_data = gb->ptr_start + data_offset;
                info->av1_size = (size_t)data_size;
            } else {
                /* Conservative: mdat may contain more than just our image.
                   Use iloc info. But if we don't have it, use all remaining. */
                /* The actual av1 data starts at data_offset from the beginning of mdat */
                if (data_offset > 0) {
                    /* data_offset is absolute in the file */
                    info->av1_data = gb->ptr_start + data_offset;
                    if (data_size > 0)
                        info->av1_size = (size_t)data_size;
                    else
                        info->av1_size = stb_av1_getbits_size(gb) - data_offset;
                }
            }
        }
        stb_av1_getbits_seek(gb, saved);
    }
}

/* -------------------------------------------------------------------------- */
/* AV1 BITSTREAM PARSER                                                       */
/* -------------------------------------------------------------------------- */

/* OBU types */
#define STB_AV1_OBU_SEQUENCE_HEADER 1
#define STB_AV1_OBU_TEMPORAL_DELIMITER 2
#define STB_AV1_OBU_FRAME_HEADER 3
#define STB_AV1_OBU_TILE_GROUP 4
#define STB_AV1_OBU_METADATA 5
#define STB_AV1_OBU_FRAME 6
#define STB_AV1_OBU_REDUNDANT_FRAME_HEADER 7
#define STB_AV1_OBU_TILE_LIST 8
#define STB_AV1_OBU_PADDING 15

/* OBU header:
   bit 0: forbidden (0)
   bits 1-4: type
   bit 5: obu_extension_flag
   bit 6: obu_has_size_field
   bit 7: obu_reserved_1bit (1)
*/
static int stb_av1_read_obu_header(struct stb_av1_getbits *gb, int *obu_type,
                                    int *obu_extension_flag, int *obu_has_size_field)
{
    int hdr = stb_av1_getbits_read_byte(gb);
    if (hdr & 0x80) STB_AVIF_ERROR("Invalid OBU header (reserved bit not 0)");
    *obu_type = (hdr >> 3) & 0xF;
    *obu_extension_flag = (hdr >> 2) & 1;
    *obu_has_size_field = (hdr >> 1) & 1;
    return hdr & 1; /* obu_forbidden_bit */
}

/* Read OBU size (LEB128 encoded) */
static stbv_u32 stb_av1_read_obu_size(struct stb_av1_getbits *gb)
{
    return stb_av1_getbits_read_uleb128(gb);
}

/* -------------------------------------------------------------------------- */
/* AV1 BOOLEAN (ARITHMETIC) ENTROPY DECODER                                   */
/* -------------------------------------------------------------------------- */

/* The AV1 spec defines a Boolean decoder based on the Daala entropy coder.
   State: value (16-bit window), range (9-bit, 128-255) */

#define STB_AV1_BOOL_READER_SIZE 4096
#define STB_AV1_BOOL_BUF_BITS 8

struct stb_av1_bool_reader {
    const unsigned char *data;
    size_t size;
    size_t pos;
    stbv_u32 value;   /* current window (bits) */
    stbv_u32 range;   /* current range (128-255) */
    int count;        /* bits in value */
    int error;
};

/* Initialize a Boolean reader from a byte stream.
   Uses the standard AV1/Daala Boolean decoder init (ref: dav1d, libaom). */
static void stb_av1_bool_reader_init(struct stb_av1_bool_reader *br,
                                       const unsigned char *data, size_t size)
{
    br->data = data;
    br->size = size;
    br->pos = 0;
    br->value = 0;
    br->range = 128;
    br->count = 8;  /* bits remaining in value buffer */
    br->error = 0;

    /* Load the first 16 bits into value (MSB-first, as two bytes) */
    if (br->pos < br->size) {
        br->value = (stbv_u32)br->data[br->pos++];
    }
    if (br->pos < br->size) {
        br->value = (br->value << 8) | (stbv_u32)br->data[br->pos++];
    }
    /* value now contains 16 bits, we consume 8 during renormalization;
       the rest stays buffered. The count=8 tracks we have 8 usable bits
       beyond the initial renormalization requirement. 
       This matches the spec behavior. */
}

static int stb_av1_bool_read_bit(struct stb_av1_bool_reader *br)
{
    stbv_u32 split;
    int bit;

    split = 1 + (((br->range - 1) * 128) >> 8); /* prob=128 means 50% */
    if (br->value < split) {
        br->range = split;
        bit = 0;
    } else {
        br->range = br->range - split;
        br->value = br->value - split;
        bit = 1;
    }

    while (br->range < 128) {
        int b;
        if (br->pos < br->size) {
            b = (br->data[br->pos] >> (7 - (br->count & 7))) & 1;
            br->count++;
            if ((br->count & 7) == 0)
                br->pos++;
        } else {
            b = 0; /* fill with 0 if out of data */
            br->count++;
        }
        br->value = (br->value << 1) | b;
        br->range <<= 1;
    }

    return bit;
}

/* Decode a Boolean symbol with given probability (0-255, where 128 = 50%) */
static int stb_av1_bool_decode(struct stb_av1_bool_reader *br, int prob)
{
    stbv_u32 split;
    int bit;

    split = 1 + (((br->range - 1) * prob) >> 8);
    if (br->value < split) {
        br->range = split;
        bit = 0;
    } else {
        br->range = br->range - split;
        br->value = br->value - split;
        bit = 1;
    }

    {
        int _rs = 16;
        while (br->range < 128 && _rs > 0) {
            int b;
            if (br->pos < br->size) {
                b = (br->data[br->pos] >> (7 - (br->count & 7))) & 1;
                br->count++;
                if ((br->count & 7) == 0)
                    br->pos++;
            } else {
                b = 0;
                br->count++;
            }
            br->value = (br->value << 1) | b;
            br->range <<= 1;
            _rs--;
        }
    }

    return bit;
}

/* Decode an unsigned integer with equal probability (uniform) */
static stbv_u32 stb_av1_bool_decode_literal(struct stb_av1_bool_reader *br,
                                              int bits)
{
    stbv_u32 val = 0;
    while (bits > 0) {
        bits--;
        val = (val << 1) | (stbv_u32)stb_av1_bool_decode(br, 128);
    }
    return val;
}

/* Decode a "subexp" coded unsigned integer as used in AV1.
   This is used for things like base_q_idx, etc. */
static stbv_u32 stb_av1_decode_subexp(struct stb_av1_bool_reader *br,
                                       int ref, int n)
{
    stbv_u32 v;
    if (stb_av1_bool_decode(br, 128)) {
        stbv_u32 d;
        stbv_u32 d2;
        int s = 0;
        int mk = 0;
        d = stb_av1_bool_decode_literal(br, 4) + 1;
        d2 = (stbv_u32)1 << d;
        /* In AV1, probs are adapted. For simplicity, use 128 everywhere. */
        if (n <= (int)d2) {
            v = stb_av1_bool_decode_literal(br, n - 1) + ref + 1;
        } else {
            s = n - (int)d2;
            while (mk < 3 && stb_av1_bool_decode(br, 128)) {
                s -= (int)d2;
                d2 = (stbv_u32)((int)d2 << 1);
                mk++;
            }
            v = (stbv_u32)(stb_av1_bool_decode_literal(br, s) + ref + 1 + (mk * (int)((stbv_u32)1 << d)));
        }
    } else {
        v = (stbv_u32)ref;
    }
    return v;
}

/* Decode a uniform symbol with count n */
static int stb_av1_decode_uniform(struct stb_av1_bool_reader *br, int n)
{
    int l;
    int m;
    int v;
    if (n <= 1) return 0;
    l = 0;
    while ((1 << l) < n) l++;
    m = (1 << l) - n;
    v = (int)stb_av1_bool_decode_literal(br, l - 1);
    if (v < m) return v;
    return (v << 1) | (stb_av1_bool_decode(br, 128) - m);
}

/* NSYM symbol decoding (non-symmetric) with cumulative probabilities */
/* Simplified: decode an n-ary symbol using a binary tree with equal probs */
static int stb_av1_decode_nsym(struct stb_av1_bool_reader *br, int n)
{
    if (n <= 1) return 0;
    /* Use uniform decoding as a simplification */
    return stb_av1_decode_uniform(br, n);
}

/* -------------------------------------------------------------------------- */
/* AV1 SEQUENCE HEADER STRUCT                                                 */
/* -------------------------------------------------------------------------- */

struct stb_av1_sequence_header {
    int seq_profile;
    int still_picture;
    int reduced_still_picture_header;
    int frame_width_bits;
    int frame_height_bits;
    int max_frame_width;
    int max_frame_height;
    int enable_order_hint;
    int enable_dist_wtd_comp;
    int enable_masked_comp;
    int enable_intra_edge_filter;
    int enable_interintra_comp;
    int enable_dual_filter;
    int enable_jnt_comp;
    int enable_superres;
    int enable_cdef;
    int enable_restoration;
    int film_grain_params_present;
    int timing_info_present;
    int decoder_model_info_present;
    int display_model_info_present;
    int operating_points_cnt;
    int color_description_present;
    int color_primaries;
    int transfer_characteristics;
    int matrix_coefficients;
    int color_range;
    int chroma_sample_position;
    int initial_display_delay_present;
    int buffer_removal_time_length_minus_1;
    int bit_depth;
    int monochrome;
    int subsampling_x;
    int subsampling_y;
};

/* -------------------------------------------------------------------------- */
/* AV1 FRAME HEADER PARSER                                                   */
/* -------------------------------------------------------------------------- */

struct stb_av1_frame_header {
    int show_existing_frame;
    int frame_type; /* KEY_FRAME=0, INTER_FRAME=1, INTRA_ONLY=2, S_FRAME=3 */
    int show_frame;
    int error_resilient_mode;
    int disable_cdf_update;
    int allow_screen_content_tools;
    int force_integer_mv;
    int current_frame_id;
    int frame_size_override;
    int frame_width;
    int frame_height;
    int render_width;
    int render_height;
    int superres_scale_denominator;
    int use_ref_frame_mvs;
    int order_hint;
    int refresh_frame_flags;
    int allow_high_precision_mv;
    int is_motion_mode_switchable;
    int use_transposed_filter;
    int reference_select;
    int reduced_tx_set;
    int allow_intrabc;
    int primary_ref_frame;
    int base_q_idx;
    int delta_q_y_dc;
    int delta_q_u_dc;
    int delta_q_u_ac;
    int delta_q_v_dc;
    int delta_q_v_ac;
    int using_qmatrix;
    int qm_y;
    int qm_u;
    int qm_v;
    int segmentation_enabled;
    int segment_update_map;
    int seg_temporal;
    int seg_id_pre_skip;
    int last_active_seg_id;
    int cdef_bits;
    int cdef_y_pri_strength[8];
    int cdef_y_sec_strength[8];
    int cdef_uv_pri_strength[8];
    int cdef_uv_sec_strength[8];
    int cdef_damping;
    int loop_restoration;
    int lr_type[3];     /* 0=none, 1=wiener, 2=sgrproj, 3=switchable */
    int lr_unit_size[3];
    int tx_mode;
    int skip_mode;
    int skip_mode_frame[2];
};

/* Frame types */
#define STB_AV1_KEY_FRAME 0
#define STB_AV1_INTER_FRAME 1
#define STB_AV1_INTRA_ONLY 2
#define STB_AV1_S_FRAME 3

static void stb_av1_parse_frame_header(struct stb_av1_frame_header *fh,
                                        struct stb_av1_sequence_header *sh,
                                        struct stb_av1_bool_reader *br)
{
    int frame_to_show_map_idx;

    /* show_existing_frame */
    fh->show_existing_frame = stb_av1_bool_decode(br, 128);
    if (fh->show_existing_frame) {
        frame_to_show_map_idx = (int)stb_av1_bool_decode_literal(br, 3);
        (void)frame_to_show_map_idx;
        /* For AVIF we should never have this, but handle gracefully */
        if (sh->decoder_model_info_present) {
            stb_av1_bool_decode_literal(br, sh->buffer_removal_time_length_minus_1 + 1);
        }
        return; /* No more frame data needed */
    }

    fh->frame_type = (int)stb_av1_bool_decode_literal(br, 2);
    fh->show_frame = stb_av1_bool_decode(br, 128);
    fh->error_resilient_mode = stb_av1_bool_decode(br, 128);

    if (fh->frame_type == STB_AV1_KEY_FRAME && fh->show_frame) {
        /* This path is common for AVIF */
        /* no temporal delimiters needed for still images */
    }

    if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
        fh->disable_cdf_update = stb_av1_bool_decode(br, 128);
        fh->allow_screen_content_tools = stb_av1_bool_decode(br, 128);
        if (fh->allow_screen_content_tools) {
            fh->force_integer_mv = stb_av1_bool_decode(br, 128);
        } else {
            fh->force_integer_mv = 0;
        }
    }

    /* Frame size */
    if (sh->reduced_still_picture_header) {
        fh->frame_width = sh->max_frame_width;
        fh->frame_height = sh->max_frame_height;
        fh->render_width = fh->frame_width;
        fh->render_height = fh->frame_height;
        fh->superres_scale_denominator = 8; /* SCALE_NUMERATOR = 8 (no superres) */
        fh->frame_size_override = 0;
    } else {
        fh->frame_size_override = stb_av1_bool_decode(br, 128);
        if (fh->frame_size_override) {
            fh->frame_width = (int)stb_av1_bool_decode_literal(br, sh->frame_width_bits) + 1;
            fh->frame_height = (int)stb_av1_bool_decode_literal(br, sh->frame_height_bits) + 1;
        } else {
            fh->frame_width = sh->max_frame_width;
            fh->frame_height = sh->max_frame_height;
        }

        fh->superres_scale_denominator = 8; /* default: no superres */
        if (sh->enable_superres) {
            if (stb_av1_bool_decode(br, 128)) { /* use_superres */
                fh->superres_scale_denominator = (int)stb_av1_bool_decode_literal(br, 3) + 9;
            }
        }

        /* compute image size */
        {
            int upscaled_width = fh->frame_width;
            (void)upscaled_width;
        }

        fh->render_width = (int)stb_av1_bool_decode_literal(br, sh->frame_width_bits + 1) + 1;
        fh->render_height = (int)stb_av1_bool_decode_literal(br, sh->frame_height_bits + 1) + 1;
    }

    /* Use_ref_frame_mvs and inter skip */
    if (fh->frame_type == STB_AV1_INTRA_ONLY || fh->frame_type == STB_AV1_S_FRAME) {
        fh->allow_intrabc = stb_av1_bool_decode(br, 128);
        fh->use_ref_frame_mvs = 0;
        fh->reference_select = 0;
    }

    /* Refresh frame flags */
    fh->refresh_frame_flags = 0;
    if (fh->frame_type == STB_AV1_KEY_FRAME) {
        if (fh->show_frame) {
            fh->refresh_frame_flags = 0xFF;
        } else {
            fh->refresh_frame_flags = (int)stb_av1_bool_decode_literal(br, 8);
        }
    } else if (fh->frame_type == STB_AV1_INTRA_ONLY) {
        fh->refresh_frame_flags = (int)stb_av1_bool_decode_literal(br, 8);
    }

    /* Order hint */
    if (sh->enable_order_hint && !fh->error_resilient_mode) {
        fh->order_hint = (int)stb_av1_bool_decode_literal(br, 2); /* order_hint_bits_minus_1 + 1 */
    }

    if (!sh->reduced_still_picture_header) {
        /* Primary reference frame */
        if (fh->error_resilient_mode || (fh->frame_type == STB_AV1_KEY_FRAME && fh->show_frame)) {
            fh->primary_ref_frame = 7; /* PRIMARY_REF_NONE */
        } else {
            fh->primary_ref_frame = (int)stb_av1_bool_decode_literal(br, 3);
        }
    }

    /* Quantization parameters */
    {
        int y_dc_q_delta, u_dc_q_delta, u_ac_q_delta, v_dc_q_delta, v_ac_q_delta;

        fh->base_q_idx = (int)stb_av1_bool_decode_literal(br, 8);

        y_dc_q_delta = 0;
        if (stb_av1_bool_decode(br, 128)) {
            y_dc_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
        }
        /* Delta Q is signed: (-16..16) */
        fh->delta_q_y_dc = y_dc_q_delta > 16 ? y_dc_q_delta - 32 : y_dc_q_delta;

        u_dc_q_delta = 0;
        v_dc_q_delta = 0;
        u_ac_q_delta = 0;
        v_ac_q_delta = 0;

        if (sh->seq_profile > 0 && !sh->monochrome) {
            if (stb_av1_bool_decode(br, 128)) {
                u_dc_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
            if (stb_av1_bool_decode(br, 128)) {
                u_ac_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
            if (stb_av1_bool_decode(br, 128)) {
                v_dc_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
            if (stb_av1_bool_decode(br, 128)) {
                v_ac_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
        }

        fh->delta_q_u_dc = u_dc_q_delta > 16 ? u_dc_q_delta - 32 : u_dc_q_delta;
        fh->delta_q_u_ac = u_ac_q_delta > 16 ? u_ac_q_delta - 32 : u_ac_q_delta;
        fh->delta_q_v_dc = v_dc_q_delta > 16 ? v_dc_q_delta - 32 : v_dc_q_delta;
        fh->delta_q_v_ac = v_ac_q_delta > 16 ? v_ac_q_delta - 32 : v_ac_q_delta;
    }

    /* Quantization matrix */
    fh->using_qmatrix = stb_av1_bool_decode(br, 128);
    if (fh->using_qmatrix) {
        fh->qm_y = (int)stb_av1_bool_decode_literal(br, 4);
        fh->qm_u = (int)stb_av1_bool_decode_literal(br, 4);
        fh->qm_v = (int)stb_av1_bool_decode_literal(br, 4);
    }

    /* Segmentation */
    fh->segmentation_enabled = stb_av1_bool_decode(br, 128);
    if (fh->segmentation_enabled) {
        if (fh->primary_ref_frame != 7) {
            fh->seg_temporal = stb_av1_bool_decode(br, 128);
            fh->segment_update_map = stb_av1_bool_decode(br, 128);
        } else {
            fh->seg_temporal = 0;
            fh->segment_update_map = stb_av1_bool_decode(br, 128);
        }
        fh->seg_id_pre_skip = 0; /* default: skip before seg */
        fh->last_active_seg_id = 0; /* simplified */
        if (fh->seg_temporal || fh->segment_update_map) {
            /* parse segment tree */
            fh->seg_id_pre_skip = stb_av1_bool_decode(br, 128);
        }
    }

    /* Delta Q/Delta LF */
    {
        int delta_q_present = 0;
        int delta_q_res = 0;
        if (fh->primary_ref_frame != 7 || fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY) {
            delta_q_present = stb_av1_bool_decode(br, 128);
            if (delta_q_present) {
                delta_q_res = (int)stb_av1_bool_decode_literal(br, 2) + 1;
                (void)delta_q_res;
            }
        }
        if (delta_q_present) {
            int delta_lf_present = stb_av1_bool_decode(br, 128);
            (void)delta_lf_present;
            if (delta_lf_present) {
                int delta_lf_multi = stb_av1_bool_decode(br, 128);
                (void)delta_lf_multi;
            }
        }
    }

    /* tx_mode */
    fh->tx_mode = (int)stb_av1_bool_decode_literal(br, 2); /* 0=ONLY_4X4, 1=LARGEST, 2=SELECT */

    /* skip_mode */
    fh->skip_mode = 0;
    if (fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY) {
        /* skip_mode not allowed for intra frames */
        fh->skip_mode = 0;
    } else if (sh->enable_order_hint) {
        if (stb_av1_bool_decode(br, 128)) {
            fh->skip_mode_frame[0] = (int)stb_av1_bool_decode_literal(br, 3);
            fh->skip_mode_frame[1] = (int)stb_av1_bool_decode_literal(br, 3);
            fh->skip_mode = 1;
        }
    }

    /* Loop filter params */
    if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
        /* Cdef params */
        if (sh->enable_cdef) {
            fh->cdef_bits = (int)stb_av1_bool_decode_literal(br, 2);
            {
                int i;
                for (i = 0; i < (1 << fh->cdef_bits); i++) {
                    fh->cdef_y_pri_strength[i] = (int)stb_av1_bool_decode_literal(br, 4);
                    fh->cdef_y_sec_strength[i] = (int)stb_av1_bool_decode_literal(br, 2);
                    fh->cdef_uv_pri_strength[i] = (int)stb_av1_bool_decode_literal(br, 4);
                    fh->cdef_uv_sec_strength[i] = (int)stb_av1_bool_decode_literal(br, 2);
                }
            }
            fh->cdef_damping = (int)stb_av1_bool_decode_literal(br, 2) + 3;
        }

        /* Loop restoration */
        if (sh->enable_restoration) {
            {
                int i;
                for (i = 0; i < (sh->monochrome ? 1 : 3); i++) {
                    fh->lr_type[i] = (int)stb_av1_bool_decode_literal(br, 2);
                    if (fh->lr_type[i]) {
                        fh->lr_unit_size[i] = (int)stb_av1_bool_decode_literal(br, 1) + 1;
                    }
                }
            }
        }
    }

    /* Tile info */
    {
        int tile_cols_log2, tile_rows_log2;
        int tile_cols, tile_rows;
        int context_update_tile_id;
        int i;

        tile_cols_log2 = 0;
        tile_rows_log2 = 0;

        if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
            /* Number of tile columns */
            if (stb_av1_bool_decode(br, 128)) {
                tile_cols_log2 = (int)stb_av1_bool_decode_literal(br, 2);
            }
            if (stb_av1_bool_decode(br, 128)) {
                tile_rows_log2 = (int)stb_av1_bool_decode_literal(br, 2);
            }
        }

        tile_cols = 1 << tile_cols_log2;
        tile_rows = 1 << tile_rows_log2;

        /* context_update_tile_id */
        context_update_tile_id = 0;
        if (tile_cols * tile_rows > 1) {
            context_update_tile_id = (int)stb_av1_bool_decode_literal(br, tile_cols_log2 + tile_rows_log2);
        }
        (void)context_update_tile_id;

        /* tile_size_bytes */
        {
            int tile_size_bytes = (int)stb_av1_bool_decode_literal(br, 2) + 1;
            (void)tile_size_bytes;
        }

        /* For each tile, we need the tile size. For simplicity we handle single-tile. */
        for (i = 0; i < tile_cols * tile_rows; i++) {
            if (i > 0) {
                /* tile_size_minus_1 */
                stb_av1_bool_decode_literal(br, 8); /* simplified */
            }
        }
    }

    /* Quantizer matrices for the frame */
    /* (already handled above via using_qmatrix) */

    /* Film grain */
    if (sh->film_grain_params_present && (!fh->show_existing_frame || fh->frame_type != STB_AV1_KEY_FRAME)) {
        if (stb_av1_bool_decode(br, 128)) { /* apply_grain */
            /* parse film grain params (skipped for now) */
        }
    }
}

/* -------------------------------------------------------------------------- */
/* AV1 TILE DECODER - MAIN FRAME DECODE                                      */
/* -------------------------------------------------------------------------- */

/* Context for tile decoding */
struct stb_av1_tile_context {
    struct stb_av1_sequence_header *sh;
    struct stb_av1_frame_header *fh;

    /* Decoded frames */
    int frame_width;
    int frame_height;
    int mb_cols;  /* MiCols (4x4 units) */
    int mb_rows;  /* MiRows (4x4 units) */

    /* Current tile position */
    int tile_row;
    int tile_col;

    /* Boolean reader */
    struct stb_av1_bool_reader *br;

    /* Quantization parameters */
    int qindex_y;
    int qindex_u;
    int qindex_v;

    /* Dequantization matrices */
    int dequant_y_dc[2];
    int dequant_y_ac[2];
    int dequant_u_dc[2];
    int dequant_u_ac[2];
    int dequant_v_dc[2];
    int dequant_v_ac[2];

    /* Output image planes */
    unsigned char *plane_y;
    unsigned char *plane_u;
    unsigned char *plane_v;
    int stride_y;
    int stride_u;
    int stride_v;

    /* Progress tracking */
    int total_sb;
    int done_sb;
    int next_report_sb;
    time_t start_time;

    /* Bit depth */
    int bit_depth;
    int pixel_max;
};

/* -------------------------------------------------------------------------- */
/* DAV1D BACKEND                                                              */
/* -------------------------------------------------------------------------- */

#ifdef STB_AVIF_USE_DAV1D
static int stb_avif_decode_with_dav1d(const unsigned char *av1_data, size_t av1_size,
                                       int *width, int *height,
                                       unsigned char **y_plane, int *y_stride,
                                       unsigned char **u_plane, int *u_stride,
                                       unsigned char **v_plane, int *v_stride,
                                       int *bit_depth, int *monochrome,
                                       int *subsampling_x, int *subsampling_y,
                                       int *color_range, int *matrix_coefficients)
{
    Dav1dContext *ctx = NULL;
    Dav1dSettings s;
    Dav1dData data;
    Dav1dPicture pic = { 0 };
    int ret;
    int i;

    dav1d_default_settings(&s);
    s.n_threads = 1;
    s.all_layers = 0;

    ret = dav1d_open(&ctx, &s);
    if (ret < 0) {  return 0; }

    /* Copy the AV1 data into dav1d-managed memory (dav1d takes ownership) */
    {
        uint8_t *ptr = dav1d_data_create(&data, av1_size);
        if (!ptr) { dav1d_close(&ctx); return 0; }
        memcpy(ptr, av1_data, av1_size);
    }

    /* Send data to decoder */
    ret = dav1d_send_data(ctx, &data);
    if (ret < 0 && ret != DAV1D_ERR(EAGAIN)) {
        dav1d_data_unref(&data);
        dav1d_close(&ctx);
        return 0;
    }
    dav1d_data_unref(&data);

    /* Get decoded picture */
    ret = dav1d_get_picture(ctx, &pic);
    if (ret < 0) {
        fprintf(stderr, "  dav1d: get_picture failed (%d)\n", ret);
        dav1d_close(&ctx);
        return 0;
    }

    /* Extract picture info */
    *width = pic.p.w;
    *height = pic.p.h;
    *bit_depth = pic.p.bpc;
    *monochrome = 0;

    /* Extract colour properties from the decoded sequence header */
    if (pic.seq_hdr) {
        *color_range = pic.seq_hdr->color_range;
        *matrix_coefficients = (int)pic.seq_hdr->mtrx;
    } else {
        *color_range = 0;
        *matrix_coefficients = 1;
    }

    /* Determine chroma subsampling from layout */
    if (pic.p.layout == DAV1D_PIXEL_LAYOUT_I420) {
        *subsampling_x = 1;
        *subsampling_y = 1;
    } else if (pic.p.layout == DAV1D_PIXEL_LAYOUT_I422) {
        *subsampling_x = 1;
        *subsampling_y = 0;
    } else {
        *subsampling_x = 0;
        *subsampling_y = 0;
    }

    /* Compute chroma dimensions using ceiling division */
    {
        int uv_w = (*width + (1 << *subsampling_x) - 1) >> *subsampling_x;
        int uv_h = (*height + (1 << *subsampling_y) - 1) >> *subsampling_y;

        /* Allocate 8-bit output planes */
        *y_stride = (*width + 31) & ~31;
        *y_plane = (unsigned char *)malloc((size_t)(*y_stride * *height));
        if (!*y_plane) { dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

        *u_stride = (uv_w + 31) & ~31;
        *u_plane = (unsigned char *)malloc((size_t)(*u_stride * uv_h));
        if (!*u_plane) { free(*y_plane); dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

        *v_stride = *u_stride;
        *v_plane = (unsigned char *)malloc((size_t)(*v_stride * uv_h));
        if (!*v_plane) { free(*y_plane); free(*u_plane); dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }
    }

    /* Copy Y plane (convert from 16-bit/10-bit to 8-bit if needed) */
    for (i = 0; i < *height; i++) {
        int si;
        for (si = 0; si < *width; si++) {
            if (pic.p.bpc > 8) {
                uint16_t *src = (uint16_t *)((uint8_t *)pic.data[0] + i * pic.stride[0]);
                (*y_plane)[i * *y_stride + si] = (unsigned char)(src[si] >> (pic.p.bpc - 8));
            } else {
                (*y_plane)[i * *y_stride + si] = ((unsigned char *)pic.data[0])[i * pic.stride[0] + si];
            }
        }
    }

    /* Copy U plane */
    {
        int uv_h = (*height + (1 << *subsampling_y) - 1) >> *subsampling_y;
        int uv_w = (*width + (1 << *subsampling_x) - 1) >> *subsampling_x;
        for (i = 0; i < uv_h; i++) {
            int si;
            for (si = 0; si < uv_w; si++) {
                if (pic.p.bpc > 8) {
                    uint16_t *src = (uint16_t *)((uint8_t *)pic.data[1] + i * pic.stride[1]);
                    (*u_plane)[i * *u_stride + si] = (unsigned char)(src[si] >> (pic.p.bpc - 8));
                } else {
                    (*u_plane)[i * *u_stride + si] = ((unsigned char *)pic.data[1])[i * pic.stride[1] + si];
                }
            }
        }
    }

    /* Copy V plane */
    {
        int uv_h = (*height + (1 << *subsampling_y) - 1) >> *subsampling_y;
        int uv_w = (*width + (1 << *subsampling_x) - 1) >> *subsampling_x;
        for (i = 0; i < uv_h; i++) {
            int si;
            for (si = 0; si < uv_w; si++) {
                if (pic.p.bpc > 8) {
                    uint16_t *src = (uint16_t *)((uint8_t *)pic.data[2] + i * pic.stride[1]);
                    (*v_plane)[i * *v_stride + si] = (unsigned char)(src[si] >> (pic.p.bpc - 8));
                } else {
                    (*v_plane)[i * *v_stride + si] = ((unsigned char *)pic.data[2])[i * pic.stride[1] + si];
                }
            }
        }
    }

    dav1d_picture_unref(&pic);
    dav1d_close(&ctx);
    return 1;
}
#endif /* STB_AVIF_USE_DAV1D */

#ifndef STB_AVIF_USE_DAV1D
/* -------------------------------------------------------------------------- */
/* SCALAR AV1 DECODER WITH RECON HOOKS  (C89)                                   */
/* -------------------------------------------------------------------------- */
struct stb_avif_scalar_recon {
    /* Planes are stbv_u16 regardless of bit depth; strides are in pixels. */
    stbv_u16 *plane_y;
    stbv_u16 *plane_u;
    stbv_u16 *plane_v;
    int stride_y;
    int stride_u;
    int stride_v;
    int bit_depth;
    int ss_hor;
    int ss_ver;
    int frame_w;
    int frame_h;
    stbv_i32 cf[4096];
    stbv_u16 pred[128 * 128];
    int cur_bx4;
    int cur_by4;
    int cur_bw4;
    int cur_bh4;
    int y_mode;
    int y_angle;
    int uv_angle;
    int uv_mode;
    int block_skip;
    int cfl_alpha_u;
    int cfl_alpha_v;
    int has_chroma;
    int pal_y, pal_uv;
    /* dav1d intra prediction flags: seq_hdr->intra_edge_filter plus the
     * SMOOTH-neighbour flag (ANGLE_SMOOTH_EDGE_FLAG=512) and
     * ANGLE_USE_EDGE_FILTER_FLAG=1024 ORed into the angle argument. */
    int intra_edge_filter;
    int sb_step4; /* superblock step in 4x4 units: 32 for sb128, else 16 */
    const stbv_u8 *above_mode;
    const stbv_u8 *left_mode;
    unsigned int above_n;
    unsigned int left_n;
    const stbv_u8 *above_uvmode;
    const stbv_u8 *left_uvmode;
    /* dav1d CFL: one block-wide AC array shared by both chroma planes. */
    stbv_i16 cfl_ac[32 * 32];
    int cfl_ac_w, cfl_ac_h;
    int cfl_ac_bx, cfl_ac_by;
    int cfl_ac_ok;
    int cur_pl;
    int cur_ltw4, cur_lth4; /* block's max luma tx size (b->tx dims) */
    /* Deblocking: per-4x4-unit block identity + covering-transform
     * log2-width maps (frame-sized, filled during recon). */
    stbv_u32 *lf_blkid;
    stbv_u8 *lf_txlw;
    stbv_u32 *lf_blkid_c;   /* chroma-plane coverage (separate set) */
    stbv_u8 *lf_txlw_c;
    stbv_u8 *lf_done;       /* per-4x4-unit reconstruction bitmap (luma) */
    int lf_mapw4, lf_maph4;
    ptrdiff_t lf_b4stride;
    int tile_x4, tile_y4, tile_w4, tile_h4;
    /* IBC reconstruction state. */
    int is_ibc;
    int ibc_mv_y, ibc_mv_x;
};

/* Decoding-order key for availability checks: superblock row, then SB
 * column, then Morton/z-order inside the SB (AV1 decode order). */
static int stb_avif_recon_decoded_before(int qx4, int qy4,
                                         int cx4, int cy4,
                                         int sb_step4)
{
    int log2sb = sb_step4 == 32 ? 5 : 4;
    int b;
    unsigned int zq = 0, zc = 0;
    if ((qy4 >> log2sb) != (cy4 >> log2sb))
        return (qy4 >> log2sb) < (cy4 >> log2sb);
    if ((qx4 >> log2sb) != (cx4 >> log2sb))
        return (qx4 >> log2sb) < (cx4 >> log2sb);
    qx4 &= sb_step4 - 1; qy4 &= sb_step4 - 1;
    cx4 &= sb_step4 - 1; cy4 &= sb_step4 - 1;
    for (b = 0; b < log2sb; b++) {
        zq |= ((unsigned)(qx4 >> b) & 1u) << (2 * b) |
              ((unsigned)(qy4 >> b) & 1u) << (2 * b + 1);
        zc |= ((unsigned)(cx4 >> b) & 1u) << (2 * b) |
              ((unsigned)(cy4 >> b) & 1u) << (2 * b + 1);
    }
    return zq < zc;
}

/* Per-txb EDGE flags following dav1d recon_b_intra.  Top-right pixels sit
 * on the row above (decoded earlier unless in a right-hand SB of this SB
 * row); bottom-left follows decoding order.  Values use our port's
 * STBV_AV1_EDGE_I444_* convention: bit0 TOP_HAS_RIGHT, bit1 LEFT_HAS_BOTTOM. */
static int stb_avif_recon_have_left(const struct stb_avif_scalar_recon *rc,
                                       int luma, int x4)
{
    int tx0 = luma ? rc->tile_x4 : (rc->tile_x4 >> rc->ss_hor);
    return x4 > tx0;
}

static int stb_avif_recon_have_top(const struct stb_avif_scalar_recon *rc,
                                   int luma, int y4)
{
    int ty0 = luma ? rc->tile_y4 : (rc->tile_y4 >> rc->ss_ver);
    return y4 > ty0;
}

static int stb_avif_recon_block_edge_flags(struct stb_avif_scalar_recon *rc,
                                           int luma, int x4, int y4,
                                           int w4, int h4);
static int stb_avif_recon_block_edge_flags_run(struct stb_avif_scalar_recon *rc,
                                               int luma, int x4, int y4,
                                               int w4, int h4,
                                               int tr_run, int bl_run);

static int stb_avif_recon_txb_edge_flags(struct stb_avif_scalar_recon *rc,
                                         int luma, int bx4, int by4,
                                         int bw4, int bh4,
                                         int tx4, int ty4, int tw4, int th4)
{
    const int ss_hor = luma ? 0 : rc->ss_hor;
    const int ss_ver = luma ? 0 : rc->ss_ver;
    /* dav1d splits each block into 64x64 quadrants (init += 16 luma units)
     * and evaluates per-txb flags against the QUADRANT base, not the
     * per-txb offset. */
    const int qw = luma ? 16 : 16 >> ss_hor;
    const int qh = luma ? 16 : 16 >> ss_ver;
    int blk;
    int xl, yl, qxl, qyl, sub_w4, sub_h4;
    int sb_has_tr, sb_has_bl, fl = 0;
    /* txb callbacks carry block coordinates in luma 4x4 units even for
     * chroma. Convert to chroma: floor division for origin (matching dav1d's
     * init_x >> ss_hor), ceiling division for extent. */
    if (!luma) {
        bx4 = bx4 >> ss_hor;
        by4 = by4 >> ss_ver;
        bw4 = (bw4 + ss_hor) >> ss_hor;
        bh4 = (bh4 + ss_ver) >> ss_ver;
    }
    /* Block-level availability == dav1d's decode_b intra_edge_flags. */
    blk = stb_avif_recon_block_edge_flags_run(rc, luma, bx4, by4,
                                               bw4, bh4, tw4, th4);
    xl = tx4 - bx4; yl = ty4 - by4;
    qxl = xl / qw * qw;
    qyl = yl / qh * qh;
    sub_w4 = bw4 < qxl + qw ? bw4 : qxl + qw;
    sub_h4 = bh4 < qyl + qh ? bh4 : qyl + qh;

    sb_has_tr = (qxl + qw < bw4) ? 1 :
                (qyl ? 0 :
                 (blk & STBV_AV1_EDGE_I444_TOP_HAS_RIGHT ? 1 : 0));
    sb_has_bl = qxl ? 0 :
                ((qyl + qh < bh4) ? 1 :
                 (blk & STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM ? 1 : 0));

    /* dav1d recon_b_intra: sub_w4/sub_h4 are quadrant-relative extents
     * (already include qxl/qyl), so the comparisons must NOT add them
     * again.  Double-adding made every lower/left-quadrant txb think the
     * bottom-left / top-right neighbour was available when it was not,
     * and Z3 then blended unwritten zero pixels into the prediction
     * (the dark-triangle / green-fog artifacts). */
    if (!((yl > qyl || !sb_has_tr) && (xl + tw4 >= sub_w4)))
        fl |= STBV_AV1_EDGE_I444_TOP_HAS_RIGHT;
    if (!(xl > qxl || (!sb_has_bl && yl + th4 >= sub_h4)))
        fl |= STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM;
    return fl;
}

/* Block-level variant (whole-block prediction path). */
static int stb_avif_recon_block_edge_flags(struct stb_avif_scalar_recon *rc,
                                           int luma, int x4, int y4,
                                           int w4, int h4)
{
    return stb_avif_recon_block_edge_flags_run(rc, luma, x4, y4, w4, h4,
                                               w4, h4);
}

/* Run-aware variant: tr_run / bl_run give the number of 4x4 units the
 * predictor will actually read beyond the edge (the transform extent,
 * not necessarily the whole block). */
static int stb_avif_recon_block_edge_flags_run(struct stb_avif_scalar_recon *rc,
                                               int luma, int x4, int y4,
                                               int w4, int h4,
                                               int tr_run, int bl_run)
{
    const int ss_hor = luma ? 0 : rc->ss_hor;
    const int ss_ver = luma ? 0 : rc->ss_ver;
    const int fw4a = (rc->frame_w + 7) & ~7;
    const int fh4a = (rc->frame_h + 7) & ~7;
    const int fw4 = luma ? (fw4a >> 2) : ((fw4a >> 2) + ss_hor) >> ss_hor;
    const int fh4 = luma ? (fh4a >> 2) : ((fh4a >> 2) + ss_ver) >> ss_ver;
    stbv_u32 *cmap = luma ? 0 : rc->lf_blkid_c;
    int tile_x0 = luma ? rc->tile_x4 : (rc->tile_x4 >> ss_hor);
    int tile_y0 = luma ? rc->tile_y4 : (rc->tile_y4 >> ss_ver);
    int tile_x1 = luma ? (rc->tile_x4 + rc->tile_w4) :
        ((rc->tile_x4 + rc->tile_w4 + ss_hor) >> ss_hor);
    int tile_y1 = luma ? (rc->tile_y4 + rc->tile_h4) :
        ((rc->tile_y4 + rc->tile_h4 + ss_ver) >> ss_ver);
    int fl = 0;

    /* Coordinates are in the plane being predicted. Chroma coverage is
     * stored on the luma 4x4 grid, so a chroma unit maps to a 2x2 luma
     * footprint for 4:2:0. */
    if (x4 < 0 || y4 < 0 || x4 >= fw4 || y4 >= fh4)
        return 0;

    if (y4 > tile_y0 && x4 + tr_run < fw4 && x4 + tr_run < tile_x1) {
        int qx = x4 + w4;
        int qy = y4 - 1;
        if (luma) {
            if (rc->lf_done && qx >= 0 && qx < rc->lf_mapw4 &&
                qy >= 0 && qy < rc->lf_maph4 &&
                rc->lf_done[(size_t)qy * rc->lf_b4stride + qx])
                fl |= STBV_AV1_EDGE_I444_TOP_HAS_RIGHT;
        } else if (cmap) {
            int mx = qx << ss_hor;
            int my = qy << ss_ver;
            if (mx >= 0 && mx < rc->lf_mapw4 &&
                my >= 0 && my < rc->lf_maph4 &&
                cmap[(size_t)my * rc->lf_b4stride + mx] != 0xffffffffU)
                fl |= STBV_AV1_EDGE_I444_TOP_HAS_RIGHT;
        }
    }
    if (x4 > tile_x0 && y4 + bl_run < fh4 && y4 + bl_run < tile_y1) {
        int qx = x4 - 1;
        int qy = y4 + h4;
        if (luma) {
            if (rc->lf_done && qx >= 0 && qx < rc->lf_mapw4 &&
                qy >= 0 && qy < rc->lf_maph4 &&
                rc->lf_done[(size_t)qy * rc->lf_b4stride + qx])
                fl |= STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM;
        } else if (cmap) {
            int mx = qx << ss_hor;
            int my = qy << ss_ver;
            if (mx >= 0 && mx < rc->lf_mapw4 &&
                my >= 0 && my < rc->lf_maph4 &&
                cmap[(size_t)my * rc->lf_b4stride + mx] != 0xffffffffU)
                fl |= STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM;
        }
    }
    return fl;
}

/* dav1d sm_flag()/sm_uv_flag(): ANGLE_SMOOTH_EDGE_FLAG when the neighbour
 * block (intra) uses one of the SMOOTH modes. */
static int stb_avif_recon_smooth_flag(const stbv_u8 *arr, unsigned int n,
                                      int idx)
{
    int m;
    if (!arr || (unsigned)idx >= n) return 0;
    m = arr[idx];
    return (m == STBV_AV1_INTRA_SMOOTH || m == STBV_AV1_INTRA_SMOOTH_V ||
            m == STBV_AV1_INTRA_SMOOTH_H) ? 512 : 0;
}

static int stb_avif_recon_edge_flags(struct stb_avif_scalar_recon *rc,
                                     int luma, int x4, int y4)
{
    int fl = rc->intra_edge_filter << 10;
    if (luma) {
        fl |= stb_avif_recon_smooth_flag(rc->above_mode, rc->above_n, x4);
        fl |= stb_avif_recon_smooth_flag(rc->left_mode, rc->left_n, y4);
    } else {
        fl |= stb_avif_recon_smooth_flag(rc->above_uvmode,
                                         rc->ss_hor ? (rc->above_n + 1) >> 1
                                                    : rc->above_n, x4);
        fl |= stb_avif_recon_smooth_flag(rc->left_uvmode,
                                         rc->ss_ver ? (rc->left_n + 1) >> 1
                                                    : rc->left_n, y4);
    }
    return fl;
}

/* Full-block intra prediction, written straight into the frame planes.
 * Runs at block_info time (before coefficients) so skipped transforms keep
 * a valid prediction; txb callbacks then add residual on top.
 * NOTE: stbv_av1_prepare_intra_edges_8 takes x/y/w/h in 4x4 units. */
static void stb_avif_recon_predict_block(struct stb_avif_scalar_recon *rc,
                                         int ss_hor, int ss_ver,
                                         int bx4, int by4, int bw4, int bh4,
                                         int has_chroma,
                                         int y_mode, int y_angle, int uv_mode)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    /* 8-aligned extent (dav1d f->bw/f->bh): prediction edge prep must
     * reach the reconstructed padded row/column. */
    const int fw4 = (rc->frame_w + 7) >> 3 << 1;
    const int fh4 = (rc->frame_h + 7) >> 3 << 1;
    int bw4c, bh4c, i;

    bw4c = fw4 - bx4; if (bw4c > bw4) bw4c = bw4;
    bh4c = fh4 - by4; if (bh4c > bh4) bh4c = bh4;
    if (bw4c <= 0 || bh4c <= 0) return;

    /* Luma prediction */
    {
        int x = bx4 << 2;
        int y = by4 << 2;
        int w = bw4 << 2;
        int h = bh4 << 2;
        int cw, ch;
        int mode = y_mode;
        int angle = y_angle;
        /* Intra-mode FILTER shares numeric 14 with IPRED_TOP_DC; map to the
         * real ipred FILTER and carry the filter-set index (y_angle). */
        int bd = rc->bit_depth;
        int impl;
        const int filt_idx = (mode == STBV_AV1_INTRA_FILTER) ? y_angle : 0;

        cw = rc->frame_w - x; if (cw > w) cw = w;
        ch = rc->frame_h - y; if (ch > h) ch = h;
        if (mode == STBV_AV1_INTRA_FILTER)
            mode = STBV_AV1_IPRED_FILTER;
        if (cw <= 0 || ch <= 0) return;
                impl = stbv_av1_prepare_intra_edges_16(bx4, stb_avif_recon_have_left(rc, 1, bx4),
                                              by4, stb_avif_recon_have_top(rc, 1, by4),
                                              fw4, fh4,
                                              stb_avif_recon_block_edge_flags(rc, 1, bx4, by4, bw4c, bh4c),
                                              rc->plane_y + y * rc->stride_y + x,
                                              rc->stride_y, NULL,
                                              mode, &angle,
                                              bw4c, bh4c,
                                               rc->intra_edge_filter, edge, bd);
        stbv_av1_ipred_run_16(impl, rc->pred, w, edge, w, h,
                             angle | stb_avif_recon_edge_flags(rc, 1, bx4, by4),
                             filt_idx, rc->frame_w - x, rc->frame_h - y, bd);
        for (i = 0; i < ch; i++)
            memcpy(rc->plane_y + (y + i) * rc->stride_y + x,
                   rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
    }

    /* Chroma prediction (chroma coords are the luma ones shifted) */
    if (has_chroma && rc->plane_u && rc->plane_v && uv_mode >= 0)
    {
        const int cfw4 = (fw4 + ss_hor) >> ss_hor;
        const int cfh4 = (fh4 + ss_ver) >> ss_ver;
        int cx4 = bx4 >> ss_hor;
        int cy4 = by4 >> ss_ver;
        int cbw4 = (bw4c + ss_hor) >> ss_hor;
        int cbh4 = (bh4c + ss_ver) >> ss_ver;
        int cm = uv_mode == STBV_AV1_INTRA_CFL ? STBV_AV1_INTRA_DC : uv_mode;
        int cangle = 0;
        int cimpl;
        int x = cx4 << 2;
        int y = cy4 << 2;
        int w = cbw4 << 2;
        int h = cbh4 << 2;
        int cw, ch;
        if (cbw4 <= 0 || cbh4 <= 0 || cx4 >= cfw4 || cy4 >= cfh4) return;
        cbw4 = cfw4 - cx4; if (cbw4 > (w >> 2)) cbw4 = w >> 2;
        cbh4 = cfh4 - cy4; if (cbh4 > (h >> 2)) cbh4 = h >> 2;
        if (cbw4 <= 0 || cbh4 <= 0) return;
        w = cbw4 << 2;
        h = cbh4 << 2;
        cw = ((rc->frame_w + ss_hor) >> ss_hor) - x; if (cw > w) cw = w;
        ch = ((rc->frame_h + ss_ver) >> ss_ver) - y; if (ch > h) ch = h;
        if (cw <= 0 || ch <= 0) return;
    /* Predict U and V separately: each plane has different reference edges. */
    {
        int pl_idx;
        for (pl_idx = 0; pl_idx < 2; pl_idx++) {
            stbv_u16 *cur_plane = pl_idx == 0 ? rc->plane_u : rc->plane_v;
            int cur_stride = pl_idx == 0 ? rc->stride_u : rc->stride_v;
            cimpl = stbv_av1_prepare_intra_edges_16(cx4, stb_avif_recon_have_left(rc, 0, cx4),
                                                   cy4, stb_avif_recon_have_top(rc, 0, cy4),
                                                   cfw4, cfh4,
                                                   stb_avif_recon_block_edge_flags(rc, 0, cx4, cy4, cbw4, cbh4),
                                                   cur_plane + y * cur_stride + x,
                                                   cur_stride, NULL,
                                                   cm, &cangle,
                                                   cbw4, cbh4,
                                                   rc->intra_edge_filter,
                                                   edge, rc->bit_depth);
            stbv_av1_ipred_run_16(cimpl, rc->pred, w, edge, w, h,
                                  cangle | stb_avif_recon_edge_flags(rc, 0, bx4, by4),
                                  0, (cfw4 - cx4) << 2, (cfh4 - cy4) << 2, rc->bit_depth);
            for (i = 0; i < ch; i++) {
                memcpy(cur_plane + (y + i) * cur_stride + x,
                       rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
            }
        }
    }
    }
}

static void stb_avif_recon_block_info(void *ud, int intra, int bs, int bx4, int by4, int has_chroma, int cbw4, int cbh4, int uv_tx, int tx0, int pal_sz_y, int pal_sz_uv, int skip, int y_mode, int y_angle, int uv_mode, int uv_angle, int cfl_alpha_u, int cfl_alpha_v, int ibc_mv_y, int ibc_mv_x)
{
    struct stb_avif_scalar_recon *rc;
    int bw4, bh4;
    (void)cbw4; (void)cbh4; (void)uv_tx;
    (void)pal_sz_y; (void)pal_sz_uv;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    /* Always record position so luma_txb/luma_pal can fill lf_blkid. */
    rc->cur_bx4 = bx4;
    rc->cur_by4 = by4;
    bw4 = stbv_av1_block_dimensions[bs][0];
    bh4 = stbv_av1_block_dimensions[bs][1];
    rc->cur_bw4 = bw4;
    rc->cur_bh4 = bh4;
    rc->cur_ltw4 = stbv_av1_tx_dims[tx0 >= 0 && tx0 < STBV_AV1_N_TX_SIZES
                                   ? tx0 : 0].w;
    rc->cur_lth4 = stbv_av1_tx_dims[tx0 >= 0 && tx0 < STBV_AV1_N_TX_SIZES
                                   ? tx0 : 0].h;
    rc->block_skip = skip;
    rc->has_chroma = has_chroma;
    if (!intra) {
        /* IBC block: copy already-reconstructed pixels from the current
         * frame at the reference position indicated by the MV.  MV is in
         * 1/8-pel luma units; for IBC it is always integer-pixel aligned
         * (mv_prec=-1). */
        int src_px_x, src_px_y;
        int dst_px_x, dst_px_y;
        int pw, ph, cw, ch;
        int ss_h = rc->ss_hor, ss_v = rc->ss_ver;
        int i;
        rc->is_ibc = 1;
        rc->ibc_mv_y = ibc_mv_y;
        rc->ibc_mv_x = ibc_mv_x;
        rc->y_mode = STBV_AV1_INTRA_DC;
        rc->y_angle = 0;
        rc->uv_mode = STBV_AV1_INTRA_DC;
        rc->uv_angle = 0;
        rc->cfl_alpha_u = 0;
        rc->cfl_alpha_v = 0;
        rc->pal_y = 0;
        rc->pal_uv = 0;
        /* Source position in pixel coords (matching dav1d mc: dx = bx*4 + mvx>>3). */
        src_px_x = bx4 * 4 + (ibc_mv_x >> 3);
        src_px_y = by4 * 4 + (ibc_mv_y >> 3);
        dst_px_x = bx4 * 4;
        dst_px_y = by4 * 4;
        pw = bw4 << 2;
        ph = bh4 << 2;
        /* Clamp to frame bounds. */
        if (src_px_x < 0) { pw += src_px_x; src_px_x = 0; }
        if (src_px_y < 0) { ph += src_px_y; src_px_y = 0; }
        if (src_px_x + pw > rc->frame_w) pw = rc->frame_w - src_px_x;
        if (src_px_y + ph > rc->frame_h) ph = rc->frame_h - src_px_y;
        cw = rc->frame_w - dst_px_x; if (cw > pw) cw = pw;
        ch = rc->frame_h - dst_px_y; if (ch > ph) ch = ph;
        /* Reference-availability check: source must be within decoded
         * region (above or to the left of current position).  AV1 decode
         * order guarantees this when src is within the current tile. */
        if (cw > 0 && ch > 0 && rc->plane_y) {
            for (i = 0; i < ch; i++)
                memmove(rc->plane_y + (size_t)(dst_px_y + i) * rc->stride_y + dst_px_x,
                       rc->plane_y + (size_t)(src_px_y + i) * rc->stride_y + src_px_x,
                       (size_t)cw * sizeof(stbv_u16));
        }
        /* Copy chroma if present. */
        if (has_chroma && rc->plane_u && rc->plane_v) {
            int cbx4_dst = bx4 >> ss_h;
            int cby4_dst = by4 >> ss_v;
            int cx_dst = cbx4_dst * 4;
            int cy_dst = cby4_dst * 4;
            int luma_src_x = cbx4_dst * (4 << ss_h) + (ibc_mv_x >> 3);
            int luma_src_y = cby4_dst * (4 << ss_v) + (ibc_mv_y >> 3);
            int cx_src = luma_src_x >> ss_h;
            int cy_src = luma_src_y >> ss_v;
            int cw4 = (bw4 + ss_h) >> ss_h;
            int ch4 = (bh4 + ss_v) >> ss_v;
            int cpw = cw4 << 2;
            int cph = ch4 << 2;
            int ccw, cch, j;
            if (cx_src < 0) { cpw += cx_src; cx_src = 0; }
            if (cy_src < 0) { cph += cy_src; cy_src = 0; }
            ccw = ((rc->frame_w + ss_h) >> ss_h) - cx_dst; if (ccw > cpw) ccw = cpw;
            cch = ((rc->frame_h + ss_v) >> ss_v) - cy_dst; if (cch > cph) cch = cph;
            if (ccw > 0 && cch > 0) {
                for (j = 0; j < 2; j++) {
                    stbv_u16 *plane = j == 0 ? rc->plane_u : rc->plane_v;
                    int stride = j == 0 ? rc->stride_u : rc->stride_v;
                    if (!plane) continue;
                    for (i = 0; i < cch; i++)
                        memmove(plane + (size_t)(cy_dst + i) * stride + cx_dst,
                               plane + (size_t)(cy_src + i) * stride + cx_src,
                               (size_t)ccw * sizeof(stbv_u16));
                }
            }
        }
        /* Fill lf_blkid for IBC skip blocks (same logic as intra skip). */
        if (skip && rc->lf_blkid && bw4 > 0 && bh4 > 0) {
            stbv_u32 blkid = ((stbv_u32)bx4 << 16) | (stbv_u32)by4;
            int ii, jj;
            for (ii = 0; ii < bh4 && (by4 + ii) < rc->lf_maph4; ii++)
                for (jj = 0; jj < bw4 && (bx4 + jj) < rc->lf_mapw4; jj++) {
                    size_t off = (size_t)(by4 + ii) * rc->lf_b4stride + (bx4 + jj);
                    rc->lf_blkid[off] = blkid;
                    rc->lf_txlw[off] = rc->cur_ltw4;
                    rc->lf_done[off] = 1;
                }
        }
        if (skip && has_chroma && rc->lf_blkid_c && bw4 > 0 && bh4 > 0) {
            int cbx4 = bx4 >> ss_h;
            int cby4 = by4 >> ss_v;
            int cbw4_u = (bw4 + ss_h) >> ss_h;
            int cbh4_u = (bh4 + ss_v) >> ss_v;
            int ii, jj;
            for (ii = 0; ii < cbh4_u && (cby4 + ii) < rc->lf_maph4; ii++)
                for (jj = 0; jj < cbw4_u && (cbx4 + jj) < rc->lf_mapw4; jj++) {
                    size_t off = (size_t)(cby4 + ii) * rc->lf_b4stride + (cbx4 + jj);
                    rc->lf_blkid_c[off] = ((stbv_u32)cbx4 << 16) | (stbv_u32)cby4;
                    rc->lf_txlw_c[off] = rc->cur_ltw4;
                }
        }
        return;
    }
    rc->is_ibc = 0;
    rc->y_mode = y_mode;
    rc->y_angle = y_angle;
    rc->uv_mode = uv_mode;
    rc->uv_angle = uv_angle;
    rc->cfl_alpha_u = cfl_alpha_u;
    rc->cfl_alpha_v = cfl_alpha_v;
    rc->block_skip = skip;
    rc->has_chroma = has_chroma;
    rc->pal_y = pal_sz_y;
    rc->pal_uv = pal_sz_uv;
    /* dav1d predicts PER TRANSFORM so every txb sees freshly reconstructed
     * neighbours; the txb callbacks do that.  Whole-block-skip blocks never
     * reach the txb callbacks, so predict them in one shot here (no residual
     * will be interleaved). */
    if (skip && !pal_sz_y)
        stb_avif_recon_predict_block(rc, rc->ss_hor, rc->ss_ver,
                                     bx4, by4, rc->cur_bw4, rc->cur_bh4,
                                     has_chroma,
                                     y_mode, y_angle, uv_mode);
    /* Skip blocks never reach luma_txb/chroma_txb, so their lf_blkid map
     * entries would stay zero (calloc default), colliding with blkid=0 at
     * (0,0) and creating false deblocking edges.  Fill the map here. */
    if (skip && rc->lf_blkid && rc->cur_bw4 > 0 && rc->cur_bh4 > 0) {
        stbv_u32 blkid = ((stbv_u32)bx4 << 16) | (stbv_u32)by4;
        int i, j;
        for (i = 0; i < rc->cur_bh4 && (by4 + i) < rc->lf_maph4; i++)
            for (j = 0; j < rc->cur_bw4 && (bx4 + j) < rc->lf_mapw4; j++) {
                size_t off = (size_t)(by4 + i) * rc->lf_b4stride + (bx4 + j);
                rc->lf_blkid[off] = blkid;
                rc->lf_txlw[off] = rc->cur_ltw4;
                rc->lf_done[off] = 1;
            }
    }
    if (skip && has_chroma && rc->lf_blkid_c && rc->cur_bw4 > 0 && rc->cur_bh4 > 0) {
        int cbx4 = bx4 >> rc->ss_hor;
        int cby4 = by4 >> rc->ss_ver;
        int cbw4_u = (rc->cur_bw4 + rc->ss_hor) >> rc->ss_hor;
        int cbh4_u = (rc->cur_bh4 + rc->ss_ver) >> rc->ss_ver;
        int i, j;
        for (i = 0; i < cbh4_u && (cby4 + i) < rc->lf_maph4; i++)
            for (j = 0; j < cbw4_u && (cbx4 + j) < rc->lf_mapw4; j++) {
                size_t off = (size_t)(cby4 + i) * rc->lf_b4stride + (cbx4 + j);
                rc->lf_blkid_c[off] = ((stbv_u32)cbx4 << 16) | (stbv_u32)cby4;
                rc->lf_txlw_c[off] = rc->cur_ltw4;
            }
    }
}

    /* Per-transform-block intra prediction written into the plane.
     * px4/py4/tw4/th4 are 4x4 units in the given plane's coordinate system;
     * pw/ph are the plane's pixel dimensions. */
    /* Palette blocks own their plane pixels: dav1d applies the palette
     * instead of intra prediction (recon_b_intra goto skip_y_pred), so
     * never let txb prediction clobber it here. */
static void stb_avif_recon_pred_rect(struct stb_avif_scalar_recon *rc,
                                     stbv_u16 *plane, int stride,
                                     int px4, int py4, int tw4, int th4,
                                     int pw, int ph,
                                     int mode_in, int angle_in)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    const int fw4 = (pw + 3) >> 2;
    const int fh4 = (ph + 3) >> 2;
    int w = tw4 << 2;
    int h = th4 << 2;
    int cw, ch, i;
    int mode = mode_in;
    int angle = angle_in;
    int impl;
    const int filt_idx = (mode == STBV_AV1_INTRA_FILTER) ? angle : 0;
    if (mode == STBV_AV1_INTRA_FILTER)
        mode = STBV_AV1_IPRED_FILTER;

    if (tw4 <= 0 || th4 <= 0 || px4 >= fw4 || py4 >= fh4) return;
    if (tw4 > fw4 - px4) tw4 = fw4 - px4;
    if (th4 > fh4 - py4) th4 = fh4 - py4;
    cw = pw - (px4 << 2); if (cw > w) cw = w;
    ch = ph - (py4 << 2); if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) return;

    impl = stbv_av1_prepare_intra_edges_16(px4, stb_avif_recon_have_left(rc, 1, px4),
                                          py4, stb_avif_recon_have_top(rc, 1, py4),
                                          fw4, fh4, 0,
                                          plane + (py4 << 2) * stride + (px4 << 2),
                                          stride, NULL,
                                          mode, &angle,
                                          tw4, th4, 0, edge, rc->bit_depth);
    stbv_av1_ipred_run_16(impl, rc->pred, w, edge, w, h, angle, filt_idx,
                         w, h, rc->bit_depth);
    for (i = 0; i < ch; i++)
        memcpy(plane + ((py4 << 2) + i) * stride + (px4 << 2),
               rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
}

/* Residual add: copy plane region to scratch, inverse-transform on top of it
 * (stbv_av1_inv_txfm_add8 adds residual in place), copy back clipped. */
static void stb_avif_recon_add_res(struct stb_avif_scalar_recon *rc,
                                   stbv_u16 *plane, int stride,
                                   int px, int py, int pw, int ph,
                                   int tx, int txtp, int eob, stbv_i32 *cf)
{
    int w = stbv_av1_tx_dims[tx].w << 2;
    int h = stbv_av1_tx_dims[tx].h << 2;
    int cw, ch, i;
    /* Clip against BUFFER capacity (stride/rows incl. padding), not the
     * visible frame: reconstruction must fill the full padded tx extent so
     * CFL AC gather on adjacent edge blocks never reads zero padding.
     * ph is the ALLOCATED row count (visible + pad). */
    cw = (stride - px); if (cw > w) cw = w;
    ch = ph - py; if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) return;
    for (i = 0; i < ch; i++) {
        memcpy(rc->pred + i * w, plane + (py + i) * stride + px,
               (size_t)(cw * sizeof(stbv_u16)));
        memset(rc->pred + i * w + cw, 0,
                              (size_t)((w - cw) * sizeof(stbv_u16)));
    }


    stbv_av1_inv_txfm_add16(rc->pred, w, cf, eob, tx, txtp, rc->bit_depth);
    for (i = 0; i < ch; i++)
        memcpy(plane + (py + i) * stride + px, rc->pred + i * w,
               (size_t)(cw * sizeof(stbv_u16)));
}

static void stb_avif_recon_predict_txb_luma(struct stb_avif_scalar_recon *rc, int x4, int y4, int tx);
static void stb_avif_recon_predict_txb_chroma(struct stb_avif_scalar_recon *rc, int pl, int x4, int y4, int tx);

static void stb_avif_extend_right_edge_u16(stbv_u16 *plane, int stride,
                                           int frame_w, int frame_h,
                                           int x4, int y4, int tx)
{
    int x0 = x4 << 2;
    int y0 = y4 << 2;
    int tw = stbv_av1_tx_dims[tx].w << 2;
    int th = stbv_av1_tx_dims[tx].h << 2;
    int aw = (frame_w + 7) & ~7;
    int yy, xx;
    if (!plane || x0 + tw < frame_w || x0 >= aw || y0 >= frame_h)
        return;
    if (y0 + th > frame_h) th = frame_h - y0;
    for (yy = 0; yy < th; yy++) {
        stbv_u16 v = plane[(size_t)(y0 + yy) * stride + frame_w - 1];
        for (xx = frame_w; xx < aw; xx++)
            plane[(size_t)(y0 + yy) * stride + xx] = v;
    }
}

static void stb_avif_recon_luma_txb(void *ud, int x4, int y4, int tx, int txtp, int eob, stbv_i32 *cf)
{
    struct stb_avif_scalar_recon *rc;
    int txw4 = stbv_av1_tx_dims[tx].w;
    int txh4 = stbv_av1_tx_dims[tx].h;
    (void)eob;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    (void)txw4; (void)txh4;
#ifdef STB_AVIF_PRED_ONLY
    (void)cf; (void)tx; (void)txtp;
#else
#ifndef STB_AVIF_NO_RESIDUAL
    /* Per-transform prediction from currently reconstructed neighbours
     * (dav1d recon_b_intra: intra_pred -> coefs -> itxfm_add).
     * Intra-frame "skip" suppresses only the residual; the prediction
     * itself must always be written.
     * For IBC blocks the reference pixels were already copied into the
     * plane by block_info, so skip intra prediction. */
    if (!rc->pal_y && !rc->is_ibc) {
        stb_avif_recon_predict_txb_luma(rc, x4, y4, tx);
        {
            int _w = stbv_av1_tx_dims[tx].w << 2;
            int _h = stbv_av1_tx_dims[tx].h << 2;
            int _cw = rc->stride_y - (x4 << 2);
            int _ch = (rc->frame_h + 64) - (y4 << 2);
            int _i;
            if (_cw > _w) _cw = _w;
            if (_ch > _h) _ch = _h;
            if (_cw > 0 && _ch > 0) {
                for (_i = 0; _i < _ch; _i++)
                    memcpy(rc->plane_y + (size_t)((y4 << 2) + _i) * rc->stride_y + (x4 << 2),
                           rc->pred + _i * _w, (size_t)(_cw * sizeof(stbv_u16)));
            }
        }
    }
    /* eob is dav1d-style 0-based LAST-coefficient index: 0 == DC-only
     * (coefficients present!), < 0 == none. */
    if (!rc->block_skip && eob >= 0)
    {
        stb_avif_recon_add_res(rc, rc->plane_y, rc->stride_y,
                               x4 << 2, y4 << 2, rc->frame_w,
                               rc->frame_h + 64,
                                 tx, txtp, eob, cf);
    }
    {
        /* record transform coverage for the deblocking pass */
        if (rc->lf_blkid) {
            int tw = stbv_av1_tx_dims[tx].w, th = stbv_av1_tx_dims[tx].h;
            int lx, ly;
            stbv_u32 id = ((stbv_u32)rc->cur_bx4 << 16) |
                          (stbv_u32)rc->cur_by4;
            for (ly = y4; ly < y4 + th && ly < rc->lf_maph4; ly++)
                for (lx = x4; lx < x4 + tw && lx < rc->lf_mapw4; lx++) {
                    rc->lf_blkid[(size_t)ly * rc->lf_b4stride + lx] = id;
                    rc->lf_txlw[(size_t)ly * rc->lf_b4stride + lx] =
                        (stbv_u8)stbv_av1_tx_dims[tx].lw;
                    rc->lf_done[(size_t)ly * rc->lf_b4stride + lx] = 1;
                }
        }
    }
    stb_avif_extend_right_edge_u16(rc->plane_y, rc->stride_y,
                                   rc->frame_w, rc->frame_h, x4, y4, tx);
#endif
#endif
}

/* Per-txb luma prediction straight into the plane. */
static void stb_avif_recon_predict_txb_luma(struct stb_avif_scalar_recon *rc,
                                            int x4, int y4, int tx)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    /* 8-aligned extent (dav1d f->bw/f->bh): prediction edge prep must
     * reach the reconstructed padded row/column. */
    const int fw4 = (rc->frame_w + 7) >> 3 << 1;
    const int fh4 = (rc->frame_h + 7) >> 3 << 1;
    int w = stbv_av1_tx_dims[tx].w << 2;
    int h = stbv_av1_tx_dims[tx].h << 2;
    int cw, ch, i;
    int mode = rc->y_mode;
    int angle = rc->y_angle;
    const int filt_idx = (mode == STBV_AV1_INTRA_FILTER) ? rc->y_angle : 0;
    int bd = rc->bit_depth;
    int impl;
    /* dav1d recon_b_intra hands prepare_intra_edges a snapshot of the row
     * above at every superblock top boundary and that path reads it
     * UNCLIPPED across the padded width; all other blocks read dst[-stride]
     * clipped to the 8-aligned frame width with edge replication.  We are
     * single-threaded and the plane row above is exactly what dav1d would
     * have snapshotted, so gate the same way and pass the row directly. */
    const stbv_u16 *sb_edge =
        (y4 > rc->tile_y4 && !(y4 & (rc->sb_step4 - 1))) ?
            rc->plane_y + (size_t)((y4 << 2) - 1) * rc->stride_y : NULL;

    if (mode == STBV_AV1_INTRA_FILTER)
        mode = STBV_AV1_IPRED_FILTER;
    if (x4 >= fw4 || y4 >= fh4) return;
    /* Write the FULL tx extent into the padded plane: later blocks'
     * intra edges read these pixels (dav1d recon_b_intra does the same).
     * Clip against BUFFER capacity (stride/allocated rows), never against
     * the 8-aligned frame size: clipping here left unwritten columns past
     * right-edge txbs, and the zeroed loads poisoned every subsequent
     * prediction that gathered its top edge across them (the dark
     * triangle / green fog). */
    cw = rc->stride_y - (x4 << 2); if (cw > w) cw = w;
    ch = (rc->frame_h + 64) - (y4 << 2); if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) return;
    impl = stbv_av1_prepare_intra_edges_16(x4, stb_avif_recon_have_left(rc, 1, x4),
                                          y4, stb_avif_recon_have_top(rc, 1, y4),
                                          fw4, fh4,
                                          stb_avif_recon_txb_edge_flags(
                                              rc, 1, rc->cur_bx4, rc->cur_by4,
                                              rc->cur_bw4, rc->cur_bh4,
                                              x4, y4,
                                              stbv_av1_tx_dims[tx].w,
                                              stbv_av1_tx_dims[tx].h),
                                          rc->plane_y + (y4 << 2) * rc->stride_y +
                                          (x4 << 2),
                                          rc->stride_y, sb_edge,
                                          mode, &angle,
                                          stbv_av1_tx_dims[tx].w,
                                          stbv_av1_tx_dims[tx].h,
                                          rc->intra_edge_filter, edge, bd);
    stbv_av1_ipred_run_16(impl, rc->pred, w, edge, w, h,
                          angle | stb_avif_recon_edge_flags(rc, 1, rc->cur_bx4, rc->cur_by4),
                          filt_idx, rc->frame_w - (x4 << 2), rc->frame_h - (y4 << 2), bd);
    for (i = 0; i < ch; i++)
        memcpy(rc->plane_y + ((y4 << 2) + i) * rc->stride_y + (x4 << 2),
               rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
}

static void stb_avif_recon_chroma_txb(void *ud, int pl, int x4, int y4, int tx, int txtp, int eob, stbv_i32 *cf)
{
    struct stb_avif_scalar_recon *rc;
    stbv_u16 *plane;
    int stride, pw, ph;
    int txw4 = stbv_av1_tx_dims[tx].w;
    int txh4 = stbv_av1_tx_dims[tx].h;
    (void)eob;
    rc = (struct stb_avif_scalar_recon *)ud;
    rc->cur_pl = pl;
    if (!rc || !rc->plane_u || !rc->plane_v) return;
    plane = pl == 0 ? rc->plane_u : rc->plane_v;
    stride = pl == 0 ? rc->stride_u : rc->stride_v;
    /* Chroma plane extent follows ACTUAL subsampling: full width for
     * 4:2:2/4:4:4, half height for 4:2:2/4:2:0. Hardcoded >>1 broke
     * 4:2:2 (right half of chroma never written). */
    /* pw unused for clipping now; ph = ALLOCATED chroma rows. */
    pw = (rc->frame_w + rc->ss_hor) >> rc->ss_hor;
    ph = ((rc->frame_h + rc->ss_ver) >> rc->ss_ver) + 32;
    (void)txw4; (void)txh4;
#ifdef STB_AVIF_PRED_ONLY
    (void)cf; (void)tx; (void)txtp;
#else
#ifndef STB_AVIF_NO_RESIDUAL
    /* Prediction always runs for intra; "skip" only suppresses the
     * residual (dav1d recon_b_intra semantics).  eob >= 0: DC-only
     * still carries a coefficient.  UV-palette blocks own the chroma
     * planes instead.  IBC blocks already have reference pixels in the
     * plane (copied by block_info), so skip intra prediction. */
    if (!rc->pal_uv && !rc->is_ibc) {
        stb_avif_recon_predict_txb_chroma(rc, pl, x4, y4, tx);
        /* Copy prediction from rc->pred into the plane so add_res adds
         * residual on top of the prediction, not stale plane data. */
        {
            int _w = stbv_av1_tx_dims[tx].w << 2;
            int _h = stbv_av1_tx_dims[tx].h << 2;
            int _cw = stride - (x4 << 2);
            int _ch = (ph + 32) - (y4 << 2);
            int _i;
            if (_cw > _w) _cw = _w;
            if (_ch > _h) _ch = _h;
            if (_cw > 0 && _ch > 0) {
                for (_i = 0; _i < _ch; _i++)
                    memcpy(plane + (size_t)((y4 << 2) + _i) * stride + (x4 << 2),
                           rc->pred + _i * _w, (size_t)(_cw * sizeof(stbv_u16)));
            }
        }
    }
    if (!rc->block_skip && eob >= 0)
        stb_avif_recon_add_res(rc, plane, stride,
                               x4 << 2, y4 << 2, stride, ph + 32,
                               tx, txtp, eob, cf);
    if (pl == 0)
        stb_avif_extend_right_edge_u16(rc->plane_u, rc->stride_u, pw, ph, x4, y4, tx);
    else
        stb_avif_extend_right_edge_u16(rc->plane_v, rc->stride_v, pw, ph, x4, y4, tx);
    if (pl == 0 && rc->lf_blkid_c && rc->has_chroma) {
        /* record this chroma txb's own extent (mapped to luma units);
         * identity = chroma-plane origin so chroma-internal boundaries
         * remain visible to the deblocker */
        int tw = stbv_av1_tx_dims[tx].w << rc->ss_hor;
        int th = stbv_av1_tx_dims[tx].h << rc->ss_ver;
        int lx0 = x4 << rc->ss_hor, ly0 = y4 << rc->ss_ver;
        int lx, ly;
        stbv_u32 id = ((stbv_u32)x4 << 16) | (stbv_u32)y4;
        for (ly = ly0; ly < ly0 + th && ly < rc->lf_maph4; ly++)
            for (lx = lx0; lx < lx0 + tw && lx < rc->lf_mapw4; lx++) {
                rc->lf_blkid_c[(size_t)ly * rc->lf_b4stride + lx] = id;
                rc->lf_txlw_c[(size_t)ly * rc->lf_b4stride + lx] =
                    (stbv_u8)stbv_av1_tx_dims[tx].lw;
            }
    }
#endif
#endif
}

/* Per-txb chroma prediction (UV mode; CFL currently falls back to DC). */
static void stb_avif_recon_predict_txb_chroma(struct stb_avif_scalar_recon *rc,
                                               int pl, int x4, int y4, int tx)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    stbv_u16 *plane;
    int stride;
    const int pw = (rc->frame_w + rc->ss_hor) >> rc->ss_hor;
    const int ph = (rc->frame_h + rc->ss_ver) >> rc->ss_ver;
    /* 8-aligned (dav1d f->bw/f->bh), see note in block_edge_flags_run. */
    const int lfw4 = (rc->frame_w + 7) >> 3 << 1;
    const int lfh4 = (rc->frame_h + 7) >> 3 << 1;
    const int cfw4 = (lfw4 + rc->ss_hor) >> rc->ss_hor;
    const int cfh4 = (lfh4 + rc->ss_ver) >> rc->ss_ver;
    int cx4 = x4, cy4 = y4, cm, cangle = rc->uv_angle, cimpl;
    int w, h, cw, ch, i, j;
    if (!rc->plane_u || !rc->plane_v || !rc->has_chroma) return;
    plane = pl == 0 ? rc->plane_u : rc->plane_v;
    stride = pl == 0 ? rc->stride_u : rc->stride_v;
    cm = rc->uv_mode == STBV_AV1_INTRA_CFL ? STBV_AV1_INTRA_DC : rc->uv_mode;
    if (cx4 >= cfw4 || cy4 >= cfh4) return;
    w = stbv_av1_tx_dims[tx].w << 2;
    h = stbv_av1_tx_dims[tx].h << 2;
    /* Full padded extent (see predict_txb_luma note): clip against buffer
     * capacity, not the 8-aligned visible size. */
    cw = stride - (cx4 << 2);
    ch = ph + 32 - (cy4 << 2);
    if (cw > w) cw = w;
    if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) {
        return;
    }
    cimpl = stbv_av1_prepare_intra_edges_16(cx4, stb_avif_recon_have_left(rc, 0, cx4),
                                           cy4, stb_avif_recon_have_top(rc, 0, cy4),
                                           cfw4, cfh4,
                                           stb_avif_recon_txb_edge_flags(
                                               rc, 0, rc->cur_bx4, rc->cur_by4,
                                               rc->cur_bw4, rc->cur_bh4,
                                               x4, y4,
                                               stbv_av1_tx_dims[tx].w,
                                               stbv_av1_tx_dims[tx].h),
                                           plane + (cy4 << 2) * stride + (cx4 << 2),
                                           stride, NULL,
                                           cm, &cangle,
                                           stbv_av1_tx_dims[tx].w,
                                           stbv_av1_tx_dims[tx].h,
                                           rc->intra_edge_filter,
                                           edge, rc->bit_depth);
    stbv_av1_ipred_run_16(cimpl, rc->pred, w, edge, w, h,
                          cangle | stb_avif_recon_edge_flags(rc, 0, rc->cur_bx4 >> rc->ss_hor,
                    rc->cur_by4 >> rc->ss_ver),
                          0, (cfw4 - cx4) << 2, (cfh4 - cy4) << 2, rc->bit_depth);
    /* Chroma-from-luma, ported from dav1d recon_b_intra + cfl_ac_c +
     * cfl_pred: ONE block-wide AC array (built from the fully
     * reconstructed co-located luma) shared by both planes;
     * pred = edge-DC + alpha*ac with symmetric rounding; a plane with
     * zero alpha keeps its plain DC prediction. */
#ifdef STB_AVIF_TEST_NO_CFL
    if (0) {
#else
    if (rc->uv_mode == STBV_AV1_INTRA_CFL) {
#endif
        const int alpha = pl == 0 ? rc->cfl_alpha_u : rc->cfl_alpha_v;
        const int ss_h = rc->ss_hor, ss_v = rc->ss_ver;
        if (!alpha) {
            /* dav1d skips CFL entirely for this plane; the DC-family
             * prediction already written above is the final result. */
        } else {
            const int fw4 = (rc->frame_w + 7) >> 3 << 1;
            const int fh4 = (rc->frame_h + 7) >> 3 << 1;
            /* dav1d CFL gathers over the UNCLIPPED block dims (cbw4 =
             * b_dim-derived); clipping to the 8-aligned frame here shrank
             * right-edge blocks, corrupted the DC-subtraction and blew the
             * AC magnitudes up (saturated green fog). */
            int cw4u = rc->cur_bw4, ch4u = rc->cur_bh4;
            int cbw4, cbh4, W, H, i, j;
            const stbv_u16 mx = (stbv_u16)((1 << rc->bit_depth) - 1);
            (void)fw4; (void)fh4;
            cbw4 = (cw4u + ss_h) >> ss_h;
            cbh4 = (ch4u + ss_v) >> ss_v;
            W = cbw4 << 2;
            H = cbh4 << 2;
            if (W > 32 || H > 32 || cw4u <= 0 || ch4u <= 0) {
                /* out of contract; leave DC prediction */
            } else {
                const stbv_u16 *ysrc = rc->plane_y +
                    (((rc->cur_by4 & ~ss_v) << 2)) * rc->stride_y +
                    ((rc->cur_bx4 & ~ss_h) << 2);
                const int sh_l = 1 + !ss_v + !ss_h;
                int log2sz, x, y;
                long acc;
                if (!rc->cfl_ac_ok || rc->cfl_ac_bx != rc->cur_bx4 ||
                    rc->cfl_ac_by != rc->cur_by4) {
                    /* w_pad/h_pad from the UV transform dims (dav1d
                     * furthest_r/furthest_b use b->uvtx's t_dim);
                     * padded cols replicate. */
                    int twu = stbv_av1_tx_dims[tx].w;
                    int thu = stbv_av1_tx_dims[tx].h;
                    int furthest_r = ((cw4u << ss_h) + twu - 1) & ~(twu - 1);
                    int furthest_b = ((ch4u << ss_v) + thu - 1) & ~(thu - 1);
                    int w_pad = cbw4 - (furthest_r >> ss_h);
                    int h_pad = cbh4 - (furthest_b >> ss_v);
                    if (w_pad < 0) w_pad = 0;
                    if (h_pad < 0) h_pad = 0;
                    for (y = 0; y < H - 4 * h_pad; y++) {
                        const stbv_u16 *row0 = ysrc +
                            (y << ss_v) * rc->stride_y;
                        const stbv_u16 *row1 = row0 +
                            (ss_v ? rc->stride_y : 0);
                        for (x = 0; x < W - 4 * w_pad; x++) {
                            int s = row0[x << ss_h];
                            if (ss_h) s += row0[x * 2 + 1];
                            if (ss_v) {
                                s += row1[x << ss_h];
                                if (ss_h) s += row1[x * 2 + 1];
                            }
                            rc->cfl_ac[y * W + x] =
                                (stbv_i16)(s << sh_l);
                        }
                        for (; x < W; x++)
                            rc->cfl_ac[y * W + x] = rc->cfl_ac[y * W + x - 1];
                    }
                    for (; y < H; y++)
                        memcpy(rc->cfl_ac + y * W,
                               rc->cfl_ac + (y - 1) * W,
                               (size_t)(W * sizeof(stbv_i16)));
                    log2sz = stbv_av1_ipred_ctz((unsigned)W) +
                             stbv_av1_ipred_ctz((unsigned)H);
                    acc = (long)1 << log2sz >> 1;
                    for (y = 0; y < H; y++)
                        for (x = 0; x < W; x++)
                            acc += rc->cfl_ac[y * W + x];
                    acc >>= log2sz;
                    for (y = 0; y < H; y++)
                        for (x = 0; x < W; x++)
                            rc->cfl_ac[y * W + x] -= (stbv_i16)acc;
                    rc->cfl_ac_w = W;
                    rc->cfl_ac_h = H;
                    rc->cfl_ac_bx = rc->cur_bx4;
                    rc->cfl_ac_by = rc->cur_by4;
                    rc->cfl_ac_ok = 1;
                }
                {
                    const int off_x =
                        (x4 - (rc->cur_bx4 >> ss_h)) << 2;
                    const int off_y =
                        (y4 - (rc->cur_by4 >> ss_v)) << 2;
                    for (i = 0; i < ch; i++)
                        for (j = 0; j < cw; j++) {
                            int a = rc->cfl_ac[(off_y + i) * W + off_x + j];
                            int diff = alpha * a;
                            int adj = ((diff < 0 ? -diff : diff) + 32) >> 6;
                            int v = (int)rc->pred[i * w + j] +
                                    (diff < 0 ? -adj : adj);
                            rc->pred[i * w + j] =
                                (stbv_u16)(v < 0 ? 0 :
                                           (v > mx ? mx : v));
                        }
                }
            }
        }
    }
    for (i = 0; i < ch; i++) {
        memcpy(plane + ((cy4 << 2) + i) * stride + (cx4 << 2),
               rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
    }
}

static void stb_avif_recon_luma_pal(void *ud, const stbv_u8 *idx, int sz, int bw4, int bh4, const stbv_u16 *pal)
{
    struct stb_avif_scalar_recon *rc;
    /* planes are stbv_u16 */
    int x, y, w, h, cw, ch, i, j;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    x = rc->cur_bx4 << 2;
    y = rc->cur_by4 << 2;
    w = bw4 << 2;
    h = bh4 << 2;
    cw = rc->frame_w - x; if (cw > w) cw = w;
    ch = rc->frame_h - y; if (ch > h) ch = h;
    for (i = 0; i < ch; i++)
        for (j = 0; j < cw; j++) {
            int id = idx[i * w + j];
            rc->plane_y[(y + i) * rc->stride_y + x + j] =
                (stbv_u16)(id < sz ? pal[id] : 0);
        }
    /* record palette block identity for deblocking (no internal tx
     * boundaries: treat the entire palette block as one tx) */
    if (rc->lf_blkid) {
        stbv_u32 blkid = ((stbv_u32)rc->cur_bx4 << 16) |
                          (stbv_u32)rc->cur_by4;
        int bw4_log2 = 0, bh4_log2 = 0, tmp;
        tmp = bw4; while (tmp > 1) { tmp >>= 1; ++bw4_log2; }
        tmp = bh4; while (tmp > 1) { tmp >>= 1; ++bh4_log2; }
        for (i = 0; i < bh4 && (rc->cur_by4 + i) < rc->lf_maph4; i++)
            for (j = 0; j < bw4 && (rc->cur_bx4 + j) < rc->lf_mapw4; j++) {
                size_t off = (size_t)(rc->cur_by4 + i) * rc->lf_b4stride
                           + (rc->cur_bx4 + j);
                rc->lf_blkid[off] = blkid;
                rc->lf_txlw[off] = (stbv_u8)bw4_log2;
                rc->lf_done[off] = 1;
            }
        (void)bh4_log2;
    }
}

static void stb_avif_recon_chroma_pal(void *ud, int pl, const stbv_u8 *idx, int sz, int cbw4, int cbh4, const stbv_u16 *pal)
{
    struct stb_avif_scalar_recon *rc;
    int x, y, w, h, cw, ch, i, j;
    stbv_u16 *plane;
    int stride;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    x = (rc->cur_bx4 >> rc->ss_hor) << 2;
    y = (rc->cur_by4 >> rc->ss_ver) << 2;
    w = cbw4 << 2;
    h = cbh4 << 2;
    plane = pl == 0 ? rc->plane_u : rc->plane_v;
    stride = pl == 0 ? rc->stride_u : rc->stride_v;
    if (!plane) return;
    cw = (((rc->frame_w + rc->ss_hor) >> rc->ss_hor)) - x; if (cw > w) cw = w;
    ch = (((rc->frame_h + rc->ss_ver) >> rc->ss_ver)) - y; if (ch > h) ch = h;
    for (i = 0; i < ch; i++)
        for (j = 0; j < cw; j++) {
            int id = idx[i * w + j];
            plane[(y + i) * stride + x + j] =
                (stbv_u16)(id < sz ? pal[id] : 0);
        }
    /* record chroma palette block identity for deblocking */
    if (pl == 0 && rc->lf_blkid_c && rc->has_chroma) {
        int cx4 = rc->cur_bx4 >> rc->ss_hor;
        int cy4 = rc->cur_by4 >> rc->ss_ver;
        int lw = cbw4 << rc->ss_hor;
        int lh = cbh4 << rc->ss_ver;
        int lx0 = cx4 << rc->ss_hor;
        int ly0 = cy4 << rc->ss_ver;
        int lx, ly;
        stbv_u32 id = ((stbv_u32)cx4 << 16) | (stbv_u32)cy4;
        int cbw4_log2 = 0, tmp;
        tmp = cbw4; while (tmp > 1) { tmp >>= 1; ++cbw4_log2; }
        for (ly = ly0; ly < ly0 + lh && ly < rc->lf_maph4; ly++)
            for (lx = lx0; lx < lx0 + lw && lx < rc->lf_mapw4; lx++) {
                rc->lf_blkid_c[(size_t)ly * rc->lf_b4stride + lx] = id;
                rc->lf_txlw_c[(size_t)ly * rc->lf_b4stride + lx] =
                    (stbv_u8)cbw4_log2;
            }
        (void)lh;
    }
}

static struct stb_avif_scalar_recon g_scalar_recon;
static stbv_av1_leaf_recon g_scalar_recon_cb;

static void stb_avif_row_reset_cb(void *opaque)
{
    stbv_av1_leaf_state_reset_row((stbv_av1_leaf_state *)opaque);
}

static int stb_avif_leaf_cb(struct stb_av1_tile_decoder *td, const struct stb_av1_tile_leaf_info *li, void *opaque)
{
    stbv_av1_leaf_state *state;
    stbv_av1_leaf_tx_result out;
    int r;
    state = (stbv_av1_leaf_state *)opaque;
    r = stbv_av1_decode_leaf_syntax(&td->msac, &td->cdf, state,
                                       td->seq, td->frame,
                                       li->bs, li->bx, li->by,
                                       &out, &g_scalar_recon_cb);
    return r;
}

static int stb_avif_decode_frame_scalar(struct stb_av1_tile_context *tc, const unsigned char *av1_data, size_t av1_size)
{
    struct stb_av1_internal_stream *stream;
    struct stbv_av1_leaf_state_arrays arrays;
    stbv_av1_leaf_state state;
    struct stb_avif_scalar_recon *recon;
    struct stb_av1_tile_decoder td;
    int r;
    int frame_w4, frame_h4, frame_w8, frame_h8;
    stbv_u8 *above_mode = 0, *left_mode = 0, *above_tx = 0, *left_tx = 0;
    stbv_u8 *above_tx_intra = 0, *left_tx_intra = 0;
    stbv_u8 *above_res = 0, *left_res = 0;
    int res_w4, res_h4;
    stbv_u32 *lf_blkid_map = 0, *lf_blkid_map_c = 0;
    stbv_u8 *lf_txlw_map = 0, *lf_txlw_map_c = 0;
    stbv_u8 *lf_done_map = 0;
    int *cdef_idx_grid = 0;
    int cdef_grid_stride = 0;
    stbv_av1_lr_mask lr_mask;
    int lr_mask_ok = 0;
    int bw8al, bh8al;
    stbv_u8 *above_cre0 = 0, *above_cre1 = 0, *left_cre0 = 0, *left_cre1 = 0;
    stbv_u8 *above_skip = 0, *left_skip = 0, *above_pal_sz = 0;
    stbv_u8 *left_pal_sz = 0, *above_pal_uv = 0, *left_pal_uv = 0;
    stbv_u8 *above_uvmode = 0, *left_uvmode = 0;
    stbv_u16 *above_pal0 = 0, *above_pal1 = 0, *left_pal0 = 0, *left_pal1 = 0;
    stbv_u16 *py16 = 0, *pu16 = 0, *pv16 = 0;
    stbv_u8 *above_seg_id = 0, *left_seg_id = 0;
    int *above_ibc_mv_y = 0, *above_ibc_mv_x = 0;
    stbv_u8 *above_ibc_valid = 0;
    int *left_ibc_mv_y = 0, *left_ibc_mv_x = 0;
    stbv_u8 *left_ibc_valid = 0;
    stbv_refmvs_cell *refmvs_r = 0;
    int cframe_w8 = 0, cframe_h8 = 0;
    int i, j, h2, w2;
    stream = (struct stb_av1_internal_stream *)stb_avif_calloc(1, sizeof(*stream));
    recon = (struct stb_avif_scalar_recon *)stb_avif_calloc(1, sizeof(*recon));
    if (!stream || !recon) { stb_avif_free(stream); stb_avif_free(recon); return -1; }
    r = stb_av1_parse_internal_stream(stream, av1_data, av1_size);
    if (r < 0 || !stream->have_seq || !stream->have_frame)
        { stb_avif_free(stream); stb_avif_free(recon); return -1; }
    if ((int)stream->frame.width[0] != tc->frame_width ||
        (int)stream->frame.height != tc->frame_height)
        { stb_avif_free(stream); stb_avif_free(recon); return -2; }
    if (!stream->tile_data || !stream->tile_size)
        { stb_avif_free(stream); stb_avif_free(recon); return -3; }

    /* grey safety net for regions the tile walk may not cover */
    for (i = 0; i < tc->frame_height; i++)
        for (j = 0; j < tc->frame_width; j++)
            tc->plane_y[i*tc->stride_y+j] = 128;
    if (tc->plane_u && tc->plane_v) {
        h2 = (tc->frame_height+1)>>1; w2 = (tc->frame_width+1)>>1;
        for (i = 0; i < h2; i++)
            for (j = 0; j < w2; j++) {
                tc->plane_u[i*tc->stride_u+j] = 128;
                tc->plane_v[i*tc->stride_v+j] = 128;
            }
    }

    frame_w4 = ((tc->frame_width + 7) >> 3) << 1;
    frame_h4 = ((tc->frame_height + 7) >> 3) << 1;
    /* Chroma neighbour contexts are indexed in 4px chroma units.  dav1d
     * keeps them per-superblock, so edge-clamped blocks still publish
     * marks for units that round past the frame edge; round the frame-
     * exact count up to a superblock multiple so those writes/reads are
     * in-bounds and identical to dav1d's. */
    frame_w8 = ((((tc->frame_width + 7) >> 3) + 15) & ~15);
    frame_h8 = ((((tc->frame_height + 7) >> 3) + 15) & ~15);
    cframe_w8 = frame_w8;
    cframe_h8 = frame_h8;
    above_mode = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_mode = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_tx = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_tx = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_tx_intra = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_tx_intra = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    /* Residual-context arrays must be superblock-padded like dav1d's
     * f->bw/f->lh row/col contexts: edge transforms write their full
     * extent (e.g. a 16x32 txb at the right frame edge marks columns
     * beyond the 8-aligned frame width) and later dc_sign/skip ctx
     * reads those positions. */
    {
        int sbstep4 = stream->seq.sb128 ? 32 : 16;
        res_w4 = ((frame_w4 + sbstep4 - 1) / sbstep4) * sbstep4;
        res_h4 = ((frame_h4 + sbstep4 - 1) / sbstep4) * sbstep4;
        bw8al = (int)(((tc->frame_width + 7U) & ~7U) >> 2);
        bh8al = (int)(((tc->frame_height + 7U) & ~7U) >> 2);
    }
    above_res = (stbv_u8*)stb_avif_calloc(res_w4, 1);
    left_res = (stbv_u8*)stb_avif_calloc(res_h4, 1);
    /* Chroma context/pal_uv arrays must cover the CHROMA plane extent
     * (== luma extent for 4:4:4), SB-rounded like dav1d's f->bw arrays;
     * frame_w8 alone only fits the subsampled case. */
    {
        int ss_h = stream->seq.ss_hor ? 1 : 0;
        int ss_v = stream->seq.ss_ver ? 1 : 0;
        int cfw4 = (frame_w4 + ss_h) >> ss_h;
        int cfh4 = (frame_h4 + ss_v) >> ss_v;
        /* Chroma context arrays are SB-padded on the CHROMA grid
         * (sbstep4>>ss per superblock), matching dav1d whose f->bw-derived
         * write clip never rejects in-extent marks. */
        int step_h = ((stream->seq.sb128 ? 32 : 16) >> ss_h);
        int step_v = ((stream->seq.sb128 ? 32 : 16) >> ss_v);
        cframe_w8 = ((cfw4 + step_h - 1) / step_h) * step_h;
        cframe_h8 = ((cfh4 + step_v - 1) / step_v) * step_v;
    }
    above_cre0 = (stbv_u8*)stb_avif_calloc(cframe_w8, 1);
    above_cre1 = (stbv_u8*)stb_avif_calloc(cframe_w8, 1);
    left_cre0 = (stbv_u8*)stb_avif_calloc(cframe_h8, 1);
    left_cre1 = (stbv_u8*)stb_avif_calloc(cframe_h8, 1);
    /* dav1d's BlockContexts default to 0x40 (reset_context) at tile init;
     * never-written positions must read as 'no residual', not zero. */
    memset(above_res, 0x40, (size_t)res_w4);
    memset(left_res, 0x40, (size_t)res_h4);
    memset(above_cre0, 0x40, (size_t)cframe_w8);
    memset(above_cre1, 0x40, (size_t)cframe_w8);
    memset(left_cre0, 0x40, (size_t)cframe_h8);
    memset(left_cre1, 0x40, (size_t)cframe_h8);
    above_skip = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_skip = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_pal_sz = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_pal_sz = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_pal_uv = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_pal_uv = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_uvmode = (stbv_u8*)stb_avif_calloc(
        stream->seq.ss_hor ? ((frame_w4 + 1) >> 1) : frame_w4, 1);
    left_uvmode = (stbv_u8*)stb_avif_calloc(
        stream->seq.ss_ver ? ((frame_h4 + 1) >> 1) : frame_h4, 1);
    above_pal0 = (stbv_u16*)stb_avif_calloc(frame_w4*8, 2);
    above_pal1 = (stbv_u16*)stb_avif_calloc(frame_w4*8, 2);
    left_pal0 = (stbv_u16*)stb_avif_calloc(frame_h4*8, 2);
    left_pal1 = (stbv_u16*)stb_avif_calloc(frame_h4*8, 2);
    above_seg_id = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_seg_id = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    /* IBC MV neighbour arrays for spatial prediction. */
    above_ibc_mv_y = (int*)stb_avif_calloc(frame_w4, sizeof(int));
    above_ibc_mv_x = (int*)stb_avif_calloc(frame_w4, sizeof(int));
    above_ibc_valid = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_ibc_mv_y = (int*)stb_avif_calloc(frame_h4, sizeof(int));
    left_ibc_mv_x = (int*)stb_avif_calloc(frame_h4, sizeof(int));
    left_ibc_valid = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    /* 2D refmvs block array for dav1d-compatible IBC MV prediction. */
    refmvs_r = (stbv_refmvs_cell*)stb_avif_calloc(
        (size_t)frame_h4 * frame_w4, sizeof(stbv_refmvs_cell));
    if (!above_mode || !left_mode || !above_tx || !left_tx || !above_res ||
        !left_res || !above_cre0 || !above_cre1 || !left_cre0 || !left_cre1 ||
        !above_skip || !left_skip || !above_pal_sz || !left_pal_sz ||
        !above_pal_uv || !left_pal_uv || !above_pal0 || !above_pal1 ||
        !left_pal0 || !left_pal1 || !above_seg_id || !left_seg_id ||
        !above_ibc_mv_y || !above_ibc_mv_x || !above_ibc_valid ||
        !left_ibc_mv_y || !left_ibc_mv_x || !left_ibc_valid ||
        !above_tx_intra || !left_tx_intra)
        { stb_avif_free(stream); stb_avif_free(recon); return -4; }
    memset(&arrays, 0, sizeof(arrays));
    arrays.above_mode = above_mode; arrays.above_mode_n = frame_w4;
    arrays.left_mode = left_mode; arrays.left_mode_n = frame_h4;
    arrays.above_tx = above_tx; arrays.above_tx_n = frame_w4;
    arrays.left_tx = left_tx; arrays.left_tx_n = frame_h4;
    arrays.above_tx_intra = above_tx_intra;
    arrays.left_tx_intra = left_tx_intra;
    arrays.above_res = above_res; arrays.above_res_n = res_w4;
    arrays.left_res = left_res; arrays.left_res_n = res_h4;
    /* dav1d's f->bw/f->bh are SB-ALIGNED unit counts, so its write clip
     * (imin(txw, f->bw - bx)) never rejects in-frame marks; context arrays
     * keep every mark including past-pixel-edge units.  Clipping disabled
     * (0 = use above_n/left_n). */
    /* dav1d recon_tmpl.c clips CODED residual-context writes to the
     * frame extent: luma imin(txw, f->bw - bx); chroma ctw =
     * imin(uvtx_w, (f->bw - bx + ss_hor) >> ss_hor).  With f->bw =
     * frame_w4 this is simply "units inside the chroma plane
     * extent". SKIP marking bypasses the clip entirely. */
    arrays.above_res_mark_n = bw8al;
    arrays.left_res_mark_n = bh8al;
    /* dav1d clips CODED residual marking to f->bw/f->bh (8-aligned px
     * width): luma imin(txw, f->bw - bx); chroma ctw =
     * imin(uvtx_w, (f->bw - bx + ss_hor) >> ss_hor).  Expressed as an
     * absolute column limit on our frame-wide arrays this is simply
     * bw8al (=f->bw) for luma and (bw8al+ss)>>ss for chroma. */
    arrays.above_cre_mark_n[0] = (bw8al + (stream->seq.ss_hor ? 1 : 0)) >>
                                 (stream->seq.ss_hor ? 1 : 0);
    arrays.above_cre_mark_n[1] = (bw8al + (stream->seq.ss_hor ? 1 : 0)) >>
                                 (stream->seq.ss_hor ? 1 : 0);
    arrays.left_cre_mark_n[0] = (bh8al + (stream->seq.ss_ver ? 1 : 0)) >>
                                 (stream->seq.ss_ver ? 1 : 0);
    arrays.left_cre_mark_n[1] = (bh8al + (stream->seq.ss_ver ? 1 : 0)) >>
                                 (stream->seq.ss_ver ? 1 : 0);
    arrays.above_cre[0] = above_cre0; arrays.above_cre_n[0] = cframe_w8;
    arrays.above_cre[1] = above_cre1; arrays.above_cre_n[1] = cframe_w8;
    arrays.left_cre[0] = left_cre0; arrays.left_cre_n[0] = cframe_h8;
    arrays.left_cre[1] = left_cre1; arrays.left_cre_n[1] = cframe_h8;
    arrays.above_skip = above_skip; arrays.above_skip_n = frame_w4;
    arrays.left_skip = left_skip; arrays.left_skip_n = frame_h4;
    arrays.above_pal_sz = above_pal_sz; arrays.above_pal_sz_n = frame_w4;
    arrays.left_pal_sz = left_pal_sz; arrays.left_pal_sz_n = frame_h4;
    arrays.above_pal_uv = above_pal_uv; arrays.above_pal_uv_n = frame_w4;
    arrays.left_pal_uv = left_pal_uv; arrays.left_pal_uv_n = frame_h4;
    arrays.above_pal[0] = above_pal0; arrays.above_pal[1] = above_pal1;
    arrays.left_pal[0] = left_pal0; arrays.left_pal[1] = left_pal1;
    arrays.above_pal_n = frame_w4; arrays.left_pal_n = frame_h4;
    arrays.above_seg_id = above_seg_id; arrays.above_seg_id_n = frame_w4;
    arrays.left_seg_id = left_seg_id; arrays.left_seg_id_n = frame_h4;
    arrays.above_ibc_mv_y = above_ibc_mv_y;
    arrays.above_ibc_mv_x = above_ibc_mv_x;
    arrays.above_ibc_valid = above_ibc_valid;
    arrays.above_ibc_mv_n = frame_w4;
    arrays.left_ibc_mv_y = left_ibc_mv_y;
    arrays.left_ibc_mv_x = left_ibc_mv_x;
    arrays.left_ibc_valid = left_ibc_valid;
    arrays.left_ibc_mv_n = frame_h4;
    arrays.refmvs_r = (stbv_u8*)refmvs_r;
    arrays.refmvs_stride = frame_w4;
    arrays.refmvs_h4 = frame_h4;
    arrays.refmvs_w4 = frame_w4;
    stbv_av1_leaf_state_init(&state, &arrays);
    state.cdef_idx_grid = NULL;
    state.cdef_grid_stride = 0;
    stb_av1_intra_state_set_uv(&state.intra, above_uvmode,
                               stream->seq.ss_hor ? ((frame_w4 + 1) >> 1)
                                                  : frame_w4,
                               left_uvmode,
                               stream->seq.ss_ver ? ((frame_h4 + 1) >> 1)
                                                  : frame_h4);

    /* Internal planes are stbv_u16 (bit-depth generic); converted back to
     * the 8-bit tc->planes after decoding. */
    py16 = (stbv_u16*)stb_avif_calloc(
        (size_t)tc->stride_y * (tc->frame_height + 64), sizeof(stbv_u16));
    if (!py16) { r = -5; goto oom16; }
    if (tc->plane_u && tc->plane_v) {
        int uvh2 = (stream->seq.ss_ver ? ((tc->frame_height + 1) >> 1)
                                      : tc->frame_height) + 32;
        pu16 = (stbv_u16*)stb_avif_calloc(
            (size_t)tc->stride_u * uvh2, sizeof(stbv_u16));
        pv16 = (stbv_u16*)stb_avif_calloc(
            (size_t)tc->stride_v * uvh2, sizeof(stbv_u16));
        if (!pu16 || !pv16) { r = -5; goto oom16; }
    }
    /* Deblocking maps: frame-sized 4x4-unit grid (SB-padded like the
     * residual context arrays). */
    {
        int mw = res_w4, mh = res_h4;
        lf_blkid_map = (stbv_u32*)stb_avif_calloc((size_t)mw * mh, sizeof(stbv_u32));
        lf_txlw_map = (stbv_u8*)stb_avif_calloc((size_t)mw * mh, 1);
        lf_blkid_map_c = (stbv_u32*)stb_avif_calloc((size_t)mw * mh, sizeof(stbv_u32));
        lf_txlw_map_c = (stbv_u8*)stb_avif_calloc((size_t)mw * mh, 1);
        lf_done_map = (stbv_u8*)stb_avif_calloc((size_t)mw * mh, 1);
        if (!lf_blkid_map || !lf_txlw_map ||
            !lf_blkid_map_c || !lf_txlw_map_c || !lf_done_map) { r = -5; goto oom16; }
        memset(lf_blkid_map_c, 0xFF, (size_t)mw * mh * sizeof(stbv_u32));
    }
    /* CDEF index grid: one entry per 64x64 block. */
    if (stream->seq.cdef) {
        cdef_grid_stride = (frame_w4 + 15) / 16;
        {
            int cdef_grid_rows = (frame_h4 + 15) / 16;
            cdef_idx_grid = (int*)stb_avif_calloc(
                (size_t)cdef_grid_stride * cdef_grid_rows, sizeof(int));
            if (!cdef_idx_grid) { r = -5; goto oom16; }
            /* Initialize to -1 (no CDEF / skip). */
            {
                int gi;
                int cdef_grid_total = cdef_grid_stride * cdef_grid_rows;
                for (gi = 0; gi < cdef_grid_total; gi++)
                    cdef_idx_grid[gi] = -1;
            }
        }
    }
    /* Wire the CDEF grid into the leaf state (after allocation). */
    state.cdef_idx_grid = cdef_idx_grid;
    state.cdef_grid_stride = cdef_grid_stride;

    /* Allocate LR mask for storing decoded restoration params. */
    if (stream->seq.restoration && !stream->frame.allow_intrabc) {
        int lr_usz[2];
        lr_usz[0] = stream->frame.restoration.unit_size[0];
        lr_usz[1] = stream->frame.restoration.unit_size[1];
        if (stbv_av1_lr_mask_alloc(&lr_mask, tc->frame_width, tc->frame_height,
                                   lr_usz, stream->seq.ss_hor ? 1 : 0,
                                   (stream->seq.layout == STB_AV1_LAYOUT_I420) ? 1 : 0) == 0) {
            lr_mask_ok = 1;
        }
    }
    memset(recon, 0, sizeof(*recon));
    recon->lf_blkid = lf_blkid_map;
    recon->lf_txlw = lf_txlw_map;
    recon->lf_blkid_c = lf_blkid_map_c;
    recon->lf_txlw_c = lf_txlw_map_c;
    recon->lf_done = lf_done_map;
    recon->lf_mapw4 = res_w4;
    recon->lf_maph4 = res_h4;
    recon->lf_b4stride = res_w4;
    recon->plane_y = py16;
    recon->plane_u = pu16;
    recon->plane_v = pv16;
    recon->stride_y = tc->stride_y;
    recon->stride_u = tc->stride_u;
    recon->stride_v = tc->stride_v;
    recon->bit_depth = 8 + stream->seq.hbd * 2;
    recon->ss_hor = stream->seq.ss_hor ? 1 : 0;
    recon->ss_ver = (stream->seq.layout == STB_AV1_LAYOUT_I420) ? 1 : 0;
    recon->frame_w = tc->frame_width;
    recon->frame_h = tc->frame_height;
    recon->tile_x4 = 0; recon->tile_y4 = 0;
    recon->tile_w4 = (int)((stream->frame.width[0] + 3U) >> 2);
    recon->tile_h4 = (int)((stream->frame.height + 3U) >> 2);
    recon->intra_edge_filter = stream->seq.intra_edge_filter ? 1 : 0;
    recon->sb_step4 = stream->seq.sb128 ? 32 : 16;
    recon->above_mode = above_mode;
    recon->left_mode = left_mode;
    recon->above_n = frame_w4;
    recon->left_n = frame_h4;
    recon->above_uvmode = above_uvmode;
    recon->left_uvmode = left_uvmode;
    g_scalar_recon = *recon;
    g_scalar_recon_cb.ud = &g_scalar_recon;
    g_scalar_recon_cb.cf = g_scalar_recon.cf;
    g_scalar_recon_cb.block_info = stb_avif_recon_block_info;
    g_scalar_recon_cb.luma_txb = stb_avif_recon_luma_txb;
    g_scalar_recon_cb.chroma_txb = stb_avif_recon_chroma_txb;
    g_scalar_recon_cb.luma_pal = stb_avif_recon_luma_pal;
    g_scalar_recon_cb.chroma_pal = stb_avif_recon_chroma_pal;

    memset(&td, 0, sizeof(td));
    td.seq = &stream->seq;
    td.frame = &stream->frame;
    r = 0;
    {
        unsigned int ti;
        unsigned int ntiles = stream->frame.tiling.cols * stream->frame.tiling.rows;
        unsigned int sb_log2 = 6U + stream->seq.sb128;
        unsigned int sb_size = 1U << sb_log2;
        for (ti = 0; ti < ntiles; ti++) {
            unsigned int tr, tcx;
            if (!stream->tile_seen[ti]) { r = -1; break; }
            tr = ti / stream->frame.tiling.cols;
            tcx = ti - tr * stream->frame.tiling.cols;
            recon->tile_x4 = (int)(stream->frame.tiling.col_start_sb[tcx] * (sb_size >> 2));
            recon->tile_y4 = (int)(stream->frame.tiling.row_start_sb[tr] * (sb_size >> 2));
            recon->tile_w4 = (int)((stream->frame.tiling.col_start_sb[tcx + 1] -
                                   stream->frame.tiling.col_start_sb[tcx]) * (sb_size >> 2));
            recon->tile_h4 = (int)((stream->frame.tiling.row_start_sb[tr + 1] -
                                   stream->frame.tiling.row_start_sb[tr]) * (sb_size >> 2));
            stbv_av1_leaf_state_init(&state, &arrays);
            state.last_qidx = (int)stream->frame.quant.yac;
            memset(state.last_delta_lf, 0, sizeof(state.last_delta_lf));
            /* leaf_state_init NULLs above/left_uvmode via intra_state_init;
             * re-establish them so chroma intra mode decode has proper
             * above/left contexts for each tile (tiles are independent). */
            stb_av1_intra_state_set_uv(&state.intra, above_uvmode,
                                       stream->seq.ss_hor ? ((frame_w4 + 1) >> 1)
                                                          : frame_w4,
                                       left_uvmode,
                                       stream->seq.ss_ver ? ((frame_h4 + 1) >> 1)
                                                          : frame_h4);
            /* Recon callbacks point at g_scalar_recon, so publish the
             * current tile's bounds before decoding its first leaf. */
            g_scalar_recon = *recon;
            td.lr_mask = lr_mask_ok ? &lr_mask : 0;
            r = stb_av1_decode_tile_at(&td, &stream->seq, &stream->frame,
                                       stream->tiles[ti].data, stream->tiles[ti].size,
                                       tcx, tr, stb_avif_leaf_cb, &state,
                                       stb_avif_row_reset_cb);
            if (r) { break; }
        }
    }

#ifdef STB_AVIF_DEBLOCK
    if (!r) {
        const struct stb_av1_framehdr *fh = &stream->frame;
        int lvl_yv = (int)fh->loopfilter.level_y[0];
        int lvl_yh = (int)fh->loopfilter.level_y[1];
        int lvl_u = (int)fh->loopfilter.level_u;
        int lvl_v = (int)fh->loopfilter.level_v;
        int sharp = (int)fh->loopfilter.sharpness;
        int maxv = (1 << recon->bit_depth) - 1;
        if (recon->ss_ver && !lvl_u) lvl_u = lvl_v;
        if (py16)
            stb_avif_deblock_plane_u16(py16, tc->stride_y,
                                       tc->frame_width, tc->frame_height,
                                       lvl_yv, lvl_yh, sharp, 0, maxv,
                                       recon->bit_depth - 8,
                                       lf_blkid_map, lf_txlw_map, res_w4,
                                       res_w4, res_h4, 0, 0,
                                       stream->frame.tiling.col_start_sb,
                                       (int)stream->frame.tiling.cols,
                                       stream->frame.tiling.row_start_sb,
                                       (int)stream->frame.tiling.rows,
                                       (int)(1U << (6U + stream->seq.sb128)));
        if (pu16 && !stream->seq.monochrome) {
            int cw = (tc->frame_width + (recon->ss_hor ? 1 : 0)) >> recon->ss_hor;
            int ch = (tc->frame_height + (recon->ss_ver ? 1 : 0)) >> recon->ss_ver;
            stb_avif_deblock_plane_u16(pu16, tc->stride_u, cw, ch,
                                       lvl_u, lvl_u, sharp, 1, maxv,
                                       recon->bit_depth - 8,
                                       lf_blkid_map_c, lf_txlw_map_c, res_w4,
                                       res_w4, res_h4,
                                       recon->ss_hor, recon->ss_ver,
                                       stream->frame.tiling.col_start_sb,
                                       (int)stream->frame.tiling.cols,
                                       stream->frame.tiling.row_start_sb,
                                       (int)stream->frame.tiling.rows,
                                       (int)(1U << (6U + stream->seq.sb128)));
            stb_avif_deblock_plane_u16(pv16, tc->stride_v, cw, ch,
                                       lvl_v ? lvl_v : lvl_u, lvl_v ? lvl_v : lvl_u,
                                       sharp, 1, maxv, recon->bit_depth - 8,
                                       lf_blkid_map_c, lf_txlw_map_c, res_w4,
                                       res_w4, res_h4,
                                       recon->ss_hor, recon->ss_ver,
                                       stream->frame.tiling.col_start_sb,
                                       (int)stream->frame.tiling.cols,
                                       stream->frame.tiling.row_start_sb,
                                       (int)stream->frame.tiling.rows,
                                       (int)(1U << (6U + stream->seq.sb128)));
        }
    }
#endif

    /* CDEF filtering (after deblocking, before loop restoration). */
    if (!r && stream->seq.cdef && cdef_idx_grid) {
        const struct stb_av1_framehdr *fh = &stream->frame;
        int y_pri_arr[8], y_sec_arr[8], uv_pri_arr[8], uv_sec_arr[8];
        int ci;
        for (ci = 0; ci < (1 << fh->cdef.n_bits); ci++) {
            y_pri_arr[ci] = (int)(fh->cdef.y_strength[ci] >> 2);
            y_sec_arr[ci] = (int)(fh->cdef.y_strength[ci] & 3);
            uv_pri_arr[ci] = (int)(fh->cdef.uv_strength[ci] >> 2);
            uv_sec_arr[ci] = (int)(fh->cdef.uv_strength[ci] & 3);
        }
        stb_av1_cdef_frame(py16, pu16, pv16,
                           tc->stride_y, tc->stride_u, tc->stride_v,
                           tc->frame_width, tc->frame_height,
                           recon->ss_hor, recon->ss_ver,
                           recon->bit_depth,
                           cdef_idx_grid, cdef_grid_stride,
                           y_pri_arr, y_sec_arr,
                           uv_pri_arr, uv_sec_arr,
                               (int)fh->cdef.damping);
    }

    /* Loop restoration filtering (after CDEF, before 8-bit conversion). */
    if (!r && lr_mask_ok && stream->seq.restoration && !stream->frame.allow_intrabc) {
        stb_av1_lr_frame(py16, pu16, pv16,
                         tc->stride_y, tc->stride_u, tc->stride_v,
                         tc->frame_width, tc->frame_height,
                         stream->seq.ss_hor ? 1 : 0,
                         (stream->seq.layout == STB_AV1_LAYOUT_I420) ? 1 : 0,
                         8 + stream->seq.hbd * 2,
                           &lr_mask);
    }

    /* Convert internal u16 planes to the caller's 8-bit planes.
     * Use truncating shift to match dav1d's u16->u8 conversion. */
    {
        const int bd = 8 + stream->seq.hbd * 2;
        const int sh = bd - 8;
        int w, h, hh, y0, x0;
        w = tc->frame_width;
        h = tc->frame_height;
        for (y0 = 0; y0 < h; y0++)
            for (x0 = 0; x0 < w; x0++) {
                unsigned v = (unsigned)py16[y0 * tc->stride_y + x0];
                v = v >> sh;
                tc->plane_y[y0 * tc->stride_y + x0] =
                    (stbv_u8)(v > 255u ? 255u : v);
            }
        if (pu16 && pv16 && tc->plane_u && tc->plane_v) {
            int ss_h = stream->seq.ss_hor ? 1 : 0;
            int ss_v = stream->seq.ss_ver ? 1 : 0;
            w = (tc->frame_width + ss_h) >> ss_h;
            h = (tc->frame_height + ss_v) >> ss_v;
            for (hh = 0; hh < h; hh++)
                for (x0 = 0; x0 < w; x0++) {
                    unsigned vu = (unsigned)pu16[hh * tc->stride_u + x0];
                    unsigned vv = (unsigned)pv16[hh * tc->stride_v + x0];
                    vu = vu >> sh;
                    vv = vv >> sh;
                    tc->plane_u[hh * tc->stride_u + x0] =
                        (stbv_u8)(vu > 255u ? 255u : vu);
                    tc->plane_v[hh * tc->stride_v + x0] =
                        (stbv_u8)(vv > 255u ? 255u : vv);
                }
        }
    }
oom16:
    if (py16) stb_avif_free_internal(py16);
    if (pu16) stb_avif_free_internal(pu16);
    if (pv16) stb_avif_free_internal(pv16);
    stb_avif_free_internal(above_mode); stb_avif_free_internal(left_mode);
    stb_avif_free_internal(above_tx); stb_avif_free_internal(left_tx);
    stb_avif_free_internal(above_tx_intra); stb_avif_free_internal(left_tx_intra);
    stb_avif_free_internal(above_res); stb_avif_free_internal(left_res);
    stb_avif_free_internal(above_cre0); stb_avif_free_internal(above_cre1);
    stb_avif_free_internal(left_cre0); stb_avif_free_internal(left_cre1);
    stb_avif_free_internal(above_skip); stb_avif_free_internal(left_skip);
    stb_avif_free_internal(above_seg_id); stb_avif_free_internal(left_seg_id);
    stb_avif_free_internal(above_ibc_mv_y); stb_avif_free_internal(above_ibc_mv_x);
    stb_avif_free_internal(above_ibc_valid);
    stb_avif_free_internal(left_ibc_mv_y); stb_avif_free_internal(left_ibc_mv_x);
    stb_avif_free_internal(left_ibc_valid);
    stb_avif_free_internal(refmvs_r);
    stb_avif_free_internal(above_pal_sz); stb_avif_free_internal(left_pal_sz);
    stb_avif_free_internal(above_pal_uv);
    stb_avif_free_internal(above_uvmode);
    stb_avif_free_internal(left_uvmode); stb_avif_free_internal(left_pal_uv);
    stb_avif_free_internal(above_pal0); stb_avif_free_internal(above_pal1);
    stb_avif_free_internal(left_pal0); stb_avif_free_internal(left_pal1);
    stb_avif_free_internal(cdef_idx_grid);
    stbv_av1_lr_mask_free(&lr_mask);
    stb_avif_free_internal(lf_blkid_map); stb_avif_free_internal(lf_blkid_map_c);
    stb_avif_free_internal(lf_txlw_map); stb_avif_free_internal(lf_txlw_map_c);
    stb_avif_free_internal(lf_done_map);
    stb_avif_free(stream);
    stb_avif_free(recon);
    return r;
}

#endif /* !STB_AVIF_USE_DAV1D */

/* -------------------------------------------------------------------------- */
/* MAIN API IMPLEMENTATION                                                    */
/* -------------------------------------------------------------------------- */

const char *stb_avif_failure_reason(void)
{
    return stb_avif_error_msg;
}

void stb_avif_free(void *ptr)
{
    free(ptr);
}

unsigned char *stb_avif_load_from_file(const char *filePath,
                                       int *w, int *h, int *channels,
                                       int req_channels)
{
    FILE *f;
    unsigned char *buf;
    long file_size;
    unsigned char *result;

    if (!filePath) return NULL;
    f = fopen(filePath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) { fclose(f); return NULL; }

    buf = (unsigned char *)malloc((size_t)file_size);
    if (!buf) { fclose(f); return NULL; }

    if ((long)fread(buf, 1, (size_t)file_size, f) != file_size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    result = stb_avif_load_from_memory(buf, (int)file_size, w, h, channels,
                                       req_channels);
    free(buf);
    return result;
}

unsigned char *stb_avif_load_from_memory(const unsigned char *data, int len,
                                          int *x, int *y, int *channels,
                                          int req_channels)
{
    struct stb_av1_getbits gb;
    
    struct stb_avif_avif_info info;
    struct stb_av1_sequence_header sh;
    struct stb_av1_frame_header fh;
    struct stb_av1_tile_context tc;
    struct stb_av1_getbits obu_gb;
    struct stb_av1_bool_reader br;
    unsigned char *result = NULL;
    int output_channels;

    /* Free YUV planes from previous load. */
    if (stb_avif_g_last_yuv_y) { stb_avif_free_internal(stb_avif_g_last_yuv_y); stb_avif_g_last_yuv_y = NULL; }
    if (stb_avif_g_last_yuv_u) { stb_avif_free_internal(stb_avif_g_last_yuv_u); stb_avif_g_last_yuv_u = NULL; }
    if (stb_avif_g_last_yuv_v) { stb_avif_free_internal(stb_avif_g_last_yuv_v); stb_avif_g_last_yuv_v = NULL; }

    /* Initialize info struct */
    memset(&info, 0, sizeof(info));
    info.bit_depth = 8;
    info.chroma_subsampling_x = 1;
    info.chroma_subsampling_y = 1;
    info.input = data;
    info.input_len = len;

    memset(&sh, 0, sizeof(sh));
    memset(&fh, 0, sizeof(fh));
    memset(&tc, 0, sizeof(tc));

    result = NULL;

    /* Validate input */
    STB_AVIF_CHECK(data != NULL && len >= 16, "Invalid input data");

    /* Detect IVF container (starts with "DKIF") */
    if (data[0] == 'D' && data[1] == 'K' && data[2] == 'I' && data[3] == 'F') {
        unsigned hdr_len;
        int ivf_w, ivf_h;
        unsigned pos;
        size_t total_av1 = 0;
        if (len < 32) goto error_exit;
        hdr_len = (unsigned)data[6] | ((unsigned)data[7] << 8);
        ivf_w = (int)((unsigned)data[12] | ((unsigned)data[13] << 8));
        ivf_h = (int)((unsigned)data[14] | ((unsigned)data[15] << 8));
        if ((int)hdr_len > len || ivf_w <= 0 || ivf_h <= 0) goto error_exit;
        info.width = ivf_w;
        info.height = ivf_h;
        /* Concatenate ALL frame payloads into a single AV1 bitstream */
        pos = hdr_len;
        while ((int)(pos + 12) <= len) {
            unsigned fsz = (unsigned)data[pos] | ((unsigned)data[pos+1] << 8) |
                           ((unsigned)data[pos+2] << 16) | ((unsigned)data[pos+3] << 24);
            if (fsz == 0 || (int)(pos + 12 + fsz) > len) break;
            total_av1 += fsz;
            pos += 12 + fsz;
        }
        if (total_av1 == 0) goto error_exit;
        {
            unsigned char *av1_buf = (unsigned char *)stb_avif_calloc(1, total_av1);
            unsigned char *dst;
            if (!av1_buf) goto error_exit;
            dst = av1_buf;
            pos = hdr_len;
            while ((int)(pos + 12) <= len) {
                unsigned fsz = (unsigned)data[pos] | ((unsigned)data[pos+1] << 8) |
                               ((unsigned)data[pos+2] << 16) | ((unsigned)data[pos+3] << 24);
                if (fsz == 0 || (int)(pos + 12 + fsz) > len) break;
                memcpy(dst, data + pos + 12, fsz);
                dst += fsz;
                pos += 12 + fsz;
            }
            info.av1_data = av1_buf;
            info.av1_size = total_av1;
            info.ivf_concat_buf = av1_buf;
        }
        /* Validate: first byte must look like a valid OBU header */
        if (total_av1 > 0) {
            unsigned char first = info.av1_data[0];
            int obu_type = (first >> 3) & 0xF;
            if ((first & 0x80) != 0 || obu_type == 0 || (first & 1) != 0) {
                stb_avif_error_msg = "Invalid IVF: first frame does not contain valid AV1 OBU data";
                goto error_exit;
            }
        }
        info.input = data;
        info.input_len = len;
        goto ivf_decoded;
    }

    /* Setup error handling */
    if (setjmp(stb_avif_jmp)) {
        goto error_exit;
    }

    stb_av1_getbits_init(&gb, data, (size_t)len);

    /* Look for ftyp box */
    STB_AVIF_CHECK(stb_avif_find_box(&gb, STB_AVIF_BOX_FTYP, 0, NULL),
                   "No ftyp box found");
    stb_avif_parse_ftyp(&gb, &info);

    /* Look for meta box */
    {
        struct stb_avif_box meta_hdr;
        stb_av1_getbits_init(&gb, data, (size_t)len);
        STB_AVIF_CHECK(stb_avif_find_box(&gb, STB_AVIF_BOX_META, 1, &meta_hdr),
                       "No meta box found");
        /* Save meta end position for parse_meta */
        info.meta_end_offset = (size_t)(meta_hdr.data_start + meta_hdr.data_size);
    }

    /* Parse the meta box to extract all AVIF metadata */
    stb_avif_parse_meta(&gb, &info);

    /* Verify we have image dimensions */
    STB_AVIF_CHECK(info.width > 0 && info.height > 0,
                   "Could not determine image dimensions");
    STB_AVIF_CHECK(info.width <= STB_AVIF_MAX_DIMENSION &&
                   info.height <= STB_AVIF_MAX_DIMENSION,
                   "Image too large");

    /* Verify we have compressed data */
    STB_AVIF_CHECK(info.av1_data != NULL && info.av1_size > 0,
                   "No AV1 compressed data found");

ivf_decoded:
    /* Set up sequence header defaults */
    sh.bit_depth = info.bit_depth;
    sh.monochrome = info.monochrome;
    sh.subsampling_x = info.chroma_subsampling_x;
    sh.subsampling_y = info.chroma_subsampling_y;
    sh.reduced_still_picture_header = 1;
    sh.still_picture = 1;
    sh.max_frame_width = info.width;
    sh.max_frame_height = info.height;
    sh.frame_width_bits = 4;
    sh.frame_height_bits = 4;
    sh.enable_order_hint = 0;
    sh.enable_dist_wtd_comp = 0;
    sh.enable_masked_comp = 0;
    sh.enable_intra_edge_filter = 1;
    sh.enable_interintra_comp = 0;
    sh.enable_dual_filter = 0;
    sh.enable_jnt_comp = 0;
    sh.enable_superres = 0;
    sh.enable_cdef = 1;
    sh.enable_restoration = 0;
    sh.film_grain_params_present = 0;
    sh.color_description_present = 0;

    /* Parse the AV1 bitstream */
    stb_av1_getbits_init(&obu_gb, info.av1_data, info.av1_size);

    /* Initialize Boolean reader from the OBU data */
    stb_av1_bool_reader_init(&br, info.av1_data, info.av1_size);

    /* Process OBUs */
    {
        int obu_type;
            int obu_extension_flag;
            int obu_has_size_field;
        stbv_u32 obu_size;
        int more_obus = 1;
        int seq_header_found = 0;
        int frame_header_found = 0;

        (void)obu_extension_flag;
        obu_size = 0;

        while (more_obus && stb_av1_getbits_bytepos(&obu_gb) < stb_av1_getbits_size(&obu_gb)) {
            /* Parse OBU header */
            if (stb_av1_getbits_bytepos(&obu_gb) + 1 > stb_av1_getbits_size(&obu_gb))
                break;

            stb_av1_read_obu_header(&obu_gb, &obu_type,
                                     &obu_extension_flag, &obu_has_size_field);

            /* Read OBU size */
            obu_size = 0;
            if (obu_has_size_field) {
                obu_size = stb_av1_read_obu_size(&obu_gb);
            }

            /* Process based on type */
            switch (obu_type) {
                case STB_AV1_OBU_SEQUENCE_HEADER: {
#ifndef STB_AVIF_USE_DAV1D
                    /* Parse the sequence header with the authoritative
                     * raw-bit parser so sh is populated BEFORE the frame
                     * header parser runs (it reads sh->frame_width_bits,
                     * sh->enable_cdef, etc.). */
                    struct stb_av1_seqhdr sq;
                    struct stb_av1_getbits sq_gb;
                    const stbv_u8 *sq_data = obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb);
                    size_t sq_sz = (size_t)obu_size;
                    memset(&sq, 0, sizeof(sq));
                    stb_av1_getbits_init(&sq_gb, sq_data, sq_sz);
                    if (stb_av1_parse_seqhdr(&sq, &sq_gb) == 0) {
                        sh.frame_width_bits  = (int)sq.width_n_bits;
                        sh.frame_height_bits = (int)sq.height_n_bits;
                        sh.max_frame_width   = (int)sq.max_width;
                        sh.max_frame_height  = (int)sq.max_height;
                        sh.seq_profile                = (int)sq.profile;
                        sh.still_picture              = (int)sq.still_picture;
                        sh.reduced_still_picture_header = (int)sq.reduced_still_picture_header;
                        sh.enable_order_hint        = (int)sq.order_hint;
                        sh.enable_dist_wtd_comp     = 0;
                        sh.enable_masked_comp       = (int)sq.masked_compound;
                        sh.enable_intra_edge_filter = (int)sq.intra_edge_filter;
                        sh.enable_interintra_comp   = (int)sq.inter_intra;
                        sh.enable_dual_filter       = (int)sq.dual_filter;
                        sh.enable_jnt_comp          = (int)sq.jnt_comp;
                        sh.enable_superres          = (int)sq.super_res;
                        sh.enable_cdef              = (int)sq.cdef;
                        sh.enable_restoration       = (int)sq.restoration;
                        sh.film_grain_params_present = (int)sq.film_grain_present;
                        sh.timing_info_present         = (int)sq.timing_info_present;
                        sh.decoder_model_info_present  = (int)sq.decoder_model_info_present;
                        sh.display_model_info_present  = (int)sq.display_model_info_present;
                        sh.operating_points_cnt        = (int)sq.num_operating_points;
                        sh.color_description_present = (int)sq.color_description_present;
                        sh.color_primaries           = (int)sq.pri;
                        sh.transfer_characteristics  = (int)sq.trc;
                        sh.matrix_coefficients       = (int)sq.mtrx;
                        sh.color_range               = (int)sq.color_range;
                        sh.chroma_sample_position     = (int)sq.chr;
                        sh.monochrome    = sq.monochrome ? 1 : 0;
                        sh.bit_depth     = 8 + (int)sq.hbd * 2;
                        sh.subsampling_x = sq.ss_hor ? 1 : 0;
                        sh.subsampling_y = sq.ss_ver ? 1 : 0;
                        probe_seq_hbd  = sq.hbd;
                        probe_seq_mono = sq.monochrome ? 1 : 0;
                    }
#endif
                    seq_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME_HEADER:
                case STB_AV1_OBU_REDUNDANT_FRAME_HEADER: {
                    struct stb_av1_bool_reader fh_br;

                    stb_av1_bool_reader_init(&fh_br,
                                               obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb),
                                               (size_t)obu_size);

                    stb_av1_parse_frame_header(&fh, &sh, &fh_br);
                    frame_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME: {
                    /* Combined frame header + tile group OBU */
                    /* For simplicity, we handle frame + tile group separately */
                    struct stb_av1_bool_reader frame_br;

                    stb_av1_bool_reader_init(&frame_br,
                                               obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb),
                                               (size_t)obu_size);

                    /* Frame OBU contains frame header followed by tile group data.
                       Parse frame header first. */
                    stb_av1_parse_frame_header(&fh, &sh, &frame_br);
                    frame_header_found = 1;

                    /* Remaining data in the OBU is tile group data.
                       We'll read it right here using the same reader. */
                    if (fh.show_existing_frame) {
                        /* nothing to decode */
                    } else {
                        /* The position in frame_br is now at the tile data.
                           Use it for tile decoding. */
                        /* Store the boolean reader position for tile decoding */
                        br = frame_br;
                    }
                    break;
                }
                case STB_AV1_OBU_TILE_GROUP: {
                    /* We already parsed frame header; this is tile data.
                       Transfer the boolean reader from current position. */
                    /* The tile group data starts at obu_gb position */
                    if (frame_header_found) {
                        br.data = obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb);
                        br.size = (size_t)obu_size;
                        br.pos = 0;
                        br.value = 0;
                        br.range = 128;
                        br.count = 0;
                        br.error = 0;
                        /* Re-init properly */
                        stb_av1_bool_reader_init(&br,
                                                   obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb),
                                                   (size_t)obu_size);
                    }
                    break;
                }
                case STB_AV1_OBU_TEMPORAL_DELIMITER:
                case STB_AV1_OBU_METADATA:
                case STB_AV1_OBU_PADDING:
                default:
                    break;
            }

            /* Advance past this OBU's data */
            if (obu_has_size_field && obu_size > 0) {
                stb_av1_getbits_seek(&obu_gb, stb_av1_getbits_bytepos(&obu_gb) + (size_t)obu_size);
            } else if (obu_has_size_field) {
                /* OBU with has_size_field=1 and size=0 is valid (e.g. temporal delimiter).
                   Just skip the header+size bytes we already consumed. */
                /* Already advanced past header+size, nothing more to skip. */
            } else {
                /* No size field: determine from remaining data or break on unknown */
                if (stb_av1_getbits_bytepos(&obu_gb) < stb_av1_getbits_size(&obu_gb))
                    stb_av1_getbits_seek(&obu_gb, stb_av1_getbits_size(&obu_gb)); /* consume all remaining */
                else
                    break;
            }

            /* Check if we've found end of OBUs */
            if (stb_av1_getbits_bytepos(&obu_gb) >= stb_av1_getbits_size(&obu_gb))
                more_obus = 0;
        }

        /* Fallback: no sequence-header OBU in the item stream (AVIF
         * encoders often put it only inside av1C).  Parse the config
         * OBU from av1C now - AFTER the stream walk, so the stream's
         * own seq header always wins. */
        if (!seq_header_found && info.av1c_size > 0) {
            struct stb_av1_getbits config_gb;
            int config_obu_type, config_obu_ext, config_obu_hassize;
            stbv_u32 config_obu_sz;

            stb_av1_getbits_init(&config_gb, info.av1c_data, (size_t)info.av1c_size);
            stb_av1_read_obu_header(&config_gb, &config_obu_type,
                                     &config_obu_ext, &config_obu_hassize);
            if (config_obu_hassize)
                config_obu_sz = stb_av1_read_obu_size(&config_gb);
            else
                config_obu_sz = (stbv_u32)(info.av1c_size - stb_av1_getbits_bytepos(&config_gb));

            if (config_obu_type == STB_AV1_OBU_SEQUENCE_HEADER && config_obu_sz > 0) {
                seq_header_found = 1;
            }
        }

        /* For reduced still_picture_header, restore dimensions from ISPE */
        if (sh.reduced_still_picture_header && info.width > 0 && info.height > 0) {
            sh.max_frame_width = info.width;
            sh.max_frame_height = info.height;
        }
        STB_AVIF_CHECK(seq_header_found, "No AV1 sequence header found");
        sh_parsed_ok = 1;
    }

#ifndef STB_AVIF_USE_DAV1D
    /* Authoritative sequence-header parse: stb_av1_parse_internal_stream
     * uses raw-bit f(n) parsing that matches dav1d bit-for-bit.  It owns
     * ALL geometry/colour/filtering fields so we no longer need the
     * legacy bool-reader based parser above. */
    {
        struct stb_av1_internal_stream probe;
        if (stb_av1_parse_internal_stream(&probe, info.av1_data,
                                          info.av1_size) == 0 &&
            probe.have_seq) {
            const struct stb_av1_seqhdr *sq = &probe.seq;

            /* Geometry */
            sh.frame_width_bits  = (int)sq->width_n_bits;
            sh.frame_height_bits = (int)sq->height_n_bits;
            sh.max_frame_width   = (int)sq->max_width;
            sh.max_frame_height  = (int)sq->max_height;

            /* Profile / picture type */
            sh.seq_profile                = (int)sq->profile;
            sh.still_picture              = (int)sq->still_picture;
            sh.reduced_still_picture_header = (int)sq->reduced_still_picture_header;

            /* Tool flags (consumed by frame header parser) */
            sh.enable_order_hint        = (int)sq->order_hint;
            sh.enable_dist_wtd_comp     = 0;
            sh.enable_masked_comp       = (int)sq->masked_compound;
            sh.enable_intra_edge_filter = (int)sq->intra_edge_filter;
            sh.enable_interintra_comp   = (int)sq->inter_intra;
            sh.enable_dual_filter       = (int)sq->dual_filter;
            sh.enable_jnt_comp          = (int)sq->jnt_comp;
            sh.enable_superres          = (int)sq->super_res;
            sh.enable_cdef              = (int)sq->cdef;
            sh.enable_restoration       = (int)sq->restoration;
            sh.film_grain_params_present = (int)sq->film_grain_present;

            /* Timing / decoder model */
            sh.timing_info_present         = (int)sq->timing_info_present;
            sh.decoder_model_info_present  = (int)sq->decoder_model_info_present;
            sh.display_model_info_present  = (int)sq->display_model_info_present;
            sh.operating_points_cnt        = (int)sq->num_operating_points;

            /* Colour */
            sh.color_description_present = (int)sq->color_description_present;
            sh.color_primaries           = (int)sq->pri;
            sh.transfer_characteristics  = (int)sq->trc;
            sh.matrix_coefficients       = (int)sq->mtrx;
            sh.color_range               = (int)sq->color_range;
            sh.chroma_sample_position     = (int)sq->chr;

            /* Pixel format */
            sh.monochrome    = sq->monochrome ? 1 : 0;
            sh.bit_depth     = 8 + (int)sq->hbd * 2;
            sh.subsampling_x = sq->ss_hor ? 1 : 0;
            sh.subsampling_y = sq->ss_ver ? 1 : 0;

            probe_seq_hbd  = sq->hbd;
            probe_seq_mono = sq->monochrome ? 1 : 0;
            sh_parsed_ok   = 1;
        }
    }
#endif /* !STB_AVIF_USE_DAV1D */

    /* After sequence header parse, override info.width/height with the
     * real bitstream dimensions.  IVF headers or ispe boxes can be wrong
     * (e.g. thumbnail dimensions); the sequence header is authoritative. */
    if (sh.max_frame_width > 0 && sh.max_frame_height > 0) {
        info.width  = sh.max_frame_width;
        info.height = sh.max_frame_height;
    }

    /* If we didn't find a frame header, use defaults for still picture */
    if (!fh.frame_width || !fh.frame_height) {
        fh.frame_width = (int)sh.max_frame_width;
        fh.frame_height = (int)sh.max_frame_height;
        fh.frame_type = STB_AV1_KEY_FRAME;
        fh.show_frame = 1;
        fh.base_q_idx = 100; /* reasonable default */
        fh.cdef_damping = 4;
        fh.cdef_bits = 0;
        fh.tx_mode = 2; /* SELECT */
        /* enable_cdef in sh, not fh */
    }

    /* Geometry comes from the REAL OBU sequence header parsed above
     * (bit-exact vs dav1d on the whole sample set).  av1C is only a
     * fallback: some encoders write wrong subsampling there (e.g. 444
     * in av1C for a profile-2 stream whose seq header forces ss_x=1),
     * which used to corrupt plane allocation and YUV->RGB sampling.
     * Only fill fields when the OBU parse left them unset. */
    if (!sh_parsed_ok) {
        sh.monochrome = info.monochrome;
        sh.bit_depth = info.bit_depth;
        sh.subsampling_x = info.chroma_subsampling_x;
        sh.subsampling_y = info.chroma_subsampling_y;
    }

    /* Allocate image planes */
    info.stride_y = (info.width + 31) & ~31;
    info.stride_u = ((info.width >> sh.subsampling_x) + 31) & ~31;
    info.stride_v = info.stride_u;

    /* +64 rows of padding so intra edge gathering on partial bottom-edge
     * blocks (frame height not a multiple of 4/8) stays in bounds. */
    info.plane_y = (unsigned char *)stb_avif_calloc(
        (size_t)(info.stride_y * (info.height + 64)), 1);
    if (sh.monochrome) {
        info.plane_u = NULL;
        info.plane_v = NULL;
    } else {
        int uv_rows = (info.height + (1 << sh.subsampling_y) - 1) >> sh.subsampling_y;
        info.plane_u = (unsigned char *)stb_avif_calloc(
            (size_t)(info.stride_u * (uv_rows + 32)), 1);
        info.plane_v = (unsigned char *)stb_avif_calloc(
            (size_t)(info.stride_v * (uv_rows + 32)), 1);
    }

    /* Initialize tile context */
    tc.sh = &sh;
    tc.fh = &fh;
    tc.frame_width = fh.frame_width;
    tc.frame_height = fh.frame_height;
    tc.mb_cols = (tc.frame_width + 3) / 4;
    tc.mb_rows = (tc.frame_height + 3) / 4;
    tc.br = &br;
    tc.qindex_y = fh.base_q_idx;
    tc.qindex_u = fh.base_q_idx;
    tc.qindex_v = fh.base_q_idx;
    tc.plane_y = info.plane_y;
    tc.plane_u = info.plane_u;
    tc.plane_v = info.plane_v;
    tc.stride_y = info.stride_y;
    tc.stride_u = info.stride_u;
    tc.stride_v = info.stride_v;
    tc.bit_depth = sh.bit_depth;
    tc.tile_row = 0;
    tc.tile_col = 0;
    tc.done_sb = 0;
    tc.total_sb = 1;
    tc.next_report_sb = 1;
    tc.start_time = 0;

    /* Allocate pixel max */
    tc.pixel_max = (1 << sh.bit_depth) - 1;

#ifdef STB_AVIF_USE_DAV1D
    {
        int dav1d_w, dav1d_h;
        int dav1d_bd, dav1d_mono, dav1d_sx, dav1d_sy;
        int dav1d_cr, dav1d_mc;
        unsigned char *dav1d_y = NULL, *dav1d_u = NULL, *dav1d_v = NULL;
        int dav1d_ys, dav1d_us, dav1d_vs;
        int dav1d_ok;

        dav1d_ok = stb_avif_decode_with_dav1d(
            info.av1_data, info.av1_size,
            &dav1d_w, &dav1d_h,
            &dav1d_y, &dav1d_ys,
            &dav1d_u, &dav1d_us,
            &dav1d_v, &dav1d_vs,
            &dav1d_bd, &dav1d_mono, &dav1d_sx, &dav1d_sy,
            &dav1d_cr, &dav1d_mc);

        if (dav1d_ok) {
            /* Replace internal planes with dav1d output */
            if (info.plane_y) stb_avif_free_internal(info.plane_y);
            if (info.plane_u) stb_avif_free_internal(info.plane_u);
            if (info.plane_v) stb_avif_free_internal(info.plane_v);
            info.plane_y = dav1d_y;
            info.plane_u = dav1d_u;
            info.plane_v = dav1d_v;
            info.stride_y = dav1d_ys;
            info.stride_u = dav1d_us;
            info.stride_v = dav1d_vs;
            info.width = dav1d_w;
            info.height = dav1d_h;
            sh.bit_depth = dav1d_bd;
            sh.monochrome = dav1d_mono;
            sh.subsampling_x = dav1d_sx;
            sh.subsampling_y = dav1d_sy;
            sh.color_range = dav1d_cr;
            sh.matrix_coefficients = dav1d_mc;
        } else {
            stb_avif_error_msg = "dav1d decode failed";
            goto error_exit;
        }
    }
#else
    {
        int r = stb_avif_decode_frame_scalar(&tc, info.av1_data, info.av1_size);
        if (r < 0) {
            stb_avif_error_msg = "scalar AV1 decode failed";
            goto error_exit;
        }
    }
#endif

    /* ---- auxiliary alpha item (AVIF auxl -> primary) ---- */
    if (info.alpha_item_id > 0 && !info.alpha_plane) {
        /* Locate the alpha item's coded data with a fresh iloc scan. */
        struct stb_av1_getbits ar_gb;
        size_t ameta_start = 0;
        int found_iloc_box = 0;
        {
            /* re-find the meta box payload start */
            size_t scan = 0;
            while (scan + 8 <= (size_t)len) {
                stbv_u32 bsz = ((stbv_u32)data[scan]<<24)|((stbv_u32)data[scan+1]<<16)|
                               ((stbv_u32)data[scan+2]<<8)|data[scan+3];
                stbv_u32 bty = ((stbv_u32)data[scan+4]<<24)|((stbv_u32)data[scan+5]<<16)|
                               ((stbv_u32)data[scan+6]<<8)|data[scan+7];
                if (bsz < 8) break;
                if (bty == STB_AVIF_FOURCC('m','e','t','a')) { ameta_start = scan + 8 + 4; break; }
                scan += bsz;
            }
        }
        if (ameta_start) {
            size_t ap = ameta_start;
            size_t aend = info.meta_end_offset;
            ap = ameta_start;
            while (ap + 8 <= aend) {
                stbv_u32 bsz = ((stbv_u32)data[ap]<<24)|((stbv_u32)data[ap+1]<<16)|
                               ((stbv_u32)data[ap+2]<<8)|data[ap+3];
                stbv_u32 bty = ((stbv_u32)data[ap+4]<<24)|((stbv_u32)data[ap+5]<<16)|
                               ((stbv_u32)data[ap+6]<<8)|data[ap+7];
                if (bsz < 8 || ap + bsz > aend + 4096) break;
                if (bty == STB_AVIF_FOURCC('i','l','o','c')) {
                    stbv_u32 aoff = 0; stbv_u64 asz = 0;
                                stb_av1_getbits_init(&ar_gb, data + ap + 8, bsz - 8);
                    stb_avif_parse_iloc(&ar_gb, &info, info.alpha_item_id,
                                        &aoff, &asz);
                    if (asz > 0 && aoff + asz <= (stbv_u64)len) {
                        info.alpha_av1 = info.input + aoff;
                        info.alpha_size = (size_t)asz;
                    }
                    found_iloc_box = 1;
                    break;
                }
                ap += bsz;
            }
        }
        (void)found_iloc_box;
        if (info.alpha_av1 && info.alpha_size > 0 &&
            !sh.monochrome) {
#ifndef STB_AVIF_USE_DAV1D
            /* Decode the mono alpha stream with the scalar path. */
            struct stb_av1_tile_context tc2;
            struct stb_av1_internal_stream astream;
            memset(&astream, 0, sizeof(astream));
            if (stb_av1_parse_internal_stream(&astream, info.alpha_av1,
                                              info.alpha_size) == 0 &&
                astream.have_seq) {
                /* C89: declarations first */
                int abd = 8 + (int)astream.seq.hbd * 2;
                int aw = fh.frame_width, ah = fh.frame_height;
                int astride = (aw + 31) & ~31;
                unsigned char *aplane;
                if (astream.seq.monochrome)
                    aplane = (unsigned char *)stb_avif_calloc(
                        (size_t)astride * (ah + 64), 1);
                else
                    aplane = NULL;
                if (aplane) {
                        memset(&tc2, 0, sizeof(tc2));
                        tc2.sh = &sh; tc2.fh = &fh;
                        tc2.frame_width = aw; tc2.frame_height = ah;
                        tc2.mb_cols = (aw + 3) / 4; tc2.mb_rows = (ah + 3) / 4;
                        tc2.qindex_y = fh.base_q_idx; tc2.qindex_u = fh.base_q_idx;
                        tc2.qindex_v = fh.base_q_idx;
                        tc2.plane_y = aplane;
                        tc2.stride_y = astride;
                        tc2.bit_depth = abd;
                        tc2.pixel_max = (1 << abd) - 1;
                        sh.bit_depth = abd;
                        sh.monochrome = 1;
                        if (stb_avif_decode_frame_scalar(&tc2,
                                info.alpha_av1, info.alpha_size) == 0) {
                            int yy2;
                            info.alpha_plane = (unsigned char *)stb_avif_malloc(
                                (size_t)aw * ah);
                            if (info.alpha_plane)
                                for (yy2 = 0; yy2 < ah; yy2++)
                                    memcpy(info.alpha_plane + (size_t)yy2 * aw,
                                           aplane + (size_t)yy2 * astride, aw);
                            info.alpha_stride = aw;
                        }
                        stb_avif_free_internal(aplane);
                        /* restore colour description clobbered above */
                        sh.bit_depth = 8 + (int)probe_seq_hbd * 2;
                        sh.monochrome = probe_seq_mono;
                    }
            }
#endif /* !STB_AVIF_USE_DAV1D */
        }
    }

    /* Determine output channels */
    output_channels = req_channels;
    if (output_channels == 0) {
        if (sh.monochrome)
            output_channels = 1;
        else
            output_channels = 4; /* RGBA */
    }

    /* Allocate output buffer with proper RGBA conversion */
    result = (unsigned char *)stb_avif_malloc(
        (size_t)(info.width * info.height * output_channels));
    if (!result) {
        stb_avif_error_msg = "Out of memory";
        goto error_exit;
    }

    /* Convert YUV to RGB */
    if (sh.monochrome && output_channels == 1) {
        /* Direct copy of luma */
        int row, col;
        for (row = 0; row < info.height; row++) {
            for (col = 0; col < info.width; col++) {
                result[row * info.width * output_channels + col] =
                    info.plane_y[row * info.stride_y + col];
            }
        }
    } else {
        /* YUV (4:2:0 or 4:4:4) to RGBA conversion */
        int row, col;
        int uv_h = (info.height + (1 << sh.subsampling_y) - 1) >> sh.subsampling_y;
        int uv_w = (info.width + (1 << sh.subsampling_x) - 1) >> sh.subsampling_x;

        for (row = 0; row < info.height; row++) {
            for (col = 0; col < info.width; col++) {
                int y_val, u_val, v_val;
                int r, g, b;

                y_val = (int)info.plane_y[row * info.stride_y + col];

                if (sh.monochrome || !info.plane_u || !info.plane_v) {
                    /* neutral chroma for monochrome -> grey = luma */
                    u_val = 128;
                    v_val = 128;
                } else if (sh.subsampling_x > 0 || sh.subsampling_y > 0) {
                    /* any subsampling (4:2:0 AND 4:2:2) needs scaled
                     * chroma coords; the old y-only test made 4:2:2
                     * sample chroma at full rate -> half-width
                     * stretched colour bands. */
                    int uv_r = row >> sh.subsampling_y;
                    int uv_c = col >> sh.subsampling_x;
                    if (uv_r >= uv_h) uv_r = uv_h - 1;
                    if (uv_c >= uv_w) uv_c = uv_w - 1;
                    if (uv_r < 0) uv_r = 0;
                    if (uv_c < 0) uv_c = 0;
                    u_val = (int)info.plane_u[uv_r * info.stride_u + uv_c];
                    v_val = (int)info.plane_v[uv_r * info.stride_v + uv_c];
                } else {
                    u_val = (int)info.plane_u[row * info.stride_u + col];
                    v_val = (int)info.plane_v[row * info.stride_v + col];
                }

                /* NOTE: planes are already scaled to 8-bit in the
                 * u16 -> u8 conversion above (lines ~3662-3745).
                 * Do NOT apply a second bit-depth shift here. */

                /* Range expansion for limited range (color_range=0) */
                if (sh.color_range == 0) {
                    y_val = ((y_val - 16) * 255) / 219;
                    if (y_val < 0) y_val = 0;
                    if (y_val > 255) y_val = 255;
                }
                u_val -= 128;
                v_val -= 128;

                /* Color matrix based on sequence header matrix_coefficients */
                {
                    int mc = sh.matrix_coefficients;
                    if (mc == 0) {
                        /* AV1/H.273 matrix 0 is GBR: Y=G, Cb=B, Cr=R.
                         * u_val/v_val were centered above, so restore the
                         * unsigned plane values here. */
                        r = v_val + 128;
                        g = y_val;
                        b = u_val + 128;
                    } else if (mc >= 8 && mc <= 10) {
                        r = y_val + ((378 * v_val) >> 8);
                        g = y_val - ((42 * u_val + 120 * v_val) >> 8);
                        b = y_val + ((482 * u_val) >> 8);
                    } else if (mc == 1 || mc == 2) {
                        r = y_val + ((403 * v_val) >> 8);
                        g = y_val - ((48 * u_val + 120 * v_val) >> 8);
                        b = y_val + ((475 * u_val) >> 8);
                    } else {
                        r = y_val + ((359 * v_val) >> 8);
                        g = y_val - ((88 * u_val + 183 * v_val) >> 8);
                        b = y_val + ((454 * u_val) >> 8);
                    }
                }

                /* Clamp */
                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;

                result[(row * info.width + col) * output_channels + 0] = (unsigned char)r;
                result[(row * info.width + col) * output_channels + 1] = (unsigned char)g;
                result[(row * info.width + col) * output_channels + 2] = (unsigned char)b;

                if (output_channels == 4) {
                    if (info.alpha_plane)
                        result[(row * info.width + col) * 4 + 3] =
                            info.alpha_plane[(size_t)row * info.alpha_stride + col];
                    else
                        result[(row * info.width + col) * 4 + 3] = 255;
                }
            }
        }
    }

    stb_avif_g_last_alpha = info.alpha_plane;
    stb_avif_g_last_alpha_stride = info.alpha_plane ? info.alpha_stride : 0;

    /* Keep YUV planes alive for external access via stb_avif_last_yuv(). */
    stb_avif_g_last_yuv_y = info.plane_y;
    stb_avif_g_last_yuv_u = info.plane_u;
    stb_avif_g_last_yuv_v = info.plane_v;
    stb_avif_g_last_yuv_stride_y = info.stride_y;
    stb_avif_g_last_yuv_stride_u = info.stride_u;
    stb_avif_g_last_yuv_stride_v = info.stride_v;
    info.plane_y = NULL;  /* prevent free — ownership transferred to global */
    info.plane_u = NULL;
    info.plane_v = NULL;

    /* Set output parameters */
    *x = info.width;
    *y = info.height;
    *channels = output_channels;

    /* Cleanup */
    if (info.ivf_concat_buf) stb_avif_free_internal(info.ivf_concat_buf);
    /* plane_y/u/v ownership transferred to global above — do not free here */

    stb_avif_error_msg = "no error";
    return result;

error_exit:
    if (info.ivf_concat_buf) stb_avif_free_internal(info.ivf_concat_buf);
    if (info.plane_y) stb_avif_free_internal(info.plane_y);
    if (info.plane_u) stb_avif_free_internal(info.plane_u);
    if (info.plane_v) stb_avif_free_internal(info.plane_v);
    if (result) stb_avif_free_internal(result);
    return NULL;
}

#endif /* STB_AVIF_IMPLEMENTATION */
