/*
 * C89 port of dav1d's decode_coefs (from recon_tmpl.c)
 * Intra-only, 8-bit only, simplified context selection.
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
    int tx_sz = (tx_w == 4 ? 0 : tx_w == 8 ? 1 : tx_w == 16 ? 2 : 3);
    int max_coeffs = tx_w * tx_h;
    int i, eob, all_skip;
    int sctx;
    unsigned short sk_cdf[3];
    (void)intra;
    (void)plane;

    /* Setup t_dim */
    t_dim.w = tx_w / 4; t_dim.h = tx_h / 4;
    t_dim.lw = 0; while ((1 << (t_dim.lw + 2)) < tx_w) t_dim.lw++;
    t_dim.lh = 0; while ((1 << (t_dim.lh + 2)) < tx_h) t_dim.lh++;
    t_dim.ctx = t_dim.lw + t_dim.lh;
    t_dim.min = t_dim.lw < t_dim.lh ? t_dim.lw : t_dim.lh;
    t_dim.max = t_dim.lw > t_dim.lh ? t_dim.lw : t_dim.lh;
    t_dim.sub = 0; t_dim.pad = 0;

    /* Decode skip flag */
    sctx = stb_get_skip_ctx(&t_dim, 0, a_ctx, l_ctx, plane ? 1 : 0);
    sk_cdf[0] = cdf->skip[sctx][0];
    sk_cdf[1] = 32768;
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
        int tx2dszctx = (t_dim.lw < 5 ? t_dim.lw : 4) + (t_dim.lh < 5 ? t_dim.lh : 4);
        int is_1d = 0; /* TX_CLASS_2D */
        unsigned short eob_cdf[16];
        int ns;
        
        if (tx2dszctx <= 4) {
            /* Use eob_bin_16 for small blocks */
            int bin_idx = tx2dszctx;
            eob_cdf[0] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][0];
            eob_cdf[1] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][1];
            eob_cdf[2] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][2];
            eob_cdf[3] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][3];
            eob_cdf[4] = cdf->eob_bin_16[plane ? 1 : 0][is_1d][4];
            eob_cdf[5] = 0;
            (void)bin_idx;
            ns = 4 + (tx2dszctx > 0 ? 1 : 0);
            eob = (int)stb_av1_msac_decode_symbol(msac, eob_cdf, (unsigned long)ns);
        } else {
            /* For larger blocks, use simplified EOB */
            eob = 0;
        }
    }

    if (eob > 1) {
        int eob_bin = eob - 2;
        unsigned short eob_hi_cdf[3];
        int eob_hi_bit;
        eob_hi_cdf[0] = cdf->eob_hi_bit[t_dim.ctx][plane ? 1 : 0][eob_bin];
        eob_hi_cdf[1] = 32768;
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
            /* Use base_eob_tok for position 0, base_tok for others */
            if (i == 0) {
                tok_cdf[0] = cdf->eob_base_tok[t_dim.ctx][plane ? 1 : 0][0];
                tok_cdf[1] = cdf->eob_base_tok[t_dim.ctx][plane ? 1 : 0][1];
                tok_cdf[2] = cdf->eob_base_tok[t_dim.ctx][plane ? 1 : 0][2];
                tok_cdf[3] = cdf->eob_base_tok[t_dim.ctx][plane ? 1 : 0][3];
                tok_cdf[4] = 0;
            } else {
                tok_cdf[0] = cdf->base_tok[t_dim.ctx][plane ? 1 : 0][0];
                tok_cdf[1] = cdf->base_tok[t_dim.ctx][plane ? 1 : 0][1];
                tok_cdf[2] = cdf->base_tok[t_dim.ctx][plane ? 1 : 0][2];
                tok_cdf[3] = cdf->base_tok[t_dim.ctx][plane ? 1 : 0][3];
                tok_cdf[4] = 0;
            }
            tok = stb_av1_msac_decode_symbol(msac, tok_cdf, 3);
            
            if (tok == 3) {
                /* Decode higher levels with br_tok */
                unsigned short br_cdf[6];
                br_cdf[0] = cdf->br_tok[t_dim.ctx < 3 ? t_dim.ctx : 3][plane ? 1 : 0][0];
                br_cdf[1] = cdf->br_tok[t_dim.ctx < 3 ? t_dim.ctx : 3][plane ? 1 : 0][1];
                br_cdf[2] = cdf->br_tok[t_dim.ctx < 3 ? t_dim.ctx : 3][plane ? 1 : 0][2];
                br_cdf[3] = cdf->br_tok[t_dim.ctx < 3 ? t_dim.ctx : 3][plane ? 1 : 0][3];
                br_cdf[4] = cdf->br_tok[t_dim.ctx < 3 ? t_dim.ctx : 3][plane ? 1 : 0][4];
                br_cdf[5] = 0;
                {
                    unsigned hi = stb_av1_msac_decode_hi_tok(msac, br_cdf);
                    if (i == 0)
                        coeffs[i] = (int)hi;
                    else
                        coeffs[i] = (int)hi;
                }
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
    (void)tx_sz;
    return eob;
}
