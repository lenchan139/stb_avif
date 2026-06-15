/*
 * C89 port of dav1d's src/obu.c - OBU parsing for stb_avif
 *
 * This provides OBU (Open Bitstream Unit) parsing for AV1.
 * Uses GetBits (raw bit reading) for OBU headers and sequence/frame headers.
 * The actual compressed bitstream data uses MSAC (Multi-Symbol Arithmetic Coder).
 *
 * C89 compatibility:
 *   - No // comments, no inline, no restrict
 *   - No C99 for-loop declarations, no designated initializers
 *   - unsigned char/short/int instead of uint8/16/32_t
 *   - dav1d_* -> stb_av1_*
 */

#include "getbits_c89.h"
#include "levels_c89.h"
#include "levels_compat_c89.h"

#ifndef STB_AVIF_USE_C89_DAV1D
#define STB_AV1_OBU_C89_C 1
#endif

/* Map stb_avif.h types to our naming */
#ifndef StbAv1SequenceHeader
#define StbAv1SequenceHeader struct stb_av1_sequence_header
#endif

#ifndef StbAv1FrameHeader
#define StbAv1FrameHeader struct stb_av1_frame_header
#endif

/* Forward declaration - actual struct defined in stb_avif.h */
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
    int bit_depth;
    int monochrome;
    int subsampling_x;
    int subsampling_y;
    int hbd;
    int sep_colour_desc_flag;
    int color_description_present_flag;
    int num_planes;
    int use_128x128_superblock;
    int filter_intra;
    int intra_edge_filter;
    int inter_intra;
    int masked_comp;
    int warped_motion;
    int dual_filter;
    int order_hint;
    int order_hint_n_bits;
    int screen_content_tools;
    int force_integer_mv;
    int enable_ref_frame_mvs;
    int frame_id_numbers_present;
    int delta_frame_id_n_bits;
    int frame_id_n_bits;
    int use_filter_intra;
    int palette_mode;
    int switchable_motion_mode;
    int use_jnt_comp;
    int use_mask_comp;
    int use_wedge_compound;
    int use_legacy_filter_switch;
    int enable_po_grad;
    int chroma_format;
    int superres;
    int separate_uv_delta_q;
    int seq_level_idx;
    int seq_tier;
    int initial_decoding_delay;
    int sb_size;
    int initial_display_delay;
    int op_pts_idc[7];
    int op_pts_level[7];
    int timing_info_num_units_in_tick;
    int timing_info_time_scale;
    int equal_picture_interval;
    int pri;
    int trc;
    int mtrx;
    int ss_x;
    int ss_y;
};

struct stb_av1_frame_header {
    int show_existing_frame;
    int frame_type;
    int show_frame;
    int showable_frame;
    int error_resilient_mode;
    int disable_cdf_update;
    int allow_screen_content_tools;
    int allow_screen_content_tools_flag;
    int force_integer_mv;
    int force_integer_mv_flag;
    int current_frame_id;
    int frame_size_override;
    int frame_width;
    int frame_height;
    int render_width;
    int render_height;
    int superres_enabled;
    int superres_scale_denominator;
    int superres_width;
    int have_render_size;
    int frame_offset;
    int primary_ref_frame;
    int refresh_frame_flags;
    int ref_frame_idx[7];
    int ref_frame_sign_bias[7];
    int hp;
    int allow_high_precision_mv;
    int use_ref_frame_mvs;
    int allow_ref_frame_mvs;
    int allow_intrabc;
    int frame_refs_short_signaling;
    int last_frame_idx;
    int gold_frame_idx;
    int allow_warped_motion;
    int allow_filter_intra;
    int allow_intra_edge_filter;
    int allow_interintra_comp;
    int allow_masked_comp;
    int allow_wedge_compound;
    int cdef_enabled;
    int cdef_n_bits;
    int cdef_y_pri[8];
    int cdef_y_sec[8];
    int cdef_uv_pri[8];
    int cdef_uv_sec[8];
    int loopfilter_enabled;
    int loopfilter_level_y[2];
    int loopfilter_level_u;
    int loopfilter_level_v;
    int loopfilter_sharpness;
    int loopfilter_delta_enabled;
    int loopfilter_delta_update;
    int loopfilter_delta_lf[4];
    int segmentation_enabled;
    int segmentation_update_data;
    int segmentation_temporal_update;
    int segmentation_feat_quant[8][4];
    int segmentation_feat_skip[8];
    int segmentation_abs_delta;
    int base_q_idx;
    int delta_q_y_dc;
    int delta_q_u_ac;
    int delta_q_u_dc;
    int delta_q_v_ac;
    int delta_q_v_dc;
    int using_qmatrix;
    int qm_y;
    int qm_u;
    int delta_lf_present;
    int delta_lf_multi;
    int delta_lf_res_log2;
    int delta_lf_from_base;
    int delta_lf_y[4];
    int delta_lf_u;
    int delta_lf_v;
    int skip_mode_present;
    int skip;
    int is_motion_mode_switchable;
    int reduced_still_picture_header;
    int interpolation_filter;
    int tx_mode;
    int tx_mode_switchable;
    int reference_mode;
    int reference_select;
    int reduced_txtp_set;
    int allow_transform_64;
    int cdef_strengths[8];
    int lr_type[3];
    int wm_type[7];
    int existing_frame_idx;
    int frame_presentation_delay;
    int buffer_removal_time_present;
    int buffer_removal_time[8];
    int order_hint;
    int frame_id;
    int delta_q_present;
    int delta_q_res_log2;
    int quantization_present;
};

/* OBU types */
enum {
    STB_AV1_OBU_RESERVED_0 = 0,
    STB_AV1_OBU_SEQUENCE_HEADER = 1,
    STB_AV1_OBU_TEMPORAL_DELIMITER = 2,
    STB_AV1_OBU_FRAME_HEADER = 3,
    STB_AV1_OBU_TILE_GROUP = 4,
    STB_AV1_OBU_METADATA = 5,
    STB_AV1_OBU_FRAME = 6,
    STB_AV1_OBU_REDUNDANT_FRAME_HEADER = 7,
    STB_AV1_OBU_TILE_LIST = 8,
    STB_AV1_OBU_END_SEQUENCE = 9,
    STB_AV1_OBU_PADDING = 15,
};

/* Color primaries (from AV1 spec) */
#define STB_AV1_COLOR_PRI_UNKNOWN     0
#define STB_AV1_COLOR_PRI_BT709       1
#define STB_AV1_COLOR_PRI_BT601_625   5
#define STB_AV1_COLOR_PRI_BT601_525   6
#define STB_AV1_COLOR_PRI_BT2020      9
#define STB_AV1_COLOR_PRI_DCI_P3     11

/* Transfer characteristics */
#define STB_AV1_TRC_UNKNOWN           0
#define STB_AV1_TRC_BT709            1
#define STB_AV1_TRC_SRGB             13

/* Matrix coefficients */
#define STB_AV1_MC_IDENTITY           0
#define STB_AV1_MC_BT709             1
#define STB_AV1_MC_BT601             6
#define STB_AV1_MC_BT2020_NCL       9
#define STB_AV1_MC_BT2020_CL        10

/* Pixel layout */
#define STB_AV1_PIXEL_LAYOUT_I400    0
#define STB_AV1_PIXEL_LAYOUT_I420    1
#define STB_AV1_PIXEL_LAYOUT_I422    2
#define STB_AV1_PIXEL_LAYOUT_I444    3

/* TX modes */
#define STB_AV1_TX_ONLY_SWITCHABLE   0
#define STB_AV1_TX_SWITCHABLE        2
#define STB_AV1_TX_SELECT            3

/* Helper macros */
#ifndef imin
#define imin(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef imax
#define imax(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef iclip
#define iclip(val, lo, hi) ((val) < (lo) ? (lo) : (val) > (hi) ? (hi) : (val))
#endif

/*
 * Parse AV1 sequence header from raw bytes using GetBits.
 * Returns 0 on success, -1 on error.
 */
static int stb_av1_parse_seq_hdr_gb(struct StbAv1GetBits *gb,
                                  struct stb_av1_sequence_header *hdr)
{
    int i;
    int op_idx;

    /* Initialize all fields to zero */
    hdr->seq_profile = 0;
    hdr->still_picture = 0;
    hdr->reduced_still_picture_header = 0;
    hdr->timing_info_present = 0;
    hdr->decoder_model_info_present = 0;
    hdr->display_model_info_present = 0;
    hdr->operating_points_cnt = 1;
    hdr->initial_display_delay_present = 0;
    hdr->bit_depth = 8;
    hdr->monochrome = 0;
    hdr->chrom_sample_position = 0;
    hdr->color_description_present = 0;
    hdr->color_primaries = STB_AV1_COLOR_PRI_UNKNOWN;
    hdr->transfer_characteristics = STB_AV1_TRC_UNKNOWN;
    hdr->matrix_coefficients = STB_AV1_MC_BT709;
    hdr->color_range = 0;
    hdr->subsampling_x = 1;
    hdr->subsampling_y = 1;
    hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I420;
    hdr->frame_width_bits = 4;
    hdr->frame_height_bits = 4;
    hdr->max_frame_width = 16;
    hdr->max_frame_height = 16;
    hdr->superres = 0;
    hdr->sb_size = 0;
    hdr->filter_intra = 0;
    hdr->enable_intra_edge_filter = 1;
    hdr->enable_interintra_comp = 0;
    hdr->enable_masked_comp = 0;
    hdr->enable_dual_filter = 0;
    hdr->enable_jnt_comp = 0;
    hdr->enable_superres = 0;
    hdr->enable_cdef = 1;
    hdr->enable_restoration = 0;
    hdr->enable_order_hint = 0;
    hdr->enable_dist_wtd_comp = 0;
    hdr->enable_ref_frame_mvs = 0;
    hdr->force_integer_mv = 0;
    hdr->screen_content_tools = 0;
    hdr->allow_intrabc = 0;
    hdr->hbd = 0;
    hdr->ss_x = 1;
    hdr->ss_y = 1;
    hdr->num_planes = 1;
    hdr->film_grain_params_present = 0;
    hdr->seq_level_idx = 0;
    hdr->seq_tier = 0;
    hdr->initial_decoding_delay = 0;
    hdr->initial_display_delay = 0;
    hdr->enable_po_grad = 0;
    hdr->use_filter_intra = 0;
    hdr->palette_mode = 0;
    hdr->switchable_motion_mode = 1;
    hdr->use_jnt_comp = 0;
    hdr->use_mask_comp = 0;
    hdr->use_wedge_compound = 0;
    hdr->use_legacy_filter_switch = 0;
    hdr->equal_picture_interval = 0;
    hdr->sep_colour_desc_flag = 0;
    hdr->color_description_present_flag = 0;
    hdr->use_128x128_superblock = 0;

    /* Parse AV1 sequence header (AV1 spec section 5.5) */
    hdr->seq_profile = (int)stb_av1_get_bits(gb, 3);
    if (hdr->seq_profile > 2) return -1;

    hdr->still_picture = (int)stb_av1_get_bit(gb);
    hdr->reduced_still_picture_header = (int)stb_av1_get_bit(gb);

    if (hdr->reduced_still_picture_header && !hdr->still_picture)
        return -1;

    if (hdr->reduced_still_picture_header) {
        /* Reduced still picture header path */
        hdr->timing_info_present = 0;
        hdr->decoder_model_info_present = 0;
        hdr->display_model_info_present = 0;
        hdr->operating_points_cnt = 1;
        hdr->frame_width_bits = 4;
        hdr->frame_height_bits = 4;
        hdr->max_frame_width = (int)stb_av1_get_bits(gb, 4) + 1;
        hdr->max_frame_height = (int)stb_av1_get_bits(gb, 4) + 1;
        hdr->enable_order_hint = 0;
        hdr->enable_dist_wtd_comp = 0;
        hdr->enable_masked_comp = 0;
        hdr->enable_intra_edge_filter = 1;
        hdr->enable_interintra_comp = 0;
        hdr->enable_dual_filter = 0;
        hdr->enable_jnt_comp = 0;
        hdr->enable_superres = 0;
        hdr->enable_cdef = 1;
        hdr->enable_restoration = 0;
        hdr->film_grain_params_present = 0;
    } else {
        /* Full sequence header */
        hdr->timing_info_present = (int)stb_av1_get_bit(gb);
        if (hdr->timing_info_present) {
            stb_av1_get_bits(gb, 32);  /* num_units_in_tick */
            stb_av1_get_bits(gb, 32);  /* time_scale */
            hdr->equal_picture_interval = (int)stb_av1_get_bit(gb);
            hdr->decoder_model_info_present = (int)stb_av1_get_bit(gb);
            if (hdr->decoder_model_info_present) {
                stb_av1_get_bits(gb, 5);  /* buffer_delay_length_minus_1 */
                stb_av1_get_bits(gb, 32);  /* num_units_in_decoding_tick */
                stb_av1_get_bits(gb, 5);  /* buffer_removal_delay_length_minus_1 */
                stb_av1_get_bits(gb, 5);  /* frame_presentation_delay_length_minus_1 */
            }
        }

        /* Operating points */
        hdr->operating_points_cnt = (int)stb_av1_get_bits(gb, 5) + 1;
        if (hdr->operating_points_cnt > 7) hdr->operating_points_cnt = 7;
        for (op_idx = 0; op_idx < hdr->operating_points_cnt; op_idx++) {
            stb_av1_get_bits(gb, 12);  /* operating_point_idc */
            hdr->op_pts_level[op_idx] = (int)stb_av1_get_bits(gb, 3);
            if (hdr->op_pts_level[op_idx] > 3)
                stb_av1_get_bit(gb);  /* seq_tier */
            if (hdr->decoder_model_info_present) {
                int dmp = (int)stb_av1_get_bit(gb);
                if (dmp) {
                    stb_av1_get_bits(gb, 5);  /* decoder_buffer_delay_length_minus_1 */
                    stb_av1_get_bits(gb, 5);  /* encoder_buffer_delay_length_minus_1 */
                    stb_av1_get_bit(gb);  /* low_delay_mode_flag */
                }
            }
            if (hdr->display_model_info_present)
                stb_av1_get_bit(gb);  /* initial_display_delay_present */
        }

        /* Frame size */
        hdr->frame_width_bits = (int)stb_av1_get_bits(gb, 4) + 1;
        hdr->frame_height_bits = (int)stb_av1_get_bits(gb, 4) + 1;
        hdr->max_frame_width = (int)stb_av1_get_bits(gb, hdr->frame_width_bits) + 1;
        hdr->max_frame_height = (int)stb_av1_get_bits(gb, hdr->frame_height_bits) + 1;

        if (hdr->max_frame_width <= 0 || hdr->max_frame_height <= 0)
            return -1;

        /* frame_id_numbers_present */
        hdr->frame_id_numbers_present = (int)stb_av1_get_bit(gb);
        if (hdr->frame_id_numbers_present) {
            hdr->delta_frame_id_n_bits = (int)stb_av1_get_bits(gb, 4) + 2;
            hdr->frame_id_n_bits = (int)stb_av1_get_bits(gb, 3) + hdr->delta_frame_id_n_bits + 1;
        }

        /* Use 128x128 superblock */
        hdr->use_128x128_superblock = (int)stb_av1_get_bit(gb);
        hdr->filter_intra = (int)stb_av1_get_bit(gb);
        hdr->intra_edge_filter = (int)stb_av1_get_bit(gb);

        /* Inter-specific */
        hdr->inter_intra = (int)stb_av1_get_bit(gb);
        hdr->masked_comp = (int)stb_av1_get_bit(gb);
        hdr->warped_motion = (int)stb_av1_get_bit(gb);
        hdr->dual_filter = (int)stb_av1_get_bit(gb);
        hdr->order_hint = (int)stb_av1_get_bit(gb);

        if (hdr->order_hint) {
            hdr->enable_jnt_comp = (int)stb_av1_get_bit(gb);
            hdr->enable_ref_frame_mvs = (int)stb_av1_get_bit(gb);
        }

        /* Screen content tools */
        {
            int sct_bit = (int)stb_av1_get_bit(gb);
            if (sct_bit)
                hdr->screen_content_tools = 2;  /* ADAPTIVE */
            else
                hdr->screen_content_tools = 0;  /* OFF */
        }

        /* Force integer MV */
        if (hdr->screen_content_tools) {
            hdr->force_integer_mv = (int)stb_av1_get_bit(gb);
        } else {
            hdr->force_integer_mv = 0;
        }

        if (hdr->order_hint) {
            hdr->order_hint_n_bits = (int)stb_av1_get_bits(gb, 3) + 1;
        }

        /* Enable flags from features */
        hdr->enable_order_hint = hdr->order_hint;
        hdr->enable_dist_wtd_comp = 0;
        hdr->enable_masked_comp = hdr->masked_comp;
        hdr->enable_dual_filter = hdr->dual_filter;
        hdr->enable_interintra_comp = hdr->inter_intra;
    }

    /* Superres */
    hdr->superres = (int)stb_av1_get_bit(gb);
    if (hdr->superres) {
        hdr->enable_superres = 1;
    } else {
        hdr->enable_superres = 0;
    }

    /* CDEF and restoration */
    hdr->enable_cdef = (int)stb_av1_get_bit(gb);
    hdr->enable_restoration = (int)stb_av1_get_bit(gb);

    /* Color config */
    hdr->hbd = (int)stb_av1_get_bit(gb);
    if (hdr->seq_profile == 2 && hdr->hbd)
        hdr->hbd += (int)stb_av1_get_bit(gb);  /* extra bit for profile 2 */
    hdr->bit_depth = hdr->hbd ? 10 : 8;

    if (hdr->seq_profile != 1) {
        hdr->monochrome = (int)stb_av1_get_bit(gb);
    } else {
        hdr->monochrome = 0;
    }

    /* Color description */
    hdr->color_description_present = (int)stb_av1_get_bit(gb);
    if (hdr->color_description_present) {
        hdr->color_primaries = (int)stb_av1_get_bits(gb, 8);
        hdr->transfer_characteristics = (int)stb_av1_get_bits(gb, 8);
        hdr->matrix_coefficients = (int)stb_av1_get_bits(gb, 8);
    } else {
        hdr->color_primaries = STB_AV1_COLOR_PRI_UNKNOWN;
        hdr->transfer_characteristics = STB_AV1_TRC_UNKNOWN;
        hdr->matrix_coefficients = STB_AV1_MC_BT709;
    }

    /* Color range */
    if (hdr->monochrome) {
        hdr->color_range = (int)stb_av1_get_bit(gb);
        hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I400;
        hdr->subsampling_x = 0;
        hdr->subsampling_y = 0;
        hdr->num_planes = 1;
    } else if (hdr->color_primaries == STB_AV1_COLOR_PRI_BT709 &&
               hdr->transfer_characteristics == STB_AV1_TRC_SRGB &&
               hdr->matrix_coefficients == STB_AV1_MC_IDENTITY)
    {
        hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I444;
        hdr->color_range = 1;
    } else {
        hdr->color_range = (int)stb_av1_get_bit(gb);
        switch (hdr->seq_profile) {
        case 0:
            hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I420;
            hdr->subsampling_x = 1;
            hdr->subsampling_y = 1;
            break;
        case 1:
            hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I444;
            hdr->subsampling_x = 0;
            hdr->subsampling_y = 0;
            break;
        case 2:
            if (hdr->hbd) {
                hdr->subsampling_x = (int)stb_av1_get_bit(gb);
                if (hdr->subsampling_x)
                    hdr->subsampling_y = (int)stb_av1_get_bit(gb);
                else
                    hdr->subsampling_y = 0;
            } else {
                hdr->subsampling_x = 1;
                hdr->subsampling_y = 1;
            }
            if (!hdr->subsampling_x && !hdr->subsampling_y)
                hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I444;
            else if (hdr->subsampling_x && hdr->subsampling_y)
                hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I420;
            else
                hdr->chroma_format = STB_AV1_PIXEL_LAYOUT_I422;
            break;
        default:
            return -1;
        }
        /* Chroma sample position */
        if (hdr->chroma_format != STB_AV1_PIXEL_LAYOUT_I444) {
            hdr->chroma_sample_position = (int)stb_av1_get_bits(gb, 2);
        }
        hdr->separate_uv_delta_q = (int)stb_av1_get_bit(gb);
    }

    /* Number of planes */
    hdr->num_planes = hdr->monochrome ? 1 : 3;
    hdr->ss_x = hdr->subsampling_x;
    hdr->ss_y = hdr->subsampling_y;
    hdr->matrix_coefficients = (hdr->matrix_coefficients == STB_AV1_MC_IDENTITY &&
                              hdr->chroma_format != STB_AV1_PIXEL_LAYOUT_I444) ?
                             STB_AV1_MC_BT709 : hdr->matrix_coefficients;

    /* Film grain */
    hdr->film_grain_params_present = (int)stb_av1_get_bit(gb);

    /* Filter intra */
    hdr->use_filter_intra = (int)stb_av1_get_bit(gb);

    return stb_av1_check_trailing_bits(gb, 0);
}

/*
 * stb_av1_parse_sequence_header - main entry point for sequence header parsing.
 * Returns 0 on success, -1 on error.
 */
int stb_av1_parse_sequence_header(void *seq_hdr_out,
                                  const unsigned char *data,
                                  unsigned int size)
{
    struct StbAv1GetBits gb;
    struct stb_av1_sequence_header *hdr;
    int obu_has_ext;
    int obu_has_size;

    hdr = (struct stb_av1_sequence_header *)seq_hdr_out;
    if (!hdr || !data || size == 0) return -1;

    stb_av1_init_get_bits(&gb, data, size);

    /* Check for OBU header */
    if (gb.ptr >= gb.ptr_end) return -1;

    /* Check if this looks like an OBU header.
       OBU header: forbidden(1) | type(4) | extension(1) | has_size(1) | reserved(1) */
    {
        unsigned int first_byte = (unsigned int)gb.ptr[0];
        if ((first_byte & 0xF0) == 0x80) {
            /* It's an OBU - skip OBU header */
            stb_av1_get_bits(&gb, 4);  /* forbidden + type */
            obu_has_ext = (int)stb_av1_get_bit(&gb);
            obu_has_size = (int)stb_av1_get_bit(&gb);
            (void)stb_av1_get_bit(&gb);  /* reserved bit */
            if (obu_has_ext) stb_av1_get_bits(&gb, 8);  /* temporal_id, spatial_id */
            if (obu_has_size) stb_av1_get_uleb128(&gb);  /* OBU size */
        }
        /* Otherwise, it's a raw sequence header - proceed directly */
    }

    return stb_av1_parse_seq_hdr_gb(&gb, hdr);
}

/*
 * stb_av1_parse_frame_hdr_gb - Parse AV1 frame header from GetBits.
 * Returns 0 on success, -1 on error.
 */
static int stb_av1_parse_frame_hdr_gb(struct StbAv1GetBits *gb,
                                       struct stb_av1_frame_header *fh,
                                       const struct stb_av1_sequence_header *sh)
{
    int i;

    /* Initialize all fields to zero/defaults */
    fh->show_existing_frame = 0;
    fh->frame_type = 0;
    fh->show_frame = 1;
    fh->showable_frame = 0;
    fh->error_resilient_mode = 1;
    fh->disable_cdf_update = 0;
    fh->allow_screen_content_tools = sh->allow_screen_content_tools;
    fh->allow_screen_content_tools_flag = 0;
    fh->force_integer_mv = sh->force_integer_mv;
    fh->force_integer_mv_flag = 0;
    fh->refresh_frame_flags = 0xFF;
    fh->base_q_idx = 100;
    fh->skip = 0;
    fh->skip_mode_present = 0;
    fh->allow_intrabc = 0;
    fh->allow_warped_motion = 0;
    fh->allow_filter_intra = 0;
    fh->allow_intra_edge_filter = 1;
    fh->allow_interintra_comp = 0;
    fh->allow_masked_comp = 0;
    fh->allow_wedge_compound = 0;
    fh->cdef_enabled = 0;
    fh->cdef_n_bits = 0;
    fh->loopfilter_enabled = 0;
    fh->tx_mode = STB_AV1_TX_SWITCHABLE;
    fh->tx_mode_switchable = 0;
    fh->reference_mode = 0;
    fh->reference_select = 0;
    fh->allow_ref_frame_mvs = 0;
    fh->use_ref_frame_mvs = 0;
    fh->is_motion_mode_switchable = 0;
    fh->frame_refs_short_signaling = 0;
    fh->last_frame_idx = 0;
    fh->gold_frame_idx = 0;
    fh->hp = 1;
    fh->allow_high_precision_mv = 1;
    fh->interpolation_filter = 0;
    fh->segmentation_enabled = 0;
    fh->quantization_present = 0;
    fh->delta_q_present = 0;
    fh->delta_q_y_dc = 0;
    fh->delta_q_u_ac = 0;
    fh->delta_q_u_dc = 0;
    fh->delta_q_v_ac = 0;
    fh->delta_q_v_dc = 0;
    fh->using_qmatrix = 0;
    fh->qm_y = 0;
    fh->qm_u = 0;
    fh->delta_lf_present = 0;
    fh->delta_lf_multi = 0;
    fh->delta_lf_res_log2 = 0;
    fh->delta_lf_from_base = 0;
    fh->allow_transform_64 = 0;
    fh->reduced_still_picture_header = sh->reduced_still_picture_header;
    fh->frame_size_override = 0;
    fh->frame_width = sh->max_frame_width;
    fh->frame_height = sh->max_frame_height;
    fh->render_width = sh->max_frame_width;
    fh->render_height = sh->max_frame_height;
    fh->superres_enabled = 0;
    fh->superres_scale_denominator = 8;
    fh->superres_width = sh->max_frame_width;
    fh->have_render_size = 0;
    fh->frame_offset = 0;
    fh->primary_ref_frame = 0;
    fh->current_frame_id = 0;
    fh->frame_id = 0;
    fh->order_hint = 0;
    fh->delta_q_present = 0;
    fh->delta_q_res_log2 = 0;
    fh->quantization_present = 0;
    fh->existing_frame_idx = 0;
    fh->frame_presentation_delay = 0;
    fh->buffer_removal_time_present = 0;
    for (i = 0; i < 8; i++) {
        fh->ref_frame_idx[i] = 0;
        fh->ref_frame_sign_bias[i] = 0;
        fh->segmentation_feat_quant[i][0] = 0;
        fh->segmentation_feat_quant[i][1] = 0;
        fh->segmentation_feat_quant[i][2] = 0;
        fh->segmentation_feat_quant[i][3] = 0;
        fh->segmentation_feat_skip[i] = 0;
        fh->cdef_y_pri[i] = 0;
        fh->cdef_y_sec[i] = 0;
        fh->cdef_uv_pri[i] = 0;
        fh->cdef_uv_sec[i] = 0;
        fh->cdef_strengths[i] = 0;
        fh->buffer_removal_time[i] = 0;
        fh->delta_lf_y[i] = 0;
        fh->wm_type[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        fh->loopfilter_level_y[i] = 0;
        fh->loopfilter_delta_lf[i] = 0;
    }
    fh->loopfilter_level_u = 0;
    fh->loopfilter_level_v = 0;
    fh->loopfilter_sharpness = 0;
    fh->loopfilter_delta_enabled = 0;
    fh->loopfilter_delta_update = 0;
    fh->segmentation_update_data = 0;
    fh->segmentation_temporal_update = 0;
    fh->segmentation_abs_delta = 0;
    fh->lr_type[0] = 0;
    fh->lr_type[1] = 0;
    fh->lr_type[2] = 0;

    if (sh->reduced_still_picture_header) {
        fh->frame_type = 0; /* KEY_FRAME */
        fh->show_frame = 1;
        fh->error_resilient_mode = 1;
    } else {
        fh->frame_type = (int)stb_av1_get_bits(gb, 2);
        fh->show_frame = (int)stb_av1_get_bit(gb);
        if (fh->show_frame)
            fh->showable_frame = (fh->frame_type != 0);
        else
            fh->showable_frame = (int)stb_av1_get_bit(gb);
        fh->error_resilient_mode = (int)stb_av1_get_bit(gb);
    }

    /* Disable CDF update */
    if (sh->reduced_still_picture_header)
        fh->disable_cdf_update = 0;
    else
        fh->disable_cdf_update = (int)stb_av1_get_bit(gb);

    /* Screen content tools */
    if (sh->allow_screen_content_tools) {
        fh->allow_screen_content_tools_flag = (int)stb_av1_get_bit(gb);
        if (fh->allow_screen_content_tools_flag)
            fh->allow_screen_content_tools = 1;
    }

    if (fh->allow_screen_content_tools)
        fh->force_integer_mv_flag = (int)stb_av1_get_bit(gb);
    else
        fh->force_integer_mv_flag = 0;

    if (fh->force_integer_mv_flag)
        fh->force_integer_mv = 1;
    else
        fh->force_integer_mv = 0;

    /* For key or intra-only frames, force integer MV */
    if (fh->frame_type == 0 || fh->frame_type == 2)
        fh->force_integer_mv = 1;

    /* Frame size for key frame */
    if (fh->frame_type == 0) {
        /* KEY_FRAME: use dimensions from sequence header */
        fh->frame_width = sh->max_frame_width;
        fh->frame_height = sh->max_frame_height;
        fh->render_width = sh->max_frame_width;
        fh->render_height = sh->max_frame_height;
    } else {
        /* For non-key frames */
        int frame_size_override = 0;
        if (sh->enable_order_hint)
            frame_size_override = (int)stb_av1_get_bit(gb);
        if (frame_size_override) {
            fh->frame_width = (int)stb_av1_get_bits(gb, sh->frame_width_bits) + 1;
            fh->frame_height = (int)stb_av1_get_bits(gb, sh->frame_height_bits) + 1;
        } else {
            fh->frame_width = sh->max_frame_width;
            fh->frame_height = sh->max_frame_height;
        }
        fh->render_width = fh->frame_width;
        fh->render_height = fh->frame_height;
        fh->have_render_size = (int)stb_av1_get_bit(gb);
        if (fh->have_render_size) {
            fh->render_width = (int)stb_av1_get_bits(gb, 16) + 1;
            fh->render_height = (int)stb_av1_get_bits(gb, 16) + 1;
        }
    }

    /* Superres */
    if (sh->superres && fh->frame_type != 0) {
        fh->superres_enabled = (int)stb_av1_get_bit(gb);
        if (fh->superres_enabled) {
            fh->superres_scale_denominator = (int)stb_av1_get_bits(gb, 3) + 9;
            fh->superres_width = (fh->frame_width * 8 + fh->superres_scale_denominator / 2)
                                 / fh->superres_scale_denominator;
        } else {
            fh->superres_scale_denominator = 8;
            fh->superres_width = fh->frame_width;
        }
    }

    /* Refresh frame flags for key/intra-only */
    if (fh->frame_type == 0 || fh->frame_type == 2) {
        fh->refresh_frame_flags = (int)stb_av1_get_bits(gb, 8);
    }

    /* Loop filter */
    fh->loopfilter_enabled = (int)stb_av1_get_bit(gb);
    if (fh->loopfilter_enabled) {
        fh->loopfilter_level_y[0] = (int)stb_av1_get_bits(gb, 6);
        fh->loopfilter_level_y[1] = (int)stb_av1_get_bits(gb, 6);
        fh->loopfilter_level_u = (int)stb_av1_get_bits(gb, 6);
        fh->loopfilter_level_v = (int)stb_av1_get_bits(gb, 6);
        fh->loopfilter_sharpness = (int)stb_av1_get_bits(gb, 3);
        fh->loopfilter_delta_enabled = (int)stb_av1_get_bit(gb);
        if (fh->loopfilter_delta_enabled) {
            fh->loopfilter_delta_update = (int)stb_av1_get_bit(gb);
        }
    }

    /* CDEF */
    fh->cdef_enabled = (int)stb_av1_get_bit(gb);
    if (fh->cdef_enabled) {
        int cdef_bits = (int)stb_av1_get_bits(gb, 2);
        fh->cdef_n_bits = cdef_bits;
        for (i = 0; i < (1 << cdef_bits); i++) {
            fh->cdef_y_pri[i] = (int)stb_av1_get_bits(gb, 4);
            fh->cdef_y_sec[i] = (int)stb_av1_get_bits(gb, 2);
            if (sh->subsampling_x == 0 || sh->subsampling_y == 0) {
                fh->cdef_uv_pri[i] = (int)stb_av1_get_bits(gb, 4);
                fh->cdef_uv_sec[i] = (int)stb_av1_get_bits(gb, 2);
            }
        }
    }

    /* Loop restoration */
    if (sh->enable_restoration) {
        fh->lr_type[0] = (int)stb_av1_get_bits(gb, 2);
        fh->lr_type[1] = (int)stb_av1_get_bits(gb, 2);
        fh->lr_type[2] = (int)stb_av1_get_bits(gb, 2);
    }

    /* Quantization */
    fh->base_q_idx = (int)stb_av1_get_bits(gb, 8);
    fh->delta_q_y_dc = 0;
    fh->delta_q_u_ac = 0;
    fh->delta_q_u_dc = 0;
    fh->delta_q_v_ac = 0;
    fh->delta_q_v_dc = 0;
    fh->using_qmatrix = 0;
    fh->qm_y = 0;
    fh->qm_u = 0;
    if (fh->base_q_idx > 0) {
        fh->delta_q_present = (int)stb_av1_get_bit(gb);
        if (fh->delta_q_present) {
            fh->delta_q_res_log2 = (int)stb_av1_get_bits(gb, 3);
        }
        fh->using_qmatrix = (int)stb_av1_get_bit(gb);
        if (fh->using_qmatrix) {
            fh->qm_y = (int)stb_av1_get_bits(gb, 4);
            fh->qm_u = (int)stb_av1_get_bits(gb, 4);
        }
    }

    /* TX mode */
    fh->tx_mode_switchable = (int)stb_av1_get_bit(gb);
    if (fh->tx_mode_switchable)
        fh->tx_mode = (int)stb_av1_get_bits(gb, 2);
    else
        fh->tx_mode = STB_AV1_TX_SWITCHABLE;

    /* Reference mode for inter frames */
    if (fh->frame_type == 1) {
        fh->reference_select = (int)stb_av1_get_bit(gb);
        if (fh->reference_select)
            fh->reference_mode = 2; /* SELECT */
        else
            fh->reference_mode = 0; /* SINGLE_REFERENCE */
    }

    /* Skip mode */
    if (fh->frame_type == 1 && fh->reference_mode != 2)
        fh->skip_mode_present = (int)stb_av1_get_bit(gb);

    /* Reduced reference frame MV */
    if (sh->enable_ref_frame_mvs && fh->error_resilient_mode)
        fh->allow_ref_frame_mvs = (int)stb_av1_get_bit(gb);
    else
        fh->allow_ref_frame_mvs = 0;

    /* For key frame, skip mode is implicit */
    if (fh->frame_type == 0)
        fh->skip = 1;

    return stb_av1_check_trailing_bits(gb, 0);
}

/*
 * stb_av1_parse_frame_header - parse a frame header OBU from raw bytes.
 * Returns 0 on success, -1 on error.
 */
int stb_av1_parse_frame_header(void *frame_hdr_out,
                                const unsigned char *data,
                                unsigned int size,
                                const void *seq_hdr)
{
    struct StbAv1GetBits gb;
    struct stb_av1_frame_header *fh;
    const struct stb_av1_sequence_header *sh;
    int obu_has_ext;
    int obu_has_size;

    fh = (struct stb_av1_frame_header *)frame_hdr_out;
    sh = (const struct stb_av1_sequence_header *)seq_hdr;
    if (!fh || !data || size == 0) return -1;

    stb_av1_init_get_bits(&gb, data, size);

    /* Check for OBU header */
    if (gb.ptr < gb.ptr_end) {
        unsigned int first_byte = (unsigned int)gb.ptr[0];
        if ((first_byte & 0xF0) == 0x80) {
            /* OBU header detected - skip it */
            stb_av1_get_bits(&gb, 4);  /* forbidden + type */
            obu_has_ext = (int)stb_av1_get_bit(&gb);
            obu_has_size = (int)stb_av1_get_bit(&gb);
            (void)stb_av1_get_bit(&gb);  /* reserved bit */
            if (obu_has_ext) stb_av1_get_bits(&gb, 8);  /* temporal_id, spatial_id */
            if (obu_has_size) stb_av1_get_uleb128(&gb);  /* OBU size */
        }
    }

    return stb_av1_parse_frame_hdr_gb(&gb, fh, sh);
}
