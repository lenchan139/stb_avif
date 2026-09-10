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
    unsigned int primary_ref_frame;
    unsigned int refresh_frame_flags;
    unsigned int allow_intrabc;
    unsigned int refresh_context;
    unsigned int buffer_removal_time_present;

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

    if (!h->error_resilient_mode && (h->frame_type == STB_AV1_FRAME_INTER ||
                                     h->frame_type == STB_AV1_FRAME_SWITCH))
        h->primary_ref_frame = (int)stb_av1_get_bits(gb, 3);
    else
        h->primary_ref_frame = 7; /* PRIMARY_REF_NONE */

    if (s->decoder_model_info_present) {
        h->buffer_removal_time_present = stb_av1_get_bit(gb);
        if (h->buffer_removal_time_present) {
            for (i = 0; i < s->num_operating_points; i++) {
                if (s->operating_points[i].decoder_model_param_present) {
                    unsigned int idc = s->operating_points[i].idc;
                    int in_temporal = (idc >> 0) & 1;
                    int in_spatial  = (idc >> 0) & 1;
                    if (!idc || (in_temporal && in_spatial))
                        (void)stb_av1_get_bits(gb, (int)s->buffer_removal_delay_length);
                }
            }
        }
    }

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
        h->segmentation.preskip = 0;
        if (h->primary_ref_frame == 7) { /* PRIMARY_REF_NONE */
            h->segmentation.update_map = 1;
            h->segmentation.update_data = 1;
        } else {
            h->segmentation.update_map = stb_av1_get_bit(gb);
            if (h->segmentation.update_map)
                h->segmentation.temporal = stb_av1_get_bit(gb);
            h->segmentation.update_data = stb_av1_get_bit(gb);
        }
        if (h->segmentation.update_data) {
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
        } /* end if (update_data) */
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
