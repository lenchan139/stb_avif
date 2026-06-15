/*
 * dav1d_ref/getbits_c89.h
 *
 * Minimal C89-compatible GetBits (raw bit reader) implementation.
 * Ports dav1d's src/getbits.h to C89.
 *
 * Include this file, then call stb_av1_init_get_bits() to start reading.
 */

#ifndef STB_AV1_GETBITS_C89_H
#define STB_AV1_GETBITS_C89_H

#include <stddef.h>  /* for ptrdiff_t */

/* C89-compatible bit reader for OBU parsing (not MSAC/arithmetic coding) */

struct StbAv1GetBits {
    const unsigned char *ptr;       /* current byte pointer */
    const unsigned char *ptr_end;   /* end of buffer */
    const unsigned char *ptr_start; /* start of buffer */
    unsigned int state;             /* accumulated bits (up to 31 bits) */
    int bits_left;                  /* valid bits in state (0-31) */
    int error;                      /* 1 if error/overrun */
};

/* Initialize GetBits from a buffer */
static void stb_av1_init_get_bits(struct StbAv1GetBits *gb,
                                  const unsigned char *data,
                                  unsigned int size)
{
    gb->ptr_start = data;
    gb->ptr = data;
    gb->ptr_end = data + size;
    gb->state = 0;
    gb->bits_left = 0;
    gb->error = 0;
}

/* Refill the state register from the byte stream */
static void stb_av1_get_bits_refill(struct StbAv1GetBits *gb)
{
    while (gb->bits_left <= 24 && gb->ptr < gb->ptr_end) {
        gb->state = (gb->state << 8) | (unsigned int)(*gb->ptr);
        gb->ptr++;
        gb->bits_left += 8;
    }
}

/* Read a single bit */
static int stb_av1_get_bit(struct StbAv1GetBits *gb)
{
    if (gb->error) return 0;
    if (gb->bits_left == 0) stb_av1_get_bits_refill(gb);
    if (gb->bits_left == 0) { gb->error = 1; return 0; }
    gb->bits_left--;
    return (int)((gb->state >> gb->bits_left) & 1U);
}

/* Read n bits (n must be <= 16) */
static unsigned int stb_av1_get_bits(struct StbAv1GetBits *gb, int n)
{
    unsigned int val = 0;
    int i;
    if (gb->error) return 0;
    for (i = 0; i < n; i++) {
        val = (val << 1) | (unsigned int)stb_av1_get_bit(gb);
    }
    return val;
}

/* Read signed bits (n must be <= 16) */
static int stb_av1_get_sbits(struct StbAv1GetBits *gb, int n)
{
    unsigned int u = stb_av1_get_bits(gb, n);
    /* sign-extend: if MSB is 1, fill upper bits with 1s */
    if (u >= ((unsigned int)1 << (n - 1)))
        u |= ~(((unsigned int)1 << n) - 1U);
    return (int)u;
}

/* Read ULEB128 variable-length integer */
static unsigned int stb_av1_get_uleb128(struct StbAv1GetBits *gb)
{
    unsigned int val = 0;
    int i;
    if (gb->error) return 0;
    for (i = 0; i < 8; i++) {
        int b;
        if (gb->bits_left == 0) stb_av1_get_bits_refill(gb);
        if (gb->ptr > gb->ptr_end) { gb->error = 1; break; }
        /* read one byte */
        b = stb_av1_get_bits(gb, 8);
        val |= (b & 0x7fU) << (i * 7);
        if (!(b & 0x80)) break;
    }
    return val;
}

/* Get current bit position (for debugging) */
static unsigned int stb_av1_get_bits_pos(struct StbAv1GetBits *gb)
{
    ptrdiff_t byte_pos = (ptrdiff_t)(gb->ptr - gb->ptr_start);
    return (unsigned int)(byte_pos * 8 - gb->bits_left);
}

/* Byte-align the bit reader (skip to next byte boundary) */
static void stb_av1_bytealign_get_bits(struct StbAv1GetBits *gb)
{
    int bits_in_current_byte = gb->bits_left % 8;
    if (bits_in_current_byte == 0) return;
    gb->bits_left -= bits_in_current_byte;
}

/* Check trailing bits are valid */
static int stb_av1_check_trailing_bits(struct StbAv1GetBits *gb,
                                        int strict_std_compliance)
{
    int trailing_one_bit;
    if (gb->error) return -1;
    trailing_one_bit = stb_av1_get_bit(gb);
    if (!strict_std_compliance) return 0;
    if (!trailing_one_bit || gb->state) return -1;
    /* Check remaining bytes are all zero */
    while (gb->ptr < gb->ptr_end) {
        if (gb->ptr[0] != 0) return -1;
        gb->ptr++;
    }
    return 0;
}

#endif /* STB_AV1_GETBITS_C89_H */
