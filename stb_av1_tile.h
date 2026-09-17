/*
 * stb_av1_tile.h - AV1 tile-group bridge for the scalar decoder.
 *
 * The tile-group header handling follows dav1d 1.5.4 parse_tile_hdr().
 * This first bridge deliberately supports one tile only.  That is enough for
 * the common reduced-still AVIF case and, more importantly, gives the new
 * MSAC/CDF decoder the exact tile byte range instead of the legacy Boolean
 * reader's byte stream.
 */
#ifndef STB_AV1_TILE_H
#define STB_AV1_TILE_H

#include <stddef.h>

struct stb_av1_tile_span {
    const stbv_u8 *data;
    size_t size;
    unsigned int start;
    unsigned int end;
};

/* AV1 byte_alignment(): one 1 bit followed by zero bits until aligned. */
static int stb_av1_byte_align(struct stb_av1_getbits *gb)
{
    if (!gb)
        return -1;
    while (gb->bits_left)
        (void)stb_av1_get_bit(gb);
    return gb->error ? -1 : 0;
}

/* Parse the tile-group header and return the remaining bytes as the tile.
 * For a single tile, dav1d consumes no tile-group header bits at all. */
static int stb_av1_parse_tile_group(const struct stb_av1_framehdr *fh,
                                    struct stb_av1_getbits *gb,
                                    struct stb_av1_tile_span *tiles,
                                    unsigned int max_tiles,
                                    unsigned int *tile_count,
                                    unsigned int *tile_start,
                                    unsigned int *tile_end)
{
    unsigned int n_tiles, start, end, i, nbits, tile_size_bytes;
    const stbv_u8 *p, *pend;
    if (!fh || !gb || !tiles || !tile_count) return -1;
    n_tiles = fh->tiling.cols * fh->tiling.rows;
    if (!n_tiles || n_tiles > max_tiles) return -1;
    stb_av1_getbits_bytealign(gb);
    if (gb->error) { fprintf(stderr, "TG_FAIL: bytealign error\n"); return -1; }
    start = 0; end = n_tiles - 1;
    if (n_tiles > 1 && stb_av1_get_bit(gb)) {
        nbits = fh->tiling.log2_cols + fh->tiling.log2_rows;
        start = stb_av1_get_bits(gb, (int)nbits);
        end = stb_av1_get_bits(gb, (int)nbits);
        if (gb->error || start > end || end >= n_tiles) {
            fprintf(stderr, "TG_FAIL: tile header error gb_err=%d start=%u end=%u n_tiles=%u\n", gb->error, start, end, n_tiles);
            return -1;
        }
    }
    stb_av1_getbits_bytealign(gb);
    if (gb->error) { fprintf(stderr, "TG_FAIL: bytealign2 error\n"); return -1; }
    tile_size_bytes = fh->tiling.n_bytes;
    if (n_tiles > 1 && tile_size_bytes == 0) { fprintf(stderr, "TG_FAIL: n_bytes=0 multi-tile\n"); return -1; }
    p = gb->ptr; pend = gb->ptr_end; *tile_count = 0;
    for (i = start; i <= end; i++) {
        size_t sz; unsigned int k;
        if (i != end) {
            sz = 0;
            if ((size_t)(pend - p) < tile_size_bytes) {
                fprintf(stderr, "TG_FAIL: tile %u not enough bytes for size (%lu < %u)\n", i, (unsigned long)(pend - p), tile_size_bytes);
                return -1;
            }
            for (k = 0; k < tile_size_bytes; k++) sz |= (size_t)p[k] << (k * 8);
            sz += 1; p += tile_size_bytes;
            if (sz > (size_t)(pend - p)) {
                fprintf(stderr, "TG_FAIL: tile %u size %lu > remaining %lu\n", i, (unsigned long)sz, (unsigned long)(pend - p));
                return -1;
            }
        } else sz = (size_t)(pend - p);
        tiles[*tile_count].data = p;
        tiles[*tile_count].size = sz;
        tiles[*tile_count].start = i; tiles[*tile_count].end = i;
        (*tile_count)++; p += sz;
    }
    if (p != pend) {
        fprintf(stderr, "TG_FAIL: p!=pend (%d != %d)\n", (int)(p - gb->ptr_start), (int)(pend - gb->ptr_start));
        return -1;
    }
    if (tile_start) *tile_start = start;
    if (tile_end) *tile_end = end;
    return 0;
}

#endif
