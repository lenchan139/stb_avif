/*
 * Copyright (c) 2018, VideoLAN and dav1d authors
 * All rights reserved.
 * C89 port: block context and CDF selection (intra-only).
 */

#ifndef STB_AV1_SRC_ENV_H
#define STB_AV1_SRC_ENV_H

#include <stddef.h>
#include <stdlib.h>

#include "dav1d_ref/levels_c89.h"

/* Block context - stores per-column state for neighbor-dependent decoding */
typedef struct StbAv1BlockContext {
    unsigned char mode[32];
    unsigned char lcoef[32];
    unsigned char ccoef[2][32];
    unsigned char seg_pred[32];
    unsigned char skip[32];
    unsigned char skip_mode[32];
    unsigned char intra[32];
    unsigned char comp_type[32];
    signed char ref[2][32];
    unsigned char filter[2][32];
    signed char tx_intra[32];
    signed char tx[32];
    unsigned char tx_lpf_y[32];
    unsigned char tx_lpf_uv[32];
    unsigned char partition[16];
    unsigned char uvmode[32];
    unsigned char pal_sz[32];
} StbAv1BlockContext;

/* Context derivation for intra prediction mode selection */
static int stb_av1_get_intra_ctx(const StbAv1BlockContext *a,
                                  const StbAv1BlockContext *l,
                                  int y_mode, int a_idx, int l_idx)
{
    if (a && a->mode[a_idx] < STB_AV1_N_INTRA_PRED_MODES) {
        if (l && l->mode[l_idx] < STB_AV1_N_INTRA_PRED_MODES) {
            return (a->mode[a_idx] > y_mode) + (l->mode[l_idx] > y_mode);
        }
        return a->mode[a_idx] > y_mode;
    }
    if (l && l->mode[l_idx] < STB_AV1_N_INTRA_PRED_MODES)
        return l->mode[l_idx] > y_mode;
    return 0;
}

/* Context derivation for transform size selection */
static int stb_av1_get_tx_ctx(const StbAv1BlockContext *a,
                               const StbAv1BlockContext *l,
                               int tx_sz, int a_idx, int l_idx)
{
    int above = (a && a->tx_intra[a_idx] < tx_sz) ? 1 : 0;
    int left  = (l && l->tx_intra[l_idx] < tx_sz) ? 1 : 0;
    return above + left;
}

/* Context derivation for DC/AC coefficient skip */
static int stb_av1_get_dc_ctx(const StbAv1BlockContext *a,
                               const StbAv1BlockContext *l,
                               int a_idx, int l_idx, int plane)
{
    int above = a ? (a->lcoef[a_idx] | (plane > 0 ? a->ccoef[plane-1][a_idx] : 0)) : 0;
    int left  = l ? (l->lcoef[l_idx] | (plane > 0 ? l->ccoef[plane-1][l_idx] : 0)) : 0;
    return (above > 0) + (left > 0);
}

/* Get partition sub-block type for a given block position */
static int stb_av1_get_partition_subclass(int partition, int block)
{
    /* Simplified: returns the partition type for the sub-block */
    if (partition == STB_AV1_PARTITION_NONE) return STB_AV1_PARTITION_NONE;
    if (partition == STB_AV1_PARTITION_H)    return (block < 2) ? STB_AV1_PARTITION_NONE : STB_AV1_PARTITION_NONE;
    if (partition == STB_AV1_PARTITION_V)    return (block & 1) ? STB_AV1_PARTITION_NONE : STB_AV1_PARTITION_NONE;
    return STB_AV1_PARTITION_SPLIT;
}

/* Set the block context after decoding a block */
static void stb_av1_set_blk_ctx(StbAv1BlockContext *ctx, int idx,
                                 unsigned char mode, unsigned char intra,
                                 unsigned char skip, signed char tx)
{
    ctx->mode[idx] = mode;
    ctx->intra[idx] = intra;
    ctx->skip[idx] = skip;
    ctx->tx_intra[idx] = tx;
    ctx->tx[idx] = tx;
}

#endif /* STB_AV1_SRC_ENV_H */
