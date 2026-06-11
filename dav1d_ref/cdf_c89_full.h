/*
 * C89 port: Full CDF context struct matching dav1d's CdfContext layout.
 * NO alignment padding (ALIGN removed) to allow flat array initialization.
 */

#ifndef STB_AV1_CDF_H
#define STB_AV1_CDF_H

/* Full CDF context struct - mirrors dav1d's CdfContext/CdfDefaultContext */
struct StbCdfContext {
    /* CdfModeContext fields (in exact order) */
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
    unsigned short skip[3][2];
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
    /* CdfMvComponent */
    unsigned short mv_comp[2][9][3];
    unsigned short mv_joint[4];
    /* kfym */
    unsigned short kfym[5][5][16];
};

#endif /* STB_AV1_CDF_H */
