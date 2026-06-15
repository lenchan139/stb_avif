/*
 * dav1d_ref/obu_c89.h
 *
 * C89-compatible OBU parsing for stb_avif.
 * Ports dav1d's src/obu.c to C89-compatible code.
 *
 * This provides stb_av1_parse_sequence_header() which reads AV1
 * sequence header OBUs using GetBits (raw bit reading, not MSAC).
 */

#ifndef STB_AV1_OBU_C89_H
#define STB_AV1_OBU_C89_H

#include "dav1d_ref/getbits_c89.h"

/* OBU types (AV1 spec section 5.3) */
enum StbAv1ObuType {
    STB_AV1_OBU_RESERVED_0       = 0,
    STB_AV1_OBU_RESERVED_1       = 1,
    STB_AV1_OBU_SEQUENCE_HEADER  = 1,
    STB_AV1_OBU_FRAME_HEADER     = 3,
    STB_AV1_OBU_TILE_GROUP       = 4,
    STB_AV1_OBU_METADATA         = 5,
    STB_AV1_OBU_FRAME            = 6,
    STB_AV1_OBU_REDUNDANT_FRAME_HEADER = 7,
    STB_AV1_OBU_TILE_LIST        = 8,
    STB_AV1_OBU_END_SEQUENCE     = 9,
    STB_AV1_OBU_PADDING          = 15,
};

/*
 * stb_av1_parse_sequence_header - parse AV1 sequence header OBU from raw bytes.
 *
 * This function reads an AV1 sequence header OBU using GetBits (raw bit reading)
 * and fills in the stb_av1_sequence_header struct from stb_avif.h.
 *
 * Returns 0 on success, -1 on error.
 */
int stb_av1_parse_sequence_header(void *seq_hdr_out,
                                   const unsigned char *data,
                                   unsigned int size);

#endif /* STB_AV1_OBU_C89_H */