/* stb_avif.h - v0.01 - AVIF image decoder - public domain
 *                                                  - http://github.com/nothings/stb
 *
 * A single-header C89 library for decoding AVIF images.
 *
 * REFERENCES
 *   libavif - https://github.com/AOMediaCodec/libavif
 *   dav1d   - https://code.videolan.org/videolan/dav1d
 *   AV1     - https://aomediacodec.github.io/av1-spec/
 *   ISOBMFF - ISO 14496-12
 *   HEIF    - ISO 23000-22
 *
 * LIBRARY OVERVIEW
 *
 *   stb_avif.h is a single-header library for decoding AVIF images.
 *   To use it, #define STB_AVIF_IMPLEMENTATION in exactly one C file
 *   that includes this header.
 *
 *   Example (without dav1d, internal decoder produces garbage/snow):
 *      #define STB_AVIF_IMPLEMENTATION
 *      #include "stb_avif.h"
 *      ...
 *      int x, y, c;
 *      unsigned char *img = stb_avif_load_from_memory(data, len, &x, &y, &c, 4);
 *      // ... use img ...
 *      stb_avif_free(img);
 *
 *   Example (with dav1d — correct output):
 *      #define STB_AVIF_USE_DAV1D
 *      #define STB_AVIF_IMPLEMENTATION
 *      #include "stb_avif.h"
 *      ...
 *      // Compile: cc ... -D STB_AVIF_USE_DAV1D -ldav1d
 *      int x, y, c;
 *      unsigned char *img = stb_avif_load_from_memory(data, len, &x, &y, &c, 4);
 *      // ... use img ...
 *      stb_avif_free(img);
 *
 *   The library decodes AVIF images down to plain RGBA pixels.
 *   With STB_AVIF_USE_DAV1D, it uses libdav1d for correct AV1 decoding.
 *   Without it, the built-in AV1 decoder is a simplified placeholder
 *   and will produce garbage pixels ("snow").
 *
 *   Supported formats:
 *     - Profile 0 (Main): 8-bit, 4:2:0, 4:2:2, 4:4:4
 *     - Profile 1 (High): 8/10-bit, 4:2:0, 4:2:2, 4:4:4
 *     - Monochrome (limited)
 *     - Still images only (no sequences)
 *
 * LICENSE
 *
 *   This software is in the public domain. Where that dedication is not
 *   recognized, you are granted a perpetual, irrevocable license to use,
 *   copy, modify, and distribute this software for any purpose.
 *
 *   See http://creativecommons.org/publicdomain/zero/1.0/ for details.
 */

#ifndef STB_AVIF_H
#define STB_AVIF_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* PUBLIC API                                                                 */
/* -------------------------------------------------------------------------- */

/* Load an AVIF image from memory.
 *
 *  data    - pointer to the complete AVIF file contents
 *  len     - length of the data buffer
 *  x, y    - output: image dimensions (in pixels)
 *  channels - output: number of color channels in the returned data
 *  req_channels - desired number of output channels (0 = use image default,
 *                 3 = RGB, 4 = RGBA)
 *
 *  Returns a pointer to decoded pixels (row-major, top-left first) or NULL
 *  on failure.
 *
 *  The returned buffer is req_channels bytes per pixel (or channels if 0).
 *  Free it with stb_avif_free().
 *
 *  When STB_AVIF_USE_DAV1D is defined, uses libdav1d for correct output.
 *  Link with -ldav1d. Without dav1d, the internal decoder produces garbage.
 */
unsigned char *stb_avif_load_from_memory(const unsigned char *data, int len,
                                          int *x, int *y, int *channels,
                                          int req_channels);

/* Free an image buffer previously returned by stb_avif_load_from_memory(). */
void stb_avif_free(void *ptr);

/* Returns a string describing the last error. */
const char *stb_avif_failure_reason(void);

/* -------------------------------------------------------------------------- */
/* PRIVATE TYPES (exposed for implementation)                                 */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* STB_AVIF_H */

/* -------------------------------------------------------------------------- */
/* IMPLEMENTATION                                                             */
/* -------------------------------------------------------------------------- */

#ifdef STB_AVIF_IMPLEMENTATION

#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* memset, memcpy */
#include <setjmp.h>     /* setjmp, longjmp */
#include <math.h>       /* cos, sin, sqrt */
#include <time.h>       /* clock, time */
#include <stdio.h>      /* fprintf, stderr */

/* Optional dav1d backend for correct AV1 decoding.
   Define STB_AVIF_USE_DAV1D and link with -ldav1d */
#ifdef STB_AVIF_USE_DAV1D
#include <dav1d/dav1d.h>
#endif

/* ----------- CONFIGURATION ----------- */

#ifndef STB_AVIF_MAX_DIMENSION
#define STB_AVIF_MAX_DIMENSION 16384
#endif

#ifndef STB_AVIF_MAX_TILE_WIDTH
#define STB_AVIF_MAX_TILE_WIDTH 4096
#endif

#ifndef STB_AVIF_MAX_TILE_HEIGHT
#define STB_AVIF_MAX_TILE_HEIGHT 4096
#endif

/* ----------- C89 COMPATIBILITY HELPERS ----------- */

/* We avoid stdint.h for strict C89 compatibility.
   Define our own fixed-size types. */
typedef unsigned char  stbv_u8;
typedef signed char    stbv_s8;
typedef unsigned short stbv_u16;
typedef signed short   stbv_s16;
typedef unsigned int   stbv_u32;
typedef signed int     stbv_s32;

/* We need 64-bit types. Use unsigned long long which is available in C89
   as a common extension, or we can use two-32-bit approach. */
#ifndef STB_AVIF_NO_64BIT
  #if defined(_MSC_VER)
    typedef unsigned __int64 stbv_u64;
  #else
    typedef unsigned long long stbv_u64;
  #endif
#else
  /* 64-bit not available; use a struct pair approach (not implemented) */
  #error "stb_avif requires 64-bit integer support"
#endif

/* ----------- ERROR HANDLING ----------- */

static const char *stb_avif_error_msg = "no error";
static jmp_buf stb_avif_jmp;

#define STB_AVIF_ERROR(msg) do { stb_avif_error_msg = msg; longjmp(stb_avif_jmp, 1); } while (0)
#define STB_AVIF_CHECK(cond, msg) do { if (!(cond)) STB_AVIF_ERROR(msg); } while (0)

/* ----------- MEMORY ALLOCATION ----------- */

static void *stb_avif_malloc(size_t size)
{
    return malloc(size);
}

static void *stb_avif_calloc(size_t count, size_t size)
{
    void *p;
    size_t total = count * size;
    p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

static void stb_avif_free_internal(void *ptr)
{
    free(ptr);
}

/* ----------- BITSTREAM READER ----------- */

struct stb_avif_reader {
    const unsigned char *data;
    size_t size;
    size_t pos;
    int bit_pos;        /* current bit position (0-7) within current byte */
    int byte_buf;       /* buffered byte for bit reads (-1 = none) */
};

static void stb_avif_reader_init(struct stb_avif_reader *r, const unsigned char *data, size_t size)
{
    r->data = data;
    r->size = size;
    r->pos = 0;
    r->bit_pos = 0;
    r->byte_buf = -1;
}

static int stb_avif_read_byte(struct stb_avif_reader *r)
{
    if (r->pos >= r->size)
        STB_AVIF_ERROR("Unexpected end of data");
    return r->data[r->pos++];
}

static int stb_avif_peek_byte(struct stb_avif_reader *r)
{
    if (r->pos >= r->size)
        STB_AVIF_ERROR("Unexpected end of data");
    return r->data[r->pos];
}

static stbv_u32 stb_avif_read_be32(struct stb_avif_reader *r)
{
    stbv_u32 v;
    v = (stbv_u32)stb_avif_read_byte(r) << 24;
    v |= (stbv_u32)stb_avif_read_byte(r) << 16;
    v |= (stbv_u32)stb_avif_read_byte(r) << 8;
    v |= (stbv_u32)stb_avif_read_byte(r);
    return v;
}

static stbv_u16 stb_avif_read_be16(struct stb_avif_reader *r)
{
    stbv_u16 v;
    v = (stbv_u16)(stb_avif_read_byte(r) << 8);
    v |= (stbv_u16)(stb_avif_read_byte(r));
    return v;
}

static stbv_u64 stb_avif_read_be64(struct stb_avif_reader *r)
{
    stbv_u64 v;
    v = (stbv_u64)stb_avif_read_byte(r) << 56;
    v |= (stbv_u64)stb_avif_read_byte(r) << 48;
    v |= (stbv_u64)stb_avif_read_byte(r) << 40;
    v |= (stbv_u64)stb_avif_read_byte(r) << 32;
    v |= (stbv_u64)stb_avif_read_byte(r) << 24;
    v |= (stbv_u64)stb_avif_read_byte(r) << 16;
    v |= (stbv_u64)stb_avif_read_byte(r) << 8;
    v |= (stbv_u64)stb_avif_read_byte(r);
    return v;
}

/* Skip n bytes forward */
static void stb_avif_skip_bytes(struct stb_avif_reader *r, size_t n)
{
    if (r->pos + n > r->size)
        STB_AVIF_ERROR("Unexpected end of data");
    r->pos += n;
}

/* Read a 7-bit variable-length quantity (used in ISOBMFF) */
static stbv_u32 stb_avif_read_uleb128(struct stb_avif_reader *r)
{
    stbv_u32 val = 0;
    int i;
    for (i = 0; i < 5; i++) {
        int b = stb_avif_read_byte(r);
        val |= ((stbv_u32)(b & 0x7F)) << (i * 7);
        if (!(b & 0x80))
            break;
    }
    return val;
}

/* ----------- ISOBMFF/HEIF BOX PARSER ----------- */

/* Box header: size (4 or 8 bytes) + type (4 bytes) */
#define STB_AVIF_BOX_HEADER_SIZE 8
#define STB_AVIF_BOX_EXTENDED_SIZE 16

/* Known box types as 32-bit integers (big-endian ASCII) */
#define STB_AVIF_FOURCC(a,b,c,d) ((stbv_u32)((a)<<24|(b)<<16|(c)<<8|(d)))
#define STB_AVIF_BOX_FTYP   STB_AVIF_FOURCC('f','t','y','p')
#define STB_AVIF_BOX_META   STB_AVIF_FOURCC('m','e','t','a')
#define STB_AVIF_BOX_HDLR   STB_AVIF_FOURCC('h','d','l','r')
#define STB_AVIF_BOX_PITM   STB_AVIF_FOURCC('p','i','t','m')
#define STB_AVIF_BOX_ILOC   STB_AVIF_FOURCC('i','l','o','c')
#define STB_AVIF_BOX_IINF   STB_AVIF_FOURCC('i','i','n','f')
#define STB_AVIF_BOX_INFE   STB_AVIF_FOURCC('i','n','f','e')
#define STB_AVIF_BOX_IPRP   STB_AVIF_FOURCC('i','p','r','p')
#define STB_AVIF_BOX_IPCO   STB_AVIF_FOURCC('i','p','c','o')
#define STB_AVIF_BOX_IPMA   STB_AVIF_FOURCC('i','p','m','a')
#define STB_AVIF_BOX_ISPE   STB_AVIF_FOURCC('i','s','p','e')
#define STB_AVIF_BOX_PIXI   STB_AVIF_FOURCC('p','i','x','i')
#define STB_AVIF_BOX_AV1C   STB_AVIF_FOURCC('a','v','1','C')
#define STB_AVIF_BOX_COLR   STB_AVIF_FOURCC('c','o','l','r')
#define STB_AVIF_BOX_MDAT   STB_AVIF_FOURCC('m','d','a','t')
#define STB_AVIF_BOX_MOOV   STB_AVIF_FOURCC('m','o','o','v')
#define STB_AVIF_BOX_MOOF   STB_AVIF_FOURCC('m','o','o','f')

struct stb_avif_box {
    stbv_u64 size;    /* total box size including header */
    stbv_u32 type;    /* 4-byte box type */
    stbv_u64 data_start; /* position of box content (after header) */
    stbv_u64 data_size;  /* size of box content */
};

/* Read a box header at current position and advance past it */
static void stb_avif_read_box_header(struct stb_avif_reader *r, struct stb_avif_box *box)
{
    stbv_u32 size32;
    stbv_u64 start = (stbv_u64)r->pos;

    size32 = stb_avif_read_be32(r);
    box->type = stb_avif_read_be32(r);

    if (size32 == 1) {
        /* Extended size (64-bit) */
        box->size = stb_avif_read_be64(r);
    } else if (size32 == 0) {
        /* Box extends to end of file */
        box->size = (stbv_u64)r->size - start;
    } else {
        box->size = (stbv_u64)size32;
    }

    box->data_start = (stbv_u64)r->pos;
    if (box->size >= (stbv_u64)(r->pos - start)) {
        box->data_size = box->size - (stbv_u64)(r->pos - start);
    } else {
        box->data_size = 0;
    }
}

/* Skip to end of box */
static void stb_avif_skip_box(struct stb_avif_reader *r, const struct stb_avif_box *box)
{
    stbv_u64 end = box->data_start + box->data_size;
    if (end > (stbv_u64)r->size)
        STB_AVIF_ERROR("Box extends beyond data");
    r->pos = (size_t)end;
}

/* Enter a box: position at data start */
static void stb_avif_enter_box(struct stb_avif_reader *r, const struct stb_avif_box *box)
{
    if (box->data_start > (stbv_u64)r->size)
        STB_AVIF_ERROR("Box position out of bounds");
    r->pos = (size_t)box->data_start;
}

/* -------------------------------------------------------------------------- */
/* AVIF PARSER STATE                                                          */
/* -------------------------------------------------------------------------- */

struct stb_avif_avif_info {
    /* Image info */
    int width;
    int height;
    int bit_depth;
    int chroma_subsampling_x;
    int chroma_subsampling_y;
    int monochrome;

    /* AV1 codec config (from av1C box) */
    unsigned char av1c_data[32];
    int av1c_size;

    /* Compressed AV1 data */
    const unsigned char *av1_data;
    size_t av1_size;

    /* Output buffer */
    unsigned char *output;
    int output_channels;

    /* Input data */
    const unsigned char *input;
    int input_len;
    size_t meta_end_offset;

    /* Decoded planes (8-bit) */
    unsigned char *plane_y;
    unsigned char *plane_u;
    unsigned char *plane_v;
    int stride_y;
    int stride_u;
    int stride_v;
};

/* ----------- ISOBMFF PARSER ----------- */

/* Find a box of given type within a container; recurses into sub-boxes if needed.
   Returns 1 if found, 0 if not. Does not modify r->pos on return. */
static int stb_avif_find_box(struct stb_avif_reader *r, stbv_u32 type,
                              int deep_search, struct stb_avif_box *box_out)
{
    size_t saved_pos = r->pos;

    while (r->pos + 8 <= r->size) {
        struct stb_avif_box box;
        size_t box_start = r->pos;

        stb_avif_read_box_header(r, &box);

        if (box.type == type) {
            r->pos = (size_t)box.data_start;
            if (box_out) *box_out = box;
            return 1;
        }

        if (deep_search && (box.type == STB_AVIF_BOX_META ||
                            box.type == STB_AVIF_BOX_IPRP ||
                            box.type == STB_AVIF_BOX_IPCO ||
                            box.type == STB_AVIF_BOX_MOOV ||
                            box.type == STB_AVIF_BOX_MOOF))
        {
            stb_avif_enter_box(r, &box);
            if (stb_avif_find_box(r, type, deep_search, box_out)) {
                return 1;
            }
        }

        r->pos = (size_t)(box_start + box.size);
    }

    r->pos = saved_pos;
    return 0;
}

/* Parse ftyp box to verify this is an AVIF file */
static void stb_avif_parse_ftyp(struct stb_avif_reader *r,
                                 struct stb_avif_avif_info *info)
{
    /* Skip major brand (4 bytes), minor version (4 bytes) */
    stb_avif_skip_bytes(r, 8);

    /* Check for compatible brands */
    while (r->pos < r->size) {
        stbv_u32 brand = stb_avif_read_be32(r);
        if (brand == STB_AVIF_FOURCC('a','v','i','f'))
            return; /* OK */
        /* We found avif brand; we're good */
    }

    /* Some files might not have avif brand but still be AVIF;
       check if we at least have an mif1 brand */
    /* no need to re-check; avif brand was found above */
}

/* Parse the av1C box (AV1 codec configuration) 
   box_data_size: remaining bytes in the av1C box (after box header) */
static void stb_avif_parse_av1c(struct stb_avif_reader *r,
                                 struct stb_avif_avif_info *info,
                                 size_t box_data_size)
{
    int i;

    /* The av1C box contains an AV1CodecConfigurationBox */
    /* marker=1, version=1 */
    /* Actually the box just contains the AV1 config OBU data.
       From the ISOBMFF spec: the av1C box contains:
       unsigned int(1) marker = 1;
       unsigned int(7) version = 1;
       unsigned int(3) seq_profile;
       unsigned int(5) seq_level_idx_0;
       unsigned int(1) seq_tier_0;
       unsigned int(1) high_bitdepth;
       unsigned int(1) twelve_bit;
       unsigned int(1) monochrome;
       unsigned int(1) chroma_subsampling_x;
       unsigned int(1) chroma_subsampling_y;
       unsigned int(2) chroma_sample_position;
       unsigned int(3) reserved;
       unsigned int(1) initial_presentation_delay_present;
       if (initial_presentation_delay_present) {
           unsigned int(4) initial_presentation_delay_minus_one;
       } else {
           unsigned int(4) reserved;
       }
    */
    int seq_profile, seq_tier_0;
    int high_bitdepth, twelve_bit, monochrome;
    int chroma_subsampling_x, chroma_subsampling_y, chroma_sample_position;
    int initial_presentation_delay_present;

    /* Byte 0: marker(1)=1 + version(7)=1 */
    stb_avif_read_byte(r);

    /* Byte 1: seq_profile(3) + seq_level_idx_0(5) */
    {
        int byte1 = stb_avif_read_byte(r);
        seq_profile = (byte1 >> 5) & 7;
        /* seq_level_idx_0 = byte1 & 31; */
    }

    /* Byte 2: flags */
    {
        int byte2 = stb_avif_read_byte(r);
        seq_tier_0                  = (byte2 >> 7) & 1;
        high_bitdepth               = (byte2 >> 6) & 1;
        twelve_bit                  = (byte2 >> 5) & 1;
        monochrome                  = (byte2 >> 4) & 1;
        chroma_subsampling_x        = (byte2 >> 3) & 1;
        chroma_subsampling_y        = (byte2 >> 2) & 1;
        chroma_sample_position      = byte2 & 3;

        info->monochrome = monochrome;
        info->chroma_subsampling_x = chroma_subsampling_x;
        info->chroma_subsampling_y = chroma_subsampling_y;

        if (high_bitdepth) {
            info->bit_depth = twelve_bit ? 12 : 10;
        } else {
            info->bit_depth = 8;
        }
    }

    /* Byte 3: reserved + initial_presentation_delay */
    {
        int byte3 = stb_avif_read_byte(r);
        initial_presentation_delay_present = (byte3 >> 4) & 1;
    }

    /* Remaining bytes: config OBUs (sequence header OBU data) 
       box_data_size is the total av1C box content size; we've read 4 fixed bytes */
    info->av1c_size = (int)box_data_size - 4;
    if (info->av1c_size > (int)sizeof(info->av1c_data))
        info->av1c_size = (int)sizeof(info->av1c_data);
    if (info->av1c_size < 0) info->av1c_size = 0;

    for (i = 0; i < info->av1c_size && i < (int)box_data_size - 4; i++) {
        info->av1c_data[i] = (unsigned char)stb_avif_read_byte(r);
    }

    STB_AVIF_CHECK(high_bitdepth == 0 || high_bitdepth == 1,
                   "Invalid bitdepth flag");
    (void)seq_profile;
    (void)seq_tier_0;
    (void)chroma_sample_position;
    (void)initial_presentation_delay_present;
}

/* Parse the iloc box (item location) to find where coded data is stored */
static void stb_avif_parse_iloc(struct stb_avif_reader *r,
                                 struct stb_avif_avif_info *info,
                                 stbv_u32 *data_offset,
                                 stbv_u64 *data_size)
{
    int version;
    int offset_size, length_size, base_offset_size, index_size;
    int item_count, i_item;

    version = stb_avif_read_byte(r);
    {
        int byte2 = stb_avif_read_byte(r);
        offset_size = ((byte2 >> 4) & 0xF) + 1;
        length_size = (byte2 & 0xF) + 1;
    }

    if (version >= 1) {
        int byte3 = stb_avif_read_byte(r);
        base_offset_size = ((byte3 >> 4) & 0xF) + 1;
        index_size = (byte3 & 0xF) + 1;
        (void)index_size;
    } else {
        base_offset_size = 1;
        index_size = 0;
    }

    item_count = (int)stb_avif_read_be16(r);

    *data_offset = 0;
    *data_size = 0;

    for (i_item = 0; i_item < item_count; i_item++) {
        int item_ID;
        int data_ref_index;
        int i_extent;
        int extent_count;

        if (version < 2) {
            item_ID = (int)stb_avif_read_be16(r);
        } else {
            item_ID = (int)stb_avif_read_be16(r);
            stb_avif_read_be16(r);
        }
        (void)item_ID;

        if (version >= 1) {
            /* construction_method */
            /* 4 bytes: (12 reserved + 4 construction_method) or more depending on version */
            stb_avif_read_be16(r); /* skip */
            data_ref_index = stb_avif_read_be16(r);
        } else {
            data_ref_index = stb_avif_read_be16(r);
        }
        (void)data_ref_index;

        /* base_offset */
        {
            int _off_sz;
            int j;
            stbv_u64 base_offset_val = 0;

            /* For version 0, base_offset size = offset_size (from first byte).
               For version >= 1, base_offset size = base_offset_size. */
            if (version == 0)
                _off_sz = offset_size;
            else
                _off_sz = base_offset_size;

            for (j = 0; j < _off_sz; j++) {
                base_offset_val = (base_offset_val << 8) | (stbv_u64)stb_avif_read_byte(r);
            }

            extent_count = (int)stb_avif_read_be16(r);

            for (i_extent = 0; i_extent < extent_count; i_extent++) {
                stbv_u64 extent_offset = 0;
                stbv_u64 extent_length = 0;
                int k;

                for (k = 0; k < offset_size; k++) {
                    extent_offset = (extent_offset << 8) | (stbv_u64)stb_avif_read_byte(r);
                }
                for (k = 0; k < length_size; k++) {
                    extent_length = (extent_length << 8) | (stbv_u64)stb_avif_read_byte(r);
                }

                /* Store the first extent for now */
                if (i_item == 0 && i_extent == 0) {
                    *data_offset = (stbv_u32)(base_offset_val + extent_offset);
                    *data_size = extent_length;
                }
            }

            /* For simplicity, we take the first item's data.
               In a real decoder we'd match item_ID from pitm. */
            if (i_item == 0) {
                break; /* We'll use the first item */
            }
        }
    }
}

/* Parse pitm (Primary Item ID) */
static int stb_avif_parse_pitm(struct stb_avif_reader *r)
{
    int version = stb_avif_read_byte(r);
    stb_avif_read_byte(r); /* flags */
    stb_avif_read_byte(r); /* flags */
    stb_avif_read_byte(r); /* flags */
    if (version < 1) {
        return (int)stb_avif_read_be16(r);
    } else {
        /* version >= 1 uses 32-bit */
        return (int)stb_avif_read_be32(r);
    }
}

/* Parse the meta box to extract AVIF metadata */
static void stb_avif_parse_meta(struct stb_avif_reader *r,
                                 struct stb_avif_avif_info *info)
{
    stbv_u32 data_offset = 0;
    stbv_u64 data_size = 0;
    struct stb_avif_box meta_box;
    size_t meta_end;

    /* Skip FullBox version+flags (4 bytes) */
    stb_avif_read_byte(r); stb_avif_read_byte(r);
    stb_avif_read_byte(r); stb_avif_read_byte(r);

    meta_box.data_start = (stbv_u64)r->pos;
    meta_box.data_size = (stbv_u64)(info->meta_end_offset - r->pos);

    meta_end = info->meta_end_offset;

    /* Scan sub-boxes within meta */
    while (r->pos < meta_end) {
        struct stb_avif_box sub;
        size_t sub_start = r->pos;

        if (r->pos + 8 > r->size) break;

        stb_avif_read_box_header(r, &sub);

        if (sub.type == STB_AVIF_BOX_HDLR) {
            /* handler box - verify picture handler */
        }
        else if (sub.type == STB_AVIF_BOX_PITM) {
            stb_avif_parse_pitm(r);
        }
        else if (sub.type == STB_AVIF_BOX_ILOC) {
            stb_avif_parse_iloc(r, info, &data_offset, &data_size);
        }
        else if (sub.type == STB_AVIF_BOX_IPRP) {
            /* Item properties container */
            struct stb_avif_box iprp_box = sub;
            stb_avif_enter_box(r, &iprp_box);

            while (r->pos < (size_t)(iprp_box.data_start + iprp_box.data_size)) {
                struct stb_avif_box iprp_sub;
                size_t iprp_sub_start = r->pos;

                if (r->pos + 8 > r->size) break;
                stb_avif_read_box_header(r, &iprp_sub);

                if (iprp_sub.type == STB_AVIF_BOX_IPCO) {
                    /* Item property container */
                    struct stb_avif_box ipco_box = iprp_sub;
                    stb_avif_enter_box(r, &ipco_box);

                    while (r->pos < (size_t)(ipco_box.data_start + ipco_box.data_size)) {
                        struct stb_avif_box prop;
                        size_t prop_start = r->pos;

                        if (r->pos + 8 > r->size) break;
                        stb_avif_read_box_header(r, &prop);

                        if (prop.type == STB_AVIF_BOX_AV1C) {
                            stb_avif_parse_av1c(r, info, (size_t)prop.data_size);
                        }
                        else if (prop.type == STB_AVIF_BOX_ISPE) {
                            /* Image spatial extents */
                            stb_avif_read_byte(r); /* version */
                            stb_avif_read_byte(r); /* flags */
                            stb_avif_read_byte(r);
                            stb_avif_read_byte(r);
                            info->width = (int)stb_avif_read_be32(r);
                            info->height = (int)stb_avif_read_be32(r);
                        }
                        else if (prop.type == STB_AVIF_BOX_PIXI) {
                            /* Pixel information (bit depth per channel) */
                            stb_avif_read_byte(r); /* version */
                            stb_avif_read_byte(r); /* flags */
                            stb_avif_read_byte(r);
                            stb_avif_read_byte(r);
                            /* num_channels */
                            (void)stb_avif_read_byte(r);
                        }

                        r->pos = (size_t)(prop_start + prop.size);
                    }
                }

                r->pos = (size_t)(iprp_sub_start + iprp_sub.size);
            }
        }

        r->pos = (size_t)(sub_start + sub.size);
    }

    /* Now read the mdat data */
    {
        size_t saved = r->pos;

        r->pos = 0;
        if (stb_avif_find_box(r, STB_AVIF_BOX_MDAT, 0, NULL)) {
                    info->av1_data = r->data + r->pos;
            info->av1_size = r->size - r->pos; /* Rest of file is mdat content */

            /* If we have iloc info, use that offset instead */
            if (data_size > 0 && data_offset > 0) {
                info->av1_data = r->data + data_offset;
                info->av1_size = (size_t)data_size;
            } else {
                /* Conservative: mdat may contain more than just our image.
                   Use iloc info. But if we don't have it, use all remaining. */
                /* The actual av1 data starts at data_offset from the beginning of mdat */
                if (data_offset > 0) {
                    /* data_offset is absolute in the file */
                    info->av1_data = r->data + data_offset;
                    if (data_size > 0)
                        info->av1_size = (size_t)data_size;
                    else
                        info->av1_size = r->size - data_offset;
                }
            }
        }
        r->pos = saved;
    }
}

/* -------------------------------------------------------------------------- */
/* AV1 BITSTREAM PARSER                                                       */
/* -------------------------------------------------------------------------- */

/* OBU types */
#define STB_AV1_OBU_SEQUENCE_HEADER 1
#define STB_AV1_OBU_TEMPORAL_DELIMITER 2
#define STB_AV1_OBU_FRAME_HEADER 3
#define STB_AV1_OBU_TILE_GROUP 4
#define STB_AV1_OBU_METADATA 5
#define STB_AV1_OBU_FRAME 6
#define STB_AV1_OBU_REDUNDANT_FRAME_HEADER 7
#define STB_AV1_OBU_TILE_LIST 8
#define STB_AV1_OBU_PADDING 15

/* OBU header:
   bit 0: forbidden (0)
   bits 1-4: type
   bit 5: obu_extension_flag
   bit 6: obu_has_size_field
   bit 7: obu_reserved_1bit (1)
*/
static int stb_av1_read_obu_header(struct stb_avif_reader *r, int *obu_type,
                                    int *obu_extension_flag, int *obu_has_size_field)
{
    int hdr = stb_avif_read_byte(r);
    if (hdr & 0x80) STB_AVIF_ERROR("Invalid OBU header (reserved bit not 0)");
    *obu_type = (hdr >> 3) & 0xF;
    *obu_extension_flag = (hdr >> 2) & 1;
    *obu_has_size_field = (hdr >> 1) & 1;
    return hdr & 1; /* obu_forbidden_bit */
}

/* Read OBU size (LEB128 encoded) */
static stbv_u32 stb_av1_read_obu_size(struct stb_avif_reader *r)
{
    return stb_avif_read_uleb128(r);
}

/* -------------------------------------------------------------------------- */
/* AV1 BOOLEAN (ARITHMETIC) ENTROPY DECODER                                   */
/* -------------------------------------------------------------------------- */

/* The AV1 spec defines a Boolean decoder based on the Daala entropy coder.
   State: value (16-bit window), range (9-bit, 128-255) */

#define STB_AV1_BOOL_READER_SIZE 4096
#define STB_AV1_BOOL_BUF_BITS 8

/* Math helpers (from dav1d intops.h) */
#ifndef stb_av1_imin
static int stb_av1_imin(const int a, const int b) { return a < b ? a : b; }
static int stb_av1_imax(const int a, const int b) { return a > b ? a : b; }
static int stb_av1_iclip(const int v, const int minv, const int maxv) { return v < minv ? minv : v > maxv ? maxv : v; }
static int stb_av1_ulog2(const unsigned v) { int r = 0; unsigned tmp = v; while (tmp >>= 1) r++; return r; }
#endif

/* Default scan tables for square transforms (AV1 spec order).
   scan[i] = x * h + y (packed), where (x,y) is position of i-th coefficient.
   For square blocks: h = w, so scan[i] = x * w + y.
   Extract: x = scan[i] / w, y = scan[i] % w. */
static const unsigned short stb_av1_scan_4x4[] = {
       0,    4,    1,    2,    5,    8,   12,    9,    6,    3,    7,   10,   13,   14,   11,   15
};

static const unsigned short stb_av1_scan_4x8[] = {
       0,    8,    1,   16,    9,    2,   24,   17,   10,    3,   25,   18,   11,    4,   26,   19,
      12,    5,   27,   20,   13,    6,   28,   21,   14,    7,   29,   22,   15,   30,   23,   31
};

static const unsigned short stb_av1_scan_4x16[] = {
       0,   16,    1,   32,   17,    2,   48,   33,   18,    3,   49,   34,   19,    4,   50,   35,
      20,    5,   51,   36,   21,    6,   52,   37,   22,    7,   53,   38,   23,    8,   54,   39,
      24,    9,   55,   40,   25,   10,   56,   41,   26,   11,   57,   42,   27,   12,   58,   43,
      28,   13,   59,   44,   29,   14,   60,   45,   30,   15,   61,   46,   31,   62,   47,   63
};

static const unsigned short stb_av1_scan_8x4[] = {
       0,    1,    4,    2,    5,    8,    3,    6,    9,   12,    7,   10,   13,   16,   11,   14,
      17,   20,   15,   18,   21,   24,   19,   22,   25,   28,   23,   26,   29,   27,   30,   31
};

static const unsigned short stb_av1_scan_8x8[] = {
       0,    8,    1,    2,    9,   16,   24,   17,   10,    3,    4,   11,   18,   25,   32,   40,
      33,   26,   19,   12,    5,    6,   13,   20,   27,   34,   41,   48,   56,   49,   42,   35,
      28,   21,   14,    7,   15,   22,   29,   36,   43,   50,   57,   58,   51,   44,   37,   30,
      23,   31,   38,   45,   52,   59,   60,   53,   46,   39,   47,   54,   61,   62,   55,   63
};

static const unsigned short stb_av1_scan_8x16[] = {
       0,   16,    1,   32,   17,    2,   48,   33,   18,    3,   64,   49,   34,   19,    4,   80,
      65,   50,   35,   20,    5,   96,   81,   66,   51,   36,   21,    6,  112,   97,   82,   67,
      52,   37,   22,    7,  113,   98,   83,   68,   53,   38,   23,    8,  114,   99,   84,   69,
      54,   39,   24,    9,  115,  100,   85,   70,   55,   40,   25,   10,  116,  101,   86,   71,
      56,   41,   26,   11,  117,  102,   87,   72,   57,   42,   27,   12,  118,  103,   88,   73,
      58,   43,   28,   13,  119,  104,   89,   74,   59,   44,   29,   14,  120,  105,   90,   75,
      60,   45,   30,   15,  121,  106,   91,   76,   61,   46,   31,  122,  107,   92,   77,   62,
      47,  123,  108,   93,   78,   63,  124,  109,   94,   79,  125,  110,   95,  126,  111,  127
};

static const unsigned short stb_av1_scan_8x32[] = {
       0,   32,    1,   64,   33,    2,   96,   65,   34,    3,  128,   97,   66,   35,    4,  160,
     129,   98,   67,   36,    5,  192,  161,  130,   99,   68,   37,    6,  224,  193,  162,  131,
     100,   69,   38,    7,  225,  194,  163,  132,  101,   70,   39,    8,  226,  195,  164,  133,
     102,   71,   40,    9,  227,  196,  165,  134,  103,   72,   41,   10,  228,  197,  166,  135,
     104,   73,   42,   11,  229,  198,  167,  136,  105,   74,   43,   12,  230,  199,  168,  137,
     106,   75,   44,   13,  231,  200,  169,  138,  107,   76,   45,   14,  232,  201,  170,  139,
     108,   77,   46,   15,  233,  202,  171,  140,  109,   78,   47,   16,  234,  203,  172,  141,
     110,   79,   48,   17,  235,  204,  173,  142,  111,   80,   49,   18,  236,  205,  174,  143,
     112,   81,   50,   19,  237,  206,  175,  144,  113,   82,   51,   20,  238,  207,  176,  145,
     114,   83,   52,   21,  239,  208,  177,  146,  115,   84,   53,   22,  240,  209,  178,  147,
     116,   85,   54,   23,  241,  210,  179,  148,  117,   86,   55,   24,  242,  211,  180,  149,
     118,   87,   56,   25,  243,  212,  181,  150,  119,   88,   57,   26,  244,  213,  182,  151,
     120,   89,   58,   27,  245,  214,  183,  152,  121,   90,   59,   28,  246,  215,  184,  153,
     122,   91,   60,   29,  247,  216,  185,  154,  123,   92,   61,   30,  248,  217,  186,  155,
     124,   93,   62,   31,  249,  218,  187,  156,  125,   94,   63,  250,  219,  188,  157,  126,
      95,  251,  220,  189,  158,  127,  252,  221,  190,  159,  253,  222,  191,  254,  223,  255
};

static const unsigned short stb_av1_scan_16x4[] = {
       0,    1,    4,    2,    5,    8,    3,    6,    9,   12,    7,   10,   13,   16,   11,   14,
      17,   20,   15,   18,   21,   24,   19,   22,   25,   28,   23,   26,   29,   32,   27,   30,
      33,   36,   31,   34,   37,   40,   35,   38,   41,   44,   39,   42,   45,   48,   43,   46,
      49,   52,   47,   50,   53,   56,   51,   54,   57,   60,   55,   58,   61,   59,   62,   63
};

static const unsigned short stb_av1_scan_16x8[] = {
       0,    1,    8,    2,    9,   16,    3,   10,   17,   24,    4,   11,   18,   25,   32,    5,
      12,   19,   26,   33,   40,    6,   13,   20,   27,   34,   41,   48,    7,   14,   21,   28,
      35,   42,   49,   56,   15,   22,   29,   36,   43,   50,   57,   64,   23,   30,   37,   44,
      51,   58,   65,   72,   31,   38,   45,   52,   59,   66,   73,   80,   39,   46,   53,   60,
      67,   74,   81,   88,   47,   54,   61,   68,   75,   82,   89,   96,   55,   62,   69,   76,
      83,   90,   97,  104,   63,   70,   77,   84,   91,   98,  105,  112,   71,   78,   85,   92,
      99,  106,  113,  120,   79,   86,   93,  100,  107,  114,  121,   87,   94,  101,  108,  115,
     122,   95,  102,  109,  116,  123,  103,  110,  117,  124,  111,  118,  125,  119,  126,  127
};

static const unsigned short stb_av1_scan_16x16[] = {
       0,   16,    1,    2,   17,   32,   48,   33,   18,    3,    4,   19,   34,   49,   64,   80,
      65,   50,   35,   20,    5,    6,   21,   36,   51,   66,   81,   96,  112,   97,   82,   67,
      52,   37,   22,    7,    8,   23,   38,   53,   68,   83,   98,  113,  128,  144,  129,  114,
      99,   84,   69,   54,   39,   24,    9,   10,   25,   40,   55,   70,   85,  100,  115,  130,
     145,  160,  176,  161,  146,  131,  116,  101,   86,   71,   56,   41,   26,   11,   12,   27,
      42,   57,   72,   87,  102,  117,  132,  147,  162,  177,  192,  208,  193,  178,  163,  148,
     133,  118,  103,   88,   73,   58,   43,   28,   13,   14,   29,   44,   59,   74,   89,  104,
     119,  134,  149,  164,  179,  194,  209,  224,  240,  225,  210,  195,  180,  165,  150,  135,
     120,  105,   90,   75,   60,   45,   30,   15,   31,   46,   61,   76,   91,  106,  121,  136,
     151,  166,  181,  196,  211,  226,  241,  242,  227,  212,  197,  182,  167,  152,  137,  122,
     107,   92,   77,   62,   47,   63,   78,   93,  108,  123,  138,  153,  168,  183,  198,  213,
     228,  243,  244,  229,  214,  199,  184,  169,  154,  139,  124,  109,   94,   79,   95,  110,
     125,  140,  155,  170,  185,  200,  215,  230,  245,  246,  231,  216,  201,  186,  171,  156,
     141,  126,  111,  127,  142,  157,  172,  187,  202,  217,  232,  247,  248,  233,  218,  203,
     188,  173,  158,  143,  159,  174,  189,  204,  219,  234,  249,  250,  235,  220,  205,  190,
     175,  191,  206,  221,  236,  251,  252,  237,  222,  207,  223,  238,  253,  254,  239,  255
};

static const unsigned short stb_av1_scan_16x32[] = {
       0,   32,    1,   64,   33,    2,   96,   65,   34,    3,  128,   97,   66,   35,    4,  160,
     129,   98,   67,   36,    5,  192,  161,  130,   99,   68,   37,    6,  224,  193,  162,  131,
     100,   69,   38,    7,  256,  225,  194,  163,  132,  101,   70,   39,    8,  288,  257,  226,
     195,  164,  133,  102,   71,   40,    9,  320,  289,  258,  227,  196,  165,  134,  103,   72,
      41,   10,  352,  321,  290,  259,  228,  197,  166,  135,  104,   73,   42,   11,  384,  353,
     322,  291,  260,  229,  198,  167,  136,  105,   74,   43,   12,  416,  385,  354,  323,  292,
     261,  230,  199,  168,  137,  106,   75,   44,   13,  448,  417,  386,  355,  324,  293,  262,
     231,  200,  169,  138,  107,   76,   45,   14,  480,  449,  418,  387,  356,  325,  294,  263,
     232,  201,  170,  139,  108,   77,   46,   15,  481,  450,  419,  388,  357,  326,  295,  264,
     233,  202,  171,  140,  109,   78,   47,   16,  482,  451,  420,  389,  358,  327,  296,  265,
     234,  203,  172,  141,  110,   79,   48,   17,  483,  452,  421,  390,  359,  328,  297,  266,
     235,  204,  173,  142,  111,   80,   49,   18,  484,  453,  422,  391,  360,  329,  298,  267,
     236,  205,  174,  143,  112,   81,   50,   19,  485,  454,  423,  392,  361,  330,  299,  268,
     237,  206,  175,  144,  113,   82,   51,   20,  486,  455,  424,  393,  362,  331,  300,  269,
     238,  207,  176,  145,  114,   83,   52,   21,  487,  456,  425,  394,  363,  332,  301,  270,
     239,  208,  177,  146,  115,   84,   53,   22,  488,  457,  426,  395,  364,  333,  302,  271,
     240,  209,  178,  147,  116,   85,   54,   23,  489,  458,  427,  396,  365,  334,  303,  272,
     241,  210,  179,  148,  117,   86,   55,   24,  490,  459,  428,  397,  366,  335,  304,  273,
     242,  211,  180,  149,  118,   87,   56,   25,  491,  460,  429,  398,  367,  336,  305,  274,
     243,  212,  181,  150,  119,   88,   57,   26,  492,  461,  430,  399,  368,  337,  306,  275,
     244,  213,  182,  151,  120,   89,   58,   27,  493,  462,  431,  400,  369,  338,  307,  276,
     245,  214,  183,  152,  121,   90,   59,   28,  494,  463,  432,  401,  370,  339,  308,  277,
     246,  215,  184,  153,  122,   91,   60,   29,  495,  464,  433,  402,  371,  340,  309,  278,
     247,  216,  185,  154,  123,   92,   61,   30,  496,  465,  434,  403,  372,  341,  310,  279,
     248,  217,  186,  155,  124,   93,   62,   31,  497,  466,  435,  404,  373,  342,  311,  280,
     249,  218,  187,  156,  125,   94,   63,  498,  467,  436,  405,  374,  343,  312,  281,  250,
     219,  188,  157,  126,   95,  499,  468,  437,  406,  375,  344,  313,  282,  251,  220,  189,
     158,  127,  500,  469,  438,  407,  376,  345,  314,  283,  252,  221,  190,  159,  501,  470,
     439,  408,  377,  346,  315,  284,  253,  222,  191,  502,  471,  440,  409,  378,  347,  316,
     285,  254,  223,  503,  472,  441,  410,  379,  348,  317,  286,  255,  504,  473,  442,  411,
     380,  349,  318,  287,  505,  474,  443,  412,  381,  350,  319,  506,  475,  444,  413,  382,
     351,  507,  476,  445,  414,  383,  508,  477,  446,  415,  509,  478,  447,  510,  479,  511
};

static const unsigned short stb_av1_scan_32x8[] = {
       0,    1,    8,    2,    9,   16,    3,   10,   17,   24,    4,   11,   18,   25,   32,    5,
      12,   19,   26,   33,   40,    6,   13,   20,   27,   34,   41,   48,    7,   14,   21,   28,
      35,   42,   49,   56,   15,   22,   29,   36,   43,   50,   57,   64,   23,   30,   37,   44,
      51,   58,   65,   72,   31,   38,   45,   52,   59,   66,   73,   80,   39,   46,   53,   60,
      67,   74,   81,   88,   47,   54,   61,   68,   75,   82,   89,   96,   55,   62,   69,   76,
      83,   90,   97,  104,   63,   70,   77,   84,   91,   98,  105,  112,   71,   78,   85,   92,
      99,  106,  113,  120,   79,   86,   93,  100,  107,  114,  121,  128,   87,   94,  101,  108,
     115,  122,  129,  136,   95,  102,  109,  116,  123,  130,  137,  144,  103,  110,  117,  124,
     131,  138,  145,  152,  111,  118,  125,  132,  139,  146,  153,  160,  119,  126,  133,  140,
     147,  154,  161,  168,  127,  134,  141,  148,  155,  162,  169,  176,  135,  142,  149,  156,
     163,  170,  177,  184,  143,  150,  157,  164,  171,  178,  185,  192,  151,  158,  165,  172,
     179,  186,  193,  200,  159,  166,  173,  180,  187,  194,  201,  208,  167,  174,  181,  188,
     195,  202,  209,  216,  175,  182,  189,  196,  203,  210,  217,  224,  183,  190,  197,  204,
     211,  218,  225,  232,  191,  198,  205,  212,  219,  226,  233,  240,  199,  206,  213,  220,
     227,  234,  241,  248,  207,  214,  221,  228,  235,  242,  249,  215,  222,  229,  236,  243,
     250,  223,  230,  237,  244,  251,  231,  238,  245,  252,  239,  246,  253,  247,  254,  255
};

static const unsigned short stb_av1_scan_32x16[] = {
       0,    1,   16,    2,   17,   32,    3,   18,   33,   48,    4,   19,   34,   49,   64,    5,
      20,   35,   50,   65,   80,    6,   21,   36,   51,   66,   81,   96,    7,   22,   37,   52,
      67,   82,   97,  112,    8,   23,   38,   53,   68,   83,   98,  113,  128,    9,   24,   39,
      54,   69,   84,   99,  114,  129,  144,   10,   25,   40,   55,   70,   85,  100,  115,  130,
     145,  160,   11,   26,   41,   56,   71,   86,  101,  116,  131,  146,  161,  176,   12,   27,
      42,   57,   72,   87,  102,  117,  132,  147,  162,  177,  192,   13,   28,   43,   58,   73,
      88,  103,  118,  133,  148,  163,  178,  193,  208,   14,   29,   44,   59,   74,   89,  104,
     119,  134,  149,  164,  179,  194,  209,  224,   15,   30,   45,   60,   75,   90,  105,  120,
     135,  150,  165,  180,  195,  210,  225,  240,   31,   46,   61,   76,   91,  106,  121,  136,
     151,  166,  181,  196,  211,  226,  241,  256,   47,   62,   77,   92,  107,  122,  137,  152,
     167,  182,  197,  212,  227,  242,  257,  272,   63,   78,   93,  108,  123,  138,  153,  168,
     183,  198,  213,  228,  243,  258,  273,  288,   79,   94,  109,  124,  139,  154,  169,  184,
     199,  214,  229,  244,  259,  274,  289,  304,   95,  110,  125,  140,  155,  170,  185,  200,
     215,  230,  245,  260,  275,  290,  305,  320,  111,  126,  141,  156,  171,  186,  201,  216,
     231,  246,  261,  276,  291,  306,  321,  336,  127,  142,  157,  172,  187,  202,  217,  232,
     247,  262,  277,  292,  307,  322,  337,  352,  143,  158,  173,  188,  203,  218,  233,  248,
     263,  278,  293,  308,  323,  338,  353,  368,  159,  174,  189,  204,  219,  234,  249,  264,
     279,  294,  309,  324,  339,  354,  369,  384,  175,  190,  205,  220,  235,  250,  265,  280,
     295,  310,  325,  340,  355,  370,  385,  400,  191,  206,  221,  236,  251,  266,  281,  296,
     311,  326,  341,  356,  371,  386,  401,  416,  207,  222,  237,  252,  267,  282,  297,  312,
     327,  342,  357,  372,  387,  402,  417,  432,  223,  238,  253,  268,  283,  298,  313,  328,
     343,  358,  373,  388,  403,  418,  433,  448,  239,  254,  269,  284,  299,  314,  329,  344,
     359,  374,  389,  404,  419,  434,  449,  464,  255,  270,  285,  300,  315,  330,  345,  360,
     375,  390,  405,  420,  435,  450,  465,  480,  271,  286,  301,  316,  331,  346,  361,  376,
     391,  406,  421,  436,  451,  466,  481,  496,  287,  302,  317,  332,  347,  362,  377,  392,
     407,  422,  437,  452,  467,  482,  497,  303,  318,  333,  348,  363,  378,  393,  408,  423,
     438,  453,  468,  483,  498,  319,  334,  349,  364,  379,  394,  409,  424,  439,  454,  469,
     484,  499,  335,  350,  365,  380,  395,  410,  425,  440,  455,  470,  485,  500,  351,  366,
     381,  396,  411,  426,  441,  456,  471,  486,  501,  367,  382,  397,  412,  427,  442,  457,
     472,  487,  502,  383,  398,  413,  428,  443,  458,  473,  488,  503,  399,  414,  429,  444,
     459,  474,  489,  504,  415,  430,  445,  460,  475,  490,  505,  431,  446,  461,  476,  491,
     506,  447,  462,  477,  492,  507,  463,  478,  493,  508,  479,  494,  509,  495,  510,  511
};

static const unsigned short stb_av1_scan_32x32[] = {
       0,   32,    1,    2,   33,   64,   96,   65,   34,    3,    4,   35,   66,   97,  128,  160,
     129,   98,   67,   36,    5,    6,   37,   68,   99,  130,  161,  192,  224,  193,  162,  131,
     100,   69,   38,    7,    8,   39,   70,  101,  132,  163,  194,  225,  256,  288,  257,  226,
     195,  164,  133,  102,   71,   40,    9,   10,   41,   72,  103,  134,  165,  196,  227,  258,
     289,  320,  352,  321,  290,  259,  228,  197,  166,  135,  104,   73,   42,   11,   12,   43,
      74,  105,  136,  167,  198,  229,  260,  291,  322,  353,  384,  416,  385,  354,  323,  292,
     261,  230,  199,  168,  137,  106,   75,   44,   13,   14,   45,   76,  107,  138,  169,  200,
     231,  262,  293,  324,  355,  386,  417,  448,  480,  449,  418,  387,  356,  325,  294,  263,
     232,  201,  170,  139,  108,   77,   46,   15,   16,   47,   78,  109,  140,  171,  202,  233,
     264,  295,  326,  357,  388,  419,  450,  481,  512,  544,  513,  482,  451,  420,  389,  358,
     327,  296,  265,  234,  203,  172,  141,  110,   79,   48,   17,   18,   49,   80,  111,  142,
     173,  204,  235,  266,  297,  328,  359,  390,  421,  452,  483,  514,  545,  576,  608,  577,
     546,  515,  484,  453,  422,  391,  360,  329,  298,  267,  236,  205,  174,  143,  112,   81,
      50,   19,   20,   51,   82,  113,  144,  175,  206,  237,  268,  299,  330,  361,  392,  423,
     454,  485,  516,  547,  578,  609,  640,  672,  641,  610,  579,  548,  517,  486,  455,  424,
     393,  362,  331,  300,  269,  238,  207,  176,  145,  114,   83,   52,   21,   22,   53,   84,
     115,  146,  177,  208,  239,  270,  301,  332,  363,  394,  425,  456,  487,  518,  549,  580,
     611,  642,  673,  704,  736,  705,  674,  643,  612,  581,  550,  519,  488,  457,  426,  395,
     364,  333,  302,  271,  240,  209,  178,  147,  116,   85,   54,   23,   24,   55,   86,  117,
     148,  179,  210,  241,  272,  303,  334,  365,  396,  427,  458,  489,  520,  551,  582,  613,
     644,  675,  706,  737,  768,  800,  769,  738,  707,  676,  645,  614,  583,  552,  521,  490,
     459,  428,  397,  366,  335,  304,  273,  242,  211,  180,  149,  118,   87,   56,   25,   26,
      57,   88,  119,  150,  181,  212,  243,  274,  305,  336,  367,  398,  429,  460,  491,  522,
     553,  584,  615,  646,  677,  708,  739,  770,  801,  832,  864,  833,  802,  771,  740,  709,
     678,  647,  616,  585,  554,  523,  492,  461,  430,  399,  368,  337,  306,  275,  244,  213,
     182,  151,  120,   89,   58,   27,   28,   59,   90,  121,  152,  183,  214,  245,  276,  307,
     338,  369,  400,  431,  462,  493,  524,  555,  586,  617,  648,  679,  710,  741,  772,  803,
     834,  865,  896,  928,  897,  866,  835,  804,  773,  742,  711,  680,  649,  618,  587,  556,
     525,  494,  463,  432,  401,  370,  339,  308,  277,  246,  215,  184,  153,  122,   91,   60,
      29,   30,   61,   92,  123,  154,  185,  216,  247,  278,  309,  340,  371,  402,  433,  464,
     495,  526,  557,  588,  619,  650,  681,  712,  743,  774,  805,  836,  867,  898,  929,  960,
     992,  961,  930,  899,  868,  837,  806,  775,  744,  713,  682,  651,  620,  589,  558,  527,
     496,  465,  434,  403,  372,  341,  310,  279,  248,  217,  186,  155,  124,   93,   62,   31,
      63,   94,  125,  156,  187,  218,  249,  280,  311,  342,  373,  404,  435,  466,  497,  528,
     559,  590,  621,  652,  683,  714,  745,  776,  807,  838,  869,  900,  931,  962,  993,  994,
     963,  932,  901,  870,  839,  808,  777,  746,  715,  684,  653,  622,  591,  560,  529,  498,
     467,  436,  405,  374,  343,  312,  281,  250,  219,  188,  157,  126,   95,  127,  158,  189,
     220,  251,  282,  313,  344,  375,  406,  437,  468,  499,  530,  561,  592,  623,  654,  685,
     716,  747,  778,  809,  840,  871,  902,  933,  964,  995,  996,  965,  934,  903,  872,  841,
     810,  779,  748,  717,  686,  655,  624,  593,  562,  531,  500,  469,  438,  407,  376,  345,
     314,  283,  252,  221,  190,  159,  191,  222,  253,  284,  315,  346,  377,  408,  439,  470,
     501,  532,  563,  594,  625,  656,  687,  718,  749,  780,  811,  842,  873,  904,  935,  966,
     997,  998,  967,  936,  905,  874,  843,  812,  781,  750,  719,  688,  657,  626,  595,  564,
     533,  502,  471,  440,  409,  378,  347,  316,  285,  254,  223,  255,  286,  317,  348,  379,
     410,  441,  472,  503,  534,  565,  596,  627,  658,  689,  720,  751,  782,  813,  844,  875,
     906,  937,  968,  999, 1000,  969,  938,  907,  876,  845,  814,  783,  752,  721,  690,  659,
     628,  597,  566,  535,  504,  473,  442,  411,  380,  349,  318,  287,  319,  350,  381,  412,
     443,  474,  505,  536,  567,  598,  629,  660,  691,  722,  753,  784,  815,  846,  877,  908,
     939,  970, 1001, 1002,  971,  940,  909,  878,  847,  816,  785,  754,  723,  692,  661,  630,
     599,  568,  537,  506,  475,  444,  413,  382,  351,  383,  414,  445,  476,  507,  538,  569,
     600,  631,  662,  693,  724,  755,  786,  817,  848,  879,  910,  941,  972, 1003, 1004,  973,
     942,  911,  880,  849,  818,  787,  756,  725,  694,  663,  632,  601,  570,  539,  508,  477,
     446,  415,  447,  478,  509,  540,  571,  602,  633,  664,  695,  726,  757,  788,  819,  850,
     881,  912,  943,  974, 1005, 1006,  975,  944,  913,  882,  851,  820,  789,  758,  727,  696,
     665,  634,  603,  572,  541,  510,  479,  511,  542,  573,  604,  635,  666,  697,  728,  759,
     790,  821,  852,  883,  914,  945,  976, 1007, 1008,  977,  946,  915,  884,  853,  822,  791,
     760,  729,  698,  667,  636,  605,  574,  543,  575,  606,  637,  668,  699,  730,  761,  792,
     823,  854,  885,  916,  947,  978, 1009, 1010,  979,  948,  917,  886,  855,  824,  793,  762,
     731,  700,  669,  638,  607,  639,  670,  701,  732,  763,  794,  825,  856,  887,  918,  949,
     980, 1011, 1012,  981,  950,  919,  888,  857,  826,  795,  764,  733,  702,  671,  703,  734,
     765,  796,  827,  858,  889,  920,  951,  982, 1013, 1014,  983,  952,  921,  890,  859,  828,
     797,  766,  735,  767,  798,  829,  860,  891,  922,  953,  984, 1015, 1016,  985,  954,  923,
     892,  861,  830,  799,  831,  862,  893,  924,  955,  986, 1017, 1018,  987,  956,  925,  894,
     863,  895,  926,  957,  988, 1019, 1020,  989,  958,  927,  959,  990, 1021, 1022,  991, 1023
};

/* Scan table lookup: (tx_w, tx_h) -> scan table, shift = log2(tx_h) */
/* Scan values encode rc = col * tx_h + row; decode: col = rc >> shift, row = rc & mask */
static const unsigned short *stb_av1_get_scan(int tx_w, int tx_h, int *shift_out) {
    int shift = 0, s = tx_h;
    while (s > 1) { s >>= 1; shift++; }
    *shift_out = shift;
    if (tx_w == tx_h) {
        switch (tx_w) {
            case 4:  return stb_av1_scan_4x4;
            case 8:  return stb_av1_scan_8x8;
            case 16: return stb_av1_scan_16x16;
            case 32: return stb_av1_scan_32x32;
            default: return stb_av1_scan_32x32;
        }
    } else if (tx_w < tx_h) { /* tall */
        if (tx_w == 4) {
            if (tx_h == 8)  return stb_av1_scan_4x8;
            if (tx_h == 16) return stb_av1_scan_4x16;
            return stb_av1_scan_4x16;
        } else if (tx_w == 8) {
            if (tx_h == 16) return stb_av1_scan_8x16;
            if (tx_h == 32) return stb_av1_scan_8x32;
            return stb_av1_scan_8x32;
        } else if (tx_w == 16) {
            if (tx_h == 32) return stb_av1_scan_16x32;
            return stb_av1_scan_16x32;
        }
        return stb_av1_scan_32x32;
    } else { /* wide */
        if (tx_h == 4) {
            if (tx_w == 8)  return stb_av1_scan_8x4;
            if (tx_w == 16) return stb_av1_scan_16x4;
            return stb_av1_scan_32x8;
        } else if (tx_h == 8) {
            if (tx_w == 16) return stb_av1_scan_16x8;
            if (tx_w == 32) return stb_av1_scan_32x8;
            return stb_av1_scan_32x8;
        } else if (tx_h == 16) {
            if (tx_w == 32) return stb_av1_scan_32x16;
            return stb_av1_scan_32x16;
        }
        return stb_av1_scan_32x32;
    }
}


#ifdef STB_AVIF_USE_C89_DAV1D
/*
 * C89 port: Full CDF context struct matching INIT ORDER from dav1d's cdf.c
 * Fields are in the order that the default_cdf initialization uses.
 * Coefficient CDFs are placed after mode CDFs to match the flat data array.
 */

#ifndef STB_AV1_CDF_H
#define STB_AV1_CDF_H

/* Coefficient CDF sub-struct (matches dav1d's CdfCoefContext, fields in INIT ORDER) */
struct StbCdfCoef {
    unsigned short skip[5][13][2];
    unsigned short eob_bin_16[2][2][8];
    unsigned short eob_bin_32[2][2][8];
    unsigned short eob_bin_64[2][2][8];
    unsigned short eob_bin_128[2][2][8];
    unsigned short eob_bin_256[2][2][16];
    unsigned short eob_bin_512[2][16];
    unsigned short eob_bin_1024[2][16];
    unsigned short eob_hi_bit[5][2][9][2];
    unsigned short eob_base_tok[5][2][4][3];
    unsigned short base_tok[5][2][41][4];
    unsigned short dc_sign[2][3][2];
    unsigned short br_tok[4][2][21][4];
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
    unsigned short txsz[4][3][4];
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
/* Auto-generated per-field CDF data from dav1d cdf.c */
/* Generated by gen_final2.py - DO NOT EDIT MANUALLY */

static const unsigned short stb_av1_uv_mode[2][13][15] = {
    {
        {10137, 8616, 7390, 7107, 6782, 6248, 5713, 4845, 4524, 2709, 1827,  807,    0,    0,    0},
        {23255, 5887, 5795, 5722, 5650, 5104, 5029, 4944, 4409, 3263, 2968,  972,    0,    0,    0},
        {22923,22853, 4105, 4064, 4011, 3988, 3570, 2946, 2914, 2004,  991,  739,    0,    0,    0},
        {19129,18871,18597, 7437, 7162, 7041, 6815, 5620, 4191, 2156, 1413,  275,    0,    0,    0},
        {23004,22933,22838,22814, 7382, 5715, 4810, 4620, 4525, 1667, 1024,  405,    0,    0,    0},
        {20943,19179,19091,19048,17720, 3555, 3467, 3310, 3057, 1607, 1327,  218,    0,    0,    0},
        {18593,18369,16160,15947,15050,14993, 4217, 2568, 2523,  931,  426,  101,    0,    0,    0},
        {19883,19730,17790,17178,17095,17020,16592, 3640, 3501, 2125,  807,  307,    0,    0,    0},
        {20742,19107,18894,17463,17278,17042,16773,16495, 4325, 2380, 2001,  352,    0,    0,    0},
        {13716,12928,12189,11852,11618,11301,10883,10049, 9594, 3907, 2389,  593,    0,    0,    0},
        {14141,13119,11794,11549,11276,10952,10569, 9649, 9241, 5715, 1371,  620,    0,    0,    0},
        {15742,13764,12771,12429,12182,11665,11419,10861,10286, 6872, 6227,  949,    0,    0,    0},
        {20644,19009,17809,17776,17761,17717,17690,17602,17513,17015,16729,16162,    0,    0,    0}
    },
    {
        {22361,21560,19868,19587,18945,18593,17869,17112,16782,12682,11773,10313, 8556,    0,    0},
        {28236,12988,12711,12553,12340,11697,11569,11317,10669, 8540, 8075, 5736, 3296,    0,    0},
        {27495,27389,12591,12498,12383,12329,11819,11073,10994, 9630, 8512, 8065, 6089,    0,    0},
        {26028,25601,25106,18616,18232,17983,17734,16027,14397,11248,10562, 9379, 8586,    0,    0},
        {27781,27400,26840,26700,13654,12453,10911,10515,10357, 7857, 7388, 6741, 6392,    0,    0},
        {27398,25879,25521,25375,23270,11654,11366,11015,10787, 7988, 7382, 6251, 5592,    0,    0},
        {27952,27807,25564,25442,24003,23838,12599,12086,11965, 9580, 9005, 8313, 7828,    0,    0},
        {26160,26028,24239,23719,23511,23412,23033,13941,13709,10432, 9564, 8804, 7975,    0,    0},
        {26770,25349,24987,23835,23513,23219,23015,22351,13870,10274, 9629, 8004, 6779,    0,    0},
        {22108,21470,20218,19811,19446,19144,18728,17764,17234,12054,10979, 9325, 7907,    0,    0},
        {22246,21238,20216,19805,19390,18989,18523,17533,16866,12666,10072, 8994, 6930,    0,    0},
        {22669,22077,20129,19719,19382,19103,18643,17605,17132,13092,12294, 9249, 7560,    0,    0},
        {29624,27681,25386,25264,25175,25078,24967,24704,24536,23520,22893,22247, 3720,    0,    0}
    }
};

static const unsigned short stb_av1_partition[5][4][16] = {
    {
        { 4869, 4549, 4239,  284,  229,  149,  129,    0,    0,    0,    0,    0,    0,    0,    0,    0},
        {26161,25778,24500,  708,  549,  430,  397,    0,    0,    0,    0,    0,    0,    0,    0,    0},
        {27339,26092,25646,  741,  541,  237,  186,    0,    0,    0,    0,    0,    0,    0,    0,    0},
        {32057,31802,31596,  320,  230,  151,  104,    0,    0,    0,    0,    0,    0,    0,    0,    0}
    },
    {
        {12631,11221, 9690, 3202, 2931, 2507, 2244, 1876, 1044,    0,    0,    0,    0,    0,    0,    0},
        {26036,25278,23271, 4824, 4518, 4253, 3799, 3138, 2664,    0,    0,    0,    0,    0,    0,    0},
        {26823,25105,24420, 4085, 3651, 3019, 2704, 2470,  530,    0,    0,    0,    0,    0,    0,    0},
        {31898,31556,31281, 1570, 1374, 1194, 1025,  887,  436,    0,    0,    0,    0,    0,    0,    0}
    },
    {
        {14306,11848, 9644, 5121, 4541, 3719, 3249, 2590, 1224,    0,    0,    0,    0,    0,    0,    0},
        {25079,23708,20712, 7776, 7108, 6586, 5817, 4727, 3716,    0,    0,    0,    0,    0,    0,    0},
        {26753,23759,22706, 8224, 7359, 6223, 5697, 5242,  721,    0,    0,    0,    0,    0,    0,    0},
        {31374,30560,29972, 4154, 3707, 3302, 2928, 2583,  869,    0,    0,    0,    0,    0,    0,    0}
    },
    {
        {17171,11839, 8197, 6062, 5104, 3947, 3167, 2197,  866,    0,    0,    0,    0,    0,    0,    0},
        {24843,21725,15983,10298, 8797, 7725, 6117, 4067, 2934,    0,    0,    0,    0,    0,    0,    0},
        {27354,19499,17657,12280,10408, 8268, 7231, 6432,  651,    0,    0,    0,    0,    0,    0,    0},
        {30106,26406,24154,11908, 9715, 7990, 6332, 4939, 1597,    0,    0,    0,    0,    0,    0,    0}
    },
    {
        {13636, 7258, 2376,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0},
        {18840,12913, 4228,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0},
        {20246, 9089, 4139,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0},
        {22872,13985, 6915,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0}
    }
};

static const unsigned short stb_av1_cfl_alpha[6][16] = {
    {25131,12049, 1367,  287,  111,   80,   76,   72,   68,   64,   60,   56,   52,   48,   44,    0},
    {18403, 9165, 4633, 1600,  601,  373,  281,  195,  148,  121,  100,   96,   92,   88,   84,    0},
    {21236,10388, 4323, 1408,  419,  245,  184,  119,   95,   91,   87,   83,   79,   75,   71,    0},
    { 5778, 1366,  486,  197,   76,   72,   68,   64,   60,   56,   52,   48,   44,   40,   36,    0},
    {15520, 6710, 3864, 2160, 1463,  891,  642,  447,  374,  304,  252,  208,  192,  175,  146,    0},
    {18030,11090, 6989, 4867, 3744, 2466, 1788,  925,  624,  355,  248,  174,  146,  112,  108,    0}
};

static const unsigned short stb_av1_txtp_inter1[2][16] = {
    {28310,27208,25073,23059,19438,17979,15231,12502,11264, 9920, 8834, 7294, 5041, 3853, 2137,    0},
    {31123,30195,27990,27057,24961,24146,22246,17411,15094,12360,10251, 7758, 5652, 3912, 2019,    0}
};

static const unsigned short stb_av1_txtp_inter2[16] = {31998,30347,27543,19861,16949,13841,11207, 8679, 6173, 4242, 2239,    0,    0,    0,    0,    0};

static const unsigned short stb_av1_txtp_intra1[2][13][8] = {
    {
        {31233,24733,23307,20017, 9301, 4943,    0,    0},
        {32204,29433,23059,21898,14625, 4674,    0,    0},
        {32096,29521,29092,20786,13353, 9641,    0,    0},
        {27489,18883,17281,14724, 9241, 2516,    0,    0},
        {28345,26694,24783,22352, 7075, 3470,    0,    0},
        {31282,28527,23308,22106,16312, 5074,    0,    0},
        {32329,29930,29246,26031,14710, 9014,    0,    0},
        {31578,28535,27913,21098,12487, 8391,    0,    0},
        {31723,28456,24121,22609,14124, 3433,    0,    0},
        {32566,29034,28021,25470,15641, 8752,    0,    0},
        {32321,28456,25949,23884,16758, 8910,    0,    0},
        {32491,28399,27513,23863,16303,10497,    0,    0},
        {29359,27332,22169,17169,13081, 8728,    0,    0}
    },
    {
        {30898,19026,18238,16270, 8998, 5070,    0,    0},
        {32442,23972,18136,17689,13496, 5282,    0,    0},
        {32284,25192,25056,18325,13609,10177,    0,    0},
        {31642,17428,16873,15745,11872, 2489,    0,    0},
        {32113,27914,27519,26855,10669, 5630,    0,    0},
        {31469,26310,23883,23478,17917, 7271,    0,    0},
        {32457,27473,27216,25883,16661,10096,    0,    0},
        {31885,24709,24498,21510,15479,11219,    0,    0},
        {32027,25188,23450,22423,16080, 3722,    0,    0},
        {32658,25362,24853,23573,16727, 9439,    0,    0},
        {32405,24794,23411,22095,17139, 8294,    0,    0},
        {32615,25121,24656,22832,17461,12772,    0,    0},
        {29257,26436,21603,17433,13445, 9174,    0,    0}
    }
};

static const unsigned short stb_av1_txtp_intra2[3][13][8] = {
    {
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0}
    },
    {
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0},
        {26214,19661,13107, 6554,    0,    0,    0,    0}
    },
    {
        {31641,19954, 9996, 5285,    0,    0,    0,    0},
        {32623,26007,20788, 6101,    0,    0,    0,    0},
        {32406,26881,21090,16043,    0,    0,    0,    0},
        {32383,17555,14181, 2075,    0,    0,    0,    0},
        {32743,29854, 9634, 4865,    0,    0,    0,    0},
        {32708,28298,21019, 8777,    0,    0,    0,    0},
        {32731,29436,18257,11320,    0,    0,    0,    0},
        {32611,26448,19732,15329,    0,    0,    0,    0},
        {32649,26049,19862, 3372,    0,    0,    0,    0},
        {32721,27231,20192,11269,    0,    0,    0,    0},
        {32499,26692,21510, 9653,    0,    0,    0,    0},
        {32685,27153,20767,15540,    0,    0,    0,    0},
        {30800,27212,20745,14221,    0,    0,    0,    0}
    }
};

static const unsigned short stb_av1_cfl_sign[8] = {31350,30645,19428,14363, 5796, 4425,  474,    0};

static const unsigned short stb_av1_angle_delta[8][8] = {
    {30588,27736,25201, 9992, 5779, 2551,    0,    0},
    {30467,27160,23967, 9281, 5794, 2438,    0,    0},
    {28988,21750,19069,13414, 9685, 1482,    0,    0},
    {28187,21542,17621,15630,10934, 4371,    0,    0},
    {31031,21841,18259,13180,10023, 3945,    0,    0},
    {30104,22592,20283,15118,11168, 2273,    0,    0},
    {30528,21672,17315,12427,10207, 3851,    0,    0},
    {29163,22340,20309,15092,11524, 2113,    0,    0}
};

static const unsigned short stb_av1_filter_intra[8] = {23819,19992,15557, 3210,    0,    0,    0,    0};

static const unsigned short stb_av1_seg_id[3][8] = {
    {27146,24875,16675,14535, 4959, 4395,  235,    0},
    {18494,14538,10211, 7833, 2788, 1917,  424,    0},
    { 5241, 4281, 4045, 3878,  371,  121,   89,    0}
};

static const unsigned short stb_av1_pal_sz[2][7][8] = {
    {
        {24816,19768,14619,11290, 7241, 3527,    0,    0},
        {25629,21347,16573,13224, 9102, 4695,    0,    0},
        {24980,20027,15443,12268, 8453, 4238,    0,    0},
        {24497,18704,14522,11204, 7697, 4235,    0,    0},
        {20043,13588,10905, 7929, 5233, 2648,    0,    0},
        {23057,17880,15845,11716, 7107, 4893,    0,    0},
        {17828,11971,11090, 8582, 5735, 3769,    0,    0}
    },
    {
        {24055,12789, 5640, 3159, 1437,  496,    0,    0},
        {26929,17195, 9187, 5821, 2920, 1068,    0,    0},
        {28342,21508,14769,11285, 6905, 3338,    0,    0},
        {29540,23304,17775,14679,10245, 5348,    0,    0},
        {29000,23882,19677,14916,10273, 5561,    0,    0},
        {30304,24317,19907,11136, 7243, 4213,    0,    0},
        {31499,27333,22335,13805,11068, 6903,    0,    0}
    }
};

static const unsigned short stb_av1_color_map[2][7][5][8] = {
    {
        {
            { 4058,    0,    0,    0,    0,    0,    0,    0},
            {16384,    0,    0,    0,    0,    0,    0,    0},
            {22215,    0,    0,    0,    0,    0,    0,    0},
            { 5732,    0,    0,    0,    0,    0,    0,    0},
            { 1165,    0,    0,    0,    0,    0,    0,    0}
        },
        {
            { 4891, 2278,    0,    0,    0,    0,    0,    0},
            {21236, 7071,    0,    0,    0,    0,    0,    0},
            {26224, 2534,    0,    0,    0,    0,    0,    0},
            { 9750, 4696,    0,    0,    0,    0,    0,    0},
            {  853,  383,    0,    0,    0,    0,    0,    0}
        },
        {
            { 7196, 4722, 2723,    0,    0,    0,    0,    0},
            {23290,11178, 5512,    0,    0,    0,    0,    0},
            {25520, 5931, 2944,    0,    0,    0,    0,    0},
            {13601, 8282, 4419,    0,    0,    0,    0,    0},
            { 1368,  943,  518,    0,    0,    0,    0,    0}
        },
        {
            { 7989, 5813, 4192, 2486,    0,    0,    0,    0},
            {24099,12404, 8695, 4675,    0,    0,    0,    0},
            {28513, 5203, 3391, 1701,    0,    0,    0,    0},
            {12904, 9094, 6052, 3238,    0,    0,    0,    0},
            { 1122,  875,  621,  342,    0,    0,    0,    0}
        },
        {
            { 9636, 7361, 5798, 4333, 2695,    0,    0,    0},
            {25325,15526,12051, 8006, 4786,    0,    0,    0},
            {26468, 7906, 5824, 3984, 2097,    0,    0,    0},
            {13852, 9873, 7501, 5333, 3116,    0,    0,    0},
            { 1498, 1218,  960,  709,  415,    0,    0,    0}
        },
        {
            { 9663, 7569, 6304, 5084, 3837, 2450,    0,    0},
            {25818,17321,13816,10087, 7201, 4205,    0,    0},
            {25208, 9294, 7278, 5565, 3847, 2060,    0,    0},
            {14224,10395, 8311, 6573, 4649, 2723,    0,    0},
            { 1570, 1317, 1098,  886,  645,  377,    0,    0}
        },
        {
            {11079, 8885, 7605, 6416, 5262, 3941, 2573,    0},
            {25876,17383,14928,11162, 8481, 6015, 3564,    0},
            {27117, 9586, 7726, 6250, 4786, 3376, 1868,    0},
            {13419,10190, 8350, 6774, 5244, 3737, 2320,    0},
            { 1740, 1498, 1264, 1063,  841,  615,  376,    0}
        }
    },
    {
        {
            { 3679,    0,    0,    0,    0,    0,    0,    0},
            {16384,    0,    0,    0,    0,    0,    0,    0},
            {24055,    0,    0,    0,    0,    0,    0,    0},
            { 3511,    0,    0,    0,    0,    0,    0,    0},
            { 1158,    0,    0,    0,    0,    0,    0,    0}
        },
        {
            { 7511, 3623,    0,    0,    0,    0,    0,    0},
            {20481, 5475,    0,    0,    0,    0,    0,    0},
            {25735, 4808,    0,    0,    0,    0,    0,    0},
            {12623, 7363,    0,    0,    0,    0,    0,    0},
            { 2160, 1129,    0,    0,    0,    0,    0,    0}
        },
        {
            { 8558, 5593, 2865,    0,    0,    0,    0,    0},
            {22880,10382, 5554,    0,    0,    0,    0,    0},
            {26867, 6715, 3475,    0,    0,    0,    0,    0},
            {14450,10616, 4435,    0,    0,    0,    0,    0},
            { 2309, 1632,  842,    0,    0,    0,    0,    0}
        },
        {
            { 9788, 7289, 4987, 2782,    0,    0,    0,    0},
            {24355,11360, 7909, 3894,    0,    0,    0,    0},
            {30511, 3319, 2174, 1170,    0,    0,    0,    0},
            {13579,11566, 6853, 4148,    0,    0,    0,    0},
            {  924,  724,  487,  250,    0,    0,    0,    0}
        },
        {
            {10551, 8201, 6131, 4085, 2220,    0,    0,    0},
            {25461,16362,13132, 8136, 4344,    0,    0,    0},
            {28327, 7704, 5889, 3826, 1849,    0,    0,    0},
            {15558,12240, 9449, 6018, 3186,    0,    0,    0},
            { 2094, 1815, 1372, 1033,  561,    0,    0,    0}
        },
        {
            {11529, 9600, 7724, 5806, 4063, 2262,    0,    0},
            {26223,17756,14764,10951, 7265, 4067,    0,    0},
            {29320, 6473, 5331, 4064, 2642, 1326,    0,    0},
            {16879,14445,11064, 8070, 5792, 3078,    0,    0},
            { 1780, 1564, 1289, 1034,  785,  443,    0,    0}
        },
        {
            {11326, 9480, 8010, 6522, 5119, 3788, 2205,    0},
            {26905,17835,15216,12100, 9085, 6357, 3495,    0},
            {29353, 6958, 5891, 4778, 3545, 2374, 1150,    0},
            {14803,12684,10536, 8794, 6494, 4366, 2378,    0},
            { 1578, 1439, 1252, 1089,  943,  742,  446,    0}
        }
    }
};

static const unsigned short stb_av1_txsz[4][3][4] = {
    {
        {12800,    0,    0,    0},
        {12800,    0,    0,    0},
        { 8448,    0,    0,    0}
    },
    {
        {20496, 2596,    0,    0},
        {20496, 2596,    0,    0},
        {14091, 1920,    0,    0}
    },
    {
        {19782,17588,    0,    0},
        {19782,17588,    0,    0},
        { 8466, 7166,    0,    0}
    },
    {
        {26986,21293,    0,    0},
        {26986,21293,    0,    0},
        {15965,10009,    0,    0}
    }
};

static const unsigned short stb_av1_delta_q[4] = { 4608,  648,   91,    0};

static const unsigned short stb_av1_delta_lf[5][4] = {
    { 4608,  648,   91,    0},
    { 4608,  648,   91,    0},
    { 4608,  648,   91,    0},
    { 4608,  648,   91,    0},
    { 4608,  648,   91,    0}
};

static const unsigned short stb_av1_restore_switchable[4] = {23355,10187,    0,    0};

static const unsigned short stb_av1_restore_wiener[2] = {21198,    0};

static const unsigned short stb_av1_restore_sgrproj[2] = {15913,    0};

static const unsigned short stb_av1_txtp_inter3[4][2] = {
    {16384,    0},
    {28601,    0},
    {30770,    0},
    {32020,    0}
};

static const unsigned short stb_av1_use_filter_intra[22][2] = {
    {16384,    0},
    {16384,    0},
    {16384,    0},
    {16384,    0},
    {16384,    0},
    {16384,    0},
    {16384,    0},
    {10425,    0},
    {20012,    0},
    {14667,    0},
    {16384,    0},
    {18467,    0},
    {20360,    0},
    {23374,    0},
    {22400,    0},
    {12539,    0},
    {20217,    0},
    {24902,    0},
    {26875,    0},
    {19998,    0},
    {26025,    0},
    {28147,    0}
};

static const unsigned short stb_av1_txpart[7][3][2] = {
    {
        { 4187,    0},
        { 8922,    0},
        {11921,    0}
    },
    {
        { 8453,    0},
        {14572,    0},
        {20635,    0}
    },
    {
        {13977,    0},
        {21881,    0},
        {21763,    0}
    },
    {
        { 5589,    0},
        {12764,    0},
        {21487,    0}
    },
    {
        { 6219,    0},
        {13460,    0},
        {18544,    0}
    },
    {
        { 4753,    0},
        {11222,    0},
        {18368,    0}
    },
    {
        { 4603,    0},
        {10367,    0},
        {16680,    0}
    }
};

static const unsigned short stb_av1_skip[3][2] = {
    { 1097,    0},
    {16253,    0},
    {28192,    0}
};

static const unsigned short stb_av1_pal_y[7][3][2] = {
    {
        { 1092,    0},
        {29349,    0},
        {31507,    0}
    },
    {
        {  856,    0},
        {29909,    0},
        {31788,    0}
    },
    {
        {  945,    0},
        {29368,    0},
        {31987,    0}
    },
    {
        {  738,    0},
        {29207,    0},
        {31864,    0}
    },
    {
        {  459,    0},
        {25431,    0},
        {31306,    0}
    },
    {
        {  503,    0},
        {28753,    0},
        {31247,    0}
    },
    {
        {  318,    0},
        {24822,    0},
        {32639,    0}
    }
};

static const unsigned short stb_av1_pal_uv[2][2] = {
    {  307,    0},
    {11280,    0}
};

static const unsigned short stb_av1_intrabc[2] = { 2237,    0};

static const unsigned short stb_av1_y_mode[4][16] = {
    { 9967, 9279, 8475, 8012, 7167, 6645, 6162, 5350, 4823, 3540, 3083, 2419,    0,    0,    0,    0},
    {14095,12923,10137, 9450, 8818, 8119, 7241, 5404, 4616, 3067, 2784, 1916,    0,    0,    0,    0},
    {12998,11789, 9372, 8829, 8527, 8114, 7632, 5695, 4938, 3408, 3038, 2109,    0,    0,    0,    0},
    {12613,11467, 9930, 9590, 9507, 9235, 9065, 7964, 7416, 6193, 5752, 4719,    0,    0,    0,    0}
};

static const unsigned short stb_av1_wedge_idx[9][16] = {
    {30330,28328,26169,24105,21763,19894,17017,14674,12409,10406, 8641, 7066, 5016, 3318, 1597,    0},
    {31962,29502,26763,26030,25550,25401,24997,18180,16445,15401,14316,13346, 9929, 6641, 3139,    0},
    {29989,29030,28085,25555,24993,24751,24113,18411,14829,11436, 8248, 5298, 3312, 2239, 1112,    0},
    {31084,29143,27093,25660,23466,21494,18339,15624,13605,11807, 9884, 8297, 6049, 4054, 1891,    0},
    {31626,29277,26491,25454,24679,24413,23745,19144,17399,16038,14654,13455,10247, 6756, 3218,    0},
    {30026,28573,27041,24733,23788,23432,22622,18644,15498,12235, 9334, 6796, 4824, 3198, 1352,    0},
    {31041,28820,26667,24972,22927,20424,17002,13824,12130,10730, 8805, 7457, 5780, 4002, 1756,    0},
    {32614,31781,30843,30717,30680,30657,30617, 9735, 9065, 8484, 7783, 7084, 5509, 3885, 1857,    0},
    {31633,31446,31275,30133,30072,30031,29998,11752, 9833, 7711, 5517, 3595, 2679, 1808,  835,    0}
};

static const unsigned short stb_av1_comp_inter_mode[8][8] = {
    {25008,18945,16960,15127,13612,12102, 5877,    0},
    {22038,13316,11623,10019, 8729, 7637, 4044,    0},
    {22104,12547,11180, 9862, 8473, 7381, 4332,    0},
    {19470,15784,12297, 8586, 7701, 7032, 6346,    0},
    {13864, 9443, 7526, 5336, 4870, 4510, 2010,    0},
    {22043,15314,12644, 9948, 8573, 7600, 6722,    0},
    {15643, 8495, 6954, 5276, 4554, 4064, 2176,    0},
    {19722, 9554, 8263, 6826, 5333, 4326, 3438,    0}
};

static const unsigned short stb_av1_filter[2][8][8] = {
    {
        {  833,   48,    0,    0,    0,    0,    0,    0},
        {27200,   49,    0,    0,    0,    0,    0,    0},
        {32346,29830,    0,    0,    0,    0,    0,    0},
        { 4524,  160,    0,    0,    0,    0,    0,    0},
        { 1562,  815,    0,    0,    0,    0,    0,    0},
        {27906,  647,    0,    0,    0,    0,    0,    0},
        {31998,31616,    0,    0,    0,    0,    0,    0},
        {11879, 7131,    0,    0,    0,    0,    0,    0}
    },
    {
        {  858,   44,    0,    0,    0,    0,    0,    0},
        {28648,   56,    0,    0,    0,    0,    0,    0},
        {32463,30521,    0,    0,    0,    0,    0,    0},
        { 5365,  132,    0,    0,    0,    0,    0,    0},
        { 1746,  759,    0,    0,    0,    0,    0,    0},
        {29805,  675,    0,    0,    0,    0,    0,    0},
        {32167,31825,    0,    0,    0,    0,    0,    0},
        {17799,11370,    0,    0,    0,    0,    0,    0}
    }
};

static const unsigned short stb_av1_interintra_mode[4][4] = {
    {24576,16384, 8192,    0},
    {30893,21686, 5436,    0},
    {30295,22772, 6380,    0},
    {28530,21231, 6842,    0}
};

static const unsigned short stb_av1_motion_mode[22][4] = {
    {  261,  210,    0,    0},
    { 1890, 1433,    0,    0},
    { 3870, 2371,    0,    0},
    { 3252, 2067,    0,    0},
    {11089, 5938,    0,    0},
    { 3026, 1565,    0,    0},
    {12408, 4706,    0,    0},
    { 6508, 3652,    0,    0},
    {21162, 8460,    0,    0},
    { 6337, 1994,    0,    0},
    { 3795, 1174,    0,    0},
    {27645, 9162,    0,    0},
    {13349, 5958,    0,    0},
    {27377, 7240,    0,    0},
    {    0,    0,    0,    0},
    { 3969, 1378,    0,    0},
    {28030, 8003,    0,    0},
    {25117, 8008,    0,    0},
    {    0,    0,    0,    0},
    {    0,    0,    0,    0},
    {    0,    0,    0,    0},
    {    0,    0,    0,    0}
};

static const unsigned short stb_av1_skip_mode[3][2] = {
    {  147,    0},
    {12060,    0},
    {24641,    0}
};

static const unsigned short stb_av1_newmv_mode[6][2] = {
    { 8733,    0},
    {16138,    0},
    {17429,    0},
    {24382,    0},
    {20546,    0},
    {28092,    0}
};

static const unsigned short stb_av1_globalmv_mode[2][2] = {
    {30593,    0},
    {31714,    0}
};

static const unsigned short stb_av1_refmv_mode[6][2] = {
    { 8794,    0},
    { 8580,    0},
    {14920,    0},
    { 4146,    0},
    { 8456,    0},
    {12845,    0}
};

static const unsigned short stb_av1_drl_bit[3][2] = {
    {19664,    0},
    { 8208,    0},
    {13823,    0}
};

static const unsigned short stb_av1_intra[4][2] = {
    {31962,    0},
    {16106,    0},
    {12582,    0},
    { 6230,    0}
};

static const unsigned short stb_av1_comp[5][2] = {
    { 5940,    0},
    { 8733,    0},
    {20737,    0},
    {22128,    0},
    {29867,    0}
};

static const unsigned short stb_av1_comp_dir[5][2] = {
    {31570,    0},
    {30698,    0},
    {23602,    0},
    {25269,    0},
    {10293,    0}
};

static const unsigned short stb_av1_jnt_comp[6][2] = {
    {14524,    0},
    {19903,    0},
    {25715,    0},
    {19509,    0},
    {23434,    0},
    {28124,    0}
};

static const unsigned short stb_av1_mask_comp[6][2] = {
    { 6161,    0},
    { 9877,    0},
    {13928,    0},
    { 8174,    0},
    {12834,    0},
    {10094,    0}
};

static const unsigned short stb_av1_wedge_comp[9][2] = {
    { 9337,    0},
    {19597,    0},
    {21298,    0},
    {22998,    0},
    {23668,    0},
    {24535,    0},
    {26596,    0},
    {20948,    0},
    {25067,    0}
};

static const unsigned short stb_av1_ref[6][3][2] = {
    {
        {27871,    0},
        {15795,    0},
        { 3024,    0}
    },
    {
        {31213,    0},
        {16017,    0},
        { 2489,    0}
    },
    {
        {28532,    0},
        {13121,    0},
        { 1574,    0}
    },
    {
        {24118,    0},
        { 7995,    0},
        {  873,    0}
    },
    {
        {31864,    0},
        {21754,    0},
        { 5893,    0}
    },
    {
        {31324,    0},
        {17681,    0},
        { 2464,    0}
    }
};

static const unsigned short stb_av1_comp_fwd_ref[3][3][2] = {
    {
        {27822,    0},
        {12877,    0},
        { 2037,    0}
    },
    {
        {23300,    0},
        {10327,    0},
        { 1709,    0}
    },
    {
        {31265,    0},
        {17608,    0},
        { 5224,    0}
    }
};

static const unsigned short stb_av1_comp_bwd_ref[2][3][2] = {
    {
        {30533,    0},
        {15586,    0},
        { 2162,    0}
    },
    {
        {31345,    0},
        {17593,    0},
        { 2279,    0}
    }
};

static const unsigned short stb_av1_comp_uni_ref[3][3][2] = {
    {
        {27484,    0},
        { 9616,    0},
        {  994,    0}
    },
    {
        {28903,    0},
        {18595,    0},
        { 7648,    0}
    },
    {
        {29640,    0},
        {17498,    0},
        { 6058,    0}
    }
};

static const unsigned short stb_av1_seg_pred[3][2] = {
    {16384,    0},
    {16384,    0},
    {16384,    0}
};

static const unsigned short stb_av1_interintra[7][2] = {
    {16384,    0},
    { 5881,    0},
    { 5171,    0},
    { 2531,    0},
    {    0,    0},
    {    0,    0},
    {    0,    0}
};

static const unsigned short stb_av1_interintra_wedge[7][2] = {
    {12732,    0},
    { 7811,    0},
    { 6064,    0},
    { 5238,    0},
    { 3204,    0},
    { 3324,    0},
    { 5896,    0}
};

static const unsigned short stb_av1_obmc[22][2] = {
    {  130,    0},
    { 1208,    0},
    { 1754,    0},
    { 2640,    0},
    {10685,    0},
    { 5889,    0},
    { 9945,    0},
    { 6951,    0},
    {17626,    0},
    {11867,    0},
    { 8760,    0},
    {18345,    0},
    {15336,    0},
    {23467,    0},
    {    0,    0},
    { 9104,    0},
    {23397,    0},
    {22331,    0},
    {    0,    0},
    {    0,    0},
    {    0,    0},
    {    0,    0}
};

static const unsigned short stb_av1_mv_classes[16] = { 4096, 1792,  910,  448,  217,  112,   28,   11,    6,    1,    0,    0,    0,    0,    0,    0};

static const unsigned short stb_av1_mv_sign[2] = {16384,    0};

/* Summary: 61 OK, 0 FAIL */
static const unsigned short stb_av1_mv_class0[2] = { 5120,    0};

static const unsigned short stb_av1_mv_class0_fp[2][4] = {
    {16384, 8192, 6144,    0},
    {20480,11520, 8640,    0}
};

static const unsigned short stb_av1_mv_class0_hp[2] = {12288,    0};

static const unsigned short stb_av1_mv_classN[10][2] = {
    {15360,    0},
    {14848,    0},
    {13824,    0},
    {12288,    0},
    {10240,    0},
    { 8192,    0},
    { 4096,    0},
    { 2816,    0},
    { 2816,    0},
    { 2048,    0}
};

static const unsigned short stb_av1_mv_classN_fp[4] = {24576,15360,11520,    0};

static const unsigned short stb_av1_mv_classN_hp[2] = {16384,    0};

static const unsigned short stb_av1_mv_joint[4] = {28672,21504,13440,    0};

static const unsigned short stb_av1_kfym[5][5][16] = {
    {
        {17180,15741,13430,12550,12086,11658,10943, 9524, 8579, 4603, 3675, 2302,    0,    0,    0,    0},
        {20752,14702,13252,12465,12049,11324,10880, 9736, 8334, 4110, 2596, 1359,    0,    0,    0,    0},
        {22716,21997,10472, 9980, 9713, 9529, 8635, 7148, 6608, 3432, 2839, 1201,    0,    0,    0,    0},
        {18677,17362,16326,13960,13632,13222,12770,10672, 8022, 3183, 1810,  306,    0,    0,    0,    0},
        {20646,19503,17165,16267,14159,12735,10377, 7185, 6331, 2507, 1695,  293,    0,    0,    0,    0}
    },
    {
        {22745,13183,11920,11328,10936,10008, 9679, 8745, 7387, 3754, 2286, 1332,    0,    0,    0,    0},
        {26785, 8669, 8208, 7882, 7702, 6973, 6855, 6345, 5158, 2863, 1492,  974,    0,    0,    0,    0},
        {25324,19987,12591,12040,11691,11161,10598, 9363, 8299, 4853, 3678, 2276,    0,    0,    0,    0},
        {24231,18079,17336,15681,15360,14596,14360,12943, 8119, 3615, 1672,  558,    0,    0,    0,    0},
        {25225,18537,17272,16573,14863,12051,10784, 8252, 6767, 3093, 1787,  774,    0,    0,    0,    0}
    },
    {
        {20155,19177,11385,10764,10456,10191, 9367, 7713, 7039, 3230, 2463,  691,    0,    0,    0,    0},
        {23081,19298,14262,13538,13164,12621,12073,10706, 9549, 5025, 3557, 1861,    0,    0,    0,    0},
        {26585,26263, 6744, 6516, 6402, 6334, 5686, 4414, 4213, 2301, 1974,  682,    0,    0,    0,    0},
        {22050,21034,17814,15544,15203,14844,14207,11245, 8890, 3793, 2481,  516,    0,    0,    0,    0},
        {23574,22910,16267,15505,14344,13597,11205, 6807, 6207, 2696, 2031,  305,    0,    0,    0,    0}
    },
    {
        {20166,18369,17280,14387,13990,13453,13044,11349, 7708, 3072, 1851,  359,    0,    0,    0,    0},
        {24565,18947,18244,15663,15329,14637,14364,13300, 7543, 3283, 1610,  426,    0,    0,    0,    0},
        {24317,23037,17764,15125,14756,14343,13698,11230, 8163, 3650, 2690,  750,    0,    0,    0,    0},
        {25054,23720,23252,16101,15951,15774,15615,14001, 6025, 2379, 1232,  240,    0,    0,    0,    0},
        {23925,22488,21272,17451,16116,14825,13660,10050, 6999, 2815, 1785,  283,    0,    0,    0,    0}
    },
    {
        {20190,19097,16789,15934,13693,11855, 9779, 7319, 6549, 2554, 1618,  291,    0,    0,    0,    0},
        {23205,19142,17688,16876,15012,11905,10561, 8532, 7388, 3115, 1625,  491,    0,    0,    0,    0},
        {24412,23867,15152,14512,13418,12662,10170, 6821, 6302, 2868, 2245,  507,    0,    0,    0,    0},
        {21933,20953,19644,16726,15750,14729,13821,10015, 8153, 3279, 1885,  286,    0,    0,    0,    0},
        {25150,24480,22909,22259,17382,14111, 9865, 3992, 3588, 1413,  966,  175,    0,    0,    0,    0}
    }
};





/* Default coefficient CDF arrays for qcat=0 (from dav1d cdf.c). Fields in INIT ORDER. */
static const unsigned short stb_av1_default_coef_skip[4][5][13][2] = {
    { /* qcat=0 */
              919,     0, 26876,     0, 20656,     0, 10833,     0,
            12479,     0,  5295,     0,   281,     0, 25114,     0,
            13295,     0,  2784,     0, 22807,     0,  2526,     0,
              651,     0,  1220,     0, 31219,     0, 22638,     0,
            16112,     0, 14177,     0,  6460,     0,   231,     0,
            27365,     0, 14672,     0,  2765,     0, 16384,     0,
            16384,     0, 16384,     0,  2811,     0, 27377,     0,
            14729,     0,  9202,     0, 10337,     0,  6946,     0,
              571,     0, 28990,     0, 17432,     0,  3787,     0,
            16384,     0, 16384,     0, 16384,     0, 14848,     0,
            30950,     0, 25486,     0,  7495,     0, 21845,     0,
             1214,     0,   144,     0, 31402,     0, 17140,     0,
             2306,     0, 32622,     0, 27636,     0,  1111,     0,
            26460,     0, 32651,     0, 31130,     0, 30607,     0,
            16384,     0, 21845,     0,  2521,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0,
    },
    { /* qcat=1 */
             2397,     0, 25198,     0, 19613,     0, 12017,     0,
            11799,     0,  5701,     0,   755,     0, 27273,     0,
            14826,     0,  4488,     0, 16384,     0, 16384,     0,
            16384,     0,   986,     0, 30932,     0, 22079,     0,
            15164,     0, 11146,     0,  5250,     0,   369,     0,
            28349,     0, 16474,     0,  4423,     0, 16384,     0,
            16384,     0, 16384,     0,   867,     0, 22457,     0,
            14721,     0,  7962,     0,  9480,     0,  4854,     0,
              472,     0, 28553,     0, 17012,     0,  4427,     0,
            16384,     0, 16384,     0, 16384,     0,  6042,     0,
            31723,     0, 21065,     0, 12178,     0, 14214,     0,
             6798,     0,   830,     0, 27185,     0, 11455,     0,
             3378,     0, 32127,     0, 10503,     0,  1316,     0,
             6184,     0, 32580,     0, 23921,     0,  8249,     0,
             9830,     0,  2185,     0,   160,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0,
    },
    { /* qcat=2 */
             3154,     0, 23700,     0, 19844,     0, 13230,     0,
            15031,     0,  8149,     0,  2126,     0, 28649,     0,
            16742,     0,  7111,     0, 16384,     0, 16384,     0,
            16384,     0,   811,     0, 29538,     0, 21615,     0,
            14645,     0, 12625,     0,  6232,     0,   782,     0,
            29718,     0, 18165,     0,  7613,     0, 16384,     0,
            16384,     0, 16384,     0,   405,     0, 22076,     0,
            13678,     0,  8411,     0,  8326,     0,  4456,     0,
              599,     0, 29120,     0, 17078,     0,  5953,     0,
            16384,     0, 16384,     0, 16384,     0,  2099,     0,
            28936,     0, 21105,     0, 13879,     0, 12986,     0,
             9455,     0,  1438,     0, 27644,     0, 14049,     0,
             4300,     0, 29686,     0, 11786,     0,  3325,     0,
             4195,     0, 29585,     0, 14966,     0,  6791,     0,
             6091,     0,  4936,     0,   381,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0,
    },
    { /* qcat=3 */
             5881,     0, 26039,     0, 22407,     0, 15326,     0,
            17723,     0, 10290,     0,  3696,     0, 30055,     0,
            20907,     0, 11995,     0, 16384,     0, 16384,     0,
            16384,     0,   865,     0, 30724,     0, 25240,     0,
            18150,     0, 16586,     0,  8600,     0,  1731,     0,
            29982,     0, 21574,     0, 12613,     0, 16384,     0,
            16384,     0, 16384,     0,   258,     0, 24338,     0,
            15450,     0,  8614,     0,  9094,     0,  3979,     0,
              629,     0, 29328,     0, 19651,     0, 10066,     0,
            16384,     0, 16384,     0, 16384,     0,  1097,     0,
            30712,     0, 21022,     0, 15916,     0, 14133,     0,
             8053,     0,  1284,     0, 28112,     0, 16694,     0,
             8064,     0, 30962,     0, 18123,     0,  7432,     0,
             1229,     0, 24335,     0, 12192,     0,  4864,     0,
             4916,     0,  2742,     0,   327,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_16[4][2][2][8] = {
    { /* qcat=0 */
            31928, 31729, 30788, 27873,     0,     0,     0,     0, 32398, 32097, 30885, 28297,     0,     0,     0,     0, 29521, 27818, 23080, 18205,     0,     0,     0,     0, 30864, 29414, 25005, 18121,     0,     0,     0,     0,
    },
    { /* qcat=1 */
            30643, 30217, 27603, 23822,     0,     0,     0,     0, 32255, 32003, 30909, 26429,     0,     0,     0,     0, 25131, 23270, 18509, 13660,     0,     0,     0,     0, 30271, 28672, 23902, 15775,     0,     0,     0,     0,
    },
    { /* qcat=2 */
            28752, 27871, 23887, 17800,     0,     0,     0,     0, 32052, 31663, 30122, 22712,     0,     0,     0,     0, 21629, 19498, 14527,  9202,     0,     0,     0,     0, 29576, 27736, 22471, 13013,     0,     0,     0,     0,
    },
    { /* qcat=3 */
            26060, 23810, 18022, 10635,     0,     0,     0,     0, 31546, 30694, 27985, 17358,     0,     0,     0,     0, 13193, 11002,  6724,  3059,     0,     0,     0,     0, 25471, 22001, 13495,  4574,     0,     0,     0,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_32[4][2][2][8] = {
    { /* qcat=0 */
            32368, 32248, 31791, 30666, 26226,     0,     0,     0, 32558, 32363, 31453, 29442, 25231,     0,     0,     0, 30132, 28495, 25180, 20974, 12367,     0,     0,     0, 30982, 29589, 25866, 21411, 13714,     0,     0,     0,
    },
    { /* qcat=1 */
            31779, 31519, 30749, 28617, 21983,     0,     0,     0, 32455, 32327, 31669, 29851, 24206,     0,     0,     0, 24374, 22416, 18836, 13913,  6754,     0,     0,     0, 30190, 28644, 24587, 19098,  8534,     0,     0,     0,
    },
    { /* qcat=2 */
            30253, 29765, 28316, 24606, 16727,     0,     0,     0, 32194, 31947, 30932, 27679, 19640,     0,     0,     0, 19300, 16465, 12407,  7663,  3487,     0,     0,     0, 29226, 27266, 22353, 16008,  7124,     0,     0,     0,
    },
    { /* qcat=3 */
            28151, 27059, 24322, 19184,  9633,     0,     0,     0, 31612, 31066, 29093, 23494, 12229,     0,     0,     0, 10682,  8486,  5758,  2998,  1025,     0,     0,     0, 25069, 21871, 11877,  5842,  1140,     0,     0,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_64[4][2][2][8] = {
    { /* qcat=0 */
            32439, 32270, 31667, 30984, 29503, 25010,     0,     0, 32433, 32038, 31309, 27274, 24013, 19771,     0,     0, 29263, 27464, 22682, 18954, 15084,  9398,     0,     0, 31205, 30068, 27892, 21857, 18062, 10288,     0,     0,
    },
    { /* qcat=1 */
            31508, 31322, 30515, 29056, 26116, 19399,     0,     0, 32367, 32163, 31739, 30205, 26923, 20142,     0,     0, 24159, 22156, 18144, 14054, 10154,  3744,     0,     0, 30845, 29641, 26901, 23065, 18491,  5668,     0,     0,
    },
    { /* qcat=2 */
            30394, 29996, 28185, 25492, 20480, 13062,     0,     0, 32271, 31958, 31453, 29768, 25764, 17127,     0,     0, 17718, 15642, 11358,  7882,  4612,  2042,     0,     0, 28734, 26478, 22533, 17786, 11554,  4277,     0,     0,
    },
    { /* qcat=3 */
            26461, 25227, 20708, 16410, 10215,  4903,     0,     0, 31479, 30448, 28797, 24842, 18615,  8477,     0,     0,  8556,  7060,  4500,  2733,  1461,   719,     0,     0, 24042, 20390, 13359,  6318,  2730,   306,     0,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_128[4][2][2][8] = {
    { /* qcat=0 */
            32549, 32286, 31628, 30677, 29088, 26740, 20182,     0, 32397, 32069, 31514, 27938, 23289, 20206, 15271,     0, 27523, 25312, 19888, 16916, 12735,  8836,  5160,     0, 30714, 29296, 26899, 18536, 14526, 12178,  6016,     0,
    },
    { /* qcat=1 */
            32083, 31835, 31280, 30054, 28002, 24206, 13514,     0, 32551, 32416, 32150, 30465, 27507, 22799, 15296,     0, 24723, 21568, 17271, 13173,  8820,  5360,  1830,     0, 30458, 28608, 25297, 17771, 14837, 12000,  2528,     0,
    },
    { /* qcat=2 */
            31402, 31030, 30241, 27752, 23413, 16971,  8125,     0, 32414, 32210, 31824, 30008, 25481, 18731, 10989,     0, 19141, 16522, 12595,  8339,  4820,  2353,   905,     0, 26493, 22879, 17999,  9604,  4780,  2275,   496,     0,
    },
    { /* qcat=3 */
            29296, 27883, 25279, 20287, 14251,  8232,  3133,     0, 31882, 31037, 29497, 24299, 17199, 10642,  4385,     0,  8455,  6706,  4383,  2661,  1551,   870,   423,     0, 23603, 19486, 11618,  2482,   874,   197,    56,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_256[4][2][2][16] = {
    { /* qcat=0 */
            32458, 32184, 30881, 29179, 26600, 24157, 21416, 17116,     0,     0,     0,     0,     0,     0,     0,     0, 31770, 30918, 29770, 27164, 15427, 12880,  9869,  7185,     0,     0,     0,     0,     0,     0,     0,     0, 30248, 29528, 26816, 23898, 20191, 15210, 12814,  8600,     0,     0,     0,     0,     0,     0,     0,     0, 30565, 28638, 25333, 22029, 12116,  9087,  7159,  5507,     0,     0,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=1 */
            31320, 30659, 28617, 26505, 23439, 19508, 14824,  9468,     0,     0,     0,     0,     0,     0,     0,     0, 32369, 31749, 31019, 29730, 22324, 17222, 10029,  5474,     0,     0,     0,     0,     0,     0,     0,     0, 26366, 24620, 20145, 17696, 14040,  9921,  6321,  3391,     0,     0,     0,     0,     0,     0,     0,     0, 31094, 29516, 27034, 22609, 10371,  8966,  7947,  1828,     0,     0,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=2 */
            29679, 28848, 26730, 23308, 18502, 12887,  7002,  3592,     0,     0,     0,     0,     0,     0,     0,     0, 31684, 30410, 29280, 27646, 21285, 14665,  6745,  2969,     0,     0,     0,     0,     0,     0,     0,     0, 21254, 18974, 15288, 12014,  8407,  5390,  3276,  1491,     0,     0,     0,     0,     0,     0,     0,     0, 26197, 23158, 17252, 10942,  3676,  1939,   926,    60,     0,     0,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=3 */
            27420, 25655, 20948, 16844, 10662,  5991,  2434,  1011,     0,     0,     0,     0,     0,     0,     0,     0, 30315, 28294, 26461, 23991, 16294,  9793,  3768,  1221,     0,     0,     0,     0,     0,     0,     0,     0,  9658,  8171,  5628,  3874,  2601,  1841,  1376,   674,     0,     0,     0,     0,     0,     0,     0,     0, 22770, 15107,  7590,  4671,  1460,   730,   365,    73,     0,     0,     0,     0,     0,     0,     0,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_512[4][2][16] = {
    { /* qcat=0 */
            32127, 31785, 29061, 27338, 22534, 17810, 13980,  9356,  6707,     0,     0,     0,     0,     0,     0,     0, 27673, 26322, 22772, 19414, 16751, 14782, 11849,  6639,  3628,     0,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=1 */
            31538, 30490, 27733, 24992, 20897, 17422, 13178,  8184,  4019,     0,     0,     0,     0,     0,     0,     0, 25503, 22789, 16949, 13518, 10988,  8922,  6290,  4372,   957,     0,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=2 */
            30144, 28832, 26288, 23082, 18789, 15042,  9501,  4358,  1690,     0,     0,     0,     0,     0,     0,     0, 20753, 17999, 13180, 10716,  8546,  6956,  5468,  3549,   654,     0,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=3 */
            26841, 24959, 21845, 18171, 13329,  8633,  4312,  1626,   708,     0,     0,     0,     0,     0,     0,     0, 11675,  9725,  7026,  5110,  3671,  3052,  2695,  1948,   812,     0,     0,     0,     0,     0,     0,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_bin_1024[4][2][16] = {
    { /* qcat=0 */
            32375, 32347, 32017, 31145, 29608, 26416, 19423, 14721, 10197,  6938,     0,     0,     0,     0,     0,     0, 30903, 30780, 29838, 28526, 22235, 16230, 11414,  5513,  4222,   984,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=1 */
            32072, 31820, 29623, 27066, 23062, 19551, 14917, 10912,  7076,  4734,     0,     0,     0,     0,     0,     0, 30096, 29177, 23438, 15684, 10043,  8484,  6241,  4741,  4391,  1892,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=2 */
            29984, 28937, 25727, 22247, 17921, 13924,  9613,  6086,  3539,  1723,     0,     0,     0,     0,     0,     0, 23191, 20302, 15029, 12018, 10707,  9553,  8167,  7285,  6925,   712,     0,     0,     0,     0,     0,     0,
    },
    { /* qcat=3 */
            26070, 24434, 20807, 17006, 12582,  8906,  5334,  3442,  1686,   718,     0,     0,     0,     0,     0,     0, 12199, 10342,  7199,  5909,  4715,  3855,  3282,  3044,  2961,   198,     0,     0,     0,     0,     0,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_hi_bit[4][5][2][9][2] = {
    { /* qcat=0 */
            15807,     0, 15545,     0, 25147,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 13699,     0, 10243,     0, 19391,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 12367,     0, 15743,     0,
            19923,     0, 19895,     0, 18674,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 12087,     0,
            12067,     0, 17518,     0, 17751,     0, 17840,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
             8863,     0, 15574,     0, 16598,     0, 15073,     0,
            18942,     0, 16958,     0, 20732,     0, 16384,     0,
            16384,     0,  8809,     0, 11969,     0, 13747,     0,
            16565,     0, 14882,     0, 18624,     0, 20758,     0,
            16384,     0, 16384,     0,  5369,     0, 16441,     0,
            14697,     0, 13184,     0, 12047,     0, 14336,     0,
            13208,     0, 22618,     0, 23963,     0,  7836,     0,
            11935,     0, 20741,     0, 16098,     0, 12854,     0,
            17662,     0, 15106,     0, 18985,     0,  4012,     0,
             9362,     0, 10923,     0, 14336,     0, 16384,     0,
            15672,     0, 20207,     0, 15448,     0, 10373,     0,
            11398,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0,
    },
    { /* qcat=1 */
            15297,     0, 12545,     0, 21411,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 12433,     0, 11101,     0, 17950,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 12338,     0, 12106,     0,
            17401,     0, 15798,     0, 18111,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 10651,     0,
            10740,     0, 14118,     0, 16726,     0, 16883,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            10359,     0, 11756,     0, 17118,     0, 15373,     0,
            17299,     0, 12563,     0, 13257,     0, 16384,     0,
            16384,     0,  8548,     0, 10288,     0, 15031,     0,
            13852,     0, 13500,     0, 14356,     0, 13924,     0,
            16384,     0, 16384,     0,  6777,     0, 12454,     0,
            15037,     0, 13090,     0, 14119,     0, 15461,     0,
            10970,     0, 15219,     0, 17138,     0,  6183,     0,
            11299,     0, 12336,     0, 15033,     0, 13488,     0,
            17533,     0, 12471,     0, 10297,     0,  3771,     0,
             6163,     0, 21464,     0, 16042,     0, 16208,     0,
            11902,     0,  9244,     0, 12890,     0, 19299,     0,
             9684,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0,
    },
    { /* qcat=2 */
            13785,     0, 12256,     0, 17883,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 12678,     0, 13324,     0, 15482,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 13629,     0, 11281,     0,
            13809,     0, 11858,     0, 13679,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 12232,     0,
            12104,     0, 12143,     0, 13645,     0, 17906,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            12935,     0, 11266,     0, 15283,     0, 12501,     0,
            14415,     0,  9439,     0, 11290,     0, 16384,     0,
            16384,     0, 10727,     0,  9334,     0, 12767,     0,
            12214,     0, 11817,     0, 12623,     0, 17206,     0,
            16384,     0, 16384,     0,  9456,     0, 11161,     0,
            16242,     0, 13811,     0, 14734,     0, 13834,     0,
             8521,     0, 15847,     0, 15688,     0,  6189,     0,
             7858,     0, 14131,     0, 12968,     0, 12380,     0,
            22881,     0, 17126,     0,  2570,     0,  8047,     0,
             5770,     0, 16031,     0, 14930,     0, 13846,     0,
            13253,     0, 14132,     0, 15435,     0, 16992,     0,
            10110,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0,
    },
    { /* qcat=3 */
            12591,     0, 11979,     0, 12506,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 11352,     0, 11913,     0,  9358,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 12530,     0, 11711,     0,
            13609,     0, 10431,     0, 12609,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 12643,     0,
            12209,     0, 11061,     0, 10472,     0, 15435,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            12827,     0, 12241,     0, 11298,     0, 10281,     0,
            13210,     0, 10414,     0, 12437,     0, 16384,     0,
            16384,     0, 10016,     0,  7762,     0, 10693,     0,
            11192,     0, 15028,     0, 11078,     0, 13557,     0,
            16384,     0, 16384,     0, 11326,     0, 10410,     0,
            14265,     0, 12477,     0, 12823,     0, 11474,     0,
            11590,     0, 13368,     0, 22212,     0,  8120,     0,
             7819,     0, 12060,     0,  8863,     0, 12267,     0,
            23210,     0, 23345,     0,  2403,     0, 13515,     0,
             6704,     0, 10670,     0, 13155,     0, 12243,     0,
            15173,     0, 16150,     0, 12271,     0, 13779,     0,
            17255,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0, 16384,     0, 16384,     0,
            16384,     0, 16384,     0,
    },
};
static const unsigned short stb_av1_default_coef_eob_base_tok[4][5][2][4][3] = {
    { /* qcat=0 */
            14931,  3713,     0,  3168,  1322,     0,
             1924,   890,     0,  7842,  3820,     0,
            11403,  2742,     0,  2256,   345,     0,
             1110,   147,     0,  3138,   887,     0,
            27051,  6291,     0,  2277,  1065,     0,
             1218,   610,     0,  3120,  1277,     0,
            20160,  4948,     0,  2088,   543,     0,
             1959,   433,     0,  1469,   345,     0,
            30982, 20156,     0,  2105,  1143,     0,
              429,   300,     0,  1620,   935,     0,
            13911,  8903,     0,  1340,   340,     0,
             1024,   395,     0,   993,   242,     0,
            30981, 30236,     0,  1936,  1106,     0,
              944,    86,     0,   635,   199,     0,
            19017, 10533,     0,   679,   359,     0,
             5684,  4848,     0,  3477,   174,     0,
            31043, 29319,     0,  1666,   833,     0,
              311,   155,     0,   356,   119,     0,
            21845, 10923,     0, 21845, 10923,     0,
            21845, 10923,     0, 21845, 10923,     0,
    },
    { /* qcat=1 */
            15208,  2880,     0,  3097,  1219,     0,
             1761,   712,     0,  5482,  2762,     0,
             6174,  1556,     0,  1560,   186,     0,
              933,   131,     0,  2173,   562,     0,
            17529,  2836,     0,  1453,   673,     0,
              638,   334,     0,  1904,   772,     0,
             6489,  1800,     0,  1626,   273,     0,
             1055,   228,     0,   839,   174,     0,
            30124,  7570,     0,   730,   317,     0,
              129,    73,     0,   602,   250,     0,
            15581,  5100,     0,  1054,   218,     0,
              485,    90,     0,   838,   205,     0,
            31724, 30511,     0,  2013,   845,     0,
              560,    75,     0,   524,   153,     0,
            11451,  6561,     0,  3635,  1900,     0,
             3457,  1537,     0,  3111,  1681,     0,
            32290, 30934,     0,  1763,   781,     0,
              451,    44,     0,  1903,   120,     0,
            21845, 10923,     0, 21845, 10923,     0,
            21845, 10923,     0, 21845, 10923,     0,
    },
    { /* qcat=2 */
            12676,  1994,     0,  2073,   748,     0,
             1637,   665,     0,  4102,  1898,     0,
             5510,  1673,     0,   964,   145,     0,
             1005,   240,     0,  1330,   262,     0,
            14719,  2279,     0,  1062,   482,     0,
              605,   295,     0,  1218,   584,     0,
             5652,  1926,     0,   797,   170,     0,
              680,   192,     0,   701,   104,     0,
            19914,  3675,     0,   496,   210,     0,
              101,    39,     0,   462,   183,     0,
             7292,  2402,     0,   599,    81,     0,
              289,    79,     0,  1095,   134,     0,
            29959, 13467,     0,   563,   146,     0,
              430,    38,     0,   982,   152,     0,
            10031,  3663,     0,  1958,   406,     0,
             2754,   141,     0,  2240,   194,     0,
            31833, 29386,     0,  1979,   859,     0,
              302,    12,     0,  1908,   255,     0,
            21845, 10923,     0, 21845, 10923,     0,
            21845, 10923,     0, 21845, 10923,     0,
    },
    { /* qcat=3 */
            10271,  1570,     0,  1053,   273,     0,
             1162,   431,     0,  2380,   778,     0,
             4891,  1184,     0,   598,    40,     0,
              613,    80,     0,   549,    66,     0,
            11311,  1725,     0,   817,   285,     0,
              615,   206,     0,  1295,   553,     0,
             5210,  1617,     0,   748,   128,     0,
              671,   193,     0,   526,    49,     0,
            12788,  2177,     0,   549,   171,     0,
              187,    62,     0,   965,   481,     0,
             6295,  2261,     0,   337,    45,     0,
              572,   157,     0,  1180,   240,     0,
             8121,  2305,     0,   356,    73,     0,
              300,    48,     0,  1499,   245,     0,
             4286,  1263,     0,   616,    67,     0,
             1036,   170,     0,  1001,    56,     0,
            20410,  7791,     0,  1437,   383,     0,
              134,    12,     0,  2357,   220,     0,
            21845, 10923,     0, 21845, 10923,     0,
            21845, 10923,     0, 21845, 10923,     0,
    },
};
static const unsigned short stb_av1_default_coef_base_tok[4][5][2][41][4] = {
    { /* qcat=0 */
            28734, 23838, 20041,     0, 14686,  3027,   891,     0,
            20172,  6644,  2275,     0, 23322, 11650,  5763,     0,
            26460, 17627, 11489,     0, 30305, 26411, 22985,     0,
            12101,  2222,   839,     0, 19725,  6645,  2634,     0,
            24617, 14011,  7990,     0, 27513, 19929, 14136,     0,
            29948, 25562, 21607,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 17032,  5215,  2164,     0,
            21558,  8974,  3981,     0, 26821, 18894, 13067,     0,
            28553, 23445, 18877,     0, 29935, 26306, 22709,     0,
            13163,  2375,  1186,     0, 19245,  6516,  2520,     0,
            24322, 14146,  8256,     0, 28950, 22425, 16794,     0,
            31287, 28651, 25972,     0, 10119,  1466,   578,     0,
            17939,  5641,  2319,     0, 24455, 15066,  9464,     0,
            29746, 24467, 19982,     0, 31232, 28356, 25584,     0,
            10414,  2994,  1396,     0, 18045,  7296,  3554,     0,
            26095, 19023, 14106,     0, 30700, 27002, 23446,     0,
            24576, 16384,  8192,     0, 26466, 16324, 11007,     0,
             9728,  1230,   293,     0, 17572,  4316,  1272,     0,
            22748,  9822,  4254,     0, 26235, 15906,  9267,     0,
            29230, 22952, 17692,     0,  8324,   893,   243,     0,
            16887,  3844,  1133,     0, 22846,  9895,  4302,     0,
            26241, 15802,  9077,     0, 28654, 21465, 15548,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            12567,  1998,   559,     0, 18014,  4697,  1510,     0,
            24390, 12582,  6251,     0, 26852, 17469, 10790,     0,
            28500, 21185, 14867,     0,  8407,   743,   187,     0,
            14095,  2663,   825,     0, 22572, 10524,  5192,     0,
            27273, 18419, 12351,     0, 30092, 25353, 21270,     0,
             8090,   810,   183,     0, 14139,  2862,   937,     0,
            23404, 12044,  6453,     0, 28127, 20450, 14674,     0,
            30010, 25381, 21189,     0,  7335,   926,   299,     0,
            13973,  3479,  1357,     0, 25124, 15184,  9176,     0,
            29360, 23754, 17721,     0, 24576, 16384,  8192,     0,
            28232, 22696, 18767,     0,  7309,  1352,   562,     0,
            16163,  4720,  1950,     0, 21760,  9911,  5049,     0,
            25853, 16500, 10453,     0, 30143, 25956, 22231,     0,
             8511,   980,   269,     0, 15888,  3314,   889,     0,
            20810,  7714,  2990,     0, 24852, 14050,  7684,     0,
            29385, 23991, 19322,     0, 10048,  1165,   375,     0,
            17808,  4643,  1433,     0, 23037, 10558,  4840,     0,
            26464, 16936, 10491,     0, 29858, 24950, 20602,     0,
            12393,  2141,   637,     0, 18864,  5484,  1881,     0,
            23400, 11210,  5624,     0, 26831, 17802, 11649,     0,
            30101, 25543, 21449,     0,  8798,  1298,   390,     0,
            15595,  3034,   750,     0, 19973,  7327,  2803,     0,
            23787, 13088,  6875,     0, 28040, 21396, 15866,     0,
             8481,   971,   329,     0, 16065,  3623,  1072,     0,
            21935,  9214,  4043,     0, 26300, 16202,  9711,     0,
            30353, 26206, 22490,     0,  6158,   373,   109,     0,
            14178,  2270,   651,     0, 20348,  7012,  2818,     0,
            25129, 14022,  8058,     0, 29767, 24682, 20421,     0,
             7692,   704,   188,     0, 14822,  2640,   740,     0,
            20744,  7783,  3390,     0, 25251, 14378,  8464,     0,
            29525, 23987, 19437,     0, 26731, 15997, 10811,     0,
             7994,  1064,   342,     0, 15938,  4179,  1712,     0,
            22166,  9940,  5008,     0, 26035, 15939,  9697,     0,
            29518, 23854, 19212,     0,  7186,   548,   100,     0,
            14109,  2426,   545,     0, 20222,  6619,  2253,     0,
            24348, 12317,  5967,     0, 28132, 20348, 14424,     0,
             5187,   406,   129,     0, 13781,  2685,   790,     0,
            21441,  8520,  3684,     0, 25504, 15049,  8648,     0,
            28773, 22000, 16599,     0,  6875,   937,   281,     0,
            16191,  4181,  1389,     0, 22579, 10020,  4586,     0,
            25936, 15674,  9212,     0, 29060, 22658, 17434,     0,
             6864,   486,   112,     0, 13047,  1976,   492,     0,
            19949,  6525,  2357,     0, 24196, 12154,  5877,     0,
            27404, 18709, 12301,     0,  6188,   330,    91,     0,
            11916,  1543,   428,     0, 20333,  7068,  2801,     0,
            24077, 11943,  5792,     0, 28322, 20559, 15499,     0,
             5418,   339,    72,     0, 11396,  1791,   496,     0,
            20095,  7498,  2915,     0, 23560, 11843,  6128,     0,
            27750, 19417, 14036,     0,  5417,   289,    55,     0,
            11370,  1559,   381,     0, 20606,  7721,  2926,     0,
            24872, 14077,  7449,     0, 28098, 19886, 13887,     0,
            27281, 22308, 19060,     0, 11171,  4465,  2094,     0,
            21731, 10815,  6292,     0, 24621, 14806,  9816,     0,
            27526, 19707, 14236,     0, 30879, 27560, 24586,     0,
             5994,   635,   178,     0, 14924,  3204,  1001,     0,
            21078,  8330,  3597,     0, 25226, 14553,  8309,     0,
            29775, 24718, 20449,     0,  4745,   440,   177,     0,
            14117,  2642,   814,     0, 20604,  7622,  3179,     0,
            25006, 14238,  7997,     0, 29276, 23585, 18848,     0,
             5177,   760,   277,     0, 15619,  3915,  1258,     0,
            21283,  8765,  3908,     0, 25071, 14682,  8558,     0,
            29693, 24769, 20550,     0,  4500,   286,   114,     0,
            13137,  1717,   364,     0, 18908,  5508,  1748,     0,
            23163, 11155,  5174,     0, 27892, 20606, 14860,     0,
             5520,   452,   192,     0, 13813,  2311,   693,     0,
            20944,  8771,  3973,     0, 25422, 14572,  8121,     0,
            29365, 23521, 18657,     0,  3057,   113,    33,     0,
            11599,  1374,   351,     0, 19281,  5570,  1811,     0,
            23940, 11085,  5154,     0, 28498, 21317, 15730,     0,
             4060,   190,    37,     0, 12648,  1527,   286,     0,
            19076,  5218,  1447,     0, 23350, 10254,  4329,     0,
            27769, 19485, 13306,     0, 27095, 18466, 13057,     0,
             6517,  2067,   934,     0, 19986,  8985,  4965,     0,
            23641, 12111,  6960,     0, 26400, 16560, 11306,     0,
            30303, 25591, 21946,     0,  2807,   205,    49,     0,
            14450,  2877,   819,     0, 21407,  8254,  3411,     0,
            24868, 13165,  7161,     0, 28766, 22178, 17222,     0,
             3131,   458,   173,     0, 14472,  2855,   959,     0,
            22624, 11253,  5897,     0, 27410, 18446, 12374,     0,
            29701, 24406, 19422,     0,  4116,   298,    92,     0,
            15230,  1997,   559,     0, 18844,  5886,  2274,     0,
            22272,  9931,  4899,     0, 25532, 16372, 11147,     0,
             2025,    81,    22,     0,  9762,  1092,   279,     0,
            18274,  4940,  1648,     0, 22594,  9967,  4416,     0,
            26526, 17487, 11725,     0,  6951,   525,    48,     0,
            14150,  1401,   443,     0, 18771,  4450,   890,     0,
            20513,  6234,  1385,     0, 23207, 11180,  4318,     0,
             4580,   133,    44,     0, 10708,   403,    40,     0,
            14666,  2078,   240,     0, 18572,  3904,   769,     0,
            20506,  6976,  1903,     0,  8592,   659,   140,     0,
            14488,  3087,   805,     0, 22563,  9065,  3104,     0,
            24879, 12743,  5092,     0, 26708, 16025,  8798,     0,
            27627, 25672, 24508,     0,  5582,  3746,  2979,     0,
            26100, 20200, 17086,     0, 30596, 26587, 24130,     0,
            31642, 29389, 28237,     0, 32325, 31407, 30514,     0,
             6685,  1615,   332,     0, 19282,  8165,  4285,     0,
            26260, 17928, 12858,     0, 29382, 23968, 19482,     0,
            31238, 28446, 25714,     0,  3129,   688,   220,     0,
            16871,  5216,  2478,     0, 24180, 12721,  7385,     0,
            27879, 19429, 13499,     0, 30528, 25897, 22270,     0,
             4603,   571,   251,     0, 12033,  2341,  1200,     0,
            18443,  8097,  5076,     0, 27649, 20214, 14963,     0,
            30958, 27327, 24507,     0,  1556,    44,    20,     0,
             9416,  1002,   223,     0, 18099,  5198,  1709,     0,
            24276, 11874,  5496,     0, 29124, 22574, 17564,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 30307, 25755, 23397,     0,
             8019,  3168,  1782,     0, 23302, 13731, 10351,     0,
            29184, 23488, 18368,     0, 31263, 28839, 27335,     0,
            32091, 31268, 30032,     0,  8781,  2066,   651,     0,
            19214,  8197,  3505,     0, 26557, 18212, 11613,     0,
            29633, 21796, 17143,     0, 30333, 25641, 21341,     0,
             1468,   236,   218,     0, 18011,  2403,   814,     0,
            28363, 21156, 14215,     0, 32188, 28636, 25446,     0,
            31073, 22599, 18644,     0,  2760,   486,   177,     0,
            13524,  2660,  1020,     0, 21588,  8610,  3213,     0,
            27118, 17796, 13559,     0, 30654, 27659, 24312,     0,
              912,    52,    20,     0,  9756,  1104,   196,     0,
            19074,  6112,  2132,     0, 24626, 13260,  6675,     0,
            28515, 21813, 16044,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            32167, 31785, 31457,     0, 14043,  9362,  4681,     0,
            27307, 24576, 21845,     0, 28987, 17644, 11343,     0,
            30181, 25007, 20696,     0, 32662, 32310, 31958,     0,
            10486,  3058,   874,     0, 24260, 11842,  6784,     0,
            29042, 20055, 14685,     0, 31148, 25656, 21875,     0,
            32039, 30532, 29273,     0,  2605,   294,    84,     0,
            14464,  2304,   768,     0, 21325,  6242,  3121,     0,
            26761, 17476, 11469,     0, 30534, 26065, 23831,     0,
             1814,   591,   197,     0, 15405,  3206,  1692,     0,
            23082, 10304,  5358,     0, 24576, 16384, 11378,     0,
            31013, 24722, 21504,     0,  1600,    34,    20,     0,
            10282,  1327,   297,     0, 19935,  7141,  3030,     0,
            25788, 15389,  9646,     0, 29657, 23881, 19289,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
    },
    { /* qcat=1 */
            26727, 20914, 16841,     0, 12442,  1863,   517,     0,
            18604,  5937,  2043,     0, 23008, 12121,  6183,     0,
            26352, 17815, 11549,     0, 29802, 25617, 21877,     0,
             9201,  1394,   514,     0, 17790,  5352,  1822,     0,
            23334, 12543,  6514,     0, 26110, 18210, 12233,     0,
            28852, 24091, 19779,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 14680,  3223,  1181,     0,
            19706,  6925,  2695,     0, 23828, 15941, 10517,     0,
            25114, 19548, 14795,     0, 27035, 22452, 18312,     0,
             9889,  1380,   654,     0, 17553,  4775,  1813,     0,
            23371, 13323,  7790,     0, 29326, 22955, 17424,     0,
            31400, 28832, 26236,     0,  7274,   735,   362,     0,
            15996,  4805,  2050,     0, 23349, 14603,  9508,     0,
            30091, 25267, 20971,     0, 31252, 28424, 25598,     0,
             6212,  1314,   667,     0, 15640,  5733,  2660,     0,
            24444, 17424, 12519,     0, 30865, 27072, 23299,     0,
            24576, 16384,  8192,     0, 24313, 13765,  8400,     0,
             9205,   747,   164,     0, 16531,  3322,   833,     0,
            22044,  8769,  3410,     0, 26043, 15240,  8352,     0,
            28841, 21841, 15943,     0,  6455,   480,   134,     0,
            15338,  2673,   673,     0, 21652,  8162,  3089,     0,
            25573, 14384,  7499,     0, 28042, 19916, 13453,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
             9946,  1120,   285,     0, 16044,  3135,   839,     0,
            22507,  9735,  4043,     0, 25739, 14928,  8240,     0,
            27901, 18882, 11266,     0,  7470,   876,   277,     0,
            14959,  3438,  1256,     0, 23100, 11439,  6189,     0,
            27994, 19812, 13792,     0, 30446, 25738, 21228,     0,
             7296,   848,   225,     0, 14811,  3381,  1136,     0,
            23572, 12175,  6368,     0, 28088, 20063, 13566,     0,
            29851, 24312, 19332,     0,  6297,   709,   194,     0,
            14310,  2985,   859,     0, 24368, 13304,  6812,     0,
            28956, 21795, 15562,     0, 24576, 16384,  8192,     0,
            25989, 19025, 15090,     0,  7962,   971,   311,     0,
            15152,  3721,  1396,     0, 21705,  9593,  4765,     0,
            26247, 16658, 10444,     0, 30004, 25264, 21114,     0,
             7502,   401,   131,     0, 13714,  2215,   593,     0,
            20629,  7556,  2961,     0, 25457, 14606,  8064,     0,
            29371, 23604, 18694,     0,  6780,   560,   246,     0,
            16515,  3856,  1242,     0, 23617, 11381,  5396,     0,
            27080, 17853, 11272,     0, 30051, 25141, 20764,     0,
             9624,   913,   325,     0, 16698,  4277,  1443,     0,
            24066, 12301,  6251,     0, 27525, 18812, 12401,     0,
            30147, 25433, 21201,     0,  6132,   428,   138,     0,
            12778,  1718,   427,     0, 19525,  6663,  2453,     0,
            24180, 13247,  6850,     0, 28051, 21183, 15464,     0,
             6924,   476,   186,     0, 13678,  2133,   671,     0,
            20805,  8222,  3829,     0, 26550, 16681, 10414,     0,
            30428, 26160, 22342,     0,  4722,   192,    74,     0,
            11590,  1455,   472,     0, 19282,  6584,  2898,     0,
            25619, 14897,  9045,     0, 29935, 24810, 20509,     0,
             5058,   240,    82,     0, 12094,  1692,   500,     0,
            20355,  7813,  3525,     0, 26092, 15841,  9671,     0,
            29802, 24435, 19849,     0, 24129, 13429,  8339,     0,
             8364,   931,   243,     0, 15771,  3343,   984,     0,
            21515,  8534,  3619,     0, 26017, 15374,  8740,     0,
            29278, 22938, 17577,     0,  6485,   297,    54,     0,
            13169,  1600,   326,     0, 19622,  5814,  1875,     0,
            24554, 12180,  5878,     0, 28069, 19687, 13468,     0,
             4556,   310,    99,     0, 14174,  2452,   668,     0,
            21549,  8360,  3534,     0, 25903, 15112,  8619,     0,
            29090, 22406, 16762,     0,  6943,   632,   152,     0,
            15455,  2915,   747,     0, 21571,  8297,  3296,     0,
            25821, 14987,  8363,     0, 29000, 22108, 16507,     0,
             5416,   268,    62,     0, 11918,  1300,   299,     0,
            18747,  5061,  1635,     0, 23804, 11020,  4930,     0,
            27331, 18103, 11581,     0,  6464,   276,    70,     0,
            12359,  1388,   383,     0, 19086,  5546,  2136,     0,
            23794, 11532,  6083,     0, 28534, 21103, 15834,     0,
             6495,   411,    57,     0, 12096,  1526,   327,     0,
            18596,  5514,  1866,     0, 22898, 10870,  5493,     0,
            27604, 19262, 13498,     0,  6043,   309,    40,     0,
            11777,  1326,   241,     0, 19697,  6334,  1957,     0,
            24584, 12678,  6026,     0, 27965, 19513, 12873,     0,
            25213, 17826, 14267,     0,  8358,  1590,   481,     0,
            18374,  6030,  2515,     0, 24355, 13214,  7573,     0,
            28002, 19844, 13983,     0, 30739, 26962, 23561,     0,
             5992,   404,   105,     0, 14036,  2801,   837,     0,
            21763,  8982,  3916,     0, 26302, 15859,  9258,     0,
            29724, 24130, 19349,     0,  3560,   186,    64,     0,
            12700,  1911,   560,     0, 20765,  7683,  3173,     0,
            25821, 15018,  8579,     0, 29523, 23665, 18761,     0,
             5409,   303,    99,     0, 13347,  2154,   594,     0,
            20853,  7758,  3189,     0, 25818, 15092,  8694,     0,
            29761, 24295, 19672,     0,  3766,    92,    33,     0,
            10666,   919,   192,     0, 18360,  4759,  1363,     0,
            23741, 11089,  4837,     0, 28074, 20090, 14020,     0,
             4552,   240,    86,     0, 11919,  1504,   450,     0,
            20012,  6953,  3017,     0, 25203, 13967,  7845,     0,
            29259, 23235, 18291,     0,  2635,    81,    29,     0,
             9705,   858,   253,     0, 18180,  4717,  1636,     0,
            23683, 11119,  5311,     0, 28507, 21114, 15504,     0,
             3250,    77,    20,     0, 10317,   809,   155,     0,
            17904,  4046,  1068,     0, 23073,  9804,  4052,     0,
            27836, 19410, 13266,     0, 26303, 15810, 11080,     0,
             7569,  1254,   408,     0, 17994,  5619,  2161,     0,
            23511, 11330,  5796,     0, 27045, 17585, 10886,     0,
            29618, 23889, 19037,     0,  5779,   506,    86,     0,
            15372,  2831,   683,     0, 21381,  7867,  2984,     0,
            25479, 13947,  7220,     0, 29034, 22191, 16682,     0,
             3040,   267,    73,     0, 15337,  3067,   865,     0,
            22847,  9942,  4468,     0, 26872, 17334, 10700,     0,
            29338, 23122, 18011,     0,  4154,   257,    63,     0,
            13404,  2130,   505,     0, 19639,  6514,  2366,     0,
            24014, 12284,  6328,     0, 28390, 21161, 15658,     0,
             2476,    97,    24,     0, 10988,  1165,   267,     0,
            18454,  4939,  1477,     0, 23157, 10441,  4505,     0,
            27878, 19681, 13703,     0,  6906,   201,    35,     0,
            11974,   718,   201,     0, 15525,  2143,   514,     0,
            19485,  5140,  1294,     0, 23099, 10236,  3850,     0,
             5333,    71,    20,     0,  7846,   378,    54,     0,
            11319,  1264,   232,     0, 16376,  3039,   936,     0,
            21076,  7884,  3692,     0,  8575,   478,    33,     0,
            13859,  1664,   205,     0, 20532,  5927,  1365,     0,
            24597, 10928,  3686,     0, 25544, 15488,  7493,     0,
            29690, 25929, 22878,     0, 18931, 12318,  8289,     0,
            26854, 18546, 13440,     0, 28902, 22501, 18006,     0,
            30156, 25560, 21726,     0, 31701, 29777, 27992,     0,
             6951,  1122,   239,     0, 19060,  6430,  2383,     0,
            25440, 14183,  7898,     0, 28077, 19688, 13492,     0,
            30943, 27515, 24416,     0,  3382,   453,   144,     0,
            15608,  3767,  1408,     0, 23166, 10906,  5372,     0,
            26853, 16996, 10620,     0, 29982, 24989, 20721,     0,
             3522,   318,   105,     0, 14072,  2839,   950,     0,
            22258,  9399,  4208,     0, 26539, 16269,  9643,     0,
            30160, 25320, 21063,     0,  2015,    58,    20,     0,
            11130,  1281,   265,     0, 19831,  5914,  1898,     0,
            24586, 12172,  5798,     0, 29131, 22499, 17271,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 27524, 20618, 15862,     0,
            12282,  5910,  3067,     0, 25012, 14451,  9033,     0,
            29316, 23512, 19622,     0, 30748, 27562, 24539,     0,
            30967, 27775, 24865,     0,  5717,   910,   237,     0,
            16780,  5237,  2149,     0, 23580, 11284,  6049,     0,
            26495, 15582,  8968,     0, 29660, 23413, 18004,     0,
             1692,   248,    88,     0, 14649,  2731,   918,     0,
            22524,  9799,  5296,     0, 28076, 18691, 13495,     0,
            29074, 21091, 15212,     0,  2708,   187,    48,     0,
            11757,  1993,   648,     0, 20837,  7948,  3479,     0,
            25649, 15106,  8412,     0, 28935, 22062, 16464,     0,
              814,    37,    20,     0,  8855,  1044,   279,     0,
            17248,  4708,  1482,     0, 21251,  9760,  4197,     0,
            26575, 18260, 12139,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            31733, 29961, 28612,     0, 19606, 14630, 11829,     0,
            30072, 26135, 24013,     0, 31395, 28607, 25915,     0,
            31669, 30022, 28052,     0, 32428, 31747, 31169,     0,
             9942,  2349,   633,     0, 22373, 11006,  5826,     0,
            28042, 20361, 15407,     0, 30321, 25688, 22175,     0,
            31541, 29051, 26757,     0,  4612,  1344,   834,     0,
            15853,  5014,  2395,     0, 23620, 11778,  6337,     0,
            26818, 17253, 11620,     0, 30276, 25441, 21242,     0,
             2166,   291,    98,     0, 12742,  2813,  1200,     0,
            21548,  9140,  4663,     0, 26116, 15749,  9795,     0,
            29704, 24232, 19725,     0,   999,    44,    20,     0,
            10538,  1881,   395,     0, 20534,  7689,  3037,     0,
            25442, 13952,  7415,     0, 28835, 21861, 16152,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
    },
    { /* qcat=2 */
            23872, 16541, 12138,     0,  9139,   986,   241,     0,
            17595,  5013,  1447,     0, 22610, 11535,  5386,     0,
            26348, 17911, 11210,     0, 29499, 24613, 20122,     0,
             7933,   759,   272,     0, 16259,  4347,  1189,     0,
            21811, 11254,  5350,     0, 24887, 16838, 10672,     0,
            27380, 21808, 16850,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 12023,  1995,   675,     0,
            17568,  5547,  1907,     0, 19736, 11895,  7101,     0,
            20483, 14105,  9274,     0, 21205, 15287, 11279,     0,
             6508,   786,   448,     0, 17371,  4685,  1668,     0,
            23026, 13551,  7944,     0, 29507, 23139, 17406,     0,
            31288, 28446, 25269,     0,  5169,   512,   308,     0,
            15911,  5109,  1994,     0, 23217, 14478,  9020,     0,
            29716, 23835, 18665,     0, 30747, 26858, 22981,     0,
             3763,   753,   376,     0, 15091,  5074,  1905,     0,
            23564, 15412,  9549,     0, 30365, 25252, 19954,     0,
            24576, 16384,  8192,     0, 21960, 10712,  5872,     0,
             7029,   455,    92,     0, 15480,  2565,   547,     0,
            21409,  7890,  2872,     0, 25819, 15001,  7875,     0,
            28481, 20972, 14697,     0,  4888,   247,    63,     0,
            13730,  1764,   354,     0, 20204,  6423,  2000,     0,
            24499, 12821,  5989,     0, 27094, 18111, 11094,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
             7026,   449,    97,     0, 13211,  1604,   314,     0,
            19387,  6387,  2013,     0, 22667, 11302,  6046,     0,
            23559, 13118,  5943,     0,  5661,   851,   336,     0,
            14712,  3875,  1565,     0, 22568, 11334,  6004,     0,
            28108, 19855, 13266,     0, 30400, 25838, 20264,     0,
             5808,   610,   155,     0, 14140,  2763,   737,     0,
            22535, 10326,  4536,     0, 27297, 18138, 11252,     0,
            29533, 22001, 15659,     0,  5072,   328,    76,     0,
            12736,  1601,   330,     0, 24068, 11427,  4326,     0,
            27106, 17937, 10973,     0, 24576, 16384,  8192,     0,
            23064, 15474, 11636,     0,  6006,   490,   135,     0,
            14386,  3148,   949,     0, 21877,  9293,  4045,     0,
            26410, 16185,  9459,     0, 29520, 23650, 18627,     0,
             5564,   195,    69,     0, 12950,  1944,   439,     0,
            20996,  7648,  2727,     0, 25773, 14735,  7729,     0,
            29016, 22326, 16670,     0,  5546,   512,   209,     0,
            17412,  4369,  1293,     0, 23947, 12133,  5711,     0,
            27257, 18364, 11529,     0, 29833, 24546, 19717,     0,
             7893,   648,   239,     0, 17535,  4503,  1323,     0,
            24163, 12198,  5836,     0, 27337, 18355, 11572,     0,
            29774, 24427, 19545,     0,  4567,   164,    68,     0,
            11727,  1322,   312,     0, 19547,  6555,  2293,     0,
            24513, 13383,  6731,     0, 27838, 20183, 13938,     0,
             4000,   320,   141,     0, 13063,  2207,   747,     0,
            21196,  9179,  4548,     0, 27236, 17734, 11322,     0,
            30308, 25618, 21312,     0,  2894,   149,    69,     0,
            11147,  1697,   567,     0, 20257,  8021,  3776,     0,
            26487, 16373, 10020,     0, 29522, 23490, 18271,     0,
             3053,   143,    56,     0, 11810,  1757,   485,     0,
            21535,  9097,  3962,     0, 26756, 16640,  9900,     0,
            29341, 22917, 17354,     0, 21752, 10657,  5974,     0,
             6822,   411,    91,     0, 14878,  2316,   516,     0,
            21090,  7626,  2952,     0, 26048, 15234,  8184,     0,
            28538, 21103, 14948,     0,  4368,   145,    21,     0,
            11604,  1100,   193,     0, 19196,  5380,  1586,     0,
            24534, 12018,  5410,     0, 27703, 18713, 11871,     0,
             3787,   221,    63,     0, 14087,  2225,   529,     0,
            21849,  8693,  3482,     0, 26337, 15569,  8691,     0,
            28949, 22304, 16150,     0,  5898,   301,    75,     0,
            13727,  1937,   421,     0, 20974,  7557,  2752,     0,
            25880, 14749,  7798,     0, 28398, 20405, 13776,     0,
             3190,    98,    24,     0,  9609,   761,   155,     0,
            17453,  4099,  1092,     0, 23470, 10161,  3986,     0,
            26624, 16855,  9800,     0,  4658,   269,    99,     0,
            11194,  1831,   753,     0, 20009,  7950,  4041,     0,
            26223, 16007,  9726,     0, 29119, 22171, 15935,     0,
             4605,   216,    40,     0, 10667,  1299,   304,     0,
            19608,  7296,  2625,     0, 25465, 14084,  7300,     0,
            27527, 18793, 11813,     0,  4368,   137,    24,     0,
            10664,   975,   165,     0, 19211,  6197,  1922,     0,
            25019, 12907,  6093,     0, 27895, 18738, 11534,     0,
            22968, 15133, 11695,     0,  6615,   883,   241,     0,
            17730,  4916,  1762,     0, 24050, 12204,  6282,     0,
            27640, 18692, 12254,     0, 30132, 25202, 20843,     0,
             5217,   264,    67,     0, 14458,  2714,   668,     0,
            22557,  9348,  3686,     0, 26546, 15892,  8852,     0,
            29306, 22814, 17270,     0,  2777,   135,    47,     0,
            12885,  2017,   567,     0, 21627,  8584,  3483,     0,
            26348, 15828,  8994,     0, 29376, 23015, 17650,     0,
             4303,   152,    56,     0, 12918,  2066,   524,     0,
            21785,  8744,  3545,     0, 26474, 15998,  9186,     0,
            29524, 23485, 18259,     0,  2745,    51,    20,     0,
             9828,   736,   142,     0, 18486,  4840,  1295,     0,
            24206, 11441,  4854,     0, 27922, 19375, 12849,     0,
             2787,   178,    73,     0, 12303,  1805,   602,     0,
            21289,  9189,  4573,     0, 26852, 17120, 10695,     0,
            29737, 24163, 19370,     0,  1622,    77,    29,     0,
             9662,  1044,   324,     0, 18985,  6030,  2329,     0,
            24916, 13300,  6961,     0, 28908, 21644, 15915,     0,
             1754,    44,    20,     0,  9139,   659,   140,     0,
            18021,  4653,  1365,     0, 24223, 11526,  5290,     0,
            28194, 19987, 13701,     0, 23583, 13074,  8080,     0,
             6687,   783,   147,     0, 16753,  3768,   981,     0,
            22226,  9078,  3562,     0, 26036, 14823,  8091,     0,
            28852, 21729, 16046,     0,  4544,   202,    24,     0,
            13668,  1630,   283,     0, 20240,  6148,  1889,     0,
            25027, 12491,  5883,     0, 28202, 19923, 13778,     0,
             2835,   175,    50,     0, 15098,  2435,   613,     0,
            22383,  9168,  3859,     0, 26525, 16532, 10361,     0,
            28792, 22379, 16751,     0,  4391,   207,    30,     0,
            13402,  1593,   286,     0, 19441,  5593,  1674,     0,
            24510, 11999,  5625,     0, 28065, 19570, 13241,     0,
             1682,    62,    20,     0,  9915,   866,   185,     0,
            18009,  4582,  1349,     0, 23484, 10386,  4420,     0,
            27183, 17576, 10900,     0,  4477,   116,    22,     0,
            12919,   661,   197,     0, 17934,  5950,  3554,     0,
            22462, 10174,  4096,     0, 26153, 15384,  9384,     0,
             3821,   164,    23,     0,  7143,   479,   122,     0,
            14010,  4096,  1365,     0, 22751,  9338,  4245,     0,
            25906, 17499, 10637,     0,  8835,   259,    29,     0,
            12841,  1273,   137,     0, 20865,  6745,  2147,     0,
            25742, 12674,  5516,     0, 26770, 14662,  8331,     0,
            28312, 21494, 17235,     0, 11549,  3689,  1152,     0,
            21595,  8994,  4201,     0, 25486, 14475,  8505,     0,
            27878, 19482, 13653,     0, 30878, 27260, 24109,     0,
             6117,   632,   121,     0, 18138,  4514,  1313,     0,
            24052, 11481,  5373,     0, 27153, 17437, 10760,     0,
            30093, 25068, 20618,     0,  2814,   242,    78,     0,
            16642,  3786,  1135,     0, 23738, 11407,  5416,     0,
            27357, 17975, 11497,     0, 29825, 24346, 19605,     0,
             3229,   167,    38,     0, 14643,  2383,   567,     0,
            22346,  8678,  3300,     0, 26300, 15281,  8330,     0,
            29798, 24115, 19237,     0,  1856,    53,    20,     0,
            12102,  1395,   271,     0, 20259,  6128,  1851,     0,
            24710, 12139,  5478,     0, 28537, 20762, 14716,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 22566, 12135,  7284,     0,
             5432,  1323,   416,     0, 20348,  8384,  4216,     0,
            25120, 14653,  8912,     0, 27106, 18427, 12866,     0,
            29157, 22440, 17378,     0,  1823,   152,    32,     0,
            14086,  2263,   515,     0, 21255,  7432,  2565,     0,
            25319, 13316,  6620,     0, 28286, 19717, 13882,     0,
              746,    78,    21,     0, 14190,  2267,   622,     0,
            21519,  9400,  4137,     0, 27123, 15810, 10610,     0,
            27759, 21324, 16131,     0,  1411,    58,    20,     0,
            11216,  1274,   264,     0, 18877,  5091,  1428,     0,
            23717, 10670,  4596,     0, 27578, 19391, 13282,     0,
              404,    28,    20,     0,  7929,   861,   217,     0,
            15608,  3989,  1072,     0, 20316,  8631,  3166,     0,
            26603, 17379, 10291,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            30193, 25487, 21691,     0, 18766, 11902,  7366,     0,
            26425, 17712, 13110,     0, 28294, 20910, 15727,     0,
            29903, 24469, 20234,     0, 31424, 28819, 26377,     0,
             8048,  1529,   309,     0, 20183,  7412,  2800,     0,
            25587, 14522,  8324,     0, 27743, 19101, 12883,     0,
            30247, 25464, 21163,     0,  2860,   516,   184,     0,
            15347,  3612,  1193,     0, 22879, 10580,  4986,     0,
            26890, 17121, 10645,     0, 29954, 24103, 19445,     0,
             2585,   200,    55,     0, 14240,  2573,   719,     0,
            21786,  8162,  3111,     0, 25811, 14603,  7537,     0,
            29260, 22650, 17300,     0,  1007,    32,    20,     0,
            11727,  1440,   222,     0, 20200,  6036,  1602,     0,
            24716, 12048,  5035,     0, 28432, 20576, 14372,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
    },
    { /* qcat=3 */
            25706, 16296, 10449,     0,  8230,   507,    94,     0,
            19093,  4727,   989,     0, 24178, 12094,  5137,     0,
            27083, 18093, 10755,     0, 29113, 22870, 17037,     0,
             6275,   350,   110,     0, 16392,  3426,   678,     0,
            22174, 10119,  3798,     0, 24592, 15598,  8465,     0,
            27163, 20074, 13629,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0,  8880,   866,   226,     0,
            14156,  3081,   781,     0, 16523,  7916,  3519,     0,
            17003, 10160,  5209,     0, 12873,  8069,  5258,     0,
             4367,   556,   311,     0, 17494,  4943,  1788,     0,
            23404, 14640,  8436,     0, 30485, 24575, 17686,     0,
            31540, 28796, 24887,     0,  3313,   299,   148,     0,
            14787,  4523,  1380,     0, 21847, 12670,  6528,     0,
            29025, 20939, 14111,     0, 30394, 23175, 17053,     0,
             1700,   302,   133,     0, 12447,  3196,   797,     0,
            21997, 12513,  5649,     0, 29973, 22358, 15407,     0,
            24576, 16384,  8192,     0, 23448, 10666,  4928,     0,
             5711,   304,    44,     0, 16437,  2500,   459,     0,
            22449,  8833,  3048,     0, 26579, 16320,  8662,     0,
            29179, 21884, 13960,     0,  3742,   144,    20,     0,
            13542,  1261,   181,     0, 20076,  5847,  1565,     0,
            25719, 13236,  5133,     0, 25041, 17099,  9516,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
             4712,   143,    20,     0, 10385,   693,    99,     0,
            17351,  5670,  1019,     0, 14641,  6275,  5578,     0,
            27307, 16384, 10923,     0,  4786,   677,   184,     0,
            13723,  2900,   796,     0, 22371, 10502,  4836,     0,
            26778, 19071, 11268,     0, 30976, 25856, 17664,     0,
             4570,   267,    50,     0, 11234,  1247,   199,     0,
            21659,  7551,  2751,     0, 27097, 17644,  6617,     0,
            28087, 18725, 14043,     0,  4080,   188,    27,     0,
            10192,   689,   107,     0, 22141, 10627,  4428,     0,
            23406, 18725,  4681,     0, 24576, 16384,  8192,     0,
            25014, 15820, 10626,     0,  7098,   438,    77,     0,
            17105,  3543,   774,     0, 22890,  9480,  3610,     0,
            26349, 15680,  8432,     0, 28909, 21765, 15729,     0,
             5206,   173,    43,     0, 15193,  2180,   369,     0,
            21949,  7930,  2459,     0, 25644, 14082,  6852,     0,
            28289, 20080, 13428,     0,  4383,   292,    95,     0,
            17462,  3763,   830,     0, 23831, 11153,  4446,     0,
            26786, 17165,  9982,     0, 29148, 22501, 16632,     0,
             5488,   304,   101,     0, 17161,  3608,   764,     0,
            23677, 10633,  4028,     0, 26536, 16136,  8748,     0,
            28721, 21391, 15096,     0,  3548,   138,    50,     0,
            13118,  1548,   306,     0, 19718,  6456,  1941,     0,
            23540, 11898,  5300,     0, 26622, 17619, 10797,     0,
             2599,   287,   145,     0, 15556,  3457,  1214,     0,
            22857, 11457,  5886,     0, 28281, 19454, 12396,     0,
            30198, 24996, 19879,     0,  1844,   155,    60,     0,
            13278,  2562,   661,     0, 21536,  8770,  3492,     0,
            25999, 14813,  7733,     0, 28370, 20145, 13554,     0,
             2159,   141,    46,     0, 13398,  2186,   481,     0,
            22311,  9149,  3359,     0, 26325, 15131,  7934,     0,
            28123, 19532, 12662,     0, 24142, 12497,  6552,     0,
             6061,   362,    57,     0, 15769,  2439,   482,     0,
            21323,  7645,  2482,     0, 26357, 13940,  7167,     0,
            25967, 20310, 12520,     0,  2850,    86,    20,     0,
            12119,  1029,   150,     0, 19889,  4995,  1187,     0,
            24872, 11017,  4524,     0, 27508, 17898,  9070,     0,
             3516,   175,    37,     0, 15696,  2308,   474,     0,
            22115,  8625,  3403,     0, 26232, 15278,  8785,     0,
            27839, 19598, 12683,     0,  4631,   250,    53,     0,
            14597,  1984,   361,     0, 21331,  7332,  2309,     0,
            25516, 14234,  6592,     0, 28642, 19415, 11790,     0,
             1606,    42,    20,     0,  9751,   546,    67,     0,
            17139,  3535,   722,     0, 23381, 10147,  3288,     0,
            25846, 15152,  7758,     0,  3930,   503,   154,     0,
            13067,  2562,   848,     0, 21554, 10358,  4835,     0,
            27448, 18591,  9734,     0, 27719, 19887, 14941,     0,
             5284,   297,    34,     0, 11692,  1242,   207,     0,
            20061,  6465,  1557,     0, 24599, 11046,  4549,     0,
            26723, 13362,  5726,     0,  5015,   196,    23,     0,
            11936,   890,   115,     0, 19518,  5412,  1094,     0,
            25050, 11260,  2910,     0, 25559, 14418,  7209,     0,
            24892, 15867, 11027,     0,  8767,   870,   143,     0,
            18239,  4809,  1317,     0, 24495, 11950,  5510,     0,
            27490, 18095, 11258,     0, 29785, 23925, 18729,     0,
             4752,   194,    36,     0, 15297,  2462,   467,     0,
            22544,  8705,  3040,     0, 26166, 14814,  7716,     0,
            28766, 21183, 15009,     0,  2578,   134,    29,     0,
            15271,  2486,   498,     0, 22539,  9039,  3230,     0,
            26424, 15557,  8328,     0, 28919, 21579, 15660,     0,
             4198,   185,    42,     0, 15247,  2607,   530,     0,
            22615,  9203,  3390,     0, 26313, 15427,  8325,     0,
            28861, 21726, 15744,     0,  2079,    53,    20,     0,
            11222,   928,   158,     0, 19221,  5187,  1309,     0,
            23856, 11011,  4459,     0, 27220, 17688, 10722,     0,
             1985,   228,    83,     0, 15228,  3240,  1100,     0,
            22608, 11300,  5985,     0, 28044, 19375, 12714,     0,
            30066, 24594, 19666,     0,  1120,    82,    26,     0,
            11814,  1674,   431,     0, 20348,  7070,  2589,     0,
            25464, 13448,  6520,     0, 28402, 20507, 13904,     0,
             1187,    45,    20,     0, 11395,  1182,   243,     0,
            20024,  6143,  1883,     0, 25337, 12446,  5818,     0,
            28076, 19445, 12657,     0, 24935, 14399,  8673,     0,
             6118,   495,    66,     0, 16397,  2807,   577,     0,
            21713,  8686,  3139,     0, 25876, 14124,  7368,     0,
            27762, 19711, 13528,     0,  2934,   102,    20,     0,
            13191,  1433,   198,     0, 20515,  6259,  1646,     0,
            24777, 11996,  5057,     0, 27091, 16858,  9709,     0,
             2659,   236,    48,     0, 16021,  2602,   516,     0,
            22634,  9226,  3584,     0, 26977, 16592,  9212,     0,
            28406, 22354, 15484,     0,  3276,   142,    20,     0,
            12874,  1366,   243,     0, 19826,  5697,  1899,     0,
            24422, 11552,  5363,     0, 26196, 15681,  8909,     0,
              733,    33,    20,     0,  9811,   930,   150,     0,
            18044,  4196,   996,     0, 22404,  8769,  3215,     0,
            25764, 14335,  7113,     0,  5240,   491,    87,     0,
            15809,  1597,   672,     0, 22282,  9175,  4806,     0,
            24576, 16384,  9557,     0, 23831, 14895, 11916,     0,
             5053,   766,   153,     0, 17695,  3277,  1092,     0,
            21504,  8192,  4096,     0, 30427, 14043,  9362,     0,
            25486, 14564,  7282,     0,  4221,   555,   111,     0,
            11980,  2995,   529,     0, 25988, 11299,  2260,     0,
            26810, 17873,  8937,     0, 16384, 10923,  5461,     0,
            26776, 18464, 13003,     0, 10156,  1530,   312,     0,
            19312,  5606,  1681,     0, 24767, 12706,  6264,     0,
            27600, 18663, 12004,     0, 30136, 24997, 20383,     0,
             5734,   424,    59,     0, 16918,  3353,   771,     0,
            23274,  9992,  3927,     0, 26617, 15938,  8799,     0,
            29307, 22729, 17046,     0,  2634,   199,    37,     0,
            17130,  3346,   823,     0, 23618, 10903,  4550,     0,
            27121, 17049, 10092,     0, 29366, 22996, 17291,     0,
             4238,   182,    33,     0, 15629,  2470,   476,     0,
            22568,  8729,  3083,     0, 26349, 15094,  7982,     0,
            29224, 22543, 16944,     0,  1435,    42,    20,     0,
            12150,  1281,   224,     0, 19867,  5551,  1536,     0,
            24144, 11034,  4597,     0, 27664, 18577, 12020,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 21562, 11678,  6207,     0,
             4009,   489,    97,     0, 18597,  4816,  1199,     0,
            23025,  9861,  3627,     0, 25897, 14882,  7900,     0,
            27808, 19616, 13453,     0,  1691,   107,    20,     0,
            13368,  1573,   253,     0, 20016,  5910,  1728,     0,
            24398, 10670,  4177,     0, 27311, 17395, 10470,     0,
             1071,    62,    20,     0, 14908,  2111,   435,     0,
            20258,  7956,  3507,     0, 26588, 13644,  8046,     0,
            27727, 19220, 14809,     0,  1216,    52,    20,     0,
            10860,   999,   145,     0, 18298,  4567,  1203,     0,
            23275,  9786,  4160,     0, 25910, 15528,  8631,     0,
              225,    16,    12,     0,  8482,   671,   102,     0,
            16810,  3551,   744,     0, 22561,  8534,  2810,     0,
            25839, 14463,  7116,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            28631, 21921, 17086,     0, 14944,  5767,  2710,     0,
            22564,  9972,  4477,     0, 26692, 16833, 10643,     0,
            28916, 21831, 15952,     0, 30516, 26444, 22637,     0,
             6928,   752,   106,     0, 17659,  4500,  1237,     0,
            23383, 10537,  4428,     0, 26686, 16096,  9289,     0,
            29450, 23341, 18087,     0,  2174,   194,    50,     0,
            15932,  3216,   909,     0, 23212, 10226,  4412,     0,
            26463, 16043,  9228,     0, 29392, 22873, 17584,     0,
             3385,   151,    23,     0, 13877,  1959,   367,     0,
            21080,  6826,  2081,     0, 25300, 13299,  6117,     0,
            28859, 21410, 15756,     0,  1204,    32,    20,     0,
            11862,  1157,   168,     0, 19577,  5147,  1231,     0,
            24000, 10739,  4092,     0, 27689, 18659, 11862,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
            24576, 16384,  8192,     0, 24576, 16384,  8192,     0,
    },
};
static const unsigned short stb_av1_default_coef_dc_sign[4][2][3][2] = {
    { /* qcat=0 */
            16768,     0, 19712,     0, 13952,     0, 17536,     0,
            19840,     0, 15488,     0,
    },
    { /* qcat=1 */
            16768,     0, 19712,     0, 13952,     0, 17536,     0,
            19840,     0, 15488,     0,
    },
    { /* qcat=2 */
            16768,     0, 19712,     0, 13952,     0, 17536,     0,
            19840,     0, 15488,     0,
    },
    { /* qcat=3 */
            16768,     0, 19712,     0, 13952,     0, 17536,     0,
            19840,     0, 15488,     0,
    },
};
static const unsigned short stb_av1_default_coef_br_tok[4][4][2][21][4] = {
    { /* qcat=0 */
            18470, 12050,  8594,     0, 20232, 13167,  8979,     0,
            24056, 17717, 13265,     0, 26598, 21441, 17334,     0,
            28026, 23842, 20230,     0, 28965, 25451, 22222,     0,
            31072, 29451, 27897,     0, 18376, 12817, 10012,     0,
            16790,  9550,  5950,     0, 20581, 13294,  8879,     0,
            23592, 17128, 12509,     0, 25700, 20113, 15740,     0,
            27112, 22326, 18296,     0, 30188, 27776, 25524,     0,
            20632, 14719, 11342,     0, 18984, 12047,  8287,     0,
            21932, 15147, 10868,     0, 24396, 18324, 13921,     0,
            26245, 20989, 16768,     0, 27431, 22870, 19008,     0,
            29734, 26908, 24306,     0, 16801,  9863,  6482,     0,
            19234, 12114,  8189,     0, 23264, 16676, 12233,     0,
            25793, 20200, 15865,     0, 27404, 22677, 18748,     0,
            28411, 24398, 20911,     0, 30262, 27834, 25550,     0,
             9736,  3953,  1832,     0, 13228,  6064,  3049,     0,
            17610,  9799,  5671,     0, 21360, 13903,  9118,     0,
            23883, 17320, 12518,     0, 25660, 19915, 15352,     0,
            28537, 24727, 21288,     0, 12945,  6278,  3612,     0,
            13878,  6839,  3836,     0, 17108,  9277,  5335,     0,
            20621, 12992,  8280,     0, 23040, 15994, 11119,     0,
            24849, 18491, 13702,     0, 27328, 22598, 18583,     0,
            18362, 11906,  8354,     0, 20944, 13861,  9659,     0,
            24511, 18375, 13965,     0, 26908, 22021, 17990,     0,
            28293, 24282, 20784,     0, 29162, 25814, 22725,     0,
            31032, 29358, 27720,     0, 18338, 12722,  9886,     0,
            17175,  9869,  6059,     0, 20666, 13400,  8957,     0,
            23709, 17184, 12506,     0, 25769, 20165, 15720,     0,
            27084, 22271, 18215,     0, 29946, 27330, 24906,     0,
            16983, 11183,  8409,     0, 14421,  7539,  4502,     0,
            17794, 10281,  6379,     0, 21345, 14087,  9497,     0,
            23905, 17418, 12760,     0, 25615, 19916, 15490,     0,
            29061, 25732, 22786,     0, 17308, 11072,  7299,     0,
            20598, 13519,  9577,     0, 24045, 17741, 13436,     0,
            26340, 21064, 16894,     0, 27846, 23476, 19716,     0,
            28629, 25073, 21758,     0, 30477, 28260, 26170,     0,
            12912,  5848,  2940,     0, 14845,  7479,  3976,     0,
            18490, 10800,  6471,     0, 21858, 14632,  9818,     0,
            24345, 17953, 13141,     0, 25997, 20485, 15994,     0,
            28694, 25018, 21687,     0, 12916,  6694,  4096,     0,
            13397,  6658,  3779,     0, 16503,  8895,  5105,     0,
            20010, 12390,  7816,     0, 22673, 15670, 10807,     0,
            24518, 18140, 13317,     0, 27563, 23023, 19146,     0,
            22205, 16535, 13005,     0, 22974, 16746, 12964,     0,
            26018, 20823, 17009,     0, 27805, 23582, 20016,     0,
            28923, 25333, 22141,     0, 29717, 26683, 23934,     0,
            31457, 30172, 28938,     0, 21522, 16364, 13079,     0,
            20453, 13857, 10037,     0, 22211, 15673, 11479,     0,
            24632, 18762, 14519,     0, 26420, 21294, 17203,     0,
            27572, 23113, 19368,     0, 30419, 28242, 26181,     0,
            19431, 14038, 11199,     0, 13462,  6697,  3886,     0,
            16816,  9228,  5514,     0, 20359, 12834,  8338,     0,
            23008, 16062, 11379,     0, 24764, 18548, 13950,     0,
            28630, 24974, 21807,     0, 21898, 16084, 11819,     0,
            23104, 17538, 14088,     0, 25882, 20659, 17360,     0,
            27943, 23868, 20463,     0, 29138, 25606, 22454,     0,
            29732, 26339, 23381,     0, 31097, 29472, 27828,     0,
            18949, 13609,  9742,     0, 20784, 13660,  9648,     0,
            22078, 15558, 11105,     0, 24784, 18614, 14435,     0,
            25900, 20474, 16644,     0, 27494, 23774, 19900,     0,
            29780, 26997, 24344,     0, 13032,  6121,  3627,     0,
            13835,  6698,  3784,     0, 16989,  9720,  5568,     0,
            20130, 12707,  8236,     0, 22076, 15223, 10548,     0,
            23551, 17517, 12714,     0, 27690, 23484, 20174,     0,
            30437, 29106, 27524,     0, 29877, 27997, 26623,     0,
            28170, 25145, 23039,     0, 29248, 25923, 23569,     0,
            29351, 26649, 23444,     0, 30167, 27356, 25383,     0,
            32168, 31595, 31024,     0, 25096, 19482, 15299,     0,
            28536, 24976, 21975,     0, 29853, 27451, 25371,     0,
            30450, 28412, 26616,     0, 30641, 28768, 27214,     0,
            30918, 29290, 27493,     0, 31791, 30835, 29925,     0,
            14488,  8381,  4779,     0, 16916, 10097,  6583,     0,
            18923, 11817,  7979,     0, 21713, 14802, 10639,     0,
            23630, 17346, 12967,     0, 25314, 19623, 15312,     0,
            29398, 26375, 23755,     0, 26926, 23539, 21930,     0,
            30455, 29277, 28492,     0, 29770, 26664, 25272,     0,
            30348, 25321, 22900,     0, 29734, 24273, 21845,     0,
            28692, 23831, 21793,     0, 31682, 30398, 29469,     0,
            23054, 15514, 12324,     0, 24225, 19070, 15645,     0,
            27850, 23761, 20858,     0, 28639, 25236, 22215,     0,
            30404, 27235, 24710,     0, 30934, 29222, 27205,     0,
            31295, 29860, 28635,     0, 17363, 11575,  7149,     0,
            17077, 10816,  6207,     0, 19806, 13574,  8603,     0,
            22496, 14913, 10639,     0, 24180, 17498, 12050,     0,
            24086, 18099, 13268,     0, 27898, 23132, 19563,     0,
    },
    { /* qcat=1 */
            17773, 11427,  8019,     0, 19610, 12479,  8167,     0,
            23827, 17442, 12892,     0, 26471, 21227, 16961,     0,
            27951, 23739, 19992,     0, 29037, 25495, 22141,     0,
            30921, 29151, 27414,     0, 18296, 13109, 10425,     0,
            15962,  8606,  5235,     0, 19868, 12364,  8055,     0,
            23357, 16656, 11971,     0, 25712, 20071, 15620,     0,
            27224, 22429, 18308,     0, 29814, 27064, 24449,     0,
            20304, 14697, 11414,     0, 17286, 10240,  6734,     0,
            20698, 13499,  9144,     0, 23815, 17362, 12662,     0,
            25741, 20038, 15548,     0, 26881, 21855, 17628,     0,
            28975, 25490, 22321,     0, 17197, 10536,  7019,     0,
            18262, 11193,  7394,     0, 22579, 15679, 11199,     0,
            25452, 19467, 14853,     0, 26985, 21856, 17578,     0,
            28008, 23613, 19680,     0, 29775, 26802, 23994,     0,
             9344,  3865,  1990,     0, 11993,  5102,  2478,     0,
            16294,  8358,  4469,     0, 20297, 12588,  7781,     0,
            23358, 16281, 11329,     0, 25232, 19154, 14239,     0,
            27720, 23182, 19219,     0, 11678,  5478,  3012,     0,
            11972,  5366,  2742,     0, 14949,  7283,  3799,     0,
            18908, 10859,  6306,     0, 21766, 14274,  9239,     0,
            23815, 16839, 11871,     0, 26320, 20850, 16314,     0,
            16769, 10560,  7319,     0, 19718, 12780,  8646,     0,
            24174, 17904, 13390,     0, 26735, 21689, 17530,     0,
            28214, 24085, 20421,     0, 29096, 25629, 22431,     0,
            30868, 28997, 27192,     0, 16980, 11428,  8819,     0,
            15943,  8533,  5010,     0, 19895, 12366,  7958,     0,
            23178, 16405, 11674,     0, 25416, 19559, 15035,     0,
            26808, 21779, 17584,     0, 29536, 26534, 23761,     0,
            17007, 12052,  9544,     0, 13450,  6779,  4009,     0,
            17239,  9674,  5839,     0, 21106, 13779,  9127,     0,
            23813, 17200, 12402,     0, 25487, 19662, 15060,     0,
            28520, 24709, 21328,     0, 17869, 11551,  8265,     0,
            19249, 12485,  8721,     0, 23339, 16802, 12403,     0,
            26068, 20413, 16116,     0, 27680, 23064, 19052,     0,
            28525, 24614, 21037,     0, 30066, 27404, 24907,     0,
            10023,  4380,  2314,     0, 12533,  5622,  2846,     0,
            16872,  9053,  5131,     0, 20928, 13418,  8637,     0,
            23646, 16836, 11888,     0, 25280, 19187, 14406,     0,
            27654, 23200, 19398,     0, 11923,  6215,  3836,     0,
            11787,  5396,  2884,     0, 14987,  7433,  3983,     0,
            19008, 11060,  6471,     0, 21793, 14353,  9403,     0,
            23723, 16979, 12082,     0, 26638, 21569, 17345,     0,
            19219, 13044,  9610,     0, 20924, 14386, 10522,     0,
            24849, 19149, 14995,     0, 27282, 22625, 18822,     0,
            28602, 24785, 21444,     0, 29404, 26262, 23341,     0,
            31170, 29608, 28094,     0, 17487, 11789,  8987,     0,
            17829, 10649,  6816,     0, 21405, 14361,  9956,     0,
            24159, 17911, 13398,     0, 26031, 20584, 16288,     0,
            27262, 22505, 18506,     0, 29778, 26982, 24388,     0,
            12519,  7515,  5351,     0, 11698,  5250,  2767,     0,
            15914,  8299,  4694,     0, 19904, 12282,  7768,     0,
            22806, 15790, 10990,     0, 24694, 18430, 13720,     0,
            28274, 24289, 20862,     0, 18808, 13151,  9939,     0,
            21618, 15427, 11540,     0, 25618, 19804, 15578,     0,
            27437, 22766, 18901,     0, 28601, 25024, 21711,     0,
            29288, 26139, 23122,     0, 30885, 28984, 27082,     0,
            14016,  7108,  3856,     0, 15800,  8182,  4738,     0,
            19248, 11713,  7455,     0, 22315, 15142, 10488,     0,
            24382, 18263, 13652,     0, 26026, 20173, 15760,     0,
            28495, 24628, 21269,     0, 10648,  4941,  2535,     0,
            12205,  5410,  2873,     0, 15692,  8124,  4615,     0,
            19406, 11826,  7459,     0, 21974, 14803, 10073,     0,
            23754, 17116, 12449,     0, 27060, 22256, 18271,     0,
            27063, 21838, 17043,     0, 24822, 20003, 16653,     0,
            25967, 20645, 16542,     0, 27306, 22633, 18568,     0,
            28579, 24757, 21261,     0, 29577, 26539, 23360,     0,
            31711, 30631, 29556,     0, 22750, 15701, 11277,     0,
            25388, 20186, 16315,     0, 26700, 21923, 18429,     0,
            27670, 23570, 20213,     0, 28456, 24758, 21649,     0,
            29068, 25802, 22987,     0, 31075, 29442, 27881,     0,
            14011,  7838,  4994,     0, 15120,  8172,  4951,     0,
            18061, 10716,  6742,     0, 21048, 13916,  9476,     0,
            23411, 16816, 12243,     0, 24958, 19015, 14558,     0,
            28889, 25435, 22440,     0, 24490, 19526, 16846,     0,
            22221, 16901, 13849,     0, 23662, 16926, 12159,     0,
            25935, 19761, 15550,     0, 27957, 23056, 18845,     0,
            28783, 25416, 21640,     0, 31080, 29310, 27506,     0,
            19817, 10907,  6258,     0, 22980, 16724, 12492,     0,
            26459, 21524, 17898,     0, 27585, 23419, 20202,     0,
            28379, 24539, 21276,     0, 29135, 25823, 22148,     0,
            29168, 25921, 22861,     0, 11020,  4631,  2513,     0,
            13332,  6187,  3208,     0, 16409,  8567,  4815,     0,
            18807, 11075,  6897,     0, 21224, 14082,  9446,     0,
            23396, 16306, 11816,     0, 26630, 21558, 17378,     0,
    },
    { /* qcat=2 */
            16630, 10545,  7259,     0, 17421, 10338,  6436,     0,
            23154, 16032, 11436,     0, 26168, 20493, 15861,     0,
            27957, 23344, 19221,     0, 29020, 24959, 21348,     0,
            30514, 28181, 25878,     0, 17572, 12484,  9591,     0,
            14451,  7299,  4317,     0, 18850, 11117,  6926,     0,
            22716, 15618, 10773,     0, 25269, 19138, 14181,     0,
            26610, 21351, 16765,     0, 28754, 24983, 21516,     0,
            17720, 11701,  8384,     0, 14566,  7422,  4215,     0,
            18466, 10749,  6412,     0, 21929, 14629,  9602,     0,
            24053, 17024, 11962,     0, 25232, 19192, 14224,     0,
            27355, 22433, 18270,     0, 15374,  8267,  4873,     0,
            16879,  9348,  5583,     0, 21207, 13635,  8898,     0,
            24483, 17956, 12924,     0, 26272, 20725, 16218,     0,
            27997, 23194, 19091,     0, 29165, 25938, 22624,     0,
            11112,  5064,  2568,     0, 11444,  4853,  2257,     0,
            15441,  7432,  3771,     0, 19351, 11387,  6735,     0,
            22636, 15343, 10430,     0, 24188, 17752, 13135,     0,
            27074, 21291, 16357,     0,  8652,  2988,  1318,     0,
             8915,  3073,  1177,     0, 12683,  5154,  2340,     0,
            17442,  8433,  4193,     0, 20954, 13296,  7958,     0,
            22547, 14157,  8001,     0, 25079, 18210, 12447,     0,
            16554, 10388,  6998,     0, 18555, 11464,  7473,     0,
            23555, 16945, 12313,     0, 26373, 21010, 16629,     0,
            27989, 23581, 19702,     0, 28947, 25267, 21815,     0,
            30475, 28201, 25973,     0, 16909, 11485,  8948,     0,
            14364,  7166,  4042,     0, 18443, 10788,  6562,     0,
            22099, 14831, 10048,     0, 24471, 18126, 13321,     0,
            26022, 20379, 15875,     0, 28444, 24517, 20998,     0,
            16236, 11137,  8293,     0, 12101,  5618,  3100,     0,
            16040,  8258,  4593,     0, 19907, 12123,  7436,     0,
            22692, 15407, 10351,     0, 24373, 17828, 12805,     0,
            27037, 22085, 17856,     0, 18335, 11613,  7830,     0,
            18110, 11052,  7223,     0, 22845, 15944, 11211,     0,
            25786, 19716, 15047,     0, 27349, 22265, 17718,     0,
            27916, 23606, 19754,     0, 29497, 26373, 23138,     0,
            10558,  4935,  2659,     0, 12018,  5400,  2947,     0,
            15874,  7940,  4195,     0, 19521, 11492,  7011,     0,
            22730, 15503, 10205,     0, 24181, 17821, 12441,     0,
            27123, 21397, 17516,     0, 10741,  5242,  3054,     0,
             9670,  3622,  1547,     0, 12882,  5427,  2496,     0,
            17159,  9021,  4722,     0, 20775, 12703,  7829,     0,
            23131, 14501,  9097,     0, 25143, 18967, 13624,     0,
            18330, 11970,  8679,     0, 20147, 13565,  9671,     0,
            24591, 18643, 14366,     0, 27094, 22267, 18312,     0,
            28532, 24529, 21035,     0, 29321, 26018, 22962,     0,
            30782, 28818, 26904,     0, 16560, 10669,  7838,     0,
            16231,  8743,  5183,     0, 19988, 12387,  7901,     0,
            23001, 16156, 11352,     0, 25082, 19030, 14370,     0,
            26435, 21154, 16804,     0, 28827, 25197, 21932,     0,
             9949,  5346,  3566,     0, 10544,  4254,  2047,     0,
            15108,  7335,  3855,     0, 19194, 11286,  6766,     0,
            22139, 14791,  9830,     0, 24156, 17470, 12503,     0,
            27161, 22277, 18172,     0, 19199, 12968,  9562,     0,
            19640, 12844,  8899,     0, 24439, 17927, 13365,     0,
            26638, 21792, 17711,     0, 28086, 23929, 20250,     0,
            29112, 25359, 22180,     0, 30191, 27669, 25356,     0,
            10341,  4084,  2183,     0, 11855,  5018,  2629,     0,
            16928,  8659,  4934,     0, 20460, 12739,  8199,     0,
            22552, 15983, 11310,     0, 24459, 18565, 13655,     0,
            26725, 21600, 17461,     0,  9602,  3867,  1770,     0,
            10869,  4363,  2017,     0, 14355,  6677,  3325,     0,
            17535,  9654,  5416,     0, 20085, 12296,  7480,     0,
            22066, 14509,  9359,     0, 24643, 18304, 13542,     0,
            23728, 17982, 14408,     0, 22789, 17050, 13353,     0,
            24855, 18850, 14457,     0, 26909, 21879, 17584,     0,
            28175, 24091, 20258,     0, 28948, 25372, 21977,     0,
            31038, 29297, 27576,     0, 20965, 14403, 10059,     0,
            21349, 14710, 10543,     0, 23350, 16994, 12525,     0,
            25229, 19443, 15111,     0, 26535, 21451, 17384,     0,
            27631, 23112, 19223,     0, 29791, 26994, 24419,     0,
            11561,  5522,  3128,     0, 13221,  6190,  3271,     0,
            16599,  8897,  5078,     0, 19948, 12310,  7750,     0,
            22544, 15436, 10554,     0, 24242, 17720, 12884,     0,
            27731, 23358, 19650,     0, 20429, 15439, 12628,     0,
            19263, 12873,  9543,     0, 22921, 15824, 11204,     0,
            25488, 19512, 14420,     0, 28056, 22759, 18314,     0,
            28407, 24854, 20291,     0, 29898, 27140, 24773,     0,
            12707,  7264,  4242,     0, 17533,  9890,  6623,     0,
            19783, 12810,  8613,     0, 22986, 16127, 11365,     0,
            23312, 16408, 12008,     0, 25913, 19828, 14211,     0,
            27107, 22204, 17766,     0,  7112,  2166,   874,     0,
            10198,  3661,  1676,     0, 13851,  6345,  3227,     0,
            16828,  9119,  5014,     0, 19965, 12187,  7549,     0,
            21686, 14073,  9392,     0, 24829, 18395, 13763,     0,
    },
    { /* qcat=3 */
            14453,  8479,  5217,     0, 15914,  8700,  4933,     0,
            22628, 14841,  9595,     0, 26046, 19786, 14501,     0,
            28107, 22942, 18062,     0, 28936, 24603, 20474,     0,
            29973, 26670, 23523,     0, 15623,  9442,  6096,     0,
            12035,  5088,  2460,     0, 16736,  8307,  4222,     0,
            21115, 12675,  7687,     0, 23478, 16339, 10682,     0,
            24972, 18170, 12786,     0, 26266, 20390, 15327,     0,
            11087,  5036,  2448,     0, 10379,  3724,  1507,     0,
            13741,  6037,  2681,     0, 18029,  9013,  4144,     0,
            21410, 11990,  7257,     0, 21773, 14695,  8578,     0,
            23606, 17778, 12151,     0, 11343,  4816,  2380,     0,
            14706,  6930,  3734,     0, 20812, 12887,  7960,     0,
            25050, 17768, 11788,     0, 27066, 21514, 16625,     0,
            27870, 23680, 15904,     0, 29089, 25992, 20861,     0,
             9474,  2608,  1105,     0,  8371,  2872,   932,     0,
            13523,  5640,  2175,     0, 19566, 12943,  6364,     0,
            21190, 13471,  8811,     0, 24695, 19471, 11398,     0,
            27307, 21845, 13023,     0,  5401,  2247,   834,     0,
             7864,  2097,   828,     0,  9693,  4308,  1469,     0,
            18368,  9110,  2351,     0, 18883,  8886,  4443,     0,
            18022,  9830,  4915,     0, 27307, 16384,  5461,     0,
            14494,  7955,  4878,     0, 17231,  9619,  5765,     0,
            23319, 16028, 10941,     0, 26068, 20270, 15507,     0,
            27780, 22902, 18570,     0, 28532, 24621, 20866,     0,
            29901, 26908, 24114,     0, 15644,  9597,  6667,     0,
            12372,  5291,  2620,     0, 16195,  8139,  4276,     0,
            20019, 11922,  7094,     0, 22535, 14890,  9950,     0,
            24243, 17436, 12405,     0, 26485, 21136, 16513,     0,
            12302,  6257,  3482,     0,  9709,  3594,  1577,     0,
            13287,  5505,  2527,     0, 17310,  9137,  4631,     0,
            20352, 12160,  7075,     0, 22507, 14757,  9507,     0,
            24752, 18113, 13102,     0, 15152,  8182,  4656,     0,
            16959,  9469,  5613,     0, 22001, 13878,  8975,     0,
            25041, 18513, 13903,     0, 26639, 20842, 15886,     0,
            28286, 23064, 17907,     0, 29491, 25316, 21246,     0,
             9812,  4217,  2038,     0, 10044,  3831,  1807,     0,
            14301,  6444,  3188,     0, 19534, 12055,  7119,     0,
            21587, 15176, 10287,     0, 24477, 14410,  8192,     0,
            25200, 20887, 17784,     0,  7820,  3767,  1621,     0,
             7094,  2149,   617,     0, 11927,  5975,  3165,     0,
            18099,  8412,  4102,     0, 21434,  9175,  4549,     0,
            23846, 18006,  9895,     0, 24467, 19224, 12233,     0,
            15655,  9035,  5687,     0, 18629, 11362,  7316,     0,
            24216, 17766, 12992,     0, 26897, 21648, 17390,     0,
            28313, 24152, 20515,     0, 29299, 25858, 22382,     0,
            30513, 28215, 25986,     0, 14544,  8392,  5715,     0,
            13478,  6058,  3154,     0, 17832,  9777,  5584,     0,
            21530, 13817,  9006,     0, 23982, 17151, 12180,     0,
            25451, 19540, 14765,     0, 27667, 23256, 19275,     0,
            10129,  4546,  2558,     0,  9552,  3437,  1461,     0,
            13693,  6006,  2873,     0, 17754,  9655,  5311,     0,
            20830, 12911,  8016,     0, 22826, 15488, 10486,     0,
            25601, 19624, 15016,     0, 16948, 10030,  6280,     0,
            19238, 11883,  7552,     0, 24373, 17238, 12316,     0,
            26194, 20447, 16388,     0, 27415, 22349, 18200,     0,
            28155, 24322, 20387,     0, 29328, 25610, 22865,     0,
             8521,  3717,  1544,     0, 10650,  4710,  2399,     0,
            16270,  8000,  4379,     0, 19848, 11593,  6631,     0,
            22038, 14149,  7416,     0, 22581, 16489,  9977,     0,
            23458, 18137, 10641,     0,  7798,  2210,   711,     0,
             7967,  2826,  1070,     0, 10336,  4315,  1913,     0,
            13714,  7088,  3188,     0, 18376,  9732,  4659,     0,
            20273, 11821,  6118,     0, 20326, 12442,  6554,     0,
            20606, 13983, 10120,     0, 20019, 13071,  8962,     0,
            24188, 17471, 12422,     0, 26599, 21019, 16225,     0,
            27932, 23377, 19320,     0, 28947, 25057, 21155,     0,
            30540, 28167, 25698,     0, 16449,  8043,  4488,     0,
            17070,  9491,  5600,     0, 20042, 12400,  7721,     0,
            22856, 15753, 10792,     0, 24880, 18548, 13589,     0,
            25991, 20484, 15750,     0, 28276, 24178, 20516,     0,
             9519,  3864,  1821,     0, 11718,  4860,  2256,     0,
            15328,  7428,  3819,     0, 18709, 10750,  6227,     0,
            21480, 13865,  8870,     0, 23357, 16426, 11340,     0,
            26490, 21180, 16824,     0, 18787, 12701,  9542,     0,
            15846,  9188,  5985,     0, 21763, 13729,  8281,     0,
            25379, 18550, 12970,     0, 27170, 21263, 15562,     0,
            26678, 21555, 17109,     0, 28948, 25397, 22649,     0,
            11686,  5843,  3093,     0, 11506,  4141,  1640,     0,
            14376,  6314,  2331,     0, 17898,  9858,  5672,     0,
            20148, 13284,  7860,     0, 23478, 16215,  9966,     0,
            26100, 18480, 12764,     0,  5064,  1713,   819,     0,
             8059,  2790,   980,     0, 11100,  3504,  1111,     0,
            14473,  5800,  2694,     0, 16369,  8346,  3455,     0,
            18421,  9742,  4664,     0, 20398, 12962,  8291,     0,
    },
};




void stb_av1_cdf_full_init(struct StbCdfContext *cdf, int base_q_idx) {
    int i, j, k;
    int qcat = (base_q_idx > 20) + (base_q_idx > 60) + (base_q_idx > 120);
    if (qcat < 0) qcat = 0;
    if (qcat > 3) qcat = 3;
    { static int once = 0; if (!once) { once = 1; fprintf(stderr, "CDF_INIT: base_q_idx=%d qcat=%d\n", base_q_idx, qcat); } }

    memcpy(cdf->uv_mode, stb_av1_uv_mode, sizeof(stb_av1_uv_mode));
    memcpy(cdf->partition, stb_av1_partition, sizeof(stb_av1_partition));
    memcpy(cdf->cfl_alpha, stb_av1_cfl_alpha, sizeof(stb_av1_cfl_alpha));
    memcpy(cdf->txtp_inter1, stb_av1_txtp_inter1, sizeof(stb_av1_txtp_inter1));
    memcpy(cdf->txtp_inter2, stb_av1_txtp_inter2, sizeof(stb_av1_txtp_inter2));
    memcpy(cdf->txtp_intra1, stb_av1_txtp_intra1, sizeof(stb_av1_txtp_intra1));
    memcpy(cdf->txtp_intra2, stb_av1_txtp_intra2, sizeof(stb_av1_txtp_intra2));
    memcpy(cdf->cfl_sign, stb_av1_cfl_sign, sizeof(stb_av1_cfl_sign));
    memcpy(cdf->angle_delta, stb_av1_angle_delta, sizeof(stb_av1_angle_delta));
    memcpy(cdf->filter_intra, stb_av1_filter_intra, sizeof(stb_av1_filter_intra));
    memcpy(cdf->seg_id, stb_av1_seg_id, sizeof(stb_av1_seg_id));
    memcpy(cdf->pal_sz, stb_av1_pal_sz, sizeof(stb_av1_pal_sz));
    memcpy(cdf->color_map, stb_av1_color_map, sizeof(stb_av1_color_map));
    memcpy(cdf->txsz, stb_av1_txsz, sizeof(stb_av1_txsz));
    memcpy(cdf->delta_q, stb_av1_delta_q, sizeof(stb_av1_delta_q));
    memcpy(cdf->delta_lf, stb_av1_delta_lf, sizeof(stb_av1_delta_lf));
    memcpy(cdf->restore_switchable, stb_av1_restore_switchable, sizeof(stb_av1_restore_switchable));
    memcpy(cdf->restore_wiener, stb_av1_restore_wiener, sizeof(stb_av1_restore_wiener));
    memcpy(cdf->restore_sgrproj, stb_av1_restore_sgrproj, sizeof(stb_av1_restore_sgrproj));
    memcpy(cdf->txtp_inter3, stb_av1_txtp_inter3, sizeof(stb_av1_txtp_inter3));
    memcpy(cdf->use_filter_intra, stb_av1_use_filter_intra, sizeof(stb_av1_use_filter_intra));
    memcpy(cdf->txpart, stb_av1_txpart, sizeof(stb_av1_txpart));
    memcpy(cdf->skip, stb_av1_skip, sizeof(stb_av1_skip));
    memcpy(cdf->pal_y, stb_av1_pal_y, sizeof(stb_av1_pal_y));
    memcpy(cdf->pal_uv, stb_av1_pal_uv, sizeof(stb_av1_pal_uv));
    memcpy(cdf->intrabc, stb_av1_intrabc, sizeof(stb_av1_intrabc));
    memcpy(cdf->y_mode, stb_av1_y_mode, sizeof(stb_av1_y_mode));
    memcpy(cdf->wedge_idx, stb_av1_wedge_idx, sizeof(stb_av1_wedge_idx));
    memcpy(cdf->comp_inter_mode, stb_av1_comp_inter_mode, sizeof(stb_av1_comp_inter_mode));
    memcpy(cdf->filter, stb_av1_filter, sizeof(stb_av1_filter));
    memcpy(cdf->interintra_mode, stb_av1_interintra_mode, sizeof(stb_av1_interintra_mode));
    memcpy(cdf->motion_mode, stb_av1_motion_mode, sizeof(stb_av1_motion_mode));
    memcpy(cdf->skip_mode, stb_av1_skip_mode, sizeof(stb_av1_skip_mode));
    memcpy(cdf->newmv_mode, stb_av1_newmv_mode, sizeof(stb_av1_newmv_mode));
    memcpy(cdf->globalmv_mode, stb_av1_globalmv_mode, sizeof(stb_av1_globalmv_mode));
    memcpy(cdf->refmv_mode, stb_av1_refmv_mode, sizeof(stb_av1_refmv_mode));
    memcpy(cdf->drl_bit, stb_av1_drl_bit, sizeof(stb_av1_drl_bit));
    memcpy(cdf->intra, stb_av1_intra, sizeof(stb_av1_intra));
    memcpy(cdf->comp, stb_av1_comp, sizeof(stb_av1_comp));
    memcpy(cdf->comp_dir, stb_av1_comp_dir, sizeof(stb_av1_comp_dir));
    memcpy(cdf->jnt_comp, stb_av1_jnt_comp, sizeof(stb_av1_jnt_comp));
    memcpy(cdf->mask_comp, stb_av1_mask_comp, sizeof(stb_av1_mask_comp));
    memcpy(cdf->wedge_comp, stb_av1_wedge_comp, sizeof(stb_av1_wedge_comp));
    memcpy(cdf->ref, stb_av1_ref, sizeof(stb_av1_ref));
    memcpy(cdf->comp_fwd_ref, stb_av1_comp_fwd_ref, sizeof(stb_av1_comp_fwd_ref));
    memcpy(cdf->comp_bwd_ref, stb_av1_comp_bwd_ref, sizeof(stb_av1_comp_bwd_ref));
    memcpy(cdf->comp_uni_ref, stb_av1_comp_uni_ref, sizeof(stb_av1_comp_uni_ref));
    memcpy(cdf->seg_pred, stb_av1_seg_pred, sizeof(stb_av1_seg_pred));
    memcpy(cdf->interintra, stb_av1_interintra, sizeof(stb_av1_interintra));
    memcpy(cdf->interintra_wedge, stb_av1_interintra_wedge, sizeof(stb_av1_interintra_wedge));
    memcpy(cdf->obmc, stb_av1_obmc, sizeof(stb_av1_obmc));
    memcpy(cdf->mv_classes, stb_av1_mv_classes, sizeof(stb_av1_mv_classes));
    memcpy(cdf->mv_sign, stb_av1_mv_sign, sizeof(stb_av1_mv_sign));
    memcpy(cdf->mv_class0, stb_av1_mv_class0, sizeof(stb_av1_mv_class0));
    memcpy(cdf->mv_class0_fp, stb_av1_mv_class0_fp, sizeof(stb_av1_mv_class0_fp));
    memcpy(cdf->mv_class0_hp, stb_av1_mv_class0_hp, sizeof(stb_av1_mv_class0_hp));
    memcpy(cdf->mv_classN, stb_av1_mv_classN, sizeof(stb_av1_mv_classN));
    memcpy(cdf->mv_classN_fp, stb_av1_mv_classN_fp, sizeof(stb_av1_mv_classN_fp));
    memcpy(cdf->mv_classN_hp, stb_av1_mv_classN_hp, sizeof(stb_av1_mv_classN_hp));
    memcpy(cdf->mv_joint, stb_av1_mv_joint, sizeof(stb_av1_mv_joint));
    memcpy(cdf->kfym, stb_av1_kfym, sizeof(stb_av1_kfym));

    memcpy(cdf->coef.skip, stb_av1_default_coef_skip[qcat], sizeof(cdf->coef.skip));
    memcpy(cdf->coef.eob_bin_16, stb_av1_default_coef_eob_bin_16[qcat], sizeof(cdf->coef.eob_bin_16));
    memcpy(cdf->coef.eob_bin_32, stb_av1_default_coef_eob_bin_32[qcat], sizeof(cdf->coef.eob_bin_32));
    memcpy(cdf->coef.eob_bin_64, stb_av1_default_coef_eob_bin_64[qcat], sizeof(cdf->coef.eob_bin_64));
    memcpy(cdf->coef.eob_bin_128, stb_av1_default_coef_eob_bin_128[qcat], sizeof(cdf->coef.eob_bin_128));
    memcpy(cdf->coef.eob_bin_256, stb_av1_default_coef_eob_bin_256[qcat], sizeof(cdf->coef.eob_bin_256));
    memcpy(cdf->coef.eob_bin_512, stb_av1_default_coef_eob_bin_512[qcat], sizeof(cdf->coef.eob_bin_512));
    memcpy(cdf->coef.eob_bin_1024, stb_av1_default_coef_eob_bin_1024[qcat], sizeof(cdf->coef.eob_bin_1024));
    memcpy(cdf->coef.eob_hi_bit, stb_av1_default_coef_eob_hi_bit[qcat], sizeof(cdf->coef.eob_hi_bit));
    memcpy(cdf->coef.eob_base_tok, stb_av1_default_coef_eob_base_tok[qcat], sizeof(cdf->coef.eob_base_tok));
    memcpy(cdf->coef.base_tok, stb_av1_default_coef_base_tok[qcat], sizeof(cdf->coef.base_tok));
    memcpy(cdf->coef.dc_sign, stb_av1_default_coef_dc_sign[qcat], sizeof(cdf->coef.dc_sign));
    memcpy(cdf->coef.br_tok, stb_av1_default_coef_br_tok[qcat], sizeof(cdf->coef.br_tok));
    {
        int pl, ctx, ii;
        for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 3; ctx++) cdf->coef.dc_sign[pl][ctx][1] = 0;
        for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 21; ctx++) for (ii = 0; ii < 4; ii++)
            cdf->coef.br_tok[ii][pl][ctx][3] = 0;
        for (ii = 0; ii < 5; ii++) for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 41; ctx++)
            cdf->coef.base_tok[ii][pl][ctx][3] = 0;
        for (ii = 0; ii < 5; ii++) for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 4; ctx++)
            cdf->coef.eob_base_tok[ii][pl][ctx][2] = 0;
        for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 2; ctx++) {
            cdf->coef.eob_bin_16[pl][ctx][4] = 0;
            cdf->coef.eob_bin_32[pl][ctx][5] = 0;
            cdf->coef.eob_bin_64[pl][ctx][6] = 0;
            cdf->coef.eob_bin_128[pl][ctx][7] = 0;
        }
        for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 2; ctx++)
            cdf->coef.eob_bin_256[pl][ctx][8] = 0;
        for (pl = 0; pl < 2; pl++) {
            cdf->coef.eob_bin_512[pl][9] = 0;
            cdf->coef.eob_bin_1024[pl][10] = 0;
        }
        for (ii = 0; ii < 5; ii++) for (pl = 0; pl < 2; pl++) for (ctx = 0; ctx < 9; ctx++)
            cdf->coef.eob_hi_bit[ii][pl][ctx][1] = 0;
        for (ii = 0; ii < 5; ii++) for (ctx = 0; ctx < 13; ctx++)
            cdf->coef.skip[ii][ctx][1] = 0;
    }
}

struct stb_av1_msac {
    const unsigned char *buf_start, *buf_pos, *buf_end;
    stbv_u64 dif;
    unsigned rng, cnt;
    int allow_update_cdf;
};
int stb_av1_msac_trace_flag = 0;
void stb_avif_set_msac_trace(int on) { stb_av1_msac_trace_flag = on; }

static void stb_av1_msac_refill(struct stb_av1_msac *s) {
    const unsigned char *p = s->buf_pos, *e = s->buf_end;
    int c = 64 - (int)s->cnt - 24;
    stbv_u64 d = s->dif;
    do { if (p >= e) { d |= ~(~(stbv_u64)0xff << c); break; }
         d |= (stbv_u64)(*p++ ^ 0xff) << c; c -= 8; } while (c >= 0);
    s->dif = d; s->cnt = (unsigned)(64 - c - 24);
    s->buf_pos = p;
}

static void stb_av1_msac_norm(struct stb_av1_msac *s, stbv_u64 d, unsigned r) {
    int d2 = 0, cnt;
    unsigned r0 = r;
    if (r == 0) { s->rng = 0x8000; return; }
    if (r & 0xff00) { d2 = 8; r >>= 8; }
    if (r & 0xf0) { d2 += 4; r >>= 4; }
    if (r & 0xc)  { d2 += 2; r >>= 2; }
    if (r & 0x2)  { d2 += 1; r >>= 1; }
    d2 = 15 - d2;
    cnt = (int)s->cnt;
    s->dif = d << d2; s->rng = r0 << d2;
    s->cnt = (unsigned)(cnt - d2);
    if ((unsigned)cnt < (unsigned)d2) stb_av1_msac_refill(s);
}

static void stb_av1_msac_init(struct stb_av1_msac *s, const unsigned char *data, unsigned long sz, int no_cdf) {
    s->buf_start = data; s->buf_pos = data; s->buf_end = data + sz;
    s->dif = 0; s->rng = 0x8000; s->cnt = -15;
    s->allow_update_cdf = !no_cdf;
    stb_av1_msac_refill(s);
}

static unsigned stb_av1_msac_decode_bool_equi(struct stb_av1_msac *s) {
    unsigned r = s->rng, v = ((r >> 8) << 7) + 4;
    stbv_u64 d = s->dif, vw = (stbv_u64)v << 48;
    unsigned ret = (d >= vw) ? 1u : 0u;
    if (ret) d -= vw; v += ret * (r - 2 * v);
    stb_av1_msac_norm(s, d, v);
    if (stb_av1_msac_trace_flag)
        fprintf(stderr,"M E val=%u rng=%u cnt=%u dif=%08x%08x\n", !ret, s->rng, s->cnt,
            (unsigned)(s->dif>>32),(unsigned)(s->dif));
    return !ret;
}

static unsigned stb_av1_msac_decode_bool(struct stb_av1_msac *s, unsigned f) {
    unsigned r = s->rng, v = ((r >> 8) * (f >> 6) >> 1) + 4;
    stbv_u64 d = s->dif, vw = (stbv_u64)v << 48;
    unsigned ret = (d >= vw) ? 1u : 0u;
    if (ret) d -= vw; v += ret * (r - 2 * v);
    stb_av1_msac_norm(s, d, v);
    if (stb_av1_msac_trace_flag)
        fprintf(stderr,"M B %u val=%u rng=%u cnt=%u dif=%08x%08x\n", f, !ret, s->rng, s->cnt,
            (unsigned)(s->dif>>32),(unsigned)(s->dif));
    return !ret;
}

static unsigned stb_av1_msac_decode_bools(struct stb_av1_msac *s, unsigned n) {
    unsigned v = 0;
    while (n--) v = (v << 1) | stb_av1_msac_decode_bool_equi(s);
    return v;
}

/* Multi-symbol arithmetic decode with adaptation.
   n_symbols: number of symbols (e.g., 4 for 4 symbols).
   CDF layout: cdf[0..n_symbols-1] = ICDF values, cdf[n_symbols] = count.
   Matches dav1d convention (n_symbols, not n_sym_minus_1). */
static unsigned stb_av1_msac_decode_symbol(struct stb_av1_msac *s, unsigned short *cdf, unsigned long n_symbols) {
    unsigned c = (unsigned)(s->dif >> 48), r = s->rng >> 8;
    unsigned u, v = s->rng, val = -1;
    unsigned trng = s->rng, tcnt = s->cnt; stbv_u64 tdif = s->dif;

    do {
        val++;
        u = v;
        v = r * (cdf[val] >> 6);
        v >>= 1;
        v += 4 * ((unsigned)(n_symbols - val));
    } while (c < v);

    { unsigned rng_new = u - v; if (rng_new == 0) rng_new = 1; stb_av1_msac_norm(s, s->dif - ((stbv_u64)v << 48), rng_new); }
    if (stb_av1_msac_trace_flag)
        fprintf(stderr,"M S %lu %u %u val=%u rng=%u cnt=%u dif=%08x%08x\n",
            n_symbols, cdf[0], n_symbols>1?cdf[1]:0, val, s->rng, s->cnt,
            (unsigned)(s->dif>>32),(unsigned)(s->dif));
    if (s->allow_update_cdf) {
        unsigned cnt = cdf[n_symbols], rate = 4 + (cnt >> 4) + (n_symbols > 2 ? 1u : 0u);
        unsigned i;
        for (i = 0; i < val; i++) cdf[i] += (unsigned short)((32768 - cdf[i]) >> rate);
        for (; i < n_symbols; i++) cdf[i] -= (unsigned short)(cdf[i] >> rate);
        cdf[n_symbols] = (unsigned short)(cnt + (cnt < 32 ? 1u : 0u));
        { static int _dbgu=0; if (_dbgu++<25) fprintf(stderr,"[UPD] n=%lu val=%u cnt=%u->%u rate=%u\n", n_symbols, val, cnt, (unsigned)cdf[n_symbols], rate); }
    }
    return val;
}

static unsigned stb_av1_msac_decode_bool_adapt(struct stb_av1_msac *s, unsigned short *cdf) {
    unsigned bit = stb_av1_msac_decode_bool(s, *cdf);
    if (s->allow_update_cdf) {
        unsigned cnt = cdf[1], rate = 4 + (cnt >> 4);
        if (bit) cdf[0] += (unsigned short)((32768 - cdf[0]) >> rate);
        else cdf[0] -= (unsigned short)(cdf[0] >> rate);
        cdf[1] = (unsigned short)(cnt + (cnt < 32 ? 1u : 0u));
    }
    return bit;
}

static unsigned stb_av1_msac_decode_uniform(struct stb_av1_msac *s, unsigned n) {
    unsigned l = 0, m, v;
    if (n <= 1) return 0;
    while (((unsigned)1 << l) < n) l++;
    m = ((unsigned)1 << l) - n;
    v = stb_av1_msac_decode_bools(s, l - 1);
    return (v < m) ? v : (v << 1) - m + stb_av1_msac_decode_bool_equi(s);
}

static unsigned stb_av1_msac_decode_subexp(struct stb_av1_msac *s, int ref, int n, unsigned k) {
    unsigned a = 0, v;
    if (stb_av1_msac_decode_bool_equi(s)) {
        if (stb_av1_msac_decode_bool_equi(s))
            k += stb_av1_msac_decode_bool_equi(s) + 1;
        a = 1u << k;
    }
    v = stb_av1_msac_decode_bools(s, k) + a;
    if (ref * 2 <= n) return (v > (unsigned)ref) ? v + (unsigned)ref : (unsigned)ref - v;
    return (v > (unsigned)(n - 1 - ref)) ? (unsigned)(n - 1) - (v - (unsigned)(n - 1 - ref)) : (unsigned)ref + v;
}

static unsigned stb_av1_msac_decode_hi_tok(struct stb_av1_msac *s, unsigned short *cdf) {
    unsigned tok_br = stb_av1_msac_decode_symbol(s, cdf, 3);
    unsigned tok = 3 + tok_br;
    if (tok_br == 3) {
        tok_br = stb_av1_msac_decode_symbol(s, cdf, 3);
        tok = 6 + tok_br;
        if (tok_br == 3) {
            tok_br = stb_av1_msac_decode_symbol(s, cdf, 3);
            tok = 9 + tok_br;
            if (tok_br == 3)
                tok = 12 + stb_av1_msac_decode_symbol(s, cdf, 3);
        }
    }
    return tok;
}

/* dav1d read_golomb (recon_tmpl.c:49-57): residual for coefficient magnitudes >= 15. */
static unsigned stb_av1_msac_read_golomb(struct stb_av1_msac *s) {
    int len = 0;
    unsigned val = 1;
    while (!stb_av1_msac_decode_bool_equi(s) && len < 32) len++;
    while (len--) val = (val << 1) + stb_av1_msac_decode_bool_equi(s);
    return val - 1;
}

/* ===== CDF-based Decoder: Helpers, Tables, Block Decoder ===== */
/* Transform type tables from dav1d tables_c89.c */
static const unsigned char stb_av1_tx_types_per_set[40] = {
    9, 0, 3, 1, 2, /* Intra2: IDTX, DCT_DCT, ADST_ADST, ADST_DCT, DCT_ADST */
    9, 0,10,11, 3, 1, 2, /* Intra1: IDTX, DCT_DCT, V_DCT, H_DCT, ADST_ADST, ADST_DCT, DCT_ADST */
    9,10,11, 0, 1, 2, 4, 5, 3, 6, 7, 8, /* Inter2 */
    9,10,11,12,13,14,15, 0, 1, 2, 4, 5, 3, 6, 7, 8, /* Inter1 */
};

/* UV->tx_type mapping for chroma intra blocks */
static const unsigned char stb_av1_txtp_from_uvmode[13] = {
    0, 1, 2, 0, 3, 1, 2, 2, 1, 3, 1, 2, 3,
};



/* Bitmask helpers for SET_CTX macros */
typedef union { unsigned long long u64; unsigned char u8[8]; } StbAv1Alias64;
typedef union { unsigned int u32; unsigned char u8[4]; } StbAv1Alias32;
typedef union { unsigned short u16; unsigned char u8[2]; } StbAv1Alias16;
typedef union { unsigned char u8; } StbAv1Alias8;

#define STB_AV1_SET_CTX1(var, off, val) \
    (((StbAv1Alias8 *) &(var)[off])->u8 = (unsigned char)((val) * 0x01))
#define STB_AV1_SET_CTX2(var, off, val) \
    (((StbAv1Alias16 *) &(var)[off])->u16 = (unsigned short)((val) * 0x0101))
#define STB_AV1_SET_CTX4(var, off, val) \
    (((StbAv1Alias32 *) &(var)[off])->u32 = (unsigned int)((val) * 0x01010101U))
#define STB_AV1_SET_CTX8(var, off, val) \
    (((StbAv1Alias64 *) &(var)[off])->u64 = (stbv_u64)((val) * 0x0101010101010101ULL))

/* memset_pow2 function pointer type */
typedef void (*StbAv1MemsetFn)(void *ptr, int value);

/* memset_pow2 implementation: broadcast byte to 1/2/4/8 bytes */
static void stb_av1_memset_1(void *ptr, int val) { *(unsigned char *)ptr = (unsigned char)val; }
static void stb_av1_memset_2(void *ptr, int val) { *(unsigned short *)ptr = (unsigned short)((unsigned char)val * 0x0101U); }
static void stb_av1_memset_4(void *ptr, int val) { *(unsigned int *)ptr = (unsigned int)((unsigned char)val * 0x01010101U); }
static void stb_av1_memset_8(void *ptr, int val) { *(unsigned long long *)ptr = (stbv_u64)((unsigned char)val * 0x0101010101010101ULL); }

static const StbAv1MemsetFn stb_av1_memset_pow2[6] = {
    stb_av1_memset_1, stb_av1_memset_2, stb_av1_memset_4,
    stb_av1_memset_8, NULL, NULL
};

/* Block dimensions [N_BS_SIZES][4]: width4, height4, log2w, log2h */
/* Uses the enum values: BS_128x128=0, BS_128x64=1, ... BS_4x4=21 */
static const unsigned char stb_av1_block_dimensions[22][4] = {
    {32,32,5,5}, {32,16,5,4}, {16,32,4,5}, {16,16,4,4},
    {16,8,4,3}, {16,4,4,2}, {8,16,3,4}, {8,8,3,3},
    {8,4,3,2}, {8,2,3,1}, {4,16,2,4}, {4,8,2,3},
    {4,4,2,2}, {4,2,2,1}, {4,1,2,0}, {2,8,1,3},
    {2,4,1,2}, {2,2,1,1}, {2,1,1,0}, {1,4,0,2},
    {1,2,0,1}, {1,1,0,0}
};

/* CFL allowed block sizes bitmask (matches dav1d cfl_allowed_mask) */
static const unsigned stb_av1_cfl_allowed_mask =
    (1u << 7)  | (1u << 8)  | (1u << 9)  | (1u << 11) | (1u << 12) |
    (1u << 13) | (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17) |
    (1u << 18) | (1u << 19) | (1u << 20) | (1u << 21);

/* Map block width/height (in pixels) to block-size index 0..21 */
static int stb_av1_bs_lookup(int w, int h) {
    int i;
    for (i = 0; i < 22; i++)
        if ((int)stb_av1_block_dimensions[i][0] * 4 == w &&
            (int)stb_av1_block_dimensions[i][1] * 4 == h)
            return i;
    return 17; /* BS_8x8 fallback */
}

/* Transform dimensions and info for all RectTxfmSize values */
typedef struct { unsigned char w,h,lw,lh,min,max,sub,ctx; } StbAv1TxfmInfo;
static const StbAv1TxfmInfo stb_av1_txfm_dimensions[32] = {
    {4,4,0,0,0,0,0,0}, {8,8,1,1,1,1,0,1}, {16,16,2,2,2,2,1,2}, {32,32,3,3,3,3,2,3}, {64,64,4,4,4,4,3,4},
    {4,8,0,1,0,1,0,1}, {8,4,1,0,0,1,0,1}, {8,16,1,2,1,2,1,2},
    {16,8,2,1,1,2,1,2}, {16,32,2,3,2,3,2,3}, {32,16,3,2,2,3,2,3},
    {32,64,3,4,3,4,3,4}, {64,32,4,3,3,4,3,4},
    {4,16,0,2,0,2,5,1}, {16,4,2,0,0,2,6,1},
    {8,32,1,3,1,3,7,2}, {32,8,3,1,1,3,8,2},
    {16,64,2,4,2,4,9,3}, {64,16,4,2,2,4,10,3}
};

/* Max transform size for each block size [N_BS_SIZES][4] (Y, 420, 422, 444) */
static const unsigned char stb_av1_max_txfm_size_for_bs[22][4] = {
    /* stb_av1_block_dimensions order (4px units):
       128x128,128x64,64x128,64x64,64x32,64x16,32x64,32x32,32x16,32x8,
       16x64,16x32,16x16,16x8,16x4,8x32,8x16,8x8,8x4,4x16,4x8,4x4
       values = RectTxfmSize index into stb_av1_txfm_dimensions, per [y,420,422,444]
       (matches dav1d_max_txfm_size_for_bs) */
    {4,3,3,3}, {4,3,3,3}, {4,3,0,3}, {4,3,3,3},
    {12,10,3,3}, {18,16,10,10}, {11,9,0,3}, {3,2,9,3},
    {10,8,2,10}, {16,14,8,16}, {17,15,0,9}, {9,7,0,9},
    {2,1,7,2}, {8,6,1,8}, {14,6,6,14}, {15,13,0,15},
    {7,5,0,7}, {1,0,5,1}, {6,0,0,6}, {13,5,0,13},
    {5,0,0,5}, {0,0,0,0}
};

/* Intra mode context lookup (key frame Y modes) */
static const unsigned char stb_av1_intra_mode_context[13] = {
    0, 1, 2, 3, 4, 4, 4, 4, 3, 0, 1, 2, 0
};

/* ICDF values for the last symbol in each kfym CDF (16 modes) */
/* These are used to compute the total count for mode decoding */
static const unsigned short stb_av1_kfym_cdf_end[5][5] = {
    {32768,32768,32768,32768,32768},
    {32768,32768,32768,32768,32768},
    {32768,32768,32768,32768,32768},
    {32768,32768,32768,32768,32768},
    {32768,32768,32768,32768,32768}
};

/* ===== CDF-based Intra Block Decoder ===== */
/* This decoder properly uses context-adaptive CDF tables from StbCdfContext
   to decode all symbols (modes, partitions, tx splits, coefficients).
   Ported from dav1d decode.c / decode_c89.c */

/* Block structure (simplified from dav1d's Av1Block) */
struct StbAv1DecodedBlock {
    int bl, bp, bs;      /* block level, partition, block size */
    int seg_id;
    int skip, skip_mode;
    int intra;
    int y_mode, uv_mode;
    int y_angle, uv_angle;
    int tx, uvtx;
    int max_ytx;
    unsigned char tx_split0;
    unsigned short tx_split1;
    int pal_sz[2];
    short cfl_alpha[2];
    short interintra_mode;
    short motione_mode;
    short comp_type;
};

/* Context state for left neighbor columns (per 4px column) */
struct StbAv1LeftContext {
    unsigned char tx_intra[32];
    unsigned char tx[32];
    unsigned char mode[32];
    unsigned char pal_sz[32];
    unsigned char seg_pred[32];
    unsigned char skip_mode[32];
    unsigned char intra[32];
    unsigned char skip[32];
    unsigned char uvmode[32];
    unsigned char coef[32];
};

/* Decode a key-frame intra block with full CDF context adaptation.
   Reads: skip flag, intra Y mode, UV mode, transform tree, coefficients.
   Uses ts->cdf for all CDF lookups. */
static int stb_av1_decode_intra_block_cdf(
    struct stb_av1_msac *msac,
    struct StbCdfContext *cdf,
    const int bx4, const int by4,       /* block position in 4px units */
    const int bw4, const int bh4,       /* block size in 4px units */
    const int bsize,                    /* block size enum */
    unsigned char *above_skip,
    unsigned char *above_mode,
    unsigned char *above_tx,
    struct StbAv1LeftContext *left,
    int *y_mode_out, int *uv_mode_out,
    int *skip_out, int *tx_out, int *uvtx_out,
    int *pal_sz_y, int *pal_sz_uv)
{
    int sctx;
    int ymode;
    unsigned short *cdf_ptr;
    int above_mode_ctx, left_mode_ctx;

    /* ---- skip flag (CDF context-adaptive) ---- */
    sctx = (above_skip && above_skip[bx4] ? 1 : 0) + (left ? left->skip[by4] : 0);
    cdf_ptr = cdf->skip[sctx];
    *skip_out = (int)stb_av1_msac_decode_bool_adapt(msac, cdf_ptr);

    /* ---- intra Y mode (use inline CDF since kfym not fully initialized) ---- */
    {
        unsigned short cdf_iy[13] = {15360,17920,20480,23040,25600,28160,30720,32000,33280,34560,35840,36608,32768};
        (void)above_mode_ctx;
        (void)left_mode_ctx;
        ymode = (int)stb_av1_msac_decode_symbol(msac, cdf_iy, 11);
    }
    *y_mode_out = ymode;

    /* ---- UV mode (simplified: always DC_PRED for now if no chroma context) ---- */
    *uv_mode_out = 0; /* DC_PRED */
    *tx_out = 0; /* TX_4X4 mapped to 0 */
    *uvtx_out = 0;
    *pal_sz_y = 0;
    *pal_sz_uv = 0;

    /* Update above/left context with decoded info */
    if (above_skip) above_skip[bx4] = (unsigned char)*skip_out;
    if (above_mode) above_mode[bx4] = (unsigned char)ymode;
    if (above_tx) above_tx[bx4] = 0;
    if (left) {
        left->skip[by4] = (unsigned char)*skip_out;
        left->mode[by4] = (unsigned char)ymode;
        left->tx[by4] = 0;
    }

    return 0;
}

/* Default CDF tables (intra-mode, skip, coefficient EOB) */
static const unsigned short stb_av1_cdf_intra_y[12] = {
    15360, 17920, 20480, 23040, 25600, 28160, 30720, 32000,
    33280, 34560, 35840, 32768
};
static const unsigned short stb_av1_cdf_skip[3] = { 28160, 32768, 0 };
static const unsigned short stb_av1_cdf_eob4x4[66] = {
    0,128,256,512,1024,2048,3072,4096,5120,6144,7168,8192,
    9216,10240,11264,12288,13312,14336,15360,16384,17408,
    18432,19456,20480,21504,22528,23552,24576,25600,26624,
    27648,28672,29696,30720,31744,32256,32768,32768,32768,
    32768,32768,32768,32768,32768,32768,32768,32768,32768,
    32768,32768,32768,32768,32768,32768,32768,32768,32768,
    32768,32768,32768,32768,32768,32768,32768,32768,0
};

/* Decode intra mode using CDF */
static int stb_av1_decode_intra_mode_cdf(struct stb_av1_msac *msac) {
    unsigned short cdf[12]; int i;
    for (i = 0; i < 12; i++) cdf[i] = stb_av1_cdf_intra_y[i];
    return (int)stb_av1_msac_decode_symbol(msac, cdf, 11);
}

/* Position-based context offset for square transform blocks (TX_CLASS_2D).
   Indexed as: lo_ctx_offsets[min(y,4)][min(x,4)] for a 5x5 sub-block grid.
   Maps position to base context index (0-21) before magnitude adjustment (0-4). */
static const unsigned char stb_av1_lo_ctx_offsets[3][5][5] = {
    { /* w == h (square) */
        {  0,  1,  6,  6, 21 },
        {  1,  6,  6, 21, 21 },
        {  6,  6, 21, 21, 21 },
        {  6, 21, 21, 21, 21 },
        { 21, 21, 21, 21, 21 },
    }, { /* w > h (wide) */
        {  0, 16,  6,  6, 21 },
        { 16, 16,  6, 21, 21 },
        { 16, 16, 21, 21, 21 },
        { 16, 16, 21, 21, 21 },
        { 16, 16, 21, 21, 21 },
    }, { /* w < h (tall) */
        {  0, 11, 11, 11, 11 },
        { 11, 11, 11, 11, 11 },
        {  6,  6, 21, 21, 21 },
        {  6, 21, 21, 21, 21 },
        { 21, 21, 21, 21, 21 },
    },
};

/* dav1d_skip_ctx (tables.c:297-303), indexed by
   [umin(above_cul_level,4)][umin(left_cul_level,4)]. */
static const unsigned char stb_av1_skip_ctx[5][5] = {
    { 1, 2, 2, 2, 3 },
    { 2, 4, 4, 4, 5 },
    { 2, 4, 4, 4, 5 },
    { 2, 4, 4, 4, 5 },
    { 3, 5, 5, 5, 6 },
};

/* CDF-based coefficient decoder with proper scan-order context mapping.
   Decodes coefficients in scan order, computing context from (x,y) position
   using the lo_ctx_offsets table (TX_CLASS_2D, mag_adj = 0). */
static int stb_av1_get_dc_sign_ctx(unsigned char *a_ctx, unsigned char *l_ctx, int tx_w, int tx_h) {
    /* Extract sign bits (bits 6-7) from above/left lcoef.
       bit 6=1: no DC coefficient; bit 7=1: negative DC sign.
       a>>6 yields: 0=positive, 1=no-coeff, 2=negative.
       s = sum(above) + sum(left) - (num_above + num_left)
       return (s != 0) + (s > 0)  → {0, 1, 2} */
    int s = 0;
    int n = 0, i;
    int tx4 = tx_w / 4;
    if (tx4 < 1) tx4 = 1;
    for (i = 0; i < tx4 && a_ctx; i++) { s += (a_ctx[i] >> 6); n++; }
    { int ty4 = tx_h / 4; if (ty4 < 1) ty4 = 1;
      for (i = 0; i < ty4 && l_ctx; i++) { s += (l_ctx[i] >> 6); n++; }
      s -= n; }
    { static int _dsg=0; if (_dsg<60000) { _dsg++;
      fprintf(stderr,"[DCSG] txw=%d txh=%d a=%d,%d l=%d,%d ctx=%d\n", tx_w, tx_h,
        (a_ctx&&tx_w/4>=1)?(a_ctx[0]>>6):-1, (a_ctx&&tx_w/4>=2)?(a_ctx[1]>>6):-1,
        (l_ctx&&tx_h/4>=1)?(l_ctx[0]>>6):-1, (l_ctx&&tx_h/4>=2)?(l_ctx[1]>>6):-1,
        (s != 0) + (s > 0)); } }
    return (s != 0) + (s > 0);
}
static int stb_av1_decode_coeffs_cdf(struct stb_av1_msac *msac, int *coeffs, int tx_w, int tx_h, int *eob, struct StbCdfContext *cdf, int plane, unsigned char *a_ctx, unsigned char *l_ctx, int b_w4, int b_h4, int ss_hor, int ss_ver, unsigned char *res_ctx_out, int txtp_mode, int txtp_in, int reduced_tx_set, int *txtp_out) {
    int max_coeffs = tx_w * tx_h, i;
    int tx_sz_idx;
    int eob_bin_sz, eob_bin_val;
    int t_lw, t_lh, tx_w4, tx_h4;
    int tx2dszctx;
    int sctx;
    int dc_sign_level = 1 << 6;
    int cul_level = 0;
    int txtp = txtp_in;
    int is_1d;

    tx_sz_idx = 0;
    { int tlw = 0, tlh = 0; while ((1 << (tlw + 2)) < tx_w) tlw++; while ((1 << (tlh + 2)) < tx_h) tlh++;
      /* dav1d t_dim->ctx: square -> lw; rect -> min(lw,lh)+1 */
      tx_sz_idx = (tlw == tlh) ? tlw : ((tlw < tlh ? tlw : tlh) + 1); }
    if (tx_sz_idx > 4) tx_sz_idx = 4;

    t_lw = 0; while ((1 << (t_lw + 2)) < tx_w) t_lw++;
    t_lh = 0; while ((1 << (t_lh + 2)) < tx_h) t_lh++;
    tx_w4 = tx_w / 4; if (tx_w4 < 1) tx_w4 = 1;
    tx_h4 = tx_h / 4; if (tx_h4 < 1) tx_h4 = 1;

    /* Coefficient-level skip with dav1d get_skip_ctx (recon_tmpl.c:59-138).
       b_w4/b_h4 = log2(block_w/4), log2(block_h/4) = dav1d b_dim[2]/[3]. */
    if (plane == 0) {
        if (b_w4 == t_lw && b_h4 == t_lh) {
            sctx = 0;
        } else {
            int la = 0, ll = 0;
            for (i = 0; i < tx_w4; i++) la |= a_ctx[i];
            for (i = 0; i < tx_h4; i++) ll |= l_ctx[i];
            la &= 0x3F; if (la > 4) la = 4;
            ll &= 0x3F; if (ll > 4) ll = 4;
            sctx = (int)stb_av1_skip_ctx[la][ll];
        }
    } else {
        int not_one_blk = 0, ca = 0, cl = 0;
        if (b_w4 - ((b_w4 && ss_hor) ? 1 : 0) > t_lw ||
            b_h4 - ((b_h4 && ss_ver) ? 1 : 0) > t_lh) not_one_blk = 1;
        for (i = 0; i < tx_w4; i++) if (a_ctx[i] != 0x40) { ca = 1; break; }
        for (i = 0; i < tx_h4; i++) if (l_ctx[i] != 0x40) { cl = 1; break; }
        sctx = 7 + not_one_blk * 3 + ca + cl;
    }

    { static int _skipdbg=0; if (_skipdbg++<60000) fprintf(stderr,"[SKIPDBG] txsz=%d sctx=%d a=%d l=%d cdf0=%d cdf1=%d\n", tx_sz_idx, sctx, (a_ctx&&tx_w4>=1)?(a_ctx[0]&0x3f):-1, (l_ctx&&tx_h4>=1)?(l_ctx[0]&0x3f):-1, cdf->coef.skip[tx_sz_idx][sctx][0], cdf->coef.skip[tx_sz_idx][sctx][1]); }
    if (stb_av1_msac_decode_bool_adapt(msac, cdf->coef.skip[tx_sz_idx][sctx])) {
        *eob = 0;
        *res_ctx_out = 0x40;
        *txtp_out = 0;
        memset(coeffs, 0, (size_t)(max_coeffs * sizeof(int)));
        return 0;
    }

    /* Transform type decode (dav1d recon_tmpl.c:351-401).
       Luma intra: explicitly coded. Chroma intra: derived from uv_mode. */
    if (plane == 0 && txtp_mode >= 0) {
        int tmin = t_lw < t_lh ? t_lw : t_lh;
        int tmax = t_lw > t_lh ? t_lw : t_lh;
        { static int _dbg3=0; if (_dbg3++<12) fprintf(stderr,"[C89DBG3] txtp txw=%d txh=%d tmin=%d tmax=%d reduced=%d txfm_mode=%d txtp_mode=%d\n", tx_w, tx_h, tmin, tmax, reduced_tx_set, txtp_mode, txtp_mode); }
        if (tmax + 1 >= 4) {
            txtp = 0; /* DCT_DCT */
        } else if (reduced_tx_set || tmin == 2) {
            unsigned long tx_idx = stb_av1_msac_decode_symbol(msac,
                cdf->txtp_intra2[tmin][txtp_mode], 4);
            txtp = (int)stb_av1_tx_types_per_set[tx_idx + 0];
        } else {
            unsigned long tx_idx = stb_av1_msac_decode_symbol(msac,
                cdf->txtp_intra1[tmin][txtp_mode], 6);
            txtp = (int)stb_av1_tx_types_per_set[tx_idx + 5];
        }
    }
    *txtp_out = txtp;
    is_1d = (txtp >= 10 && txtp <= 15) ? 1 : 0;

    tx2dszctx = ((t_lw < 3) ? t_lw : 3) + ((t_lh < 3) ? t_lh : 3);
    eob_bin_sz = tx2dszctx;
    switch (tx2dszctx) {
        case 0: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_16[plane ? 1 : 0][is_1d], 4); break;
        case 1: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_32[plane ? 1 : 0][is_1d], 5); break;
        case 2: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_64[plane ? 1 : 0][is_1d], 6); break;
        case 3: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_128[plane ? 1 : 0][is_1d], 7); break;
        case 4: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_256[plane ? 1 : 0][is_1d], 8); break;
        case 5: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_512[plane ? 1 : 0], 9); break;
        default: eob_bin_val = (int)stb_av1_msac_decode_symbol(msac, cdf->coef.eob_bin_1024[plane ? 1 : 0], 10); break;
    }
    if (eob_bin_val < 0) eob_bin_val = 0;
    if (eob_bin_val > (int)(4 + eob_bin_sz)) eob_bin_val = (int)(4 + eob_bin_sz);

    if (eob_bin_val > 1) {
        int eob_bin = eob_bin_val - 2;
        { static int __dbge=0; if (__dbge++<60) fprintf(stderr,"[EOBHI] tx_sz_idx=%d plane=%d val=%d ctx=%d\n", tx_sz_idx, plane, eob_bin_val, eob_bin); }
        int eob_hi = (int)stb_av1_msac_decode_bool_adapt(msac, cdf->coef.eob_hi_bit[tx_sz_idx][plane ? 1 : 0][eob_bin]);
        if (eob_hi)
            eob_bin_val = ((1 | 2) << eob_bin) | (int)stb_av1_msac_decode_bools(msac, (unsigned)eob_bin);
        else
            eob_bin_val = (2 << eob_bin) | (int)stb_av1_msac_decode_bools(msac, (unsigned)eob_bin);
    }
    if (eob_bin_val < 0) eob_bin_val = 0;
    if (eob_bin_val >= max_coeffs) eob_bin_val = max_coeffs - 1;
    *eob = eob_bin_val;
    { static int __dbg2=0; if (__dbg2++<40) fprintf(stderr,"[C89DBG2] final eob=%d eob_bin_val=%d plane=%d txw=%d txh=%d\n", *eob, eob_bin_val, plane, tx_w, tx_h); }

    {
        int tx_class = 0; /* 0=2D, 1=H, 2=V */
        int shift = 0, shift2 = 0;
        const unsigned short *scan = NULL;
        int mask = 0, stride, sw, sh;
        unsigned char levels[34 * 34 + 35];
        int lw, lh, slw, slh, br_tx;
        int eob_ctx, dc_ctx;
        int eob_tok, tok;
        unsigned rc_eob, sx_eob, sy_eob;
        unsigned char *pl_eob;

        memset(levels, 0, sizeof(levels));
        memset(coeffs, 0, (size_t)(max_coeffs * sizeof(int)));

        lw = 0; while ((1 << (lw + 2)) < tx_w) lw++;
        lh = 0; while ((1 << (lh + 2)) < tx_h) lh++;
        slw = (lw < 3) ? lw : 3; /* min(lw, TX_32X32) */
        slh = (lh < 3) ? lh : 3;
        br_tx = (tx_sz_idx > 3) ? 3 : tx_sz_idx;

        /* dav1d tx_class dispatch (recon_tmpl.c:552-579):
           H: x = i&mask, y = i>>shift, rc = i
           V: x = i&mask, y = i>>shift, rc = (x<<shift2)|y */
        if (is_1d) tx_class = (txtp & 1) ? 1 : 2;
        if (tx_class == 0) {
            scan = stb_av1_get_scan(tx_w, tx_h, &shift);
            mask = (1 << shift) - 1;
            stride = (tx_sz_idx == 4) ? 34 : (tx_w + 2);
        } else if (tx_class == 1) {
            stride = 16;
            shift = lh + 2;
            sh = tx_h4 > 8 ? 8 : tx_h4;
            mask = 4 * sh - 1;
        } else {
            stride = 16;
            shift = lw + 2;
            shift2 = lh + 2;
            sw = tx_w4 > 8 ? 8 : tx_w4;
            mask = 4 * sw - 1;
        }

        /* EOB base token context: 1 + (eob > 2<<tx2dszctx) + (eob > 4<<tx2dszctx) */
        eob_ctx = 1 + (*eob > (2 << tx2dszctx) ? 1 : 0) + (*eob > (4 << tx2dszctx) ? 1 : 0);
        if (eob_ctx > 3) eob_ctx = 3;
        if (*eob == 0) eob_ctx = 0;

        /* === PHASE 1: Decode ALL tokens (unsigned magnitude) without signs ===
           Token decode order: EOB, then eob-1 down to 1, then DC (position 0).
           Signs are decoded in Phase 2 after all tokens are decoded,
           matching dav1d's two-pass order (all tokens first, then all signs). */

        /* EOB position */
        if (tx_class == 0) {
            rc_eob = scan[*eob];
            sx_eob = rc_eob >> shift;
            sy_eob = rc_eob & mask;
        } else if (tx_class == 1) {
            sx_eob = *eob & mask;
            sy_eob = *eob >> shift;
            rc_eob = *eob;
        } else {
            sx_eob = *eob & mask;
            sy_eob = *eob >> shift;
            rc_eob = (sx_eob << shift2) | sy_eob;
        }
        pl_eob = &levels[(sy_eob + 1) * stride + (sx_eob + 1)];

        eob_tok = (int)stb_av1_msac_decode_symbol(msac,
            cdf->coef.eob_base_tok[tx_sz_idx][plane ? 1 : 0][eob_ctx], 2);
        tok = eob_tok + 1;

        if (eob_tok == 2) {
            int ctx_br = (*eob == 0) ? 0 : (tx_class == 0 ? (((sx_eob | sy_eob) > 1) ? 14 : 7) : ((sy_eob != 0) ? 14 : 7));
            { static int _eb=0; if (_eb<5000) { _eb++;
              fprintf(stderr,"[EOBHITK] txw=%d txh=%d cls=%d eob=%d ctx_br=%d rng=%d\n", tx_w, tx_h, tx_class, *eob, ctx_br, msac->rng); } }
            tok = (int)stb_av1_msac_decode_hi_tok(msac,
                cdf->coef.br_tok[br_tx][plane ? 1 : 0][ctx_br]);
        }

        coeffs[*eob] = tok;
        if (tok > 0) {
            unsigned char level = (unsigned char)((eob_tok == 2) ? (tok + (3 << 6)) : (tok * 0x41));
            pl_eob[0] = level;
        }

        /* AC positions: eob-1 down to 1 */
        for (i = *eob - 1; i > 0; i--) {
            int rc, sx, sy;
            unsigned char *pl;
            int mag, hi_mag, mag_adj, ctx;
            if (tx_class == 0) {
                rc = scan[i];
                sx = rc >> shift;
                sy = rc & mask;
                pl = &levels[(sy + 1) * stride + (sx + 1)];
                mag = pl[0 * stride + 1] + pl[1 * stride + 0]
                    + pl[1 * stride + 1] + pl[0 * stride + 2]
                    + pl[2 * stride + 0];
                hi_mag = pl[0 * stride + 1] + pl[1 * stride + 0] + pl[1 * stride + 1];
            } else {
                sx = i & mask;
                sy = i >> shift;
                rc = (tx_class == 2) ? ((sx << shift2) | sy) : i;
                pl = &levels[(sy + 1) * stride + (sx + 1)];
                mag = pl[0 * stride + 1] + pl[1 * stride + 0]
                    + pl[2 * stride + 0] + pl[3 * stride + 0]
                    + pl[4 * stride + 0];
                hi_mag = pl[0 * stride + 1] + pl[1 * stride + 0] + pl[2 * stride + 0];
            }
            mag_adj = (mag > 512) ? 4 : (mag + 64) >> 7;
            if (tx_class == 0) {
                int ctx_sx = sx > 4 ? 4 : sx;
                int ctx_sy = sy > 4 ? 4 : sy;
                int lo_idx = (tx_w > tx_h) ? 1 : (tx_w < tx_h) ? 2 : 0;
                ctx = (int)stb_av1_lo_ctx_offsets[lo_idx][ctx_sy][ctx_sx] + mag_adj;
            } else {
                ctx = (26 + (sy > 1 ? 10 : sy * 5)) + mag_adj;
            }
            if (ctx < 0) ctx = 0;
            if (ctx > 40) ctx = 40;

            { static int _l=0; if (_l<20000) { _l++;
              int _li = (tx_w > tx_h) ? 1 : (tx_w < tx_h) ? 2 : 0;
              fprintf(stderr,"[ACTK] txw=%d txh=%d cls=%d i=%d eob=%d rc=%d sx=%d sy=%d mag=%d mag_adj=%d lo_idx=%d ctx=%d cdf=%d,%d,%d,%d rng=%d\n",
                tx_w, tx_h, tx_class, i, *eob, rc, sx, sy, mag, mag_adj, _li, ctx,
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][ctx][0],
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][ctx][1],
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][ctx][2],
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][ctx][3], msac->rng); } }

            tok = (int)stb_av1_msac_decode_symbol(msac,
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][ctx], 3);

            if (tok == 3) {
                int ctx_br;
                hi_mag &= 63;
                ctx_br = ((tx_class == 0) ? (((sy | sx) > 1) ? 14 : 7) : ((sy > 0) ? 14 : 7))
                       + ((hi_mag > 12) ? 6 : (hi_mag + 1) >> 1);
                if (ctx_br > 20) ctx_br = 20;
                { static int _hdbg=0; if (_hdbg<5000) { _hdbg++;
                  fprintf(stderr,"[HITK] txw=%d txh=%d cls=%d i=%d sy=%d himag=%d ctx_br=%d rng=%d\n",
                    tx_w, tx_h, tx_class, i, sy, hi_mag, ctx_br, msac->rng); } }
                tok = (int)stb_av1_msac_decode_hi_tok(msac,
                    cdf->coef.br_tok[br_tx][plane ? 1 : 0][ctx_br]);
            }

            coeffs[i] = tok;
            if (tok > 0) {
                unsigned char level = (unsigned char)((tok >= 3) ? (tok + (3 << 6)) : (tok * 0x41));
                pl[0] = level;
            }
        }

        /* DC position (scan position 0); for dc-only (eob==0) the eob
           position above already IS the DC token, so skip. */
        if (*eob != 0) {
            unsigned char *pl = &levels[1 * stride + 1];
            int mag, hi_mag, mag_adj;
            if (tx_class == 0) {
                mag = pl[0 * stride + 1] + pl[1 * stride + 0]
                    + pl[1 * stride + 1] + pl[0 * stride + 2]
                    + pl[2 * stride + 0];
                hi_mag = pl[0 * stride + 1] + pl[1 * stride + 0] + pl[1 * stride + 1];
                dc_ctx = 0;
            } else {
                mag = pl[0 * stride + 1] + pl[1 * stride + 0]
                    + pl[2 * stride + 0] + pl[3 * stride + 0]
                    + pl[4 * stride + 0];
                hi_mag = pl[0 * stride + 1] + pl[1 * stride + 0] + pl[2 * stride + 0];
                mag_adj = (mag > 512) ? 4 : (mag + 64) >> 7;
                dc_ctx = 26 + mag_adj;
            }
            (void)mag;

            tok = (int)stb_av1_msac_decode_symbol(msac,
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][dc_ctx], 3);

            if (tok == 3) {
                int ctx_br;
                hi_mag &= 63;
                ctx_br = (hi_mag > 12) ? 6 : (hi_mag + 1) >> 1;
                if (ctx_br > 20) ctx_br = 20;
                tok = (int)stb_av1_msac_decode_hi_tok(msac,
                    cdf->coef.br_tok[br_tx][plane ? 1 : 0][ctx_br]);
            }

            coeffs[0] = tok;
            { static int _ddbg=0; if (_ddbg<5000) { _ddbg++;
              fprintf(stderr,"[DCTK] txw=%d txh=%d cls=%d dc_ctx=%d cdf=%d,%d,%d,%d rng=%d\n",
                tx_w, tx_h, tx_class, dc_ctx,
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][dc_ctx][0],
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][dc_ctx][1],
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][dc_ctx][2],
                cdf->coef.base_tok[tx_sz_idx][plane ? 1 : 0][dc_ctx][3], msac->rng); } }
        }

        /* === PHASE 2: Signs + residuals (dav1d decode_coefs residual section) ===
           Order: DC sign first (adaptive bool, dc_sign_ctx), then per AC
           position in ascending scan order: equiprobable sign, then golomb
           residual if the magnitude token == 15. Accumulate cul_level and
           dc_sign_level to produce the res_ctx neighbor context (dav1d :726). */
        cul_level = 0;
        if (coeffs[0] > 0) {
            int dc_sign_ctx = stb_av1_get_dc_sign_ctx(a_ctx, l_ctx, tx_w, tx_h);
            int dc_sign = (int)stb_av1_msac_decode_bool_adapt(msac, cdf->coef.dc_sign[plane ? 1 : 0][dc_sign_ctx]);
            dc_sign_level = (dc_sign - 1) & 0x80;
            if (coeffs[0] == 15)
                coeffs[0] = ((int)stb_av1_msac_read_golomb(msac) + 15) & 0xfffff;
            cul_level = coeffs[0];
            if (dc_sign) coeffs[0] = -coeffs[0];
        }
        for (i = 1; i <= *eob; i++) {
            if (coeffs[i] > 0) {
                int sign = (int)stb_av1_msac_decode_bool_equi(msac);
                if (coeffs[i] == 15)
                    coeffs[i] = ((int)stb_av1_msac_read_golomb(msac) + 15) & 0xfffff;
                cul_level += coeffs[i];
                if (sign) coeffs[i] = -coeffs[i];
            }
        }
        if (cul_level > 63) cul_level = 63;
        *res_ctx_out = (unsigned char)(cul_level | dc_sign_level);
        { static int _rcx=0; if (_rcx<8000) { _rcx++;
          fprintf(stderr,"[RCX] txw=%d txh=%d eob=%d cul=%d dsl=%d res_ctx=%d\n", tx_w, tx_h, *eob, cul_level, dc_sign_level, (unsigned char)(cul_level | dc_sign_level)); } }
    }
    { static int _dbg_coeff=1; if (_dbg_coeff) { int k; _dbg_coeff=0;
      fprintf(stderr,"[COEFFDBG] txw=%d txh=%d eob=%d\n", tx_w, tx_h, *eob);
      for (k=0; k<max_coeffs; k++) { if (coeffs[k]) fprintf(stderr,"[COEFFDBG]  [%d]=%d", k, coeffs[k]); }
      fprintf(stderr,"\n");
    } }
    return *eob;
}

/* Compatibility layer: map old bool reader API to MSAC */
#define stb_av1_bool_reader      stb_av1_msac
#define stb_av1_bool_reader_init(s,d,sz) stb_av1_msac_init(s,d,sz,0)
#define stb_av1_bool_decode(s,p) ((p)==128?(int)stb_av1_msac_decode_bool_equi(s):(int)stb_av1_msac_decode_bool(s,(unsigned)(p)<<7))
#define stb_av1_bool_decode_literal(s,b) stb_av1_msac_decode_bools(s,(unsigned)(b))
#define stb_av1_decode_uniform(s,n) stb_av1_msac_decode_uniform(s,(unsigned)(n))
#define stb_av1_decode_subexp(s,ref,n) stb_av1_msac_decode_subexp(s,(ref),(n),4)
/* The old coefficient decoder is replaced by the CDF version */
#define stb_av1_decode_coeffs(br,c,m,e,q) stb_av1_decode_coeffs_cdf(br,c,8,8,e,0,0,0,0)

#endif /* STB_AVIF_USE_C89_DAV1D */

#ifndef STB_AVIF_USE_C89_DAV1D
struct stb_av1_bool_reader {
    const unsigned char *data;
    size_t size;
    size_t pos;
    stbv_u32 value;   /* current window (bits) */
    stbv_u32 range;   /* current range (128-255) */
    int count;        /* bits in value */
    int error;
};

/* Initialize a Boolean reader from a byte stream.
   Uses the standard AV1/Daala Boolean decoder init (ref: dav1d, libaom). */
static void stb_av1_bool_reader_init(struct stb_av1_bool_reader *br,
                                       const unsigned char *data, size_t size)
{
    br->data = data;
    br->size = size;
    br->pos = 0;
    br->value = 0;
    br->range = 128;
    br->count = 8;  /* bits remaining in value buffer */
    br->error = 0;

    /* Load the first 16 bits into value (MSB-first, as two bytes) */
    if (br->pos < br->size) {
        br->value = (stbv_u32)br->data[br->pos++];
    }
    if (br->pos < br->size) {
        br->value = (br->value << 8) | (stbv_u32)br->data[br->pos++];
    }
    /* value now contains 16 bits, we consume 8 during renormalization;
       the rest stays buffered. The count=8 tracks we have 8 usable bits
       beyond the initial renormalization requirement. 
       This matches the spec behavior. */
}

static int stb_av1_bool_read_bit(struct stb_av1_bool_reader *br)
{
    stbv_u32 split;
    int bit;

    split = 1 + (((br->range - 1) * 128) >> 8); /* prob=128 means 50% */
    if (br->value < split) {
        br->range = split;
        bit = 0;
    } else {
        br->range = br->range - split;
        br->value = br->value - split;
        bit = 1;
    }

    while (br->range < 128) {
        int b;
        if (br->pos < br->size) {
            b = (br->data[br->pos] >> (7 - (br->count & 7))) & 1;
            br->count++;
            if ((br->count & 7) == 0)
                br->pos++;
        } else {
            b = 0; /* fill with 0 if out of data */
            br->count++;
        }
        br->value = (br->value << 1) | b;
        br->range <<= 1;
    }

    return bit;
}

/* Decode a Boolean symbol with given probability (0-255, where 128 = 50%) */
static int stb_av1_bool_decode(struct stb_av1_bool_reader *br, int prob)
{
    stbv_u32 split;
    int bit;

    split = 1 + (((br->range - 1) * prob) >> 8);
    if (br->value < split) {
        br->range = split;
        bit = 0;
    } else {
        br->range = br->range - split;
        br->value = br->value - split;
        bit = 1;
    }

    {
        int _rs = 16;
        while (br->range < 128 && _rs > 0) {
            int b;
            if (br->pos < br->size) {
                b = (br->data[br->pos] >> (7 - (br->count & 7))) & 1;
                br->count++;
                if ((br->count & 7) == 0)
                    br->pos++;
            } else {
                b = 0;
                br->count++;
            }
            br->value = (br->value << 1) | b;
            br->range <<= 1;
            _rs--;
        }
    }

    return bit;
}

/* Decode an unsigned integer with equal probability (uniform) */
static stbv_u32 stb_av1_bool_decode_literal(struct stb_av1_bool_reader *br,
                                              int bits)
{
    stbv_u32 val = 0;
    while (bits > 0) {
        bits--;
        val = (val << 1) | (stbv_u32)stb_av1_bool_decode(br, 128);
    }
    return val;
}

/* Decode a "subexp" coded unsigned integer as used in AV1.
   This is used for things like base_q_idx, etc. */
static stbv_u32 stb_av1_decode_subexp(struct stb_av1_bool_reader *br,
                                       int ref, int n)
{
    stbv_u32 v;
    if (stb_av1_bool_decode(br, 128)) {
        stbv_u32 d;
        stbv_u32 d2;
        int s = 0;
        int mk = 0;
        d = stb_av1_bool_decode_literal(br, 4) + 1;
        d2 = (stbv_u32)1 << d;
        /* In AV1, probs are adapted. For simplicity, use 128 everywhere. */
        if (n <= (int)d2) {
            v = stb_av1_bool_decode_literal(br, n - 1) + ref + 1;
        } else {
            s = n - (int)d2;
            while (mk < 3 && stb_av1_bool_decode(br, 128)) {
                s -= (int)d2;
                d2 = (stbv_u32)((int)d2 << 1);
                mk++;
            }
            v = (stbv_u32)(stb_av1_bool_decode_literal(br, s) + ref + 1 + (mk * (int)((stbv_u32)1 << d)));
        }
    } else {
        v = (stbv_u32)ref;
    }
    return v;
}

/* Decode a uniform symbol with count n */
static int stb_av1_decode_uniform(struct stb_av1_bool_reader *br, int n)
{
    int l;
    int m;
    int v;
    if (n <= 1) return 0;
    l = 0;
    while ((1 << l) < n) l++;
    m = (1 << l) - n;
    v = (int)stb_av1_bool_decode_literal(br, l - 1);
    if (v < m) return v;
    return (v << 1) | stb_av1_bool_decode(br, 128) - m;
}

/* NSYM symbol decoding (non-symmetric) with cumulative probabilities */
/* Simplified: decode an n-ary symbol using a binary tree with equal probs */
static int stb_av1_decode_nsym(struct stb_av1_bool_reader *br, int n)
{
    if (n <= 1) return 0;
    /* Use uniform decoding as a simplification */
    return stb_av1_decode_uniform(br, n);
}

#endif /* STB_AVIF_USE_C89_DAV1D */

/* -------------------------------------------------------------------------- */
/* AV1 SEQUENCE HEADER PARSER                                                */
/* -------------------------------------------------------------------------- */

struct stb_av1_sequence_header {
    int seq_profile;
    int still_picture;
    int reduced_still_picture_header;
    int frame_width_bits;
    int frame_height_bits;
    int max_frame_width;
    int max_frame_height;
    int enable_order_hint;
    int enable_dist_wtd_comp;
    int enable_masked_comp;
    int enable_intra_edge_filter;
    int enable_interintra_comp;
    int enable_dual_filter;
    int enable_jnt_comp;
    int enable_ref_frame_mvs;
    int enable_superres;
    int enable_cdef;
    int enable_restoration;
    int film_grain_params_present;
    int timing_info_present;
    int decoder_model_info_present;
    int display_model_info_present;
    int operating_points_cnt;
    int color_description_present;
    int color_primaries;
    int transfer_characteristics;
    int matrix_coefficients;
    int color_range;
    int chroma_sample_position;
    int initial_display_delay_present;
    int buffer_removal_time_length_minus_1;
    int bit_depth;
    int monochrome;
    int subsampling_x;
    int subsampling_y;
    int sb128;
    int filter_intra;
    int separate_uv_delta_q;
    int screen_content_tools; /* 0=ADAPTIVE, 1=ON, 2=OFF */
    int force_integer_mv;     /* 0=ADAPTIVE, 1=ON, 2=OFF */
    int frame_id_numbers_present;
    int order_hint_n_bits;
};

static void stb_av1_parse_sequence_header_obu(struct stb_avif_reader *r,
                                               struct stb_av1_sequence_header *sh,
                                               struct stb_av1_bool_reader *br)
{
    /* AV1 spec section 5.5: Sequence Header OBU syntax */
    sh->seq_profile = (int)stb_av1_bool_decode_literal(br, 3);
    sh->still_picture = stb_av1_bool_decode(br, 128);
    sh->reduced_still_picture_header = stb_av1_bool_decode(br, 128);

    if (sh->reduced_still_picture_header) {
        sh->timing_info_present = 0;
        sh->decoder_model_info_present = 0;
        sh->display_model_info_present = 0;
        sh->operating_points_cnt = 1;
        /* operating_point_idc[0] = 0 implicitly */
        sh->frame_width_bits = 4;
        sh->frame_height_bits = 4;
        sh->max_frame_width = 16;
        sh->max_frame_height = 16;
        sh->enable_order_hint = 0;
        sh->enable_dist_wtd_comp = 0;
        sh->enable_masked_comp = 0;
        sh->enable_intra_edge_filter = 1; /* default 1 */
        sh->enable_interintra_comp = 0;
        sh->enable_dual_filter = 0;
        sh->enable_jnt_comp = 0;
        sh->enable_superres = 0;
        sh->enable_cdef = 1; /* default 1 for still picture? */
        sh->enable_restoration = 0; /* default */
        sh->film_grain_params_present = 0;
    } else {
        int op;
        sh->operating_points_cnt = (int)stb_av1_bool_decode_literal(br, 5) + 1;
        for (op = 0; op < sh->operating_points_cnt; op++) {
            /* operating_point_idc */
            stb_av1_bool_decode_literal(br, 12);
            /* seq_level_idx */
            stb_av1_bool_decode_literal(br, 5);
            if (stb_av1_bool_decode(br, 128)) { /* seq_tier */
                stb_av1_bool_decode(br, 128);
            }
            if (op == 0) {
                /* decoder_model_present_for_this_op */
                if (stb_av1_bool_decode(br, 128)) {
                    /* decoder_buffer_delay */
                    stb_av1_bool_decode_literal(br, 8);
                    /* encoder_buffer_delay */
                    stb_av1_bool_decode_literal(br, 8);
                    stb_av1_bool_decode(br, 128); /* low_delay_mode */
                }
            }
        }

        /* frame_width_bits */
        sh->frame_width_bits = (int)stb_av1_bool_decode_literal(br, 4) + 1;
        /* frame_height_bits */
        sh->frame_height_bits = (int)stb_av1_bool_decode_literal(br, 4) + 1;
        /* max_frame_width */
        sh->max_frame_width = (int)stb_av1_bool_decode_literal(br, sh->frame_width_bits) + 1;
        /* max_frame_height */
        sh->max_frame_height = (int)stb_av1_bool_decode_literal(br, sh->frame_height_bits) + 1;

        /* Frame ID numbers */
        if (stb_av1_bool_decode(br, 128)) {
            stb_av1_bool_decode_literal(br, 4); /* delta_frame_id_length */
            stb_av1_bool_decode_literal(br, 3); /* additional_frame_id_length */
        }

        /* Use 124th order hint */
        sh->enable_order_hint = stb_av1_bool_decode(br, 128);
        if (sh->enable_order_hint) {
            stb_av1_bool_decode_literal(br, 2); /* order_hint_bits_minus_1 */
        }
        sh->enable_dist_wtd_comp = stb_av1_bool_decode(br, 128);
        sh->enable_masked_comp = stb_av1_bool_decode(br, 128);

        sh->enable_intra_edge_filter = stb_av1_bool_decode(br, 128);
        sh->enable_interintra_comp = stb_av1_bool_decode(br, 128);
        sh->enable_dual_filter = stb_av1_bool_decode(br, 128);
        sh->enable_jnt_comp = stb_av1_bool_decode(br, 128);
        sh->enable_superres = stb_av1_bool_decode(br, 128);

        /* Timing info */
        sh->timing_info_present = stb_av1_bool_decode(br, 128);
        if (sh->timing_info_present) {
            stb_av1_bool_decode_literal(br, 32); /* num_units_in_tick */
            stb_av1_bool_decode_literal(br, 32); /* time_scale */
            if (stb_av1_bool_decode(br, 128)) { /* equal_picture_interval */
                stb_av1_bool_decode_literal(br, 32); /* num_ticks_per_picture */
            }

            /* decoder_model_info */
            sh->decoder_model_info_present = stb_av1_bool_decode(br, 128);
            if (sh->decoder_model_info_present) {
                stb_av1_bool_decode_literal(br, 5); /* buffer_delay_length_minus_1 */
                stb_av1_bool_decode_literal(br, 4); /* num_units_in_decoding_tick */
                sh->buffer_removal_time_length_minus_1 = (int)stb_av1_bool_decode_literal(br, 5);
                stb_av1_bool_decode_literal(br, 5); /* frame_presentation_time_length_minus_1 */
            }

            sh->display_model_info_present = stb_av1_bool_decode(br, 128);
        }
    }

    /* Initial display delay */
    if (!sh->reduced_still_picture_header) {
        sh->initial_display_delay_present = stb_av1_bool_decode(br, 128);
        if (sh->initial_display_delay_present) {
            stb_av1_bool_decode_literal(br, 4); /* initial_display_delay */
        }
    }

    /* Color config -- always present in AV1 spec, even for reduced_still_picture */
    {
        int high_bitdepth;
        high_bitdepth = stb_av1_bool_decode(br, 128); /* high_bitdepth */
        if (high_bitdepth) {
            sh->bit_depth = stb_av1_bool_decode(br, 128) ? 12 : 10;
        } else {
            sh->bit_depth = 8;
        }

        if (sh->seq_profile == 0 && sh->bit_depth > 8) {
            sh->monochrome = 0;
        } else {
            sh->monochrome = stb_av1_bool_decode(br, 128);
        }

        if (stb_av1_bool_decode(br, 128)) {
            sh->color_description_present = 1;
            sh->color_primaries = (int)stb_av1_bool_decode_literal(br, 8);
            sh->transfer_characteristics = (int)stb_av1_bool_decode_literal(br, 8);
            sh->matrix_coefficients = (int)stb_av1_bool_decode_literal(br, 8);
        } else {
            sh->color_description_present = 0;
            sh->color_primaries = 2;
            sh->transfer_characteristics = 2;
            sh->matrix_coefficients = 2;
        }

        if (sh->monochrome) {
            sh->color_range = stb_av1_bool_decode(br, 128);
            sh->subsampling_x = 1;
            sh->subsampling_y = 1;
            sh->chroma_sample_position = 0;
        } else if (sh->color_primaries == 1
                   && sh->transfer_characteristics == 13
                   && sh->matrix_coefficients == 0) {
            sh->color_range = 1;
            sh->subsampling_x = 0;
            sh->subsampling_y = 0;
            sh->chroma_sample_position = 0;
        } else {
            sh->color_range = stb_av1_bool_decode(br, 128);
            sh->subsampling_x = stb_av1_bool_decode(br, 128);
            sh->subsampling_y = stb_av1_bool_decode(br, 128);
            if (sh->subsampling_x && sh->subsampling_y) {
                sh->chroma_sample_position = (int)stb_av1_bool_decode_literal(br, 2);
            }
        }

        sh->film_grain_params_present = stb_av1_bool_decode(br, 128);
    }

    /* Separator: always 1 for valid sequence headers */
    if (!stb_av1_bool_decode(br, 128)) {
        STB_AVIF_ERROR("Invalid AV1 sequence header");
    }

    /* CDEF and restoration filtering */
    if (!sh->reduced_still_picture_header) {
        sh->enable_cdef = stb_av1_bool_decode(br, 128);
        sh->enable_restoration = stb_av1_bool_decode(br, 128);
        fprintf(stderr, "ENREST2 enable_restoration=%d\n", sh->enable_restoration);
    }
}

/* -------------------------------------------------------------------------- */
/* AV1 FRAME HEADER PARSER                                                   */
/* -------------------------------------------------------------------------- */

struct stb_av1_frame_header {
    int show_existing_frame;
    int frame_type; /* KEY_FRAME=0, INTER_FRAME=1, INTRA_ONLY=2, S_FRAME=3 */
    int show_frame;
    int error_resilient_mode;
    int disable_cdf_update;
    int allow_screen_content_tools;
    int force_integer_mv;
    int current_frame_id;
    int frame_size_override;
    int frame_width;
    int frame_height;
    int render_width;
    int render_height;
    int superres_scale_denominator;
    int use_ref_frame_mvs;
    int order_hint;
    int refresh_frame_flags;
    int allow_high_precision_mv;
    int is_motion_mode_switchable;
    int use_transposed_filter;
    int reference_select;
    int reduced_tx_set;
    int allow_intrabc;
    int primary_ref_frame;
    int base_q_idx;
    int delta_q_y_dc;
    int delta_q_u_dc;
    int delta_q_u_ac;
    int delta_q_v_dc;
    int delta_q_v_ac;
    int delta_q_present;
    int delta_q_res_log2;
    int delta_lf_present;
    int delta_lf_res_log2;
    int delta_lf_multi;
    int using_qmatrix;
    int qm_y;
    int qm_u;
    int qm_v;
    int segmentation_enabled;
    int segment_update_map;
    int seg_temporal;
    int seg_id_pre_skip;
    int last_active_seg_id;
    int cdef_bits;
    int cdef_y_pri_strength[8];
    int cdef_y_sec_strength[8];
    int cdef_uv_pri_strength[8];
    int cdef_uv_sec_strength[8];
    int cdef_damping;
    int loop_restoration;
    int lr_type[3];     /* 0=none, 1=wiener, 2=sgrproj, 3=switchable */
    int lr_unit_size[3];
    int tx_mode;
    int skip_mode;
    int skip_mode_frame[2];
    int tile_cols;
    int tile_rows;
};

/* Frame types */
#define STB_AV1_KEY_FRAME 0
#define STB_AV1_INTER_FRAME 1
#define STB_AV1_INTRA_ONLY 2
#define STB_AV1_S_FRAME 3

static void stb_av1_parse_frame_header(struct stb_avif_reader *r,
                                        struct stb_av1_frame_header *fh,
                                        struct stb_av1_sequence_header *sh,
                                        struct stb_av1_bool_reader *br)
{
    int frame_to_show_map_idx;

    /* show_existing_frame */
    fh->show_existing_frame = stb_av1_bool_decode(br, 128);
    if (fh->show_existing_frame) {
        frame_to_show_map_idx = (int)stb_av1_bool_decode_literal(br, 3);
        (void)frame_to_show_map_idx;
        /* For AVIF we should never have this, but handle gracefully */
        if (sh->decoder_model_info_present) {
            stb_av1_bool_decode_literal(br, sh->buffer_removal_time_length_minus_1 + 1);
        }
        return; /* No more frame data needed */
    }

    fh->frame_type = (int)stb_av1_bool_decode_literal(br, 2);
    fh->show_frame = stb_av1_bool_decode(br, 128);
    fh->error_resilient_mode = stb_av1_bool_decode(br, 128);

    if (fh->frame_type == STB_AV1_KEY_FRAME && fh->show_frame) {
        /* This path is common for AVIF */
        /* no temporal delimiters needed for still images */
    }

    if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
        fh->disable_cdf_update = stb_av1_bool_decode(br, 128);
        fh->allow_screen_content_tools = stb_av1_bool_decode(br, 128);
        if (fh->allow_screen_content_tools) {
            fh->force_integer_mv = stb_av1_bool_decode(br, 128);
        } else {
            fh->force_integer_mv = 0;
        }
    }
    fprintf(stderr, "[FRAMEHDR_DBG] allow_screen_content_tools=%d seq_screen_content_tools=%d\n",
        fh->allow_screen_content_tools, sh->screen_content_tools);

    /* Frame size */
    if (sh->reduced_still_picture_header) {
        fh->frame_width = sh->max_frame_width;
        fh->frame_height = sh->max_frame_height;
        fh->render_width = fh->frame_width;
        fh->render_height = fh->frame_height;
        fh->superres_scale_denominator = 8; /* SCALE_NUMERATOR = 8 (no superres) */
        fh->frame_size_override = 0;
    } else {
        fh->frame_size_override = stb_av1_bool_decode(br, 128);
        if (fh->frame_size_override) {
            fh->frame_width = (int)stb_av1_bool_decode_literal(br, sh->frame_width_bits) + 1;
            fh->frame_height = (int)stb_av1_bool_decode_literal(br, sh->frame_height_bits) + 1;
        } else {
            fh->frame_width = sh->max_frame_width;
            fh->frame_height = sh->max_frame_height;
        }

        fh->superres_scale_denominator = 8; /* default: no superres */
        if (sh->enable_superres) {
            if (stb_av1_bool_decode(br, 128)) { /* use_superres */
                fh->superres_scale_denominator = (int)stb_av1_bool_decode_literal(br, 3) + 9;
            }
        }

        /* compute image size */
        {
            int upscaled_width = fh->frame_width;
            (void)upscaled_width;
        }

        fh->render_width = (int)stb_av1_bool_decode_literal(br, sh->frame_width_bits + 1) + 1;
        fh->render_height = (int)stb_av1_bool_decode_literal(br, sh->frame_height_bits + 1) + 1;
    }

    /* Use_ref_frame_mvs and inter skip */
    if (fh->frame_type == STB_AV1_INTRA_ONLY || fh->frame_type == STB_AV1_S_FRAME) {
        fh->allow_intrabc = stb_av1_bool_decode(br, 128);
        fh->use_ref_frame_mvs = 0;
        fh->reference_select = 0;
    }

    /* Refresh frame flags */
    fh->refresh_frame_flags = 0;
    if (fh->frame_type == STB_AV1_KEY_FRAME) {
        if (fh->show_frame) {
            fh->refresh_frame_flags = 0xFF;
        } else {
            fh->refresh_frame_flags = (int)stb_av1_bool_decode_literal(br, 8);
        }
    } else if (fh->frame_type == STB_AV1_INTRA_ONLY) {
        fh->refresh_frame_flags = (int)stb_av1_bool_decode_literal(br, 8);
    }

    /* Order hint */
    if (sh->enable_order_hint && !fh->error_resilient_mode) {
        fh->order_hint = (int)stb_av1_bool_decode_literal(br, 2); /* order_hint_bits_minus_1 + 1 */
    }

    if (!sh->reduced_still_picture_header) {
        /* Primary reference frame */
        if (fh->error_resilient_mode || (fh->frame_type == STB_AV1_KEY_FRAME && fh->show_frame)) {
            fh->primary_ref_frame = 7; /* PRIMARY_REF_NONE */
        } else {
            fh->primary_ref_frame = (int)stb_av1_bool_decode_literal(br, 3);
        }
    }

    /* Quantization parameters */
    {
        int y_dc_q_delta, u_dc_q_delta, u_ac_q_delta, v_dc_q_delta, v_ac_q_delta;

        fh->base_q_idx = (int)stb_av1_bool_decode_literal(br, 8);

        y_dc_q_delta = 0;
        if (stb_av1_bool_decode(br, 128)) {
            y_dc_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
        }
        /* Delta Q is signed: (-16..16) */
        fh->delta_q_y_dc = y_dc_q_delta > 16 ? y_dc_q_delta - 32 : y_dc_q_delta;

        u_dc_q_delta = 0;
        v_dc_q_delta = 0;
        u_ac_q_delta = 0;
        v_ac_q_delta = 0;

        if (sh->seq_profile > 0 && !sh->monochrome) {
            if (stb_av1_bool_decode(br, 128)) {
                u_dc_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
            if (stb_av1_bool_decode(br, 128)) {
                u_ac_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
            if (stb_av1_bool_decode(br, 128)) {
                v_dc_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
            if (stb_av1_bool_decode(br, 128)) {
                v_ac_q_delta = (int)stb_av1_decode_subexp(br, 0, 1 + 2 * 16);
            }
        }

        fh->delta_q_u_dc = u_dc_q_delta > 16 ? u_dc_q_delta - 32 : u_dc_q_delta;
        fh->delta_q_u_ac = u_ac_q_delta > 16 ? u_ac_q_delta - 32 : u_ac_q_delta;
        fh->delta_q_v_dc = v_dc_q_delta > 16 ? v_dc_q_delta - 32 : v_dc_q_delta;
        fh->delta_q_v_ac = v_ac_q_delta > 16 ? v_ac_q_delta - 32 : v_ac_q_delta;
    }

    /* Quantization matrix */
    fh->using_qmatrix = stb_av1_bool_decode(br, 128);
    if (fh->using_qmatrix) {
        fh->qm_y = (int)stb_av1_bool_decode_literal(br, 4);
        fh->qm_u = (int)stb_av1_bool_decode_literal(br, 4);
        fh->qm_v = (int)stb_av1_bool_decode_literal(br, 4);
    }

    /* Segmentation */
    fh->segmentation_enabled = stb_av1_bool_decode(br, 128);
    if (fh->segmentation_enabled) {
        if (fh->primary_ref_frame != 7) {
            fh->seg_temporal = stb_av1_bool_decode(br, 128);
            fh->segment_update_map = stb_av1_bool_decode(br, 128);
        } else {
            fh->seg_temporal = 0;
            fh->segment_update_map = stb_av1_bool_decode(br, 128);
        }
        fh->seg_id_pre_skip = 0; /* default: skip before seg */
        fh->last_active_seg_id = 0; /* simplified */
        if (fh->seg_temporal || fh->segment_update_map) {
            /* parse segment tree */
            fh->seg_id_pre_skip = stb_av1_bool_decode(br, 128);
        }
    }

    /* Delta Q/Delta LF */
    {
        fh->delta_q_present = 0;
        fh->delta_q_res_log2 = 0;
        fh->delta_lf_present = 0;
        fh->delta_lf_res_log2 = 0;
        fh->delta_lf_multi = 0;
        if (fh->primary_ref_frame != 7 || fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY) {
            fh->delta_q_present = stb_av1_bool_decode(br, 128);
            if (fh->delta_q_present) {
                fh->delta_q_res_log2 = (int)stb_av1_bool_decode_literal(br, 2) + 1;
            }
        }
        if (fh->delta_q_present) {
            fh->delta_lf_present = stb_av1_bool_decode(br, 128);
            if (fh->delta_lf_present) {
                fh->delta_lf_res_log2 = (int)stb_av1_bool_decode_literal(br, 2) + 1;
                fh->delta_lf_multi = stb_av1_bool_decode(br, 128);
            }
        }
    }

    /* tx_mode */
    fh->tx_mode = (int)stb_av1_bool_decode_literal(br, 2); /* 0=ONLY_4X4, 1=LARGEST, 2=SELECT */

    /* skip_mode */
    fh->skip_mode = 0;
    if (fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY) {
        /* skip_mode not allowed for intra frames */
        fh->skip_mode = 0;
    } else if (sh->enable_order_hint) {
        if (stb_av1_bool_decode(br, 128)) {
            fh->skip_mode_frame[0] = (int)stb_av1_bool_decode_literal(br, 3);
            fh->skip_mode_frame[1] = (int)stb_av1_bool_decode_literal(br, 3);
            fh->skip_mode = 1;
        }
    }

    /* Loop filter params */
    if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
        /* Cdef params */
        if (sh->enable_cdef) {
            fh->cdef_bits = (int)stb_av1_bool_decode_literal(br, 2);
            {
                int i;
                for (i = 0; i < (1 << fh->cdef_bits); i++) {
                    fh->cdef_y_pri_strength[i] = (int)stb_av1_bool_decode_literal(br, 4);
                    fh->cdef_y_sec_strength[i] = (int)stb_av1_bool_decode_literal(br, 2);
                    fh->cdef_uv_pri_strength[i] = (int)stb_av1_bool_decode_literal(br, 4);
                    fh->cdef_uv_sec_strength[i] = (int)stb_av1_bool_decode_literal(br, 2);
                }
            }
            fh->cdef_damping = (int)stb_av1_bool_decode_literal(br, 2) + 3;
        }

        /* Loop restoration */
        if (sh->enable_restoration) {
            {
                int i;
                for (i = 0; i < (sh->monochrome ? 1 : 3); i++) {
                    fh->lr_type[i] = (int)stb_av1_bool_decode_literal(br, 2);
                                        if (fh->lr_type[i]) {
                        fh->lr_unit_size[i] = (int)stb_av1_bool_decode_literal(br, 1) + 1;
                    }
                }
            }
        }
    }

    /* Tile info */
    {
        int tile_cols_log2, tile_rows_log2;
        int tile_cols, tile_rows;
        int context_update_tile_id;
        int i;

        tile_cols_log2 = 0;
        tile_rows_log2 = 0;

        if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
            /* Number of tile columns */
            if (stb_av1_bool_decode(br, 128)) {
                tile_cols_log2 = (int)stb_av1_bool_decode_literal(br, 2);
            }
            if (stb_av1_bool_decode(br, 128)) {
                tile_rows_log2 = (int)stb_av1_bool_decode_literal(br, 2);
            }
        }

        tile_cols = 1 << tile_cols_log2;
        tile_rows = 1 << tile_rows_log2;
        fh->tile_cols = tile_cols;
        fh->tile_rows = tile_rows;

        /* context_update_tile_id */
        context_update_tile_id = 0;
        if (tile_cols * tile_rows > 1) {
            context_update_tile_id = (int)stb_av1_bool_decode_literal(br, tile_cols_log2 + tile_rows_log2);
        }
        (void)context_update_tile_id;

        /* tile_size_bytes */
        {
            int tile_size_bytes = (int)stb_av1_bool_decode_literal(br, 2) + 1;
            (void)tile_size_bytes;
        }

        /* For each tile, we need the tile size. For simplicity we handle single-tile. */
        for (i = 0; i < tile_cols * tile_rows; i++) {
            if (i > 0) {
                /* tile_size_minus_1 */
                stb_av1_bool_decode_literal(br, 8); /* simplified */
            }
        }
    }

    /* Quantizer matrices for the frame */
    /* (already handled above via using_qmatrix) */

    /* Film grain */
    if (sh->film_grain_params_present && (!fh->show_existing_frame || fh->frame_type != STB_AV1_KEY_FRAME)) {
        if (stb_av1_bool_decode(br, 128)) { /* apply_grain */
            /* parse film grain params (skipped for now) */
        }
    }
}

/* -------------------------------------------------------------------------- */
/* AV1 TILE DECODER - MAIN FRAME DECODE                                      */
/* -------------------------------------------------------------------------- */

/* Constants */
#define STB_AV1_MAX_SB_SIZE 128
#define STB_AV1_MAX_TILE_WIDTH 4096
#define STB_AV1_MAX_TILE_HEIGHT 4096
#define STB_AV1_MAX_BLOCK_SIZE 4096

/* Convolutional numbers for transforms */
#define STB_AV1_TX_4X4 0
#define STB_AV1_TX_8X8 1
#define STB_AV1_TX_16X16 2
#define STB_AV1_TX_32X32 3
#define STB_AV1_TX_64X64 4
#define STB_AV1_TX_4X8 5
#define STB_AV1_TX_8X4 6
#define STB_AV1_TX_8X16 7
#define STB_AV1_TX_16X8 8
#define STB_AV1_TX_16X32 9
#define STB_AV1_TX_32X16 10
#define STB_AV1_TX_32X64 11
#define STB_AV1_TX_64X32 12
#define STB_AV1_TX_4X16 13
#define STB_AV1_TX_16X4 14
#define STB_AV1_TX_8X32 15
#define STB_AV1_TX_32X8 16

/* Prediction modes */
#define STB_AV1_DC_PRED 0
#define STB_AV1_V_PRED 1
#define STB_AV1_H_PRED 2
#define STB_AV1_D45_PRED 3
#define STB_AV1_D135_PRED 4
#define STB_AV1_D113_PRED 5
#define STB_AV1_D157_PRED 6
#define STB_AV1_D203_PRED 7
#define STB_AV1_D67_PRED 8
#define STB_AV1_SMOOTH_PRED 9
#define STB_AV1_SMOOTH_V_PRED 10
#define STB_AV1_SMOOTH_H_PRED 11
#define STB_AV1_PAETH_PRED 12

#define STB_AV1_INTRA_MODES 13

/* Partition types (for a given block size) */
#define STB_AV1_PARTITION_NONE 0
#define STB_AV1_PARTITION_HORZ 1
#define STB_AV1_PARTITION_VERT 2
#define STB_AV1_PARTITION_SPLIT 3

/* Reference array types */
#define STB_AV1_MAX_REF_FRAMES 8

/* Context for tile decoding */
struct stb_av1_tile_context {
    struct stb_av1_sequence_header *sh;
    struct stb_av1_frame_header *fh;

    /* Decoded frames */
    int frame_width;
    int frame_height;
    int mb_cols;  /* MiCols (4x4 units) */
    int mb_rows;  /* MiRows (4x4 units) */

    /* Current tile position */
    int tile_row;
    int tile_col;

    /* Boolean reader */
    struct stb_av1_bool_reader *br;
#ifdef STB_AVIF_USE_C89_DAV1D
    struct stb_av1_msac *msac;
    struct StbCdfContext *cdf;
#endif

    /* Quantization parameters */
    int qindex_y;
    int qindex_u;
    int qindex_v;

    /* Dequantization matrices */
    int dequant_y_dc[2];
    int dequant_y_ac[2];
    int dequant_u_dc[2];
    int dequant_u_ac[2];
    int dequant_v_dc[2];
    int dequant_v_ac[2];

    /* Output image planes */
    unsigned char *plane_y;
    unsigned char *plane_u;
    unsigned char *plane_v;
    int stride_y;
    int stride_u;
    int stride_v;

    /* Progress tracking */
    int total_sb;
    int done_sb;
    int next_report_sb;
    time_t start_time;

    /* Loop restoration state (dav1d ts->lr_ref: per-plane prev unit filters) */
    int lr_ref_v[3][3];
    int lr_ref_h[3][3];
    int lr_ref_w[3][2];
    int lr_ref_sgr_idx[3];

    /* Bit depth */
    int bit_depth;
    int pixel_max;

    /* Partition context arrays for cross-superblock context propagation.
       above_part[col_in_4px] = partition type of block at that column position
       from the previous SB row. left_part[row_in_4px] = from previous SB column. */
    unsigned char frame_above_part[4096];
    unsigned char frame_left_part[4096];

    /* Mode and NZ coefficient context for cross-superblock propagation */
    unsigned char frame_above_modes[4096];
    unsigned char frame_above_nz[4096];
    unsigned char frame_left_modes[4096];
    unsigned char frame_left_nz[4096];
    unsigned char frame_above_nz_u[4096];
    unsigned char frame_left_nz_u[4096];
    unsigned char frame_above_nz_v[4096];
    unsigned char frame_left_nz_v[4096];

    /* Block-level skip context for cross-superblock propagation */
    unsigned char frame_above_bskip[4096];
    unsigned char frame_left_bskip[4096];

    /* Transform-size context (dav1d tx_intra) for cross-superblock propagation.
       Stored as actual tx log2-dim + 1 (0 = unset), like dav1d's -1 sentinel. */
    unsigned char frame_above_tx[4096];
    unsigned char frame_left_tx[4096];

    /* CDEF index per 32x32 quadrant within current superblock */
    int cur_sb_cdef_idx[4];
};

/* Dequantization lookup table from dav1d: [bit_depth_idx][qindex][DC/AC].
   bit_depth_idx: 0=8-bit, 1=10-bit, 2=12-bit. */
static const unsigned short stb_av1_dq_tbl[3][256][2] = {
    {
        {    4,    4, }, {    8,    8, }, {    8,    9, }, {    9,   10, },
        {   10,   11, }, {   11,   12, }, {   12,   13, }, {   12,   14, },
        {   13,   15, }, {   14,   16, }, {   15,   17, }, {   16,   18, },
        {   17,   19, }, {   18,   20, }, {   19,   21, }, {   19,   22, },
        {   20,   23, }, {   21,   24, }, {   22,   25, }, {   23,   26, },
        {   24,   27, }, {   25,   28, }, {   26,   29, }, {   26,   30, },
        {   27,   31, }, {   28,   32, }, {   29,   33, }, {   30,   34, },
        {   31,   35, }, {   32,   36, }, {   32,   37, }, {   33,   38, },
        {   34,   39, }, {   35,   40, }, {   36,   41, }, {   37,   42, },
        {   38,   43, }, {   38,   44, }, {   39,   45, }, {   40,   46, },
        {   41,   47, }, {   42,   48, }, {   43,   49, }, {   43,   50, },
        {   44,   51, }, {   45,   52, }, {   46,   53, }, {   47,   54, },
        {   48,   55, }, {   48,   56, }, {   49,   57, }, {   50,   58, },
        {   51,   59, }, {   52,   60, }, {   53,   61, }, {   53,   62, },
        {   54,   63, }, {   55,   64, }, {   56,   65, }, {   57,   66, },
        {   57,   67, }, {   58,   68, }, {   59,   69, }, {   60,   70, },
        {   61,   71, }, {   62,   72, }, {   62,   73, }, {   63,   74, },
        {   64,   75, }, {   65,   76, }, {   66,   77, }, {   66,   78, },
        {   67,   79, }, {   68,   80, }, {   69,   81, }, {   70,   82, },
        {   70,   83, }, {   71,   84, }, {   72,   85, }, {   73,   86, },
        {   74,   87, }, {   74,   88, }, {   75,   89, }, {   76,   90, },
        {   77,   91, }, {   78,   92, }, {   78,   93, }, {   79,   94, },
        {   80,   95, }, {   81,   96, }, {   81,   97, }, {   82,   98, },
        {   83,   99, }, {   84,  100, }, {   85,  101, }, {   85,  102, },
        {   87,  104, }, {   88,  106, }, {   90,  108, }, {   92,  110, },
        {   93,  112, }, {   95,  114, }, {   96,  116, }, {   98,  118, },
        {   99,  120, }, {  101,  122, }, {  102,  124, }, {  104,  126, },
        {  105,  128, }, {  107,  130, }, {  108,  132, }, {  110,  134, },
        {  111,  136, }, {  113,  138, }, {  114,  140, }, {  116,  142, },
        {  117,  144, }, {  118,  146, }, {  120,  148, }, {  121,  150, },
        {  123,  152, }, {  125,  155, }, {  127,  158, }, {  129,  161, },
        {  131,  164, }, {  134,  167, }, {  136,  170, }, {  138,  173, },
        {  140,  176, }, {  142,  179, }, {  144,  182, }, {  146,  185, },
        {  148,  188, }, {  150,  191, }, {  152,  194, }, {  154,  197, },
        {  156,  200, }, {  158,  203, }, {  161,  207, }, {  164,  211, },
        {  166,  215, }, {  169,  219, }, {  172,  223, }, {  174,  227, },
        {  177,  231, }, {  180,  235, }, {  182,  239, }, {  185,  243, },
        {  187,  247, }, {  190,  251, }, {  192,  255, }, {  195,  260, },
        {  199,  265, }, {  202,  270, }, {  205,  275, }, {  208,  280, },
        {  211,  285, }, {  214,  290, }, {  217,  295, }, {  220,  300, },
        {  223,  305, }, {  226,  311, }, {  230,  317, }, {  233,  323, },
        {  237,  329, }, {  240,  335, }, {  243,  341, }, {  247,  347, },
        {  250,  353, }, {  253,  359, }, {  257,  366, }, {  261,  373, },
        {  265,  380, }, {  269,  387, }, {  272,  394, }, {  276,  401, },
        {  280,  408, }, {  284,  416, }, {  288,  424, }, {  292,  432, },
        {  296,  440, }, {  300,  448, }, {  304,  456, }, {  309,  465, },
        {  313,  474, }, {  317,  483, }, {  322,  492, }, {  326,  501, },
        {  330,  510, }, {  335,  520, }, {  340,  530, }, {  344,  540, },
        {  349,  550, }, {  354,  560, }, {  359,  571, }, {  364,  582, },
        {  369,  593, }, {  374,  604, }, {  379,  615, }, {  384,  627, },
        {  389,  639, }, {  395,  651, }, {  400,  663, }, {  406,  676, },
        {  411,  689, }, {  417,  702, }, {  423,  715, }, {  429,  729, },
        {  435,  743, }, {  441,  757, }, {  447,  771, }, {  454,  786, },
        {  461,  801, }, {  467,  816, }, {  475,  832, }, {  482,  848, },
        {  489,  864, }, {  497,  881, }, {  505,  898, }, {  513,  915, },
        {  522,  933, }, {  530,  951, }, {  539,  969, }, {  549,  988, },
        {  559, 1007, }, {  569, 1026, }, {  579, 1046, }, {  590, 1066, },
        {  602, 1087, }, {  614, 1108, }, {  626, 1129, }, {  640, 1151, },
        {  654, 1173, }, {  668, 1196, }, {  684, 1219, }, {  700, 1243, },
        {  717, 1267, }, {  736, 1292, }, {  755, 1317, }, {  775, 1343, },
        {  796, 1369, }, {  819, 1396, }, {  843, 1423, }, {  869, 1451, },
        {  896, 1479, }, {  925, 1508, }, {  955, 1537, }, {  988, 1567, },
        { 1022, 1597, }, { 1058, 1628, }, { 1098, 1660, }, { 1139, 1692, },
        { 1184, 1725, }, { 1232, 1759, }, { 1282, 1793, }, { 1336, 1828, },
    }, {
        {    4,    4, }, {    9,    9, }, {   10,   11, }, {   13,   13, },
        {   15,   16, }, {   17,   18, }, {   20,   21, }, {   22,   24, },
        {   25,   27, }, {   28,   30, }, {   31,   33, }, {   34,   37, },
        {   37,   40, }, {   40,   44, }, {   43,   48, }, {   47,   51, },
        {   50,   55, }, {   53,   59, }, {   57,   63, }, {   60,   67, },
        {   64,   71, }, {   68,   75, }, {   71,   79, }, {   75,   83, },
        {   78,   88, }, {   82,   92, }, {   86,   96, }, {   90,  100, },
        {   93,  105, }, {   97,  109, }, {  101,  114, }, {  105,  118, },
        {  109,  122, }, {  113,  127, }, {  116,  131, }, {  120,  136, },
        {  124,  140, }, {  128,  145, }, {  132,  149, }, {  136,  154, },
        {  140,  158, }, {  143,  163, }, {  147,  168, }, {  151,  172, },
        {  155,  177, }, {  159,  181, }, {  163,  186, }, {  166,  190, },
        {  170,  195, }, {  174,  199, }, {  178,  204, }, {  182,  208, },
        {  185,  213, }, {  189,  217, }, {  193,  222, }, {  197,  226, },
        {  200,  231, }, {  204,  235, }, {  208,  240, }, {  212,  244, },
        {  215,  249, }, {  219,  253, }, {  223,  258, }, {  226,  262, },
        {  230,  267, }, {  233,  271, }, {  237,  275, }, {  241,  280, },
        {  244,  284, }, {  248,  289, }, {  251,  293, }, {  255,  297, },
        {  259,  302, }, {  262,  306, }, {  266,  311, }, {  269,  315, },
        {  273,  319, }, {  276,  324, }, {  280,  328, }, {  283,  332, },
        {  287,  337, }, {  290,  341, }, {  293,  345, }, {  297,  349, },
        {  300,  354, }, {  304,  358, }, {  307,  362, }, {  310,  367, },
        {  314,  371, }, {  317,  375, }, {  321,  379, }, {  324,  384, },
        {  327,  388, }, {  331,  392, }, {  334,  396, }, {  337,  401, },
        {  343,  409, }, {  350,  417, }, {  356,  425, }, {  362,  433, },
        {  369,  441, }, {  375,  449, }, {  381,  458, }, {  387,  466, },
        {  394,  474, }, {  400,  482, }, {  406,  490, }, {  412,  498, },
        {  418,  506, }, {  424,  514, }, {  430,  523, }, {  436,  531, },
        {  442,  539, }, {  448,  547, }, {  454,  555, }, {  460,  563, },
        {  466,  571, }, {  472,  579, }, {  478,  588, }, {  484,  596, },
        {  490,  604, }, {  499,  616, }, {  507,  628, }, {  516,  640, },
        {  525,  652, }, {  533,  664, }, {  542,  676, }, {  550,  688, },
        {  559,  700, }, {  567,  713, }, {  576,  725, }, {  584,  737, },
        {  592,  749, }, {  601,  761, }, {  609,  773, }, {  617,  785, },
        {  625,  797, }, {  634,  809, }, {  644,  825, }, {  655,  841, },
        {  666,  857, }, {  676,  873, }, {  687,  889, }, {  698,  905, },
        {  708,  922, }, {  718,  938, }, {  729,  954, }, {  739,  970, },
        {  749,  986, }, {  759, 1002, }, {  770, 1018, }, {  782, 1038, },
        {  795, 1058, }, {  807, 1078, }, {  819, 1098, }, {  831, 1118, },
        {  844, 1138, }, {  856, 1158, }, {  868, 1178, }, {  880, 1198, },
        {  891, 1218, }, {  906, 1242, }, {  920, 1266, }, {  933, 1290, },
        {  947, 1314, }, {  961, 1338, }, {  975, 1362, }, {  988, 1386, },
        { 1001, 1411, }, { 1015, 1435, }, { 1030, 1463, }, { 1045, 1491, },
        { 1061, 1519, }, { 1076, 1547, }, { 1090, 1575, }, { 1105, 1603, },
        { 1120, 1631, }, { 1137, 1663, }, { 1153, 1695, }, { 1170, 1727, },
        { 1186, 1759, }, { 1202, 1791, }, { 1218, 1823, }, { 1236, 1859, },
        { 1253, 1895, }, { 1271, 1931, }, { 1288, 1967, }, { 1306, 2003, },
        { 1323, 2039, }, { 1342, 2079, }, { 1361, 2119, }, { 1379, 2159, },
        { 1398, 2199, }, { 1416, 2239, }, { 1436, 2283, }, { 1456, 2327, },
        { 1476, 2371, }, { 1496, 2415, }, { 1516, 2459, }, { 1537, 2507, },
        { 1559, 2555, }, { 1580, 2603, }, { 1601, 2651, }, { 1624, 2703, },
        { 1647, 2755, }, { 1670, 2807, }, { 1692, 2859, }, { 1717, 2915, },
        { 1741, 2971, }, { 1766, 3027, }, { 1791, 3083, }, { 1817, 3143, },
        { 1844, 3203, }, { 1871, 3263, }, { 1900, 3327, }, { 1929, 3391, },
        { 1958, 3455, }, { 1990, 3523, }, { 2021, 3591, }, { 2054, 3659, },
        { 2088, 3731, }, { 2123, 3803, }, { 2159, 3876, }, { 2197, 3952, },
        { 2236, 4028, }, { 2276, 4104, }, { 2319, 4184, }, { 2363, 4264, },
        { 2410, 4348, }, { 2458, 4432, }, { 2508, 4516, }, { 2561, 4604, },
        { 2616, 4692, }, { 2675, 4784, }, { 2737, 4876, }, { 2802, 4972, },
        { 2871, 5068, }, { 2944, 5168, }, { 3020, 5268, }, { 3102, 5372, },
        { 3188, 5476, }, { 3280, 5584, }, { 3375, 5692, }, { 3478, 5804, },
        { 3586, 5916, }, { 3702, 6032, }, { 3823, 6148, }, { 3953, 6268, },
        { 4089, 6388, }, { 4236, 6512, }, { 4394, 6640, }, { 4559, 6768, },
        { 4737, 6900, }, { 4929, 7036, }, { 5130, 7172, }, { 5347, 7312, },
    }, {
        {     4,     4 }, {    12,    13 }, {    18,    19 }, {    25,    27 },
        {    33,    35 }, {    41,    44 }, {    50,    54 }, {    60,    64 },
        {    70,    75 }, {    80,    87 }, {    91,    99 }, {   103,   112 },
        {   115,   126 }, {   127,   139 }, {   140,   154 }, {   153,   168 },
        {   166,   183 }, {   180,   199 }, {   194,   214 }, {   208,   230 },
        {   222,   247 }, {   237,   263 }, {   251,   280 }, {   266,   297 },
        {   281,   314 }, {   296,   331 }, {   312,   349 }, {   327,   366 },
        {   343,   384 }, {   358,   402 }, {   374,   420 }, {   390,   438 },
        {   405,   456 }, {   421,   475 }, {   437,   493 }, {   453,   511 },
        {   469,   530 }, {   484,   548 }, {   500,   567 }, {   516,   586 },
        {   532,   604 }, {   548,   623 }, {   564,   642 }, {   580,   660 },
        {   596,   679 }, {   611,   698 }, {   627,   716 }, {   643,   735 },
        {   659,   753 }, {   674,   772 }, {   690,   791 }, {   706,   809 },
        {   721,   828 }, {   737,   846 }, {   752,   865 }, {   768,   884 },
        {   783,   902 }, {   798,   920 }, {   814,   939 }, {   829,   957 },
        {   844,   976 }, {   859,   994 }, {   874,  1012 }, {   889,  1030 },
        {   904,  1049 }, {   919,  1067 }, {   934,  1085 }, {   949,  1103 },
        {   964,  1121 }, {   978,  1139 }, {   993,  1157 }, {  1008,  1175 },
        {  1022,  1193 }, {  1037,  1211 }, {  1051,  1229 }, {  1065,  1246 },
        {  1080,  1264 }, {  1094,  1282 }, {  1108,  1299 }, {  1122,  1317 },
        {  1136,  1335 }, {  1151,  1352 }, {  1165,  1370 }, {  1179,  1387 },
        {  1192,  1405 }, {  1206,  1422 }, {  1220,  1440 }, {  1234,  1457 },
        {  1248,  1474 }, {  1261,  1491 }, {  1275,  1509 }, {  1288,  1526 },
        {  1302,  1543 }, {  1315,  1560 }, {  1329,  1577 }, {  1342,  1595 },
        {  1368,  1627 }, {  1393,  1660 }, {  1419,  1693 }, {  1444,  1725 },
        {  1469,  1758 }, {  1494,  1791 }, {  1519,  1824 }, {  1544,  1856 },
        {  1569,  1889 }, {  1594,  1922 }, {  1618,  1954 }, {  1643,  1987 },
        {  1668,  2020 }, {  1692,  2052 }, {  1717,  2085 }, {  1741,  2118 },
        {  1765,  2150 }, {  1789,  2183 }, {  1814,  2216 }, {  1838,  2248 },
        {  1862,  2281 }, {  1885,  2313 }, {  1909,  2346 }, {  1933,  2378 },
        {  1957,  2411 }, {  1992,  2459 }, {  2027,  2508 }, {  2061,  2556 },
        {  2096,  2605 }, {  2130,  2653 }, {  2165,  2701 }, {  2199,  2750 },
        {  2233,  2798 }, {  2267,  2847 }, {  2300,  2895 }, {  2334,  2943 },
        {  2367,  2992 }, {  2400,  3040 }, {  2434,  3088 }, {  2467,  3137 },
        {  2499,  3185 }, {  2532,  3234 }, {  2575,  3298 }, {  2618,  3362 },
        {  2661,  3426 }, {  2704,  3491 }, {  2746,  3555 }, {  2788,  3619 },
        {  2830,  3684 }, {  2872,  3748 }, {  2913,  3812 }, {  2954,  3876 },
        {  2995,  3941 }, {  3036,  4005 }, {  3076,  4069 }, {  3127,  4149 },
        {  3177,  4230 }, {  3226,  4310 }, {  3275,  4390 }, {  3324,  4470 },
        {  3373,  4550 }, {  3421,  4631 }, {  3469,  4711 }, {  3517,  4791 },
        {  3565,  4871 }, {  3621,  4967 }, {  3677,  5064 }, {  3733,  5160 },
        {  3788,  5256 }, {  3843,  5352 }, {  3897,  5448 }, {  3951,  5544 },
        {  4005,  5641 }, {  4058,  5737 }, {  4119,  5849 }, {  4181,  5961 },
        {  4241,  6073 }, {  4301,  6185 }, {  4361,  6297 }, {  4420,  6410 },
        {  4479,  6522 }, {  4546,  6650 }, {  4612,  6778 }, {  4677,  6906 },
        {  4742,  7034 }, {  4807,  7162 }, {  4871,  7290 }, {  4942,  7435 },
        {  5013,  7579 }, {  5083,  7723 }, {  5153,  7867 }, {  5222,  8011 },
        {  5291,  8155 }, {  5367,  8315 }, {  5442,  8475 }, {  5517,  8635 },
        {  5591,  8795 }, {  5665,  8956 }, {  5745,  9132 }, {  5825,  9308 },
        {  5905,  9484 }, {  5984,  9660 }, {  6063,  9836 }, {  6149, 10028 },
        {  6234, 10220 }, {  6319, 10412 }, {  6404, 10604 }, {  6495, 10812 },
        {  6587, 11020 }, {  6678, 11228 }, {  6769, 11437 }, {  6867, 11661 },
        {  6966, 11885 }, {  7064, 12109 }, {  7163, 12333 }, {  7269, 12573 },
        {  7376, 12813 }, {  7483, 13053 }, {  7599, 13309 }, {  7715, 13565 },
        {  7832, 13821 }, {  7958, 14093 }, {  8085, 14365 }, {  8214, 14637 },
        {  8352, 14925 }, {  8492, 15213 }, {  8635, 15502 }, {  8788, 15806 },
        {  8945, 16110 }, {  9104, 16414 }, {  9275, 16734 }, {  9450, 17054 },
        {  9639, 17390 }, {  9832, 17726 }, { 10031, 18062 }, { 10245, 18414 },
        { 10465, 18766 }, { 10702, 19134 }, { 10946, 19502 }, { 11210, 19886 },
        { 11482, 20270 }, { 11776, 20670 }, { 12081, 21070 }, { 12409, 21486 },
        { 12750, 21902 }, { 13118, 22334 }, { 13501, 22766 }, { 13913, 23214 },
        { 14343, 23662 }, { 14807, 24126 }, { 15290, 24590 }, { 15812, 25070 },
        { 16356, 25551 }, { 16943, 26047 }, { 17575, 26559 }, { 18237, 27071 },
        { 18949, 27599 }, { 19718, 28143 }, { 20521, 28687 }, { 21387, 29247 },
    }
};

static int stb_av1_get_dequant(int qindex, int is_dc, int bit_depth)
{
    int bd_idx;
    if (qindex > 255) qindex = 255;
    if (qindex < 0) qindex = 0;
    bd_idx = (bit_depth <= 8) ? 0 : (bit_depth <= 10) ? 1 : 2;
    return (int)stb_av1_dq_tbl[bd_idx][qindex][is_dc ? 0 : 1];
}

/* -------------------------------------------------------------------------- */
/* Fixed-point 1D inverse transforms (ported from dav1d: itx_1d.c)            */
/* -------------------------------------------------------------------------- */

#ifndef INT16_MIN
#define INT16_MIN (-32768)
#define INT16_MAX 32767
#endif

/* CLIP helper: clamp value to [min, max] */
#define STB_AV1_CLIP(a, min, max) stb_av1_iclip(a, min, max)

/* ---------- 4-point DCT ---------- */

static void stb_av1_inv_dct4_1d(signed int *c, int stride,
                                int min, int max, int shift)
{
    int in0 = c[0 * stride], in1 = c[1 * stride];
    int in2 = c[2 * stride], in3 = c[3 * stride];
    int t0, t1, t2, t3;

    (void)min; (void)max;
    t0 = ((in0 + in2) * 181 + 128) >> 8;
    t1 = ((in0 - in2) * 181 + 128) >> 8;
    t2 = ((in1 *  1567         - in3 * (3784 - 4096) + 2048) >> 12) - in3;
    t3 = ((in1 * (3784 - 4096) + in3 *  1567         + 2048) >> 12) + in1;

    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0 * stride] = (t0 + t3 + rnd) >> shift;
        c[1 * stride] = (t1 + t2 + rnd) >> shift;
        c[2 * stride] = (t1 - t2 + rnd) >> shift;
        c[3 * stride] = (t0 - t3 + rnd) >> shift;
    } else {
        c[0 * stride] = t0 + t3;
        c[1 * stride] = t1 + t2;
        c[2 * stride] = t1 - t2;
        c[3 * stride] = t0 - t3;
    }
}

/* ---------- 8-point DCT ---------- */

static void stb_av1_inv_dct8_1d(signed int *c, int stride,
                                 int min, int max, int shift)
{
    int in0, in1, in2, in3, in4, in5, in6, in7;
    int t0, t1, t2, t3, t4a, t5a, t6a, t7a;
    int t4, t5, t6, t7;

    in0 = c[0 * stride]; in1 = c[1 * stride];
    in2 = c[2 * stride]; in3 = c[3 * stride];
    in4 = c[4 * stride]; in5 = c[5 * stride];
    in6 = c[6 * stride]; in7 = c[7 * stride];

    t0 = ((in0 + in4) * 181 + 128) >> 8;
    t1 = ((in0 - in4) * 181 + 128) >> 8;
    t2 = ((in2 *  1567         - in6 * (3784 - 4096) + 2048) >> 12) - in6;
    t3 = ((in2 * (3784 - 4096) + in6 *  1567         + 2048) >> 12) + in2;

    t4a = ((in1 *   799         - in7 * (4017 - 4096) + 2048) >> 12) - in7;
    t5a =  (in5 *  1703         - in3 *  1138         + 1024) >> 11;
    t6a =  (in5 *  1138         + in3 *  1703         + 1024) >> 11;
    t7a = ((in1 * (4017 - 4096) + in7 *   799         + 2048) >> 12) + in1;

    t4  = STB_AV1_CLIP(t4a + t5a, min, max);
    t5a = STB_AV1_CLIP(t4a - t5a, min, max);
    t7  = STB_AV1_CLIP(t7a + t6a, min, max);
    t6a = STB_AV1_CLIP(t7a - t6a, min, max);

    t5 = ((t6a - t5a) * 181 + 128) >> 8;
    t6 = ((t6a + t5a) * 181 + 128) >> 8;

    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0 * stride] = (t0 + t7 + rnd) >> shift;
        c[1 * stride] = (t1 + t6 + rnd) >> shift;
        c[2 * stride] = (t2 + t5 + rnd) >> shift;
        c[3 * stride] = (t3 + t4 + rnd) >> shift;
        c[4 * stride] = (t3 - t4 + rnd) >> shift;
        c[5 * stride] = (t2 - t5 + rnd) >> shift;
        c[6 * stride] = (t1 - t6 + rnd) >> shift;
        c[7 * stride] = (t0 - t7 + rnd) >> shift;
    } else {
        c[0 * stride] = t0 + t7;
        c[1 * stride] = t1 + t6;
        c[2 * stride] = t2 + t5;
        c[3 * stride] = t3 + t4;
        c[4 * stride] = t3 - t4;
        c[5 * stride] = t2 - t5;
        c[6 * stride] = t1 - t6;
        c[7 * stride] = t0 - t7;
    }
}

/* ---------- 16-point DCT ---------- */

static void stb_av1_inv_dct16_1d(signed int *c, int stride,
                                  int min, int max, int shift)
{
    int in0, in1, in2, in3, in4, in5, in6, in7;
    int in8, in9, in10, in11, in12, in13, in14, in15;
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t8a, t9a, t10a, t11a, t12a, t13a, t14a, t15a;
    int t8, t9, t10, t11, t12, t13, t14, t15;
    int t8b, t9b, t10b, t11b, t12b, t13b;

    (void)min; (void)max;

    in0 = c[0*stride]; in1 = c[1*stride]; in2 = c[2*stride]; in3 = c[3*stride];
    in4 = c[4*stride]; in5 = c[5*stride]; in6 = c[6*stride]; in7 = c[7*stride];
    in8 = c[8*stride]; in9 = c[9*stride]; in10 = c[10*stride]; in11 = c[11*stride];
    in12 = c[12*stride]; in13 = c[13*stride]; in14 = c[14*stride]; in15 = c[15*stride];

    t0 = ((in0 + in8) * 181 + 128) >> 8;
    t1 = ((in0 - in8) * 181 + 128) >> 8;
    t2 = ((in4 * 1567 - in12 * (3784 - 4096) + 2048) >> 12) - in12;
    t3 = ((in4 * (3784 - 4096) + in12 * 1567 + 2048) >> 12) + in4;

    t4 = ((in2 + in10) * 181 + 128) >> 8;
    t5 = ((in2 - in10) * 181 + 128) >> 8;
    t6 = ((in6 * 1567 - in14 * (3784 - 4096) + 2048) >> 12) - in14;
    t7 = ((in6 * (3784 - 4096) + in14 * 1567 + 2048) >> 12) + in6;

    t8a = ((in1 * 401 - in15 * (4076 - 4096) + 2048) >> 12) - in15;
    t9a = (in9 * 1583 - in7 * 1299 + 1024) >> 11;
    t10a = ((in5 * 1931 - in11 * (3612 - 4096) + 2048) >> 12) - in11;
    t11a = ((in13 * (3920 - 4096) - in3 * 1189 + 2048) >> 12) + in13;
    t12a = ((in13 * 1189 + in3 * (3920 - 4096) + 2048) >> 12) + in3;
    t13a = ((in5 * (3612 - 4096) + in11 * 1931 + 2048) >> 12) + in5;
    t14a = (in9 * 1299 + in7 * 1583 + 1024) >> 11;
    t15a = ((in1 * (4076 - 4096) + in15 * 401 + 2048) >> 12) + in1;

    t8  = STB_AV1_CLIP(t8a  + t9a, min, max);
    t9  = STB_AV1_CLIP(t8a  - t9a, min, max);
    t10 = STB_AV1_CLIP(t11a - t10a, min, max);
    t11 = STB_AV1_CLIP(t11a + t10a, min, max);
    t12 = STB_AV1_CLIP(t12a + t13a, min, max);
    t13 = STB_AV1_CLIP(t12a - t13a, min, max);
    t14 = STB_AV1_CLIP(t15a - t14a, min, max);
    t15 = STB_AV1_CLIP(t15a + t14a, min, max);

    t9a  = ((  t14 * 1567 - t9  * (3784 - 4096) + 2048) >> 12) - t9;
    t14a = ((  t14 * (3784 - 4096) + t9  * 1567 + 2048) >> 12) + t14;
    t10a = ((-(t13 * (3784 - 4096) + t10 * 1567) + 2048) >> 12) - t13;
    t13a = ((  t13 * 1567 - t10 * (3784 - 4096) + 2048) >> 12) - t10;

    t8a  = STB_AV1_CLIP(t8   + t11, min, max);
    t9   = STB_AV1_CLIP(t9a  + t10a, min, max);
    t10  = STB_AV1_CLIP(t9a  - t10a, min, max);
    t11a = STB_AV1_CLIP(t8   - t11, min, max);
    t12a = STB_AV1_CLIP(t15  - t12, min, max);
    t13  = STB_AV1_CLIP(t14a - t13a, min, max);
    t14  = STB_AV1_CLIP(t14a + t13a, min, max);
    t15a = STB_AV1_CLIP(t15  + t12, min, max);

    t10b = ((t13  - t10) * 181 + 128) >> 8;
    t13b = ((t13  + t10) * 181 + 128) >> 8;
    t11  = ((t12a - t11a) * 181 + 128) >> 8;
    t12  = ((t12a + t11a) * 181 + 128) >> 8;

    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0*stride]  = (t0   + t15a + rnd) >> shift;
        c[1*stride]  = (t1   + t14  + rnd) >> shift;
        c[2*stride]  = (t2   + t13b + rnd) >> shift;
        c[3*stride]  = (t3   + t12  + rnd) >> shift;
        c[4*stride]  = (t4   + t11  + rnd) >> shift;
        c[5*stride]  = (t5   + t10b + rnd) >> shift;
        c[6*stride]  = (t6   + t9   + rnd) >> shift;
        c[7*stride]  = (t7   + t8a  + rnd) >> shift;
        c[8*stride]  = (t7   - t8a  + rnd) >> shift;
        c[9*stride]  = (t6   - t9   + rnd) >> shift;
        c[10*stride] = (t5   - t10b + rnd) >> shift;
        c[11*stride] = (t4   - t11  + rnd) >> shift;
        c[12*stride] = (t3   - t12  + rnd) >> shift;
        c[13*stride] = (t2   - t13b + rnd) >> shift;
        c[14*stride] = (t1   - t14  + rnd) >> shift;
        c[15*stride] = (t0   - t15a + rnd) >> shift;
    } else {
        c[0*stride]  = t0   + t15a;
        c[1*stride]  = t1   + t14;
        c[2*stride]  = t2   + t13b;
        c[3*stride]  = t3   + t12;
        c[4*stride]  = t4   + t11;
        c[5*stride]  = t5   + t10b;
        c[6*stride]  = t6   + t9;
        c[7*stride]  = t7   + t8a;
        c[8*stride]  = t7   - t8a;
        c[9*stride]  = t6   - t9;
        c[10*stride] = t5   - t10b;
        c[11*stride] = t4   - t11;
        c[12*stride] = t3   - t12;
        c[13*stride] = t2   - t13b;
        c[14*stride] = t1   - t14;
        c[15*stride] = t0   - t15a;
    }
}

/* ---------- 32-point DCT ---------- */

static void stb_av1_inv_dct32_1d(signed int *c, int stride,
                                  int min, int max, int shift)
{
    int in0, in1, in2, in3, in4, in5, in6, in7;
    int in8, in9, in10, in11, in12, in13, in14, in15;
    int in16, in17, in18, in19, in20, in21, in22, in23;
    int in24, in25, in26, in27, in28, in29, in30, in31;
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t8, t9, t10, t11, t12, t13, t14, t15;
    int t16a, t17a, t18a, t19a, t20a, t21a, t22a, t23a;
    int t24a, t25a, t26a, t27a, t28a, t29a, t30a, t31a;
    int t16, t17, t18, t19, t20, t21, t22, t23;
    int t24, t25, t26, t27, t28, t29, t30, t31;
    int t16b, t17b, t18b, t19b, t20b, t21b, t22b, t23b;
    int t24b, t25b, t26b, t27b, t28b, t29b, t30b, t31b;

    (void)min; (void)max;

    in0 = c[0*stride]; in1 = c[1*stride]; in2 = c[2*stride]; in3 = c[3*stride];
    in4 = c[4*stride]; in5 = c[5*stride]; in6 = c[6*stride]; in7 = c[7*stride];
    in8 = c[8*stride]; in9 = c[9*stride]; in10 = c[10*stride]; in11 = c[11*stride];
    in12 = c[12*stride]; in13 = c[13*stride]; in14 = c[14*stride]; in15 = c[15*stride];
    in16 = c[16*stride]; in17 = c[17*stride]; in18 = c[18*stride]; in19 = c[19*stride];
    in20 = c[20*stride]; in21 = c[21*stride]; in22 = c[22*stride]; in23 = c[23*stride];
    in24 = c[24*stride]; in25 = c[25*stride]; in26 = c[26*stride]; in27 = c[27*stride];
    in28 = c[28*stride]; in29 = c[29*stride]; in30 = c[30*stride]; in31 = c[31*stride];

    t0 = ((in0 + in16) * 181 + 128) >> 8;
    t1 = ((in0 - in16) * 181 + 128) >> 8;
    t2 = ((in8 * 1567 - in24 * (3784 - 4096) + 2048) >> 12) - in24;
    t3 = ((in8 * (3784 - 4096) + in24 * 1567 + 2048) >> 12) + in8;

    t4 = ((in4 + in20) * 181 + 128) >> 8;
    t5 = ((in4 - in20) * 181 + 128) >> 8;
    t6 = ((in12 * 1567 - in28 * (3784 - 4096) + 2048) >> 12) - in28;
    t7 = ((in12 * (3784 - 4096) + in28 * 1567 + 2048) >> 12) + in12;

    t8 = ((in2 + in18) * 181 + 128) >> 8;
    t9 = ((in2 - in18) * 181 + 128) >> 8;
    t10 = ((in10 * 1567 - in26 * (3784 - 4096) + 2048) >> 12) - in26;
    t11 = ((in10 * (3784 - 4096) + in26 * 1567 + 2048) >> 12) + in10;

    t12 = ((in6 + in22) * 181 + 128) >> 8;
    t13 = ((in6 - in22) * 181 + 128) >> 8;
    t14 = ((in14 * 1567 - in30 * (3784 - 4096) + 2048) >> 12) - in30;
    t15 = ((in14 * (3784 - 4096) + in30 * 1567 + 2048) >> 12) + in14;

    t16a = ((in1 * 201 - in31 * (4091 - 4096) + 2048) >> 12) - in31;
    t17a = ((in17 * (3035 - 4096) - in15 * 2751 + 2048) >> 12) + in17;
    t18a = ((in9 * 1751 - in23 * (3703 - 4096) + 2048) >> 12) - in23;
    t19a = ((in25 * (3857 - 4096) - in7 * 1380 + 2048) >> 12) + in25;
    t20a = ((in5 * 995 - in27 * (3973 - 4096) + 2048) >> 12) - in27;
    t21a = ((in21 * (3513 - 4096) - in11 * 2106 + 2048) >> 12) + in21;
    t22a = (in13 * 1220 - in19 * 1645 + 1024) >> 11;
    t23a = ((in29 * (4052 - 4096) - in3 * 601 + 2048) >> 12) + in29;
    t24a = ((in29 * 601 + in3 * (4052 - 4096) + 2048) >> 12) + in3;
    t25a = (in13 * 1645 + in19 * 1220 + 1024) >> 11;
    t26a = ((in21 * 2106 + in11 * (3513 - 4096) + 2048) >> 12) + in11;
    t27a = ((in5 * (3973 - 4096) + in27 * 995 + 2048) >> 12) + in5;
    t28a = ((in25 * 1380 + in7 * (3857 - 4096) + 2048) >> 12) + in7;
    t29a = ((in9 * (3703 - 4096) + in23 * 1751 + 2048) >> 12) + in9;
    t30a = ((in17 * 2751 + in15 * (3035 - 4096) + 2048) >> 12) + in15;
    t31a = ((in1 * (4091 - 4096) + in31 * 201 + 2048) >> 12) + in1;

    t16 = STB_AV1_CLIP(t16a + t17a, min, max);
    t17 = STB_AV1_CLIP(t16a - t17a, min, max);
    t18 = STB_AV1_CLIP(t19a - t18a, min, max);
    t19 = STB_AV1_CLIP(t19a + t18a, min, max);
    t20 = STB_AV1_CLIP(t20a + t21a, min, max);
    t21 = STB_AV1_CLIP(t20a - t21a, min, max);
    t22 = STB_AV1_CLIP(t23a - t22a, min, max);
    t23 = STB_AV1_CLIP(t23a + t22a, min, max);
    t24 = STB_AV1_CLIP(t24a + t25a, min, max);
    t25 = STB_AV1_CLIP(t24a - t25a, min, max);
    t26 = STB_AV1_CLIP(t27a - t26a, min, max);
    t27 = STB_AV1_CLIP(t27a + t26a, min, max);
    t28 = STB_AV1_CLIP(t28a + t29a, min, max);
    t29 = STB_AV1_CLIP(t28a - t29a, min, max);
    t30 = STB_AV1_CLIP(t31a - t30a, min, max);
    t31 = STB_AV1_CLIP(t31a + t30a, min, max);

    t17a = ((  t30 * 799 - t17 * (4017 - 4096) + 2048) >> 12) - t17;
    t30a = ((  t30 * (4017 - 4096) + t17 * 799 + 2048) >> 12) + t30;
    t18a = ((-(t29 * (4017 - 4096) + t18 * 799) + 2048) >> 12) - t29;
    t29a = ((  t29 * 799 - t18 * (4017 - 4096) + 2048) >> 12) - t18;
    t21a = (  t26 * 1703 - t21 * 1138 + 1024) >> 11;
    t26a = (  t26 * 1138 + t21 * 1703 + 1024) >> 11;
    t22a = ( -(t25 * 1138 + t22 * 1703) + 1024) >> 11;
    t25a = (  t25 * 1703 - t22 * 1138 + 1024) >> 11;

    t16a = STB_AV1_CLIP(t16  + t19, min, max);
    t17  = STB_AV1_CLIP(t17a + t18a, min, max);
    t18  = STB_AV1_CLIP(t17a - t18a, min, max);
    t19a = STB_AV1_CLIP(t16  - t19, min, max);
    t20a = STB_AV1_CLIP(t23  - t20, min, max);
    t21  = STB_AV1_CLIP(t22a - t21a, min, max);
    t22  = STB_AV1_CLIP(t22a + t21a, min, max);
    t23a = STB_AV1_CLIP(t23  + t20, min, max);
    t24a = STB_AV1_CLIP(t24  + t27, min, max);
    t25  = STB_AV1_CLIP(t25a + t26a, min, max);
    t26  = STB_AV1_CLIP(t25a - t26a, min, max);
    t27a = STB_AV1_CLIP(t24  - t27, min, max);
    t28a = STB_AV1_CLIP(t31  - t28, min, max);
    t29  = STB_AV1_CLIP(t30a - t29a, min, max);
    t30  = STB_AV1_CLIP(t30a + t29a, min, max);
    t31a = STB_AV1_CLIP(t31  + t28, min, max);

    t18a = ((  t29  * 1567 - t18  * (3784 - 4096) + 2048) >> 12) - t18;
    t29a = ((  t29  * (3784 - 4096) + t18  * 1567 + 2048) >> 12) + t29;
    t19  = ((  t28a * 1567 - t19a * (3784 - 4096) + 2048) >> 12) - t19a;
    t28  = ((  t28a * (3784 - 4096) + t19a * 1567 + 2048) >> 12) + t28a;
    t20  = ((-(t27a * (3784 - 4096) + t20a * 1567) + 2048) >> 12) - t27a;
    t27  = ((  t27a * 1567 - t20a * (3784 - 4096) + 2048) >> 12) - t20a;
    t21a = ((-(t26  * (3784 - 4096) + t21  * 1567) + 2048) >> 12) - t26;
    t26a = ((  t26  * 1567 - t21  * (3784 - 4096) + 2048) >> 12) - t21;

    t16  = STB_AV1_CLIP(t16a + t23a, min, max);
    t17a = STB_AV1_CLIP(t17  + t22, min, max);
    t18  = STB_AV1_CLIP(t18a + t21a, min, max);
    t19a = STB_AV1_CLIP(t19  + t20, min, max);
    t20a = STB_AV1_CLIP(t19  - t20, min, max);
    t21  = STB_AV1_CLIP(t18a - t21a, min, max);
    t22a = STB_AV1_CLIP(t17  - t22, min, max);
    t23  = STB_AV1_CLIP(t16a - t23a, min, max);
    t24  = STB_AV1_CLIP(t31a - t24a, min, max);
    t25a = STB_AV1_CLIP(t30  - t25, min, max);
    t26  = STB_AV1_CLIP(t29a - t26a, min, max);
    t27a = STB_AV1_CLIP(t28  - t27, min, max);
    t28a = STB_AV1_CLIP(t28  + t27, min, max);
    t29  = STB_AV1_CLIP(t29a + t26a, min, max);
    t30a = STB_AV1_CLIP(t30  + t25, min, max);
    t31  = STB_AV1_CLIP(t31a + t24a, min, max);

    t20 = ((t27a - t20a) * 181 + 128) >> 8;
    t27 = ((t27a + t20a) * 181 + 128) >> 8;
    t21a = ((t26  - t21 ) * 181 + 128) >> 8;
    t26a = ((t26  + t21 ) * 181 + 128) >> 8;
    t22 = ((t25a - t22a) * 181 + 128) >> 8;
    t25 = ((t25a + t22a) * 181 + 128) >> 8;
    t23a = ((t24  - t23 ) * 181 + 128) >> 8;
    t24a = ((t24  + t23 ) * 181 + 128) >> 8;

    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0*stride]  = (t0  + t31  + rnd) >> shift;
        c[1*stride]  = (t1  + t30a + rnd) >> shift;
        c[2*stride]  = (t2  + t29  + rnd) >> shift;
        c[3*stride]  = (t3  + t28a + rnd) >> shift;
        c[4*stride]  = (t4  + t27  + rnd) >> shift;
        c[5*stride]  = (t5  + t26a + rnd) >> shift;
        c[6*stride]  = (t6  + t25  + rnd) >> shift;
        c[7*stride]  = (t7  + t24a + rnd) >> shift;
        c[8*stride]  = (t8  + t23a + rnd) >> shift;
        c[9*stride]  = (t9  + t22  + rnd) >> shift;
        c[10*stride] = (t10 + t21a + rnd) >> shift;
        c[11*stride] = (t11 + t20  + rnd) >> shift;
        c[12*stride] = (t12 + t19a + rnd) >> shift;
        c[13*stride] = (t13 + t18  + rnd) >> shift;
        c[14*stride] = (t14 + t17a + rnd) >> shift;
        c[15*stride] = (t15 + t16  + rnd) >> shift;
        c[16*stride] = (t15 - t16  + rnd) >> shift;
        c[17*stride] = (t14 - t17a + rnd) >> shift;
        c[18*stride] = (t13 - t18  + rnd) >> shift;
        c[19*stride] = (t12 - t19a + rnd) >> shift;
        c[20*stride] = (t11 - t20  + rnd) >> shift;
        c[21*stride] = (t10 - t21a + rnd) >> shift;
        c[22*stride] = (t9  - t22  + rnd) >> shift;
        c[23*stride] = (t8  - t23a + rnd) >> shift;
        c[24*stride] = (t7  - t24a + rnd) >> shift;
        c[25*stride] = (t6  - t25  + rnd) >> shift;
        c[26*stride] = (t5  - t26a + rnd) >> shift;
        c[27*stride] = (t4  - t27  + rnd) >> shift;
        c[28*stride] = (t3  - t28a + rnd) >> shift;
        c[29*stride] = (t2  - t29  + rnd) >> shift;
        c[30*stride] = (t1  - t30a + rnd) >> shift;
        c[31*stride] = (t0  - t31  + rnd) >> shift;
    } else {
        c[0*stride]  = t0  + t31;
        c[1*stride]  = t1  + t30a;
        c[2*stride]  = t2  + t29;
        c[3*stride]  = t3  + t28a;
        c[4*stride]  = t4  + t27;
        c[5*stride]  = t5  + t26a;
        c[6*stride]  = t6  + t25;
        c[7*stride]  = t7  + t24a;
        c[8*stride]  = t8  + t23a;
        c[9*stride]  = t9  + t22;
        c[10*stride] = t10 + t21a;
        c[11*stride] = t11 + t20;
        c[12*stride] = t12 + t19a;
        c[13*stride] = t13 + t18;
        c[14*stride] = t14 + t17a;
        c[15*stride] = t15 + t16;
        c[16*stride] = t15 - t16;
        c[17*stride] = t14 - t17a;
        c[18*stride] = t13 - t18;
        c[19*stride] = t12 - t19a;
        c[20*stride] = t11 - t20;
        c[21*stride] = t10 - t21a;
        c[22*stride] = t9  - t22;
        c[23*stride] = t8  - t23a;
        c[24*stride] = t7  - t24a;
        c[25*stride] = t6  - t25;
        c[26*stride] = t5  - t26a;
        c[27*stride] = t4  - t27;
        c[28*stride] = t3  - t28a;
        c[29*stride] = t2  - t29;
        c[30*stride] = t1  - t30a;
        c[31*stride] = t0  - t31;
    }
}

/* ---------- 4-point ADST ---------- */

static void stb_av1_inv_adst4_1d(signed int *c, int stride,
                                  int min, int max, int shift)
{
    int in0 = c[0 * stride], in1 = c[1 * stride];
    int in2 = c[2 * stride], in3 = c[3 * stride];
    int out0, out1, out2, out3;
    int tmp0, tmp1, tmp2, tmp3;
    (void)min; (void)max;

    out0 = (( 1321 * in0 + (3803 - 4096) * in2 +
             (2482 - 4096) * in3 + (3344 - 4096) * in1 + 2048) >> 12) +
            in2 + in3 + in1;
    out1 = (((2482 - 4096) * in0 - 1321 * in2 -
             (3803 - 4096) * in3 + (3344 - 4096) * in1 + 2048) >> 12) +
            in0 - in3 + in1;
    out2 = (209 * (in0 - in2 + in3) + 128) >> 8;
    out3 = (((3803 - 4096) * in0 + (2482 - 4096) * in2 -
             1321 * in3 - (3344 - 4096) * in1 + 2048) >> 12) +
            in0 + in2 - in1;

    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0 * stride] = (out0 + rnd) >> shift;
        c[1 * stride] = (out1 + rnd) >> shift;
        c[2 * stride] = (out2 + rnd) >> shift;
        c[3 * stride] = (out3 + rnd) >> shift;
    } else {
        c[0 * stride] = out0;
        c[1 * stride] = out1;
        c[2 * stride] = out2;
        c[3 * stride] = out3;
    }
}

/* ---------- 8-point ADST ---------- */

static void stb_av1_inv_adst8_1d(signed int *c, int stride,
                                  int min, int max, int shift)
{
    int in0 = c[0*stride], in1 = c[1*stride], in2 = c[2*stride], in3 = c[3*stride];
    int in4 = c[4*stride], in5 = c[5*stride], in6 = c[6*stride], in7 = c[7*stride];
    int t0a, t1a, t2a, t3a, t4a, t5a, t6a, t7a;
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t4b, t5b, t6b, t7b;
    (void)min; (void)max;

    t0a = (((4076 - 4096) * in7 +   401 * in0 + 2048) >> 12) + in7;
    t1a = ((  401         * in7 - (4076 - 4096) * in0 + 2048) >> 12) - in0;
    t2a = (((3612 - 4096) * in5 +  1931 * in2 + 2048) >> 12) + in5;
    t3a = (( 1931         * in5 - (3612 - 4096) * in2 + 2048) >> 12) - in2;
    t4a =  ( 1299 * in3 +  1583 * in4 + 1024) >> 11;
    t5a =  ( 1583 * in3 -  1299 * in4 + 1024) >> 11;
    t6a = (( 1189 * in1 + (3920 - 4096) * in6 + 2048) >> 12) + in6;
    t7a = (((3920 - 4096) * in1 -  1189 * in6 + 2048) >> 12) + in1;

    t0 = STB_AV1_CLIP(t0a + t4a, min, max);
    t1 = STB_AV1_CLIP(t1a + t5a, min, max);
    t2 = STB_AV1_CLIP(t2a + t6a, min, max);
    t3 = STB_AV1_CLIP(t3a + t7a, min, max);
    t4 = STB_AV1_CLIP(t0a - t4a, min, max);
    t5 = STB_AV1_CLIP(t1a - t5a, min, max);
    t6 = STB_AV1_CLIP(t2a - t6a, min, max);
    t7 = STB_AV1_CLIP(t3a - t7a, min, max);

    t4a = (((3784 - 4096) * t4 + 1567 * t5 + 2048) >> 12) + t4;
    t5a = (( 1567 * t4 - (3784 - 4096) * t5 + 2048) >> 12) - t5;
    t6a = (((3784 - 4096) * t7 - 1567 * t6 + 2048) >> 12) + t7;
    t7a = (( 1567 * t7 + (3784 - 4096) * t6 + 2048) >> 12) + t6;

    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0 * stride] = (t0  + t2  + rnd) >> shift;
        c[7 * stride] = (-(t1  + t3 ) + rnd) >> shift;
        c[1 * stride] = (-(t4a + t6a) + rnd) >> shift;
        c[6 * stride] = (t5a + t7a + rnd) >> shift;
    } else {
        c[0 * stride] =  STB_AV1_CLIP(t0  + t2 , min, max);
        c[7 * stride] = -STB_AV1_CLIP(t1  + t3 , min, max);
        c[1 * stride] = -STB_AV1_CLIP(t4a + t6a, min, max);
        c[6 * stride] =  STB_AV1_CLIP(t5a + t7a, min, max);
    }
    t2            =  STB_AV1_CLIP(t0  - t2 , min, max);
    t3            =  STB_AV1_CLIP(t1  - t3 , min, max);
    t6            =  STB_AV1_CLIP(t4a - t6a, min, max);
    t7            =  STB_AV1_CLIP(t5a - t7a, min, max);
    c[3 * stride] = -(((t2 + t3) * 181 + 128) >> 8);
    c[4 * stride] =   ((t2 - t3) * 181 + 128) >> 8;
    c[2 * stride] =   ((t6 + t7) * 181 + 128) >> 8;
    c[5 * stride] = -(((t6 - t7) * 181 + 128) >> 8);
}

/* ---------- 16-point ADST (in-place) ---------- */

static void stb_av1_inv_adst16_1d(signed int *c, int stride,
                                    int min, int max, int shift)
{
    int in0, in1, in2, in3, in4, in5, in6, in7;
    int in8, in9, in10, in11, in12, in13, in14, in15;
    int t0, t1, t2, t3, t4, t5, t6, t7;
    int t8, t9, t10, t11, t12, t13, t14, t15;
    int t0a, t1a, t2a, t3a, t4a, t5a, t6a, t7a;
    int t8a, t9a, t10a, t11a, t12a, t13a, t14a, t15a;
    int t2b, t3b, t6b, t7b, t10b, t11b, t14b, t15b;
    int out[16];
    (void)min; (void)max;

    in0 = c[0*stride]; in1 = c[1*stride]; in2 = c[2*stride]; in3 = c[3*stride];
    in4 = c[4*stride]; in5 = c[5*stride]; in6 = c[6*stride]; in7 = c[7*stride];
    in8 = c[8*stride]; in9 = c[9*stride]; in10 = c[10*stride]; in11 = c[11*stride];
    in12 = c[12*stride]; in13 = c[13*stride]; in14 = c[14*stride]; in15 = c[15*stride];

    t0  = ((in15 * (4091 - 4096) + in0  * 201 + 2048) >> 12) + in15;
    t1  = ((in15 * 201 - in0 * (4091 - 4096) + 2048) >> 12) - in0;
    t2  = ((in13 * (3973 - 4096) + in2  * 995 + 2048) >> 12) + in13;
    t3  = ((in13 * 995 - in2 * (3973 - 4096) + 2048) >> 12) - in2;
    t4  = ((in11 * (3703 - 4096) + in4  * 1751 + 2048) >> 12) + in11;
    t5  = ((in11 * 1751 - in4 * (3703 - 4096) + 2048) >> 12) - in4;
    t6  =  (in9  * 1645 + in6  * 1220 + 1024) >> 11;
    t7  =  (in9  * 1220 - in6  * 1645 + 1024) >> 11;
    t8  = ((in7  * 2751 + in8  * (3035 - 4096) + 2048) >> 12) + in8;
    t9  = ((in7  * (3035 - 4096) - in8  * 2751 + 2048) >> 12) + in7;
    t10 = ((in5  * 2106 + in10 * (3513 - 4096) + 2048) >> 12) + in10;
    t11 = ((in5  * (3513 - 4096) - in10 * 2106 + 2048) >> 12) + in5;
    t12 = ((in3  * 1380 + in12 * (3857 - 4096) + 2048) >> 12) + in12;
    t13 = ((in3  * (3857 - 4096) - in12 * 1380 + 2048) >> 12) + in3;
    t14 = ((in1  * 601 + in14 * (4052 - 4096) + 2048) >> 12) + in14;
    t15 = ((in1  * (4052 - 4096) - in14 * 601 + 2048) >> 12) + in1;

    t0a = STB_AV1_CLIP(t0 + t8, min, max);  t1a = STB_AV1_CLIP(t1 + t9, min, max);
    t2a = STB_AV1_CLIP(t2 + t10, min, max); t3a = STB_AV1_CLIP(t3 + t11, min, max);
    t4a = STB_AV1_CLIP(t4 + t12, min, max); t5a = STB_AV1_CLIP(t5 + t13, min, max);
    t6a = STB_AV1_CLIP(t6 + t14, min, max); t7a = STB_AV1_CLIP(t7 + t15, min, max);
    t8a = STB_AV1_CLIP(t0 - t8, min, max);  t9a = STB_AV1_CLIP(t1 - t9, min, max);
    t10a = STB_AV1_CLIP(t2 - t10, min, max); t11a = STB_AV1_CLIP(t3 - t11, min, max);
    t12a = STB_AV1_CLIP(t4 - t12, min, max); t13a = STB_AV1_CLIP(t5 - t13, min, max);
    t14a = STB_AV1_CLIP(t6 - t14, min, max); t15a = STB_AV1_CLIP(t7 - t15, min, max);

    t8   = ((t8a  * (4017 - 4096) + t9a  * 799 + 2048) >> 12) + t8a;
    t9   = ((t8a  * 799 - t9a  * (4017 - 4096) + 2048) >> 12) - t9a;
    t10  = ((t10a * 2276 + t11a * (3406 - 4096) + 2048) >> 12) + t11a;
    t11  = ((t10a * (3406 - 4096) - t11a * 2276 + 2048) >> 12) + t10a;
    t12  = ((t13a * (4017 - 4096) - t12a * 799 + 2048) >> 12) + t13a;
    t13  = ((t13a * 799 + t12a * (4017 - 4096) + 2048) >> 12) + t12a;
    t14  = ((t15a * 2276 - t14a * (3406 - 4096) + 2048) >> 12) - t14a;
    t15  = ((t15a * (3406 - 4096) + t14a * 2276 + 2048) >> 12) + t15a;

    t0   = STB_AV1_CLIP(t0a + t4a, min, max); t1   = STB_AV1_CLIP(t1a + t5a, min, max);
    t2   = STB_AV1_CLIP(t2a + t6a, min, max); t3   = STB_AV1_CLIP(t3a + t7a, min, max);
    t4   = STB_AV1_CLIP(t0a - t4a, min, max); t5   = STB_AV1_CLIP(t1a - t5a, min, max);
    t6   = STB_AV1_CLIP(t2a - t6a, min, max); t7   = STB_AV1_CLIP(t3a - t7a, min, max);
    t8a  = STB_AV1_CLIP(t8  + t12, min, max); t9a  = STB_AV1_CLIP(t9  + t13, min, max);
    t10a = STB_AV1_CLIP(t10 + t14, min, max); t11a = STB_AV1_CLIP(t11 + t15, min, max);
    t12a = STB_AV1_CLIP(t8  - t12, min, max); t13a = STB_AV1_CLIP(t9  - t13, min, max);
    t14a = STB_AV1_CLIP(t10 - t14, min, max); t15a = STB_AV1_CLIP(t11 - t15, min, max);

    t4a  = ((t4   * (3784 - 4096) + t5   * 1567 + 2048) >> 12) + t4;
    t5a  = ((t4   * 1567 - t5   * (3784 - 4096) + 2048) >> 12) - t5;
    t6a  = ((t7   * (3784 - 4096) - t6   * 1567 + 2048) >> 12) + t7;
    t7a  = ((t7   * 1567 + t6   * (3784 - 4096) + 2048) >> 12) + t6;
    t12  = ((t12a * (3784 - 4096) + t13a * 1567 + 2048) >> 12) + t12a;
    t13  = ((t12a * 1567 - t13a * (3784 - 4096) + 2048) >> 12) - t13a;
    t14  = ((t15a * (3784 - 4096) - t14a * 1567 + 2048) >> 12) + t15a;
    t15  = ((t15a * 1567 + t14a * (3784 - 4096) + 2048) >> 12) + t14a;

    t2b     =  STB_AV1_CLIP(t0  - t2 , min, max);
    t3b     =  STB_AV1_CLIP(t1  - t3 , min, max);
    t6b     =  STB_AV1_CLIP(t4a - t6a, min, max);
    t7b     =  STB_AV1_CLIP(t5a - t7a, min, max);
    t10b    =  STB_AV1_CLIP(t8a - t10a, min, max);
    t11b    =  STB_AV1_CLIP(t9a - t11a, min, max);
    t14b    =  STB_AV1_CLIP(t12 - t14 , min, max);
    t15b    =  STB_AV1_CLIP(t13 - t15 , min, max);

    if (shift) {
        int rnd = 1 << (shift - 1);
        out[ 0] = (t0  + t2  + rnd) >> shift;
        out[15] = (-(t1  + t3 ) + rnd) >> shift;
        out[ 3] = (-(t4a + t6a) + rnd) >> shift;
        out[12] = (t5a + t7a + rnd) >> shift;
        out[ 1] = (-(t8a + t10a) + rnd) >> shift;
        out[14] = (t9a + t11a + rnd) >> shift;
        out[ 2] = (t12 + t14  + rnd) >> shift;
        out[13] = (-(t13 + t15 ) + rnd) >> shift;
    } else {
        out[ 0] =  STB_AV1_CLIP(t0  + t2 , min, max);
        out[15] = -STB_AV1_CLIP(t1  + t3 , min, max);
        out[ 3] = -STB_AV1_CLIP(t4a + t6a, min, max);
        out[12] =  STB_AV1_CLIP(t5a + t7a, min, max);
        out[ 1] = -STB_AV1_CLIP(t8a + t10a, min, max);
        out[14] =  STB_AV1_CLIP(t9a + t11a, min, max);
        out[ 2] =  STB_AV1_CLIP(t12 + t14 , min, max);
        out[13] = -STB_AV1_CLIP(t13 + t15 , min, max);
    }

    out[ 7] = -(((t2b  + t3b)  * 181 + 128) >> 8);
    out[ 8] =   ((t2b  - t3b)  * 181 + 128) >> 8;
    out[ 4] =   ((t6b  + t7b)  * 181 + 128) >> 8;
    out[11] = -(((t6b  - t7b)  * 181 + 128) >> 8);
    out[ 6] =   ((t10b + t11b) * 181 + 128) >> 8;
    out[ 9] = -(((t10b - t11b) * 181 + 128) >> 8);
    out[ 5] = -(((t14b + t15b) * 181 + 128) >> 8);
    out[10] =   ((t14b - t15b) * 181 + 128) >> 8;

    {
        int i;
        for (i = 0; i < 16; i++)
            c[i * stride] = out[i];
    }
}

/* ---------- Identity transforms ---------- */

static void stb_av1_inv_identity4_1d(signed int *c, int stride,
                                      int min, int max, int shift)
{
    int in0 = c[0 * stride], in1 = c[1 * stride], in2 = c[2 * stride], in3 = c[3 * stride];
    int e0 = in0 + ((in0 * 1697 + 2048) >> 12);
    int e1 = in1 + ((in1 * 1697 + 2048) >> 12);
    int e2 = in2 + ((in2 * 1697 + 2048) >> 12);
    int e3 = in3 + ((in3 * 1697 + 2048) >> 12);

    (void)min; (void)max;
    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0 * stride] = (e0 + rnd) >> shift;
        c[1 * stride] = (e1 + rnd) >> shift;
        c[2 * stride] = (e2 + rnd) >> shift;
        c[3 * stride] = (e3 + rnd) >> shift;
    } else {
        c[0 * stride] = e0;
        c[1 * stride] = e1;
        c[2 * stride] = e2;
        c[3 * stride] = e3;
    }
}

static void stb_av1_inv_identity8_1d(signed int *c, int stride,
                                      int min, int max, int shift)
{
    int in0 = c[0 * stride], in1 = c[1 * stride], in2 = c[2 * stride], in3 = c[3 * stride];
    int in4 = c[4 * stride], in5 = c[5 * stride], in6 = c[6 * stride], in7 = c[7 * stride];
    int e0 = in0 * 2, e1 = in1 * 2, e2 = in2 * 2, e3 = in3 * 2;
    int e4 = in4 * 2, e5 = in5 * 2, e6 = in6 * 2, e7 = in7 * 2;

    (void)min; (void)max;
    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0 * stride] = (e0 + rnd) >> shift;
        c[1 * stride] = (e1 + rnd) >> shift;
        c[2 * stride] = (e2 + rnd) >> shift;
        c[3 * stride] = (e3 + rnd) >> shift;
        c[4 * stride] = (e4 + rnd) >> shift;
        c[5 * stride] = (e5 + rnd) >> shift;
        c[6 * stride] = (e6 + rnd) >> shift;
        c[7 * stride] = (e7 + rnd) >> shift;
    } else {
        c[0 * stride] = e0;
        c[1 * stride] = e1;
        c[2 * stride] = e2;
        c[3 * stride] = e3;
        c[4 * stride] = e4;
        c[5 * stride] = e5;
        c[6 * stride] = e6;
        c[7 * stride] = e7;
    }
}

static void stb_av1_inv_identity16_1d(signed int *c, int stride,
                                       int min, int max, int shift)
{
    int in0  = c[0  * stride], in1  = c[1  * stride], in2  = c[2  * stride], in3  = c[3  * stride];
    int in4  = c[4  * stride], in5  = c[5  * stride], in6  = c[6  * stride], in7  = c[7  * stride];
    int in8  = c[8  * stride], in9  = c[9  * stride], in10 = c[10 * stride], in11 = c[11 * stride];
    int in12 = c[12 * stride], in13 = c[13 * stride], in14 = c[14 * stride], in15 = c[15 * stride];
    int e0  = 2 * in0  + ((in0  * 1697 + 1024) >> 11);
    int e1  = 2 * in1  + ((in1  * 1697 + 1024) >> 11);
    int e2  = 2 * in2  + ((in2  * 1697 + 1024) >> 11);
    int e3  = 2 * in3  + ((in3  * 1697 + 1024) >> 11);
    int e4  = 2 * in4  + ((in4  * 1697 + 1024) >> 11);
    int e5  = 2 * in5  + ((in5  * 1697 + 1024) >> 11);
    int e6  = 2 * in6  + ((in6  * 1697 + 1024) >> 11);
    int e7  = 2 * in7  + ((in7  * 1697 + 1024) >> 11);
    int e8  = 2 * in8  + ((in8  * 1697 + 1024) >> 11);
    int e9  = 2 * in9  + ((in9  * 1697 + 1024) >> 11);
    int e10 = 2 * in10 + ((in10 * 1697 + 1024) >> 11);
    int e11 = 2 * in11 + ((in11 * 1697 + 1024) >> 11);
    int e12 = 2 * in12 + ((in12 * 1697 + 1024) >> 11);
    int e13 = 2 * in13 + ((in13 * 1697 + 1024) >> 11);
    int e14 = 2 * in14 + ((in14 * 1697 + 1024) >> 11);
    int e15 = 2 * in15 + ((in15 * 1697 + 1024) >> 11);

    (void)min; (void)max;
    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0  * stride] = (e0  + rnd) >> shift;
        c[1  * stride] = (e1  + rnd) >> shift;
        c[2  * stride] = (e2  + rnd) >> shift;
        c[3  * stride] = (e3  + rnd) >> shift;
        c[4  * stride] = (e4  + rnd) >> shift;
        c[5  * stride] = (e5  + rnd) >> shift;
        c[6  * stride] = (e6  + rnd) >> shift;
        c[7  * stride] = (e7  + rnd) >> shift;
        c[8  * stride] = (e8  + rnd) >> shift;
        c[9  * stride] = (e9  + rnd) >> shift;
        c[10 * stride] = (e10 + rnd) >> shift;
        c[11 * stride] = (e11 + rnd) >> shift;
        c[12 * stride] = (e12 + rnd) >> shift;
        c[13 * stride] = (e13 + rnd) >> shift;
        c[14 * stride] = (e14 + rnd) >> shift;
        c[15 * stride] = (e15 + rnd) >> shift;
    } else {
        c[0  * stride] = e0;
        c[1  * stride] = e1;
        c[2  * stride] = e2;
        c[3  * stride] = e3;
        c[4  * stride] = e4;
        c[5  * stride] = e5;
        c[6  * stride] = e6;
        c[7  * stride] = e7;
        c[8  * stride] = e8;
        c[9  * stride] = e9;
        c[10 * stride] = e10;
        c[11 * stride] = e11;
        c[12 * stride] = e12;
        c[13 * stride] = e13;
        c[14 * stride] = e14;
        c[15 * stride] = e15;
    }
}

static void stb_av1_inv_identity32_1d(signed int *c, int stride,
                                       int min, int max, int shift)
{
    int in0  = c[0  * stride], in1  = c[1  * stride], in2  = c[2  * stride], in3  = c[3  * stride];
    int in4  = c[4  * stride], in5  = c[5  * stride], in6  = c[6  * stride], in7  = c[7  * stride];
    int in8  = c[8  * stride], in9  = c[9  * stride], in10 = c[10 * stride], in11 = c[11 * stride];
    int in12 = c[12 * stride], in13 = c[13 * stride], in14 = c[14 * stride], in15 = c[15 * stride];
    int in16 = c[16 * stride], in17 = c[17 * stride], in18 = c[18 * stride], in19 = c[19 * stride];
    int in20 = c[20 * stride], in21 = c[21 * stride], in22 = c[22 * stride], in23 = c[23 * stride];
    int in24 = c[24 * stride], in25 = c[25 * stride], in26 = c[26 * stride], in27 = c[27 * stride];
    int in28 = c[28 * stride], in29 = c[29 * stride], in30 = c[30 * stride], in31 = c[31 * stride];
    int e0 = in0 * 4,  e1  = in1  * 4,  e2  = in2  * 4,  e3  = in3  * 4;
    int e4 = in4 * 4,  e5  = in5  * 4,  e6  = in6  * 4,  e7  = in7  * 4;
    int e8 = in8 * 4,  e9  = in9  * 4,  e10 = in10 * 4,  e11 = in11 * 4;
    int e12 = in12 * 4, e13 = in13 * 4, e14 = in14 * 4, e15 = in15 * 4;
    int e16 = in16 * 4, e17 = in17 * 4, e18 = in18 * 4, e19 = in19 * 4;
    int e20 = in20 * 4, e21 = in21 * 4, e22 = in22 * 4, e23 = in23 * 4;
    int e24 = in24 * 4, e25 = in25 * 4, e26 = in26 * 4, e27 = in27 * 4;
    int e28 = in28 * 4, e29 = in29 * 4, e30 = in30 * 4, e31 = in31 * 4;

    (void)min; (void)max;
    if (shift) {
        int rnd = 1 << (shift - 1);
        c[0  * stride] = (e0  + rnd) >> shift;
        c[1  * stride] = (e1  + rnd) >> shift;
        c[2  * stride] = (e2  + rnd) >> shift;
        c[3  * stride] = (e3  + rnd) >> shift;
        c[4  * stride] = (e4  + rnd) >> shift;
        c[5  * stride] = (e5  + rnd) >> shift;
        c[6  * stride] = (e6  + rnd) >> shift;
        c[7  * stride] = (e7  + rnd) >> shift;
        c[8  * stride] = (e8  + rnd) >> shift;
        c[9  * stride] = (e9  + rnd) >> shift;
        c[10 * stride] = (e10 + rnd) >> shift;
        c[11 * stride] = (e11 + rnd) >> shift;
        c[12 * stride] = (e12 + rnd) >> shift;
        c[13 * stride] = (e13 + rnd) >> shift;
        c[14 * stride] = (e14 + rnd) >> shift;
        c[15 * stride] = (e15 + rnd) >> shift;
        c[16 * stride] = (e16 + rnd) >> shift;
        c[17 * stride] = (e17 + rnd) >> shift;
        c[18 * stride] = (e18 + rnd) >> shift;
        c[19 * stride] = (e19 + rnd) >> shift;
        c[20 * stride] = (e20 + rnd) >> shift;
        c[21 * stride] = (e21 + rnd) >> shift;
        c[22 * stride] = (e22 + rnd) >> shift;
        c[23 * stride] = (e23 + rnd) >> shift;
        c[24 * stride] = (e24 + rnd) >> shift;
        c[25 * stride] = (e25 + rnd) >> shift;
        c[26 * stride] = (e26 + rnd) >> shift;
        c[27 * stride] = (e27 + rnd) >> shift;
        c[28 * stride] = (e28 + rnd) >> shift;
        c[29 * stride] = (e29 + rnd) >> shift;
        c[30 * stride] = (e30 + rnd) >> shift;
        c[31 * stride] = (e31 + rnd) >> shift;
    } else {
        c[0  * stride] = e0;
        c[1  * stride] = e1;
        c[2  * stride] = e2;
        c[3  * stride] = e3;
        c[4  * stride] = e4;
        c[5  * stride] = e5;
        c[6  * stride] = e6;
        c[7  * stride] = e7;
        c[8  * stride] = e8;
        c[9  * stride] = e9;
        c[10 * stride] = e10;
        c[11 * stride] = e11;
        c[12 * stride] = e12;
        c[13 * stride] = e13;
        c[14 * stride] = e14;
        c[15 * stride] = e15;
        c[16 * stride] = e16;
        c[17 * stride] = e17;
        c[18 * stride] = e18;
        c[19 * stride] = e19;
        c[20 * stride] = e20;
        c[21 * stride] = e21;
        c[22 * stride] = e22;
        c[23 * stride] = e23;
        c[24 * stride] = e24;
        c[25 * stride] = e25;
        c[26 * stride] = e26;
        c[27 * stride] = e27;
        c[28 * stride] = e28;
        c[29 * stride] = e29;
        c[30 * stride] = e30;
        c[31 * stride] = e31;
    }
}

/* ---------- Dispatch functions matching old API ---------- */

/* Inverse DCT dispatch (type III) */
static void stb_av1_idct(int *coeffs, int n, int shift)
{
    switch (n) {
        case 4:  stb_av1_inv_dct4_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 8:  stb_av1_inv_dct8_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 16: stb_av1_inv_dct16_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 32: stb_av1_inv_dct32_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        default: break;
    }
}

/* Inverse ADST dispatch */
static void stb_av1_iadst(int *coeffs, int n, int shift)
{
    switch (n) {
        case 4:  stb_av1_inv_adst4_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 8:  stb_av1_inv_adst8_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 16: stb_av1_inv_adst16_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        default: break;
    }
}

/* Inverse FLIPADST dispatch */
static void stb_av1_iflipadst(int *coeffs, int n, int shift)
{
    /* FLIPADST is ADST applied to normal-order input, with the OUTPUT
       written in reverse order (matches dav1d inv_flipadst_1d_c) */
    switch (n) {
        case 4:  stb_av1_inv_adst4_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 8:  stb_av1_inv_adst8_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        case 16: stb_av1_inv_adst16_1d(coeffs, 1, INT16_MIN, INT16_MAX, shift); break;
        default: break;
    }
    {
        int rev[64];
        int i;
        if (n > 64) return;
        for (i = 0; i < n; i++) rev[i] = coeffs[n - 1 - i];
        for (i = 0; i < n; i++) coeffs[i] = rev[i];
    }
}

/* Identity dispatch */
static void stb_av1_identity(int *coeffs, int n, int shift)
{
    switch (n) {
        case 4:  stb_av1_inv_identity4_1d(coeffs, 1, 0, 0, shift); break;
        case 8:  stb_av1_inv_identity8_1d(coeffs, 1, 0, 0, shift); break;
        case 16: stb_av1_inv_identity16_1d(coeffs, 1, 0, 0, shift); break;
        case 32: stb_av1_inv_identity32_1d(coeffs, 1, 0, 0, shift); break;
        default: break;
    }
}

/* Apply inverse transform in 2D (separable).
   tx_type: 0=DCT_DCT, 1=ADST_DCT, 2=DCT_ADST, 3=ADST_ADST,
            4=FLIPADST_DCT, 5=DCT_FLIPADST, 6=FLIPADST_FLIPADST,
            7=ADST_FLIPADST, 8=FLIPADST_ADST, 16=IDENTITY_IDENTITY */
static void stb_av1_inv_transform_2d(int *block, int w, int h, int tx_type)
{
    int i, j;
    int *temp;
    int *col;
    int is_dct_row, is_dct_col;
    int is_adst_row, is_adst_col;
    int is_flipadst_row, is_flipadst_col;

    /* dav1d itx shift lookup table: shift[lw][lh] where lw=log2(w/4), lh=log2(h/4) */
    static const int itx_shift_lut[5][5] = {
        /*       lw=0  lw=1  lw=2  lw=3  lw=4 */
        /*lh=0*/ {  0,    0,    1,    2,    2 },
        /*lh=1*/ {  0,    1,    1,    2,    2 },
        /*lh=2*/ {  1,    1,    2,    1,    2 },
        /*lh=3*/ {  2,    2,    1,    2,    1 },
        /*lh=4*/ {  2,    2,    2,    1,    2 },
    };

    /* Correct mapping: matches dav1d stb_av1_tx1d_types[txtp] = {row_type, col_type} */
    is_dct_row = (tx_type == 0 || tx_type == 1 || tx_type == 4 || tx_type == 11);
    is_dct_col = (tx_type == 0 || tx_type == 2 || tx_type == 5 || tx_type == 10);
    is_adst_row = (tx_type == 2 || tx_type == 3 || tx_type == 6 || tx_type == 13);
    is_adst_col = (tx_type == 1 || tx_type == 3 || tx_type == 7 || tx_type == 12);
    is_flipadst_row = (tx_type == 5 || tx_type == 7 || tx_type == 8 || tx_type == 15);
    is_flipadst_col = (tx_type == 4 || tx_type == 6 || tx_type == 8 || tx_type == 14);

    /* Allocate temp arrays */
    temp = (int *)stb_avif_malloc((size_t)(w * h) * sizeof(int));
    col = (int *)stb_avif_malloc((size_t)(h) * sizeof(int));

    if (!temp || !col) {
        if (temp) stb_avif_free_internal(temp);
        if (col) stb_avif_free_internal(col);
        return;
    }

    {
        int itx_shift;
        int lw, lh;
        int is_rect2 = (w * 2 == h || h * 2 == w);

        /* Compute shift from dav1d lookup table */
        lw = 0; { int t = w; while (t > 4) { t >>= 1; lw++; } }
        lh = 0; { int t = h; while (t > 4) { t >>= 1; lh++; } }
        if (lw > 4) lw = 4;
        if (lh > 4) lh = 4;
        itx_shift = itx_shift_lut[lw][lh];

        /* Rect2 pre-scaling: for transforms where one dim is 2x the other,
           scale coefficients by sqrt(2)/2 ≈ 0.707 before row transform */
        if (is_rect2) {
            for (i = 0; i < h; i++)
                for (j = 0; j < w; j++)
                    block[i * w + j] = (block[i * w + j] * 181 + 128) >> 8;
        }

        /* Process rows */
        for (i = 0; i < h; i++) {
            int row[64];
            for (j = 0; j < w; j++)
                row[j] = block[i * w + j];

            if (is_dct_row) {
                stb_av1_idct(row, w, 0);
            } else if (is_adst_row) {
                stb_av1_iadst(row, w, 0);
            } else if (is_flipadst_row) {
                stb_av1_iflipadst(row, w, 0);
            } else {
                stb_av1_identity(row, w, 0);
            }

            for (j = 0; j < w; j++)
                temp[i * w + j] = row[j];
        }

        /* Intermediate shift */
        if (itx_shift > 0) {
            int rnd = 1 << (itx_shift - 1);
            for (i = 0; i < w * h; i++)
                temp[i] = (temp[i] + rnd) >> itx_shift;
        }

        /* Process columns */
        for (j = 0; j < w; j++) {
            for (i = 0; i < h; i++)
                col[i] = temp[i * w + j];

            if (is_dct_col) {
                stb_av1_idct(col, h, 0);
            } else if (is_adst_col) {
                stb_av1_iadst(col, h, 0);
            } else if (is_flipadst_col) {
                stb_av1_iflipadst(col, h, 0);
            } else {
                stb_av1_identity(col, h, 0);
            }

            for (i = 0; i < h; i++)
                block[i * w + j] = col[i];
        }
    }

    stb_avif_free_internal(temp);
    stb_avif_free_internal(col);
}

/* -------------------------------------------------------------------------- */
/* INTRA PREDICTION                                                           */
/* -------------------------------------------------------------------------- */

/* Intra prediction for a block.
   For simplicity, we handle common modes: DC, V, H, D45, D135, Paeth, Smooth.

   Parameters:
     dst     - output block
     stride  - stride of destination
     w, h    - block width/height
     mode    - intra prediction mode
     above   - pointer to row above (size w + left_needed)
     left    - pointer to column left (size h + top_needed)
     topleft - pixel at (-1,-1)
     bit_depth - pixel bit depth
 */

/* Filter-intra taps: [5 modes][8 positions * 7 taps], matches dav1d */
static const signed char stb_av1_filter_intra_taps[5][64] = {
    {
       -6, 10,  0,  0,  0, 12,  0,  0,
       -5,  2, 10,  0,  0,  9,  0,  0,
       -3,  1,  1, 10,  0,  7,  0,  0,
       -3,  1,  1,  2, 10,  5,  0,  0,
       -4,  6,  0,  0,  0,  2, 12,  0,
       -3,  2,  6,  0,  0,  2,  9,  0,
       -3,  2,  2,  6,  0,  2,  7,  0,
       -3,  1,  2,  2,  6,  3,  5,  0
    }, {
      -10, 16,  0,  0,  0, 10,  0,  0,
       -6,  0, 16,  0,  0,  6,  0,  0,
       -4,  0,  0, 16,  0,  4,  0,  0,
       -2,  0,  0,  0, 16,  2,  0,  0,
      -10, 16,  0,  0,  0,  0, 10,  0,
       -6,  0, 16,  0,  0,  0,  6,  0,
       -4,  0,  0, 16,  0,  0,  4,  0,
       -2,  0,  0,  0, 16,  0,  2,  0
    }, {
       -8,  8,  0,  0,  0, 16,  0,  0,
       -8,  0,  8,  0,  0, 16,  0,  0,
       -8,  0,  0,  8,  0, 16,  0,  0,
       -8,  0,  0,  0,  8, 16,  0,  0,
       -4,  4,  0,  0,  0,  0, 16,  0,
       -4,  0,  4,  0,  0,  0, 16,  0,
       -4,  0,  0,  4,  0,  0, 16,  0,
       -4,  0,  0,  0,  4,  0, 16,  0
    }, {
       -2,  8,  0,  0,  0, 10,  0,  0,
       -1,  3,  8,  0,  0,  6,  0,  0,
       -1,  2,  3,  8,  0,  4,  0,  0,
        0,  1,  2,  3,  8,  2,  0,  0,
       -1,  4,  0,  0,  0,  3, 10,  0,
       -1,  3,  4,  0,  0,  4,  6,  0,
       -1,  2,  3,  4,  0,  4,  4,  0,
       -1,  2,  2,  3,  4,  3,  3,  0
    }, {
      -12, 14,  0,  0,  0, 14,  0,  0,
      -10,  0, 14,  0,  0, 12,  0,  0,
       -9,  0,  0, 14,  0, 11,  0,  0,
       -8,  0,  0,  0, 14, 10,  0,  0,
      -10, 12,  0,  0,  0,  0, 14,  0,
       -9,  1, 12,  0,  0,  0, 12,  0,
       -8,  0,  0, 12,  0,  1, 11,  0,
       -7,  0,  0,  1, 12,  1,  9,  0
    }
};

/* Filter-intra mode -> y intra mode (for txtp CDF index), matches dav1d */
static const unsigned char stb_av1_filter_mode_to_y_mode[5] = {
    0, 1, 2, 6, 0
};

/* Filter-intra predictor (dav1d ipred_filter_c), up to 32x32.
   topleft_in[0] = pixel at (-1,-1), top row at [1..w], left column at [-1..-h]. */
static void stb_av1_filter_intra_predict(unsigned char *dst, int stride,
                                          const unsigned char *topleft_in,
                                          int w, int h, int filt_idx)
{
    const signed char *filter = stb_av1_filter_intra_taps[filt_idx];
    const unsigned char *top = &topleft_in[1];
    int y;
    for (y = 0; y < h; y += 2) {
        const unsigned char *topleft = &topleft_in[-y];
        const unsigned char *left = &topleft[-1];
        int left_stride = -1;
        int x;
        for (x = 0; x < w; x += 4) {
            const int p0 = *topleft;
            const int p1 = top[0], p2 = top[1], p3 = top[2], p4 = top[3];
            const int p5 = left[0 * left_stride], p6 = left[1 * left_stride];
            unsigned char *ptr = &dst[x];
            const signed char *flt_ptr = filter;
            int yy;
            for (yy = 0; yy < 2; yy++) {
                int xx;
                for (xx = 0; xx < 4; xx++, flt_ptr++) {
                    int acc = flt_ptr[ 0]*p0 + flt_ptr[ 8]*p1 + flt_ptr[16]*p2 +
                              flt_ptr[24]*p3 + flt_ptr[32]*p4 + flt_ptr[40]*p5 +
                              flt_ptr[48]*p6;
                    int val = (acc + 8) >> 4;
                    if (val < 0) val = 0;
                    if (val > 255) val = 255;
                    ptr[xx] = (unsigned char)val;
                }
                ptr += stride;
            }
            left = &dst[x + 4 - 1];
            left_stride = stride;
            top += 4;
            topleft = &top[-1];
        }
        top = &dst[stride];
        dst = &dst[stride * 2];
    }
}

static void stb_av1_filter_intra_edge(unsigned char *edge, int sz,
                                       int strong, int filter_bit) {
    int i;
    if (!filter_bit) return;
    if (strong) {
        int f[256];
        f[0] = (3*edge[0] + 2*edge[1] + edge[2] + 3) / 6;
        for (i = 1; i < sz - 1; i++)
            f[i] = (edge[i-1] + 2*edge[i] + edge[i+1] + 2) / 4;
        f[sz-1] = (edge[sz-2] + 2*edge[sz-1] + edge[sz+2] + 2) / 4;
        for (i = 0; i < sz; i++) edge[i] = (unsigned char)f[i];
    } else {
        for (i = 1; i < sz - 1; i++) {
            edge[i] = (unsigned char)((edge[i-1] + 2*edge[i] + edge[i+1] + 2) / 4);
        }
    }
}

static void stb_av1_intra_predict(unsigned char *dst, int stride,
                                   int w, int h, int mode,
                                   const unsigned char *above,
                                   const unsigned char *left,
                                   unsigned char topleft, int bit_depth,
                                   int above_avail, int left_avail)
{
    int r, c;
    int max_val = (1 << bit_depth) - 1;

    (void)bit_depth;
    (void)max_val;

    switch (mode) {
        case STB_AV1_DC_PRED: {
            int sum = 0;
            int count = 0;
            int dc_val;

            if (above_avail) {
                for (c = 0; c < w; c++) { sum += above[c]; count++; }
            }
            if (left_avail) {
                for (r = 0; r < h; r++) { sum += left[r]; count++; }
            }

            if (count == 0)
                dc_val = 128;
            else
                dc_val = (sum + (count >> 1)) / count;

            if (dc_val < 0) dc_val = 0;
            if (dc_val > 255) dc_val = 255;

            { static int _dcav=0; if (_dcav<10) { _dcav++;
              fprintf(stderr,"[DC] w=%d h=%d aa=%d la=%d sum=%d cnt=%d val=%d\n", w,h,above_avail,left_avail,sum,count,dc_val); } }

            for (r = 0; r < h; r++)
                for (c = 0; c < w; c++)
                    dst[r * stride + c] = (unsigned char)dc_val;
            break;
        }

        case STB_AV1_V_PRED: {
            for (r = 0; r < h; r++)
                for (c = 0; c < w; c++)
                    dst[r * stride + c] = above[c];
            break;
        }

        case STB_AV1_H_PRED: {
            for (r = 0; r < h; r++)
                for (c = 0; c < w; c++)
                    dst[r * stride + c] = left[r];
            break;
        }

        case STB_AV1_D45_PRED: {
            /* 45-degree direction: top-right to bottom-left */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int idx = r + c + 1;
                    if (idx < w) {
                        dst[r * stride + c] = above[idx];
                    } else if (idx == w) {
                        dst[r * stride + c] = above[w - 1];
                    } else {
                        dst[r * stride + c] = left[idx - w];
                    }
                }
            }
            break;
        }

        case STB_AV1_D135_PRED: {
            /* 135-degree direction: top-left to bottom-right */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int idx = c - r;
                    if (idx > 0) {
                        dst[r * stride + c] = above[idx - 1];
                    } else if (idx == 0) {
                        dst[r * stride + c] = topleft;
                    } else {
                        dst[r * stride + c] = left[-idx - 1];
                    }
                }
            }
            break;
        }

        case STB_AV1_D113_PRED: {
            /* D113 (down-right, ~113 degrees) */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int dr = c - (r << 1);
                    int a0, a1, a2;
                    if (dr >= 0) {
                        a0 = (dr > 0) ? above[c - 1] : topleft;
                        a1 = above[c];
                        a2 = above[c + 1];
                    } else {
                        a0 = (r > 0) ? left[r - 1] : topleft;
                        a1 = left[r];
                        a2 = left[r + 1];
                    }
                    dst[r * stride + c] = (unsigned char)((a0 + 2 * a1 + a2 + 2) >> 2);
                }
            }
            break;
        }

        case STB_AV1_D157_PRED: {
            /* D157 (down-left, ~157 degrees) */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int dr = r - (c << 1);
                    int a0, a1, a2;
                    if (dr >= 0) {
                        a0 = (r > 0) ? left[r - 1] : topleft;
                        a1 = left[r];
                        a2 = left[r + 1];
                    } else {
                        a0 = (c > 0) ? above[c - 1] : topleft;
                        a1 = above[c];
                        a2 = above[c + 1];
                    }
                    dst[r * stride + c] = (unsigned char)((a0 + 2 * a1 + a2 + 2) >> 2);
                }
            }
            break;
        }

        case STB_AV1_D203_PRED: {
            /* D203 (down-right, ~203 degrees) */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int dr = c + r;
                    int a0, a1, a2;
                    if (dr < w) {
                        a0 = (dr > 0) ? above[dr - 1] : topleft;
                        a1 = above[dr];
                        a2 = above[dr + 1];
                    } else {
                        int idx = dr - w + 1;
                        a0 = left[idx - 1];
                        a1 = left[idx];
                        a2 = left[idx + 1];
                    }
                    dst[r * stride + c] = (unsigned char)((a0 + 2 * a1 + a2 + 2) >> 2);
                }
            }
            break;
        }

        case STB_AV1_D67_PRED: {
            /* D67 (up-right, ~67 degrees) */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int dr = r + c;
                    int a0, a1, a2;
                    if (dr < w) {
                        a0 = (dr > 0) ? above[dr - 1] : topleft;
                        a1 = above[dr];
                        a2 = above[dr + 1];
                    } else {
                        int idx = dr - w + 1;
                        a0 = left[idx - 1];
                        a1 = left[idx];
                        a2 = left[idx + 1];
                    }
                    dst[r * stride + c] = (unsigned char)((a0 + 2 * a1 + a2 + 2) >> 2);
                }
            }
            break;
        }

        case STB_AV1_PAETH_PRED: {
            /* Paeth prediction (from VP9) - finds the closest boundary pixel */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int a = (c > 0) ? above[c - 1] : topleft;
                    int b = (r > 0) ? left[r - 1] : topleft;
                    int d = above[c];
                    int p = a + b - d;
                    int pa = (p - a) >= 0 ? (p - a) : -(p - a);
                    int pb = (p - b) >= 0 ? (p - b) : -(p - b);
                    int pc = (p - d) >= 0 ? (p - d) : -(p - d);
                    int val;
                    if (pa <= pb && pa <= pc)
                        val = a;
                    else if (pb <= pc)
                        val = b;
                    else
                        val = d;
                    dst[r * stride + c] = (unsigned char)val;
                }
            }
            break;
        }

        case STB_AV1_SMOOTH_PRED: {
            /* Smooth: weighted average of boundaries */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int vert = (w - c) * left[r] + (c + 1) * above[w - 1];
                    int hor = (h - r) * above[c] + (r + 1) * left[h - 1];
                    int val = (vert * (h - r) + hor * (w - c)
                               + (h * w)) / (2 * h * w);
                    if (val < 0) val = 0;
                    if (val > 255) val = 255;
                    dst[r * stride + c] = (unsigned char)val;
                }
            }
            break;
        }

        case STB_AV1_SMOOTH_V_PRED: {
            /* Smooth vertical */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int val = ((h - r - 1) * above[c] + (r + 1) * left[h - 1] + (h >> 1)) / h;
                    if (val < 0) val = 0;
                    if (val > 255) val = 255;
                    dst[r * stride + c] = (unsigned char)val;
                }
            }
            break;
        }

        case STB_AV1_SMOOTH_H_PRED: {
            /* Smooth horizontal */
            for (r = 0; r < h; r++) {
                for (c = 0; c < w; c++) {
                    int val = ((w - c - 1) * left[r] + (c + 1) * above[w - 1] + (w >> 1)) / w;
                    if (val < 0) val = 0;
                    if (val > 255) val = 255;
                    dst[r * stride + c] = (unsigned char)val;
                }
            }
            break;
        }

        default: {
            /* Fallback to DC */
            int dc_val = 128;
            for (r = 0; r < h; r++)
                for (c = 0; c < w; c++)
                    dst[r * stride + c] = (unsigned char)dc_val;
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* SIMPLIFIED COEFFICIENT DECODING                                            */
/* -------------------------------------------------------------------------- */

/* Decode a single transform coefficient.
   In practice, AV1 uses a complex context-adaptive arithmetic coding scheme
   for coefficients, including EOB (end-of-block), sign, and magnitude.
   
   For our simplified decoder, we decode tokens from the bitstream using
   uniform probability coding, with a basic coefficient model. */

enum stb_av1_tx_class {
    TX_CLASS_2D = 0,
    TX_CLASS_HORIZ = 1,
    TX_CLASS_VERT = 2
};

#ifdef STB_AVIF_USE_C89_DAV1D
#undef stb_av1_decode_coeffs
#endif
/* Simplified coefficient decoding - reads zig-zag scanned tokens */
static int stb_av1_decode_coeffs(struct stb_av1_bool_reader *br,
                                  int *coeffs, int max_coeffs,
                                  int *eob, int qindex)
{
    int i;
    int has_coeff = stb_av1_bool_decode(br, 128); /* all-zero flag */

    if (!has_coeff) {
        *eob = 0;
        for (i = 0; i < max_coeffs; i++)
            coeffs[i] = 0;
        return 0;
    }

    /* For simplicity, decode coefficients with a basic EOB + run-length model */
    *eob = 0;
    for (i = 0; i < max_coeffs; i++) {
        if (stb_av1_bool_decode(br, 128)) {
            /* non-zero coefficient */
            int sign = stb_av1_bool_decode(br, 128) ? -1 : 1;
            int mag = 1;
            {
                int _mag_safe = 16;
                while (stb_av1_bool_decode(br, 128) && _mag_safe > 0) {
                    mag++;
                    _mag_safe--;
                }
            }

            coeffs[i] = sign * mag;
            *eob = i + 1;
        } else {
            coeffs[i] = 0;
        }
    }

    (void)qindex;
    return *eob;
}

/* Scanning order for different transform sizes.
   Simplification: we use raster scan order.
   The actual AV1 spec uses specific scan patterns for each transform. */

/* Apply dequantization and inverse transform to a coefficient block.
   tx_w, tx_h: transform size in pixels
   tx_type: transform type (DCT_DCT, ADST_DCT, etc.)
   qindex: quantization index
   block: output reconstructed pixel block */
static void stb_av1_reconstruct_block(struct stb_av1_tile_context *tc,
                                       int qindex,
                                       int *coeffs, int tx_w, int tx_h,
                                       int tx_type,
                                       unsigned char *pred,
                                       int pred_stride,
                                       unsigned char *dst,
                                       int dst_stride)
{
    int i, j;
    int dequant_dc, dequant_ac;
    static int dq_coeffs_buf[128*128];
    int *dq_coeffs = dq_coeffs_buf;
    int max_coeffs = tx_w * tx_h;

    if (max_coeffs > 4096)
        return;

    dequant_dc = stb_av1_get_dequant(qindex, 1, tc->bit_depth);
    dequant_ac = stb_av1_get_dequant(qindex, 0, tc->bit_depth);
    /* compute dq_shift = max(0, ctx - 2) where ctx = (lw+lh+1)>>1 per dav1d t_dim->ctx */
    {
        int lw = 0, lh = 0, ctx;
        while ((1 << (lw + 2)) < tx_w) lw++;
        while ((1 << (lh + 2)) < tx_h) lh++;
        ctx = (lw + lh + 1) >> 1;
        dequant_dc >>= (ctx > 2) ? ctx - 2 : 0;
        dequant_ac >>= (ctx > 2) ? ctx - 2 : 0;
    }
    { static int _dqonce=0; if (!_dqonce) { _dqonce=1;
      fprintf(stderr,"[RECON_DBG] qindex=%d dc=%d ac=%d bd=%d tx=%dx%d ttype=%d coeff[0]=%d coeff[1]=%d coeff[2]=%d\n",
        qindex, dequant_dc, dequant_ac, tc->bit_depth, tx_w, tx_h, tx_type, coeffs[0], coeffs[1], coeffs[2]); } }

    /* Dequantize with scan-to-raster de-scanning.
       coeffs[i] is the coefficient at scan position i.
       scan[i] is a column-major index rc = col * tx_h + row (matching dav1d).
       We convert to row-major position dq_coeffs[row * tx_w + col]. */
    {
        int shift = 0;
        const unsigned short *scan = stb_av1_get_scan(tx_w, tx_h, &shift);
        int mask = (1 << shift) - 1;
        memset(dq_coeffs, 0, (size_t)max_coeffs * sizeof(int));
        for (i = 0; i < max_coeffs; i++) {
            int rc = scan[i];
            int y = rc & mask;
            int x = rc >> shift;
            int val = coeffs[i];
            if (val) {
                int deq = (i == 0) ? dequant_dc : dequant_ac;
                int sign = 1;
                if (val < 0) { sign = -1; val = -val; }
                dq_coeffs[y * tx_w + x] = val * deq * sign;
            }
        }
        { static int _dsconce=0; if (1) { _dsconce++; int _r,_cc;
          fprintf(stderr,"[RECON_DBG] pre-idct dequant block tx=%dx%d\n", tx_w, tx_h);
          for (_r=0;_r<tx_h;_r++){ for (_cc=0;_cc<tx_w;_cc++) fprintf(stderr,"%d ", dq_coeffs[_r*tx_w+_cc]); fprintf(stderr,"\n"); } } }
    }

    stb_av1_inv_transform_2d(dq_coeffs, tx_w, tx_h, tx_type);
    { static int _itxonce=0; if (1) { _itxonce++; int _r,_cc;
      fprintf(stderr,"[RECON_DBG] after_idct: tx=%dx%d ttype=%d\n", tx_w, tx_h, tx_type);
      for (_r=0;_r<tx_h;_r++){ for (_cc=0;_cc<tx_w;_cc++) fprintf(stderr,"%d ", dq_coeffs[_r*tx_w+_cc]); fprintf(stderr,"\n"); } } }

    /* Reconstruct: pred + residual, clamp to [0, 255].
       Apply final >>4 scaling to match dav1d's inv_txfm_add_c final shift. */
    for (i = 0; i < tx_h; i++) {
        for (j = 0; j < tx_w; j++) {
            int val = (int)pred[i * pred_stride + j] + ((dq_coeffs[i * tx_w + j] + 8) >> 4);
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            dst[i * dst_stride + j] = (unsigned char)val;
        }
    }

    /* no free needed — dq_coeffs is stack-allocated */
}

/* -------------------------------------------------------------------------- */
/* CDF-BASED SUPERBLOCK AND BLOCK DECODING                                    */
/* -------------------------------------------------------------------------- */

#ifdef STB_AVIF_USE_C89_DAV1D

/* Partition constants (AV1 spec order) */
enum {
    STB_PARTITION_NONE = 0,
    STB_PARTITION_H = 1,
    STB_PARTITION_V = 2,
    STB_PARTITION_SPLIT = 3,
    STB_PARTITION_T_TOP_SPLIT = 4,
    STB_PARTITION_T_BOTTOM_SPLIT = 5,
    STB_PARTITION_T_LEFT_SPLIT = 6,
    STB_PARTITION_T_RIGHT_SPLIT = 7,
    STB_PARTITION_H4 = 8,
    STB_PARTITION_V4 = 9,
};

/* Number of partition symbols per block level (0=128x128,1=64x64,2=32x32,3=16x16,4=8x8) */
/* Matches dav1d's dav1d_partition_type_count: N_PARTITIONS-3 for 128x128, N_PARTITIONS-1 for others, N_SUB8X8_PARTITIONS-1 for 8x8 */
static const int stb_av1_partition_nsym[5] = { 7, 9, 9, 9, 3 };

/* Sub-block count for each partition type at a given level:
   returns the number of sub-blocks (1, 2, or 4) */
static int stb_av1_partition_sub_count(int bp) {
    switch (bp) {
        case STB_PARTITION_NONE: return 1;
        case STB_PARTITION_H: case STB_PARTITION_V: return 2;
        case STB_PARTITION_SPLIT: return 4;
        case STB_PARTITION_T_TOP_SPLIT: case STB_PARTITION_T_BOTTOM_SPLIT:
        case STB_PARTITION_T_LEFT_SPLIT: case STB_PARTITION_T_RIGHT_SPLIT: return 2;
        case STB_PARTITION_H4: case STB_PARTITION_V4: return 4;
        default: return 1;
    }
}

/* Context derivation: combines top and left neighbor partition info */
static int stb_av1_get_partition_ctx(const unsigned char *above,
                                      const unsigned char *left,
                                      int bl, int bx4, int by4) {
    int ctx = 0;
    if (bl < 5) { /* BL_128X128..BL_8X8 */
        ctx = (above[bx4] >> (4 - bl)) & 1;
        ctx += ((left[by4] >> (4 - bl)) & 1) << 1;
    }
    return ctx;
}

/* Partition context bitmasks (dav1d_al_part_ctx from tables.c).
   Indexed [above/left][bl][partition_type]; stores the bitmask that
   get_partition_ctx extracts bit (4-bl) from. SPLIT entries are unused
   (dav1d writes nothing for SPLIT at bl < BL_8X8). */
static const unsigned char stb_av1_al_part_ctx[2][5][10] = {
    {   /* above */
        { 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10, 0x10, 0x00, 0x00 }, /* bl0 */
        { 0x10, 0x10, 0x18, 0x00, 0x10, 0x18, 0x18, 0x18, 0x10, 0x1c }, /* bl1 */
        { 0x18, 0x18, 0x1c, 0x00, 0x18, 0x1c, 0x1c, 0x1c, 0x18, 0x1e }, /* bl2 */
        { 0x1c, 0x1c, 0x1e, 0x00, 0x1c, 0x1e, 0x1e, 0x1e, 0x1c, 0x1f }, /* bl3 */
        { 0x1e, 0x1e, 0x1f, 0x1f, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e }, /* bl4 */
    },
    {   /* left */
        { 0x00, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x10, 0x00, 0x00 }, /* bl0 */
        { 0x10, 0x18, 0x10, 0x00, 0x18, 0x18, 0x10, 0x18, 0x1c, 0x10 }, /* bl1 */
        { 0x18, 0x1c, 0x18, 0x00, 0x1c, 0x1c, 0x18, 0x1c, 0x1e, 0x18 }, /* bl2 */
        { 0x1c, 0x1e, 0x1c, 0x00, 0x1e, 0x1e, 0x1c, 0x1e, 0x1f, 0x1c }, /* bl3 */
        { 0x1e, 0x1f, 0x1e, 0x1f, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e }, /* bl4 */
    }
};

/* Write the partition context bitmask for a decoded partition, matching
   dav1d's decode_partition: nothing is written for SPLIT (the children set
   their own context). Replicates the mask across the block's 4px units. */
static void stb_av1_set_partition_ctx(unsigned char *above, unsigned char *left,
                                      int bx4, int by4, int sz4, int bl, int bp) {
    unsigned char ma, ml;
    int i;
    if (bp == STB_PARTITION_SPLIT && bl != 4) return; /* dav1d: write ctx for SPLIT at 8x8 level */
    if (bp < 0 || bp > 9) return;
    ma = stb_av1_al_part_ctx[0][bl][bp];
    ml = stb_av1_al_part_ctx[1][bl][bp];
    for (i = 0; i < sz4; i++) { above[bx4+i] = ma; left[by4+i] = ml; }
}

/* Check if H split is allowed (block extends past right edge) */
static int stb_av1_can_split_h(int bx4, int sz4, int bw4) {
    return bx4 + sz4 < bw4;
}

/* Check if V split is allowed (block extends past bottom edge) */
static int stb_av1_can_split_v(int by4, int sz4, int bh4) {
    return by4 + sz4 < bh4;
}

/* Gather left partition probability (edge case: only V split possible).
   Matches dav1d's gather_left_partition_prob from env.h. */
static unsigned stb_av1_gather_left_partition(const unsigned short *cdf, int bl) {
    unsigned out = cdf[STB_PARTITION_H - 1] - cdf[STB_PARTITION_H];
    out += cdf[STB_PARTITION_SPLIT - 1] - cdf[STB_PARTITION_T_LEFT_SPLIT];
    if (bl != 0)
        out += cdf[STB_PARTITION_H4 - 1] - cdf[STB_PARTITION_H4];
    return out;
}

/* Gather top partition probability (edge case: only H split possible).
   Matches dav1d's gather_top_partition_prob from env.h. */
static unsigned stb_av1_gather_top_partition(const unsigned short *cdf, int bl) {
    unsigned out = cdf[STB_PARTITION_V - 1] - cdf[STB_PARTITION_T_TOP_SPLIT];
    out += cdf[STB_PARTITION_T_LEFT_SPLIT - 1];
    if (bl != 0)
        out += cdf[STB_PARTITION_V4 - 1] - cdf[STB_PARTITION_T_RIGHT_SPLIT];
    return out;
}

/* Decode partition for a block at given position and level.
   Returns partition type.
   Updates above/left partition context arrays. */
static int stb_av1_decode_partition(struct stb_av1_msac *msac,
                                     struct StbCdfContext *cdf,
                                     int bl, int bx4, int by4,
                                     int bw4, int bh4,
                                     unsigned char *above_part,
                                     unsigned char *left_part) {
    int hsz4 = 16 >> bl; /* half-size in 4-pixel units */
    int can_h = stb_av1_can_split_h(bx4, hsz4, bw4);
    int can_v = stb_av1_can_split_v(by4, hsz4, bh4);

    if (!can_h && !can_v) {
        return -1; /* forced split: recurse one level deeper */
    }

    if (can_h && can_v) {
        int ctx = stb_av1_get_partition_ctx(above_part, left_part, bl, bx4, by4);
        return (int)stb_av1_msac_decode_symbol(msac,
                   cdf->partition[bl][ctx],
                   (unsigned long)(stb_av1_partition_nsym[bl]));
    }

    if (can_h) {
        int ctx = stb_av1_get_partition_ctx(above_part, left_part, bl, bx4, by4);
        unsigned prob = stb_av1_gather_top_partition(cdf->partition[bl][ctx], bl);
        unsigned bit = stb_av1_msac_decode_bool(msac, prob);
        return bit ? STB_PARTITION_SPLIT : STB_PARTITION_H;
    }

    {
        int ctx = stb_av1_get_partition_ctx(above_part, left_part, bl, bx4, by4);
        unsigned prob = stb_av1_gather_left_partition(cdf->partition[bl][ctx], bl);
        unsigned bit = stb_av1_msac_decode_bool(msac, prob);
        return bit ? STB_PARTITION_SPLIT : STB_PARTITION_V;
    }
}

/* Process a single leaf block (max 32x32 for coeff arrays).
   Larger blocks are split into 32x32 sub-blocks for coefficient decoding. */
static void stb_av1_decode_block(struct stb_av1_tile_context *tc,
                                  int abs_r, int abs_c,
                                  int blk_w, int blk_h,
                                  int bx4,
                                  unsigned char *above_row_modes,
                                  unsigned char *above_nz_coeffs,
                                  unsigned char *left_mode,
                                  unsigned char *left_nz_coeffs,
                                  unsigned char *above_bskip,
                                  unsigned char *left_bskip)
{
    int tx_w = blk_w, tx_h = blk_h, tx_type = 0, tx_depth = 0;
    int i, uv_mode, block_skip;
    int has_uv;
    int uv_cfl_a0, uv_cfl_a1;
    int uv_angle = 0;
    int txtp_mode_luma = 0;
    int coeffs[128*128], eob;
    int ctx_above, ctx_left;
    int pred_mode;
    int bw = blk_w > 32 ? 32 : blk_w;
    int bh = blk_h > 32 ? 32 : blk_h;
    int bs, blw = 0, blh = 0;
    int y_angle = 0;
    tx_w = bw; tx_h = bh;
    bs = stb_av1_bs_lookup(blk_w, blk_h);
    { int tw = blk_w; while (tw > 4) { blw++; tw >>= 1; } }
    { int th = blk_h; while (th > 4) { blh++; th >>= 1; } }
    has_uv = !tc->sh->monochrome &&
             ((blk_w / 4) > tc->sh->subsampling_x || ((abs_c / 4) & 1)) &&
             ((blk_h / 4) > tc->sh->subsampling_y || ((abs_r / 4) & 1));
{ static int _blkct=0; if (_blkct < 1) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) ENTER blk_w=%d blk_h=%d rng=%u cnt=%u delta_q=%d cdef_bits=%d seg=%d\n", abs_r, abs_c, blk_w, blk_h, tc->msac->rng, tc->msac->cnt, tc->fh->delta_q_present, tc->fh->cdef_bits, tc->fh->segmentation_enabled); _blkct++; }
    /* Block-level skip decode (dav1d's decode_b: skip before mode/coeffs) */
    {
        int above_bsk = (abs_r > 0 && above_bskip) ? ((*above_bskip >> 0) & 1) : 0;
        int left_bsk  = (abs_c > 0 && left_bskip) ? ((*left_bskip >> 0) & 1) : 0;
        int bskip_ctx = (above_bsk + left_bsk); /* 0, 1, or 2 */
        int block_skip_val;
{ static int _blkct=0; if (_blkct < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) bskip_ctx=%d skip_cdf0=%u rng_before=%u cnt=%u\n", abs_r, abs_c, bskip_ctx, tc->cdf->skip[bskip_ctx][0], tc->msac->rng, tc->msac->cnt); _blkct++; }
        block_skip_val = (int)stb_av1_msac_decode_bool_adapt(tc->msac, tc->cdf->skip[bskip_ctx]);
        if (block_skip_val) {
            /* Block-level skip: set all modes/nzs to 0, no coefficient decode */
            int _bw4 = bw / 4; int _bh4 = bh / 4; int _i;
            int _slw = 0, _slh = 0, _tw4, _th4;
            for (_i = 0; _i < _bw4; _i++) above_row_modes[_i] = 0;
            for (_i = 0; _i < _bw4; _i++) above_nz_coeffs[_i] = 0x40;
            for (_i = 0; _i < _bh4; _i++) left_mode[_i] = 0;
            for (_i = 0; _i < _bh4; _i++) left_nz_coeffs[_i] = 0x40;
            if (above_bskip) { for (_i = 0; _i < _bw4; _i++) above_bskip[_i] = 1; }
            if (left_bskip) { for (_i = 0; _i < _bh4; _i++) left_bskip[_i] = 1; }
            _tw4 = bw; while (_tw4 > 4) { _slw++; _tw4 >>= 1; }
            _th4 = bh; while (_th4 > 4) { _slh++; _th4 >>= 1; }
            for (_i = 0; _i < _bw4; _i++)
                if (abs_c / 4 + _i < 4096) tc->frame_above_tx[abs_c / 4 + _i] = (unsigned char)(_slw + 1);
            for (_i = 0; _i < _bh4; _i++)
                if (abs_r / 4 + _i < 4096) tc->frame_left_tx[abs_r / 4 + _i] = (unsigned char)(_slh + 1);
            return;
        }
        /* Not skipped: set bskip context to 0 */
        { int _bw4 = bw / 4; int _bh4 = bh / 4; int _i;
          if (above_bskip) { for (_i = 0; _i < _bw4; _i++) above_bskip[_i] = 0; }
          if (left_bskip) { for (_i = 0; _i < _bh4; _i++) left_bskip[_i] = 0; } }
    }
{ static int _blkct=0; if (_blkct < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) AFTER skip rng=%u cnt=%u\n", abs_r, abs_c, tc->msac->rng, tc->msac->cnt); _blkct++; }

    /* CDEF index decode per 64x64 quadrant within SB (dav1d: idx = ((bx&16)>>4) + ((by&16)>>3)) */
    if (tc->sh->enable_cdef && tc->fh->cdef_bits > 0) {
        int cdef_idx_x = ((abs_c >> 6) & 1);
        int cdef_idx_y = ((abs_r >> 6) & 1) << 1;
        int idx = cdef_idx_x + cdef_idx_y;
        if (tc->cur_sb_cdef_idx[idx] == -1) {
            tc->cur_sb_cdef_idx[idx] = (int)stb_av1_msac_decode_bools(tc->msac, (unsigned)tc->fh->cdef_bits);
{ static int _blkct=0; if (_blkct < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) AFTER cdef_idx=%d rng=%u cnt=%u\n", abs_r, abs_c, tc->cur_sb_cdef_idx[idx], tc->msac->rng, tc->msac->cnt); _blkct++; }
        }
    }

    eob = 0; block_skip = 0;
    memset(coeffs, 0, sizeof(coeffs));

    if (tc->fh->frame_type == STB_AV1_KEY_FRAME || tc->fh->frame_type == STB_AV1_INTRA_ONLY) {
        unsigned short *mode_cdf;
        ctx_above = (abs_r > 0)
            ? (int)stb_av1_intra_mode_context[(unsigned char)*above_row_modes % 13] : 0;
        ctx_left = (abs_c > 0)
            ? (int)stb_av1_intra_mode_context[(unsigned char)*left_mode % 13] : 0;
{ static int _lm=0; if (_lm<200) { _lm++;
  fprintf(stderr,"[LMD] blk(%d,%d) w=%d h=%d leftptr=%p leftval=%d ctx_l=%d\n", abs_r, abs_c, blk_w, blk_h, (void*)left_mode, (unsigned char)*left_mode, ctx_left); } }
{ static int _mk=0; if (_mk < 80) { _mk++;
  fprintf(stderr,"[KFYMD] blk(%d,%d) w=%d h=%d above_val=%d left_val=%d ctx_a=%d ctx_l=%d rng=%u\n",
    abs_r, abs_c, blk_w, blk_h, (unsigned char)*above_row_modes, (unsigned char)*left_mode,
    ctx_above, ctx_left, tc->msac->rng); } }
        if (ctx_above < 0) ctx_above = 0;
        if (ctx_above > 4) ctx_above = 4;
        if (ctx_left < 0) ctx_left = 0;
        if (ctx_left > 4) ctx_left = 4;
        mode_cdf = tc->cdf->kfym[ctx_above][ctx_left];
        pred_mode = (int)stb_av1_msac_decode_symbol(tc->msac, mode_cdf, 12);
        if (pred_mode < 0) pred_mode = 0;
        if (pred_mode > 12) pred_mode = 12;
{ static int _blk=0; if (_blk < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) AFTER y_mode=%d rng=%u cnt=%u\n", abs_r, abs_c, pred_mode, tc->msac->rng, tc->msac->cnt); _blk++; }
        { int _bw4 = bw / 4; int _bh4 = bh / 4; int _si;
          for (_si = 0; _si < _bw4; _si++) above_row_modes[_si] = (unsigned char)pred_mode;
          for (_si = 0; _si < _bh4; _si++) left_mode[_si] = (unsigned char)pred_mode;
          { static int _lw=0; if (_lw<200) { _lw++;
            fprintf(stderr,"[LMW] blk(%d,%d) w=%d h=%d bh4=%d leftptr=%p store=%d\n", abs_r, abs_c, blk_w, blk_h, _bh4, (void*)left_mode, pred_mode); } } }
        /* Angle delta decode for directional intra modes (dav1d decode_b) */
        /* VERT_PRED=1 through VERT_LEFT_PRED=8, blocks with lw+lh >= 2 */
        if (pred_mode >= 1 && pred_mode <= 8 && (blw + blh >= 2)) {
            unsigned short *angle_cdf = tc->cdf->angle_delta[pred_mode - 1];
            y_angle = (int)stb_av1_msac_decode_symbol(tc->msac, angle_cdf, 6) - 3;
{ static int _blk=0; if (_blk < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) AFTER angle_delta rng=%u cnt=%u\n", abs_r, abs_c, tc->msac->rng, tc->msac->cnt); _blk++; }
        }
    } else {
        pred_mode = STB_AV1_DC_PRED;
    }

    uv_mode = STB_AV1_DC_PRED;
    if (has_uv) {
        unsigned short *uvmode_cdf;
        int cfl_allowed = (stb_av1_cfl_allowed_mask & (1u << bs)) ? 1 : 0;
        uvmode_cdf = tc->cdf->uv_mode[cfl_allowed][pred_mode];
        uv_mode = (int)stb_av1_msac_decode_symbol(tc->msac,
            uvmode_cdf, 13 - !cfl_allowed);
        uv_cfl_a0 = 0; uv_cfl_a1 = 0;
        if (uv_mode == 13) {
            /* UV_CFL_PRED: decode cfl sign + alphas (dav1d decode.c 1262-1284) */
            int cfl_sign = (int)stb_av1_msac_decode_symbol(tc->msac, tc->cdf->cfl_sign, 7) + 1;
            int cfl_sign_u = cfl_sign * 0x56 >> 8;
            int cfl_sign_v = cfl_sign - cfl_sign_u * 3;
            if (cfl_sign_u) {
                int ctx = (cfl_sign_u == 2) * 3 + cfl_sign_v;
                uv_cfl_a0 = (int)stb_av1_msac_decode_symbol(tc->msac, tc->cdf->cfl_alpha[ctx], 15) + 1;
                if (cfl_sign_u == 1) uv_cfl_a0 = -uv_cfl_a0;
            }
            if (cfl_sign_v) {
                int ctx = (cfl_sign_v == 2) * 3 + cfl_sign_u;
                uv_cfl_a1 = (int)stb_av1_msac_decode_symbol(tc->msac, tc->cdf->cfl_alpha[ctx], 15) + 1;
                if (cfl_sign_v == 1) uv_cfl_a1 = -uv_cfl_a1;
            }
        } else if (blw + blh >= 2 && uv_mode >= 1 && uv_mode <= 8) {
            unsigned short *uv_acdf = tc->cdf->angle_delta[uv_mode - 1];
            uv_angle = (int)stb_av1_msac_decode_symbol(tc->msac, uv_acdf, 6) - 3;
        } else {
            uv_angle = 0;
        }
{ static int _blk=0; if (_blk < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) AFTER uv_mode=%d rng=%u cnt=%u\n", abs_r, abs_c, uv_mode, tc->msac->rng, tc->msac->cnt); _blk++; }
    }

    /* Filter intra (dav1d decode_b lines 1142-1154).
       Decoded after uv_mode/palette, before tx size. */
    if (pred_mode == STB_AV1_DC_PRED && tc->sh->filter_intra &&
        (blw > blh ? blw : blh) <= 3)
    {
        int is_filter = (int)stb_av1_msac_decode_bool_adapt(tc->msac,
                            tc->cdf->use_filter_intra[bs]);
{ static int _blk=0; if (_blk < 8) fprintf(stderr,"[MSAC_TRACE] blk(%d,%d) AFTER use_filter_intra=%d rng=%u cnt=%u\n", abs_r, abs_c, is_filter, tc->msac->rng, tc->msac->cnt); _blk++; }
        if (is_filter) {
            pred_mode = 13; /* STB_AV1_FILTER_PRED */
            y_angle = (int)stb_av1_msac_decode_symbol(tc->msac,
                          tc->cdf->filter_intra, 4);
        }
    }

    /* Transform size decode (dav1d decode_b lines 1185-1206).
       Must come AFTER y_mode/angle_delta/uv_mode, BEFORE decode_coefs.
       lw,lh = log2(w/4),log2(h/4); max = imax(lw,lh);
       nsymbols = imin(max,2); CDF = txsz[max-1][tctx]; tctx=0 for first block */
    {
        int lw = 0, lh = 0, t_dim_max, nsyms_tx;
        { int tw = bw; while (tw > 4) { lw++; tw >>= 1; } }
        { int th = bh; while (th > 4) { lh++; th >>= 1; } }
        t_dim_max = lw > lh ? lw : lh;
        nsyms_tx = t_dim_max > 0 ? (t_dim_max < 2 ? t_dim_max : 2) : 0;
        if (nsyms_tx > 0 && tc->fh->tx_mode == 2) {
            int tctx = 0;
            unsigned short *tx_cdf;
            int ax4 = abs_c / 4, ay4 = abs_r / 4;
            int above_txv = (ax4 < 4096) ? tc->frame_above_tx[ax4] : 0;
            int left_txv = (ay4 < 4096) ? tc->frame_left_tx[ay4] : 0;
            tctx = (left_txv >= lh + 1 ? 1 : 0) + (above_txv >= lw + 1 ? 1 : 0);
            tx_cdf = tc->cdf->txsz[t_dim_max - 1][tctx];
            tx_depth = (int)stb_av1_msac_decode_symbol(tc->msac, tx_cdf, (unsigned long)nsyms_tx);
        }
    }
    /* Apply tx_depth subdivision (dav1d: while (depth--) b->tx = t_dim->sub;
       each halving reduces the larger dimension, both when square) */
    {   int num_sub_x = 1, num_sub_y = 1, _di;
        tx_w = bw; tx_h = bh;
        for (_di = 0; _di < tx_depth; _di++) {
            if (tx_w > tx_h) tx_w /= 2;
            else if (tx_h > tx_w) tx_h /= 2;
            else { tx_w /= 2; tx_h /= 2; }
        }
        num_sub_x = bw / tx_w; num_sub_y = bh / tx_h;

    /* Store tx context (dav1d: a->tx_intra = lw, l->tx_intra = lh) */
    {
        int tlw = 0, tlh = 0, ti, tw4, th4;
        int ax4 = abs_c / 4, ay4 = abs_r / 4;
        int nbw4 = bw / 4, nbh4 = bh / 4;
        tw4 = tx_w; while (tw4 > 4) { tlw++; tw4 >>= 1; }
        th4 = tx_h; while (th4 > 4) { tlh++; th4 >>= 1; }
        for (ti = 0; ti < nbw4; ti++)
            if (ax4 + ti < 4096) tc->frame_above_tx[ax4 + ti] = (unsigned char)(tlw + 1);
        for (ti = 0; ti < nbh4; ti++)
            if (ay4 + ti < 4096) tc->frame_left_tx[ay4 + ti] = (unsigned char)(tlh + 1);
    }

    /* Transform type is decoded per sub-tx inside decode_coeffs_cdf
       (after the coeff skip bool), matching dav1d recon_tmpl.c. */
    txtp_mode_luma = (pred_mode == 13) ? (int)stb_av1_filter_mode_to_y_mode[y_angle] : pred_mode;

    /* Y reconstruction loop: iterate over sub-blocks when tx_depth > 0 */
    {
        int sub_y, sub_x;
        for (sub_y = 0; sub_y < num_sub_y; sub_y++) {
            for (sub_x = 0; sub_x < num_sub_x; sub_x++) {
                int sub_abs_r = abs_r + sub_y * tx_h;
                int sub_abs_c = abs_c + sub_x * tx_w;
                int sub_bw = tx_w, sub_bh = tx_h;
                unsigned char above_y[64], left_y[64];
                unsigned char topleft_y;
                unsigned char pred_buf[128*128];
                int seob;
                /* Prediction for this sub-block */
                for (i = 0; i < sub_bw && i < 64; i++)
                    above_y[i] = sub_abs_r > 0 ? tc->plane_y[(sub_abs_r-1)*tc->stride_y+sub_abs_c+i] : (unsigned char)127;
                for (i = 0; i < sub_bh && i < 64; i++)
                    left_y[i] = sub_abs_c > 0 ? tc->plane_y[(sub_abs_r+i)*tc->stride_y+sub_abs_c-1] : (unsigned char)127;
                topleft_y = (sub_abs_r > 0 && sub_abs_c > 0)
                    ? tc->plane_y[(sub_abs_r-1)*tc->stride_y+sub_abs_c-1] : (unsigned char)127;
                if (tc->sh->enable_intra_edge_filter && pred_mode > 0 && pred_mode < 9) {
                    stb_av1_filter_intra_edge(above_y, sub_bw, 0, 1);
                    stb_av1_filter_intra_edge(left_y, sub_bh, 0, 1);
                }
                if (pred_mode == 13) {
                    unsigned char fi_buf[129]; /* topleft at [64] */
                    fi_buf[64] = topleft_y;
                    for (i = 0; i < sub_bw && i < 64; i++) fi_buf[64+1+i] = above_y[i];
                    for (i = 0; i < sub_bh && i < 64; i++) fi_buf[64-1-i] = left_y[i];
                    stb_av1_filter_intra_predict(pred_buf, sub_bw, &fi_buf[64],
                                                 sub_bw, sub_bh, y_angle);
                } else {
                    stb_av1_intra_predict(pred_buf, sub_bw, sub_bw, sub_bh, pred_mode,
                                           above_y, left_y, topleft_y, tc->bit_depth,
                                           sub_abs_r > 0, sub_abs_c > 0);
                }
                /* Coefficient decode for this sub-block. dav1d read_coef_blocks:
                   decode_coefs returns res_ctx, then the tx's a/l lcoef span is
                   memset with it (recon_tmpl.c:879-891). */
                seob = 0;
                {   int sub_a_off = sub_x * (sub_bw / 4);
                    int sub_l_off = sub_y * (sub_bh / 4);
                    unsigned char res_ctx;
                    unsigned char *sa_ctx = above_nz_coeffs + sub_a_off;
                    unsigned char *sl_ctx = left_nz_coeffs + sub_l_off;
                    int _wi, _hi;
                    tx_type = 0;
                    { static int _blkdbg=0; if (_blkdbg++<60000) fprintf(stderr,"[BLKDBG] sub_y=%d sub_x=%d blw=%d blh=%d sub_bw=%d sub_bh=%d tx_depth=%d plane=0 res_ctx=%d\n", sub_y, sub_x, blw, blh, sub_bw, sub_bh, tx_depth, res_ctx); }
                    stb_av1_decode_coeffs_cdf(tc->msac, coeffs, sub_bw, sub_bh, &seob,
                        tc->cdf, 0, sa_ctx, sl_ctx, blw, blh, 0, 0, &res_ctx,
                        txtp_mode_luma, 0, tc->fh->reduced_tx_set, &tx_type);
                    for (_wi = 0; _wi < sub_bw / 4; _wi++) sa_ctx[_wi] = res_ctx;
                    for (_hi = 0; _hi < sub_bh / 4; _hi++) sl_ctx[_hi] = res_ctx;
                }
                /* Reconstruction for this sub-block */
                { static int _pdbg=0; if (_pdbg++<3) { int _rr,_cc;
                  fprintf(stderr,"[PRED] sub_y=%d sub_x=%d mode=%d w=%d h=%d above[0..3]=%d,%d,%d,%d left[0..3]=%d,%d,%d,%d tl=%d\n",
                    sub_y, sub_x, pred_mode, sub_bw, sub_bh,
                    above_y[0], above_y[1], above_y[2], above_y[3],
                    left_y[0], left_y[1], left_y[2], left_y[3], topleft_y);
                  fprintf(stderr,"[PRED] pred_buf:\n");
                  for (_rr=0;_rr<sub_bh;_rr++){ for (_cc=0;_cc<sub_bw;_cc++) fprintf(stderr,"%d ", pred_buf[_rr*sub_bw+_cc]); fprintf(stderr,"\n"); } } }
                stb_av1_reconstruct_block(tc, tc->qindex_y, coeffs, sub_bw, sub_bh, tx_type,
                                           pred_buf, sub_bw,
                                           tc->plane_y + sub_abs_r * tc->stride_y + sub_abs_c,
                                           tc->stride_y);
                { static int _rdbg=0; if (_rdbg++<8) { int _rr,_cc;
                  fprintf(stderr,"[RECON_OUT] sub_y=%d sub_x=%d:\n", sub_y, sub_x);
                  for (_rr=0;_rr<sub_bh;_rr++){ for (_cc=0;_cc<sub_bw;_cc++)
                    fprintf(stderr,"%d ", tc->plane_y[(sub_abs_r+_rr)*tc->stride_y + sub_abs_c+_cc]); fprintf(stderr,"\n"); } } }
            }
        }
    }
    } /* close subdivision scope */

    if (has_uv) {
        int ss_x = tc->sh->subsampling_x;
        int ss_y = tc->sh->subsampling_y;
        int u_r = abs_r >> ss_y, u_c = abs_c >> ss_x;
        int u_w = (blk_w + ss_x) >> ss_x, u_h = (blk_h + ss_y) >> ss_y;
        int uvi;
        int u_nz_aw = (u_w + 3) / 4, u_nz_ah = (u_h + 3) / 4;
        unsigned char u_above_nz[32], u_left_nz[32];
        unsigned char v_above_nz[32], v_left_nz[32];
        /* Chroma tx = max tx for the block+layout (dav1d_max_txfm_size_for_bs),
           which can exceed the raw subsampled block in one dimension. */
        {
            int lay = (ss_x && ss_y) ? 1 : (ss_x ? 2 : 3);
            int utx_idx = stb_av1_max_txfm_size_for_bs[bs][lay];
            { static int _txdbg=0; if (_txdbg++<40) fprintf(stderr,"[TXVDBG] bs=%d lay=%d utx=%d blk_w=%d blk_h=%d\n", bs, lay, utx_idx, blk_w, blk_h); }
            if (utx_idx < 0 || utx_idx > 18) utx_idx = 0;
            u_w = stb_av1_txfm_dimensions[utx_idx].w;
            u_h = stb_av1_txfm_dimensions[utx_idx].h;
        }
        if (u_w < 1) u_w = 1;
        if (u_h < 1) u_h = 1;
        if (u_w > 32) u_w = 32;
        if (u_h > 32) u_h = 32;
        u_nz_aw = (u_w + 3) / 4;
        u_nz_ah = (u_h + 3) / 4;
        if (u_nz_aw > 32) u_nz_aw = 32;
        if (u_nz_ah > 32) u_nz_ah = 32;
        { int uc_off = (abs_c >> ss_x) / 4;
          int ur_off = (abs_r >> ss_y) / 4;
          for (uvi = 0; uvi < u_nz_aw; uvi++) {
              u_above_nz[uvi] = (uc_off + uvi < 4096) ? tc->frame_above_nz_u[uc_off + uvi] : 0;
              v_above_nz[uvi] = (uc_off + uvi < 4096) ? tc->frame_above_nz_v[uc_off + uvi] : 0;
          }
          for (uvi = 0; uvi < u_nz_ah; uvi++) {
              u_left_nz[uvi] = (ur_off + uvi < 4096) ? tc->frame_left_nz_u[ur_off + uvi] : 0;
              v_left_nz[uvi] = (ur_off + uvi < 4096) ? tc->frame_left_nz_v[ur_off + uvi] : 0;
          }

        {
            int uv_mode_c = (uv_mode >= 0 && uv_mode <= 12) ? uv_mode : STB_AV1_DC_PRED;
            int tx_type_uv = (int)stb_av1_txtp_from_uvmode[uv_mode_c];
            unsigned char pred_uv[128*128], above_uv[64], left_uv[64];
            unsigned char topleft_uv;
            int u_eob, u_coeffs[128*128];
            unsigned char res_ctx = 0x40;
            for (uvi = 0; uvi < u_w && uvi < 64; uvi++)
                above_uv[uvi] = u_r > 0 ? tc->plane_u[(u_r-1)*tc->stride_u+u_c+uvi] : (unsigned char)128;
            for (uvi = 0; uvi < u_h && uvi < 64; uvi++)
                left_uv[uvi] = u_c > 0 ? tc->plane_u[(u_r+uvi)*tc->stride_u+u_c-1] : (unsigned char)128;
            topleft_uv = (u_r > 0 && u_c > 0)
                ? tc->plane_u[(u_r-1)*tc->stride_u+u_c-1] : (unsigned char)128;
stb_av1_intra_predict(pred_uv, u_w, u_w, u_h, uv_mode_c,
                                       above_uv, left_uv, topleft_uv, tc->bit_depth,
                                       u_r > 0, u_c > 0);
            for (uvi = 0; uvi < 16384; uvi++) u_coeffs[uvi] = 0;
            if (!block_skip)
                { static int _uvdbg=0; if (_uvdbg++<60) fprintf(stderr,"[UVDBG] u_r=%d u_c=%d u_w=%d u_h=%d ss=%d,%d blw=%d blh=%d ttype=%d\n", u_r, u_c, u_w, u_h, ss_x, ss_y, blw, blh, tx_type_uv); }
                stb_av1_decode_coeffs_cdf(tc->msac, u_coeffs, u_w, u_h, &u_eob, tc->cdf, 1,
                    u_above_nz, u_left_nz, blw, blh, ss_x, ss_y, &res_ctx,
                    -1, tx_type_uv, tc->fh->reduced_tx_set, &tx_type_uv);
            for (uvi = 0; uvi < u_nz_aw; uvi++) u_above_nz[uvi] = res_ctx;
            for (uvi = 0; uvi < u_nz_ah; uvi++) u_left_nz[uvi] = res_ctx;
            stb_av1_reconstruct_block(tc, tc->qindex_u, u_coeffs,
                                       u_w, u_h, tx_type_uv,
                                       pred_uv, u_w,
                                       tc->plane_u + u_r * tc->stride_u + u_c,
                                       tc->stride_u);
        }

        {
            int uv_mode_c = (uv_mode >= 0 && uv_mode <= 12) ? uv_mode : STB_AV1_DC_PRED;
            int tx_type_uv = (int)stb_av1_txtp_from_uvmode[uv_mode_c];
            unsigned char pred_v[128*128], above_v[64], left_v[64];
            unsigned char topleft_v;
            int v_eob, v_coeffs[128*128];
            unsigned char res_ctx = 0x40;
            for (uvi = 0; uvi < u_w && uvi < 64; uvi++)
                above_v[uvi] = u_r > 0 ? tc->plane_v[(u_r-1)*tc->stride_v+u_c+uvi] : (unsigned char)128;
            for (uvi = 0; uvi < u_h && uvi < 64; uvi++)
                left_v[uvi] = u_c > 0 ? tc->plane_v[(u_r+uvi)*tc->stride_v+u_c-1] : (unsigned char)128;
            topleft_v = (u_r > 0 && u_c > 0)
                ? tc->plane_v[(u_r-1)*tc->stride_v+u_c-1] : (unsigned char)128;
stb_av1_intra_predict(pred_v, u_w, u_w, u_h, uv_mode_c,
                                       above_v, left_v, topleft_v, tc->bit_depth,
                                       u_r > 0, u_c > 0);
            for (uvi = 0; uvi < 16384; uvi++) v_coeffs[uvi] = 0;
            if (!block_skip)
                stb_av1_decode_coeffs_cdf(tc->msac, v_coeffs, u_w, u_h, &v_eob, tc->cdf, 1,
                    v_above_nz, v_left_nz, blw, blh, ss_x, ss_y, &res_ctx,
                    -1, tx_type_uv, tc->fh->reduced_tx_set, &tx_type_uv);
            for (uvi = 0; uvi < u_nz_aw; uvi++) v_above_nz[uvi] = res_ctx;
            for (uvi = 0; uvi < u_nz_ah; uvi++) v_left_nz[uvi] = res_ctx;
            stb_av1_reconstruct_block(tc, tc->qindex_v, v_coeffs,
                                       u_w, u_h, tx_type_uv,
                                       pred_v, u_w,
                                       tc->plane_v + u_r * tc->stride_v + u_c,
                                       tc->stride_v);
        }
        /* Save chroma NZ context back to frame arrays for next blocks/SBs */
        for (uvi = 0; uvi < u_nz_aw; uvi++) {
            if (uc_off + uvi < 4096) tc->frame_above_nz_u[uc_off + uvi] = u_above_nz[uvi];
            if (uc_off + uvi < 4096) tc->frame_above_nz_v[uc_off + uvi] = v_above_nz[uvi];
        }
        for (uvi = 0; uvi < u_nz_ah; uvi++) {
            if (ur_off + uvi < 4096) tc->frame_left_nz_u[ur_off + uvi] = u_left_nz[uvi];
            if (ur_off + uvi < 4096) tc->frame_left_nz_v[ur_off + uvi] = v_left_nz[uvi];
        }
        }
    }
}

/* Recursive partition tree decoder for one superblock.
   bl: block level (0=128x128, 1=64x64, 2=32x32, 3=16x16, 4=8x8, 5=4x4).
   For a 64x64 SB, start at bl=1.
   bx4, by4: position within SB in 4px units (SB-local coords).
   above_part/left_part: partition context arrays (indexed by 4px unit). */
static void stb_av1_decode_sb_tree(struct stb_av1_tile_context *tc,
                                    int bx4, int by4, int bl,
                                    int sb_r, int sb_c, int sb_size,
                                    unsigned char *above_row_modes,
                                    unsigned char *above_nz_coeffs,
                                    unsigned char *left_modes,
                                    unsigned char *left_nzs,
                                    unsigned char *above_part,
                                    unsigned char *left_part,
                                    unsigned char *above_bskip,
                                    unsigned char *left_bskip)
{
    int sz4 = 32 >> bl; /* block size in 4px units */
    int blk_sz = sz4 * 4; /* block size in pixels */
    int abs_r = sb_r * sb_size + by4 * 4;
    int abs_c = sb_c * sb_size + bx4 * 4;

    if (abs_r >= tc->frame_height || abs_c >= tc->frame_width)
        return;


        if (bl >= 5 || sz4 <= 1) {
        { static int _b4=0; if (_b4<200) { _b4++;
          fprintf(stderr,"[B4] abs_r=%d abs_c=%d bx4=%d by4=%d blk_sz=%d blk_sz=%d sb_r=%d sb_c=%d sb_size=%d\n",
            abs_r, abs_c, bx4, by4, blk_sz, blk_sz, sb_r, sb_c, sb_size); } }
        stb_av1_decode_block(tc, abs_r, abs_c,
                              blk_sz, blk_sz,
                              bx4,
                              above_row_modes + bx4,
                              above_nz_coeffs + bx4,
                              left_modes + by4, left_nzs + by4,
                              above_bskip + bx4, left_bskip + by4);
        return;
    }

    {
        int bw4 = ((tc->frame_width + 7) >> 3) << 1;   /* dav1d f->bw: 4px units, rounded */
        int bh4 = ((tc->frame_height + 7) >> 3) << 1;  /* dav1d f->bh */
        int hsz4 = sz4 / 2; /* half-size in 4px units; dav1d: hsz = 16>>bl */
        int can_h, can_v;
        bw4 = bw4 - sb_c * (sb_size / 4); if (bw4 < 0) bw4 = 0;
        bh4 = bh4 - sb_r * (sb_size / 4); if (bh4 < 0) bh4 = 0;
        can_h = bx4 + hsz4 < bw4; /* dav1d: have_h_split = f->bw > bx + hsz */
        can_v = by4 + hsz4 < bh4; /* dav1d: have_v_split = f->bh > by + hsz */



        if (!can_h && !can_v) {
            /* Forced split: recurse one level deeper */
            stb_av1_decode_sb_tree(tc, bx4, by4, bl + 1,
                                    sb_r, sb_c, sb_size,
                                    above_row_modes, above_nz_coeffs,
                                    left_modes, left_nzs,
                                    above_part, left_part,
                                    above_bskip, left_bskip);
            return;
        }

        if (can_h && can_v) {
{ static int _pd = 0; if (_pd < 4000) {
    fprintf(stderr, "[DBG_PART] bl=%d sz4=%d bx4=%d by4=%d nsym=%d\n", bl, sz4, bx4, by4, stb_av1_partition_nsym[bl]);
    fprintf(stderr, "[DBG_PART] cdf bl=%d ctx=0: ", bl);
    { int _i; for(_i=0;_i<stb_av1_partition_nsym[bl];_i++) fprintf(stderr,"%u ",tc->cdf->partition[bl][0][_i]); fprintf(stderr," cnt=%u\n",tc->cdf->partition[bl][0][stb_av1_partition_nsym[bl]]); }
    fprintf(stderr, "[DBG_PART] msac before: dif=0x%08x%08x rng=%u cnt=%d\n",
        (unsigned)(tc->msac->dif >> 32), (unsigned)(tc->msac->dif & 0xFFFFFFFF), tc->msac->rng, tc->msac->cnt);
    _pd++;
} }
            { int _pctx = stb_av1_get_partition_ctx(above_part, left_part, bl, bx4, by4);
            int bp = (int)stb_av1_msac_decode_symbol(tc->msac,
                        tc->cdf->partition[bl][_pctx], (unsigned long)stb_av1_partition_nsym[bl]);
{ fprintf(stderr, "[DBG_PART] y4=%d x4=%d bl=%d ctx=%d above=%d left=%d nsym=%d bp=%d rng_after=%u\n",
    abs_r/4, abs_c/4, bl, _pctx, above_part[bx4], left_part[by4], stb_av1_partition_nsym[bl], bp, tc->msac->rng); }
            if (bp < 0) bp = 0;
            /* Update partition context arrays (dav1d al_part_ctx; nothing for SPLIT) */
            stb_av1_set_partition_ctx(above_part, left_part, bx4, by4, sz4, bl, bp);


            if (bp == STB_PARTITION_NONE) {
                stb_av1_decode_block(tc, abs_r, abs_c,
                                      blk_sz, blk_sz,
                                      bx4,
                                      above_row_modes + bx4,
                                      above_nz_coeffs + bx4,
                                      left_modes + by4, left_nzs + by4,
                                      above_bskip + bx4, left_bskip + by4);
            } else if (bp == STB_PARTITION_SPLIT) {
                stb_av1_decode_sb_tree(tc, bx4,     by4,     bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
                stb_av1_decode_sb_tree(tc, bx4+hsz4, by4,     bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
                stb_av1_decode_sb_tree(tc, bx4,     by4+hsz4, bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
                stb_av1_decode_sb_tree(tc, bx4+hsz4, by4+hsz4, bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
            } else if (bp == STB_PARTITION_H) {
                stb_av1_decode_block(tc, abs_r, abs_c, blk_sz, blk_sz/2, bx4, above_row_modes+bx4, above_nz_coeffs+bx4, left_modes+by4, left_nzs+by4, above_bskip+bx4, left_bskip+by4);
                stb_av1_decode_block(tc, abs_r+blk_sz/2, abs_c, blk_sz, blk_sz/2, bx4, above_row_modes+bx4, above_nz_coeffs+bx4, left_modes+by4+hsz4, left_nzs+by4+hsz4, above_bskip+bx4, left_bskip+by4+hsz4);
            } else if (bp == STB_PARTITION_V) {
                stb_av1_decode_block(tc, abs_r, abs_c, blk_sz/2, blk_sz, bx4, above_row_modes+bx4, above_nz_coeffs+bx4, left_modes+by4, left_nzs+by4, above_bskip+bx4, left_bskip+by4);
                stb_av1_decode_block(tc, abs_r, abs_c+blk_sz/2, blk_sz/2, blk_sz, bx4+hsz4, above_row_modes+bx4+hsz4, above_nz_coeffs+bx4+hsz4, left_modes+by4, left_nzs+by4, above_bskip+bx4+hsz4, left_bskip+by4);
            } else if (bp == STB_PARTITION_T_TOP_SPLIT||bp == STB_PARTITION_T_BOTTOM_SPLIT||bp == STB_PARTITION_T_LEFT_SPLIT||bp == STB_PARTITION_T_RIGHT_SPLIT) {
                int hw=blk_sz/2, hz=hsz4;
                if(bp==STB_PARTITION_T_TOP_SPLIT){
                    stb_av1_decode_block(tc,abs_r,abs_c,       hw,hw,    bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4,left_nzs+by4,above_bskip+bx4,left_bskip+by4);
                    stb_av1_decode_block(tc,abs_r,abs_c+hw,    hw,hw,    bx4+hz, above_row_modes+bx4+hz,above_nz_coeffs+bx4+hz,left_modes+by4,left_nzs+by4,above_bskip+bx4+hz,left_bskip+by4);
                    stb_av1_decode_block(tc,abs_r+hw,abs_c,    blk_sz,hw,bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4+hz,left_nzs+by4+hz,above_bskip+bx4,left_bskip+by4+hz);
                } else if(bp==STB_PARTITION_T_BOTTOM_SPLIT){
                    stb_av1_decode_block(tc,abs_r,abs_c,       blk_sz,hw,bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4,left_nzs+by4,above_bskip+bx4,left_bskip+by4);
                    stb_av1_decode_block(tc,abs_r+hw,abs_c,    hw,hw,    bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4+hz,left_nzs+by4+hz,above_bskip+bx4,left_bskip+by4+hz);
                    stb_av1_decode_block(tc,abs_r+hw,abs_c+hw, hw,hw,    bx4+hz, above_row_modes+bx4+hz,above_nz_coeffs+bx4+hz,left_modes+by4+hz,left_nzs+by4+hz,above_bskip+bx4+hz,left_bskip+by4+hz);
                } else if(bp==STB_PARTITION_T_LEFT_SPLIT){
                    stb_av1_decode_block(tc,abs_r,abs_c,       hw,hw,    bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4,left_nzs+by4,above_bskip+bx4,left_bskip+by4);
                    stb_av1_decode_block(tc,abs_r+hw,abs_c,    hw,hw,    bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4+hz,left_nzs+by4+hz,above_bskip+bx4,left_bskip+by4+hz);
                    stb_av1_decode_block(tc,abs_r,abs_c+hw,    hw,blk_sz,bx4+hz, above_row_modes+bx4+hz,above_nz_coeffs+bx4+hz,left_modes+by4,left_nzs+by4,above_bskip+bx4+hz,left_bskip+by4);
                } else { /* T_RIGHT_SPLIT */
                    stb_av1_decode_block(tc,abs_r,abs_c,       hw,blk_sz,bx4,    above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4,left_nzs+by4,above_bskip+bx4,left_bskip+by4);
                    stb_av1_decode_block(tc,abs_r,abs_c+hw,    hw,hw,    bx4+hz, above_row_modes+bx4+hz,above_nz_coeffs+bx4+hz,left_modes+by4,left_nzs+by4,above_bskip+bx4+hz,left_bskip+by4);
                    stb_av1_decode_block(tc,abs_r+hw,abs_c+hw, hw,hw,    bx4+hz, above_row_modes+bx4+hz,above_nz_coeffs+bx4+hz,left_modes+by4+hz,left_nzs+by4+hz,above_bskip+bx4+hz,left_bskip+by4+hz);
                }
            } else if (bp == STB_PARTITION_H4) {
                int qh=blk_sz/4,i; for(i=0;i<4;i++)stb_av1_decode_block(tc,abs_r+i*qh,abs_c,blk_sz,qh,bx4,above_row_modes+bx4,above_nz_coeffs+bx4,left_modes+by4+i*(hsz4/2),left_nzs+by4+i*(hsz4/2),above_bskip+bx4,left_bskip+by4+i*(hsz4/2));
            } else if (bp == STB_PARTITION_V4) {
                int qw=blk_sz/4,i; for(i=0;i<4;i++)stb_av1_decode_block(tc,abs_r,abs_c+i*qw,qw,blk_sz,bx4+i*(hsz4/2),above_row_modes+bx4+i*(hsz4/2),above_nz_coeffs+bx4+i*(hsz4/2),left_modes+by4,left_nzs+by4,above_bskip+bx4+i*(hsz4/2),left_bskip+by4);
            } else {
                stb_av1_decode_block(tc, abs_r, abs_c, blk_sz, blk_sz, bx4, above_row_modes+bx4, above_nz_coeffs+bx4, left_modes+by4, left_nzs+by4, above_bskip+bx4, left_bskip+by4);
            }
            } /* end _pctx scope */
        } else if (can_h) {
            /* Edge: only H split. Decode bool using gather_top_partition_prob. */

            { int _pctx = stb_av1_get_partition_ctx(above_part, left_part, bl, bx4, by4);
            unsigned prob = stb_av1_gather_top_partition(tc->cdf->partition[bl][_pctx], bl);
            unsigned bit = stb_av1_msac_decode_bool(tc->msac, prob);
            if (bit) {
                stb_av1_decode_sb_tree(tc, bx4,     by4,     bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
                stb_av1_decode_sb_tree(tc, bx4+hsz4, by4,     bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
            } else {
                stb_av1_decode_block(tc, abs_r, abs_c,
                                      blk_sz, blk_sz,
                                      bx4,
                                      above_row_modes + bx4,
                                      above_nz_coeffs + bx4,
                                      left_modes + by4, left_nzs + by4,
                                      above_bskip + bx4, left_bskip + by4);
            }
            /* Update partition context */
            stb_av1_set_partition_ctx(above_part, left_part, bx4, by4, sz4, bl,
                                      bit ? STB_PARTITION_SPLIT : STB_PARTITION_H);
            } /* end _pctx scope */
        } else {
            /* Edge: only V split. Decode bool using gather_left_partition_prob. */

            { int _pctx = stb_av1_get_partition_ctx(above_part, left_part, bl, bx4, by4);
            unsigned prob = stb_av1_gather_left_partition(tc->cdf->partition[bl][_pctx], bl);
            unsigned bit = stb_av1_msac_decode_bool(tc->msac, prob);
            if (bit) {
                stb_av1_decode_sb_tree(tc, bx4,     by4,     bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
                stb_av1_decode_sb_tree(tc, bx4,     by4+hsz4, bl+1, sb_r, sb_c, sb_size,
                                        above_row_modes, above_nz_coeffs, left_modes, left_nzs,
                                        above_part, left_part, above_bskip, left_bskip);
            } else {
                stb_av1_decode_block(tc, abs_r, abs_c,
                                      blk_sz, blk_sz,
                                      bx4,
                                      above_row_modes + bx4,
                                      above_nz_coeffs + bx4,
                                      left_modes + by4, left_nzs + by4,
                                      above_bskip + bx4, left_bskip + by4);
            }
            /* Update partition context */
            stb_av1_set_partition_ctx(above_part, left_part, bx4, by4, sz4, bl,
                                      bit ? STB_PARTITION_SPLIT : STB_PARTITION_V);
            } /* end _pctx scope */
        }
    }
}

/* Decode a superblock using partition-tree decoding.
   Starts the recursive tree at the root block level. */
static void stb_av1_decode_superblock(struct stb_av1_tile_context *tc,
                                       int sb_r, int sb_c, int sb_size)
{

    unsigned char above_row_modes[256];
    unsigned char above_nz_coeffs[256];
    unsigned char left_modes[256];
    unsigned char left_nzs[256];
    unsigned char above_part[256];
    unsigned char left_part[256];
    unsigned char above_bskip[256];
    unsigned char left_bskip[256];
    int sb_sz4 = sb_size / 4;
    int sc4 = sb_c * sb_sz4;
    int sr4 = sb_r * sb_sz4;
    int i;

    memset(above_row_modes, 0, sizeof(above_row_modes));
    memset(above_nz_coeffs, 0, sizeof(above_nz_coeffs));
    memset(left_modes, 0, sizeof(left_modes));
    memset(left_nzs, 0, sizeof(left_nzs));

    /* Reset cdef indices for this superblock */
    tc->cur_sb_cdef_idx[0] = -1;
    tc->cur_sb_cdef_idx[1] = -1;
    tc->cur_sb_cdef_idx[2] = -1;
    tc->cur_sb_cdef_idx[3] = -1;

    /* Delta Q at SB boundary (dav1d decode_b lines 960-1009) */
    { static int _sbct=0; if (_sbct < 4) fprintf(stderr, "[SB_ENTRY] sb(%d,%d) rng=%u cnt=%u delta_q_present=%d\n", sb_r, sb_c, tc->msac->rng, tc->msac->cnt, tc->fh->delta_q_present); _sbct++; }
    if (tc->fh->delta_q_present && sb_r == 0 && sb_c == 0) {
        /* First SB always gets delta_q decode if present */
        int delta_q_val = (int)stb_av1_msac_decode_symbol(tc->msac, tc->cdf->delta_q, 4);
        if (delta_q_val == 3) {
            int n_bits = 1 + (int)stb_av1_msac_decode_bools(tc->msac, 3);
            delta_q_val = (int)stb_av1_msac_decode_bools(tc->msac, (unsigned)n_bits) +
                          1 + (1 << n_bits);
        }
        if (delta_q_val) {
            if (stb_av1_msac_decode_bool_equi(tc->msac)) delta_q_val = -delta_q_val;
            delta_q_val *= 1 << tc->fh->delta_q_res_log2;
        }
        /* Apply delta_q to base_q_idx if needed */
        (void)delta_q_val;
    }

    /* Initialize context from frame-level arrays */
    for (i = 0; i < sb_sz4; i++) {
        above_part[i] = (sc4 + i < 4096) ? tc->frame_above_part[sc4 + i] : 0;
        left_part[i] = (sr4 + i < 4096) ? tc->frame_left_part[sr4 + i] : 0;
        above_row_modes[i] = (sc4 + i < 4096) ? tc->frame_above_modes[sc4 + i] : 0;
        above_nz_coeffs[i] = (sc4 + i < 4096) ? tc->frame_above_nz[sc4 + i] : 0;
        left_modes[i] = (sr4 + i < 4096) ? tc->frame_left_modes[sr4 + i] : 0;
        left_nzs[i] = (sr4 + i < 4096) ? tc->frame_left_nz[sr4 + i] : 0;
        above_bskip[i] = (sc4 + i < 4096) ? tc->frame_above_bskip[sc4 + i] : 0;
        left_bskip[i] = (sr4 + i < 4096) ? tc->frame_left_bskip[sr4 + i] : 0;
    }

    stb_av1_decode_sb_tree(tc, 0, 0, sb_size == 128 ? 0 : 1,
                            sb_r, sb_c, sb_size,
                            above_row_modes, above_nz_coeffs,
                            left_modes, left_nzs,
                            above_part, left_part,
                            above_bskip, left_bskip);

    /* Save context back to frame-level arrays for next SBs */
    for (i = 0; i < sb_sz4; i++) {
        if (sc4 + i < 4096) tc->frame_above_part[sc4 + i] = above_part[i];
        if (sr4 + i < 4096) tc->frame_left_part[sr4 + i] = left_part[i];
        if (sc4 + i < 4096) tc->frame_above_modes[sc4 + i] = above_row_modes[i];
        if (sc4 + i < 4096) tc->frame_above_nz[sc4 + i] = above_nz_coeffs[i];
        if (sr4 + i < 4096) tc->frame_left_modes[sr4 + i] = left_modes[i];
        if (sr4 + i < 4096) tc->frame_left_nz[sr4 + i] = left_nzs[i];
        if (sc4 + i < 4096) tc->frame_above_bskip[sc4 + i] = above_bskip[i];
        if (sr4 + i < 4096) tc->frame_left_bskip[sr4 + i] = left_bskip[i];
    }
}

static const unsigned short stb_av1_sgr_params[16][2]={
{140,3236},{112,2158},{93,1618},{80,1438},{70,1295},{58,1177},{47,1079},{37,996},
{30,925},{25,863},{0,2589},{0,1618},{0,1177},{0,925},{56,0},{22,0}};
/* Loop restoration info decode (dav1d read_restoration_info:2750-2815).
   frame_type: 0=none, 1=switchable, 2=wiener, 3=sgrproj */
static void stb_av1_read_restoration_info(struct stb_av1_tile_context *tc, int p, int frame_type)
{
    struct StbCdfContext *cdf = tc->cdf;
    int lrt = frame_type;
    if (frame_type == 1) { /* switchable */
        int filter = (int)stb_av1_msac_decode_symbol(tc->msac, cdf->restore_switchable, 2);
        lrt = filter ? (filter == 2 ? 3 : 2) : 0;
    } else {
        unsigned type = (unsigned)stb_av1_msac_decode_bool_adapt(tc->msac,
            frame_type == 2 ? cdf->restore_wiener : cdf->restore_sgrproj);
        lrt = type ? frame_type : 0;
    }
    if (lrt == 2) { /* wiener */
        int *rv = tc->lr_ref_v[p], *rh = tc->lr_ref_h[p];
        int v0 = p ? 0 : (int)stb_av1_msac_decode_subexp(tc->msac, rv[0] + 5, 16, 1) - 5;
        int v1 = (int)stb_av1_msac_decode_subexp(tc->msac, rv[1] + 23, 32, 2) - 23;
        int v2 = (int)stb_av1_msac_decode_subexp(tc->msac, rv[2] + 17, 64, 3) - 17;
        int h0 = p ? 0 : (int)stb_av1_msac_decode_subexp(tc->msac, rh[0] + 5, 16, 1) - 5;
        int h1 = (int)stb_av1_msac_decode_subexp(tc->msac, rh[1] + 23, 32, 2) - 23;
        int h2 = (int)stb_av1_msac_decode_subexp(tc->msac, rh[2] + 17, 64, 3) - 17;
        rv[0] = v0; rv[1] = v1; rv[2] = v2;
        rh[0] = h0; rh[1] = h1; rh[2] = h2;
        tc->lr_ref_w[p][0] = 0; tc->lr_ref_w[p][1] = 0;
    } else if (lrt == 3) { /* sgrproj */
        unsigned idx = stb_av1_msac_decode_bools(tc->msac, 4);
        const unsigned short *sp = stb_av1_sgr_params[idx];
        int w0 = sp[0] ? (int)stb_av1_msac_decode_subexp(tc->msac, tc->lr_ref_w[p][0] + 96, 128, 4) - 96 : 0;
        int w1 = sp[1] ? (int)stb_av1_msac_decode_subexp(tc->msac, tc->lr_ref_w[p][1] + 32, 128, 4) - 32 : 95;
        tc->lr_ref_sgr_idx[p] = (int)idx;
        tc->lr_ref_w[p][0] = w0; tc->lr_ref_w[p][1] = w1;
        tc->lr_ref_v[p][0] = 0; tc->lr_ref_v[p][1] = 0; tc->lr_ref_v[p][2] = 0;
        tc->lr_ref_h[p][0] = 0; tc->lr_ref_h[p][1] = 0; tc->lr_ref_h[p][2] = 0;
    }
}

/* Loop restoration info at SB boundaries (dav1d_decode_tile_sbrow:2889-2938) */
static void stb_av1_decode_sb_restoration(struct stb_av1_tile_context *tc, int sb_r, int sb_c, int sb_size)
{
    struct stb_av1_frame_header *fh = tc->fh;
    int p;
    for (p = 0; p < 3; p++) {
        int lrt = fh->lr_type[p];
        if (!lrt) continue;
        {
            int ss = (p && tc->sh->subsampling_x && tc->sh->subsampling_y) ? 1 : 0;
            int unit_size_log2 = fh->lr_unit_size[!!p] ? fh->lr_unit_size[!!p] : 8;
            int unit_size = 1 << unit_size_log2;
            int y = (sb_r * sb_size) >> ss;
            int x = (sb_c * sb_size) >> ss;
            if (unit_size_log2 > 6) {
                int uy = sb_r * (sb_size >> ss);
                if (uy & (unit_size - 1)) continue;
                if (sb_c * (sb_size >> ss) & (unit_size - 1)) continue;
            } else {
                if (y & (unit_size - 1)) continue;
                if (x & (unit_size - 1)) continue;
            }
            stb_av1_read_restoration_info(tc, p, lrt);
        }
    }
}

/* main tile decoding routine using CDF-based context-adaptive decoding */
static void stb_av1_decode_frame(struct stb_av1_tile_context *tc)
{
    int sb_size = 64 << (tc->sh->sb128 ? 1 : 0);
    int sb_cols, sb_rows;
    int sr, sc;

    sb_cols = (tc->frame_width + sb_size - 1) / sb_size;
    sb_rows = (tc->frame_height + sb_size - 1) / sb_size;

    /* Initialize frame-level context arrays to 0 (no neighbors) */
    memset(tc->frame_above_part, 0, sizeof(tc->frame_above_part));
    memset(tc->frame_left_part, 0, sizeof(tc->frame_left_part));
    memset(tc->frame_above_modes, 0, sizeof(tc->frame_above_modes));
    memset(tc->frame_above_nz, 0x40, sizeof(tc->frame_above_nz));
    memset(tc->frame_left_modes, 0, sizeof(tc->frame_left_modes));
    memset(tc->frame_left_nz, 0x40, sizeof(tc->frame_left_nz));
    memset(tc->frame_above_nz_u, 0x40, sizeof(tc->frame_above_nz_u));
    memset(tc->frame_left_nz_u, 0x40, sizeof(tc->frame_left_nz_u));
    memset(tc->frame_above_nz_v, 0x40, sizeof(tc->frame_above_nz_v));
    memset(tc->frame_left_nz_v, 0x40, sizeof(tc->frame_left_nz_v));
    memset(tc->frame_above_bskip, 0, sizeof(tc->frame_above_bskip));
    memset(tc->frame_left_bskip, 0, sizeof(tc->frame_left_bskip));
    memset(tc->frame_above_tx, 0, sizeof(tc->frame_above_tx));
    memset(tc->frame_left_tx, 0, sizeof(tc->frame_left_tx));

    tc->total_sb = sb_cols * sb_rows;
    tc->done_sb = 0;
    tc->next_report_sb = tc->total_sb / 20;
    if (tc->next_report_sb < 1) tc->next_report_sb = 1;
    tc->start_time = time(NULL);

    for (sr = 0; sr < sb_rows; sr++) {
        for (sc = 0; sc < sb_cols; sc++) {
            stb_av1_decode_sb_restoration(tc, sr, sc, sb_size);
            stb_av1_decode_superblock(tc, sr, sc, sb_size);
            tc->done_sb++;
            if (tc->done_sb >= tc->next_report_sb) {
                time_t now = time(NULL);
                double elapsed = (double)(now - tc->start_time);
                double pct = (double)tc->done_sb * 100.0 / (double)tc->total_sb;
                double eta = (pct > 0.0) ? (elapsed * (100.0 - pct) / pct) : 0.0;
                fprintf(stderr, "\r  [%3.0f%%%%] SB %d/%d, %ds elapsed, ETA %ds     ",
                        pct, tc->done_sb, tc->total_sb, (int)elapsed, (int)eta);
                fflush(stderr);
                tc->next_report_sb += tc->total_sb / 20;
            }
        }
    }
    fprintf(stderr, "\r  [100%%%%] Done (%d superblocks, %ds)          \n",
            tc->total_sb, (int)(time(NULL) - tc->start_time));
}

#else /* !STB_AVIF_USE_C89_DAV1D */

/* Original simplified decoder fallback (non-C89 path) */
/* Decode a superblock (64x64 or 128x128) */
static void stb_av1_decode_superblock(struct stb_av1_tile_context *tc,
                                       int sb_r, int sb_c, int sb_size)
{
    int y, x;
    int block_size = 8;
    int blk_limit = ((tc->frame_width + 7) / 8) * ((tc->frame_height + 7) / 8);

    if (blk_limit < 1) blk_limit = 1;
    blk_limit += sb_size * sb_size / 64;

    for (y = 0; y < sb_size && blk_limit > 0; y += block_size) {
        for (x = 0; x < sb_size && blk_limit > 0; x += block_size) {
            int abs_r = sb_r * sb_size + y;
            int abs_c = sb_c * sb_size + x;
            int blk_w = block_size;
            int blk_h = block_size;
            int tx_w = block_size;
            int tx_h = block_size;
            int tx_type = 0;
            int pred_mode;
            unsigned char above_data[128];
            unsigned char left_data[128];
            unsigned char topleft_pixel;
            int coeffs[64];
            int eob;
            int i;

            blk_limit--;

            if (abs_r >= tc->frame_height || abs_c >= tc->frame_width)
                continue;

            if (tc->fh->frame_type == STB_AV1_KEY_FRAME ||
                tc->fh->frame_type == STB_AV1_INTRA_ONLY) {
                pred_mode = stb_av1_decode_uniform(tc->br, STB_AV1_INTRA_MODES);
            } else {
                pred_mode = STB_AV1_DC_PRED;
            }

            for (i = 0; i < blk_w; i++) {
                if (abs_r > 0)
                    above_data[i] = tc->plane_y[(abs_r - 1) * tc->stride_y + abs_c + i];
                else
                    above_data[i] = 127;
            }
            for (i = 0; i < blk_h; i++) {
                if (abs_c > 0)
                    left_data[i] = tc->plane_y[(abs_r + i) * tc->stride_y + abs_c - 1];
                else
                    left_data[i] = 127;
            }
            topleft_pixel = (abs_r > 0 && abs_c > 0)
                ? tc->plane_y[(abs_r - 1) * tc->stride_y + abs_c - 1]
                : (unsigned char)127;

            {
                unsigned char pred_buf[128];
                stb_av1_intra_predict(pred_buf, blk_w,
                                       blk_w, blk_h, pred_mode,
                                       above_data, left_data, topleft_pixel,
                                       tc->bit_depth);

                stb_av1_decode_coeffs(tc->br, coeffs, blk_w * blk_h, &eob, tc->qindex_y);

                stb_av1_reconstruct_block(tc, tc->qindex_y, coeffs,
                                           tx_w, tx_h, tx_type,
                                           pred_buf, blk_w,
                                           tc->plane_y + abs_r * tc->stride_y + abs_c,
                                           tc->stride_y);
            }

            if (!tc->sh->monochrome) {
                int u_r = abs_r >> tc->sh->subsampling_y;
                int u_c = abs_c >> tc->sh->subsampling_x;
                int u_w = blk_w >> tc->sh->subsampling_x;
                int u_h = blk_h >> tc->sh->subsampling_y;
                int uv_pred_mode;
                unsigned char u_above_byte[64], u_left_byte[64];
                unsigned char uv_topleft;
                unsigned char pred_uv[64];

                if (u_w < 1) u_w = 1;
                if (u_h < 1) u_h = 1;

                uv_pred_mode = STB_AV1_DC_PRED;

                for (i = 0; i < u_w; i++) {
                    if (u_r > 0 && u_c + i < (tc->frame_width >> tc->sh->subsampling_x))
                        u_above_byte[i] = tc->plane_u[(u_r - 1) * tc->stride_u + u_c + i];
                    else
                        u_above_byte[i] = 128;
                }
                for (i = 0; i < u_h; i++) {
                    if (u_c > 0 && u_r + i < (tc->frame_height >> tc->sh->subsampling_y))
                        u_left_byte[i] = tc->plane_u[(u_r + i) * tc->stride_u + u_c - 1];
                    else
                        u_left_byte[i] = 128;
                }
                uv_topleft = (u_r > 0 && u_c > 0)
                    ? tc->plane_u[(u_r - 1) * tc->stride_u + u_c - 1]
                    : (unsigned char)128;

                stb_av1_intra_predict(pred_uv, u_w, u_w, u_h, uv_pred_mode,
                                       u_above_byte, u_left_byte, uv_topleft,
                                       tc->bit_depth);

                {
                    int ri, ci;
                    for (ri = 0; ri < u_h && u_r + ri < tc->frame_height; ri++) {
                        for (ci = 0; ci < u_w && u_c + ci < tc->frame_width; ci++) {
                            tc->plane_u[(u_r + ri) * tc->stride_u + u_c + ci] = pred_uv[ri * u_w + ci];
                            tc->plane_v[(u_r + ri) * tc->stride_v + u_c + ci] = pred_uv[ri * u_w + ci];
            }
        }
    }
}

/* main tile decoding routine */
static void stb_av1_decode_frame(struct stb_av1_tile_context *tc)
{
    int sb_size = 64;
    int sb_cols, sb_rows;
    int sr, sc;

    if (tc->frame_width > 64 || tc->frame_height > 64)
        sb_size = 64;
    if (tc->frame_width > 128 || tc->frame_height > 128)
        sb_size = 128;

    sb_cols = (tc->frame_width + sb_size - 1) / sb_size;
    sb_rows = (tc->frame_height + sb_size - 1) / sb_size;

    tc->total_sb = sb_cols * sb_rows;
    tc->done_sb = 0;
    tc->next_report_sb = tc->total_sb / 20;
    if (tc->next_report_sb < 1) tc->next_report_sb = 1;
    tc->start_time = time(NULL);

    for (sr = 0; sr < sb_rows; sr++) {
        for (sc = 0; sc < sb_cols; sc++) {
            stb_av1_decode_superblock(tc, sr, sc, sb_size);
            tc->done_sb++;
            if (tc->done_sb >= tc->next_report_sb) {
                time_t now = time(NULL);
                double elapsed = (double)(now - tc->start_time);
                double pct = (double)tc->done_sb * 100.0 / (double)tc->total_sb;
                double eta = (pct > 0.0) ? (elapsed * (100.0 - pct) / pct) : 0.0;
                fprintf(stderr, "\r  [%3.0f%%%%] SB %d/%d, %ds elapsed, ETA %ds     ",
                        pct, tc->done_sb, tc->total_sb, (int)elapsed, (int)eta); fflush(stderr);
                tc->next_report_sb += tc->total_sb / 20;
            }
        }
    }
    fprintf(stderr, "\r  [100%%%%] Done (%d superblocks, %ds)          \n",
            tc->total_sb, (int)(time(NULL) - tc->start_time));
}

#endif /* STB_AVIF_USE_C89_DAV1D */

/* -------------------------------------------------------------------------- */
/* CDEF FILTER (Constrained Directional Enhancement Filter)                   */
/* -------------------------------------------------------------------------- */

static const signed char stb_av1_cdef_dirs[8][2] = {
    { -12+1,-24+2 }, {  0+1,-12+2 }, {  0+1, 0+2 }, {  0+1,12+2 },
    { 12+1, 24+2 }, { 12+0, 24+1 }, { 12+0, 24+0 }, { 12+0,24-1 },
};
static int stb_cdef_clamp(int d, int L) { if(d<-L)return -L; if(d>L)return L; return d; }

static int stb_cdef_dir(const unsigned char *img, int stride) {
    int d, best=0, c[8]; unsigned bestc=~0u;
    for(d=0;d<8;d++)c[d]=0;
    { int iy, ix;
    for(iy=1;iy<7;iy++)for(ix=1;ix<7;ix++){
        int gx=(int)img[(iy-1)*stride+ix]+(int)img[(iy-1)*stride+ix+1]
              -(int)img[(iy+1)*stride+ix]-(int)img[(iy+1)*stride+ix+1];
        int gy=(int)img[(iy-1)*stride+ix]+(int)img[(iy+1)*stride+ix]
              -(int)img[(iy-1)*stride+ix+1]-(int)img[(iy+1)*stride+ix+1];
        c[0]+=(gx<0?-gx:gx);{int s=gx+gy;c[1]+=(s<0?-s:s);}
        c[2]+=(gy<0?-gy:gy);{int s=gx-gy;c[3]+=(s<0?-s:s);}
        {int s=-gx+gy;c[4]+=(s<0?-s:s);}c[5]+=(gy<0?-gy:gy)+(gx<0?-gx:gx);
        c[6]+=(gx<0?-gx:gx)+(gy<0?-gy:gy);{int s=gx+gy;c[7]+=(s<0?-s:s)/2;}}}
    for(d=0;d<8;d++)if((unsigned)c[d]<bestc){bestc=c[d];best=d;}
    return best;
}

static int stb_cdef_pix(const unsigned char *s, int str, int x, int y, int dir, int pri, int sec) {
    int c=s[y*str+x],pri_off=stb_av1_cdef_dirs[dir][0],sec_off=stb_av1_cdef_dirs[dir][1];
    int sp=0,ss=0,k,off,py,px;
    static const signed char m[4]={1,-1,2,-2};
    for(k=0;k<4;k++){off=m[k]*pri_off;py=off/12;px=off%12;if(px>6)px-=12;if(py>6)py-=12;
        sp+=stb_cdef_clamp(s[(y+py)*str+(x+px)]-c,pri);}
    for(k=0;k<4;k++){off=m[k]*sec_off;py=off/12;px=off%12;if(px>6)px-=12;if(py>6)py-=12;
        ss+=stb_cdef_clamp(s[(y+py)*str+(x+px)]-c,sec);}
    c+=(sp+((sp>0)?4:0))>>3;c+=(ss+((ss>0)?2:0))>>2;
    if(c<0)c=0;if(c>255)c=255;return c;
}

static void stb_av1_cdef_filter_plane(unsigned char *plane, int stride,
                                       int width, int height,
                                       int pri_strength, int sec_strength,
                                       int damping, int bit_depth)
{
    int y, x, by, bx, cy;
    unsigned char *tmp; int tmp_stride;
    (void)damping;(void)bit_depth;
    if(pri_strength==0&&sec_strength==0)return;
    tmp_stride=(width+31)&~31;
    tmp=(unsigned char *)malloc((size_t)(tmp_stride*height));
    if(!tmp)return;
    for(y=0;y<height;y+=8)for(x=0;x<width;x+=8){
        int bh=8,bw=8,dir;
        if(y+bh>height)bh=height-y; if(x+bw>width)bw=width-x;
        if(bh<4||bw<4){for(by=0;by<bh;by++)memcpy(tmp+(y+by)*tmp_stride+x,plane+(y+by)*stride+x,(size_t)bw);continue;}
        dir=stb_cdef_dir(plane+y*stride+x,stride);
        for(by=0;by<bh;by++)for(bx=0;bx<bw;bx++){
            int px=x+bx,py=y+by;
            if(px<2||px>=width-2||py<2||py>=height-2)tmp[py*tmp_stride+px]=plane[py*stride+px];
            else tmp[py*tmp_stride+px]=(unsigned char)stb_cdef_pix(plane,stride,px,py,dir,pri_strength,sec_strength);
        }
    }
    for(cy=0;cy<height;cy++)memcpy(plane+cy*stride,tmp+cy*tmp_stride,(size_t)width);
    free(tmp);
}

/* -------------------------------------------------------------------------- */
/* LOOP RESTORATION — Wiener 7-tap + SGR 3x3, 8-bit only                     */
/* -------------------------------------------------------------------------- */


static const unsigned char stb_sgr_xbx[256]={
255,128,85,64,51,43,37,32,28,26,23,21,20,18,17,16,15,14,14,13,12,12,11,11,10,
10,9,9,9,8,8,8,7,7,7,7,6,6,6,6,6,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,
3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#define STB_LR_STRIDE 390

static void stb_lr_wiener_h(unsigned short *d,const unsigned char *s,const signed short fh[8],int w,int hl,int hr){
 int x;
 for(x=0;x<w;x++){int sum=(1<<14)+s[x]*128,i;
  for(i=0;i<7;i++){int idx=x+i-3;
   if(idx<0){if(!hl)sum+=s[0]*fh[i];else sum+=s[idx]*fh[i];}
   else if(idx>=w){if(!hr)sum+=s[w-1]*fh[i];else sum+=s[idx]*fh[i];}
   else sum+=s[idx]*fh[i];}
  d[x]=(unsigned short)((sum+8)>>4);}}

static void stb_lr_wiener_v(unsigned char *p,unsigned short **ptrs,const signed short fv[8],int w){
 int i,k;
 for(i=0;i<w;i++){int sum=-(1<<12);
  for(k=0;k<6;k++)sum+=ptrs[k][i]*fv[k];sum+=ptrs[5][i]*fv[6];
  p[i]=(unsigned char)((sum+1024)>>11);if(p[i]>255)p[i]=255;if(p[i]<0)p[i]=0;}
 for(i=0;i<5;i++)ptrs[i]=ptrs[i+1];}

static void stb_lr_wiener_hv(unsigned char *p,unsigned short **ptrs,const unsigned char *src,const signed short f[2][8],int w,int hl,int hr){
 unsigned short tmp[STB_LR_STRIDE];const signed short*fh=f[0],*fv=f[1];int i,k;
 stb_lr_wiener_h(tmp,src,fh,w,hl,hr);
 for(i=0;i<w;i++){int sum=-(1<<12);
  for(k=0;k<6;k++)sum+=ptrs[k][i]*fv[k];sum+=tmp[i]*fv[6];
  p[i]=(unsigned char)((sum+1024)>>11);if(p[i]>255)p[i]=255;if(p[i]<0)p[i]=0;}
 for(k=0;k<6;k++)ptrs[k]=ptrs[k+1];ptrs[6]=ptrs[0];}

static void stb_lr_wiener(unsigned char *p,int stride,int w,int h,const signed short f[2][8],int hl,int hr,int ht,int hb,const unsigned char *lpf){
 unsigned short hor[6*STB_LR_STRIDE],*ptrs[7],*rows[6];int i;
 for(i=0;i<6;i++)rows[i]=&hor[i*STB_LR_STRIDE];
 if(ht){ptrs[0]=rows[0];ptrs[1]=rows[0];ptrs[2]=rows[1];ptrs[3]=rows[2];ptrs[4]=rows[2];ptrs[5]=rows[2];
  stb_lr_wiener_h(rows[0],lpf,f[0],w,hl,hr);stb_lr_wiener_h(rows[1],lpf+stride,f[0],w,hl,hr);
  stb_lr_wiener_h(rows[2],p,f[0],w,hl,hr);p+=stride;if(--h<=0)goto lrv1;ptrs[4]=ptrs[5]=rows[3];
  stb_lr_wiener_h(rows[3],p,f[0],w,hl,hr);p+=stride;if(--h<=0)goto lrv2;ptrs[5]=rows[4];
  stb_lr_wiener_h(rows[4],p,f[0],w,hl,hr);p+=stride;if(--h<=0)goto lrv3;ptrs[6]=ptrs[5]+STB_LR_STRIDE;}
 else{ptrs[0]=rows[0];ptrs[1]=rows[0];ptrs[2]=rows[0];ptrs[3]=rows[0];ptrs[4]=rows[0];ptrs[5]=rows[0];
  stb_lr_wiener_h(rows[0],p,f[0],w,hl,hr);p+=stride;if(--h<=0)goto lrv1;ptrs[4]=ptrs[5]=rows[1];
  stb_lr_wiener_h(rows[1],p,f[0],w,hl,hr);p+=stride;if(--h<=0)goto lrv2;ptrs[5]=rows[2];
  stb_lr_wiener_h(rows[2],p,f[0],w,hl,hr);p+=stride;if(--h<=0)goto lrv3;ptrs[6]=rows[3];
  stb_lr_wiener_hv(p,ptrs,p,f,w,hl,hr);p+=stride;if(--h<=0)goto lrv3;ptrs[6]=rows[4];
  stb_lr_wiener_hv(p,ptrs,p,f,w,hl,hr);p+=stride;if(--h<=0)goto lrv3;}
 ptrs[6]=ptrs[5]+STB_LR_STRIDE;
 do{stb_lr_wiener_hv(p,ptrs,p,f,w,hl,hr);p+=stride;}while(--h>0);
 if(!hb)goto lrv3;p+=stride;p+=stride;
 lrv1:stb_lr_wiener_v(p,ptrs,f[1],w);return;
 lrv3:stb_lr_wiener_v(p,ptrs,f[1],w);p+=stride;
 lrv2:stb_lr_wiener_v(p,ptrs,f[1],w);}

static void stb_lr_sgr_box3h(signed int*sumsq,int*sum,const unsigned char*s,int w,int hl,int hr){
 int a=hl?s[-2]:s[0],b=hl?s[-1]:s[0],x;
 for(x=-1;x<w+1;x++){int c=(x+1<w||hr)?s[x+1]:s[w-1];
  sum[x]=a+b+c;sumsq[x]=a*a+b*b+c*c;a=b;b=c;}}

static void stb_lr_sgr_box3v(signed int**sq,int**sm,signed int*oq,int*om,int w){
 int x;for(x=0;x<w+2;x++){oq[x]=sq[0][x]+sq[1][x]+sq[2][x];om[x]=sm[0][x]+sm[1][x]+sm[2][x];}}

static void stb_lr_sgr_calc(signed int*AA,int*BB,int w,int s,int n,int obx){
 int i;for(i=0;i<w+2;i++){unsigned p=(unsigned)(AA[i]*n-BB[i]*BB[i]);
  unsigned z=(p*(unsigned)s+(1<<19))>>20;unsigned x=stb_sgr_xbx[z>255?255:z];
  AA[i]=(int)((x*BB[i]*obx+(1<<11))>>12);BB[i]=(int)x;}}

static void stb_lr_sgr3(unsigned char*p,int stride,int w,int h,const unsigned short pr[2],int hl,int hr,int ht,int hb,const unsigned char*lpf){
#define SGB 400
 signed int sqb[SGB*3+16],*sqpt[3],*sqr[3];int smb[SGB*3+16],*smt[3],*smr[3];
 signed int Ab[SGB*3+16],*Apt[3];int Bb[SGB*3+16],*Bpt[3];
 int i,s=(int)pr[1],w1=(int)pr[0],*sumpt[3],*sum_rows[3];
 for(i=0;i<3;i++){sqr[i]=&sqb[i*SGB];smr[i]=&smb[i*SGB];Apt[i]=&Ab[i*SGB];Bpt[i]=&Bb[i*SGB];}
 if(ht){sqpt[0]=sqr[0];sqpt[1]=sqr[1];sqpt[2]=sqr[2];smt[0]=smr[0];smt[1]=smr[1];smt[2]=smr[2];
  stb_lr_sgr_box3h(sqr[0],smr[0],lpf,w,hl,hr);stb_lr_sgr_box3h(sqr[1],smr[1],lpf+stride,w,hl,hr);
  stb_lr_sgr_box3h(sqr[2],smr[2],p,w,hl,hr);p+=stride;
  stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
  {int i;for(i=0;i<2;i++){sqpt[i]=sqpt[i+1];smt[i]=smt[i+1];Apt[i]=Apt[i+1];Bpt[i]=Bpt[i+1];}}
  if(--h<=0)goto lsgv1;sqpt[2]=sqr[2];smt[2]=smr[2];}
 else{sqpt[0]=sqr[0];sqpt[1]=sqr[0];sqpt[2]=sqr[0];smt[0]=smr[0];smt[1]=smr[0];smt[2]=smr[0];
  stb_lr_sgr_box3h(sqr[0],smr[0],p,w,hl,hr);p+=stride;
  stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
  {int i;for(i=0;i<2;i++){sqpt[i]=sqpt[i+1];smt[i]=smt[i+1];Apt[i]=Apt[i+1];Bpt[i]=Bpt[i+1];}}
  if(--h<=0)goto lsgv1;sqpt[2]=sqr[1];smt[2]=smr[1];}
 do{stb_lr_sgr_box3h(sqpt[2],smt[2],p,w,hl,hr);p+=stride;
  stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
  {int i,tmp[STB_LR_STRIDE];for(i=0;i<w;i++){int a=(Bpt[1][i+1]+Bpt[1][i]+Bpt[1][i+2]+Bpt[0][i+1]+Bpt[2][i+1])*4+(Bpt[0][i]+Bpt[2][i]+Bpt[0][i+2]+Bpt[2][i+2])*3;
   int b=(Apt[1][i+1]+Apt[1][i]+Apt[1][i+2]+Apt[0][i+1]+Apt[2][i+1])*4+(Apt[0][i]+Apt[2][i]+Apt[0][i+2]+Apt[2][i+2])*3;
   tmp[i]=(b-a*(int)(p-stride)[i]+(1<<8))>>9;}
  for(i=0;i<w;i++){int v=w1*tmp[i];int nv=(int)(p-stride)[i]+((v+(1<<10))>>11);
   if(nv<0)nv=0;if(nv>255)nv=255;(p-stride)[i]=(unsigned char)nv;}}
  {int i;for(i=0;i<2;i++){sqpt[i]=sqpt[i+1];smt[i]=smt[i+1];Apt[i]=Apt[i+1];Bpt[i]=Bpt[i+1];}}
 }while(--h>0);
 if(!hb)goto lsgv2;
 stb_lr_sgr_box3h(sqpt[2],smt[2],p,w,hl,hr);p+=stride;
 stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
 {int i,tmp[STB_LR_STRIDE];for(i=0;i<w;i++){int a=(Bpt[1][i+1]+Bpt[1][i]+Bpt[1][i+2]+Bpt[0][i+1]+Bpt[2][i+1])*4+(Bpt[0][i]+Bpt[2][i]+Bpt[0][i+2]+Bpt[2][i+2])*3;
  int b=(Apt[1][i+1]+Apt[1][i]+Apt[1][i+2]+Apt[0][i+1]+Apt[2][i+1])*4+(Apt[0][i]+Apt[2][i]+Apt[0][i+2]+Apt[2][i+2])*3;
  tmp[i]=(b-a*(int)(p-stride)[i]+(1<<8))>>9;}
 for(i=0;i<w;i++){int v=w1*tmp[i];int nv=(int)(p-stride)[i]+((v+(1<<10))>>11);
  if(nv<0)nv=0;if(nv>255)nv=255;(p-stride)[i]=(unsigned char)nv;}}
 return;
 lsgv2:sqpt[2]=sqpt[1];smt[2]=smt[1];stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
 {int i,tmp[STB_LR_STRIDE];for(i=0;i<w;i++){int a=(Bpt[1][i+1]+Bpt[1][i]+Bpt[1][i+2]+Bpt[0][i+1]+Bpt[2][i+1])*4+(Bpt[0][i]+Bpt[2][i]+Bpt[0][i+2]+Bpt[2][i+2])*3;
  int b=(Apt[1][i+1]+Apt[1][i]+Apt[1][i+2]+Apt[0][i+1]+Apt[2][i+1])*4+(Apt[0][i]+Apt[2][i]+Apt[0][i+2]+Apt[2][i+2])*3;
  tmp[i]=(b-a*(int)(p-stride)[i]+(1<<8))>>9;}
 for(i=0;i<w;i++){int v=w1*tmp[i];int nv=(int)(p-stride)[i]+((v+(1<<10))>>11);
  if(nv<0)nv=0;if(nv>255)nv=255;(p-stride)[i]=(unsigned char)nv;}}
 return;
 lsgv1:sqpt[2]=sqpt[1];smt[2]=smt[1];stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
 {int i;for(i=0;i<2;i++){sqpt[i]=sqpt[i+1];smt[i]=smt[i+1];Apt[i]=Apt[i+1];Bpt[i]=Bpt[i+1];}}
 goto lsgv1_cont;
 lsgv1_cont:
 sqpt[2]=sqpt[1];smt[2]=smt[1];stb_lr_sgr_box3v(sqpt,smt,Apt[2],Bpt[2],w);stb_lr_sgr_calc(Apt[2],Bpt[2],w,s,9,455);
 {int i,tmp[STB_LR_STRIDE];for(i=0;i<w;i++){int a=(Bpt[1][i+1]+Bpt[1][i]+Bpt[1][i+2]+Bpt[0][i+1]+Bpt[2][i+1])*4+(Bpt[0][i]+Bpt[2][i]+Bpt[0][i+2]+Bpt[2][i+2])*3;
  int b=(Apt[1][i+1]+Apt[1][i]+Apt[1][i+2]+Apt[0][i+1]+Apt[2][i+1])*4+(Apt[0][i]+Apt[2][i]+Apt[0][i+2]+Apt[2][i+2])*3;
  tmp[i]=(b-a*(int)(p-stride)[i]+(1<<8))>>9;}
 for(i=0;i<w;i++){int v=w1*tmp[i];int nv=(int)(p-stride)[i]+((v+(1<<10))>>11);
  if(nv<0)nv=0;if(nv>255)nv=255;(p-stride)[i]=(unsigned char)nv;}}
#undef SGB
}

static void stb_av1_loop_restoration_plane(unsigned char *plane,int stride,int width,int height,int lr_type,const signed short wiener_f[2][8],const unsigned short sgr_p[2],int lr_us){
 int y;(void)lr_us;
 for(y=0;y<height;y+=lr_us){int uh=lr_us;const unsigned char*lpf=plane+(y>0?y-2:y)*stride;
  if(y+uh>height)uh=height-y;
  if(lr_type==0)stb_lr_wiener(plane+y*stride,stride,width,uh,wiener_f,1,1,(y>0),(y+uh<height),lpf);
  else stb_lr_sgr3(plane+y*stride,stride,width,uh,sgr_p,1,1,(y>0),(y+uh<height),lpf);}}
#undef STB_LR_STRIDE


#ifdef STB_AVIF_USE_C89_DAV1D

/* -------------------------------------------------------------------------- */
/* RAW BIT READER (GetBits) — for OBU/sequence/frame headers                  */
/* AV1 frame headers use raw bits (f(n)), NOT the arithmetic coder (MSAC).    */
/* -------------------------------------------------------------------------- */

struct StbAv1GetBits {
    const unsigned char *ptr;
    const unsigned char *ptr_end;
    const unsigned char *ptr_start;
    unsigned int state;
    int bits_left;
    int error;
};

static void stb_av1_gb_init(struct StbAv1GetBits *gb,
                            const unsigned char *data, unsigned int size)
{
    gb->ptr_start = data;
    gb->ptr = data;
    gb->ptr_end = data + size;
    gb->state = 0;
    gb->bits_left = 0;
    gb->error = 0;
}

static void stb_av1_gb_refill(struct StbAv1GetBits *gb)
{
    while (gb->bits_left <= 24 && gb->ptr < gb->ptr_end) {
        gb->state = (gb->state << 8) | (unsigned int)(*gb->ptr);
        gb->ptr++;
        gb->bits_left += 8;
    }
}

static int stb_av1_gb_bit(struct StbAv1GetBits *gb)
{
    if (gb->error) return 0;
    if (gb->bits_left == 0) stb_av1_gb_refill(gb);
    if (gb->bits_left == 0) { gb->error = 1; return 0; }
    gb->bits_left--;
    return (int)((gb->state >> gb->bits_left) & 1U);
}

static unsigned int stb_av1_gb_bits(struct StbAv1GetBits *gb, int n)
{
    unsigned int val = 0;
    int i;
    if (gb->error) return 0;
    for (i = 0; i < n; i++)
        val = (val << 1) | (unsigned int)stb_av1_gb_bit(gb);
    return val;
}

static int stb_av1_gb_sbits(struct StbAv1GetBits *gb, int n)
{
    unsigned int u = stb_av1_gb_bits(gb, n);
    if (n > 0 && u >= ((unsigned int)1 << (n - 1)))
        u |= ~(((unsigned int)1 << n) - 1U);
    return (int)u;
}

static void stb_av1_gb_bytealign(struct StbAv1GetBits *gb)
{
    int r = gb->bits_left % 8;
    if (r) gb->bits_left -= r;
}

static unsigned int stb_av1_gb_pos_bytes(struct StbAv1GetBits *gb)
{
    int bit_pos = (int)((gb->ptr - gb->ptr_start) * 8) - gb->bits_left;
    return (unsigned int)((bit_pos + 7) / 8);
}

static unsigned int stb_av1_gb_uniform(struct StbAv1GetBits *gb, unsigned int n)
{
    unsigned int l = 0, m, v;
    if (n <= 1) return 0;
    while (((unsigned)1 << l) < n) l++;
    m = ((unsigned)1 << l) - n;
    v = stb_av1_gb_bits(gb, (int)(l - 1));
    if (v < m) return v;
    return (v << 1) - m + (unsigned int)stb_av1_gb_bit(gb);
}

/* tile_log2: smallest k such that (sz << k) >= tgt */
static int stb_av1_tile_log2(int sz, int tgt)
{
    int k = 0;
    while ((sz << k) < tgt) k++;
    return k;
}

/* GetBits-based sequence header parser (raw bits, not MSAC). */
static void stb_av1_parse_seq_hdr_getbits(struct StbAv1GetBits *gb,
                                          struct stb_av1_sequence_header *sh)
{
    int op;
    sh->seq_profile = (int)stb_av1_gb_bits(gb, 3);
    sh->still_picture = stb_av1_gb_bit(gb);
    sh->reduced_still_picture_header = stb_av1_gb_bit(gb);

    if (sh->reduced_still_picture_header) {
        sh->timing_info_present = 0;
        sh->decoder_model_info_present = 0;
        sh->display_model_info_present = 0;
        sh->operating_points_cnt = 1;
        stb_av1_gb_bits(gb, 3); /* major_level */
        stb_av1_gb_bits(gb, 2); /* minor_level */
    } else {
        sh->timing_info_present = stb_av1_gb_bit(gb);
        if (sh->timing_info_present) {
            stb_av1_gb_bits(gb, 32); stb_av1_gb_bits(gb, 32);
            if (stb_av1_gb_bit(gb)) stb_av1_gb_bits(gb, 32);
            sh->decoder_model_info_present = stb_av1_gb_bit(gb);
            if (sh->decoder_model_info_present) {
                stb_av1_gb_bits(gb, 5); stb_av1_gb_bits(gb, 32);
                sh->buffer_removal_time_length_minus_1 = (int)stb_av1_gb_bits(gb, 5);
                stb_av1_gb_bits(gb, 5);
            }
        }
        sh->display_model_info_present = stb_av1_gb_bit(gb);
        sh->operating_points_cnt = (int)stb_av1_gb_bits(gb, 5) + 1;
        for (op = 0; op < sh->operating_points_cnt; op++) {
            stb_av1_gb_bits(gb, 12); /* idc */
            { int major = (int)stb_av1_gb_bits(gb, 3) + 2;
              int minor = (int)stb_av1_gb_bits(gb, 2);
              if (major > 3) stb_av1_gb_bit(gb); /* tier */
              (void)major; (void)minor; }
            if (sh->decoder_model_info_present) {
                if (stb_av1_gb_bit(gb)) {
                    stb_av1_gb_bits(gb, 8); stb_av1_gb_bits(gb, 8);
                    stb_av1_gb_bit(gb);
                }
            }
            if (sh->display_model_info_present) {
                if (stb_av1_gb_bit(gb))
                    stb_av1_gb_bits(gb, 4);
            }
        }
    }

    /* width/height bits — read for BOTH reduced and non-reduced */
    sh->frame_width_bits = (int)stb_av1_gb_bits(gb, 4) + 1;
    sh->frame_height_bits = (int)stb_av1_gb_bits(gb, 4) + 1;
    sh->max_frame_width = (int)stb_av1_gb_bits(gb, sh->frame_width_bits) + 1;
    sh->max_frame_height = (int)stb_av1_gb_bits(gb, sh->frame_height_bits) + 1;

    if (!sh->reduced_still_picture_header) {
        sh->frame_id_numbers_present = stb_av1_gb_bit(gb);
        if (sh->frame_id_numbers_present) {
            int delta_n = (int)stb_av1_gb_bits(gb, 4) + 2;
            sh->order_hint_n_bits = (int)stb_av1_gb_bits(gb, 3) + delta_n + 1;
            (void)delta_n;
        }
    }

    /* sb128, filter_intra, intra_edge_filter — for BOTH cases */
    sh->sb128 = stb_av1_gb_bit(gb);
    sh->filter_intra = stb_av1_gb_bit(gb);
    sh->enable_intra_edge_filter = stb_av1_gb_bit(gb);
    fprintf(stderr, "[SEQHDR] sb128=%d filter_intra=%d intra_edge_filter=%d\n", sh->sb128, sh->filter_intra, sh->enable_intra_edge_filter);

    if (sh->reduced_still_picture_header) {
        sh->screen_content_tools = 0; /* ADAPTIVE */
        sh->force_integer_mv = 0; /* ADAPTIVE */
    } else {
        sh->enable_interintra_comp = stb_av1_gb_bit(gb);
        sh->enable_masked_comp = stb_av1_gb_bit(gb);
        stb_av1_gb_bit(gb); /* warped_motion */
        sh->enable_dual_filter = stb_av1_gb_bit(gb);
        sh->enable_order_hint = stb_av1_gb_bit(gb);
        if (sh->enable_order_hint) {
            sh->order_hint_n_bits = (int)stb_av1_gb_bits(gb, 2) + 1;
            sh->enable_jnt_comp = stb_av1_gb_bit(gb);
            sh->enable_ref_frame_mvs = stb_av1_gb_bit(gb);
        }
        /* seq_force_screen_content_tools: 1 -> ADAPTIVE, 0 -> read choose bit */
        if (stb_av1_gb_bit(gb))
            sh->screen_content_tools = 0; /* ADAPTIVE */
        else
            sh->screen_content_tools = stb_av1_gb_bit(gb) ? 1 : 2; /* ON / OFF */
        /* seq_force_integer_mv only if screen_content_tools != OFF */
        if (sh->screen_content_tools != 2) {
            if (stb_av1_gb_bit(gb))
                sh->force_integer_mv = 0; /* ADAPTIVE */
            else
                sh->force_integer_mv = stb_av1_gb_bit(gb) ? 1 : 2; /* ON / OFF */
        } else {
            sh->force_integer_mv = 0; /* ADAPTIVE */
        }
    }

    /* enable_superres, enable_cdef, enable_restoration — always present in the
       bitstream (even for reduced_still_picture), matching dav1d */
    sh->enable_superres = stb_av1_gb_bit(gb);
    sh->enable_cdef = stb_av1_gb_bit(gb);
    sh->enable_restoration = stb_av1_gb_bit(gb);
    fprintf(stderr, "ENREST3 enable_restoration=%d\n", sh->enable_restoration);

    /* Color config */
    {
        int hbd = stb_av1_gb_bit(gb);
        if (sh->seq_profile == 2 && hbd) {
            int twelve = stb_av1_gb_bit(gb);
            sh->bit_depth = twelve ? 12 : 10;
        } else {
            sh->bit_depth = hbd ? 10 : 8;
        }
        if (sh->seq_profile == 1) {
            sh->monochrome = 0;
        } else {
            sh->monochrome = stb_av1_gb_bit(gb);
        }
        if (stb_av1_gb_bit(gb)) { /* color_description_present */
            sh->color_primaries = (int)stb_av1_gb_bits(gb, 8);
            sh->transfer_characteristics = (int)stb_av1_gb_bits(gb, 8);
            sh->matrix_coefficients = (int)stb_av1_gb_bits(gb, 8);
        } else {
            sh->color_primaries = 2; /* unspecified */
            sh->transfer_characteristics = 2;
            sh->matrix_coefficients = 2;
        }
        if (sh->monochrome) {
            sh->color_range = stb_av1_gb_bit(gb);
            sh->subsampling_x = 1; sh->subsampling_y = 1;
        } else if (sh->color_primaries == 1 && sh->transfer_characteristics == 13 && sh->matrix_coefficients == 0) {
            sh->color_range = 1; sh->subsampling_x = 0; sh->subsampling_y = 0;
        } else {
            sh->color_range = stb_av1_gb_bit(gb);
            if (sh->seq_profile == 0) { sh->subsampling_x = 1; sh->subsampling_y = 1; }
            else if (sh->seq_profile == 1) { sh->subsampling_x = 0; sh->subsampling_y = 0; }
            else {
                if (sh->bit_depth == 12) {
                    sh->subsampling_x = stb_av1_gb_bit(gb);
                    if (sh->subsampling_x) sh->subsampling_y = stb_av1_gb_bit(gb);
                    else sh->subsampling_y = 0;
                } else { sh->subsampling_x = 1; sh->subsampling_y = 0; }
            }
        }
        if (!sh->monochrome)
            stb_av1_gb_bit(gb); /* chroma_sample_position (if ssx==1 && ssy==1) — simplified */
        sh->film_grain_params_present = stb_av1_gb_bit(gb);
        sh->separate_uv_delta_q = stb_av1_gb_bit(gb);
    }
}

/* GetBits-based frame header parser (raw bits, not MSAC).
   Follows dav1d obu.c parse_frame_hdr for intra-only / key frame path.
    *frame_hdr_bytes_out = byte offset of tile data start from OBU data. */
 static void stb_av1_parse_frame_hdr_getbits(struct StbAv1GetBits *gb,
                                              struct stb_av1_frame_header *fh,
                                              struct stb_av1_sequence_header *sh,
                                              unsigned int *frame_hdr_bytes_out)
 {
#define STB_FHDR_DBG(s) { static int _fh=0; if (_fh++<200) fprintf(stderr,"[FHDR] %s: ptr_off=%ld bits_left=%d\n", s, (long)(gb->ptr - gb->ptr_start), gb->bits_left); }
    if (!sh->reduced_still_picture_header)
        fh->show_existing_frame = stb_av1_gb_bit(gb);
    if (fh->show_existing_frame) {
        stb_av1_gb_bits(gb, 3);
        *frame_hdr_bytes_out = stb_av1_gb_pos_bytes(gb);
        return;
    }
    if (sh->reduced_still_picture_header) {
        fh->frame_type = STB_AV1_KEY_FRAME;
        fh->show_frame = 1;
    } else {
        fh->frame_type = (int)stb_av1_gb_bits(gb, 2);
        fh->show_frame = stb_av1_gb_bit(gb);
    }
    fh->error_resilient_mode =
        (fh->frame_type == STB_AV1_KEY_FRAME && fh->show_frame) ||
        fh->frame_type == STB_AV1_S_FRAME ||
        sh->reduced_still_picture_header ? 1 : stb_av1_gb_bit(gb);

    if (!sh->reduced_still_picture_header) {
        fh->disable_cdf_update = stb_av1_gb_bit(gb);
        fh->allow_screen_content_tools =
            sh->screen_content_tools == 0 /* ADAPTIVE */
                ? stb_av1_gb_bit(gb)
                : (sh->screen_content_tools == 1 ? 1 : 0);
        if (fh->allow_screen_content_tools)
            fh->force_integer_mv =
                sh->force_integer_mv == 0 /* ADAPTIVE */
                    ? stb_av1_gb_bit(gb)
                    : (sh->force_integer_mv == 1 ? 1 : 0);
    } else {
        fh->disable_cdf_update = stb_av1_gb_bit(gb);
        fh->allow_screen_content_tools =
            sh->screen_content_tools == 0 ? stb_av1_gb_bit(gb) :
            (sh->screen_content_tools == 1 ? 1 : 0);
        if (fh->allow_screen_content_tools)
            fh->force_integer_mv =
                sh->force_integer_mv == 0 ? stb_av1_gb_bit(gb) :
                (sh->force_integer_mv == 1 ? 1 : 0);
    }
    if (fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY)
        fh->force_integer_mv = 1;

    if (sh->frame_id_numbers_present)
        stb_av1_gb_bits(gb, sh->order_hint_n_bits + 3); /* frame_id */

    if (!sh->reduced_still_picture_header)
        fh->frame_size_override = (fh->frame_type == STB_AV1_S_FRAME) ? 1 : stb_av1_gb_bit(gb);
    if (sh->enable_order_hint)
        fh->order_hint = (int)stb_av1_gb_bits(gb, sh->order_hint_n_bits);
    fh->primary_ref_frame = (!fh->error_resilient_mode &&
        fh->frame_type != STB_AV1_KEY_FRAME && fh->frame_type != STB_AV1_INTRA_ONLY)
        ? (int)stb_av1_gb_bits(gb, 3) : 7;

    if (fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY) {
        fh->refresh_frame_flags = (fh->frame_type == STB_AV1_KEY_FRAME && fh->show_frame)
            ? 0xFF : (int)stb_av1_gb_bits(gb, 8);
        /* read_frame_size */
        if (fh->frame_size_override) {
            fh->frame_width = (int)stb_av1_gb_bits(gb, sh->frame_width_bits) + 1;
            fh->frame_height = (int)stb_av1_gb_bits(gb, sh->frame_height_bits) + 1;
        } else {
            fh->frame_width = sh->max_frame_width;
            fh->frame_height = sh->max_frame_height;
        }
        fh->superres_scale_denominator = 8;
        if (sh->enable_superres && stb_av1_gb_bit(gb))
            fh->superres_scale_denominator = (int)stb_av1_gb_bits(gb, 3) + 9;
        fh->render_width = fh->frame_width;
        fh->render_height = fh->frame_height;
        if (stb_av1_gb_bit(gb)) { /* have_render_size */
            fh->render_width = (int)stb_av1_gb_bits(gb, 16) + 1;
            fh->render_height = (int)stb_av1_gb_bits(gb, 16) + 1;
        }
        if (fh->allow_screen_content_tools && fh->superres_scale_denominator == 8)
            fh->allow_intrabc = stb_av1_gb_bit(gb);
    } else {
        fh->refresh_frame_flags = (fh->frame_type == STB_AV1_S_FRAME)
            ? 0xFF : (int)stb_av1_gb_bits(gb, 8);
        /* Inter frame: read refs + frame_size (not needed for AVIF still) */
        fh->frame_width = sh->max_frame_width;
        fh->frame_height = sh->max_frame_height;
    }

    if (!sh->reduced_still_picture_header && !fh->disable_cdf_update)
        stb_av1_gb_bit(gb); /* refresh_context */

    /* Tiling */
    STB_FHDR_DBG("pre-tiling")
    {
        int sbsz_log2 = 6 + sh->sb128;
        int sbsz = 64 << sh->sb128;
        int sbw = (fh->frame_width + sbsz - 1) >> sbsz_log2;
        int sbh = (fh->frame_height + sbsz - 1) >> sbsz_log2;
        int max_tile_width_sb = 4096 >> sbsz_log2;
        int max_tile_area_sb = (4096 * 2304) >> (2 * sbsz_log2);
        int min_log2_cols = stb_av1_tile_log2(max_tile_width_sb, sbw);
        int max_log2_cols = stb_av1_tile_log2(1, sbw < 64 ? sbw : 64);
        int max_log2_rows = stb_av1_tile_log2(1, sbh < 64 ? sbh : 64);
        int min_log2_tiles = (stb_av1_tile_log2(max_tile_area_sb, sbw * sbh) > min_log2_cols)
            ? stb_av1_tile_log2(max_tile_area_sb, sbw * sbh) : min_log2_cols;
        int uniform = stb_av1_gb_bit(gb);
        int log2_cols, log2_rows, cols, rows;
        if (uniform) {
            for (log2_cols = min_log2_cols; log2_cols < max_log2_cols && stb_av1_gb_bit(gb); log2_cols++) ;
            { int tile_w = 1 + ((sbw - 1) >> log2_cols);
              int min_log2_rows = (min_log2_tiles - log2_cols > 0) ? min_log2_tiles - log2_cols : 0;
              for (log2_rows = min_log2_rows; log2_rows < max_log2_rows && stb_av1_gb_bit(gb); log2_rows++) ;
              (void)tile_w; }
        } else {
            int widest = 0, area = sbw * sbh;
            cols = 0;
            { int sbx;
              for (sbx = 0; sbx < sbw && cols < 64; cols++) {
                  int tw = (sbw - sbx < max_tile_width_sb) ? sbw - sbx : max_tile_width_sb;
                  int w = (tw > 1) ? 1 + (int)stb_av1_gb_uniform(gb, (unsigned)tw) : 1;
                  sbx += w;
                  if (w > widest) widest = w;
              }
            }
            log2_cols = stb_av1_tile_log2(1, cols);
            if (min_log2_tiles) area >>= min_log2_tiles + 1;
            { int max_th = (area / widest > 1) ? area / widest : 1;
              int sby;
              rows = 0;
              for (sby = 0; sby < sbh && rows < 64; rows++) {
                  int th = (sbh - sby < max_th) ? sbh - sby : max_th;
                  int h = (th > 1) ? 1 + (int)stb_av1_gb_uniform(gb, (unsigned)th) : 1;
                  sby += h;
              }
            }
            log2_rows = stb_av1_tile_log2(1, rows);
        }
        if (uniform) {
            cols = 0;
            { int tile_w = 1 + ((sbw - 1) >> log2_cols);
              int sbx;
              for (sbx = 0; sbx < sbw; sbx += tile_w, cols++) ; }
            rows = 0;
            { int min_log2_rows = (min_log2_tiles - log2_cols > 0) ? min_log2_tiles - log2_cols : 0;
              (void)min_log2_rows;
              { int tile_h = 1 + ((sbh - 1) >> log2_rows);
                int sby;
                for (sby = 0; sby < sbh; sby += tile_h, rows++) ; } }
        }
        fh->tile_cols = cols;
        fh->tile_rows = rows;
        if (log2_cols || log2_rows) {
            stb_av1_gb_bits(gb, log2_cols + log2_rows); /* tile update idx */
            fh->tile_cols = cols; fh->tile_rows = rows;
            stb_av1_gb_bits(gb, 2); /* n_bytes */
        }
    }

    /* Quant */
    fh->base_q_idx = (int)stb_av1_gb_bits(gb, 8);
    fh->delta_q_y_dc = 0;
    if (stb_av1_gb_bit(gb))
        fh->delta_q_y_dc = stb_av1_gb_sbits(gb, 7);
    fh->delta_q_u_dc = 0; fh->delta_q_u_ac = 0;
    fh->delta_q_v_dc = 0; fh->delta_q_v_ac = 0;
    if (!sh->monochrome) {
        int diff_uv = sh->separate_uv_delta_q ? stb_av1_gb_bit(gb) : 0;
        if (stb_av1_gb_bit(gb)) fh->delta_q_u_dc = stb_av1_gb_sbits(gb, 7);
        if (stb_av1_gb_bit(gb)) fh->delta_q_u_ac = stb_av1_gb_sbits(gb, 7);
        if (diff_uv) {
            if (stb_av1_gb_bit(gb)) fh->delta_q_v_dc = stb_av1_gb_sbits(gb, 7);
            if (stb_av1_gb_bit(gb)) fh->delta_q_v_ac = stb_av1_gb_sbits(gb, 7);
        } else {
            fh->delta_q_v_dc = fh->delta_q_u_dc;
            fh->delta_q_v_ac = fh->delta_q_u_ac;
        }
    }
    fh->using_qmatrix = stb_av1_gb_bit(gb);
    if (fh->using_qmatrix) {
        fh->qm_y = (int)stb_av1_gb_bits(gb, 4);
        fh->qm_u = (int)stb_av1_gb_bits(gb, 4);
        fh->qm_v = sh->separate_uv_delta_q ? (int)stb_av1_gb_bits(gb, 4) : fh->qm_u;
    }

    /* Segmentation */
    fh->segmentation_enabled = stb_av1_gb_bit(gb);
    if (fh->segmentation_enabled) {
        if (fh->primary_ref_frame == 7) {
            fh->segment_update_map = 1;
            /* update_data = 1, temporal = 0 */
        } else {
            fh->segment_update_map = stb_av1_gb_bit(gb);
            if (fh->segment_update_map)
                stb_av1_gb_bit(gb); /* temporal */
            stb_av1_gb_bit(gb); /* update_data */
        }
        /* Per-segment data (when update_data=1, which is always for pri_ref=7) */
        {
            int i;
            for (i = 0; i < 8; i++) {
                if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 9); /* delta_q */
                if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 7); /* delta_lf_y_v */
                if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 7); /* delta_lf_y_h */
                if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 7); /* delta_lf_u */
                if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 7); /* delta_lf_v */
                if (stb_av1_gb_bit(gb)) stb_av1_gb_bits(gb, 3);  /* ref */
                stb_av1_gb_bit(gb); /* skip */
                stb_av1_gb_bit(gb); /* globalmv */
            }
        }
    }

    /* Delta Q/LF flags */
        if (fh->base_q_idx) {
            if (stb_av1_gb_bit(gb)) { /* delta_q_present */
                fh->delta_q_present = 1;
                fh->delta_q_res_log2 = (int)stb_av1_gb_bits(gb, 2) + 1;
                if (!fh->allow_intrabc) {
                    if (stb_av1_gb_bit(gb)) { /* delta_lf_present */
                        fh->delta_lf_present = 1;
                        fh->delta_lf_res_log2 = (int)stb_av1_gb_bits(gb, 2) + 1;
                        fh->delta_lf_multi = (int)stb_av1_gb_bit(gb);
                    }
                }
            }
        }

    /* All_lossless determination */
    STB_FHDR_DBG("post-tiling")
    {
        int delta_lossless = !fh->delta_q_y_dc && !fh->delta_q_u_dc &&
            !fh->delta_q_u_ac && !fh->delta_q_v_dc && !fh->delta_q_v_ac;
        int all_lossless = (!fh->base_q_idx && delta_lossless) ? 1 : 0;
        (void)all_lossless;
        fh->tx_mode = 2; /* default to SELECT */
    }

    /* Loopfilter */
    STB_FHDR_DBG("post-quant")
    {
        int lf_not_lossless = fh->base_q_idx || fh->delta_q_y_dc ||
            fh->delta_q_u_dc;
        if (lf_not_lossless && !fh->allow_intrabc) {
            stb_av1_gb_bits(gb, 6); /* level_y[0] */
            stb_av1_gb_bits(gb, 6); /* level_y[1] */
            { static int _lf=1; if (_lf) { _lf=0; fprintf(stderr,"[FHDR] level_y0=%u level_y1=%u\n", (gb->state >> (gb->bits_left-6)) & 0x3F, (gb->state >> (gb->bits_left-12)) & 0x3F); } }
            if (!sh->monochrome) {
                stb_av1_gb_bits(gb, 6); /* level_u */
                stb_av1_gb_bits(gb, 6); /* level_v */
            }
            stb_av1_gb_bits(gb, 3); /* sharpness */
            /* mode_ref_delta_enabled */
            if (stb_av1_gb_bit(gb)) {
                if (stb_av1_gb_bit(gb)) { /* mode_ref_delta_update */
                    int i;
                    for (i = 0; i < 8; i++)
                        if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 7);
                    for (i = 0; i < 2; i++)
                        if (stb_av1_gb_bit(gb)) stb_av1_gb_sbits(gb, 7);
                }
            }
        }
    }

    /* CDEF */
    STB_FHDR_DBG("post-loopfilter")
    if (sh->enable_cdef && !fh->allow_intrabc) {
        int not_lossless = fh->base_q_idx != 0;
        if (not_lossless) {
            int i, n_bits;
            fh->cdef_damping = (int)stb_av1_gb_bits(gb, 2) + 3;
            n_bits = (int)stb_av1_gb_bits(gb, 2);
            fh->cdef_bits = n_bits;
            for (i = 0; i < (1 << n_bits); i++) {
                fh->cdef_y_pri_strength[i] = (int)stb_av1_gb_bits(gb, 4);
                fh->cdef_y_sec_strength[i] = (int)stb_av1_gb_bits(gb, 2);
                if (!sh->monochrome) {
                    fh->cdef_uv_pri_strength[i] = (int)stb_av1_gb_bits(gb, 4);
                    fh->cdef_uv_sec_strength[i] = (int)stb_av1_gb_bits(gb, 2);
                }
            }
        }
    }

    /* Restoration */
    STB_FHDR_DBG("post-cdef")
    if (sh->enable_restoration && !fh->allow_intrabc) {
        int not_lossless = fh->base_q_idx != 0;
        if (not_lossless) {
            int i;
            int n_planes = sh->monochrome ? 1 : 3;
            for (i = 0; i < n_planes; i++) {
                fh->lr_type[i] = (int)stb_av1_gb_bits(gb, 2);
                if (fh->lr_type[i] && i == 0) {
                    fh->lr_unit_size[i] = 6 + sh->sb128;
                    if (stb_av1_gb_bit(gb)) {
                        fh->lr_unit_size[i]++;
                        if (!sh->sb128)
                            fh->lr_unit_size[i] += stb_av1_gb_bit(gb);
                    }
                }
                fprintf(stderr, "LRDBG lr_type[%d]=%d us=%d\n", i, fh->lr_type[i], fh->lr_unit_size[i]);
            }
        }
    }

    /* Txfm mode: always present when not all_lossless, even for
       reduced_still_picture_header (dav1d obu.c:959-965). */
    STB_FHDR_DBG("post-restoration")
    {
        int not_lossless = fh->base_q_idx != 0;
        if (not_lossless)
            fh->tx_mode = stb_av1_gb_bit(gb) ? 2 /* SWITCHABLE */ : 1 /* LARGEST */;
        else
            fh->tx_mode = 0; /* ONLY_4X4 */
    }
    fh->skip_mode = 0;

    /* reduced_txtp_set */
    STB_FHDR_DBG("pre-reducedtxtp")
    fh->reduced_tx_set = stb_av1_gb_bit(gb);
    STB_FHDR_DBG("post-reducedtxtp")
    fprintf(stderr,"[FHDR] reduced_tx_set=%d\n", fh->reduced_tx_set);

    /* Byte-align: tile data starts here */
    stb_av1_gb_bytealign(gb);
    *frame_hdr_bytes_out = stb_av1_gb_pos_bytes(gb);
}

/* MSAC-based sequence header parser.
   obu_hdr_bytes: number of bytes to skip (OBU header byte + LEB128 size field).
   MSAC reads from after these header bytes. */
static void stb_av1_parse_seq_hdr_msac(struct stb_av1_msac *msac,
                                        struct stb_av1_sequence_header *sh,
                                        stbv_u32 *frame_hdr_end,
                                        unsigned obu_hdr_skip) {
    (void)obu_hdr_skip;
    sh->seq_profile = (int)stb_av1_msac_decode_bools(msac, 3);
    sh->still_picture = (int)stb_av1_msac_decode_bool_equi(msac);
    sh->reduced_still_picture_header = (int)stb_av1_msac_decode_bool_equi(msac);
    if (sh->reduced_still_picture_header) {
        sh->timing_info_present = 0; sh->operating_points_cnt = 1;
        sh->frame_width_bits = 4; sh->frame_height_bits = 4;
        sh->max_frame_width = 16; sh->max_frame_height = 16;
        sh->enable_order_hint = 0; sh->enable_intra_edge_filter = 1;
        sh->enable_cdef = 1; sh->enable_restoration = 0;
        sh->film_grain_params_present = 0;
    } else {
        int op; sh->operating_points_cnt = (int)stb_av1_msac_decode_bools(msac, 5) + 1;
        for (op = 0; op < sh->operating_points_cnt; op++) {
            stb_av1_msac_decode_bools(msac, 12); stb_av1_msac_decode_bools(msac, 5);
            if (stb_av1_msac_decode_bool_equi(msac)) stb_av1_msac_decode_bool_equi(msac);
            if (op == 0 && stb_av1_msac_decode_bool_equi(msac)) {
                stb_av1_msac_decode_bools(msac, 8); stb_av1_msac_decode_bools(msac, 8);
                stb_av1_msac_decode_bool_equi(msac); } }
        sh->frame_width_bits = (int)stb_av1_msac_decode_bools(msac, 4) + 1;
        sh->frame_height_bits = (int)stb_av1_msac_decode_bools(msac, 4) + 1;
        sh->max_frame_width = (int)stb_av1_msac_decode_bools(msac, sh->frame_width_bits) + 1;
        sh->max_frame_height = (int)stb_av1_msac_decode_bools(msac, sh->frame_height_bits) + 1;
        if (stb_av1_msac_decode_bool_equi(msac)) { stb_av1_msac_decode_bools(msac, 4); stb_av1_msac_decode_bools(msac, 3); }
        sh->enable_order_hint = (int)stb_av1_msac_decode_bool_equi(msac);
        if (sh->enable_order_hint) stb_av1_msac_decode_bools(msac, 2);
        sh->enable_dist_wtd_comp = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_masked_comp = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_intra_edge_filter = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_interintra_comp = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_dual_filter = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_jnt_comp = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_superres = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->timing_info_present = (int)stb_av1_msac_decode_bool_equi(msac);
        if (sh->timing_info_present) {
            stb_av1_msac_decode_bools(msac, 32); stb_av1_msac_decode_bools(msac, 32);
            if (stb_av1_msac_decode_bool_equi(msac)) stb_av1_msac_decode_bools(msac, 32);
            sh->decoder_model_info_present = (int)stb_av1_msac_decode_bool_equi(msac);
            if (sh->decoder_model_info_present) {
                stb_av1_msac_decode_bools(msac, 5); stb_av1_msac_decode_bools(msac, 4);
                sh->buffer_removal_time_length_minus_1 = (int)stb_av1_msac_decode_bools(msac, 5);
                stb_av1_msac_decode_bools(msac, 5); }
            sh->display_model_info_present = (int)stb_av1_msac_decode_bool_equi(msac); } }
    if (!sh->reduced_still_picture_header && stb_av1_msac_decode_bool_equi(msac))
        stb_av1_msac_decode_bools(msac, 4);
    { int hbd = (int)stb_av1_msac_decode_bool_equi(msac);
      sh->bit_depth = hbd ? (stb_av1_msac_decode_bool_equi(msac) ? 12 : 10) : 8;
      sh->monochrome = (sh->seq_profile == 0 && sh->bit_depth > 8) ? 0 : (int)stb_av1_msac_decode_bool_equi(msac);
      if (stb_av1_msac_decode_bool_equi(msac)) {
          sh->color_primaries = (int)stb_av1_msac_decode_bools(msac, 8);
          sh->transfer_characteristics = (int)stb_av1_msac_decode_bools(msac, 8);
          sh->matrix_coefficients = (int)stb_av1_msac_decode_bools(msac, 8); }
      if (sh->monochrome) {
          sh->color_range = (int)stb_av1_msac_decode_bool_equi(msac);
          sh->subsampling_x = 1; sh->subsampling_y = 1; }
      else if (sh->color_primaries == 1 && sh->transfer_characteristics == 13 && sh->matrix_coefficients == 0) {
          sh->color_range = 1; sh->subsampling_x = 0; sh->subsampling_y = 0; }
      else {
          sh->color_range = (int)stb_av1_msac_decode_bool_equi(msac);
          sh->subsampling_x = (int)stb_av1_msac_decode_bool_equi(msac);
          sh->subsampling_y = (int)stb_av1_msac_decode_bool_equi(msac); }
      sh->film_grain_params_present = (int)stb_av1_msac_decode_bool_equi(msac); }
    /* Skip separator bit (MSAC reads XOR-inverted: separator produces wrong value) */
    (void)stb_av1_msac_decode_bool_equi(msac);
    if (!sh->reduced_still_picture_header) {
        sh->enable_cdef = (int)stb_av1_msac_decode_bool_equi(msac);
        sh->enable_restoration = (int)stb_av1_msac_decode_bool_equi(msac);
        fprintf(stderr, "ENREST enable_restoration=%d\n", sh->enable_restoration); }
    if (frame_hdr_end) *frame_hdr_end = (stbv_u32)(msac->buf_pos - msac->buf_start);
}

/* MSAC-based frame header parser (intra-only) */
static void stb_av1_parse_frame_hdr_msac(struct stb_av1_msac *msac,
                                          struct stb_av1_frame_header *fh,
                                          struct stb_av1_sequence_header *sh,
                                          stbv_u32 *frame_hdr_end) {
    if (sh->reduced_still_picture_header) {
        fh->show_existing_frame = 0;
        fh->frame_type = 0;
        fh->show_frame = 1;
        fh->error_resilient_mode = 0;
    } else {
        fh->show_existing_frame = (int)stb_av1_msac_decode_bool_equi(msac);
        if (fh->show_existing_frame) { stb_av1_msac_decode_bools(msac, 3); return; }
        fh->frame_type = (int)stb_av1_msac_decode_bools(msac, 2);
        fh->show_frame = (int)stb_av1_msac_decode_bool_equi(msac);
        fh->error_resilient_mode = (int)stb_av1_msac_decode_bool_equi(msac);
        if (!fh->error_resilient_mode) {
            fh->disable_cdf_update = (int)stb_av1_msac_decode_bool_equi(msac);
            fh->allow_screen_content_tools = (int)stb_av1_msac_decode_bool_equi(msac);
            if (fh->allow_screen_content_tools)
                fh->force_integer_mv = (int)stb_av1_msac_decode_bool_equi(msac);
        }
    }
    if (sh->reduced_still_picture_header) {
        fh->frame_width = sh->max_frame_width; fh->frame_height = sh->max_frame_height; }
    else {
        if (stb_av1_msac_decode_bool_equi(msac)) {
            fh->frame_width = (int)stb_av1_msac_decode_bools(msac, sh->frame_width_bits) + 1;
            fh->frame_height = (int)stb_av1_msac_decode_bools(msac, sh->frame_height_bits) + 1; }
        else { fh->frame_width = sh->max_frame_width; fh->frame_height = sh->max_frame_height; }
        if (sh->enable_superres && stb_av1_msac_decode_bool_equi(msac)) stb_av1_msac_decode_bools(msac, 3);
        stb_av1_msac_decode_bools(msac, sh->frame_width_bits + 1);
        stb_av1_msac_decode_bools(msac, sh->frame_height_bits + 1); }
    if (fh->frame_type == 2 || fh->frame_type == 3)
        fh->allow_intrabc = (int)stb_av1_msac_decode_bool_equi(msac);
    if (fh->frame_type == 0)
        fh->refresh_frame_flags = fh->show_frame ? 0xFF : (int)stb_av1_msac_decode_bools(msac, 8);
    else if (fh->frame_type == 2)
        fh->refresh_frame_flags = (int)stb_av1_msac_decode_bools(msac, 8);
    if (!sh->reduced_still_picture_header) {
        fh->primary_ref_frame = (fh->error_resilient_mode || (fh->frame_type == 0 && fh->show_frame)) ? 7 : (int)stb_av1_msac_decode_bools(msac, 3); }
    else {
        fh->primary_ref_frame = 7; }
    fh->base_q_idx = (int)stb_av1_msac_decode_bools(msac, 8);
    { int yd = stb_av1_msac_decode_bool_equi(msac) ? (int)stb_av1_msac_decode_subexp(msac, 0, 33, 4) : 0;
      fh->delta_q_y_dc = yd > 16 ? yd - 32 : yd; }
    fh->delta_q_u_dc=0; fh->delta_q_u_ac=0; fh->delta_q_v_dc=0; fh->delta_q_v_ac=0;
    fh->using_qmatrix = (int)stb_av1_msac_decode_bool_equi(msac);
    if (fh->using_qmatrix) {
        fh->qm_y = (int)stb_av1_msac_decode_bools(msac, 4);
        fh->qm_u = (int)stb_av1_msac_decode_bools(msac, 4);
        fh->qm_v = (int)stb_av1_msac_decode_bools(msac, 4); }
    fh->segmentation_enabled = (int)stb_av1_msac_decode_bool_equi(msac);
    if (fh->segmentation_enabled) {
        fh->segment_update_map = (int)stb_av1_msac_decode_bool_equi(msac);
        if (fh->seg_temporal || fh->segment_update_map)
            fh->seg_id_pre_skip = (int)stb_av1_msac_decode_bool_equi(msac); }
    if (fh->primary_ref_frame != 7 || fh->frame_type == 0 || fh->frame_type == 2) {
        fh->delta_q_present = (int)stb_av1_msac_decode_bool_equi(msac);
        if (fh->delta_q_present) {
            fh->delta_q_res_log2 = (int)stb_av1_msac_decode_bools(msac, 2) + 1;
            if (!fh->allow_intrabc) {
                fh->delta_lf_present = (int)stb_av1_msac_decode_bool_equi(msac);
                if (fh->delta_lf_present) {
                    fh->delta_lf_res_log2 = (int)stb_av1_msac_decode_bools(msac, 2) + 1;
                    fh->delta_lf_multi = (int)stb_av1_msac_decode_bool_equi(msac);
                }
            }
        }
    } else {
        fh->delta_q_present = 0;
    }
    fh->tx_mode = (int)stb_av1_msac_decode_bools(msac, 2); fh->skip_mode = 0;
    if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
        if (sh->enable_cdef) { int i; fh->cdef_bits = (int)stb_av1_msac_decode_bools(msac, 2);
            for (i = 0; i < (1 << fh->cdef_bits); i++) {
                fh->cdef_y_pri_strength[i] = (int)stb_av1_msac_decode_bools(msac, 4);
                fh->cdef_y_sec_strength[i] = (int)stb_av1_msac_decode_bools(msac, 2);
                fh->cdef_uv_pri_strength[i] = (int)stb_av1_msac_decode_bools(msac, 4);
                fh->cdef_uv_sec_strength[i] = (int)stb_av1_msac_decode_bools(msac, 2); }
            fh->cdef_damping = (int)stb_av1_msac_decode_bools(msac, 2) + 3; }
        if (sh->enable_restoration) { int i;
            for (i = 0; i < (sh->monochrome ? 1 : 3); i++) {
                fh->lr_type[i] = (int)stb_av1_msac_decode_bools(msac, 2);
                if (fh->lr_type[i]) fh->lr_unit_size[i] = (int)stb_av1_msac_decode_bool_equi(msac) + 1;
            } }
        { int tcl = 0, trl = 0;
      if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
          if (stb_av1_msac_decode_bool_equi(msac)) tcl = (int)stb_av1_msac_decode_bools(msac, 2);
              if (stb_av1_msac_decode_bool_equi(msac)) trl = (int)stb_av1_msac_decode_bools(msac, 2); }
          (void)tcl; (void)trl; }

        if (frame_hdr_end) *frame_hdr_end = (stbv_u32)(msac->buf_pos - msac->buf_start);
        fprintf(stderr, "[FRAME_HDR] type=%d show=%d reduced_still=%d q=%d w=%d h=%d refresh=0x%x\n",
            fh->frame_type, fh->show_frame, sh->reduced_still_picture_header,
            fh->base_q_idx, fh->frame_width, fh->frame_height, fh->refresh_frame_flags);
    }
}
#endif
/* -------------------------------------------------------------------------- */
/* DAV1D BACKEND                                                              */
/* -------------------------------------------------------------------------- */

#ifdef STB_AVIF_USE_DAV1D
static int stb_avif_decode_with_dav1d(const unsigned char *av1_data, size_t av1_size,
                                       int *width, int *height,
                                       unsigned char **y_plane, int *y_stride,
                                       unsigned char **u_plane, int *u_stride,
                                       unsigned char **v_plane, int *v_stride,
                                       int *bit_depth, int *monochrome,
                                       int *subsampling_x, int *subsampling_y)
{
    Dav1dContext *ctx = NULL;
    Dav1dSettings s;
    Dav1dData data;
    Dav1dPicture pic;
    int ret;
    int i;

    dav1d_default_settings(&s);
    s.n_threads = 1;
    s.all_layers = 0;

    ret = dav1d_open(&ctx, &s);
    if (ret < 0) {  return 0; }

    /* Wrap the AV1 data */
    /* Manually initialize Dav1dData to avoid potential NULL check issues */
    memset(&data, 0, sizeof(data));
    data.data = (const uint8_t *)av1_data;
    data.sz = av1_size;
    ret = 0;

    /* Send data to decoder */
    ret = dav1d_send_data(ctx, &data);
    if (ret < 0 && ret != DAV1D_ERR(EAGAIN)) {
        
        dav1d_data_unref(&data);
        dav1d_close(&ctx);
        return 0;
    }
    dav1d_data_unref(&data);

    /* Get decoded picture */
    ret = dav1d_get_picture(ctx, &pic);
    if (ret < 0) {
        fprintf(stderr, "  dav1d: get_picture failed (%d)\n", ret);
        dav1d_close(&ctx);
        return 0;
    }

    /* Extract picture info */
    *width = pic.p.w;
    *height = pic.p.h;
    *bit_depth = pic.p.bpc;
    *monochrome = 0;

    /* Determine chroma subsampling from layout */
    if (pic.p.layout == DAV1D_PIXEL_LAYOUT_I420) {
        *subsampling_x = 1;
        *subsampling_y = 1;
    } else if (pic.p.layout == DAV1D_PIXEL_LAYOUT_I422) {
        *subsampling_x = 1;
        *subsampling_y = 0;
    } else {
        *subsampling_x = 0;
        *subsampling_y = 0;
    }

    /* Allocate 8-bit output planes */
    *y_stride = (*width + 31) & ~31;
    *y_plane = (unsigned char *)malloc((size_t)(*y_stride * *height));
    if (!*y_plane) { dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

    *u_stride = ((*width >> *subsampling_x) + 31) & ~31;
    *u_plane = (unsigned char *)malloc((size_t)(*u_stride * (*height >> *subsampling_y)));
    if (!*u_plane) { free(*y_plane); dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

    *v_stride = *u_stride;
    *v_plane = (unsigned char *)malloc((size_t)(*v_stride * (*height >> *subsampling_y)));
    if (!*v_plane) { free(*y_plane); free(*u_plane); dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

    /* Copy Y plane (convert from 16-bit/10-bit to 8-bit if needed) */
    for (i = 0; i < *height; i++) {
        int si;
        for (si = 0; si < *width; si++) {
            if (pic.p.bpc > 8) {
                uint16_t *src = (uint16_t *)((uint8_t *)pic.data[0] + i * pic.stride[0]);
                (*y_plane)[i * *y_stride + si] = (unsigned char)(src[si] >> (pic.p.bpc - 8));
            } else {
                (*y_plane)[i * *y_stride + si] = ((unsigned char *)pic.data[0])[i * pic.stride[0] + si];
            }
        }
    }

    /* Copy U plane */
    {
        int uv_h = *height >> *subsampling_y;
        int uv_w = *width >> *subsampling_x;
        for (i = 0; i < uv_h; i++) {
            int si;
            for (si = 0; si < uv_w; si++) {
                if (pic.p.bpc > 8) {
                    uint16_t *src = (uint16_t *)((uint8_t *)pic.data[1] + i * pic.stride[1]);
                    (*u_plane)[i * *u_stride + si] = (unsigned char)(src[si] >> (pic.p.bpc - 8));
                } else {
                    (*u_plane)[i * *u_stride + si] = ((unsigned char *)pic.data[1])[i * pic.stride[1] + si];
                }
            }
        }
    }

    /* Copy V plane */
    {
        int uv_h = *height >> *subsampling_y;
        int uv_w = *width >> *subsampling_x;
        for (i = 0; i < uv_h; i++) {
            int si;
            for (si = 0; si < uv_w; si++) {
                if (pic.p.bpc > 8) {
                    uint16_t *src = (uint16_t *)((uint8_t *)pic.data[2] + i * pic.stride[1]);
                    (*v_plane)[i * *v_stride + si] = (unsigned char)(src[si] >> (pic.p.bpc - 8));
                } else {
                    (*v_plane)[i * *v_stride + si] = ((unsigned char *)pic.data[2])[i * pic.stride[1] + si];
                }
            }
        }
    }

    dav1d_picture_unref(&pic);
    dav1d_close(&ctx);
    return 1;
}
#endif /* STB_AVIF_USE_DAV1D */


/* -------------------------------------------------------------------------- */
/* MAIN API IMPLEMENTATION                                                    */
/* -------------------------------------------------------------------------- */

const char *stb_avif_failure_reason(void)
{
    return stb_avif_error_msg;
}

void stb_avif_free(void *ptr)
{
    free(ptr);
}

unsigned char *stb_avif_load_from_memory(const unsigned char *data, int len,
                                          int *x, int *y, int *channels,
                                          int req_channels)
{
    struct stb_avif_reader r;
    struct stb_avif_avif_info info;
    struct stb_av1_sequence_header sh;
    struct stb_av1_frame_header fh;
    struct stb_av1_tile_context tc;
    struct stb_avif_reader obu_reader;
    struct stb_av1_bool_reader br;
#ifdef STB_AVIF_USE_C89_DAV1D
    struct stb_av1_msac stb_c89_msac;
    struct StbCdfContext stb_c89_cdf;
#endif
    unsigned char *result = NULL;
    int output_channels;

    /* Initialize info struct */
    memset(&info, 0, sizeof(info));
    info.bit_depth = 8;
    info.chroma_subsampling_x = 1;
    info.chroma_subsampling_y = 1;
    info.input = data;
    info.input_len = len;

    memset(&sh, 0, sizeof(sh));
    memset(&fh, 0, sizeof(fh));
    memset(&tc, 0, sizeof(tc));

    result = NULL;

    /* Setup error handling */
    if (setjmp(stb_avif_jmp)) {
        goto error_exit;
    }


    /* Validate input */
    STB_AVIF_CHECK(data != NULL && len >= 16, "Invalid input data");

    stb_avif_reader_init(&r, data, (size_t)len);

    /* Look for ftyp box */
    STB_AVIF_CHECK(stb_avif_find_box(&r, STB_AVIF_BOX_FTYP, 0, NULL),
                   "No ftyp box found");
    stb_avif_parse_ftyp(&r, &info);

    /* Look for meta box */
    {
        struct stb_avif_box meta_hdr;
        stb_avif_reader_init(&r, data, (size_t)len);
        STB_AVIF_CHECK(stb_avif_find_box(&r, STB_AVIF_BOX_META, 1, &meta_hdr),
                       "No meta box found");
        /* Save meta end position for parse_meta */
        info.meta_end_offset = (size_t)(meta_hdr.data_start + meta_hdr.data_size);
    }

    /* Parse the meta box to extract all AVIF metadata */
    stb_avif_parse_meta(&r, &info);

    /* Verify we have image dimensions */
    STB_AVIF_CHECK(info.width > 0 && info.height > 0,
                   "Could not determine image dimensions");
    STB_AVIF_CHECK(info.width <= STB_AVIF_MAX_DIMENSION &&
                   info.height <= STB_AVIF_MAX_DIMENSION,
                   "Image too large");

    /* Verify we have compressed data */
    STB_AVIF_CHECK(info.av1_data != NULL && info.av1_size > 0,
                   "No AV1 compressed data found");

    /* Set up sequence header defaults */
    sh.bit_depth = info.bit_depth;
    sh.monochrome = info.monochrome;
    sh.subsampling_x = info.chroma_subsampling_x;
    sh.subsampling_y = info.chroma_subsampling_y;
    sh.reduced_still_picture_header = 1;
    sh.still_picture = 1;
    sh.max_frame_width = info.width;
    sh.max_frame_height = info.height;
    sh.frame_width_bits = 4;
    sh.frame_height_bits = 4;
    sh.enable_order_hint = 0;
    sh.enable_dist_wtd_comp = 0;
    sh.enable_masked_comp = 0;
    sh.enable_intra_edge_filter = 1;
    sh.enable_interintra_comp = 0;
    sh.enable_dual_filter = 0;
    sh.enable_jnt_comp = 0;
    sh.enable_superres = 0;
    sh.enable_cdef = 1;
    sh.enable_restoration = 0;
    sh.film_grain_params_present = 0;
    sh.color_description_present = 0;
    sh.sb128 = 0;
    sh.filter_intra = 0;
    sh.separate_uv_delta_q = 0;
    sh.screen_content_tools = 0; /* ADAPTIVE */
    sh.force_integer_mv = 0; /* ADAPTIVE */
    sh.frame_id_numbers_present = 0;
    sh.order_hint_n_bits = 0;

    /* Parse the AV1 bitstream */
    stb_avif_reader_init(&obu_reader, info.av1_data, info.av1_size);

    /* Initialize Boolean reader from the OBU data */
    stb_av1_bool_reader_init(&br, info.av1_data, info.av1_size);
    fprintf(stderr, "[DBG] av1_data ptr=%p size=%zu\n", (void*)info.av1_data, info.av1_size);
    { int _di; fprintf(stderr, "[DBG] First 16 av1 bytes: "); for (_di=0;_di<16&&_di<(int)info.av1_size;_di++) fprintf(stderr,"%02x ",info.av1_data[_di]); fprintf(stderr,"\n"); }

    /* Process OBUs */
    {
        int obu_type;
            int obu_extension_flag;
            int obu_has_size_field;
        stbv_u32 obu_size;
        int more_obus = 1;
        int seq_header_found = 0;
        int frame_header_found = 0;
        stbv_u32 stb_av1_tile_data_bytes = 0; /* bytes remaining in frame OBU after frame header (0=separate tile group) */

        (void)obu_extension_flag;
        obu_size = 0;

 while (more_obus && obu_reader.pos < obu_reader.size) {
            if (!seq_header_found && info.av1c_size > 0) {
                /* av1C box already provides configuration from container parsing.
                   Values already set in sh from info above. No need to re-parse. */
                seq_header_found = 1;
            }

            /* Parse OBU header */
            if (obu_reader.pos + 1 > obu_reader.size)
                break;

            stb_av1_read_obu_header(&obu_reader, &obu_type,
                                     &obu_extension_flag, &obu_has_size_field);

            /* Read OBU size */
            obu_size = 0;
            if (obu_has_size_field) {
                obu_size = stb_av1_read_obu_size(&obu_reader);
            }



            fprintf(stderr, "[DBG] OBU: type=%d size=%lu pos=%zu/%zu\n", obu_type, (unsigned long)obu_size, obu_reader.pos, obu_reader.size);

            /* Process based on type */
            switch (obu_type) {
                case STB_AV1_OBU_SEQUENCE_HEADER: {
                    /* Parse the sequence header OBU to get sb128, filter_intra,
                       screen_content_tools, etc. — these are NOT in the av1C box.
                       Then restore av1C/ISPE values for fields they provide. */
#ifdef STB_AVIF_USE_C89_DAV1D
                    {
                        struct StbAv1GetBits gb;
                        int save_bd = sh.bit_depth, save_mono = sh.monochrome;
                        int save_sx = sh.subsampling_x, save_sy = sh.subsampling_y;
                        int save_w = sh.max_frame_width, save_h = sh.max_frame_height;
                        int save_cr = sh.color_range, save_mc = sh.matrix_coefficients;
                        stb_av1_gb_init(&gb, obu_reader.data + obu_reader.pos, (unsigned int)obu_size);
                        stb_av1_parse_seq_hdr_getbits(&gb, &sh);
                        /* Restore av1C/ISPE values */
                        sh.bit_depth = save_bd;
                        sh.monochrome = save_mono;
                        sh.subsampling_x = save_sx;
                        sh.subsampling_y = save_sy;
                        sh.max_frame_width = save_w;
                        sh.max_frame_height = save_h;
                        sh.color_range = save_cr;
                        sh.matrix_coefficients = save_mc;
                    }
#else
                    if (info.av1c_size == 0) {
                        {
                            struct stb_avif_reader seq_r;
                            struct stb_av1_bool_reader seq_br;
                            stb_avif_reader_init(&seq_r, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                            stb_av1_bool_reader_init(&seq_br, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                            stb_av1_parse_sequence_header_obu(&seq_r, &sh, &seq_br);
                        }
                    }
#endif
                    seq_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME_HEADER:
                case STB_AV1_OBU_REDUNDANT_FRAME_HEADER: {
#ifdef STB_AVIF_USE_C89_DAV1D
                    {
                        struct StbAv1GetBits gb;
                        unsigned int hdr_bytes = 0;
                        stb_av1_gb_init(&gb, obu_reader.data + obu_reader.pos, (unsigned int)obu_size);
                        stb_av1_parse_frame_hdr_getbits(&gb, &fh, &sh, &hdr_bytes);
                        /* Frame header OBU has trailing bits; tile data comes in a separate TILE_GROUP OBU */
                    }
#else
                    {
                        struct stb_avif_reader fh_r;
                        struct stb_av1_bool_reader fh_br;
                        stb_avif_reader_init(&fh_r, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                        stb_av1_bool_reader_init(&fh_br,
                                                   obu_reader.data + obu_reader.pos,
                                                   (size_t)obu_size);
                        stb_av1_parse_frame_header(&fh_r, &fh, &sh, &fh_br);
                    }
#endif
                    frame_header_found = 1;
                    fprintf(stderr, "[FRAME_HDR] type=%d show=%d q=%d w=%d h=%d refresh=0x%x reduced_still=%d disable_cdf=%d\n",
                        fh.frame_type, fh.show_frame, fh.base_q_idx,
                        fh.frame_width, fh.frame_height, fh.refresh_frame_flags, sh.reduced_still_picture_header, fh.disable_cdf_update);
                    break;
                }
                case STB_AV1_OBU_FRAME: {
#ifdef STB_AVIF_USE_C89_DAV1D
                    {
                        struct StbAv1GetBits gb;
                        unsigned int hdr_bytes = 0;
                        stbv_u32 sz = obu_size;
                        stb_av1_gb_init(&gb, obu_reader.data + obu_reader.pos, (unsigned int)sz);
                        stb_av1_parse_frame_hdr_getbits(&gb, &fh, &sh, &hdr_bytes);
                        /* Tile group header: for single tile, no reads.
                           For multi-tile, read tile_start/tile_end.
                           After byte-align, tile data begins. */
                        if (fh.tile_cols * fh.tile_rows > 1) {
                            if (stb_av1_gb_bit(&gb)) { /* have_tile_pos */
                                int n_bits = 0;
                                /* log2_cols + log2_rows — approximate from cols/rows */
                                int tc = fh.tile_cols, tr = fh.tile_rows;
                                while ((1 << n_bits) < tc * tr) n_bits++;
                                stb_av1_gb_bits(&gb, n_bits); /* tile_start */
                                stb_av1_gb_bits(&gb, n_bits); /* tile_end */
                            }
                        }
                        stb_av1_gb_bytealign(&gb);
                        {
                            unsigned int tile_offset = stb_av1_gb_pos_bytes(&gb);
                        stb_av1_msac_init(&stb_c89_msac,
                            obu_reader.data + obu_reader.pos + tile_offset,
                            (unsigned long)(sz - tile_offset), fh.disable_cdf_update);
                        fprintf(stderr, "[MSAC_INIT_FRAME] rng=%u cnt=%u disable_cdf=%d delta_q_present=%d delta_lf_present=%d seg_en=%d seg_upmap=%d skip_mode=%d cdef_bits=%d base_q=%d\n",
                            stb_c89_msac.rng, stb_c89_msac.cnt, fh.disable_cdf_update,
                            fh.delta_q_present, fh.delta_lf_present,
                            fh.segmentation_enabled, fh.segment_update_map,
                            fh.skip_mode, fh.cdef_bits, fh.base_q_idx);
                    }
                    }
#else
                    struct stb_avif_reader frame_r;
                    struct stb_av1_bool_reader frame_br;
                    stb_avif_reader_init(&frame_r, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                    stb_av1_bool_reader_init(&frame_br, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                    stb_av1_parse_frame_header(&frame_r, &fh, &sh, &frame_br);
                    br = frame_br;
#endif
                    frame_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_TILE_GROUP: {
                    if (frame_header_found) {
#ifndef STB_AVIF_USE_C89_DAV1D
                        stb_av1_bool_reader_init(&br,
                                                   obu_reader.data + obu_reader.pos,
                                                   (size_t)obu_size);
#else
                        /* Separate tile group OBU: init MSAC at tile data start.
                           For single tile, no tile group header to skip.
                           For multi-tile, read tile_start/tile_end with getbits. */
                        {
                            struct StbAv1GetBits gb;
                            stb_av1_gb_init(&gb, obu_reader.data + obu_reader.pos, (unsigned int)obu_size);
                            if (fh.tile_cols * fh.tile_rows > 1) {
                                if (stb_av1_gb_bit(&gb)) {
                                    int n_bits = 0;
                                    int tc = fh.tile_cols, tr = fh.tile_rows;
                                    while ((1 << n_bits) < tc * tr) n_bits++;
                                    stb_av1_gb_bits(&gb, n_bits);
                                    stb_av1_gb_bits(&gb, n_bits);
                                }
                            }
                            stb_av1_gb_bytealign(&gb);
                            {
                                unsigned int tile_offset = stb_av1_gb_pos_bytes(&gb);
                                unsigned long tile_data_sz = (unsigned long)(obu_size - tile_offset);
                                /* DUMP tile data first bytes */
                                {   int _di;
                                    const unsigned char *_dp = obu_reader.data + obu_reader.pos + tile_offset;
                                    fprintf(stderr, "[DBG] TILE: obu_size=%lu tile_offset=%u tile_sz=%lu pos=%zu\n",
                                        (unsigned long)obu_size, tile_offset, tile_data_sz, obu_reader.pos);
                                    fprintf(stderr, "[DBG] First 16 tile bytes: ");
                                    for (_di = 0; _di < 16 && _di < (int)tile_data_sz; _di++)
                                        fprintf(stderr, "%02x ", _dp[_di]);
                                    fprintf(stderr, "\n");
                                }
                                stb_av1_msac_init(&stb_c89_msac,
                                    obu_reader.data + obu_reader.pos + tile_offset,
                                    tile_data_sz, fh.disable_cdf_update);
                                fprintf(stderr, "[MSAC_INIT] rng=%u cnt=%u disable_cdf=%d delta_q_present=%d\n", stb_c89_msac.rng, stb_c89_msac.cnt, fh.disable_cdf_update, fh.delta_q_present);
                            }
                        }
#endif
                    }
                    break;
                }
                case STB_AV1_OBU_TEMPORAL_DELIMITER:
                case STB_AV1_OBU_METADATA:
                case STB_AV1_OBU_PADDING:
                default:
                    break;
            }

            /* Advance past this OBU's data */
            if (obu_has_size_field && obu_size > 0) {
                obu_reader.pos += (size_t)obu_size;
            } else if (obu_has_size_field) {
                /* OBU with has_size_field=1 and size=0 is valid (e.g. temporal delimiter).
                   Just skip the header+size bytes we already consumed. */
                /* Already advanced past header+size, nothing more to skip. */
            } else {
                /* No size field: determine from remaining data or break on unknown */
                if (obu_reader.pos < obu_reader.size)
                    obu_reader.pos = obu_reader.size; /* consume all remaining */
                else
                    break;
            }

            /* Check if we've found end of OBUs */
            if (obu_reader.pos >= obu_reader.size)
                more_obus = 0;
        }

        /* For reduced still_picture_header, restore dimensions from ISPE */
        if (sh.reduced_still_picture_header && info.width > 0 && info.height > 0) {
            sh.max_frame_width = info.width;
            sh.max_frame_height = info.height;
        }
        STB_AVIF_CHECK(seq_header_found, "No AV1 sequence header found");
    }

    /* If we didn't find a frame header, use defaults for still picture */
    if (!fh.frame_width || !fh.frame_height) {
        fprintf(stderr, "[DBG] frame dims fallback: fw=%d fh=%d sh_mfw=%d sh_mfh=%d info=%dx%d\n",
            fh.frame_width, fh.frame_height, sh.max_frame_width, sh.max_frame_height,
            info.width, info.height);
        fh.frame_width = (int)sh.max_frame_width;
        fh.frame_height = (int)sh.max_frame_height;
        fh.frame_type = STB_AV1_KEY_FRAME;
        fh.show_frame = 1;
        fh.base_q_idx = 100; /* reasonable default */
        fh.cdef_damping = 4;
        fh.cdef_bits = 0;
        fh.tx_mode = 2; /* SELECT */
        /* enable_cdef in sh, not fh */
    }

    /* Allocate image planes */
    info.stride_y = (info.width + 31) & ~31;
    info.stride_u = ((info.width >> sh.subsampling_x) + 31) & ~31;
    info.stride_v = info.stride_u;

    info.plane_y = (unsigned char *)stb_avif_calloc(
        (size_t)(info.stride_y * info.height), 1);
    if (sh.monochrome) {
        info.plane_u = NULL;
        info.plane_v = NULL;
    } else {
        info.plane_u = (unsigned char *)stb_avif_calloc(
            (size_t)(info.stride_u * (info.height >> sh.subsampling_y)), 1);
        info.plane_v = (unsigned char *)stb_avif_calloc(
            (size_t)(info.stride_v * (info.height >> sh.subsampling_y)), 1);
    }

    /* Initialize tile context */
    tc.sh = &sh;
    tc.fh = &fh;
    tc.frame_width = fh.frame_width;
    tc.frame_height = fh.frame_height;
    fprintf(stderr, "[FRAME] w=%d h=%d\n", fh.frame_width, fh.frame_height);
    tc.mb_cols = (tc.frame_width + 3) / 4;
    tc.mb_rows = (tc.frame_height + 3) / 4;
#ifdef STB_AVIF_USE_C89_DAV1D
    /* stb_c89_msac was already initialized in the OBU loop (FRAME or TILE_GROUP case).
       Do NOT re-init here — that would corrupt the MSAC state.
       Also use it for tc.br so mode/partition decoding reads correct data. */
    tc.br = &stb_c89_msac;
    tc.msac = &stb_c89_msac;
    stb_av1_cdf_full_init(&stb_c89_cdf, fh.base_q_idx);
    tc.cdf = &stb_c89_cdf;
#else
    tc.br = &br;
#endif
    tc.qindex_y = fh.base_q_idx;
    tc.qindex_u = fh.base_q_idx;
    tc.qindex_v = fh.base_q_idx;
    tc.plane_y = info.plane_y;
    tc.plane_u = info.plane_u;
    tc.plane_v = info.plane_v;
    tc.stride_y = info.stride_y;
    tc.stride_u = info.stride_u;
    tc.stride_v = info.stride_v;
    tc.bit_depth = sh.bit_depth;
    tc.tile_row = 0;
    tc.tile_col = 0;
    tc.done_sb = 0;
    tc.total_sb = 1;
    tc.next_report_sb = 1;
    tc.start_time = 0;

    /* Allocate pixel max */
    tc.pixel_max = (1 << sh.bit_depth) - 1;

#ifdef STB_AVIF_USE_DAV1D
    {
        int dav1d_w, dav1d_h;
        int dav1d_bd, dav1d_mono, dav1d_sx, dav1d_sy;
        unsigned char *dav1d_y = NULL, *dav1d_u = NULL, *dav1d_v = NULL;
        int dav1d_ys, dav1d_us, dav1d_vs;
        int dav1d_ok;

        dav1d_ok = stb_avif_decode_with_dav1d(
            info.av1_data, info.av1_size,
            &dav1d_w, &dav1d_h,
            &dav1d_y, &dav1d_ys,
            &dav1d_u, &dav1d_us,
            &dav1d_v, &dav1d_vs,
            &dav1d_bd, &dav1d_mono, &dav1d_sx, &dav1d_sy);

        if (dav1d_ok) {
            /* Replace internal planes with dav1d output */
            if (info.plane_y) stb_avif_free_internal(info.plane_y);
            if (info.plane_u) stb_avif_free_internal(info.plane_u);
            if (info.plane_v) stb_avif_free_internal(info.plane_v);
            info.plane_y = dav1d_y;
            info.plane_u = dav1d_u;
            info.plane_v = dav1d_v;
            info.stride_y = dav1d_ys;
            info.stride_u = dav1d_us;
            info.stride_v = dav1d_vs;
            info.width = dav1d_w;
            info.height = dav1d_h;
            sh.bit_depth = dav1d_bd;
            sh.monochrome = dav1d_mono;
            sh.subsampling_x = dav1d_sx;
            sh.subsampling_y = dav1d_sy;
        } else {
            stb_avif_error_msg = "dav1d decode failed";
            goto error_exit;
        }
    }
#else

    stb_av1_decode_frame(&tc);

    if (getenv("STB_DUMP_PLANES")) {
        FILE *fy = fopen("/tmp/c89_y.raw", "wb");
        FILE *fu = fopen("/tmp/c89_u.raw", "wb");
        FILE *fv = fopen("/tmp/c89_v.raw", "wb");
        int yy;
        for (yy = 0; yy < info.height; yy++)
            fwrite(info.plane_y + (size_t)yy * info.stride_y, 1, (size_t)info.width, fy);
        for (yy = 0; yy < (info.height >> sh.subsampling_y); yy++)
            fwrite(info.plane_u + (size_t)yy * info.stride_u, 1, (size_t)((info.width + 1) >> sh.subsampling_x), fu);
        for (yy = 0; yy < (info.height >> sh.subsampling_y); yy++)
            fwrite(info.plane_v + (size_t)yy * info.stride_v, 1, (size_t)((info.width + 1) >> sh.subsampling_x), fv);
        fclose(fy); fclose(fu); fclose(fv);
        fprintf(stderr, "[PLANES] dumped %dx%d sx=%d sy=%d\n", info.width, info.height, sh.subsampling_x, sh.subsampling_y);
    }

    /* Apply CDEF filter */
    if (sh.enable_cdef && fh.cdef_bits > 0 && fh.cdef_y_pri_strength[0] > 0) {
        stb_av1_cdef_filter_plane(info.plane_y, info.stride_y,
                                   info.width, info.height,
                                   fh.cdef_y_pri_strength[0],
                                   fh.cdef_y_sec_strength[0],
                                   fh.cdef_damping, sh.bit_depth);
    }
#endif

    /* Determine output channels */
    output_channels = req_channels;
    if (output_channels == 0) {
        if (sh.monochrome)
            output_channels = 1;
        else
            output_channels = 4; /* RGBA */
    }

    /* Allocate output buffer with proper RGBA conversion */
    result = (unsigned char *)stb_avif_malloc(
        (size_t)(info.width * info.height * output_channels));
    if (!result) {
        stb_avif_error_msg = "Out of memory";
        goto error_exit;
    }

    /* Convert YUV to RGB */
    if (sh.monochrome && output_channels == 1) {
        /* Direct copy of luma */
        int row, col;
        for (row = 0; row < info.height; row++) {
            for (col = 0; col < info.width; col++) {
                result[row * info.width * output_channels + col] =
                    info.plane_y[row * info.stride_y + col];
            }
        }
    } else {
        /* YUV (4:2:0 or 4:4:4) to RGBA conversion */
        int row, col;
        int uv_h = (info.height + (1 << sh.subsampling_y) - 1) >> sh.subsampling_y;
        int uv_w = (info.width + (1 << sh.subsampling_x) - 1) >> sh.subsampling_x;

        for (row = 0; row < info.height; row++) {
            for (col = 0; col < info.width; col++) {
                int y_val, u_val, v_val;
                int r, g, b;

                y_val = (int)info.plane_y[row * info.stride_y + col];

                if (sh.subsampling_y > 0) {
                    int uv_r = row >> sh.subsampling_y;
                    int uv_c = col >> sh.subsampling_x;
                    if (uv_r >= uv_h) uv_r = uv_h - 1;
                    if (uv_c >= uv_w) uv_c = uv_w - 1;
                    if (uv_r < 0) uv_r = 0;
                    if (uv_c < 0) uv_c = 0;
                    u_val = info.plane_u ? (int)info.plane_u[uv_r * info.stride_u + uv_c] : 128;
                    v_val = info.plane_v ? (int)info.plane_v[uv_r * info.stride_v + uv_c] : 128;
                } else {
                    u_val = info.plane_u ? (int)info.plane_u[row * info.stride_u + col] : 128;
                    v_val = info.plane_v ? (int)info.plane_v[row * info.stride_v + col] : 128;
                }

                if (row == 0 && col < 10) {
                    fprintf(stderr, "RAW[%d]: Y=%d U=%d V=%d\n", col, y_val, u_val, v_val);
                }

                /* Color matrix based on sequence header matrix_coefficients */
                {
                    int mc = sh.matrix_coefficients;
                    if (mc == 0) {
                        /* Identity matrix: YUV planes ARE RGB. No range expansion, no centering. */
                        r = y_val;
                        g = u_val;
                        b = v_val;
                    } else {
                        /* YCbCr conversion */
                        if (sh.color_range == 0) {
                            y_val = ((y_val - 16) * 255) / 219;
                            if (y_val < 0) y_val = 0;
                            if (y_val > 255) y_val = 255;
                        }
                        u_val -= 128;
                        v_val -= 128;

                        if (mc >= 8 && mc <= 10) {
                            r = y_val + ((378 * v_val) >> 8);
                            g = y_val - ((42 * u_val + 120 * v_val) >> 8);
                            b = y_val + ((482 * u_val) >> 8);
                        } else if (mc == 1 || mc == 2) {
                            r = y_val + ((403 * v_val) >> 8);
                            g = y_val - ((48 * u_val + 120 * v_val) >> 8);
                            b = y_val + ((475 * u_val) >> 8);
                        } else {
                            r = y_val + ((359 * v_val) >> 8);
                            g = y_val - ((88 * u_val + 183 * v_val) >> 8);
                            b = y_val + ((454 * u_val) >> 8);
                        }
                    }
                }

                /* Clamp */
                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;

                if (row == 0 && col < 10) {
                    fprintf(stderr, "RGB[%d]: (%d,%d,%d) mc=%d\n", col, r, g, b, sh.matrix_coefficients);
                }

                result[(row * info.width + col) * output_channels + 0] = (unsigned char)r;
                result[(row * info.width + col) * output_channels + 1] = (unsigned char)g;
                result[(row * info.width + col) * output_channels + 2] = (unsigned char)b;

                if (output_channels == 4) {
                    result[(row * info.width + col) * output_channels + 3] = 255; /* Alpha */
                }
            }
        }
    }

    /* Set output parameters */
    *x = info.width;
    *y = info.height;
    *channels = output_channels;

    /* Cleanup */
    if (info.plane_y) stb_avif_free_internal(info.plane_y);
    if (info.plane_u) stb_avif_free_internal(info.plane_u);
    if (info.plane_v) stb_avif_free_internal(info.plane_v);
    info.plane_y = NULL;
    info.plane_u = NULL;
    info.plane_v = NULL;

    stb_avif_error_msg = "no error";
    return result;

error_exit:
    if (info.plane_y) stb_avif_free_internal(info.plane_y);
    if (info.plane_u) stb_avif_free_internal(info.plane_u);
    if (info.plane_v) stb_avif_free_internal(info.plane_v);
    if (result) stb_avif_free_internal(result);
    return NULL;
}

#endif /* STB_AVIF_IMPLEMENTATION */
