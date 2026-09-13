/*
 * stb_av1_deblock.h - scalar AV1 in-loop deblocking filter
 *
 * Faithful port of dav1d's loopfilter_tmpl.c kernel plus the edge
 * selection rules: each 4x4 row independently checks for a transform-
 * block or prediction-block boundary.  Filter width per edge =
 * 4 << min(lw_left, lw_right, cap), cap 2 for luma / 1 for chroma
 * (uv widths 4 or 6).  Per-block levels from lf_level map when
 * provided; otherwise falls back to frame-global level.
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
                (p6*7 + p5*2 + p4*2 + p3 + p2 + p1 + p0 + q0 + 8) >> 4, 0, maxv);
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
                (p0 + q0 + q1 + q2 + q3 + q4*2 + q5*2 + q6*7 + 8) >> 4, 0, maxv);
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
 *   level_v/h   - base LF level for vertical/horizontal edges (0 disables)
 *   sharpness   - frame sharpness
 *   is_chroma   - caps filter width at 6
 *   blkid       - per-4x4-unit block-identity map
 *   txlw        - per-4x4-unit log2-width of the covering transform
 *   b4stride    - row stride of the blkid/txlw maps (4x4 units)
 *   mapw4, maph4 - map extent in 4x4 units
 *   ssx, ssy    - plane subsampling relative to the maps' grid
 *   lf_level    - per-4x4-block [vert,horiz] LF level (NULL = use scalar)
 *   b4stride_lf - row stride of lf_level map
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
                                       int sb_size,
                                       const stbv_u8 *lf_level,
                                       ptrdiff_t b4stride_lf)
{
    int lut_i[64], lut_e[64];
    int L, X, Y;

    if (!level_v && !level_h) return;

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
            int bx_r = (X << ssx) >> 2;
            int xl_c = (bx_r - 1) < mapw4 ? bx_r - 1 : mapw4 - 1;
            int xr_c = bx_r < mapw4 ? bx_r : mapw4 - 1;
            int tile_blocked = 0;
            if (tile_col_start_sb && tile_cols > 1) {
                int tc;
                for (tc = 1; tc < tile_cols; tc++) {
                    int tbx = (int)((tile_col_start_sb[tc] * (unsigned int)sb_size) >> ssx);
                    if (X == tbx) { tile_blocked = 1; break; }
                }
            }
            if (tile_blocked) continue;
            for (Y = 0; Y < h; Y += 4) {
                int yy = (Y << ssy) >> 2;
                int yl = yy < maph4 ? yy : maph4 - 1;
                stbv_u32 bl = blkid[(size_t)yl * b4stride + xl_c];
                stbv_u32 br = blkid[(size_t)yl * b4stride + xr_c];
                int ll = txlw[(size_t)yl * b4stride + xl_c];
                int lr = txlw[(size_t)yl * b4stride + xr_c];
                if (bl != br || ll != lr) {
                    int bucket = ll < lr ? ll : lr;
                    ptrdiff_t sb_p = 1;
                    ptrdiff_t sa_p = stride;
                    stbv_u16 *q0;
                    int wd;
                    if (bucket > (is_chroma ? 1 : 2)) bucket = is_chroma ? 1 : 2;
                    if (bucket < 0) bucket = 0;
                    if (lf_level) {
                        L = lf_level[((size_t)yl * b4stride_lf + xr_c) * 2 + 0];
                        if (!L) L = level_v;
                    } else {
                        L = level_v;
                    }
                    q0 = p + (size_t)Y * stride + X;
                    wd = 4 << bucket;
                    if (is_chroma) wd = 4 + 2 * bucket;
                    if (wd >= 16 && X < 7) wd = 8;
                    if (wd >= 8 && X < 4) wd = is_chroma ? 6 : 4;
                    if (wd >= 6 && X < 3) wd = 4;
                    if (wd >= 4 && X < 2) continue;
                    stb_av1_loop_filter_edge(q0, sa_p, sb_p, lut_e[L], lut_i[L],
                                             L >> 4, wd, maxv, bd8);
                }
            }
        }
    }

    /* ---- horizontal edges (at px Y = multiples of 4) ---- */
    if (level_h) {
        for (Y = 4; Y < h; Y += 4) {
            int by_r = (Y << ssy) >> 2;
            int yt_c = (by_r - 1) < maph4 ? by_r - 1 : maph4 - 1;
            int yb_c = by_r < maph4 ? by_r : maph4 - 1;
            int tile_blocked = 0;
            if (tile_row_start_sb && tile_rows > 1) {
                int tr;
                for (tr = 1; tr < tile_rows; tr++) {
                    int tby = (int)((tile_row_start_sb[tr] * (unsigned int)sb_size) >> ssy);
                    if (Y == tby) { tile_blocked = 1; break; }
                }
            }
            if (tile_blocked) continue;
            for (X = 0; X < w; X += 4) {
                int xx = (X << ssx) >> 2;
                int xt_c = xx < mapw4 ? xx : mapw4 - 1;
                stbv_u32 bu = blkid[(size_t)yt_c * b4stride + xt_c];
                stbv_u32 bd = blkid[(size_t)yb_c * b4stride + xt_c];
                int lu = txlw[(size_t)yt_c * b4stride + xt_c];
                int ld = txlw[(size_t)yb_c * b4stride + xt_c];
                if (bu != bd || lu != ld) {
                    int bucket = lu < ld ? lu : ld;
                    ptrdiff_t sb_p = stride;
                    ptrdiff_t sa_p = 1;
                    stbv_u16 *q0;
                    int wd;
                    if (bucket > (is_chroma ? 1 : 2)) bucket = is_chroma ? 1 : 2;
                    if (bucket < 0) bucket = 0;
                    if (lf_level) {
                        L = lf_level[((size_t)yb_c * b4stride_lf + xt_c) * 2 + 1];
                        if (!L) L = level_h;
                    } else {
                        L = level_h;
                    }
                    q0 = p + (size_t)Y * stride + X;
                    wd = 4 << bucket;
                    if (is_chroma) wd = 4 + 2 * bucket;
                    if (wd >= 16 && Y < 7) wd = 8;
                    if (wd >= 8 && Y < 4) wd = is_chroma ? 6 : 4;
                    if (wd >= 6 && Y < 3) wd = 4;
                    if (wd >= 4 && Y < 2) continue;
                    stb_av1_loop_filter_edge(q0, sa_p, sb_p, lut_e[L], lut_i[L],
                                             L >> 4, wd, maxv, bd8);
                }
            }
        }
    }
}

#endif /* STB_AV1_DEBLOCK_H */
