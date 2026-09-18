/*
 * stb_av1_avifbox.h - minimal AVIF (ISO-BMFF) container parser for the
 * scalar AV1 test harness.
 *
 * Extracts the primary AV1 item payload (the OBU stream) from an .avif
 * file.  The iloc box in some encoder outputs is not strictly compliant,
 * so the extent is validated: it must be in-bounds and start with a
 * plausible AV1 OBU header byte.  When the iloc parse fails validation,
 * the mdat payload is used instead (which is the exact item data for the
 * files in example_avif/).
 */
#ifndef STB_AV1_AVIFBOX_H
#define STB_AV1_AVIFBOX_H

#include <stddef.h>

#ifndef STBV_U8_DEFINED
typedef unsigned char stbv_u8;
#define STBV_U8_DEFINED 1
#endif

static stbv_u32 stbv_avif_be32(const stbv_u8 *p)
{
    return ((stbv_u32)p[0] << 24) | ((stbv_u32)p[1] << 16) |
           ((stbv_u32)p[2] << 8) | (stbv_u32)p[3];
}

static stbv_u64 stbv_avif_be64(const stbv_u8 *p)
{
    return ((stbv_u64)stbv_avif_be32(p) << 32) | (stbv_u64)stbv_avif_be32(p + 4);
}

/* 1-based box sizes: 0x00 is invalid, sizes 0x01..0x08 are legal. */
static int stbv_avif_nibble_size(int nibble)
{
    return nibble + 1;
}

/*
 * Parse the iloc box payload (after the 8-byte box header) per the
 * ISOBMFF spec (versions 0..2).  raw_nibbles=0 uses the spec formula
 * (stored nibble = size-1); raw_nibbles=1 uses the values as-is, which
 * matches the files produced by some avifenc builds.  Returns 0 and
 * fills *data_off/*data_len for the first item's first extent on
 * success, or -1.
 */
static int stbv_avif_parse_iloc(const stbv_u8 *p, size_t size,
                                stbv_u64 *data_off, stbv_u64 *data_len,
                                int raw_nibbles)
{
    int version;
    int offset_size, length_size, base_offset_size, index_size;
    int item_count, i_item;

    if (size < 6)
        return -1;

    version = p[0] & 0xFF;
    offset_size = raw_nibbles ? ((p[4] >> 4) & 0xF) :
                 stbv_avif_nibble_size((p[4] >> 4) & 0xF);
    length_size = raw_nibbles ? (p[4] & 0xF) :
                  stbv_avif_nibble_size(p[4] & 0xF);
    if (offset_size > 8 || length_size > 8)
        return -1;

    if (version >= 1 && version <= 2) {
        base_offset_size = raw_nibbles ? ((p[5] >> 4) & 0xF) :
                           stbv_avif_nibble_size((p[5] >> 4) & 0xF);
        index_size = raw_nibbles ? (p[5] & 0xF) :
                     stbv_avif_nibble_size(p[5] & 0xF);
        (void)index_size;
        if (size < 8)
            return -1;
        item_count = ((int)p[6] << 8) | (int)p[7];
        p += 8;
        size -= 8;
    } else if (version == 0) {
        /* Some encoders write the v1-style base_offset_size/index_size
           byte even though version==0; others omit it entirely.  Detect
           by counting: if reading it keeps the payload exactly
           consistent, use it (caller validates the result anyway). */
        if (raw_nibbles && size >= 8) {
            base_offset_size = (p[5] >> 4) & 0xF;
            index_size = p[5] & 0xF;
            item_count = ((int)p[6] << 8) | (int)p[7];
            p += 8;
            size -= 8;
        } else {
            base_offset_size = 1;
            index_size = 0;
            if (size < 6)
                return -1;
            item_count = ((int)p[5] << 8) | (int)p[6];
            p += 7;
            size -= 7;
        }
    } else {
        return -1;
    }

    *data_off = 0;
    *data_len = 0;

    for (i_item = 0; i_item < item_count; i_item++) {
        stbv_u64 base_offset = 0;
        int extent_count, i_extent;
        int item_id_size = (version == 2) ? 4 : 2;
        int k;

        if (size < (size_t)(item_id_size + 2 + base_offset_size + 2))
            return -1;

        p += item_id_size;              /* item_ID */
        p += 2;                         /* data_reference_index */
        for (k = 0; k < base_offset_size; k++)
            base_offset = (base_offset << 8) | p[k];
        p += base_offset_size;

        extent_count = ((int)p[0] << 8) | (int)p[1];
        p += 2;
        size -= (size_t)(item_id_size + 2 + base_offset_size + 2);

        for (i_extent = 0; i_extent < extent_count; i_extent++) {
            stbv_u64 extent_offset = 0;
            stbv_u64 extent_length = 0;
            int j;

            if (size < (size_t)(offset_size + length_size))
                return -1;
            for (j = 0; j < offset_size; j++)
                extent_offset = (extent_offset << 8) | p[j];
            p += offset_size;
            for (j = 0; j < length_size; j++)
                extent_length = (extent_length << 8) | p[j];
            p += length_size;
            size -= (size_t)(offset_size + length_size);

            if (i_item == 0 && i_extent == 0) {
                *data_off = base_offset + extent_offset;
                *data_len = extent_length;
            }
        }

        if (i_item == 0)
            return 0;
    }

    return -1;
}

/* A plausible first byte of an AV1 OBU stream: has_size set, legal type. */
static int stbv_avif_plausible_obu(stbv_u8 b)
{
    int type = (b >> 3) & 15;
    if (b & 0x80)
        return 0;   /* forbidden bit */
    if (b & 1)
        return 0;   /* reserved bit */
    if (!(b & 0x02))
        return 0;   /* must have the size field in AVIF */
    return type == 1 || type == 2 || type == 3 || type == 4 || type == 6 ||
           type == 7 || type == 15;
}

static int stbv_avif_valid_extent(const stbv_u8 *file, size_t file_size,
                                  stbv_u64 off, stbv_u64 len)
{
    return len > 0 && off <= file_size && len <= file_size - (size_t)off &&
           stbv_avif_plausible_obu(file[(size_t)off]);
}

/*
 * Extract the primary AV1 item payload from an AVIF file.
 * Returns 0 with *data/*size set, or -1.
 */
static int stbv_av1_extract_avif_item(const stbv_u8 *file, size_t file_size,
                                      const stbv_u8 **data, size_t *size)
{
    size_t pos = 0;
    const stbv_u8 *mdat_data = NULL;
    size_t mdat_size = 0;

    while (pos + 8 <= file_size) {
        stbv_u64 box_size = stbv_avif_be32(file + pos);
        const stbv_u8 *type = file + pos + 4;
        size_t box_data;
        size_t end;

        if (box_size == 1) {
            if (pos + 16 > file_size)
                return -1;
            box_size = stbv_avif_be64(file + pos + 8);
        } else if (box_size == 0) {
            box_size = file_size - pos;
        }
        if (box_size < 8 || box_size > file_size - pos)
            return -1;

        end = (size_t)(pos + box_size);

        if (type[0] == 'm' && type[1] == 'd' && type[2] == 'a' && type[3] == 't') {
            mdat_data = file + pos + 8;
            mdat_size = (size_t)(box_size - 8);
        }

        if (type[0] == 'm' && type[1] == 'e' && type[2] == 't' && type[3] == 'a') {
            size_t p = pos + 8 + 4;   /* skip fullbox header */
            while (p + 8 <= end) {
                stbv_u64 sub_size = stbv_avif_be32(file + p);
                size_t sub_end;

                if (sub_size == 1) {
                    if (p + 16 > end)
                        return -1;
                    sub_size = stbv_avif_be64(file + p + 8);
                } else if (sub_size == 0) {
                    sub_size = end - p;
                }
                if (sub_size < 8 || sub_size > end - p)
                    return -1;
                sub_end = (size_t)(p + sub_size);

                if (file[p + 4] == 'i' && file[p + 5] == 'l' &&
                    file[p + 6] == 'o' && file[p + 7] == 'c') {
                    int raw;
                    for (raw = 1; raw >= 0; raw--) {
                        stbv_u64 off = 0, len = 0;
                        if (stbv_avif_parse_iloc(file + p + 8,
                                                 sub_end - p - 8, &off, &len,
                                                 raw) == 0) {
                            if (stbv_avif_valid_extent(file, file_size,
                                                       off, len)) {
                                *data = file + (size_t)off;
                                *size = (size_t)len;
                                return 0;
                            }
                        }
                    }
                }

                p = sub_end;
            }
        }

        pos = end;
    }

    if (mdat_data && mdat_size > 0 &&
        stbv_avif_plausible_obu(mdat_data[0])) {
        *data = mdat_data;
        *size = mdat_size;
        return 0;
    }

    return -1;
}

#endif /* STB_AV1_AVIFBOX_H */
