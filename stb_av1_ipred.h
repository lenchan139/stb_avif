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
    dx = stbv_av1_dr_deriv[stbv_av1_ipred_iclip(angle >> 1, 0, 43)]; \
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
    dy = stbv_av1_dr_deriv[stbv_av1_ipred_iclip((angle - 90) >> 1, 0, 43)]; \
    dx = stbv_av1_dr_deriv[stbv_av1_ipred_iclip((180 - angle) >> 1, 0, 43)]; \
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
    dy = stbv_av1_dr_deriv[stbv_av1_ipred_iclip((270 - angle) >> 1, 0, 43)]; \
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
