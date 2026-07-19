/*
 * C99 tool: Dump dav1d's default CDF context as a flat uint16_t array
 * in the memory layout of StbCdfContext.
 *
 * Compile: cc -std=c99 -I dav1d_ref -o dump_cdf dump_cdf.c
 * Run: ./dump_cdf > cdf_default_correct.inc
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Include dav1d's CDF headers for types and values */
#include "dav1d.h"
#include "cdf.h"

/* Our StbCdfContext struct dimensions (must match stb_avif.h) */
typedef struct {
    uint16_t uv_mode[2][13][15];
    uint16_t partition[5][4][16];
    uint16_t cfl_alpha[6][16];
    uint16_t txtp_inter1[2][16];
    uint16_t txtp_inter2[16];
    uint16_t txtp_intra1[2][13][8];
    uint16_t txtp_intra2[3][13][8];
    uint16_t cfl_sign[8];
    uint16_t angle_delta[8][8];
    uint16_t filter_intra[8];
    uint16_t seg_id[3][8];
    uint16_t pal_sz[2][7][8];
    uint16_t color_map[2][7][5][8];
    uint16_t txsz[3][3][4];
    uint16_t delta_q[4];
    uint16_t delta_lf[5][4];
    uint16_t restore_switchable[4];
    uint16_t restore_wiener[2];
    uint16_t restore_sgrproj[2];
    uint16_t txtp_inter3[4][2];
    uint16_t use_filter_intra[22][2];
    uint16_t txpart[7][3][2];
    uint16_t skip[3][2];
    uint16_t pal_y[7][3][2];
    uint16_t pal_uv[2][2];
    uint16_t intrabc[2];
    uint16_t y_mode[4][16];
    uint16_t wedge_idx[9][16];
    uint16_t comp_inter_mode[8][8];
    uint16_t filter[2][8][8];
    uint16_t interintra_mode[4][4];
    uint16_t motion_mode[22][4];
    uint16_t skip_mode[3][2];
    uint16_t newmv_mode[6][2];
    uint16_t globalmv_mode[2][2];
    uint16_t refmv_mode[6][2];
    uint16_t drl_bit[3][2];
    uint16_t intra[4][2];
    uint16_t comp[5][2];
    uint16_t comp_dir[5][2];
    uint16_t jnt_comp[6][2];
    uint16_t mask_comp[6][2];
    uint16_t wedge_comp[9][2];
    uint16_t ref[6][3][2];
    uint16_t comp_fwd_ref[3][3][2];
    uint16_t comp_bwd_ref[2][3][2];
    uint16_t comp_uni_ref[3][3][2];
    uint16_t seg_pred[3][2];
    uint16_t interintra[7][2];
    uint16_t interintra_wedge[7][2];
    uint16_t obmc[22][2];
    /* CdfMvContext */
    uint16_t mv_classes[16];
    uint16_t mv_sign[2];
    uint16_t mv_class0[2];
    uint16_t mv_class0_fp[2][4];
    uint16_t mv_class0_hp[2];
    uint16_t mv_classN[10][2];
    uint16_t mv_classN_fp[4];
    uint16_t mv_classN_hp[2];
    uint16_t mv_joint[4];
    /* kfym */
    uint16_t kfym[5][5][16];
} StbCdfContext;

int main(void) {
    StbCdfContext dst;
    const uint16_t *src;
    int i;
    size_t count;

    /* Initialize dst from dav1d's default_cdf */
    memset(&dst, 0, sizeof(dst));

    /* Copy uv_mode: dav1d has [2][13][16], ours has [2][13][15] */
    for (i = 0; i < 2; i++) {
        int j;
        for (j = 0; j < 13; j++) {
            memcpy(dst.uv_mode[i][j], default_cdf.m.uv_mode[i][j], 15 * sizeof(uint16_t));
        }
    }

    /* Copy partition */
    memcpy(dst.partition, default_cdf.m.partition, sizeof(dst.partition));

    /* Copy cfl_alpha */
    memcpy(dst.cfl_alpha, default_cdf.m.cfl_alpha, sizeof(dst.cfl_alpha));

    /* Copy txtp_inter1 */
    memcpy(dst.txtp_inter1, default_cdf.m.txtp_inter1, sizeof(dst.txtp_inter1));

    /* Copy txtp_inter2 */
    memcpy(dst.txtp_inter2, default_cdf.m.txtp_inter2, sizeof(dst.txtp_inter2));

    /* Copy txtp_intra1 */
    memcpy(dst.txtp_intra1, default_cdf.m.txtp_intra1, sizeof(dst.txtp_intra1));

    /* Copy txtp_intra2 */
    memcpy(dst.txtp_intra2, default_cdf.m.txtp_intra2, sizeof(dst.txtp_intra2));

    /* Copy cfl_sign */
    memcpy(dst.cfl_sign, default_cdf.m.cfl_sign, sizeof(dst.cfl_sign));

    /* Copy angle_delta */
    memcpy(dst.angle_delta, default_cdf.m.angle_delta, sizeof(dst.angle_delta));

    /* Copy filter_intra */
    memcpy(dst.filter_intra, default_cdf.m.filter_intra, sizeof(dst.filter_intra));

    /* Copy seg_id */
    memcpy(dst.seg_id, default_cdf.m.seg_id, sizeof(dst.seg_id));

    /* Copy pal_sz */
    memcpy(dst.pal_sz, default_cdf.m.pal_sz, sizeof(dst.pal_sz));

    /* Copy color_map */
    memcpy(dst.color_map, default_cdf.m.color_map, sizeof(dst.color_map));

    /* Copy txsz */
    memcpy(dst.txsz, default_cdf.m.txsz, sizeof(dst.txsz));

    /* Copy delta_q */
    memcpy(dst.delta_q, default_cdf.m.delta_q, sizeof(dst.delta_q));

    /* Copy delta_lf */
    memcpy(dst.delta_lf, default_cdf.m.delta_lf, sizeof(dst.delta_lf));

    /* Copy restore_switchable */
    memcpy(dst.restore_switchable, default_cdf.m.restore_switchable, sizeof(dst.restore_switchable));

    /* Copy restore_wiener */
    memcpy(dst.restore_wiener, default_cdf.m.restore_wiener, sizeof(dst.restore_wiener));

    /* Copy restore_sgrproj */
    memcpy(dst.restore_sgrproj, default_cdf.m.restore_sgrproj, sizeof(dst.restore_sgrproj));

    /* Copy txtp_inter3 */
    memcpy(dst.txtp_inter3, default_cdf.m.txtp_inter3, sizeof(dst.txtp_inter3));

    /* Copy use_filter_intra */
    memcpy(dst.use_filter_intra, default_cdf.m.use_filter_intra, sizeof(dst.use_filter_intra));

    /* Copy txpart */
    memcpy(dst.txpart, default_cdf.m.txpart, sizeof(dst.txpart));

    /* Copy skip */
    memcpy(dst.skip, default_cdf.m.skip, sizeof(dst.skip));

    /* Copy pal_y */
    memcpy(dst.pal_y, default_cdf.m.pal_y, sizeof(dst.pal_y));

    /* Copy pal_uv */
    memcpy(dst.pal_uv, default_cdf.m.pal_uv, sizeof(dst.pal_uv));

    /* Copy intrabc */
    memcpy(dst.intrabc, default_cdf.m.intrabc, sizeof(dst.intrabc));

    /* Copy y_mode */
    memcpy(dst.y_mode, default_cdf.m.y_mode, sizeof(dst.y_mode));

    /* Copy wedge_idx */
    memcpy(dst.wedge_idx, default_cdf.m.wedge_idx, sizeof(dst.wedge_idx));

    /* Copy comp_inter_mode */
    memcpy(dst.comp_inter_mode, default_cdf.m.comp_inter_mode, sizeof(dst.comp_inter_mode));

    /* Copy filter */
    memcpy(dst.filter, default_cdf.m.filter, sizeof(dst.filter));

    /* Copy interintra_mode */
    memcpy(dst.interintra_mode, default_cdf.m.interintra_mode, sizeof(dst.interintra_mode));

    /* Copy motion_mode */
    memcpy(dst.motion_mode, default_cdf.m.motion_mode, sizeof(dst.motion_mode));

    /* Copy skip_mode */
    memcpy(dst.skip_mode, default_cdf.m.skip_mode, sizeof(dst.skip_mode));

    /* Copy newmv_mode */
    memcpy(dst.newmv_mode, default_cdf.m.newmv_mode, sizeof(dst.newmv_mode));

    /* Copy globalmv_mode */
    memcpy(dst.globalmv_mode, default_cdf.m.globalmv_mode, sizeof(dst.globalmv_mode));

    /* Copy refmv_mode */
    memcpy(dst.refmv_mode, default_cdf.m.refmv_mode, sizeof(dst.refmv_mode));

    /* Copy drl_bit */
    memcpy(dst.drl_bit, default_cdf.m.drl_bit, sizeof(dst.drl_bit));

    /* Copy intra */
    memcpy(dst.intra, default_cdf.m.intra, sizeof(dst.intra));

    /* Copy comp */
    memcpy(dst.comp, default_cdf.m.comp, sizeof(dst.comp));

    /* Copy comp_dir */
    memcpy(dst.comp_dir, default_cdf.m.comp_dir, sizeof(dst.comp_dir));

    /* Copy jnt_comp */
    memcpy(dst.jnt_comp, default_cdf.m.jnt_comp, sizeof(dst.jnt_comp));

    /* Copy mask_comp */
    memcpy(dst.mask_comp, default_cdf.m.mask_comp, sizeof(dst.mask_comp));

    /* Copy wedge_comp */
    memcpy(dst.wedge_comp, default_cdf.m.wedge_comp, sizeof(dst.wedge_comp));

    /* Copy ref */
    memcpy(dst.ref, default_cdf.m.ref, sizeof(dst.ref));

    /* Copy comp_fwd_ref */
    memcpy(dst.comp_fwd_ref, default_cdf.m.comp_fwd_ref, sizeof(dst.comp_fwd_ref));

    /* Copy comp_bwd_ref */
    memcpy(dst.comp_bwd_ref, default_cdf.m.comp_bwd_ref, sizeof(dst.comp_bwd_ref));

    /* Copy comp_uni_ref */
    memcpy(dst.comp_uni_ref, default_cdf.m.comp_uni_ref, sizeof(dst.comp_uni_ref));

    /* Copy seg_pred */
    memcpy(dst.seg_pred, default_cdf.m.seg_pred, sizeof(dst.seg_pred));

    /* Copy interintra */
    memcpy(dst.interintra, default_cdf.m.interintra, sizeof(dst.interintra));

    /* Copy interintra_wedge */
    memcpy(dst.interintra_wedge, default_cdf.m.interintra_wedge, sizeof(dst.interintra_wedge));

    /* Copy obmc */
    memcpy(dst.obmc, default_cdf.m.obmc, sizeof(dst.obmc));

    /* Copy CdfMvComponent fields */
    src = (const uint16_t *)&default_cdf.mv.comp;
    memcpy(dst.mv_classes, default_cdf.mv.comp.classes, sizeof(dst.mv_classes));
    memcpy(dst.mv_sign, default_cdf.mv.comp.sign, sizeof(dst.mv_sign));
    memcpy(dst.mv_class0, default_cdf.mv.comp.class0, sizeof(dst.mv_class0));
    memcpy(dst.mv_class0_fp, default_cdf.mv.comp.class0_fp, sizeof(dst.mv_class0_fp));
    memcpy(dst.mv_class0_hp, default_cdf.mv.comp.class0_hp, sizeof(dst.mv_class0_hp));
    memcpy(dst.mv_classN, default_cdf.mv.comp.classN, sizeof(dst.mv_classN));
    memcpy(dst.mv_classN_fp, default_cdf.mv.comp.classN_fp, sizeof(dst.mv_classN_fp));
    memcpy(dst.mv_classN_hp, default_cdf.mv.comp.classN_hp, sizeof(dst.mv_classN_hp));
    memcpy(dst.mv_joint, default_cdf.mv.joint, sizeof(dst.mv_joint));

    /* Copy kfym */
    memcpy(dst.kfym, default_cdf.kfym, sizeof(dst.kfym));

    /* Dump the entire memory as a flat uint16_t array */
    src = (const uint16_t *)&dst;
    count = sizeof(dst) / sizeof(uint16_t);

    fprintf(stderr, "/* Total uint16_t entries: %zu */\n", count);
    fprintf(stderr, "/* sizeof(StbCdfContext) = %zu bytes */\n", sizeof(dst));

    printf("static const unsigned short stb_av1_cdf_default_data[] = {\n");
    for (i = 0; i < (int)count; i++) {
        if (i % 12 == 0) printf("    ");
        printf("%5u,", src[i]);
        if (i % 12 == 11 || i == (int)count - 1) printf("\n");
    }
    printf("};\n");

    /* Also verify some key values */
    fprintf(stderr, "uv_mode[0][0][0] = %u (expect 6554 = CDF1(22631))\n", dst.uv_mode[0][0][0]);
    fprintf(stderr, "y_mode[0][0] = %u (expect 9967 = CDF1(22801))\n", dst.y_mode[0][0]);
    fprintf(stderr, "partition[0][0][0] = %u (expect 4869 = CDF1(27899))\n", dst.partition[0][0][0]);
    fprintf(stderr, "skip[0][0] = %u (expect 1097 = CDF1(31671))\n", dst.skip[0][0]);
    fprintf(stderr, "kfym[0][0][0] = %u\n", dst.kfym[0][0][0]);

    return 0;
}
