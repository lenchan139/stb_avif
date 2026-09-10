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
    return strength * (4 + i) >> 4;
}

/* --- Filter kernel --- */

/* Filter a w*h block (w,h in {4,8}).
 * dst points to the block in the frame buffer.
 * dst_stride, frame_w, frame_h: frame geometry.
 * bx, by: block position in pixels within the frame.
 * edges: CDEF_HAVE_* flags (currently unused — edge pixels are clamped). */
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

    /* Fill tmp with the block + 2-pixel padding on each side.
     * Read from dst (live frame) for all pixels. For out-of-bounds pixels
     * (frame edges), clamp coordinates to frame bounds — this replicates
     * edge pixels, matching dav1d's CDEF_HAVE_* edge handling. */
    for (y = -2; y < h + 2; y++) {
        for (x = -2; x < w + 2; x++) {
            int fx = bx + x, fy = by + y;
            int cx = (fx < 0) ? 0 : (fx >= frame_w) ? frame_w - 1 : fx;
            int cy = (fy < 0) ? 0 : (fy >= frame_h) ? frame_h - 1 : fy;
            tmp[y * tmp_stride + x] = (stbv_i16)dst[cy * dst_stride + cx];
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
                        int p0 = tmp[y * tmp_stride + x + off1];
                        int p1 = tmp[y * tmp_stride + x - off1];
                        sum += pri_tap_k * stb_av1_cdef_constrain(p0 - px, pri_strength, pri_shift);
                        sum += pri_tap_k * stb_av1_cdef_constrain(p1 - px, pri_strength, pri_shift);
                        pri_tap_k = (pri_tap_k & 3) | 2;
                        if ((unsigned)p0 < (unsigned)min) min = p0;
                        if (p0 > max) max = p0;
                        if ((unsigned)p1 < (unsigned)min) min = p1;
                        if (p1 > max) max = p1;
                        {
                            int off2 = cdef_dirs[4][k];
                            int off3 = cdef_dirs[0][k];
                            int s0 = tmp[y * tmp_stride + x + off2];
                            int s1 = tmp[y * tmp_stride + x - off2];
                            int s2 = tmp[y * tmp_stride + x + off3];
                            int s3 = tmp[y * tmp_stride + x - off3];
                            int sec_tap = 2 - k;
                            sum += sec_tap * stb_av1_cdef_constrain(s0 - px, sec_strength, sec_shift);
                            sum += sec_tap * stb_av1_cdef_constrain(s1 - px, sec_strength, sec_shift);
                            sum += sec_tap * stb_av1_cdef_constrain(s2 - px, sec_strength, sec_shift);
                            sum += sec_tap * stb_av1_cdef_constrain(s3 - px, sec_strength, sec_shift);
                            if ((unsigned)s0 < (unsigned)min) min = s0;
                            if (s0 > max) max = s0;
                            if ((unsigned)s1 < (unsigned)min) min = s1;
                            if (s1 > max) max = s1;
                            if ((unsigned)s2 < (unsigned)min) min = s2;
                            if (s2 > max) max = s2;
                            if ((unsigned)s3 < (unsigned)min) min = s3;
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
                        int p0 = tmp[y * tmp_stride + x + off];
                        int p1 = tmp[y * tmp_stride + x - off];
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
                    int s0 = tmp[y * tmp_stride + x + off1];
                    int s1 = tmp[y * tmp_stride + x - off1];
                    int s2 = tmp[y * tmp_stride + x + off2];
                    int s3 = tmp[y * tmp_stride + x - off2];
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
                                 int cdef_damping,
                                 const stbv_u8 *noskip_mask, int noskip_stride)
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

            /* Compute strengths. */
            y_pri_lvl = (y_pri[cdef_idx] << 2) << bitdepth_min_8;
            y_sec_lvl = y_sec[cdef_idx];
            y_sec_lvl += (y_sec_lvl == 3);
            y_sec_lvl <<= bitdepth_min_8;

            uv_pri_lvl = (uv_pri[cdef_idx] << 2) << bitdepth_min_8;
            uv_sec_lvl = uv_sec[cdef_idx];
            uv_sec_lvl += (uv_sec_lvl == 3);
            uv_sec_lvl <<= bitdepth_min_8;

            /* Filter luma: process 8x8 blocks within the 64x64 SB.
             * Direction and variance are computed per 8x8 block (matching dav1d). */
            {
                int lx, ly;
                for (ly = 0; ly < bh; ly += 8) {
                    for (lx = 0; lx < bw; lx += 8) {
                        int bbw = (lx + 8 <= bw) ? 8 : bw - lx;
                        int bbh = (ly + 8 <= bh) ? 8 : bh - ly;
                        int bedges = edges;
                        int block_y_pri_lvl = y_pri_lvl;
                        int block_dir = 0;
                        unsigned block_var = 0;
                        /* Skip blocks without coded coefficients (matching dav1d noskip_mask). */
                        if (noskip_mask) {
                            int abs_row8 = (by + ly) >> 3;
                            int abs_col8 = (bx + lx) >> 3;
                            int byte_idx = abs_row8 * noskip_stride + sb64_x;
                            if (!(noskip_mask[byte_idx] & (1 << (abs_col8 & 7))))
                                continue;
                        }
                        if (ly > 0) bedges |= CDEF_HAVE_TOP;
                        if (ly + 8 < bh) bedges |= CDEF_HAVE_BOTTOM;
                        if (lx > 0) bedges |= CDEF_HAVE_LEFT;
                        if (lx + 8 < bw) bedges |= CDEF_HAVE_RIGHT;
                        if (y_pri_lvl || uv_pri_lvl) {
                            block_dir = stb_av1_cdef_find_dir(
                                &plane_y[(by + ly) * stride_y + (bx + lx)], stride_y,
                                &block_var, bitdepth_min_8);
                            block_y_pri_lvl = stb_av1_cdef_adjust_strength(
                                (y_pri[cdef_idx] << 2) << bitdepth_min_8, block_var);
                        }
                        if (block_y_pri_lvl || y_sec_lvl) {
                            stb_av1_cdef_filter_block(
                                plane_y, stride_y, bx + lx, by + ly,
                                frame_w, frame_h,
                                block_y_pri_lvl, y_sec_lvl,
                                block_dir, damping, bbw, bbh, bedges,
                                bitdepth_min_8);
                        }
                    }
                }
            }

            /* Filter chroma. */
            if (uv_pri_lvl || uv_sec_lvl) {
                static const stbv_u8 uv_dirs_i420[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
                static const stbv_u8 uv_dirs_i422[8] = { 7, 0, 2, 4, 5, 6, 6, 6 };
                const stbv_u8 *uv_dir = (ss_ver && !ss_hor) ? uv_dirs_i420 : uv_dirs_i422;
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
                            int uvdir = 0;
                            /* Skip chroma blocks whose corresponding luma 8x8 block has no coded coefficients. */
                            if (noskip_mask) {
                                int luma_row8 = (by + (cly << ss_ver)) >> 3;
                                int luma_col8 = (bx + (clx << ss_hor)) >> 3;
                                int byte_idx = luma_row8 * noskip_stride + sb64_x;
                                if (!(noskip_mask[byte_idx] & (1 << (luma_col8 & 7))))
                                    continue;
                            }
                            if (cly > 0) cbedges |= CDEF_HAVE_TOP;
                            if (cly + 8 < cbh) cbedges |= CDEF_HAVE_BOTTOM;
                            if (clx > 0) cbedges |= CDEF_HAVE_LEFT;
                            if (clx + 8 < cbw) cbedges |= CDEF_HAVE_RIGHT;
                            /* Compute chroma direction from the corresponding luma block. */
                            if (uv_pri_lvl) {
                                int lx2 = clx << ss_hor;
                                int ly2 = cly << ss_ver;
                                if (lx2 < bw && ly2 < bh) {
                                    unsigned luma_var;
                                    int luma_dir = stb_av1_cdef_find_dir(
                                        &plane_y[(by + ly2) * stride_y + (bx + lx2)], stride_y,
                                        &luma_var, bitdepth_min_8);
                                    uvdir = uv_dir[luma_dir];
                                }
                            }
                            stb_av1_cdef_filter_block(
                                plane, stride, cbx + clx, cby + cly,
                                cw, ch,
                                uv_pri_lvl, uv_sec_lvl,
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
