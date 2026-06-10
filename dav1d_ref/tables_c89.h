/*
 * Copyright (c) 2018-2021, VideoLAN and dav1d authors
 * All rights reserved.
 * C89 port: table declarations for scan patterns, dimensions, etc.
 */

#ifndef STB_AV1_SRC_TABLES_H
#define STB_AV1_SRC_TABLES_H

#include "dav1d_ref/levels_c89.h"

/* C89: basic types used in table declarations */
#ifndef STB_AV1_N_FILTERS
#define STB_AV1_N_FILTERS 4  /* number of 1D filter types */
#endif

typedef struct StbAv1TxfmInfo {
    unsigned char w, h, lw, lh, min, max, sub, ctx;
} StbAv1TxfmInfo;

/* Partition context table [2][N_BL_LEVELS][N_PARTITIONS] */
extern const unsigned char stb_av1_al_part_ctx[2][STB_AV1_N_BL_LEVELS][STB_AV1_N_PARTITIONS];

/* Block sizes [N_BL_LEVELS][N_PARTITIONS][2] */
extern const unsigned char stb_av1_block_sizes[STB_AV1_N_BL_LEVELS][STB_AV1_N_PARTITIONS][2];

/* Block dimensions [N_BS_SIZES][4]: width, height, log2w, log2h */
extern const unsigned char stb_av1_block_dimensions[STB_AV1_N_BS_SIZES][4];

/* Transform dimensions [N_RECT_TX_SIZES] */
extern const StbAv1TxfmInfo stb_av1_txfm_dimensions[STB_AV1_N_RECT_TX_SIZES];

/* Max transform size for each block size [N_BS_SIZES][4] (y, 420, 422, 444) */
extern const unsigned char stb_av1_max_txfm_size_for_bs[STB_AV1_N_BS_SIZES][4];

/* Transform type from UV intra mode */
extern const unsigned char stb_av1_txtp_from_uvmode[STB_AV1_N_UV_INTRA_PRED_MODES];

/* Partition type count per block level */
extern const unsigned char stb_av1_partition_type_count[STB_AV1_N_BL_LEVELS];

/* Transform types per set */
extern const unsigned char stb_av1_tx_types_per_set[40];

/* Filter mode to Y intra mode */
extern const unsigned char stb_av1_filter_mode_to_y_mode[5];

/* Y-mode size context */
extern const unsigned char stb_av1_ymode_size_context[STB_AV1_N_BS_SIZES];

/* Coefficient context offsets */
extern const unsigned char stb_av1_lo_ctx_offsets[3][5][5];

/* Skip context */
extern const unsigned char stb_av1_skip_ctx[5][5];

/* Transform type class [N_TX_TYPES_PLUS_LL] */
extern const unsigned char stb_av1_tx_type_class[STB_AV1_N_TX_TYPES_PLUS_LL];

/* Intra mode context */
extern const unsigned char stb_av1_intra_mode_context[STB_AV1_N_INTRA_PRED_MODES];

/* Wedge context lookup */
extern const unsigned char stb_av1_wedge_ctx_lut[STB_AV1_N_BS_SIZES];

/* CDEF directions */
extern const signed char stb_av1_cdef_directions[12][2];

/* SGR parameters */
extern const unsigned short stb_av1_sgr_params[16][2];
extern const unsigned char stb_av1_sgr_x_by_x[256];

/* OBMC masks */
extern const unsigned char stb_av1_obmc_masks[64];

/* Smooth weights */
extern const unsigned char stb_av1_sm_weights[128];

/* Dr intra derivative */
extern const unsigned short stb_av1_dr_intra_derivative[44];

/* Filter intra taps */
extern const signed char stb_av1_filter_intra_taps[5][64];

/* CFL allowed block sizes bitmask */
static const unsigned stb_av1_cfl_allowed_mask =
    (1u << STB_AV1_BS_32x32) | (1u << STB_AV1_BS_32x16) | (1u << STB_AV1_BS_32x8) |
    (1u << STB_AV1_BS_16x32) | (1u << STB_AV1_BS_16x16) | (1u << STB_AV1_BS_16x8) |
    (1u << STB_AV1_BS_16x4)  | (1u << STB_AV1_BS_8x32)  | (1u << STB_AV1_BS_8x16) |
    (1u << STB_AV1_BS_8x8)   | (1u << STB_AV1_BS_8x4)   | (1u << STB_AV1_BS_4x16) |
    (1u << STB_AV1_BS_4x8)   | (1u << STB_AV1_BS_4x4);

/* Wedge allowed block sizes bitmask */
static const unsigned stb_av1_wedge_allowed_mask =
    (1u << STB_AV1_BS_32x32) | (1u << STB_AV1_BS_32x16) | (1u << STB_AV1_BS_32x8) |
    (1u << STB_AV1_BS_16x32) | (1u << STB_AV1_BS_16x16) | (1u << STB_AV1_BS_16x8) |
    (1u << STB_AV1_BS_8x32)  | (1u << STB_AV1_BS_8x16)  | (1u << STB_AV1_BS_8x8);

/* Inter-intra allowed block sizes bitmask */
static const unsigned stb_av1_interintra_allowed_mask =
    (1u << STB_AV1_BS_32x32) | (1u << STB_AV1_BS_32x16) | (1u << STB_AV1_BS_16x32) |
    (1u << STB_AV1_BS_16x16) | (1u << STB_AV1_BS_16x8)  | (1u << STB_AV1_BS_8x16) |
    (1u << STB_AV1_BS_8x8);

#endif /* STB_AV1_SRC_TABLES_H */
