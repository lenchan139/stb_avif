/* C89 port of dav1d's decode.c - intra/still-image only, simplified */

/*
 * NOTE: This file is designed to be included into stb_avif.h when
 * STB_AVIF_USE_C89_DAV1D is defined. All types, MSAC functions, and
 * external helpers are provided by stb_avif.h.
 *
 * Only KEY frame / INTRA_ONLY frame paths are kept.
 * All inter prediction, frame threading, film grain, segmentation,
 * intrabc, super-res, CDEF and loop restoration scheduling are removed.
 *
 * Transformations applied:
 *  - C89 declarations at block start
 *  - // comments -> /* */
 *  - No inline, no restrict
 *  - uint8_t -> unsigned char, etc.
 *  - dav1d_ -> stb_av1_ prefix
 *  - Type renames as per plan
 */

/* ================================================================
 *  init_quant_tables - simplified, no segmentation (single segment)
 * ================================================================ */
static void stb_init_quant_tables(const StbAv1SequenceHeader *seq_hdr,
                                   const StbAv1FrameHeader *frame_hdr,
                                   const int qidx,
                                   unsigned short (*dq)[3][2])
{
    const int yac = qidx;
    const int ydc = iclip_u8(yac + frame_hdr->quant.ydc_delta);
    const int uac = iclip_u8(yac + frame_hdr->quant.uac_delta);
    const int udc = iclip_u8(yac + frame_hdr->quant.udc_delta);
    const int vac = iclip_u8(yac + frame_hdr->quant.vac_delta);
    const int vdc = iclip_u8(yac + frame_hdr->quant.vdc_delta);

    dq[0][0][0] = stb_av1_dq_tbl[seq_hdr->hbd][ydc][0];
    dq[0][0][1] = stb_av1_dq_tbl[seq_hdr->hbd][yac][1];
    dq[0][1][0] = stb_av1_dq_tbl[seq_hdr->hbd][udc][0];
    dq[0][1][1] = stb_av1_dq_tbl[seq_hdr->hbd][uac][1];
    dq[0][2][0] = stb_av1_dq_tbl[seq_hdr->hbd][vdc][0];
    dq[0][2][1] = stb_av1_dq_tbl[seq_hdr->hbd][vac][1];
}

/* ================================================================
 *  neg_deinterleave - kept for delta_q/lf coding
 * ================================================================ */
static int neg_deinterleave(int diff, int ref, int max)
{
    if (!ref) return diff;
    if (ref >= (max - 1)) return max - diff - 1;
    if (2 * ref < max) {
        if (diff <= 2 * ref) {
            if (diff & 1)
                return ref + ((diff + 1) >> 1);
            else
                return ref - (diff >> 1);
        }
        return diff;
    } else {
        if (diff <= 2 * (max - ref - 1)) {
            if (diff & 1)
                return ref + ((diff + 1) >> 1);
            else
                return ref - (diff >> 1);
        }
        return max - (diff + 1);
    }
}

/* ================================================================
 *  read_tx_tree - kept for transform tree parsing
 * ================================================================ */
static void stb_read_tx_tree(StbAv1TaskContext *const t,
                              const enum RectTxfmSize from,
                              const int depth, unsigned short *const masks,
                              const int x_off, const int y_off)
{
    const StbAv1FrameContext *const f = t->f;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const StbAv1TxfmInfo *const t_dim = &stb_av1_txfm_dimensions[from];
    const int txw = t_dim->lw, txh = t_dim->lh;
    int is_split;

    if (depth < 2 && from > (int) TX_4X4) {
        const int cat = 2 * (TX_64X64 - t_dim->max) - depth;
        const int a = t->a->tx[bx4] < txw;
        const int l = t->l.tx[by4] < txh;

        is_split = stb_av1_msac_decode_bool_adapt(&t->ts->msac,
                       t->ts->cdf.m.txpart[cat][a + l]);
        if (is_split)
            masks[depth] |= 1 << (y_off * 4 + x_off);
    } else {
        is_split = 0;
    }

    if (is_split && t_dim->max > TX_8X8) {
        const enum RectTxfmSize sub = t_dim->sub;
        const StbAv1TxfmInfo *const sub_t_dim = &stb_av1_txfm_dimensions[sub];
        const int txsw = sub_t_dim->w, txsh = sub_t_dim->h;

        stb_read_tx_tree(t, sub, depth + 1, masks, x_off * 2 + 0, y_off * 2 + 0);
        t->bx += txsw;
        if (txw >= txh && t->bx < f->bw)
            stb_read_tx_tree(t, sub, depth + 1, masks, x_off * 2 + 1, y_off * 2 + 0);
        t->bx -= txsw;
        t->by += txsh;
        if (txh >= txw && t->by < f->bh) {
            stb_read_tx_tree(t, sub, depth + 1, masks, x_off * 2 + 0, y_off * 2 + 1);
            t->bx += txsw;
            if (txw >= txh && t->bx < f->bw)
                stb_read_tx_tree(t, sub, depth + 1, masks,
                                 x_off * 2 + 1, y_off * 2 + 1);
            t->bx -= txsw;
        }
        t->by -= txsh;
    } else {
        stb_av1_memset_pow2[t_dim->lw](&t->a->tx[bx4], is_split ? TX_4X4 : txw);
        stb_av1_memset_pow2[t_dim->lh](&t->l.tx[by4], is_split ? TX_4X4 : txh);
    }
}

/* ================================================================
 *  order_palette - kept for palette mode
 * ================================================================ */
/* meant to be SIMD'able, so that theoretical complexity of this function */
/* times block size goes from w4*h4 to w4+h4-1 */
/* a and b are previous two lines containing (a) top/left entries or (b) */
/* top/left entries, with a[0] being either the first top or first left entry, */
/* depending on top_offset being 1 or 0, and b being the first top/left entry */
/* for whichever has one. left_offset indicates whether the (len-1)th entry */
/* has a left neighbour. */
/* output is order[] and ctx for each member of this diagonal. */
static void order_palette(const unsigned char *pal_idx, int stride,
                          const int i, const int first, const int last,
                          unsigned char (*const order)[8], unsigned char *const ctx)
{
    int have_top;
    int j, n;
    int o_idx;
    unsigned mask;
    int l, t, tl;
    int same_t_l, same_t_tl, same_l_tl, same_all;
    unsigned m, bit;

    have_top = i > first;

    pal_idx += first + (i - first) * stride;
    for (j = first, n = 0; j >= last; have_top = 1, j--, n++, pal_idx += stride - 1) {
        const int have_left = j > 0;

        mask = 0;
        o_idx = 0;
        if (!have_left) {
            ctx[n] = 0;
            /* add(pal_idx[-stride]) */
            { const int v = pal_idx[-stride]; order[n][o_idx++] = v; mask |= 1 << v; }
        } else if (!have_top) {
            ctx[n] = 0;
            /* add(pal_idx[-1]) */
            { const int v = pal_idx[-1]; order[n][o_idx++] = v; mask |= 1 << v; }
        } else {
            l = pal_idx[-1]; t = pal_idx[-stride]; tl = pal_idx[-(stride + 1)];
            same_t_l = t == l;
            same_t_tl = t == tl;
            same_l_tl = l == tl;
            same_all = same_t_l & same_t_tl & same_l_tl;

            if (same_all) {
                ctx[n] = 4;
                /* add(t) */
                { const int v = t; order[n][o_idx++] = v; mask |= 1 << v; }
            } else if (same_t_l) {
                ctx[n] = 3;
                /* add(t) */
                { const int v = t; order[n][o_idx++] = v; mask |= 1 << v; }
                /* add(tl) */
                { const int v = tl; order[n][o_idx++] = v; mask |= 1 << v; }
            } else if (same_t_tl | same_l_tl) {
                ctx[n] = 2;
                /* add(tl) */
                { const int v = tl; order[n][o_idx++] = v; mask |= 1 << v; }
                /* add(same_t_tl ? l : t) */
                { const int v = same_t_tl ? l : t; order[n][o_idx++] = v; mask |= 1 << v; }
            } else {
                ctx[n] = 1;
                /* add(imin(t, l)) */
                { const int v = imin(t, l); order[n][o_idx++] = v; mask |= 1 << v; }
                /* add(imax(t, l)) */
                { const int v = imax(t, l); order[n][o_idx++] = v; mask |= 1 << v; }
                /* add(tl) */
                { const int v = tl; order[n][o_idx++] = v; mask |= 1 << v; }
            }
        }
        for (m = 1, bit = 0; m < 0x100; m <<= 1, bit++)
            if (!(mask & m))
                order[n][o_idx++] = bit;
    }
}

/* ================================================================
 *  read_pal_indices - kept for palette mode
 * ================================================================ */
static void stb_read_pal_indices(StbAv1TaskContext *const t,
                                  unsigned char *const pal_idx,
                                  const int pal_sz, const int pl,
                                  const int w4, const int h4,
                                  const int bw4, const int bh4)
{
    StbAv1TileState *const ts = t->ts;
    int stride;
    unsigned char *pal_tmp;
    unsigned short (*color_map_cdf)[8];
    unsigned char (*order)[8];
    unsigned char *ctx;
    int i, j, m;
    int first, last;

    stride = bw4 * 4;
    pal_tmp = t->scratch.pal_idx_uv;
    pal_tmp[0] = stb_av1_msac_decode_uniform(&ts->msac, pal_sz);
    color_map_cdf = ts->cdf.m.color_map[pl][pal_sz - 2];
    order = t->scratch.pal_order;
    ctx = t->scratch.pal_ctx;
    for (i = 1; i < 4 * (w4 + h4) - 1; i++) {
        /* top/left-to-bottom/right diagonals ("wave-front") */
        first = imin(i, w4 * 4 - 1);
        last = imax(0, i - h4 * 4 + 1);
        order_palette(pal_tmp, stride, i, first, last, order, ctx);
        for (j = first, m = 0; j >= last; j--, m++) {
            const int color_idx = stb_av1_msac_decode_symbol(&ts->msac,
                                      color_map_cdf[ctx[m]], pal_sz);
            pal_tmp[(i - j) * stride + j] = order[m][color_idx];
        }
    }
}

/* ================================================================
 *  read_vartx_tree - kept for variable transform size
 *  (simplified: no seg.lossless, always TX_SWITCHABLE)
 * ================================================================ */
static void stb_read_vartx_tree(StbAv1TaskContext *const t,
                                 StbAv1Block *const b,
                                 const enum BlockSize bs,
                                 const int bx4, const int by4)
{
    const StbAv1FrameContext *const f = t->f;
    const unsigned char *const b_dim = stb_av1_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    unsigned short tx_split[2];
    int y, x, y_off, x_off;
    const StbAv1TxfmInfo *ytx;

    tx_split[0] = 0;
    tx_split[1] = 0;

    /* var-tx tree coding */
    b->max_ytx = stb_av1_max_txfm_size_for_bs[bs][0];
    if (!b->skip && b->max_ytx == TX_4X4) {
        b->max_ytx = b->uvtx = TX_4X4;
        stb_av1_memset_pow2[b_dim[2]](&t->a->tx[bx4], TX_4X4);
        stb_av1_memset_pow2[b_dim[3]](&t->l.tx[by4], TX_4X4);
    } else if (b->skip) {
        stb_av1_memset_pow2[b_dim[2]](&t->a->tx[bx4], b_dim[2 + 0]);
        stb_av1_memset_pow2[b_dim[3]](&t->l.tx[by4], b_dim[2 + 1]);
        b->uvtx = stb_av1_max_txfm_size_for_bs[bs][f->cur.p.layout];
    } else {
        ytx = &stb_av1_txfm_dimensions[b->max_ytx];
        for (y = 0, y_off = 0; y < bh4; y += ytx->h, y_off++) {
            for (x = 0, x_off = 0; x < bw4; x += ytx->w, x_off++) {
                stb_read_tx_tree(t, b->max_ytx, 0, tx_split, x_off, y_off);
                /* contexts are updated inside read_tx_tree() */
                t->bx += ytx->w;
            }
            t->bx -= x;
            t->by += ytx->h;
        }
        t->by -= y;
        b->uvtx = stb_av1_max_txfm_size_for_bs[bs][f->cur.p.layout];
    }
    b->tx_split0 = (unsigned char)tx_split[0];
    b->tx_split1 = tx_split[1];
}

/* ================================================================
 *  stb_decode_b - intra-only block decoder
 *  Stripped of all inter, intrabc, segmentation, threading.
 * ================================================================ */
static int stb_decode_b(StbAv1TaskContext *const t,
                         const enum BlockLevel bl,
                         const enum BlockSize bs,
                         const enum BlockPartition bp,
                         const enum EdgeFlags intra_edge_flags)
{
    StbAv1TileState *const ts = t->ts;
    const StbAv1FrameContext *const f = t->f;
    StbAv1Block b_mem;
    StbAv1Block *const b = &b_mem;
    const unsigned char *const b_dim = stb_av1_block_dimensions[bs];
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int w4 = imin(bw4, f->bw - t->bx);
    const int h4 = imin(bh4, f->bh - t->by);
    const int cbw4 = (bw4 + ss_hor) >> ss_hor;
    const int cbh4 = (bh4 + ss_ver) >> ss_ver;
    const int have_left = t->bx > ts->tiling.col_start;
    const int have_top = t->by > ts->tiling.row_start;
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);
    const int cw4 = (w4 + ss_hor) >> ss_hor;
    const int ch4 = (h4 + ss_ver) >> ss_ver;
    int seg_pred;
    int idx, i, off, step;
    int prev_qidx;
    int have_delta_q;
    unsigned int prev_delta_lf;
    int delta_q, n_bits;
    int n_lfs, delta_lf;
    unsigned short *ymode_cdf;
    int angle;
    int cfl_allowed;
    unsigned short *uvmode_cdf;
    int sign, sign_u, sign_v, ctx_val, is_filter;
    int pal_ctx, use_y_pal, use_uv_pal;
    const StbAv1TxfmInfo *t_dim;
    int tctx, depth;
    enum IntraPredMode y_mode_nofilt;
    int t_lsz;

    b->bl = bl;
    b->bp = bp;
    b->bs = bs;

    /* segment_id - always 0 (no segmentation) */
    b->seg_id = 0;
    seg_pred = 0;

    /* skip_mode - always 0 for key frames */
    b->skip_mode = 0;

    /* skip */
    {
        const int sctx = t->a->skip[bx4] + t->l.skip[by4];
        b->skip = stb_av1_msac_decode_bool_adapt(&ts->msac, ts->cdf.m.skip[sctx]);
    }

    /* cdef index */
    if (!b->skip) {
        idx = f->seq_hdr->sb128 ? ((t->bx & 16) >> 4) + ((t->by & 16) >> 3) : 0;
        if (t->cur_sb_cdef_idx_ptr[idx] == -1) {
            int v = (int)stb_av1_msac_decode_bools(&ts->msac,
                              (unsigned)f->frame_hdr->cdef.n_bits);
            t->cur_sb_cdef_idx_ptr[idx] = v;
            if (bw4 > 16) t->cur_sb_cdef_idx_ptr[idx + 1] = v;
            if (bh4 > 16) t->cur_sb_cdef_idx_ptr[idx + 2] = v;
            if (bw4 == 32 && bh4 == 32) t->cur_sb_cdef_idx_ptr[idx + 3] = v;
        }
    }

    /* delta-q/lf */
    if (!((t->bx | t->by) & (31 >> !f->seq_hdr->sb128))) {
        prev_qidx = ts->last_qidx;
        have_delta_q = f->frame_hdr->delta.q.present &&
            (bs != (f->seq_hdr->sb128 ? BS_128x128 : BS_64x64) || !b->skip);

        prev_delta_lf = ts->last_delta_lf.u32;

        if (have_delta_q) {
            delta_q = (int)stb_av1_msac_decode_symbol(&ts->msac,
                                   ts->cdf.m.delta_q, 4);
            if (delta_q == 3) {
                n_bits = 1 + (int)stb_av1_msac_decode_bools(&ts->msac, 3);
                delta_q = (int)stb_av1_msac_decode_bools(&ts->msac, (unsigned)n_bits) +
                          1 + (1 << n_bits);
            }
            if (delta_q) {
                if (stb_av1_msac_decode_bool_equi(&ts->msac)) delta_q = -delta_q;
                delta_q *= 1 << f->frame_hdr->delta.q.res_log2;
            }
            ts->last_qidx = iclip(ts->last_qidx + delta_q, 1, 255);

            if (f->frame_hdr->delta.lf.present) {
                n_lfs = f->frame_hdr->delta.lf.multi ?
                    f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 ? 4 : 2 : 1;

                for (i = 0; i < n_lfs; i++) {
                    delta_lf = (int)stb_av1_msac_decode_symbol(&ts->msac,
                        ts->cdf.m.delta_lf[i + f->frame_hdr->delta.lf.multi], 4);
                    if (delta_lf == 3) {
                        n_bits = 1 + (int)stb_av1_msac_decode_bools(&ts->msac, 3);
                        delta_lf = (int)stb_av1_msac_decode_bools(&ts->msac, (unsigned)n_bits) +
                                   1 + (1 << n_bits);
                    }
                    if (delta_lf) {
                        if (stb_av1_msac_decode_bool_equi(&ts->msac))
                            delta_lf = -delta_lf;
                        delta_lf *= 1 << f->frame_hdr->delta.lf.res_log2;
                    }
                    ts->last_delta_lf.i8[i] =
                        iclip(ts->last_delta_lf.i8[i] + delta_lf, -63, 63);
                }
            }
        }
        if (ts->last_qidx == f->frame_hdr->quant.yac) {
            /* assign frame-wide q values to this sb */
            ts->dq = f->dq;
        } else if (ts->last_qidx != prev_qidx) {
            /* find sb-specific quant parameters */
            stb_init_quant_tables(f->seq_hdr, f->frame_hdr, ts->last_qidx, ts->dqmem);
            ts->dq = ts->dqmem;
        }
        if (!ts->last_delta_lf.u32) {
            /* assign frame-wide lf values to this sb */
            ts->lflvl = f->lf.lvl;
        } else if (ts->last_delta_lf.u32 != prev_delta_lf) {
            /* find sb-specific lf lvl parameters */
            ts->lflvl = ts->lflvlmem;
            stb_av1_calc_lf_values(ts->lflvlmem, f->frame_hdr, ts->last_delta_lf.i8);
        }
    }

    /* intra = 1 always for key frames */
    b->intra = 1;

    /* ============ INTRA-SPECIFIC STUFF ============ */

    /* y_mode */
    ymode_cdf = ts->cdf.kfym[stb_av1_intra_mode_context[t->a->mode[bx4]]]
                              [stb_av1_intra_mode_context[t->l.mode[by4]]];
    b->y_mode = (int)stb_av1_msac_decode_symbol(&ts->msac, ymode_cdf,
                                                N_INTRA_PRED_MODES);

    /* angle delta for y */
    if (b_dim[2] + b_dim[3] >= 2 && b->y_mode >= VERT_PRED &&
        b->y_mode <= VERT_LEFT_PRED)
    {
        unsigned short *const acdf = ts->cdf.m.angle_delta[b->y_mode - VERT_PRED];
        angle = (int)stb_av1_msac_decode_symbol(&ts->msac, acdf, 7);
        b->y_angle = angle - 3;
    } else {
        b->y_angle = 0;
    }

    /* uv_mode */
    if (has_chroma) {
        cfl_allowed = cbw4 == 1 && cbh4 == 1;
        /* CFL allowed only for lossless (4x4 blocks in intra). */
        /* We simplified lossless to just 4x4 blocks. */
        uvmode_cdf = ts->cdf.m.uv_mode[cfl_allowed][b->y_mode];
        b->uv_mode = (int)stb_av1_msac_decode_symbol(&ts->msac, uvmode_cdf,
                            N_UV_INTRA_PRED_MODES - !cfl_allowed);

        b->uv_angle = 0;
        if (b->uv_mode == CFL_PRED) {
            sign = (int)stb_av1_msac_decode_symbol(&ts->msac,
                               ts->cdf.m.cfl_sign, 8) + 1;
            sign_u = sign * 0x56 >> 8;
            sign_v = sign - sign_u * 3;
            if (sign_u) {
                ctx_val = (sign_u == 2) * 3 + sign_v;
                b->cfl_alpha[0] = (int)stb_av1_msac_decode_symbol(&ts->msac,
                                      ts->cdf.m.cfl_alpha[ctx_val], 16) + 1;
                if (sign_u == 1) b->cfl_alpha[0] = -b->cfl_alpha[0];
            } else {
                b->cfl_alpha[0] = 0;
            }
            if (sign_v) {
                ctx_val = (sign_v == 2) * 3 + sign_u;
                b->cfl_alpha[1] = (int)stb_av1_msac_decode_symbol(&ts->msac,
                                      ts->cdf.m.cfl_alpha[ctx_val], 16) + 1;
                if (sign_v == 1) b->cfl_alpha[1] = -b->cfl_alpha[1];
            } else {
                b->cfl_alpha[1] = 0;
            }
        } else if (b_dim[2] + b_dim[3] >= 2 && b->uv_mode >= VERT_PRED &&
                   b->uv_mode <= VERT_LEFT_PRED)
        {
            unsigned short *const acdf = ts->cdf.m.angle_delta[b->uv_mode - VERT_PRED];
            angle = (int)stb_av1_msac_decode_symbol(&ts->msac, acdf, 7);
            b->uv_angle = angle - 3;
        }
    }

    /* palette */
    b->pal_sz[0] = b->pal_sz[1] = 0;
    if (f->frame_hdr->allow_screen_content_tools &&
        imax(bw4, bh4) <= 16 && bw4 + bh4 >= 4)
    {
        const int sz_ctx = b_dim[2] + b_dim[3] - 2;
        if (b->y_mode == DC_PRED) {
            pal_ctx = (t->a->pal_sz[bx4] > 0) + (t->l.pal_sz[by4] > 0);
            use_y_pal = (int)stb_av1_msac_decode_bool_adapt(&ts->msac,
                            ts->cdf.m.pal_y[sz_ctx][pal_ctx]);
            if (use_y_pal)
                f->bd_fn.read_pal_plane(t, b, 0, sz_ctx, bx4, by4);
        }

        if (has_chroma && b->uv_mode == DC_PRED) {
            pal_ctx = b->pal_sz[0] > 0;
            use_uv_pal = (int)stb_av1_msac_decode_bool_adapt(&ts->msac,
                             ts->cdf.m.pal_uv[pal_ctx]);
            if (use_uv_pal)
                f->bd_fn.read_pal_uv(t, b, sz_ctx, bx4, by4);
        }
    }

    /* filter intra */
    if (b->y_mode == DC_PRED && !b->pal_sz[0] &&
        imax(b_dim[2], b_dim[3]) <= 3 && f->seq_hdr->filter_intra)
    {
        is_filter = (int)stb_av1_msac_decode_bool_adapt(&ts->msac,
                        ts->cdf.m.use_filter_intra[bs]);
        if (is_filter) {
            b->y_mode = FILTER_PRED;
            b->y_angle = (int)stb_av1_msac_decode_symbol(&ts->msac,
                            ts->cdf.m.filter_intra, 5);
        }
    }

    /* palette indices */
    if (b->pal_sz[0]) {
        stb_read_pal_indices(t, t->scratch.pal_idx_y, b->pal_sz[0], 0,
                             w4, h4, bw4, bh4);
    }
    if (has_chroma && b->pal_sz[1]) {
        stb_read_pal_indices(t, t->scratch.pal_idx_uv, b->pal_sz[1], 1,
                             cw4, ch4, cbw4, cbh4);
    }

    /* transform size */
    if (b->skip) {
        b->tx = stb_av1_max_txfm_size_for_bs[bs][0];
        b->uvtx = stb_av1_max_txfm_size_for_bs[bs][f->cur.p.layout];
        t_dim = &stb_av1_txfm_dimensions[b->tx];
    } else {
        b->tx = stb_av1_max_txfm_size_for_bs[bs][0];
        b->uvtx = stb_av1_max_txfm_size_for_bs[bs][f->cur.p.layout];
        t_dim = &stb_av1_txfm_dimensions[b->tx];
        if (f->frame_hdr->txfm_mode == DAV1D_TX_SWITCHABLE && t_dim->max > TX_4X4) {
            tctx = stb_av1_get_tx_ctx(t->a, &t->l, t_dim, by4, bx4);
            {
                unsigned short *const tx_cdf = ts->cdf.m.txsz[t_dim->max - 1][tctx];
                depth = (int)stb_av1_msac_decode_symbol(&ts->msac, tx_cdf,
                                  imin(t_dim->max, 2) + 1);
                while (depth--) {
                    b->tx = t_dim->sub;
                    t_dim = &stb_av1_txfm_dimensions[b->tx];
                }
            }
        }
    }

    /* reconstruction */
    f->bd_fn.recon_b_intra(t, bs, intra_edge_flags, b);

    /* loop filter mask */
    if (f->frame_hdr->loopfilter.level_y[0] ||
        f->frame_hdr->loopfilter.level_y[1])
    {
        stb_av1_create_lf_mask_intra(t->lf_mask, f->lf.level, f->b4_stride,
                                     (const unsigned char (*)[8][2])
                                     &ts->lflvl[b->seg_id][0][0][0],
                                     t->bx, t->by, f->w4, f->h4, bs,
                                     b->tx, b->uvtx, f->cur.p.layout,
                                     &t->a->tx_lpf_y[bx4], &t->l.tx_lpf_y[by4],
                                     has_chroma ? &t->a->tx_lpf_uv[cbx4] : NULL,
                                     has_chroma ? &t->l.tx_lpf_uv[cby4] : NULL);
    }

    /* update contexts */
    y_mode_nofilt = b->y_mode == FILTER_PRED ? DC_PRED : b->y_mode;

    /* horizontal context updates (edge = t->a) */
    for (step = 0; step < (1 << b_dim[2]); step++) {
        t_lsz = b_dim[2]; /* lw */
        t->a->tx_intra[bx4 + step] = t_lsz;
        t->a->tx[bx4 + step] = t_lsz;
        t->a->mode[bx4 + step] = y_mode_nofilt;
        t->a->pal_sz[bx4 + step] = b->pal_sz[0];
        t->a->seg_pred[bx4 + step] = seg_pred;
        t->a->skip_mode[bx4 + step] = 0;
        t->a->intra[bx4 + step] = 1;
        t->a->skip[bx4 + step] = b->skip;
        t->pal_sz_uv[0][bx4 + step] = (has_chroma ? b->pal_sz[1] : 0);
    }

    /* vertical context updates (edge = &t->l) */
    for (step = 0; step < (1 << b_dim[3]); step++) {
        t_lsz = b_dim[3]; /* lh */
        t->l.tx_intra[by4 + step] = t_lsz;
        t->l.tx[by4 + step] = t_lsz;
        t->l.mode[by4 + step] = y_mode_nofilt;
        t->l.pal_sz[by4 + step] = b->pal_sz[0];
        t->l.seg_pred[by4 + step] = seg_pred;
        t->l.skip_mode[by4 + step] = 0;
        t->l.intra[by4 + step] = 1;
        t->l.skip[by4 + step] = b->skip;
        t->pal_sz_uv[1][by4 + step] = (has_chroma ? b->pal_sz[1] : 0);
    }

    /* palette copy */
    if (b->pal_sz[0])
        f->bd_fn.copy_pal_block_y(t, bx4, by4, bw4, bh4);
    if (has_chroma) {
        unsigned char uv_mode = b->uv_mode;
        stb_av1_memset_pow2[ulog2(cbw4)](&t->a->uvmode[cbx4], uv_mode);
        stb_av1_memset_pow2[ulog2(cbh4)](&t->l.uvmode[cby4], uv_mode);
        if (b->pal_sz[1])
            f->bd_fn.copy_pal_block_uv(t, bx4, by4, bw4, bh4);
    }

    /* update noskip_mask */
    if (!b->skip) {
        unsigned short (*noskip_mask)[2] = &t->lf_mask->noskip_mask[by4 >> 1];
        const unsigned mask_val = (~0U >> (32 - bw4)) << (bx4 & 15);
        const int bx_idx = (bx4 & 16) >> 4;
        int y;
        for (y = 0; y < bh4; y += 2, noskip_mask++) {
            (*noskip_mask)[bx_idx] |= mask_val;
            if (bw4 == 32)
                (*noskip_mask)[1] |= mask_val;
        }
    }

    return 0;
}
