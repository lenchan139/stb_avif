/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ANY DEALINGS IN THE SOFTWARE.
 */

/* C89 port: renamed types, no anonymous unions/structs, no // comments */

#ifndef STB_AV1_SRC_LEVELS_H
#define STB_AV1_SRC_LEVELS_H

/* C89 fixed-size types (aliases for stb_avif.h types) */
/* unsigned char = stbv_u8, signed char = stbv_s8 */
/* unsigned short = stbv_u16, signed short = stbv_s16 */
/* unsigned int = stbv_u32, int = stbv_s32 */

enum StbAv1ObuMetaType {
    STB_AV1_OBU_META_HDR_CLL     = 1,
    STB_AV1_OBU_META_HDR_MDCV    = 2,
    STB_AV1_OBU_META_SCALABILITY = 3,
    STB_AV1_OBU_META_ITUT_T35    = 4,
    STB_AV1_OBU_META_TIMECODE    = 5,
};

enum StbAv1TxfmSize {
    STB_AV1_TX_4X4,
    STB_AV1_TX_8X8,
    STB_AV1_TX_16X16,
    STB_AV1_TX_32X32,
    STB_AV1_TX_64X64,
    STB_AV1_N_TX_SIZES,
};

enum StbAv1BlockLevel {
    STB_AV1_BL_128X128,
    STB_AV1_BL_64X64,
    STB_AV1_BL_32X32,
    STB_AV1_BL_16X16,
    STB_AV1_BL_8X8,
    STB_AV1_N_BL_LEVELS,
};

enum StbAv1RectTxfmSize {
    STB_AV1_RTX_4X8 = STB_AV1_N_TX_SIZES,
    STB_AV1_RTX_8X4,
    STB_AV1_RTX_8X16,
    STB_AV1_RTX_16X8,
    STB_AV1_RTX_16X32,
    STB_AV1_RTX_32X16,
    STB_AV1_RTX_32X64,
    STB_AV1_RTX_64X32,
    STB_AV1_RTX_4X16,
    STB_AV1_RTX_16X4,
    STB_AV1_RTX_8X32,
    STB_AV1_RTX_32X8,
    STB_AV1_RTX_16X64,
    STB_AV1_RTX_64X16,
    STB_AV1_N_RECT_TX_SIZES
};

enum StbAv1TxfmType {
    STB_AV1_DCT_DCT,
    STB_AV1_ADST_DCT,
    STB_AV1_DCT_ADST,
    STB_AV1_ADST_ADST,
    STB_AV1_FLIPADST_DCT,
    STB_AV1_DCT_FLIPADST,
    STB_AV1_FLIPADST_FLIPADST,
    STB_AV1_ADST_FLIPADST,
    STB_AV1_FLIPADST_ADST,
    STB_AV1_IDTX,
    STB_AV1_V_DCT,
    STB_AV1_H_DCT,
    STB_AV1_V_ADST,
    STB_AV1_H_ADST,
    STB_AV1_V_FLIPADST,
    STB_AV1_H_FLIPADST,
    STB_AV1_N_TX_TYPES,
    STB_AV1_WHT_WHT = STB_AV1_N_TX_TYPES,
    STB_AV1_N_TX_TYPES_PLUS_LL,
};

enum StbAv1TxClass {
    STB_AV1_TX_CLASS_2D,
    STB_AV1_TX_CLASS_H,
    STB_AV1_TX_CLASS_V,
};

enum StbAv1IntraPredMode {
    STB_AV1_DC_PRED,
    STB_AV1_VERT_PRED,
    STB_AV1_HOR_PRED,
    STB_AV1_DIAG_DOWN_LEFT_PRED,
    STB_AV1_DIAG_DOWN_RIGHT_PRED,
    STB_AV1_VERT_RIGHT_PRED,
    STB_AV1_HOR_DOWN_PRED,
    STB_AV1_HOR_UP_PRED,
    STB_AV1_VERT_LEFT_PRED,
    STB_AV1_SMOOTH_PRED,
    STB_AV1_SMOOTH_V_PRED,
    STB_AV1_SMOOTH_H_PRED,
    STB_AV1_PAETH_PRED,
    STB_AV1_N_INTRA_PRED_MODES,
    STB_AV1_CFL_PRED = STB_AV1_N_INTRA_PRED_MODES,
    STB_AV1_N_UV_INTRA_PRED_MODES,
    STB_AV1_N_IMPL_INTRA_PRED_MODES = STB_AV1_N_UV_INTRA_PRED_MODES,
    STB_AV1_LEFT_DC_PRED = STB_AV1_DIAG_DOWN_LEFT_PRED,
    STB_AV1_TOP_DC_PRED,
    STB_AV1_DC_128_PRED,
    STB_AV1_Z1_PRED,
    STB_AV1_Z2_PRED,
    STB_AV1_Z3_PRED,
    STB_AV1_FILTER_PRED = STB_AV1_N_INTRA_PRED_MODES,
};

enum StbAv1InterIntraPredMode {
    STB_AV1_II_DC_PRED,
    STB_AV1_II_VERT_PRED,
    STB_AV1_II_HOR_PRED,
    STB_AV1_II_SMOOTH_PRED,
    STB_AV1_N_INTER_INTRA_PRED_MODES,
};

enum StbAv1BlockPartition {
    STB_AV1_PARTITION_NONE,
    STB_AV1_PARTITION_H,
    STB_AV1_PARTITION_V,
    STB_AV1_PARTITION_SPLIT,
    STB_AV1_PARTITION_T_TOP_SPLIT,
    STB_AV1_PARTITION_T_BOTTOM_SPLIT,
    STB_AV1_PARTITION_T_LEFT_SPLIT,
    STB_AV1_PARTITION_T_RIGHT_SPLIT,
    STB_AV1_PARTITION_H4,
    STB_AV1_PARTITION_V4,
    STB_AV1_N_PARTITIONS,
    STB_AV1_N_SUB8X8_PARTITIONS = STB_AV1_PARTITION_T_TOP_SPLIT,
};

enum StbAv1BlockSize {
    STB_AV1_BS_128x128,
    STB_AV1_BS_128x64,
    STB_AV1_BS_64x128,
    STB_AV1_BS_64x64,
    STB_AV1_BS_64x32,
    STB_AV1_BS_64x16,
    STB_AV1_BS_32x64,
    STB_AV1_BS_32x32,
    STB_AV1_BS_32x16,
    STB_AV1_BS_32x8,
    STB_AV1_BS_16x64,
    STB_AV1_BS_16x32,
    STB_AV1_BS_16x16,
    STB_AV1_BS_16x8,
    STB_AV1_BS_16x4,
    STB_AV1_BS_8x32,
    STB_AV1_BS_8x16,
    STB_AV1_BS_8x8,
    STB_AV1_BS_8x4,
    STB_AV1_BS_4x16,
    STB_AV1_BS_4x8,
    STB_AV1_BS_4x4,
    STB_AV1_N_BS_SIZES,
};

enum StbAv1Filter2d {
    STB_AV1_FILTER_2D_8TAP_REGULAR,
    STB_AV1_FILTER_2D_8TAP_REGULAR_SMOOTH,
    STB_AV1_FILTER_2D_8TAP_REGULAR_SHARP,
    STB_AV1_FILTER_2D_8TAP_SHARP_REGULAR,
    STB_AV1_FILTER_2D_8TAP_SHARP_SMOOTH,
    STB_AV1_FILTER_2D_8TAP_SHARP,
    STB_AV1_FILTER_2D_8TAP_SMOOTH_REGULAR,
    STB_AV1_FILTER_2D_8TAP_SMOOTH,
    STB_AV1_FILTER_2D_8TAP_SMOOTH_SHARP,
    STB_AV1_FILTER_2D_BILINEAR,
    STB_AV1_N_2D_FILTERS,
};

enum StbAv1MVJoint {
    STB_AV1_MV_JOINT_ZERO,
    STB_AV1_MV_JOINT_H,
    STB_AV1_MV_JOINT_V,
    STB_AV1_MV_JOINT_HV,
    STB_AV1_N_MV_JOINTS,
};

enum StbAv1InterPredMode {
    STB_AV1_NEARESTMV,
    STB_AV1_NEARMV,
    STB_AV1_GLOBALMV,
    STB_AV1_NEWMV,
    STB_AV1_N_INTER_PRED_MODES,
};

enum StbAv1DRL_PROXIMITY {
    STB_AV1_NEAREST_DRL,
    STB_AV1_NEARER_DRL,
    STB_AV1_NEAR_DRL,
    STB_AV1_NEARISH_DRL
};

enum StbAv1CompInterPredMode {
    STB_AV1_NEARESTMV_NEARESTMV,
    STB_AV1_NEARMV_NEARMV,
    STB_AV1_NEARESTMV_NEWMV,
    STB_AV1_NEWMV_NEARESTMV,
    STB_AV1_NEARMV_NEWMV,
    STB_AV1_NEWMV_NEARMV,
    STB_AV1_GLOBALMV_GLOBALMV,
    STB_AV1_NEWMV_NEWMV,
    STB_AV1_N_COMP_INTER_PRED_MODES,
};

enum StbAv1CompInterType {
    STB_AV1_COMP_INTER_NONE,
    STB_AV1_COMP_INTER_WEIGHTED_AVG,
    STB_AV1_COMP_INTER_AVG,
    STB_AV1_COMP_INTER_SEG,
    STB_AV1_COMP_INTER_WEDGE,
};

enum StbAv1InterIntraType {
    STB_AV1_INTER_INTRA_NONE,
    STB_AV1_INTER_INTRA_BLEND,
    STB_AV1_INTER_INTRA_WEDGE,
};

/* C89: named the struct inside the union since anonymous structs are C99 */
typedef union StbAv1Mv {
    struct StbAv1MvComponents {
        signed short y;
        signed short x;
    } comp;
    unsigned int n;
} StbAv1Mv;

enum StbAv1MotionMode {
    STB_AV1_MM_TRANSLATION,
    STB_AV1_MM_OBMC,
    STB_AV1_MM_WARP,
};

#define STB_AV1_QINDEX_RANGE 256

/* C89: Av1Block struct - removed anonymous unions/structs */
/* Named the inner structs to avoid C99 anonymous feature */
typedef struct StbAv1BlockIntra {
    unsigned char y_mode;
    unsigned char uv_mode;
    unsigned char tx;
    unsigned char pal_sz[2];
    signed char y_angle;
    signed char uv_angle;
    signed char cfl_alpha[2];
} StbAv1BlockIntra;

typedef struct StbAv1BlockInterMv {
    StbAv1Mv mv[2];
    unsigned char wedge_idx;
    unsigned char mask_sign;
    unsigned char interintra_mode;
} StbAv1BlockInterMv;

typedef struct StbAv1BlockInterWarp {
    StbAv1Mv mv2d;
    signed short matrix[4];
} StbAv1BlockInterWarp;

typedef struct StbAv1BlockInter {
    StbAv1BlockInterMv mvm;
    StbAv1BlockInterWarp warp;
    unsigned char comp_type;
    unsigned char inter_mode;
    unsigned char motion_mode;
    unsigned char drl_idx;
    signed char ref[2];
    unsigned char max_ytx;
    unsigned char filter2d;
    unsigned char interintra_type;
    unsigned char tx_split0;
    unsigned short tx_split1;
} StbAv1BlockInter;

typedef struct StbAv1Block {
    unsigned char bl;
    unsigned char bs;
    unsigned char bp;
    unsigned char intra;
    unsigned char seg_id;
    unsigned char skip_mode;
    unsigned char skip;
    unsigned char uvtx;
    StbAv1BlockIntra intra_data;
    StbAv1BlockInter inter_data;
} StbAv1Block;

#endif /* STB_AV1_SRC_LEVELS_H */
