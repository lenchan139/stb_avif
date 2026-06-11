/*
 * C89 port: Full CDF context struct matching INIT ORDER from dav1d's cdf.c
 * Fields are in the order that the default_cdf initialization uses.
 * Coefficient CDFs are placed after mode CDFs to match the flat data array.
 */

#ifndef STB_AV1_CDF_H
#define STB_AV1_CDF_H

/* Coefficient CDF sub-struct (matches dav1d's CdfCoefContext) */
struct StbCdfCoef {
    unsigned short eob_bin_16[2][2][8];
    unsigned short eob_bin_32[2][2][8];
    unsigned short eob_bin_64[2][2][8];
    unsigned short eob_bin_128[2][2][8];
    unsigned short eob_bin_256[2][2][16];
    unsigned short eob_bin_512[2][16];
    unsigned short eob_bin_1024[2][16];
    unsigned short eob_base_tok[4][2][4][4];
    unsigned short base_tok[4][2][41][4];
    unsigned short br_tok[4][2][21][4];
    unsigned short eob_hi_bit[4][2][9][2];
    unsigned short skip[4][13][2];
    unsigned short dc_sign[2][3][2];
};

/* Full CDF context - fields in INIT ORDER to match generated flat data */
struct StbCdfContext {
    /* CdfModeContext (mode CDFs) - initialized first in default_cdf */
    unsigned short uv_mode[2][13][15];
    unsigned short partition[5][4][16];
    unsigned short cfl_alpha[6][16];
    unsigned short txtp_inter1[2][16];
    unsigned short txtp_inter2[16];
    unsigned short txtp_intra1[2][13][8];
    unsigned short txtp_intra2[3][13][8];
    unsigned short cfl_sign[8];
    unsigned short angle_delta[8][8];
    unsigned short filter_intra[8];
    unsigned short seg_id[3][8];
    unsigned short pal_sz[2][7][8];
    unsigned short color_map[2][7][5][8];
    unsigned short txsz[3][3][4];
    unsigned short delta_q[4];
    unsigned short delta_lf[5][4];
    unsigned short restore_switchable[4];
    unsigned short restore_wiener[2];
    unsigned short restore_sgrproj[2];
    unsigned short txtp_inter3[4][2];
    unsigned short use_filter_intra[22][2];
    unsigned short txpart[7][3][2];
    unsigned short skip[3][2];           /* mode skip, not coef skip */
    unsigned short pal_y[7][3][2];
    unsigned short pal_uv[2][2];
    unsigned short intrabc[2];
    unsigned short y_mode[4][16];
    unsigned short wedge_idx[9][16];
    unsigned short comp_inter_mode[8][8];
    unsigned short filter[2][8][8];
    unsigned short interintra_mode[4][4];
    unsigned short motion_mode[22][4];
    unsigned short skip_mode[3][2];
    unsigned short newmv_mode[6][2];
    unsigned short globalmv_mode[2][2];
    unsigned short refmv_mode[6][2];
    unsigned short drl_bit[3][2];
    unsigned short intra[4][2];
    unsigned short comp[5][2];
    unsigned short comp_dir[5][2];
    unsigned short jnt_comp[6][2];
    unsigned short mask_comp[6][2];
    unsigned short wedge_comp[9][2];
    unsigned short ref[6][3][2];
    unsigned short comp_fwd_ref[3][3][2];
    unsigned short comp_bwd_ref[2][3][2];
    unsigned short comp_uni_ref[3][3][2];
    unsigned short seg_pred[3][2];
    unsigned short interintra[7][2];
    unsigned short interintra_wedge[7][2];
    unsigned short obmc[22][2];

    /* CdfMvContext - initialized second */
    unsigned short mv_classes[16];
    unsigned short mv_sign[2];
    unsigned short mv_class0[2];
    unsigned short mv_class0_fp[2][4];
    unsigned short mv_class0_hp[2];
    unsigned short mv_classN[10][2];
    unsigned short mv_classN_fp[4];
    unsigned short mv_classN_hp[2];
    unsigned short mv_joint[4];

    /* kfym - initialized third */
    unsigned short kfym[5][5][16];

    /* Coefficient CDFs (CdfCoefContext) - NOT in default_cdf static init,
       but included here for completeness. Set separately. */
    struct StbCdfCoef coef;
};

#endif /* STB_AV1_CDF_H */
