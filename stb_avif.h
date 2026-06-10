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

#ifdef STB_AVIF_USE_C89_DAV1D
/* ===== C89 Internal Decoder: MSAC (Multi-Symbol Arithmetic Coder) ===== */
/* Ported from dav1d. Reads XOR-inverted bytes as AV1 specifies. */

struct stb_av1_msac {
    const unsigned char *buf_pos, *buf_end;
    unsigned long long dif;
    unsigned rng, cnt;
    int allow_update_cdf;
};

static void stb_av1_msac_refill(struct stb_av1_msac *s) {
    const unsigned char *p = s->buf_pos, *e = s->buf_end;
    int c = 64 - (int)s->cnt - 24;
    unsigned long long d = s->dif;
    do { if (p >= e) { d |= ~(~(unsigned long long)0xff << c); break; }
         d |= (unsigned long long)(*p++ ^ 0xff) << c; c -= 8; } while (c >= 0);
    s->dif = d; s->cnt = (unsigned)(64 - c - 24);
    s->buf_pos = p;
}

static void stb_av1_msac_norm(struct stb_av1_msac *s, unsigned long long d, unsigned r) {
    int d2 = 0;
    if (r & 0xff00) { d2 = 8; r >>= 8; }
    if (r & 0xf0) { d2 += 4; r >>= 4; }
    if (r & 0xc)  { d2 += 2; r >>= 2; }
    if (r & 0x2)  { d2 += 1; r >>= 1; }
    d2 = 15 - d2;
    s->dif = d << d2; s->rng = r << d2;
    s->cnt -= d2;
    if ((unsigned)s->cnt < (unsigned)d2) stb_av1_msac_refill(s);
}

static void stb_av1_msac_init(struct stb_av1_msac *s, const unsigned char *data, unsigned long sz, int no_cdf) {
    s->buf_pos = data; s->buf_end = data + sz;
    s->dif = 0; s->rng = 0x8000; s->cnt = -15;
    s->allow_update_cdf = !no_cdf;
    stb_av1_msac_refill(s);
}

static unsigned stb_av1_msac_decode_bool_equi(struct stb_av1_msac *s) {
    unsigned r = s->rng, v = ((r >> 8) << 7) + 4;
    unsigned long long d = s->dif, vw = (unsigned long long)v << 48;
    unsigned ret = (d >= vw) ? 1u : 0u;
    if (ret) d -= vw; v += ret * (r - 2 * v);
    stb_av1_msac_norm(s, d, v); return !ret;
}

static unsigned stb_av1_msac_decode_bool(struct stb_av1_msac *s, unsigned f) {
    unsigned r = s->rng, v = ((r >> 8) * (f >> 6) >> 1) + 4;
    unsigned long long d = s->dif, vw = (unsigned long long)v << 48;
    unsigned ret = (d >= vw) ? 1u : 0u;
    if (ret) d -= vw; v += ret * (r - 2 * v);
    stb_av1_msac_norm(s, d, v); return !ret;
}

static unsigned stb_av1_msac_decode_bools(struct stb_av1_msac *s, unsigned n) {
    unsigned v = 0;
    while (n--) v = (v << 1) | stb_av1_msac_decode_bool_equi(s);
    return v;
}

static unsigned stb_av1_msac_decode_symbol(struct stb_av1_msac *s, unsigned short *cdf, unsigned long nsym) {
    unsigned c = (unsigned)(s->dif >> 48), r = s->rng >> 8;
    unsigned u, v = s->rng, val = 0;
    do { u = v; v = r * (cdf[val] >> 6); v >>= 1;
         v += 4 * ((unsigned)nsym - val); val++; } while (c < v && val < nsym);
    val--;
    stb_av1_msac_norm(s, s->dif - ((unsigned long long)v << 48), u - v);
    if (s->allow_update_cdf) {
        unsigned cnt = cdf[nsym], rate = 4 + (cnt >> 4) + (nsym > 2 ? 1u : 0u);
        unsigned i;
        for (i = 0; i < val; i++) cdf[i] += (unsigned short)((32768 - cdf[i]) >> rate);
        for (; i < nsym; i++) cdf[i] -= (unsigned short)(cdf[i] >> rate);
        cdf[nsym] = (unsigned short)(cnt + (cnt < 32 ? 1u : 0u));
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
        tok_br = stb_av1_msac_decode_symbol(s, cdf, 3); tok = 6 + tok_br;
        if (tok_br == 3) {
            tok_br = stb_av1_msac_decode_symbol(s, cdf, 3); tok = 9 + tok_br;
            if (tok_br == 3) tok = 12 + stb_av1_msac_decode_symbol(s, cdf, 3);
        }
    }
    return tok;
}

/* Default CDF tables (intra-mode, skip, coefficient EOB) */
static const unsigned short stb_av1_cdf_intra_y[13] = {
    15360, 17920, 20480, 23040, 25600, 28160, 30720, 32000,
    33280, 34560, 35840, 36608, 32768
};
static const unsigned short stb_av1_cdf_skip[3] = { 28160, 32768, 0 };
static const unsigned short stb_av1_cdf_eob4x4[65] = {
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
    unsigned short cdf[13]; int i;
    for (i = 0; i < 13; i++) cdf[i] = stb_av1_cdf_intra_y[i];
    return (int)stb_av1_msac_decode_symbol(msac, cdf, 12);
}

/* CDF-based coefficient decoder (replaces old uniform decoder) */
static int stb_av1_decode_coeffs_cdf(struct stb_av1_msac *msac, int *coeffs, int tx_w, int tx_h, int *eob) {
    int max_coeffs = tx_w * tx_h, i;
    unsigned short eob_cdf[65], sk_cdf[3], cf_cdf[3], tk_cdf[4];
    for (i = 0; i < 65; i++) eob_cdf[i] = stb_av1_cdf_eob4x4[i];
    sk_cdf[0]=28160; sk_cdf[1]=32768; sk_cdf[2]=0;
    cf_cdf[0]=24576; cf_cdf[1]=32768; cf_cdf[2]=0;
    tk_cdf[0]=12288; tk_cdf[1]=20480; tk_cdf[2]=28672; tk_cdf[3]=32768;

    if (stb_av1_msac_decode_bool_adapt(msac, sk_cdf)) {
        for (i = 0; i < max_coeffs; i++) coeffs[i] = 0;
        *eob = 0; return 0;
    }
    *eob = (int)stb_av1_msac_decode_symbol(msac, eob_cdf, (unsigned long)max_coeffs);
    if (*eob < 1) *eob = 1;
    for (i = 0; i < max_coeffs; i++) {
        if (i < *eob) {
            if (stb_av1_msac_decode_bool_adapt(msac, cf_cdf)) {
                int sign = stb_av1_msac_decode_bool_equi(msac) ? -1 : 1;
                unsigned tok = stb_av1_msac_decode_symbol(msac, tk_cdf, 3);
                int mag = 1 + (int)tok;
                if (tok == 3) { mag = 1 + (int)stb_av1_msac_decode_hi_tok(msac, tk_cdf); }
                coeffs[i] = sign * mag;
            } else { coeffs[i] = 0; }
        } else { coeffs[i] = 0; }
    }
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
#define stb_av1_decode_coeffs(br,c,m,e,q) stb_av1_decode_coeffs_cdf(br,c,8,8,e)

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
        int delta_q_present = 0;
        int delta_q_res = 0;
        if (fh->primary_ref_frame != 7 || fh->frame_type == STB_AV1_KEY_FRAME || fh->frame_type == STB_AV1_INTRA_ONLY) {
            delta_q_present = stb_av1_bool_decode(br, 128);
            if (delta_q_present) {
                delta_q_res = (int)stb_av1_bool_decode_literal(br, 2) + 1;
                (void)delta_q_res;
            }
        }
        if (delta_q_present) {
            int delta_lf_present = stb_av1_bool_decode(br, 128);
            (void)delta_lf_present;
            if (delta_lf_present) {
                int delta_lf_multi = stb_av1_bool_decode(br, 128);
                (void)delta_lf_multi;
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

    /* Bit depth */
    int bit_depth;
    int pixel_max;
};

/* DC dequant lookup table (simplified version for 8-bit) */
static const int stb_av1_dc_qlookup[256] = {
    4,    8,    8,    9,    10,   11,   12,   13,
    14,   15,   16,   17,   18,   19,   20,   21,
    22,   23,   24,   25,   26,   27,   28,   29,
    30,   31,   32,   33,   34,   35,   36,   37,
    38,   39,   40,   41,   42,   43,   44,   45,
    46,   47,   48,   49,   50,   51,   52,   53,
    54,   55,   56,   57,   58,   59,   60,   61,
    62,   63,   64,   65,   66,   67,   68,   69,
    70,   71,   72,   73,   74,   75,   76,   77,
    78,   79,   80,   81,   82,   83,   84,   85,
    86,   87,   88,   89,   90,   91,   92,   93,
    94,   95,   96,   97,   98,   99,   100,  101,
    102,  103,  104,  105,  106,  107,  108,  109,
    110,  111,  112,  113,  114,  115,  116,  117,
    118,  119,  120,  121,  122,  123,  124,  125,
    126,  127,  128,  129,  130,  131,  132,  133,
    134,  135,  136,  137,  138,  139,  140,  141,
    142,  143,  144,  145,  146,  147,  148,  149,
    150,  151,  152,  153,  154,  155,  156,  157,
    158,  159,  160,  161,  162,  163,  164,  165,
    166,  167,  168,  169,  170,  171,  172,  173,
    174,  175,  176,  177,  178,  179,  180,  181,
    182,  183,  184,  185,  186,  187,  188,  189,
    190,  191,  192,  193,  194,  195,  196,  197,
    198,  199,  200,  201,  202,  203,  204,  205,
    206,  207,  208,  209,  210,  211,  212,  213,
    214,  215,  216,  217,  218,  219,  220,  221,
    222,  223,  224,  225,  226,  227,  228,  229,
    230,  231,  232,  233,  234,  235,  236,  237,
    238,  239,  240,  241,  242,  243,  244,  245,
    246,  247,  248,  249,  250,  251,  252,  253,
    254,  255,  256,  257,  258,  259,  260,  261
};

/* AC dequant lookup table */
static const int stb_av1_ac_qlookup[256] = {
    4,    8,    9,    10,   11,   12,   13,   14,
    15,   16,   17,   18,   19,   20,   21,   22,
    23,   24,   25,   26,   27,   28,   29,   30,
    31,   32,   33,   34,   35,   36,   37,   38,
    39,   40,   41,   42,   43,   44,   45,   46,
    47,   48,   49,   50,   51,   52,   53,   54,
    55,   56,   57,   58,   59,   60,   61,   62,
    63,   64,   65,   66,   67,   68,   69,   70,
    71,   72,   73,   74,   75,   76,   77,   78,
    79,   80,   81,   82,   83,   84,   85,   86,
    87,   88,   89,   90,   91,   92,   93,   94,
    95,   96,   97,   98,   99,   100,  101,  102,
    103,  104,  105,  106,  107,  108,  109,  110,
    111,  112,  113,  114,  115,  116,  117,  118,
    119,  120,  121,  122,  123,  124,  125,  126,
    127,  128,  129,  130,  131,  132,  133,  134,
    135,  136,  137,  138,  139,  140,  141,  142,
    143,  144,  145,  146,  147,  148,  149,  150,
    151,  152,  153,  154,  155,  156,  157,  158,
    159,  160,  161,  162,  163,  164,  165,  166,
    167,  168,  169,  170,  171,  172,  173,  174,
    175,  176,  177,  178,  179,  180,  181,  182,
    183,  184,  185,  186,  187,  188,  189,  190,
    191,  192,  193,  194,  195,  196,  197,  198,
    199,  200,  201,  202,  203,  204,  205,  206,
    207,  208,  209,  210,  211,  212,  213,  214,
    215,  216,  217,  218,  219,  220,  221,  222,
    223,  224,  225,  226,  227,  228,  229,  230,
    231,  232,  233,  234,  235,  236,  237,  238,
    239,  240,  241,  242,  243,  244,  245,  246,
    247,  248,  249,  250,  251,  252,  253,  254,
    255,  256,  257,  258,  259,  260,  261,  262
};

/* Get dequant value for given quantization index and is_dc flag.
   Simplified: uses DC table for DC, AC table for AC. */
static int stb_av1_get_dequant(int qindex, int is_dc, int bit_depth)
{
    (void)bit_depth;
    if (qindex > 255) qindex = 255;
    if (qindex < 0) qindex = 0;
    if (is_dc)
        return stb_av1_dc_qlookup[qindex];
    else
        return stb_av1_ac_qlookup[qindex];
}

/* -------------------------------------------------------------------------- */
/* 1D DCT and ADST transforms                                                */
/* -------------------------------------------------------------------------- */

/* DCT II transform (type II DCT) for 1D array of size n.
   In-place. n is 4, 8, 16, or 32. */
static void stb_av1_dct(int *coeffs, int n)
{
    int i, k;
    int *tmp;
    double pi = 3.14159265358979323846;

    /* Use heap allocation to avoid C89 VLA issues */
    tmp = (int *)stb_avif_malloc((size_t)n * sizeof(int));
    if (!tmp) return;

    for (k = 0; k < n; k++) {
        double sum = 0.0;
        for (i = 0; i < n; i++) {
            double angle = pi * (double)(2 * i + 1) * (double)k / (double)(2 * n);
            sum += (double)coeffs[i] * cos(angle);
        }
        if (k == 0)
            tmp[k] = (int)(sum * (1.0 / sqrt((double)n)) + 0.5);
        else
            tmp[k] = (int)(sum * (sqrt(2.0 / (double)n)) + 0.5);
    }

    for (i = 0; i < n; i++)
        coeffs[i] = tmp[i];

    stb_avif_free_internal(tmp);
}

/* Inverse DCT II (type III DCT) */
static void stb_av1_idct(int *coeffs, int n)
{
    int i, k;
    int *tmp;
    double pi = 3.14159265358979323846;

    tmp = (int *)stb_avif_malloc((size_t)n * sizeof(int));
    if (!tmp) return;

    for (k = 0; k < n; k++) {
        double sum = 0.0;
        double sqrt2_n = sqrt(2.0 / (double)n);
        double sqrt_n = 1.0 / sqrt((double)n);
        for (i = 0; i < n; i++) {
            double angle = pi * (double)(2 * k + 1) * (double)i / (double)(2 * n);
            double norm = (i == 0) ? sqrt_n : sqrt2_n;
            sum += (double)coeffs[i] * norm * cos(angle);
        }
        tmp[k] = (int)(sum + 0.5);
    }

    for (i = 0; i < n; i++)
        coeffs[i] = tmp[i];

    stb_avif_free_internal(tmp);
}

/* ADST (asymmetric discrete sine transform) type IV.
   Used in AV1 for intra prediction residuals. */
static void stb_av1_adst(int *coeffs, int n)
{
    int i, k;
    int *tmp;
    double pi = 3.14159265358979323846;

    tmp = (int *)stb_avif_malloc((size_t)n * sizeof(int));
    if (!tmp) return;

    for (k = 0; k < n; k++) {
        double sum = 0.0;
        for (i = 0; i < n; i++) {
            double angle = pi * (double)(2 * i + 1) * (double)(2 * k + 1) / (double)(4 * n);
            sum += (double)coeffs[i] * sin(angle);
        }
        tmp[k] = (int)(sum * (2.0 / sqrt((double)(2 * n))) + 0.5);
    }

    for (i = 0; i < n; i++)
        coeffs[i] = tmp[i];

    stb_avif_free_internal(tmp);
}

/* Inverse ADST */
static void stb_av1_iadst(int *coeffs, int n)
{
    int i, k;
    int *tmp;
    double pi = 3.14159265358979323846;

    tmp = (int *)stb_avif_malloc((size_t)n * sizeof(int));
    if (!tmp) return;

    for (k = 0; k < n; k++) {
        double sum = 0.0;
        double norm = 2.0 / sqrt((double)(2 * n));
        for (i = 0; i < n; i++) {
            double angle = pi * (double)(2 * k + 1) * (double)(2 * i + 1) / (double)(4 * n);
            sum += (double)coeffs[i] * norm * sin(angle);
        }
        tmp[k] = (int)(sum + 0.5);
    }

    for (i = 0; i < n; i++)
        coeffs[i] = tmp[i];

    stb_avif_free_internal(tmp);
}

/* Identity transform (no-op) */
static void stb_av1_identity(int *coeffs, int n)
{
    /* Identity does nothing */
    (void)coeffs;
    (void)n;
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

    /* For simplicity, handle common types: DCT_DCT, ADST_DCT, DCT_ADST, ADST_ADST */
    is_dct_row = (tx_type == 0 || tx_type == 1);
    is_dct_col = (tx_type == 0 || tx_type == 2);
    is_adst_row = (tx_type == 2 || tx_type == 3 || tx_type == 7 || tx_type == 8);
    is_adst_col = (tx_type == 1 || tx_type == 3 || tx_type == 4 || tx_type == 6);
    is_flipadst_row = (tx_type == 4 || tx_type == 6 || tx_type == 8);
    is_flipadst_col = (tx_type == 5 || tx_type == 6 || tx_type == 7);
    (void)is_flipadst_row;
    (void)is_flipadst_col;

    /* Allocate temp arrays */
    temp = (int *)stb_avif_malloc((size_t)(w * h) * sizeof(int));
    col = (int *)stb_avif_malloc((size_t)(h) * sizeof(int));

    if (!temp || !col) {
        if (temp) stb_avif_free_internal(temp);
        if (col) stb_avif_free_internal(col);
        return;
    }

    /* Process rows */
    for (i = 0; i < h; i++) {
        int row[64];
        for (j = 0; j < w; j++)
            row[j] = block[i * w + j];

        if (is_dct_row) {
            stb_av1_idct(row, w);
        } else if (is_adst_row) {
            stb_av1_iadst(row, w);
        } else {
            stb_av1_identity(row, w);
        }

        for (j = 0; j < w; j++)
            temp[i * w + j] = row[j];
    }

    /* Process columns */
    for (j = 0; j < w; j++) {
        for (i = 0; i < h; i++)
            col[i] = temp[i * w + j];

        if (is_dct_col) {
            stb_av1_idct(col, h);
        } else if (is_adst_col) {
            stb_av1_iadst(col, h);
        } else {
            stb_av1_identity(col, h);
        }

        for (i = 0; i < h; i++)
            block[i * w + j] = col[i];
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
static void stb_av1_intra_predict(unsigned char *dst, int stride,
                                   int w, int h, int mode,
                                   const unsigned char *above,
                                   const unsigned char *left,
                                   unsigned char topleft,
                                   int bit_depth)
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
            int above_avail = 1;
            int left_avail = 1;

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
                        a0 = left[r - 1];
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
                        a0 = left[r - 1];
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
                                       int *coeffs, int tx_w, int tx_h,
                                       int tx_type,
                                       unsigned char *pred,
                                       int pred_stride,
                                       unsigned char *dst,
                                       int dst_stride)
{
    int i, j;
    int dequant_dc, dequant_ac;
    int *dq_coeffs;
    int max_coeffs = tx_w * tx_h;

    if (max_coeffs > 4096)
        return;

    dq_coeffs = (int *)stb_avif_malloc((size_t)max_coeffs * sizeof(int));
    if (!dq_coeffs) return;

    dequant_dc = stb_av1_get_dequant(tc->qindex_y, 1, tc->bit_depth);
    dequant_ac = stb_av1_get_dequant(tc->qindex_y, 0, tc->bit_depth);

    /* Dequantize */
    for (i = 0; i < max_coeffs; i++) {
        if (i == 0)
            dq_coeffs[i] = coeffs[i] * dequant_dc;
        else
            dq_coeffs[i] = coeffs[i] * dequant_ac;
    }

    /* Apply inverse transform */
    stb_av1_inv_transform_2d(dq_coeffs, tx_w, tx_h, tx_type);

    /* Reconstruct: pred + residual, clamp to [0, 255] */
    for (i = 0; i < tx_h; i++) {
        for (j = 0; j < tx_w; j++) {
            int val = (int)pred[i * pred_stride + j] + dq_coeffs[i * tx_w + j];
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            dst[i * dst_stride + j] = (unsigned char)val;
        }
    }

    stb_avif_free_internal(dq_coeffs);
}

/* -------------------------------------------------------------------------- */
/* SIMPLIFIED SUPERBLOCK AND BLOCK DECODING                                   */
/* -------------------------------------------------------------------------- */

/* Decode a superblock (64x64 or 128x128) */
static void stb_av1_decode_superblock(struct stb_av1_tile_context *tc,
                                       int sb_r, int sb_c, int sb_size)
{
    int y, x;
    int block_size = 8;
    int blk_limit = ((tc->frame_width + 7) / 8) * ((tc->frame_height + 7) / 8);

    if (blk_limit < 1) blk_limit = 1;
    /* Safety limit: process at most once the expected block count */
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

            blk_limit--;

            /* Skip blocks outside the frame */
            if (abs_r >= tc->frame_height || abs_c >= tc->frame_width)
                continue;

            /* For intra-only frames, decode intra prediction mode.
               Simplified: read a uniform mode index */
            if (tc->fh->frame_type == STB_AV1_KEY_FRAME ||
                tc->fh->frame_type == STB_AV1_INTRA_ONLY) {
                /* Read intra Y mode with simplified uniform coding */
                if (blk_w <= 8 && blk_h <= 8) {
                    /* All intra modes available */
                    pred_mode = stb_av1_decode_uniform(tc->br, STB_AV1_INTRA_MODES);
                } else {
                    /* Larger blocks: subset of modes */
                    pred_mode = stb_av1_decode_uniform(tc->br, STB_AV1_INTRA_MODES);
                }
            } else {
                pred_mode = STB_AV1_DC_PRED;
            }

            /* Gather boundary pixels for intra prediction */
            {
                int i;
                for (i = 0; i < blk_w; i++) {
                    if (abs_r > 0)
                        above_data[i] = tc->plane_y[(abs_r - 1) * tc->stride_y + abs_c + i];
                    else
                        above_data[i] = 127; /* border extension */
                }
                for (i = 0; i < blk_h; i++) {
                    if (abs_c > 0)
                        left_data[i] = tc->plane_y[(abs_r + i) * tc->stride_y + abs_c - 1];
                    else
                        left_data[i] = 127;
                }
            }
            topleft_pixel = (abs_r > 0 && abs_c > 0)
                ? tc->plane_y[(abs_r - 1) * tc->stride_y + abs_c - 1]
                : (unsigned char)127;

            /* Generate intra prediction */
            {
                unsigned char pred_buf[128]; /* max 8x8 */
                stb_av1_intra_predict(pred_buf, blk_w,
                                       blk_w, blk_h, pred_mode,
                                       above_data, left_data, topleft_pixel,
                                       tc->bit_depth);

                /* Decode transform coefficients */
#ifdef STB_AVIF_USE_C89_DAV1D
                stb_av1_decode_coeffs_cdf(tc->msac, coeffs, blk_w, blk_h, &eob);
#else
                stb_av1_decode_coeffs(tc->br, coeffs, blk_w * blk_h, &eob, tc->qindex_y);
#endif

                /* Reconstruct */
                stb_av1_reconstruct_block(tc, coeffs,
                                           tx_w, tx_h, tx_type,
                                           pred_buf, blk_w,
                                           tc->plane_y + abs_r * tc->stride_y + abs_c,
                                           tc->stride_y);
            }

            /* For chroma, use DC mode with no residual (simplified) */
            if (!tc->sh->monochrome) {
                int u_r = abs_r >> tc->sh->subsampling_y;
                int u_c = abs_c >> tc->sh->subsampling_x;
                int u_w = blk_w >> tc->sh->subsampling_x;
                int u_h = blk_h >> tc->sh->subsampling_y;
                int uv_pred_mode;
                unsigned char u_above_byte[64], u_left_byte[64];
                int i2;
                unsigned char uv_topleft;
                unsigned char pred_uv[64];

                if (u_w < 1) u_w = 1;
                if (u_h < 1) u_h = 1;

                /* Simplified UV prediction mode */
                uv_pred_mode = STB_AV1_DC_PRED;
                if (u_w <= 8 && u_h <= 8) {
                    /* Chroma might have its own mode, simplified */
                }

                /* Gather UV boundary */
                for (i2 = 0; i2 < u_w; i2++) {
                    if (u_r > 0 && u_c + i2 < (tc->frame_width >> tc->sh->subsampling_x)) {
                        u_above_byte[i2] = tc->plane_u[(u_r - 1) * tc->stride_u + u_c + i2];
                    } else {
                        u_above_byte[i2] = 128;
                    }
                }
                for (i2 = 0; i2 < u_h; i2++) {
                    if (u_c > 0 && u_r + i2 < (tc->frame_height >> tc->sh->subsampling_y)) {
                        u_left_byte[i2] = tc->plane_u[(u_r + i2) * tc->stride_u + u_c - 1];
                    } else {
                        u_left_byte[i2] = 128;
                    }
                }
                uv_topleft = (u_r > 0 && u_c > 0)
                    ? tc->plane_u[(u_r - 1) * tc->stride_u + u_c - 1]
                    : (unsigned char)128;

                stb_av1_intra_predict(pred_uv, u_w, u_w, u_h, uv_pred_mode,
                                       u_above_byte, u_left_byte, uv_topleft,
                                       tc->bit_depth);

                /* Copy UV prediction (no residual) */
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
        }
    }
}

/* main tile decoding routine */
static void stb_av1_decode_frame(struct stb_av1_tile_context *tc)
{
    int sb_size = 64; /* superblock size: 64 or 128 depending on sequence */
    int sb_cols, sb_rows;
    int sr, sc;

    /* Determine superblock size */
    if (tc->frame_width > 64 || tc->frame_height > 64)
        sb_size = 64;
    if (tc->frame_width > 128 || tc->frame_height > 128)
        sb_size = 128;

    sb_cols = (tc->frame_width + sb_size - 1) / sb_size;
    sb_rows = (tc->frame_height + sb_size - 1) / sb_size;

    /* Init progress tracking */
    tc->total_sb = sb_cols * sb_rows;
    tc->done_sb = 0;
    tc->next_report_sb = tc->total_sb / 20;  /* report every 5% */
    if (tc->next_report_sb < 1) tc->next_report_sb = 1;
    tc->start_time = time(NULL);

    /* Decode each superblock */
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

/* -------------------------------------------------------------------------- */
/* CDEF FILTER (Constrained Directional Enhancement Filter)                   */
/* -------------------------------------------------------------------------- */

static void stb_av1_cdef_filter_plane(unsigned char *plane, int stride,
                                       int width, int height,
                                       int pri_strength, int sec_strength,
                                       int damping, int bit_depth)
{
    int y, x;
    int dummy_sd;

    (void)bit_depth;
    (void)damping;
    (void)sec_strength;
    dummy_sd = damping + bit_depth - 8;
    (void)dummy_sd;

    if (pri_strength == 0 && sec_strength == 0)
        return;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            int c = plane[y * stride + x];
            int sum_pri = 0;
            int sum_sec = 0;
            int count_pri = 0;
            int count_sec = 0;
            int sign;

            /* Simplified CDEF: compute directional filter */
            /* Primary taps (directional) */
            sign = (c > 128) ? 1 : -1; /* simplified direction detection */
            (void)sign;

            /* For each direction, compute constraint filter.
               Simplified: apply a basic low-pass filter. */
            if (pri_strength > 0) {
                int p0 = plane[(y-1) * stride + x];
                int p1 = plane[(y+1) * stride + x];
                int p2 = plane[y * stride + x-1];
                int p3 = plane[y * stride + x+1];

                /* Compute difference and constrain */
                {
                    int diff;
                    int tap;
                    diff = p0 - c;
                    tap = diff >= 0 ? diff : -diff;
                    if (tap < pri_strength) { sum_pri += diff; count_pri++; }
                    diff = p1 - c;
                    tap = diff >= 0 ? diff : -diff;
                    if (tap < pri_strength) { sum_pri += diff; count_pri++; }
                    diff = p2 - c;
                    tap = diff >= 0 ? diff : -diff;
                    if (tap < pri_strength) { sum_pri += diff; count_pri++; }
                    diff = p3 - c;
                    tap = diff >= 0 ? diff : -diff;
                    if (tap < pri_strength) { sum_pri += diff; count_pri++; }
                }
            }

            /* Apply filter */
            if (count_pri > 0) {
                int new_val = c + (sum_pri / count_pri);
                if (new_val < 0) new_val = 0;
                if (new_val > 255) new_val = 255;
                plane[y * stride + x] = (unsigned char)new_val;
            }
        }
    }
}


#ifdef STB_AVIF_USE_C89_DAV1D
/* MSAC-based sequence header parser */
static void stb_av1_parse_seq_hdr_msac(struct stb_av1_msac *msac,
                                        struct stb_av1_sequence_header *sh) {
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
        sh->enable_restoration = (int)stb_av1_msac_decode_bool_equi(msac); }
}

/* MSAC-based frame header parser (intra-only) */
static void stb_av1_parse_frame_hdr_msac(struct stb_av1_msac *msac,
                                          struct stb_av1_frame_header *fh,
                                          struct stb_av1_sequence_header *sh) {
    (void)sh;
    fh->show_existing_frame = (int)stb_av1_msac_decode_bool_equi(msac);
    if (fh->show_existing_frame) { stb_av1_msac_decode_bools(msac, 3); return; }
    fh->frame_type = (int)stb_av1_msac_decode_bools(msac, 2);
    fh->show_frame = (int)stb_av1_msac_decode_bool_equi(msac);
    fh->error_resilient_mode = (int)stb_av1_msac_decode_bool_equi(msac);
    if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
        fh->disable_cdf_update = (int)stb_av1_msac_decode_bool_equi(msac);
        fh->allow_screen_content_tools = (int)stb_av1_msac_decode_bool_equi(msac);
        if (fh->allow_screen_content_tools) fh->force_integer_mv = (int)stb_av1_msac_decode_bool_equi(msac); }
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
    fh->base_q_idx = (int)stb_av1_msac_decode_bools(msac, 8);
    { int yd = stb_av1_msac_decode_bool_equi(msac) ? (int)stb_av1_msac_decode_subexp(msac, 0, 33, 4) : 0;
      fh->delta_q_y_dc = yd > 16 ? yd - 32 : yd; }
    fh->delta_q_u_dc=0; fh->delta_q_u_ac=0; fh->delta_q_v_dc=0; fh->delta_q_v_ac=0;
    fh->using_qmatrix = (int)stb_av1_msac_decode_bool_equi(msac);
    if (fh->using_qmatrix) { fh->qm_y = (int)stb_av1_msac_decode_bools(msac, 4);
        fh->qm_u = (int)stb_av1_msac_decode_bools(msac, 4);
        fh->qm_v = (int)stb_av1_msac_decode_bools(msac, 4); }
    fh->segmentation_enabled = (int)stb_av1_msac_decode_bool_equi(msac);
    if (fh->segmentation_enabled) {
        fh->segment_update_map = (int)stb_av1_msac_decode_bool_equi(msac);
        if (fh->seg_temporal || fh->segment_update_map)
            fh->seg_id_pre_skip = (int)stb_av1_msac_decode_bool_equi(msac); }
    if (fh->primary_ref_frame != 7 || fh->frame_type == 0 || fh->frame_type == 2)
        if (stb_av1_msac_decode_bool_equi(msac)) stb_av1_msac_decode_bools(msac, 2);
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
                if (fh->lr_type[i]) fh->lr_unit_size[i] = (int)stb_av1_msac_decode_bool_equi(msac) + 1; } } }
    { int tcl = 0, trl = 0;
      if (!sh->reduced_still_picture_header && !fh->error_resilient_mode) {
          if (stb_av1_msac_decode_bool_equi(msac)) tcl = (int)stb_av1_msac_decode_bools(msac, 2);
          if (stb_av1_msac_decode_bool_equi(msac)) trl = (int)stb_av1_msac_decode_bools(msac, 2); }
      (void)tcl; (void)trl; }
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

    /* Parse the AV1 bitstream */
    stb_avif_reader_init(&obu_reader, info.av1_data, info.av1_size);

    /* Initialize Boolean reader from the OBU data */
    stb_av1_bool_reader_init(&br, info.av1_data, info.av1_size);

    /* Process OBUs */
    {
        int obu_type;
            int obu_extension_flag;
            int obu_has_size_field;
        stbv_u32 obu_size;
        int more_obus = 1;
        int seq_header_found = 0;
        int frame_header_found = 0;

        (void)obu_extension_flag;
        obu_size = 0;

while (more_obus && obu_reader.pos < obu_reader.size) {
            if (!seq_header_found) {
                /* Before we read OBUs, we may need to use the config OBU from av1C */
                                if (info.av1c_size > 0 && !seq_header_found) {
                    /* Parse sequence header from av1C config OBUs */
                    struct stb_avif_reader config_r;
                    int config_obu_type, config_obu_ext, config_obu_hassize;
                    stbv_u32 config_obu_sz;

                    stb_avif_reader_init(&config_r, info.av1c_data, (size_t)info.av1c_size);
                    stb_av1_read_obu_header(&config_r, &config_obu_type,
                                             &config_obu_ext, &config_obu_hassize);
                    if (config_obu_hassize)
                        config_obu_sz = stb_av1_read_obu_size(&config_r);
                    else
                        config_obu_sz = (stbv_u32)(info.av1c_size - (size_t)(config_r.pos));

if (config_obu_type == STB_AV1_OBU_SEQUENCE_HEADER && config_obu_sz > 0) {
#ifdef STB_AVIF_USE_C89_DAV1D
                        {
                            struct stb_av1_msac _ms;
                            stb_av1_msac_init(&_ms, config_r.data + config_r.pos, (unsigned long)config_obu_sz, 1);
                            stb_av1_parse_seq_hdr_msac(&_ms, &sh);
                        }
#else
                        struct stb_avif_reader seq_config_r;
                        struct stb_av1_bool_reader seq_config_br;
                        stb_avif_reader_init(&seq_config_r,
                                              config_r.data + config_r.pos,
                                              (size_t)config_obu_sz);
                        stb_av1_bool_reader_init(&seq_config_br,
                                                   config_r.data + config_r.pos,
                                                   (size_t)config_obu_sz);
                        stb_av1_parse_sequence_header_obu(&seq_config_r, &sh, &seq_config_br);
#endif
                        seq_header_found = 1;
                    }
                }
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

            /* Process based on type */
            switch (obu_type) {
                case STB_AV1_OBU_SEQUENCE_HEADER: {
#ifdef STB_AVIF_USE_C89_DAV1D
                    {
                        struct stb_av1_msac _ms;
                        stb_av1_msac_init(&_ms, obu_reader.data + obu_reader.pos, (unsigned long)obu_size, 1);
                        stb_av1_parse_seq_hdr_msac(&_ms, &sh);
                    }
#else
                    {
                        struct stb_avif_reader seq_r;
                        struct stb_av1_bool_reader seq_br;
                        stb_avif_reader_init(&seq_r, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                        stb_av1_bool_reader_init(&seq_br, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                        stb_av1_parse_sequence_header_obu(&seq_r, &sh, &seq_br);
                    }
#endif
                    seq_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME_HEADER:
                case STB_AV1_OBU_REDUNDANT_FRAME_HEADER: {
#ifdef STB_AVIF_USE_C89_DAV1D
                    {
                        struct stb_av1_msac _ms;
                        stb_av1_msac_init(&_ms, obu_reader.data + obu_reader.pos, (unsigned long)obu_size, 1);
                        stb_av1_parse_frame_hdr_msac(&_ms, &fh, &sh);
                    }
#else
                    {
                        struct stb_avif_reader fh_r;
                        struct stb_av1_bool_reader fh_br;
                        stb_avif_reader_init(&fh_r, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                        stb_av1_bool_reader_init(&fh_br, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                        stb_av1_parse_frame_header(&fh_r, &fh, &sh, &fh_br);
                    }
#endif
                    frame_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME: {
#ifdef STB_AVIF_USE_C89_DAV1D
                    {
                        struct stb_av1_msac _ms;
                        stb_av1_msac_init(&_ms, obu_reader.data + obu_reader.pos, (unsigned long)obu_size, 1);
                        stb_av1_parse_frame_hdr_msac(&_ms, &fh, &sh);
                        frame_header_found = 1;
                        stb_c89_msac = _ms;
                    }
#else
                    struct stb_avif_reader frame_r;
                    struct stb_av1_bool_reader frame_br;
                    stb_avif_reader_init(&frame_r, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                    stb_av1_bool_reader_init(&frame_br, obu_reader.data + obu_reader.pos, (size_t)obu_size);
                    stb_av1_parse_frame_header(&frame_r, &fh, &sh, &frame_br);
                    frame_header_found = 1;
                    if (!fh.show_existing_frame)
                        br = frame_br;
#endif
                    break;
                }
                case STB_AV1_OBU_TILE_GROUP: {
                    /* We already parsed frame header; this is tile data.
                       Transfer the boolean reader from current position. */
                    /* The tile group data starts at obu_reader.pos */
                    if (frame_header_found) {
#ifndef STB_AVIF_USE_C89_DAV1D
                        br.data = obu_reader.data + obu_reader.pos;
                        br.size = (size_t)obu_size;
                        br.pos = 0;
                        br.value = 0;
                        br.range = 128;
                        br.count = 0;
                        br.error = 0;
                        /* Re-init properly */
#endif
                        stb_av1_bool_reader_init(&br,
                                                   obu_reader.data + obu_reader.pos,
                                                   (size_t)obu_size);
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
    tc.mb_cols = (tc.frame_width + 3) / 4;
    tc.mb_rows = (tc.frame_height + 3) / 4;
    tc.br = &br;
#ifdef STB_AVIF_USE_C89_DAV1D
    stb_av1_msac_init(&stb_c89_msac, info.av1_data, (unsigned long)info.av1_size, 1);
    tc.msac = &stb_c89_msac;
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
                    u_val = (int)info.plane_u[uv_r * info.stride_u + uv_c];
                    v_val = (int)info.plane_v[uv_r * info.stride_v + uv_c];
                } else {
                    u_val = (int)info.plane_u[row * info.stride_u + col];
                    v_val = (int)info.plane_v[row * info.stride_v + col];
                }

                /* Range expansion for limited range (color_range=0) */
                if (sh.color_range == 0) {
                    y_val = ((y_val - 16) * 255) / 219;
                    if (y_val < 0) y_val = 0;
                    if (y_val > 255) y_val = 255;
                }
                u_val -= 128;
                v_val -= 128;

                /* Color matrix based on sequence header matrix_coefficients */
                {
                    int mc = sh.matrix_coefficients;
                    if (mc == 0) {
                        r = y_val + u_val;
                        g = y_val + v_val;
                        b = y_val + ((u_val + v_val) >> 1);
                    } else if (mc >= 8 && mc <= 10) {
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

                /* Clamp */
                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;

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
