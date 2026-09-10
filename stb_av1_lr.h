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

/* V-filter using 6 stored rows + 1 new row. Matches dav1d's wiener_filter_hv. */
static void stbv_av1_wiener_hv(unsigned short *p, unsigned short **ptrs,
                               const unsigned short *src, int src_stride,
                               int w, const signed short *fh, const signed short *fv,
                               int bit_depth)
{
    const int round_bits_v = 11 - (bit_depth == 12 ? 2 : 0);
    const int round_off_v = 1 << (round_bits_v - 1);
    const int round_offset = 1 << (bit_depth + (round_bits_v - 1));
    const int maxv = (1 << bit_depth) - 1;
    unsigned short tmp[STBV_LR_REST_UNIT_STRIDE];
    int i;

    /* H-filter the new source row into tmp */
    stbv_av1_wiener_filter_h(tmp, src, src_stride, w, fh, bit_depth);

    /* V-filter: 6 stored rows + 1 new row in tmp */
    for (i = 0; i < w; i++) {
        int sum = -round_offset;
        int k;
        for (k = 0; k < 6; k++)
            sum += ptrs[k][i] * fv[k];
        sum += tmp[i] * fv[6];
        p[i] = stbv_av1_lr_clip16((sum + round_off_v) >> round_bits_v, maxv);
    }

    /* Copy tmp into ptrs[6] and rotate */
    for (i = 0; i < w; i++)
        ptrs[6][i] = tmp[i];
    for (i = 0; i < 6; i++)
        ptrs[i] = ptrs[i + 1];
    ptrs[6] = ptrs[0];
}

/* V-filter for bottom padding: uses ptrs[0..5] + ptrs[5] (duplicated last row).
 * Matches dav1d's wiener_filter_v. */
static void stbv_av1_wiener_v_only(unsigned short *p, unsigned short **ptrs,
                                   int w, const signed short *fv, int bit_depth)
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
        sum += ptrs[5][i] * fv[6];
        p[i] = stbv_av1_lr_clip16((sum + round_off_v) >> round_bits_v, maxv);
    }
    for (i = 0; i < 5; i++)
        ptrs[i] = ptrs[i + 1];
}

/* Apply Wiener filter to a rectangular region of a plane.
 * Matches dav1d's wiener_c() ring buffer structure exactly. */
static void stbv_av1_wiener_plane(unsigned short *plane, int stride,
                                  int frame_w, int frame_h,
                                  int ux0, int uy0, int uw, int uh,
                                  const signed char *raw_fv, const signed char *raw_fh,
                                  int bit_depth,
                                  const unsigned short *lpf, int lpf_stride)
{
    unsigned short hor[6 * STBV_LR_REST_UNIT_STRIDE];
    unsigned short *ptrs[7], *rows[6];
    signed short fh[7], fv[7];
    int i, h;
    int have_top, have_bottom;
    const unsigned short *src;
    unsigned short *p;
    const unsigned short *lpf_bottom;

    if (uw <= 0 || uh <= 0) return;

    for (i = 0; i < 6; i++)
        rows[i] = &hor[i * STBV_LR_REST_UNIT_STRIDE];

    /* Build symmetric 7-tap filter from 3 parameters */
    fh[0] = fh[6] = raw_fh[0];
    fh[1] = fh[5] = raw_fh[1];
    fh[2] = fh[4] = raw_fh[2];
    fh[3] = (signed short)(-(fh[0] + fh[1] + fh[2]) * 2 + 128);
    fv[0] = fv[6] = raw_fv[0];
    fv[1] = fv[5] = raw_fv[1];
    fv[2] = fv[4] = raw_fv[2];
    fv[3] = (signed short)(128 - (fv[0] + fv[1] + fv[2]) * 2);

    have_top = (uy0 > 0);
    have_bottom = (uy0 + uh < frame_h);

    p = plane + uy0 * stride + ux0;
    src = p;
    lpf_bottom = lpf + 6 * lpf_stride;
    h = uh;

    if (have_top) {
        const unsigned short *lpf_top = lpf + (uy0 - 2) * lpf_stride + ux0;

        ptrs[0] = rows[0];
        ptrs[1] = rows[0];
        ptrs[2] = rows[1];
        ptrs[3] = rows[2];
        ptrs[4] = rows[2];
        ptrs[5] = rows[2];

        /* H-filter 2 lpf rows (deblocked, pre-LR) */
        stbv_av1_wiener_filter_h(rows[0], lpf_top, lpf_stride, uw, fh, bit_depth);
        lpf_top += lpf_stride;
        stbv_av1_wiener_filter_h(rows[1], lpf_top, lpf_stride, uw, fh, bit_depth);

        /* H-filter 1st src row */
        stbv_av1_wiener_filter_h(rows[2], src, stride, uw, fh, bit_depth);
        src += stride;

        if (--h <= 0) goto v1;

        ptrs[4] = ptrs[5] = rows[3];
        stbv_av1_wiener_filter_h(rows[3], src, stride, uw, fh, bit_depth);
        src += stride;

        if (--h <= 0) goto v2;

        ptrs[5] = rows[4];
        stbv_av1_wiener_filter_h(rows[4], src, stride, uw, fh, bit_depth);
        src += stride;

        if (--h <= 0) goto v3;
    } else {
        ptrs[0] = rows[0];
        ptrs[1] = rows[0];
        ptrs[2] = rows[0];
        ptrs[3] = rows[0];
        ptrs[4] = rows[0];
        ptrs[5] = rows[0];

        stbv_av1_wiener_filter_h(rows[0], src, stride, uw, fh, bit_depth);
        src += stride;

        if (--h <= 0) goto v1;

        ptrs[4] = ptrs[5] = rows[1];
        stbv_av1_wiener_filter_h(rows[1], src, stride, uw, fh, bit_depth);
        src += stride;

        if (--h <= 0) goto v2;

        ptrs[5] = rows[2];
        stbv_av1_wiener_filter_h(rows[2], src, stride, uw, fh, bit_depth);
        src += stride;

        if (--h <= 0) goto v3;

        ptrs[6] = rows[3];
        stbv_av1_wiener_hv(p, ptrs, src, stride, uw, fh, fv, bit_depth);
        src += stride;
        p += stride;

        if (--h <= 0) goto v3;

        ptrs[6] = rows[4];
        stbv_av1_wiener_hv(p, ptrs, src, stride, uw, fh, fv, bit_depth);
        src += stride;
        p += stride;

        if (--h <= 0) goto v3;
    }

    ptrs[6] = ptrs[5] + STBV_LR_REST_UNIT_STRIDE;
    do {
        stbv_av1_wiener_hv(p, ptrs, src, stride, uw, fh, fv, bit_depth);
        src += stride;
        p += stride;
    } while (--h > 0);

    if (!have_bottom)
        goto v3;

    stbv_av1_wiener_hv(p, ptrs, lpf_bottom, lpf_stride, uw, fh, fv, bit_depth);
    lpf_bottom += lpf_stride;
    p += stride;

    stbv_av1_wiener_hv(p, ptrs, lpf_bottom, lpf_stride, uw, fh, fv, bit_depth);
    p += stride;

v1:
    stbv_av1_wiener_v_only(p, ptrs, uw, fv, bit_depth);
    return;

v3:
    stbv_av1_wiener_v_only(p, ptrs, uw, fv, bit_depth);
    p += stride;
v2:
    stbv_av1_wiener_v_only(p, ptrs, uw, fv, bit_depth);
    p += stride;
    goto v1;
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
                                    const unsigned short *src, int w, int ex0)
{
    /* x ranges from -1 to w inclusive; indices into sum/sumsq are offset by +1 */
    int a, b, c, x;
    sumsq++; sum++;
    a = (ex0 >= 2) ? src[-2] : src[0];
    b = (ex0 >= 1) ? src[-1] : src[0];
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
                                    const unsigned short *src, int w, int ex0)
{
    int a, b, c, d, x;
    sumsq++; sum++;
    a = (ex0 >= 3) ? src[-3] : src[0];
    b = (ex0 >= 2) ? src[-2] : src[0];
    c = (ex0 >= 1) ? src[-1] : src[0];
    d = src[0];
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

/* Compute unweighted 3x3 SGR filter output.
 * Reads from src (const, not modified), writes uh rows to out_tmp.
 * Each output row at out_tmp[y * 384] for y in [0, uh). */
static void stbv_av1_sgr_compute_3x3(signed short *out_tmp,
                                      const unsigned short *src, int src_stride,
                                      int frame_w, int frame_h,
                                      int ux0, int uy0, int uw, int uh,
                                      int s1,
                                      const unsigned short *lpf, int lpf_stride)
{
    int BUF = 384 + 16;
    int *sumsq_buf, *sum_buf;
    int *A_buf, *B_buf;
    int *sumsq_rows[3], *sum_rows[3];
    int *A_ptrs[3], *B_ptrs[3];
    int *sumsq_ptrs[3], *sum_ptrs[3];
    int y, i;
    int ew = uw + 2;
    signed short tmp[384];

    if (ux0 + ew > frame_w) ew = frame_w - ux0;
    if (ew <= 0 || uw <= 0 || uh <= 0) return;

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

    /* Pre-fill rows[0] and rows[1] from above the LR unit (border rows).
     * These are the 2 rows above the first output row. */
    {
        int row0 = uy0 > 2 ? uy0 - 2 : 0;
        int row1 = uy0 > 1 ? uy0 - 1 : 0;
        const unsigned short *r0 = src + row0 * src_stride + ux0;
        const unsigned short *r1 = src + row1 * src_stride + ux0;
        stbv_av1_sgr_box3_row_h(sumsq_ptrs[0], sum_ptrs[0], r0, ew, ux0);
        stbv_av1_sgr_box3_row_h(sumsq_ptrs[1], sum_ptrs[1], r1, ew, ux0);
    }

    /* Main loop: process uh+2 rows starting from uy0.
     * Output rows are produced when y >= 2 (i.e., after 2 A/B rows are filled). */
    for (y = 0; y < uh + 2; y++) {
        int row = uy0 + y;
        int row_clamped;
        const unsigned short *src_ptr;

        if (row >= frame_h) row_clamped = frame_h - 1;
        else row_clamped = row;

        src_ptr = src + row_clamped * src_stride + ux0;

        stbv_av1_sgr_box3_row_h(sumsq_ptrs[2], sum_ptrs[2], src_ptr, ew, ux0);
        stbv_av1_sgr_box3_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[2], B_ptrs[2], uw);
        stbv_av1_sgr_calc_ab(A_ptrs[2], B_ptrs[2], uw, s1, 9, 455);
        stbv_av1_rotate3(sumsq_ptrs);
        stbv_av1_rotate3(sum_ptrs);

        if (y >= 2) {
            int out_y = uy0 + (y - 2);
            const unsigned short *src_row = src + out_y * src_stride + ux0;
            stbv_av1_sgr_finish_filter_row1(out_tmp + (y - 2) * 384,
                                            src_row,
                                            (const int *const *)A_ptrs,
                                            (const int *const *)B_ptrs,
                                            uw);
        }
        stbv_av1_rotate3(A_ptrs);
        stbv_av1_rotate3(B_ptrs);
    }

    stb_avif_free_internal(sumsq_buf);
    stb_avif_free_internal(sum_buf);
    stb_avif_free_internal(A_buf);
    stb_avif_free_internal(B_buf);
}

/* Standalone 3x3 SGR: compute + apply weight to dst */
static void stbv_av1_sgr_3x3(unsigned short *dst, int stride,
                              int frame_w, int frame_h,
                              int ux0, int uy0, int uw, int uh,
                              int s1, int w1,
                              const unsigned short *lpf, int lpf_stride)
{
    int y;
    signed short *out_tmp;
    if (uw <= 0 || uh <= 0) return;
    out_tmp = (signed short *)stb_avif_calloc((size_t)uh * 384, sizeof(signed short));
    if (!out_tmp) return;
    stbv_av1_sgr_compute_3x3(out_tmp, dst, stride, frame_w, frame_h,
                              ux0, uy0, uw, uh, s1, lpf, lpf_stride);
    for (y = 0; y < uh; y++)
        stbv_av1_sgr_weighted_row1(dst + (uy0 + y) * stride + ux0,
                                   out_tmp + y * 384, uw, w1);
    stb_avif_free_internal(out_tmp);
}

/* ---- SGR 5x5 filter ---- */

/* Compute unweighted 5x5 SGR filter output.
 * Reads from src (const, not modified), writes uh rows to out_tmp.
 * Each output row at out_tmp[y * 384] for y in [0, uh).
 *
 * This mirrors dav1d's sgr_5x5_c structure:
 * - Write 2 hsum rows to ptrs[3] and ptrs[4]
 * - box5_vert: vert sum of ptrs[0..4], write A[1], rotate ptrs by 2
 * - sgr_finish2: filter 2 rows using A, rotate A by 2 */
static void stbv_av1_sgr_compute_5x5(signed short *out_tmp,
                                      const unsigned short *src, int src_stride,
                                      int frame_w, int frame_h,
                                      int ux0, int uy0, int uw, int uh,
                                      int s0,
                                      const unsigned short *lpf, int lpf_stride)
{
    int BUF = 384 + 16;
    int *sumsq_buf, *sum_buf;
    int *A_buf, *B_buf;
    int *sumsq_rows[5], *sum_rows[5];
    int *sumsq_ptrs[5], *sum_ptrs[5];
    int *A_ptrs[2], *B_ptrs[2];
    int y, i;
    int ew = uw + 4;
    int h;
    int src_y;
    int out_y;
    int clamped_row;
    const unsigned short *r;

    if (ux0 + ew > frame_w) ew = frame_w - ux0;
    if (ew <= 0 || uw <= 0 || uh <= 0) return;

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

    /* Pre-fill: same row aliased for ptrs[0] and ptrs[1] (dav1d convention).
     * ptrs[0]=ptrs[1]=uy0-2 (or 0), ptrs[2]=uy0-1 (or 0). */
    {
        int row0 = uy0 > 2 ? uy0 - 2 : 0;
        int row1 = uy0 > 1 ? uy0 - 1 : 0;
        r = src + row0 * src_stride + ux0;
        stbv_av1_sgr_box5_row_h(sumsq_rows[0], sum_rows[0], r, ew, ux0);
        r = src + row1 * src_stride + ux0;
        stbv_av1_sgr_box5_row_h(sumsq_rows[1], sum_rows[1], r, ew, ux0);
    }
    sumsq_ptrs[0] = sumsq_rows[0];
    sumsq_ptrs[1] = sumsq_rows[0];
    sumsq_ptrs[2] = sumsq_rows[1];
    sum_ptrs[0] = sum_rows[0];
    sum_ptrs[1] = sum_rows[0];
    sum_ptrs[2] = sum_rows[1];

    h = uh;
    src_y = uy0;
    out_y = uy0;

    /* Phase 1: write 2 src rows (ptrs[3] and ptrs[4]), compute box5_vert, rotate. */
    clamped_row = src_y < frame_h ? src_y : frame_h - 1;
    r = src + clamped_row * src_stride + ux0;
    stbv_av1_sgr_box5_row_h(sumsq_rows[2], sum_rows[2], r, ew, ux0);
    src_y++;

    if (--h <= 0) {
        /* Only 1 row total: duplicate and process */
        stbv_av1_sgr_box5_row_h(sumsq_rows[3], sum_rows[3], r, ew, ux0);
        sumsq_ptrs[4] = sumsq_ptrs[3];
        sum_ptrs[4] = sum_ptrs[3];
        stbv_av1_sgr_box5_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[1], B_ptrs[1], uw);
        stbv_av1_sgr_calc_ab(A_ptrs[1], B_ptrs[1], uw, s0, 25, 164);
        stbv_av1_rotate2(A_ptrs);
        stbv_av1_rotate2(B_ptrs);
        /* Output 1 row */
        {
            const unsigned short *dst_row = src + out_y * src_stride + ux0;
            stbv_av1_sgr_finish_filter_row2(out_tmp + (out_y - uy0) * 384,
                                             dst_row, src_stride,
                                             (const int *const *)A_ptrs,
                                             (const int *const *)B_ptrs,
                                             uw, 1);
        }
        goto done;
    }

    clamped_row = src_y < frame_h ? src_y : frame_h - 1;
    r = src + clamped_row * src_stride + ux0;
    stbv_av1_sgr_box5_row_h(sumsq_rows[3], sum_rows[3], r, ew, ux0);
    src_y++;

    stbv_av1_sgr_box5_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[1], B_ptrs[1], uw);
    stbv_av1_sgr_calc_ab(A_ptrs[1], B_ptrs[1], uw, s0, 25, 164);
    stbv_av1_rotate5(sumsq_ptrs);
    stbv_av1_rotate5(sum_ptrs);
    stbv_av1_rotate2(A_ptrs);
    stbv_av1_rotate2(B_ptrs);

    if (--h <= 0) goto done;

    /* Fix ptrs[3] to a fresh buffer (dav1d: sumsq_ptrs[3] = sumsq_rows[4]). */
    sumsq_ptrs[3] = sumsq_rows[4];
    sum_ptrs[3] = sum_rows[4];

    /* Phase 2: main loop - write 2 rows, box5_vert, sgr_finish2 (2 output rows). */
    do {
        clamped_row = src_y < frame_h ? src_y : frame_h - 1;
        r = src + clamped_row * src_stride + ux0;
        stbv_av1_sgr_box5_row_h(sumsq_ptrs[3], sum_ptrs[3], r, ew, ux0);
        src_y++;

        if (--h <= 0) {
            /* Odd row: duplicate last, output 1 row */
            stbv_av1_sgr_box5_row_h(sumsq_ptrs[4], sum_ptrs[4], r, ew, ux0);
            stbv_av1_sgr_box5_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[1], B_ptrs[1], uw);
            stbv_av1_sgr_calc_ab(A_ptrs[1], B_ptrs[1], uw, s0, 25, 164);
            {
                const unsigned short *dst_row = src + out_y * src_stride + ux0;
                stbv_av1_sgr_finish_filter_row2(out_tmp + (out_y - uy0) * 384,
                                                 dst_row, src_stride,
                                                 (const int *const *)A_ptrs,
                                                 (const int *const *)B_ptrs,
                                                 uw, 1);
            }
            stbv_av1_rotate5(sumsq_ptrs);
            stbv_av1_rotate5(sum_ptrs);
            stbv_av1_rotate2(A_ptrs);
            stbv_av1_rotate2(B_ptrs);
            /* Output last row */
            out_y++;
            {
                const unsigned short *dst_row = src + out_y * src_stride + ux0;
                stbv_av1_sgr_finish_filter_row2(out_tmp + (out_y - uy0) * 384,
                                                 dst_row, src_stride,
                                                 (const int *const *)A_ptrs,
                                                 (const int *const *)B_ptrs,
                                                 uw, 1);
            }
            stbv_av1_rotate2(A_ptrs);
            stbv_av1_rotate2(B_ptrs);
            goto done;
        }

        clamped_row = src_y < frame_h ? src_y : frame_h - 1;
        r = src + clamped_row * src_stride + ux0;
        stbv_av1_sgr_box5_row_h(sumsq_ptrs[4], sum_ptrs[4], r, ew, ux0);
        src_y++;

        stbv_av1_sgr_box5_row_v((const int *const *)sumsq_ptrs, (const int *const *)sum_ptrs, A_ptrs[1], B_ptrs[1], uw);
        stbv_av1_sgr_calc_ab(A_ptrs[1], B_ptrs[1], uw, s0, 25, 164);
        /* Output 2 rows */
        {
            const unsigned short *dst_row = src + out_y * src_stride + ux0;
            stbv_av1_sgr_finish_filter_row2(out_tmp + (out_y - uy0) * 384,
                                             dst_row, src_stride,
                                             (const int *const *)A_ptrs,
                                             (const int *const *)B_ptrs,
                                             uw, 2);
        }
        out_y += 2;
        stbv_av1_rotate5(sumsq_ptrs);
        stbv_av1_rotate5(sum_ptrs);
        stbv_av1_rotate2(A_ptrs);
        stbv_av1_rotate2(B_ptrs);
    } while (--h > 0);

done:
    stb_avif_free_internal(sumsq_buf);
    stb_avif_free_internal(sum_buf);
    stb_avif_free_internal(A_buf);
    stb_avif_free_internal(B_buf);
}

/* Standalone 5x5 SGR: compute + apply weight to dst */
static void stbv_av1_sgr_5x5(unsigned short *dst, int stride,
                              int frame_w, int frame_h,
                              int ux0, int uy0, int uw, int uh,
                              int s0, int w0,
                              const unsigned short *lpf, int lpf_stride)
{
    int y;
    signed short *out_tmp;
    if (uw <= 0 || uh <= 0) return;
    out_tmp = (signed short *)stb_avif_calloc((size_t)uh * 384, sizeof(signed short));
    if (!out_tmp) return;
    stbv_av1_sgr_compute_5x5(out_tmp, dst, stride, frame_w, frame_h,
                              ux0, uy0, uw, uh, s0, lpf, lpf_stride);
    for (y = 0; y < uh; y++)
        stbv_av1_sgr_weighted_row1(dst + (uy0 + y) * stride + ux0,
                                   out_tmp + y * 384, uw, w0);
    stb_avif_free_internal(out_tmp);
}

/* ---- SGR mix (5x5 + 3x3) filter ---- */
/* Both filters read from the same original src (dst before modification),
 * produce unweighted outputs in tmp5/tmp3, then blend via sgr_weighted2. */
static void stbv_av1_sgr_mix(unsigned short *dst, int stride,
                              int frame_w, int frame_h,
                              int ux0, int uy0, int uw, int uh,
                              int s0, int s1, int w0, int w1,
                              const unsigned short *lpf, int lpf_stride)
{
    signed short *tmp5, *tmp3;
    int y;
    if (uw <= 0 || uh <= 0) return;
    tmp5 = (signed short *)stb_avif_calloc((size_t)uh * 384, sizeof(signed short));
    tmp3 = (signed short *)stb_avif_calloc((size_t)uh * 384, sizeof(signed short));
    if (!tmp5 || !tmp3) {
        if (tmp5) stb_avif_free_internal(tmp5);
        if (tmp3) stb_avif_free_internal(tmp3);
        return;
    }
    /* Both compute functions read from dst (original) without modifying it */
    stbv_av1_sgr_compute_5x5(tmp5, dst, stride, frame_w, frame_h,
                              ux0, uy0, uw, uh, s0, lpf, lpf_stride);
    stbv_av1_sgr_compute_3x3(tmp3, dst, stride, frame_w, frame_h,
                              ux0, uy0, uw, uh, s1, lpf, lpf_stride);
    /* Blend both filter outputs onto dst, offset to LR unit position */
    stbv_av1_sgr_weighted2(dst + uy0 * stride + ux0, stride, tmp5, tmp3, uw, uh, w0, w1);
    stb_avif_free_internal(tmp5);
    stb_avif_free_internal(tmp3);
}

/* ---- Frame-level LR application ---- */

/* Apply loop restoration to the entire frame.
 * Called after CDEF, before 8-bit conversion. */
/* Apply frame-level LR. We save a copy of each plane before processing
 * (the "lpf" buffer). Both Wiener and SGR filters read edge rows from
 * this copy (deblocked, pre-LR), matching dav1d's lr_lpf_line behavior. */
static void stb_av1_lr_frame(unsigned short *plane_y, unsigned short *plane_u,
                             unsigned short *plane_v,
                             int stride_y, int stride_u, int stride_v,
                             int frame_w, int frame_h,
                             int ss_hor, int ss_ver, int bit_depth,
                             const stbv_av1_lr_mask *m)
{
    int p;
    unsigned short *lpf_planes[3];
    int lpf_strides[3];
    if (!m) return;

    /* Save pre-LR copies for edge rows */
    for (p = 0; p < 3; p++) {
        int chroma = p > 0;
        int ss_h = chroma ? ss_hor : 0;
        int ss_v = chroma ? ss_ver : 0;
        int w = (frame_w + ss_h) >> ss_h;
        int h = (frame_h + ss_v) >> ss_v;
        int stride = chroma ? (p == 1 ? stride_u : stride_v) : stride_y;
        unsigned short *plane = chroma ? (p == 1 ? plane_u : plane_v) : plane_y;
        unsigned short *lpf;
        int y;

        lpf_planes[p] = NULL;
        lpf_strides[p] = stride;

        /* Quick check: any non-NONE types? */
        {
            int any_non_none = 0;
            int gw = m->grid_stride[p];
            int gr = m->grid_rows[p];
            int gy, gx;
            for (gy = 0; gy < gr && !any_non_none; gy++)
                for (gx = 0; gx < gw && !any_non_none; gx++)
                    if (m->units[p][gy * gw + gx].type != STBV_AV1_RESTORATION_NONE)
                        any_non_none = 1;
            if (!any_non_none) continue;
        }

        lpf = (unsigned short *)stb_avif_calloc((size_t)stride * h, sizeof(unsigned short));
        if (!lpf) continue;
        for (y = 0; y < h; y++)
            memcpy(lpf + y * stride, plane + y * stride, w * sizeof(unsigned short));
        lpf_planes[p] = lpf;
    }

    for (p = 0; p < 3; p++) {
        int chroma = p > 0;
        int ss_h = chroma ? ss_hor : 0;
        int ss_v = chroma ? ss_ver : 0;
        int w = (frame_w + ss_h) >> ss_h;
        int h = (frame_h + ss_v) >> ss_v;
        int stride = chroma ? (p == 1 ? stride_u : stride_v) : stride_y;
        unsigned short *plane = chroma ? (p == 1 ? plane_u : plane_v) : plane_y;
        unsigned short *lpf = lpf_planes[p];
        int usz = m->unit_size_log2[chroma ? 1 : 0];
        int unit_sz = 1 << usz;
        int gw = m->grid_stride[p];
        int gr = m->grid_rows[p];
        int gy, gx;

        if (!lpf) continue;

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
                                          bit_depth, lpf, stride);
                } else if (u->type >= STBV_AV1_RESTORATION_SGRPROJ) {
                    int s0 = stbv_av1_sgr_tab[u->sgr_idx][0];
                    int s1 = stbv_av1_sgr_tab[u->sgr_idx][1];
                    int w0 = u->sgr_weights[0];
                    int w1_adj = 128 - (u->sgr_weights[0] + u->sgr_weights[1]);

                    if (s0 && s1)
                        stbv_av1_sgr_mix(plane, stride, w, h,
                                         ux0, uy0, uw, uh,
                                         s0, s1, w0, w1_adj, lpf, stride);
                    else if (s0)
                        stbv_av1_sgr_5x5(plane, stride, w, h,
                                         ux0, uy0, uw, uh, s0, w0, lpf, stride);
                    else if (s1)
                        stbv_av1_sgr_3x3(plane, stride, w, h,
                                         ux0, uy0, uw, uh, s1, w1_adj, lpf, stride);
                }
                /* NONE: no-op */
            }
        }
    }

    /* Free lpf copies */
    for (p = 0; p < 3; p++) {
        if (lpf_planes[p]) stb_avif_free_internal(lpf_planes[p]);
    }
}

#endif /* STB_AV1_LR_H */
