/*
 * C89 port of dav1d's recon_tmpl.c
 * INTRA-ONLY, 8-bit AVIF still image decoding.
 *
 * Removed:
 * - Frame threading (t->frame_thread.pass)
 * - Inter prediction (mc, obmc, warp_affine, recon_b_inter)
 * - High bitdepth paths (10/12-bit) - 8-bit only
 * - Filter/loop functions (deblock, cdef, lr) - in separate files
 * - Reference frame handling
 * - SVC, film grain
 *
 * C89 compatibility:
 * - All // comments -> /* */
 * - No inline, restrict keywords
 * - No C99 for (int i=...) - declare at block top
 * - No designated initializers
 * - No __func__, __VA_ARGS__
 * - Types: unsigned char, unsigned short, unsigned int, signed int
 * - Renamed: dav1d_* -> stb_av1_*, Dav1d* -> StbAv1*
 */

#ifndef STB_AV1_RECON_TMPL_C89_H
#define STB_AV1_RECON_TMPL_C89_H

/* Forward declarations */
struct StbAv1Context;
struct StbAv1FrameContext;
struct StbAv1TaskContext;
struct StbAv1TileState;
struct StbAv1CdfContext;
struct stb_av1_msac;
struct StbAv1Block;
struct StbAv1TxfmInfo;

/* TxfmInfo struct (matches dav1d's TxfmInfo, C89-compatible) */
typedef struct StbAv1TxfmInfo {
    unsigned char w;
    unsigned char h;
    unsigned char lw;    /* log2 width */
    unsigned char lh;    /* log2 height */
    unsigned char ctx;   /* context index = lw + lh */
    unsigned char min;   /* min(lw, lh) */
    unsigned char max;   /* max(lw, lh) */
    unsigned char sub;   /* sub-tx size enum index */
    unsigned char pad;
} StbAv1TxfmInfo;

/* Block context for coefficient tracking */
typedef struct StbAv1BlockContext {
    unsigned char lcoef[32];
    unsigned char ccoef[2][16];
    unsigned char pal_sz[32];
    unsigned char filter[2][32];
    unsigned char txtp_map[32][32];
} StbAv1BlockContext;

/* Intra prediction edge buffer (scratch space) */
typedef struct StbAv1IntraScratch {
    unsigned char edge[288 + 32];  /* 128 + 128 + 32 for intra edges */
    unsigned char pal_y[3][8];
    unsigned char pal_uv[3][8];
    unsigned char pal_idx_y[32 * 32];
    unsigned char pal_idx_uv[32 * 32];
    signed short cf[32 * 32];
    unsigned char levels[64 * 36]; /* for coefficient decode */
} StbAv1IntraScratch;

/* Exported functions */
void stb_av1_recon_b_intra(struct StbAv1TaskContext *t, int bs, int edge_flags,
                           struct StbAv1Block *b);
void stb_av1_backup_ipred_edge(struct StbAv1TaskContext *t);
void stb_av1_copy_pal_block_y(struct StbAv1TaskContext *t, int bx4, int by4, int bw4, int bh4);
void stb_av1_copy_pal_block_uv(struct StbAv1TaskContext *t, int bx4, int by4, int bw4, int bh4);
void stb_av1_read_pal_plane(struct StbAv1TaskContext *t, struct StbAv1Block *b, int pl, int sz_ctx, int bx4, int by4);
void stb_av1_read_pal_uv(struct StbAv1TaskContext *t, struct StbAv1Block *b, int sz_ctx, int bx4, int by4);

/* Scan order tables (declared in recon_tmpl_c89.c, defined in recon_tmpl_c89_data.c) */
extern const unsigned short stb_av1_scans_4x4[16];
extern const unsigned short stb_av1_scans_8x8[64];
extern const unsigned short stb_av1_scans_16x16[256];
extern const unsigned short stb_av1_scans_32x32[1024];

/* Txfm dimensions table (declared in recon_tmpl_c89.c) */
extern const StbAv1TxfmInfo stb_av1_txfm_dimensions[30];

/* Block dimensions table */
extern const unsigned char stb_av1_block_dimensions[20][4];

/* Skip context table */
extern const unsigned char stb_av1_skip_ctx[5][5];

/* 2D scan order - zigzag for 4x4 */
static const unsigned char stb_av1_zigzag_4x4[16] = {
    0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};
static const unsigned char stb_av1_zigzag_8x8[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

#endif /* STB_AV1_RECON_TMPL_C89_H */