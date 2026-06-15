/*
 * C89 port of dav1d's decode_coefs (from recon_tmpl.c)
 * Intra-only, 8-bit only, simplified context selection.
 * FIXED: tx_size CDF index mapping (lw, not lw+lh).
 * FIXED: EOB bin array selection based on block size.
 */
#include "dav1d_ref/cdf_c89.h"

/* Helper: get skip context based on neighbor coefficients */
static int stb_get_skip_ctx(const StbAv1TxfmInfo *t_dim, int bs,
                             unsigned char *a, unsigned char *l, int chroma)
{
    int above = a ? (*a >> 6) : 0;
    int left  = l ? (*l >> 6) : 0;
    (void)t_dim; (void)bs;
    if (above && left) return 3 + chroma;
    if (above || left) return 2 + chroma;
    return 0;
}

static int stb_get_dc_sign_ctx(int dc_category, int plane) {
    if (dc_category == 0) return 0;
    if (plane == 1) {
        return dc_category == 1 ? 1 : 2;
    }
    return dc_category == 1 ? 2 : 1;
}

static unsigned stb_av1_msac_decode_hi_tok(struct stb_av1_msac *msac,
                                            unsigned short *cdf) {
    unsigned tok;
    tok = stb_av1_msac_decode_symbol(msac, cdf, 3);
    if (tok == 3) {
        unsigned tok2 = stb_av1_msac_decode_symbol(msac, cdf, 3);
        if (tok2 > 0) {
            /* Read remaining bits for very large coefficient */
            unsigned rem = stb_av1_msac_decode_bools(msac, tok2 + 6);
            tok = 7 + rem;
        } else {
            tok = 4 + stb_av1_msac_decode_bools(msac, 2);
        }
    } else {
        tok = tok < 2 ? tok + 1 : 4 + stb_av1_msac_decode_bools(msac, 1);
    }
    return tok;
}

/* Main coefficient decoder: ported from dav1d's decode_coefs.
   Returns EOB position, or -1 if all coefficients are zero.
   Fills coeffs array with decoded signed coefficients. */
static int stb_decode_coefs(struct StbAv1Msac *msac,
                             struct StbCdfContext *cdf,
                             int *coeffs, int tx_w, int tx_h,
                             int *eob_out,
                             unsigned char *a_ctx, unsigned char *l_ctx,
                             int plane, int intra)
{
    StbAv1TxfmInfo t_dim;
    int tx_sz_idx; /* square tx_size index (0-4) = log2(w)-2 */
    int max_coeffs = tx_w * tx_h;
    int i, eob, all_skip;
    int sctx;
    unsigned short sk_cdf[3];
    (void)intra;

    /* Setup t_dim. For square blocks, lw == lh and tx_sz_idx = lw. */
    t_dim.w = tx_w / 4; t_dim.h = tx_h / 4;
    t_dim.lw = 0; while ((1 << (t_dim.lw + 2)) < tx_w) t_dim.lw++;
    t_dim.lh = 0; while ((1 << (t_dim.lh + 2)) < tx_h) t_dim.lh++;
    t_dim.min = t_dim.lw < t_dim.lh ? t_dim.lw : t_dim.lh;
    t_dim.max = t_dim.lw > t_dim.lh ? t_dim.lw : t_dim.lh;
    t_dim.sub = 0; t_dim.pad = 0;
    tx_sz_idx = t_dim.lw; /* for square blocks, matches TX_4X4..TX_64X64 */
    t_dim.ctx = t_dim.lw + t_dim.lh;

    /* Decode skip flag */
    sctx = stb_get_skip_ctx(&t_dim, 0, a_ctx, l_ctx, plane ? 1 : 0);
    sk_cdf[0] = cdf->skip[tx_sz_idx][sctx][0];
    sk_cdf[1] = 0; /* adaptation count starts at 0 */
    sk_cdf[2] = 0;
    all_skip = (int)stb_av1_msac_decode_bool_adapt(msac, sk_cdf);
    if (all_skip) {
        for (i = 0; i < max_coeffs; i++) coeffs[i] = 0;
        *eob_out = -1;
        return -1;
    }

    /* Transform type: use DCT_DCT for all blocks (simplified) */
    /* In full dav1d, this decodes the transform type from CDFs */

    /* Decode EOB position */
    {
        int is_1d = 0; /* TX_CLASS_2D */
        unsigned short eob_cdf[16];
        int ns, eob_bin_sz;

        /* eob_bin_sz is the size index for selecting eob_bin_XX array:
           0 = eob_bin_16 (4 symbols), 1 = eob_bin_32 (5),
           2 = eob_bin_64 (6), 3 = eob_bin_128 (7),
           4 = eob_bin_256 (8), 5 = eob_bin_512 (9),
           6 = eob_bin_1024 (10) */
        if (max_coeffs <= 16) {
            eob_bin_sz = 0; ns = 4; /* eob_bin_16 */
            eob_cdf[0] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][0];
            eob_cdf[1] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][1];
            eob_cdf[2] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][2];
            eob_cdf[3] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][3];
        } else if (max_coeffs <= 32) {
            eob_bin_sz = 1; ns = 5; /* eob_bin_32 */
            for (i = 0; i < 5; i++)
                eob_cdf[i] = cdf->eob_bin_32[plane ? 1 : 0][is_1d][i];
        } else if (max_coeffs <= 64) {
            eob_bin_sz = 2; ns = 6; /* eob_bin_64 */
            for (i = 0; i < 6; i++)
                eob_cdf[i] = cdf->eob_bin_64[plane ? 1 : 0][is_1d][i];
        } else if (max_coeffs <= 128) {
            eob_bin_sz = 3; ns = 7; /* eob_bin_128 */
            for (i = 0; i < 7; i++)
                eob_cdf[i] = cdf->eob_bin_128[plane ? 1 : 0][is_1d][i];
        } else if (max_coeffs <= 256) {
            eob_bin_sz = 4; ns = 8; /* eob_bin_256 */
            for (i = 0; i < 8; i++)
                eob_cdf[i] = cdf->eob_bin_256[plane ? 1 : 0][is_1d][i];
        } else if (max_coeffs <= 512) {
            eob_bin_sz = 5; ns = 9; /* eob_bin_512 */
            for (i = 0; i < 9; i++)
                eob_cdf[i] = cdf->eob_bin_512[plane ? 1 : 0][i];
        } else {
            eob_bin_sz = 6; ns = 10; /* eob_bin_1024 */
            for (i = 0; i < 10; i++)
                eob_cdf[i] = cdf->eob_bin_1024[plane ? 1 : 0][i];
        }
        eob_cdf[ns] = 0; /* count for adaptation */
        eob = (int)stb_av1_msac_decode_symbol(msac, eob_cdf, (unsigned long)ns);
        (void)eob_bin_sz;
    }

    if (eob > 1) {
        int eob_bin = eob - 2;
        unsigned short eob_hi_cdf[3];
        int eob_hi_bit;
        eob_hi_cdf[0] = cdf->eob_hi_bit[tx_sz_idx][plane ? 1 : 0][eob_bin][0];
        eob_hi_cdf[1] = 0; /* adaptation count */
        eob_hi_cdf[2] = 0;
        eob_hi_bit = (int)stb_av1_msac_decode_bool_adapt(msac, eob_hi_cdf);
        if (eob_hi_bit) {
            eob = ((1 | 2) << eob_bin) | (int)stb_av1_msac_decode_bools(msac, (unsigned)eob_bin);
        } else {
            eob = (2 << eob_bin) | (int)stb_av1_msac_decode_bools(msac, (unsigned)eob_bin);
        }
    }
    if (eob < 0) eob = 0;
    if (eob >= max_coeffs) eob = max_coeffs - 1;

    /* Decode coefficient tokens */
    for (i = 0; i < max_coeffs; i++) {
        if (i <= eob) {
            unsigned short tok_cdf[5];
            unsigned tok;
            /* Use eob_base_tok for position 0, base_tok for others */
            if (i == 0) {
                tok_cdf[0] = cdf->eob_base_tok[tx_sz_idx][plane ? 1 : 0][0][0];
                tok_cdf[1] = cdf->eob_base_tok[tx_sz_idx][plane ? 1 : 0][0][1];
                tok_cdf[2] = cdf->eob_base_tok[tx_sz_idx][plane ? 1 : 0][0][2];
                tok_cdf[3] = cdf->eob_base_tok[tx_sz_idx][plane ? 1 : 0][0][3];
            } else {
                tok_cdf[0] = cdf->base_tok[tx_sz_idx][plane ? 1 : 0][i][0];
                tok_cdf[1] = cdf->base_tok[tx_sz_idx][plane ? 1 : 0][i][1];
                tok_cdf[2] = cdf->base_tok[tx_sz_idx][plane ? 1 : 0][i][2];
                tok_cdf[3] = cdf->base_tok[tx_sz_idx][plane ? 1 : 0][i][3];
            }
            tok_cdf[4] = 0; /* count */
            tok = stb_av1_msac_decode_symbol(msac, tok_cdf, 3);

            if (tok == 3) {
                /* Decode higher levels with br_tok */
                unsigned short br_cdf[6];
                int br_tx = tx_sz_idx > 3 ? 3 : tx_sz_idx;
                br_cdf[0] = cdf->br_tok[br_tx][plane ? 1 : 0][i][0];
                br_cdf[1] = cdf->br_tok[br_tx][plane ? 1 : 0][i][1];
                br_cdf[2] = cdf->br_tok[br_tx][plane ? 1 : 0][i][2];
                br_cdf[3] = cdf->br_tok[br_tx][plane ? 1 : 0][i][3];
                br_cdf[4] = cdf->br_tok[br_tx][plane ? 1 : 0][i][4];
                br_cdf[5] = 0; /* count */
                coeffs[i] = (int)stb_av1_msac_decode_hi_tok(msac, br_cdf);
            } else {
                coeffs[i] = (int)tok;
            }

            /* Decode sign for non-zero coefficients */
            if (coeffs[i] != 0) {
                if (stb_av1_msac_decode_bool_equi(msac))
                    coeffs[i] = -coeffs[i];
            }
        } else {
            coeffs[i] = 0;
        }
    }

    *eob_out = eob;
    return eob;
}
