/*
 * Copyright (c) 2018-2021, VideoLAN and dav1d authors
 * All rights reserved.
 * C89 port: stripped to intra-only still image decoding.
 * Removed: threading, atomics, SVC, motion comp, film grain, ref frames.
 */

#ifndef STB_AV1_SRC_INTERNAL_H
#define STB_AV1_SRC_INTERNAL_H

#include <stddef.h>  /* size_t */

/* Forward declarations */
struct StbAv1Msac;
struct StbAv1TileState;
struct StbAv1TaskContext;

/* C89: enum TaskType - simplified, no threading tasks */
enum StbAv1TaskType {
    STB_AV1_TASK_TYPE_INIT,
    STB_AV1_TASK_TYPE_TILE_RECONSTRUCTION
};

/* DSP context (minimal - just function pointers needed for still image) */
typedef struct StbAv1DSPContext {
    void *ipred;   /* StbAv1IntraPredDSPContext * (cast at use) */
    void *itx;     /* StbAv1InvTxfmDSPContext * */
    void *cdef;    /* StbAv1CdefDSPContext * */
    void *lr;      /* StbAv1LoopRestorationDSPContext * */
} StbAv1DSPContext;

/* Simplified dav1d context - NO threading, NO reference frames, NO film grain */
typedef struct StbAv1Context {
    /* Sequence and frame headers owned by this context */
    void *seq_hdr_ref;
    void *seq_hdr;
    void *frame_hdr_ref;
    void *frame_hdr;

    /* Tile data */
    struct StbAv1TileGroup {
        const unsigned char *data;
        size_t sz;
        int start;
        int end;
    } *tile;
    int n_tile_data;
    int n_tiles;

    /* Decoder state */
    struct StbAv1TileState *ts;
    int n_ts;

    /* Simplified: just 8bpc and 10bpc DSP */
    StbAv1DSPContext dsp[2];
} StbAv1Context;

/* Per-tile state */
typedef struct StbAv1TileState {
    void *cdf;           /* StbAv1CdfContext * */
    struct StbAv1Msac *msac;  /* Multi-symbol arithmetic coder (pointer) */

    /* Tiling in 4px units */
    struct {
        int col_start, col_end;
        int row_start, row_end;
        int col, row;
    } tiling;

    /* Dequantization */
    unsigned short dqmem[8][3][2];
    const unsigned short (*dq)[3][2];
    int last_qidx;

    /* Delta LFs */
    union {
        signed char i8[4];
        unsigned int u32;
    } last_delta_lf;

    /* Loop filter level memory */
    unsigned char lflvlmem[8][4][8][2];
    const unsigned char (*lflvl)[4][8][2];

    /* Loop restoration */
    void *lr_ref[3];
} StbAv1TileState;

/* Per-task context (simplified for single-tile, single-thread) */
typedef struct StbAv1TaskContext {
    const StbAv1Context *c;
    const struct StbAv1FrameContext *f;
    StbAv1TileState *ts;

    /* Current block position in 4px units */
    int bx;
    int by;

    /* Left and above block contexts */
    void *l;       /* BlockContext * */
    void *a;

    /* Scratch buffer for coefficients */
    union {
        signed short cf_8bpc[32 * 32];
        signed int cf_16bpc[32 * 32];
    } cf_union;

    /* Scratch for palette data */
    union {
        unsigned char al_pal_8bpc[2][32][3][8];
        unsigned short al_pal_16bpc[2][32][3][8];
    } al_pal_union;

    /* Warped motion params (intra-only: unused but kept for struct compat) */
    void *warpmv;

    /* Loop filter mask */
    void *lf_mask;

    /* Top pre-CDEF toggle */
    int top_pre_cdef_toggle;
    signed char *cur_sb_cdef_idx_ptr;

    /* Threading (minimal - single threaded) */
    struct {
        int pass;
    } frame_thread;
} StbAv1TaskContext;

/* Frame context (minimal - for still image only) */
struct StbAv1FrameContext {
    void *seq_hdr_ref;
    void *seq_hdr;
    void *frame_hdr_ref;
    void *frame_hdr;

    /* Current decoded picture */
    void *cur;

    /* Tile data */
    struct StbAv1TileGroup *tile;
    int n_tile_data;

    /* CDF state for this frame */
    void *in_cdf;
    void *out_cdf;

    /* Tile state */
    StbAv1TileState *ts;
    int n_ts;

    /* DSP functions */
    const StbAv1DSPContext *dsp;

    /* Block-level function pointers */
    struct {
        int (*recon_b_intra)(StbAv1TaskContext *t, int has_coeff);
        /* Simplified: no inter, no filter_sbrow, no CDEF/LR pipeline */
    } bd_fn;

    /* Dimensions in 4px units */
    int w4;
    int h4;
    int bw;
    int bh;
    int sb128w;
    int sb128h;
    int sbh;
    int sb_shift;
    int sb_step;

    /* Dequantization tables */
    unsigned short dq[8][3][2];
    const unsigned char *qm[15][3];
    void *a;           /* BlockContext *a */
    int a_sz;

    /* Reference motion vectors (simplified) */
    void *rf;

    /* Bit depth */
    int bitdepth_max;

    /* Loop filter state (minimal) */
    struct {
        void *mask;       /* Av1Filter * */
        void *lr_mask;    /* Av1Restoration * */
        int mask_sz;
        int lr_mask_sz;
        int cdef_buf_plane_sz[2];
        int cdef_buf_sbh;
        void *lim_lut;
        unsigned char lvl[8][4][8][2];
        int last_sharpness;
        unsigned char *tx_lpf_right_edge[2];
        void *cdef_line_buf;
        void *lr_line_buf;
        void *cdef_line[2][3];
        void *cdef_lpf_line[3];
        void *lr_lpf_line[3];
        void *p[3];
        void *sr_p[3];
    } lf;

    /* Threading (single-threaded stub) */
    struct {
        void *tasks;
        int num_tasks;
        int retval;
    } task_thread;
};

#endif /* STB_AV1_SRC_INTERNAL_H */
