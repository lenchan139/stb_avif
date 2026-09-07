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
 *   Example (with dav1d - correct output):
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
 *   Without it, the built-in AV1 decoder will be used.
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
 *  Link with -ldav1d. Without dav1d, the internal decoder will be used.
 */
unsigned char *stb_avif_load_from_memory(const unsigned char *data, int len,
                                          int *x, int *y, int *channels,
                                          int req_channels);

/* Free an image buffer previously returned by stb_avif_load_from_memory(). */
void stb_avif_free(void *ptr);

/* Returns a string describing the last error. */
static unsigned char *stb_avif_g_last_alpha;
static int stb_avif_g_last_alpha_stride;
static unsigned char *stb_avif_g_last_yuv_y;
static unsigned char *stb_avif_g_last_yuv_u;
static unsigned char *stb_avif_g_last_yuv_v;
static int stb_avif_g_last_yuv_stride_y;
static int stb_avif_g_last_yuv_stride_u;
static int stb_avif_g_last_yuv_stride_v;

/* Returns the 8-bit alpha plane (w-strided) decoded from the AVIF
 * auxiliary alpha item of the most recent load, or NULL. */
static unsigned char *stb_avif_last_alpha(int *stride)
{
    if (stride) *stride = stb_avif_g_last_alpha_stride;
    return stb_avif_g_last_alpha;
}

/* Returns the 8-bit YUV planes from the most recent load, or NULL.
 * Pointers are owned by the library and freed on the next stb_avif_load()
 * or stb_avif_close(); do NOT call stb_avif_free() on them. */
static void stb_avif_last_yuv(unsigned char **y, unsigned char **u, unsigned char **v,
                               int *stride_y, int *stride_u, int *stride_v)
{
    if (y) *y = stb_avif_g_last_yuv_y;
    if (u) *u = stb_avif_g_last_yuv_u;
    if (v) *v = stb_avif_g_last_yuv_v;
    if (stride_y) *stride_y = stb_avif_g_last_yuv_stride_y;
    if (stride_u) *stride_u = stb_avif_g_last_yuv_stride_u;
    if (stride_v) *stride_v = stb_avif_g_last_yuv_stride_v;
}

const char *stb_avif_failure_reason(void);

/* Load an AVIF from a file path.  Returns an 8-bit RGB/RGBA pixel buffer
 * (freed with stb_avif_free()) or NULL on failure.
 * req_channels: 0 = keep original (3 or 4), 3 = force RGB, 4 = force RGBA.
 * w/h/channels receive image dimensions and actual channel count. */
unsigned char *stb_avif_load_from_file(const char *filePath,
                                       int *w, int *h, int *channels,
                                       int req_channels);

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
#include <string.h>     /* memset, memcpy, memmove */
#include <setjmp.h>     /* setjmp, longjmp */
#include <math.h>       /* cos, sin, sqrt */
#include <time.h>       /* clock, time */
#include <stdio.h>      /* fprintf, stderr */

/* Optional dav1d backend for correct AV1 decoding.
   Define STB_AVIF_USE_DAV1D and link with -ldav1d */
#ifdef STB_AVIF_USE_DAV1D
#include <dav1d/dav1d.h>
#endif

#ifndef STB_AVIF_USE_DAV1D
#include "stb_av1_scalar.h"
#include "stb_av1_ipred.h"
#include "stb_av1_leaf.h"
#include "stb_av1_deblock.h"
#include "stb_av1_cdef.h"
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
   Define our own fixed-size types (guarded if scalar headers already included). */
#ifndef STB_AV1_SCALAR_H
typedef unsigned char  stbv_u8;
typedef signed char    stbv_s8;
typedef unsigned short stbv_u16;
typedef signed short   stbv_s16;
typedef unsigned int   stbv_u32;
typedef signed int     stbv_s32;
#ifndef STBV_I32_DEFINED
#define STBV_I32_DEFINED
typedef signed int     stbv_i32;
#endif
#endif
#ifndef STB_AV1_SCALAR_H
/* The AV1 decoder needs native 64-bit arithmetic for MSAC and bit reading.
   C89 has no standard 64-bit integer type, so use the compiler extensions
   available on the supported C89-era toolchains. */
#ifndef STB_AVIF_NO_64BIT
  #if defined(_MSC_VER)
    typedef unsigned __int64 stbv_u64;
    typedef __int64          stbv_s64;
  #else
    typedef unsigned long long stbv_u64;
    typedef long long          stbv_s64;
  #endif
#else
  #error "stb_avif requires 64-bit integer support"
#endif
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

#ifndef STB_AVIF_USE_DAV1D
#include "stb_av1_lr.h"
#endif

/* ----------- BITSTREAM READER ----------- */
int sh_parsed_ok = 0;
int probe_seq_hbd = 0, probe_seq_mono = 0;

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
#define STB_AVIF_BOX_IREF   STB_AVIF_FOURCC('i','r','e','f')
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
static void stb_avif_read_box_header(struct stb_av1_getbits *gb, struct stb_avif_box *box)
{
    stbv_u32 size32;
    stbv_u64 start = (stbv_u64)stb_av1_getbits_bytepos(gb);

    size32 = stb_av1_getbits_read_be32(gb);
    box->type = stb_av1_getbits_read_be32(gb);

    if (size32 == 1) {
        /* Extended size (64-bit) */
        box->size = stb_av1_getbits_read_be64(gb);
    } else if (size32 == 0) {
        /* Box extends to end of file */
        box->size = (stbv_u64)stb_av1_getbits_size(gb) - start;
    } else {
        box->size = (stbv_u64)size32;
    }

    box->data_start = (stbv_u64)stb_av1_getbits_bytepos(gb);
    if (box->size >= (stbv_u64)(stb_av1_getbits_bytepos(gb) - start)) {
        box->data_size = box->size - (stbv_u64)(stb_av1_getbits_bytepos(gb) - start);
    } else {
        box->data_size = 0;
    }
}

/* Skip to end of box */
static void stb_avif_skip_box(struct stb_av1_getbits *gb, const struct stb_avif_box *box)
{
    stbv_u64 end = box->data_start + box->data_size;
    if (end > (stbv_u64)stb_av1_getbits_size(gb))
        STB_AVIF_ERROR("Box extends beyond data");
    stb_av1_getbits_seek(gb, (size_t)end);
}

/* Enter a box: position at data start */
static void stb_avif_enter_box(struct stb_av1_getbits *gb, const struct stb_avif_box *box)
{
    if (box->data_start > (stbv_u64)stb_av1_getbits_size(gb))
        STB_AVIF_ERROR("Box position out of bounds");
    stb_av1_getbits_seek(gb, (size_t)box->data_start);
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
    int av1c_seen;

    /* ipma / multi-av1C support */
    int av1c_prop_idx[4];         /* 1-based property indices of av1C in ipco */
    int av1c_prop_count;          /* number of av1C properties found */
    unsigned char av1c_all_data[4][32]; /* raw av1C OBU payload for each property */
    int av1c_all_size[4];         /* size of each av1C OBU payload */
    int av1c_all_bd[4];           /* bit_depth per av1C */
    int av1c_all_ssx[4];          /* chroma_subsampling_x per av1C */
    int av1c_all_ssy[4];          /* chroma_subsampling_y per av1C */
    int av1c_all_mono[4];         /* monochrome per av1C */
    int primary_av1c_prop_idx;    /* resolved 1-based index for primary item */
    /* ispe: store dimensions per property index so we can resolve after ipma */
    int ispe_prop_idx[8];         /* 1-based property indices of ispe in ipco */
    int ispe_all_w[8];            /* width per ispe property */
    int ispe_all_h[8];            /* height per ispe property */
    int ispe_prop_count;          /* number of ispe properties found */
    int primary_ispe_prop_idx;    /* resolved 1-based index for primary ispe */
    /* ipma: up to 16 items, each with up to 8 property associations */
    int ipma_item_count;
    int ipma_item_ids[16];
    int ipma_prop_count[16];
    int ipma_prop_idx[16][8];     /* 1-based property indices per item */

    /* Compressed AV1 data */
    const unsigned char *av1_data;
    size_t av1_size;
    void *ivf_concat_buf; /* non-NULL if av1_data was allocated for IVF concatenation */

    /* Output buffer */
    unsigned char *output;
    int output_channels;

    /* Input data */
    const unsigned char *input;
    int input_len;
    size_t meta_end_offset;

    /* Auxiliary alpha item (raw AV1 payload + decoded 8-bit plane) */
    int primary_item_id;
    int alpha_item_id;
    const unsigned char *alpha_av1;
    size_t alpha_size;
    unsigned char *alpha_plane;
    int alpha_stride;

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
   Returns 1 if found, 0 if not. Does not modify gb position on return. */
static int stb_avif_find_box(struct stb_av1_getbits *gb, stbv_u32 type,
                              int deep_search, struct stb_avif_box *box_out)
{
    size_t saved_pos = stb_av1_getbits_bytepos(gb);

    while (stb_av1_getbits_bytepos(gb) + 8 <= stb_av1_getbits_size(gb)) {
        struct stb_avif_box box;
        size_t box_start = stb_av1_getbits_bytepos(gb);

        stb_avif_read_box_header(gb, &box);

        if (box.type == type) {
            stb_av1_getbits_seek(gb, (size_t)box.data_start);
            if (box_out) *box_out = box;
            return 1;
        }

        if (deep_search && (box.type == STB_AVIF_BOX_META ||
                            box.type == STB_AVIF_BOX_IPRP ||
                            box.type == STB_AVIF_BOX_IPCO ||
                            box.type == STB_AVIF_BOX_MOOV ||
                            box.type == STB_AVIF_BOX_MOOF))
        {
            stb_avif_enter_box(gb, &box);
            if (stb_avif_find_box(gb, type, deep_search, box_out)) {
                return 1;
            }
        }

        stb_av1_getbits_seek(gb, (size_t)(box_start + box.size));
    }

    stb_av1_getbits_seek(gb, saved_pos);
    return 0;
}

/* Parse ftyp box to verify this is an AVIF file */
static void stb_avif_parse_ftyp(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info)
{
    /* Skip major brand (4 bytes), minor version (4 bytes) */
    stb_av1_getbits_skip(gb, 8);

    /* Check for compatible brands */
    while (stb_av1_getbits_bytepos(gb) < stb_av1_getbits_size(gb)) {
        stbv_u32 brand = stb_av1_getbits_read_be32(gb);
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
static void stb_avif_parse_av1c(struct stb_av1_getbits *gb,
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
    stb_av1_getbits_read_byte(gb);

    /* Byte 1: seq_profile(3) + seq_level_idx_0(5) */
    {
        int byte1 = stb_av1_getbits_read_byte(gb);
        seq_profile = (byte1 >> 5) & 7;
        /* seq_level_idx_0 = byte1 & 31; */
    }

    /* Byte 2: flags */
    {
        int byte2 = stb_av1_getbits_read_byte(gb);
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
        int byte3 = stb_av1_getbits_read_byte(gb);
        initial_presentation_delay_present = (byte3 >> 4) & 1;
    }

    /* Remaining bytes: config OBUs (sequence header OBU data) 
       box_data_size is the total av1C box content size; we've read 4 fixed bytes */
    info->av1c_size = (int)box_data_size - 4;
    if (info->av1c_size > (int)sizeof(info->av1c_data))
        info->av1c_size = (int)sizeof(info->av1c_data);
    if (info->av1c_size < 0) info->av1c_size = 0;

    for (i = 0; i < info->av1c_size && i < (int)box_data_size - 4; i++) {
        info->av1c_data[i] = (unsigned char)stb_av1_getbits_read_byte(gb);
    }

    STB_AVIF_CHECK(high_bitdepth == 0 || high_bitdepth == 1,
                   "Invalid bitdepth flag");
    (void)seq_profile;
    (void)seq_tier_0;
    (void)chroma_sample_position;
    (void)initial_presentation_delay_present;
}

/* Parse the iloc box (item location) to find where coded data is stored */
static void stb_avif_parse_iloc(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info,
                                 int primary_id,
                                 stbv_u32 *data_offset,
                                 stbv_u64 *data_size)
{
    int version;
    int offset_size, length_size, base_offset_size, index_size;
    int item_count, i_item;

    version = stb_av1_getbits_read_byte(gb);
    {
        /* fullbox: version(1) + flags(3), then the sizes byte */
        int fl;
        stb_av1_getbits_read_byte(gb);          /* version */
        stb_av1_getbits_read_byte(gb);
        stb_av1_getbits_read_byte(gb);
        fl       = stb_av1_getbits_read_byte(gb);
        /* ISO/AVIF: these 4-bit fields store SIZE-1 (0 = absent) */
        offset_size = (fl >> 4) & 0xF;
        length_size = fl & 0xF;
        /* base_offset_size / index_size byte is present in ALL versions */
        fl = stb_av1_getbits_read_byte(gb);
        base_offset_size = (fl >> 4) & 0xF;
        index_size = fl & 0xF;
        if (version < 2) {
            index_size = 0;
            (void)index_size;
        }
    }

    item_count = (int)stb_av1_getbits_read_be16(gb);
    *data_offset = 0;
    *data_size = 0;

    for (i_item = 0; i_item < item_count; i_item++) {
        int item_ID;
        int data_ref_index;
        int i_extent;
        int extent_count;

        if (version < 2) {
            item_ID = (int)stb_av1_getbits_read_be16(gb);
        } else {
            item_ID = (int)stb_av1_getbits_read_be16(gb);
            stb_av1_getbits_read_be16(gb);
        }

        if (version >= 1) {
            /* construction_method */
            /* 4 bytes: (12 reserved + 4 construction_method) or more depending on version */
            stb_av1_getbits_read_be16(gb); /* skip */
            data_ref_index = stb_av1_getbits_read_be16(gb);
        } else {
            data_ref_index = stb_av1_getbits_read_be16(gb);
        }
        (void)data_ref_index;

        /* base_offset */
        {
            int _off_sz;
            int j;
            stbv_u64 base_offset_val = 0;

            /* ISO/IEC 14496-12: per-item base_offset is base_offset_size
             * units wide in every iloc version. */
            _off_sz = base_offset_size;

            for (j = 0; j < _off_sz; j++) {
                base_offset_val = (base_offset_val << 8) | (stbv_u64)stb_av1_getbits_read_byte(gb);
            }

            extent_count = (int)stb_av1_getbits_read_be16(gb);

            for (i_extent = 0; i_extent < extent_count; i_extent++) {
                stbv_u64 extent_offset = 0;
                stbv_u64 extent_length = 0;
                int k;

                for (k = 0; k < offset_size; k++) {
                    extent_offset = (extent_offset << 8) | (stbv_u64)stb_av1_getbits_read_byte(gb);
                }
                for (k = 0; k < length_size; k++) {
                    extent_length = (extent_length << 8) | (stbv_u64)stb_av1_getbits_read_byte(gb);
                }

                if (item_ID == primary_id && i_extent == 0 &&
                    !*data_size) {
                    *data_offset = (stbv_u32)(base_offset_val + extent_offset);
                    *data_size = extent_length;
                }
            }
        }
    }
}

/* Parse pitm (Primary Item ID) */
static int stb_avif_parse_pitm(struct stb_av1_getbits *gb)
{
    int version = stb_av1_getbits_read_byte(gb);
    stb_av1_getbits_read_byte(gb); /* flags */
    stb_av1_getbits_read_byte(gb); /* flags */
    stb_av1_getbits_read_byte(gb); /* flags */
    if (version < 1) {
        return (int)stb_av1_getbits_read_be16(gb);
    } else {
        /* version >= 1 uses 32-bit */
        return (int)stb_av1_getbits_read_be32(gb);
    }
}

/* Parse the meta box to extract AVIF metadata */
static void stb_avif_parse_meta(struct stb_av1_getbits *gb,
                                 struct stb_avif_avif_info *info)
{
    stbv_u32 data_offset = 0;
    stbv_u64 data_size = 0;
    struct stb_avif_box meta_box;
    size_t meta_end;

    /* Skip FullBox version+flags (4 bytes) */
    stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);
    stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);

    meta_box.data_start = (stbv_u64)stb_av1_getbits_bytepos(gb);
    meta_box.data_size = (stbv_u64)(info->meta_end_offset - stb_av1_getbits_bytepos(gb));

    meta_end = info->meta_end_offset;

    /* Scan sub-boxes within meta */
    while (stb_av1_getbits_bytepos(gb) < meta_end) {
        struct stb_avif_box sub;
        size_t sub_start = stb_av1_getbits_bytepos(gb);

        if (stb_av1_getbits_bytepos(gb) + 8 > stb_av1_getbits_size(gb)) break;

        stb_avif_read_box_header(gb, &sub);
        if (sub.type == STB_AVIF_BOX_HDLR) {
            /* handler box - verify picture handler */
        }
        else if (sub.type == STB_AVIF_BOX_PITM) {
            info->primary_item_id = stb_avif_parse_pitm(gb);
        }
        else if (sub.type == STB_AVIF_BOX_ILOC) {
            stb_avif_parse_iloc(gb, info, info->primary_item_id,
                                &data_offset, &data_size);
        }
        else if (sub.type == STB_AVIF_BOX_IREF) {
            /* iref: version/flags, then SUB-BOXES, one per reference
             * type: { u32 size; u32 type('auxl'); u16 from_item_ID;
             *         u16 reference_count; u16 to_item_ID[]; } */
            struct stb_avif_box ir = sub;
            stb_avif_enter_box(gb, &ir);
            stb_av1_getbits_read_byte(gb);
            stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);
            while (stb_av1_getbits_bytepos(gb) + 8 <= (size_t)(ir.data_start + ir.data_size)) {
                stbv_u32 esz = stb_av1_getbits_read_be32(gb);
                stbv_u32 ety = stb_av1_getbits_read_be32(gb);
                int from_id, ref_count, ri;
                size_t ebody_end;
                if (esz < 8) break;
                ebody_end = stb_av1_getbits_bytepos(gb) + esz - 8;
                if (ebody_end > (size_t)(ir.data_start + ir.data_size))
                    ebody_end = (size_t)(ir.data_start + ir.data_size);
                from_id = (int)stb_av1_getbits_read_be16(gb);
                ref_count = (int)stb_av1_getbits_read_be16(gb);
                for (ri = 0; ri < ref_count; ri++) {
                    int to_id;
                    if (stb_av1_getbits_bytepos(gb) + 2 > ebody_end) break;
                    to_id = (int)stb_av1_getbits_read_be16(gb);
                    if (ety == STB_AVIF_FOURCC('a','u','x','l') &&
                        to_id == info->primary_item_id)
                        info->alpha_item_id = from_id;
                }
                stb_av1_getbits_seek(gb, ebody_end);
            }
        }
        else if (sub.type == STB_AVIF_BOX_IPRP) {
            /* Item properties container */
            struct stb_avif_box iprp_box = sub;
            stb_avif_enter_box(gb, &iprp_box);

            while (stb_av1_getbits_bytepos(gb) < (size_t)(iprp_box.data_start + iprp_box.data_size)) {
                struct stb_avif_box iprp_sub;
                size_t iprp_sub_start = stb_av1_getbits_bytepos(gb);

                if (stb_av1_getbits_bytepos(gb) + 8 > stb_av1_getbits_size(gb)) break;
                stb_avif_read_box_header(gb, &iprp_sub);

                if (iprp_sub.type == STB_AVIF_BOX_IPCO) {
                    /* Item property container */
                    struct stb_avif_box ipco_box = iprp_sub;
                    stb_avif_enter_box(gb, &ipco_box);
                    {
                    int ipco_prop_pos = 0; /* 1-based ipco position counter */

                    while (stb_av1_getbits_bytepos(gb) < (size_t)(ipco_box.data_start + ipco_box.data_size)) {
                        struct stb_avif_box prop;
                        size_t prop_start = stb_av1_getbits_bytepos(gb);
                        ipco_prop_pos++;

                        if (stb_av1_getbits_bytepos(gb) + 8 > stb_av1_getbits_size(gb)) break;
                        stb_avif_read_box_header(gb, &prop);

                        if (prop.type == STB_AVIF_BOX_AV1C) {
                            /* Record 1-based property index for ipma lookup.
                             * Store raw av1C data; we'll select the right one
                             * after ipma is parsed. */
                            int idx = info->av1c_prop_count;
                            if (idx < 4) {
                                info->av1c_prop_idx[idx] = idx + 1;
                                /* Parse av1C into temporary storage */
                                {
                                    size_t saved_pos = stb_av1_getbits_bytepos(gb);
                                    stb_avif_parse_av1c(gb, info, (size_t)prop.data_size);
                                    /* Copy the just-parsed av1C data to all_data[idx] */
                                    if (info->av1c_size <= 32) {
                                        int bi;
                                        for (bi = 0; bi < info->av1c_size; bi++)
                                            info->av1c_all_data[idx][bi] = info->av1c_data[bi];
                                        info->av1c_all_size[idx] = info->av1c_size;
                                    }
                                    /* Cache parsed properties for this av1C */
                                    info->av1c_all_bd[idx] = info->bit_depth;
                                    info->av1c_all_ssx[idx] = info->chroma_subsampling_x;
                                    info->av1c_all_ssy[idx] = info->chroma_subsampling_y;
                                    info->av1c_all_mono[idx] = info->monochrome;
                                    (void)saved_pos;
                                }
                            }
                            info->av1c_prop_count++;
                            info->av1c_seen = 1;
                        }
                        else if (prop.type == STB_AVIF_BOX_ISPE) {
                            /* Image spatial extents: store per-property, resolve
                             * to primary item later via ipma. */
                            int si;
                            stb_av1_getbits_read_byte(gb); /* version */
                            stb_av1_getbits_read_byte(gb); /* flags */
                            stb_av1_getbits_read_byte(gb);
                            stb_av1_getbits_read_byte(gb);
                            {
                                int w = (int)stb_av1_getbits_read_be32(gb);
                                int h = (int)stb_av1_getbits_read_be32(gb);
                                si = info->ispe_prop_count;
                                if (si < 8) {
                                    info->ispe_prop_idx[si] = ipco_prop_pos;
                                    info->ispe_all_w[si] = w;
                                    info->ispe_all_h[si] = h;
                                }
                                info->ispe_prop_count++;
                            }
                        }
                        else if (prop.type == STB_AVIF_BOX_PIXI) {
                            /* Pixel information (bit depth per channel) */
                            stb_av1_getbits_read_byte(gb); /* version */
                            stb_av1_getbits_read_byte(gb); /* flags */
                            stb_av1_getbits_read_byte(gb);
                            stb_av1_getbits_read_byte(gb);
                            /* num_channels */
                            (void)stb_av1_getbits_read_byte(gb);
                        }

                        stb_av1_getbits_seek(gb, (size_t)(prop_start + prop.size));
                    }
                    } /* end ipco_prop_pos block */
                }
                else if (iprp_sub.type == STB_AVIF_BOX_IPMA) {
                    /* Item Property Association box: maps item IDs to property indices.
                     * Format: version(1) + flags(3) + entry_count(4)
                     *   each entry: item_ID(16|32) + association_count(8)
                     *     per assoc: property_index(16, 1-based, high bit = essential) */
                    struct stb_avif_box ipma_box = iprp_sub;
                    stb_avif_enter_box(gb, &ipma_box);
                    {
                        int ipma_version = stb_av1_getbits_read_byte(gb);
                        stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb); stb_av1_getbits_read_byte(gb);
                        {
                            stbv_u32 entry_count = stb_av1_getbits_read_be32(gb);
                            stbv_u32 ei;
                            info->ipma_item_count = 0;
                            for (ei = 0; ei < entry_count && ei < 16; ei++) {
                                int item_id, acnt, ai;
                                if (ipma_version == 0)
                                    item_id = (int)stb_av1_getbits_read_be16(gb);
                                else
                                    item_id = (int)stb_av1_getbits_read_be32(gb);
                                acnt = (int)stb_av1_getbits_read_byte(gb);
                                info->ipma_item_ids[info->ipma_item_count] = item_id;
                                info->ipma_prop_count[info->ipma_item_count] = 0;
                                for (ai = 0; ai < acnt && ai < 8; ai++) {
                                    int prop_idx = (int)stb_av1_getbits_read_be16(gb);
                                    /* bit 15 = essential flag; property_index is bits 14..0 */
                                    prop_idx &= 0x7fff;
                                    if (info->ipma_prop_count[info->ipma_item_count] < 8)
                                        info->ipma_prop_idx[info->ipma_item_count][info->ipma_prop_count[info->ipma_item_count]++] = prop_idx;
                                }
                                info->ipma_item_count++;
            }
        }
    }
                }

                stb_av1_getbits_seek(gb, (size_t)(iprp_sub_start + iprp_sub.size));
            }
        }

        /* Resolve the correct av1C for the primary item using ipma.
         * ipma maps item_ID -> property indices (1-based) into ipco.
         * Find the primary item's av1C property index, then select it. */
        if (info->primary_item_id > 0 && info->av1c_prop_count > 1) {
            int pi, ai2;
            for (pi = 0; pi < info->ipma_item_count; pi++) {
                if (info->ipma_item_ids[pi] == info->primary_item_id) {
                    for (ai2 = 0; ai2 < info->ipma_prop_count[pi]; ai2++) {
                        int pidx = info->ipma_prop_idx[pi][ai2];
                        int k;
                        for (k = 0; k < info->av1c_prop_count; k++) {
                            if (info->av1c_prop_idx[k] == pidx) {
                                /* Found the primary item's av1C. Copy cached
                                 * properties directly instead of re-parsing. */
                                int bi;
                                info->av1c_size = info->av1c_all_size[k];
                                for (bi = 0; bi < info->av1c_size; bi++)
                                    info->av1c_data[bi] = info->av1c_all_data[k][bi];
                                info->primary_av1c_prop_idx = pidx;
                                info->bit_depth = info->av1c_all_bd[k];
                                info->chroma_subsampling_x = info->av1c_all_ssx[k];
                                info->chroma_subsampling_y = info->av1c_all_ssy[k];
                                info->monochrome = info->av1c_all_mono[k];
                                break;
                            }
                        }
                        if (info->primary_av1c_prop_idx) break;
                    }
                    break;
                }
            }
        }

        /* Resolve the correct ispe (width/height) for the primary item using ipma.
         * Multiple ispe properties may exist (main + thumbnails); we must pick
         * the one associated with the primary item. */
        if (info->primary_item_id > 0 && info->ispe_prop_count > 0) {
            int pi, ai2;
            for (pi = 0; pi < info->ipma_item_count; pi++) {
                if (info->ipma_item_ids[pi] == info->primary_item_id) {
                    for (ai2 = 0; ai2 < info->ipma_prop_count[pi]; ai2++) {
                        int pidx = info->ipma_prop_idx[pi][ai2];
                        int k;
                        for (k = 0; k < info->ispe_prop_count && k < 8; k++) {
                            if (info->ispe_prop_idx[k] == pidx) {
                                info->width = info->ispe_all_w[k];
                                info->height = info->ispe_all_h[k];
                                info->primary_ispe_prop_idx = pidx;
                                break;
                            }
                        }
                        if (info->primary_ispe_prop_idx) break;
                    }
                    break;
                }
            }
        }
        /* Fallback: if no primary ispe found via ipma, use the first ispe */
        if (!info->primary_ispe_prop_idx && info->ispe_prop_count > 0) {
            info->width = info->ispe_all_w[0];
            info->height = info->ispe_all_h[0];
        }

        stb_av1_getbits_seek(gb, (size_t)(sub_start + sub.size));
    }

    /* Now read the mdat data */
    {
        size_t saved = stb_av1_getbits_bytepos(gb);

        stb_av1_getbits_seek(gb, 0);
        if (stb_avif_find_box(gb, STB_AVIF_BOX_MDAT, 0, NULL)) {
                    info->av1_data = gb->ptr_start + stb_av1_getbits_bytepos(gb);
            info->av1_size = stb_av1_getbits_size(gb) - stb_av1_getbits_bytepos(gb); /* Rest of file is mdat content */

            /* If we have iloc info, use that offset instead */
            if (data_size > 0 && data_offset > 0) {
                info->av1_data = gb->ptr_start + data_offset;
                info->av1_size = (size_t)data_size;
            } else {
                /* Conservative: mdat may contain more than just our image.
                   Use iloc info. But if we don't have it, use all remaining. */
                /* The actual av1 data starts at data_offset from the beginning of mdat */
                if (data_offset > 0) {
                    /* data_offset is absolute in the file */
                    info->av1_data = gb->ptr_start + data_offset;
                    if (data_size > 0)
                        info->av1_size = (size_t)data_size;
                    else
                        info->av1_size = stb_av1_getbits_size(gb) - data_offset;
                }
            }
        }
        stb_av1_getbits_seek(gb, saved);
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
static int stb_av1_read_obu_header(struct stb_av1_getbits *gb, int *obu_type,
                                    int *obu_extension_flag, int *obu_has_size_field)
{
    int hdr = stb_av1_getbits_read_byte(gb);
    if (hdr & 0x80) STB_AVIF_ERROR("Invalid OBU header (reserved bit not 0)");
    *obu_type = (hdr >> 3) & 0xF;
    *obu_extension_flag = (hdr >> 2) & 1;
    *obu_has_size_field = (hdr >> 1) & 1;
    return hdr & 1; /* obu_forbidden_bit */
}

/* Read OBU size (LEB128 encoded) */
static stbv_u32 stb_av1_read_obu_size(struct stb_av1_getbits *gb)
{
    return stb_av1_getbits_read_uleb128(gb);
}

/* -------------------------------------------------------------------------- */
/* AV1 BOOLEAN (ARITHMETIC) ENTROPY DECODER                                   */
/* -------------------------------------------------------------------------- */

/* The AV1 spec defines a Boolean decoder based on the Daala entropy coder.
   State: value (16-bit window), range (9-bit, 128-255) */

#define STB_AV1_BOOL_READER_SIZE 4096
#define STB_AV1_BOOL_BUF_BITS 8

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
    return (v << 1) | (stb_av1_bool_decode(br, 128) - m);
}

/* NSYM symbol decoding (non-symmetric) with cumulative probabilities */
/* Simplified: decode an n-ary symbol using a binary tree with equal probs */
static int stb_av1_decode_nsym(struct stb_av1_bool_reader *br, int n)
{
    if (n <= 1) return 0;
    /* Use uniform decoding as a simplification */
    return stb_av1_decode_uniform(br, n);
}

/* -------------------------------------------------------------------------- */
/* AV1 SEQUENCE HEADER STRUCT                                                 */
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

static void stb_av1_parse_frame_header(struct stb_av1_frame_header *fh,
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
                                       int *subsampling_x, int *subsampling_y,
                                       int *color_range, int *matrix_coefficients)
{
    Dav1dContext *ctx = NULL;
    Dav1dSettings s;
    Dav1dData data;
    Dav1dPicture pic = { 0 };
    int ret;
    int i;

    dav1d_default_settings(&s);
    s.n_threads = 1;
    s.all_layers = 0;

    ret = dav1d_open(&ctx, &s);
    if (ret < 0) {  return 0; }

    /* Copy the AV1 data into dav1d-managed memory (dav1d takes ownership) */
    {
        uint8_t *ptr = dav1d_data_create(&data, av1_size);
        if (!ptr) { dav1d_close(&ctx); return 0; }
        memcpy(ptr, av1_data, av1_size);
    }

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

    /* Extract colour properties from the decoded sequence header */
    if (pic.seq_hdr) {
        *color_range = pic.seq_hdr->color_range;
        *matrix_coefficients = (int)pic.seq_hdr->mtrx;
    } else {
        *color_range = 0;
        *matrix_coefficients = 1;
    }

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

    /* Compute chroma dimensions using ceiling division */
    {
        int uv_w = (*width + (1 << *subsampling_x) - 1) >> *subsampling_x;
        int uv_h = (*height + (1 << *subsampling_y) - 1) >> *subsampling_y;

        /* Allocate 8-bit output planes */
        *y_stride = (*width + 31) & ~31;
        *y_plane = (unsigned char *)malloc((size_t)(*y_stride * *height));
        if (!*y_plane) { dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

        *u_stride = (uv_w + 31) & ~31;
        *u_plane = (unsigned char *)malloc((size_t)(*u_stride * uv_h));
        if (!*u_plane) { free(*y_plane); dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }

        *v_stride = *u_stride;
        *v_plane = (unsigned char *)malloc((size_t)(*v_stride * uv_h));
        if (!*v_plane) { free(*y_plane); free(*u_plane); dav1d_picture_unref(&pic); dav1d_close(&ctx); return 0; }
    }

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
        int uv_h = (*height + (1 << *subsampling_y) - 1) >> *subsampling_y;
        int uv_w = (*width + (1 << *subsampling_x) - 1) >> *subsampling_x;
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
        int uv_h = (*height + (1 << *subsampling_y) - 1) >> *subsampling_y;
        int uv_w = (*width + (1 << *subsampling_x) - 1) >> *subsampling_x;
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

#ifndef STB_AVIF_USE_DAV1D
/* -------------------------------------------------------------------------- */
/* SCALAR AV1 DECODER WITH RECON HOOKS  (C89)                                   */
/* -------------------------------------------------------------------------- */
struct stb_avif_scalar_recon {
    /* Planes are stbv_u16 regardless of bit depth; strides are in pixels. */
    stbv_u16 *plane_y;
    stbv_u16 *plane_u;
    stbv_u16 *plane_v;
    int stride_y;
    int stride_u;
    int stride_v;
    int bit_depth;
    int ss_hor;
    int ss_ver;
    int frame_w;
    int frame_h;
    stbv_i32 cf[4096];
    stbv_u16 pred[128 * 128];
    int cur_bx4;
    int cur_by4;
    int cur_bw4;
    int cur_bh4;
    int y_mode;
    int y_angle;
    int uv_angle;
    int uv_mode;
    int block_skip;
    int cfl_alpha_u;
    int cfl_alpha_v;
    int has_chroma;
    int pal_y, pal_uv;
    /* dav1d intra prediction flags: seq_hdr->intra_edge_filter plus the
     * SMOOTH-neighbour flag (ANGLE_SMOOTH_EDGE_FLAG=512) and
     * ANGLE_USE_EDGE_FILTER_FLAG=1024 ORed into the angle argument. */
    int intra_edge_filter;
    int sb_step4; /* superblock step in 4x4 units: 32 for sb128, else 16 */
    const stbv_u8 *above_mode;
    const stbv_u8 *left_mode;
    unsigned int above_n;
    unsigned int left_n;
    const stbv_u8 *above_uvmode;
    const stbv_u8 *left_uvmode;
    /* dav1d CFL: one block-wide AC array shared by both chroma planes. */
    stbv_i16 cfl_ac[32 * 32];
    int cfl_ac_w, cfl_ac_h;
    int cfl_ac_bx, cfl_ac_by;
    int cfl_ac_ok;
    int cur_pl;
    int cur_ltw4, cur_lth4; /* block's max luma tx size (b->tx dims) */
    /* Deblocking: per-4x4-unit block identity + covering-transform
     * log2-width maps (frame-sized, filled during recon). */
    stbv_u32 *lf_blkid;
    stbv_u8 *lf_txlw;
    stbv_u32 *lf_blkid_c;   /* chroma-plane coverage (separate set) */
    stbv_u8 *lf_txlw_c;
    stbv_u8 *lf_done;       /* per-4x4-unit reconstruction bitmap (luma) */
    int lf_mapw4, lf_maph4;
    ptrdiff_t lf_b4stride;
    int tile_x4, tile_y4, tile_w4, tile_h4;
    /* IBC reconstruction state. */
    int is_ibc;
    int ibc_mv_y, ibc_mv_x;
};

/* Decoding-order key for availability checks: superblock row, then SB
 * column, then Morton/z-order inside the SB (AV1 decode order). */
static int stb_avif_recon_decoded_before(int qx4, int qy4,
                                         int cx4, int cy4,
                                         int sb_step4)
{
    int log2sb = sb_step4 == 32 ? 5 : 4;
    int b;
    unsigned int zq = 0, zc = 0;
    if ((qy4 >> log2sb) != (cy4 >> log2sb))
        return (qy4 >> log2sb) < (cy4 >> log2sb);
    if ((qx4 >> log2sb) != (cx4 >> log2sb))
        return (qx4 >> log2sb) < (cx4 >> log2sb);
    qx4 &= sb_step4 - 1; qy4 &= sb_step4 - 1;
    cx4 &= sb_step4 - 1; cy4 &= sb_step4 - 1;
    for (b = 0; b < log2sb; b++) {
        zq |= ((unsigned)(qx4 >> b) & 1u) << (2 * b) |
              ((unsigned)(qy4 >> b) & 1u) << (2 * b + 1);
        zc |= ((unsigned)(cx4 >> b) & 1u) << (2 * b) |
              ((unsigned)(cy4 >> b) & 1u) << (2 * b + 1);
    }
    return zq < zc;
}

/* Per-txb EDGE flags following dav1d recon_b_intra.  Top-right pixels sit
 * on the row above (decoded earlier unless in a right-hand SB of this SB
 * row); bottom-left follows decoding order.  Values use our port's
 * STBV_AV1_EDGE_I444_* convention: bit0 TOP_HAS_RIGHT, bit1 LEFT_HAS_BOTTOM. */
static int stb_avif_recon_have_left(const struct stb_avif_scalar_recon *rc,
                                       int luma, int x4)
{
    int tx0 = luma ? rc->tile_x4 : (rc->tile_x4 >> rc->ss_hor);
    return x4 > tx0;
}

static int stb_avif_recon_have_top(const struct stb_avif_scalar_recon *rc,
                                   int luma, int y4)
{
    int ty0 = luma ? rc->tile_y4 : (rc->tile_y4 >> rc->ss_ver);
    return y4 > ty0;
}

static int stb_avif_recon_block_edge_flags(struct stb_avif_scalar_recon *rc,
                                           int luma, int x4, int y4,
                                           int w4, int h4);
static int stb_avif_recon_block_edge_flags_run(struct stb_avif_scalar_recon *rc,
                                               int luma, int x4, int y4,
                                               int w4, int h4,
                                               int tr_run, int bl_run);

static int stb_avif_recon_txb_edge_flags(struct stb_avif_scalar_recon *rc,
                                         int luma, int bx4, int by4,
                                         int bw4, int bh4,
                                         int tx4, int ty4, int tw4, int th4)
{
    const int ss_hor = luma ? 0 : rc->ss_hor;
    const int ss_ver = luma ? 0 : rc->ss_ver;
    /* dav1d splits each block into 64x64 quadrants (init += 16 luma units)
     * and evaluates per-txb flags against the QUADRANT base, not the
     * per-txb offset. */
    const int qw = luma ? 16 : 16 >> ss_hor;
    const int qh = luma ? 16 : 16 >> ss_ver;
    int blk;
    int xl, yl, qxl, qyl, sub_w4, sub_h4;
    int sb_has_tr, sb_has_bl, fl = 0;
    /* txb callbacks carry block coordinates in luma 4x4 units even for
     * chroma. Convert to chroma: floor division for origin (matching dav1d's
     * init_x >> ss_hor), ceiling division for extent. */
    if (!luma) {
        bx4 = bx4 >> ss_hor;
        by4 = by4 >> ss_ver;
        bw4 = (bw4 + ss_hor) >> ss_hor;
        bh4 = (bh4 + ss_ver) >> ss_ver;
    }
    /* Block-level availability == dav1d's decode_b intra_edge_flags. */
    blk = stb_avif_recon_block_edge_flags_run(rc, luma, bx4, by4,
                                               bw4, bh4, tw4, th4);
    xl = tx4 - bx4; yl = ty4 - by4;
    qxl = xl / qw * qw;
    qyl = yl / qh * qh;
    sub_w4 = bw4 < qxl + qw ? bw4 : qxl + qw;
    sub_h4 = bh4 < qyl + qh ? bh4 : qyl + qh;

    sb_has_tr = (qxl + qw < bw4) ? 1 :
                (qyl ? 0 :
                 (blk & STBV_AV1_EDGE_I444_TOP_HAS_RIGHT ? 1 : 0));
    sb_has_bl = qxl ? 0 :
                ((qyl + qh < bh4) ? 1 :
                 (blk & STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM ? 1 : 0));

    /* dav1d recon_b_intra: sub_w4/sub_h4 are quadrant-relative extents
     * (already include qxl/qyl), so the comparisons must NOT add them
     * again.  Double-adding made every lower/left-quadrant txb think the
     * bottom-left / top-right neighbour was available when it was not,
     * and Z3 then blended unwritten zero pixels into the prediction
     * (the dark-triangle / green-fog artifacts). */
    if (!((yl > qyl || !sb_has_tr) && (xl + tw4 >= sub_w4)))
        fl |= STBV_AV1_EDGE_I444_TOP_HAS_RIGHT;
    if (!(xl > qxl || (!sb_has_bl && yl + th4 >= sub_h4)))
        fl |= STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM;
    return fl;
}

/* Block-level variant (whole-block prediction path). */
static int stb_avif_recon_block_edge_flags(struct stb_avif_scalar_recon *rc,
                                           int luma, int x4, int y4,
                                           int w4, int h4)
{
    return stb_avif_recon_block_edge_flags_run(rc, luma, x4, y4, w4, h4,
                                               w4, h4);
}

/* Run-aware variant: tr_run / bl_run give the number of 4x4 units the
 * predictor will actually read beyond the edge (the transform extent,
 * not necessarily the whole block). */
static int stb_avif_recon_block_edge_flags_run(struct stb_avif_scalar_recon *rc,
                                               int luma, int x4, int y4,
                                               int w4, int h4,
                                               int tr_run, int bl_run)
{
    const int ss_hor = luma ? 0 : rc->ss_hor;
    const int ss_ver = luma ? 0 : rc->ss_ver;
    const int fw4a = (rc->frame_w + 7) & ~7;
    const int fh4a = (rc->frame_h + 7) & ~7;
    const int fw4 = luma ? (fw4a >> 2) : ((fw4a >> 2) + ss_hor) >> ss_hor;
    const int fh4 = luma ? (fh4a >> 2) : ((fh4a >> 2) + ss_ver) >> ss_ver;
    stbv_u32 *cmap = luma ? 0 : rc->lf_blkid_c;
    int tile_x0 = luma ? rc->tile_x4 : (rc->tile_x4 >> ss_hor);
    int tile_y0 = luma ? rc->tile_y4 : (rc->tile_y4 >> ss_ver);
    int tile_x1 = luma ? (rc->tile_x4 + rc->tile_w4) :
        ((rc->tile_x4 + rc->tile_w4 + ss_hor) >> ss_hor);
    int tile_y1 = luma ? (rc->tile_y4 + rc->tile_h4) :
        ((rc->tile_y4 + rc->tile_h4 + ss_ver) >> ss_ver);
    int fl = 0;

    /* Coordinates are in the plane being predicted. Chroma coverage is
     * stored on the luma 4x4 grid, so a chroma unit maps to a 2x2 luma
     * footprint for 4:2:0. */
    if (x4 < 0 || y4 < 0 || x4 >= fw4 || y4 >= fh4)
        return 0;

    if (y4 > tile_y0 && x4 + tr_run < fw4 && x4 + tr_run < tile_x1) {
        int qx = x4 + w4;
        int qy = y4 - 1;
        if (luma) {
            if (rc->lf_done && qx >= 0 && qx < rc->lf_mapw4 &&
                qy >= 0 && qy < rc->lf_maph4 &&
                rc->lf_done[(size_t)qy * rc->lf_b4stride + qx])
                fl |= STBV_AV1_EDGE_I444_TOP_HAS_RIGHT;
        } else if (cmap) {
            int mx = qx << ss_hor;
            int my = qy << ss_ver;
            if (mx >= 0 && mx < rc->lf_mapw4 &&
                my >= 0 && my < rc->lf_maph4 &&
                cmap[(size_t)my * rc->lf_b4stride + mx] != 0xffffffffU)
                fl |= STBV_AV1_EDGE_I444_TOP_HAS_RIGHT;
        }
    }
    if (x4 > tile_x0 && y4 + bl_run < fh4 && y4 + bl_run < tile_y1) {
        int qx = x4 - 1;
        int qy = y4 + h4;
        if (luma) {
            if (rc->lf_done && qx >= 0 && qx < rc->lf_mapw4 &&
                qy >= 0 && qy < rc->lf_maph4 &&
                rc->lf_done[(size_t)qy * rc->lf_b4stride + qx])
                fl |= STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM;
        } else if (cmap) {
            int mx = qx << ss_hor;
            int my = qy << ss_ver;
            if (mx >= 0 && mx < rc->lf_mapw4 &&
                my >= 0 && my < rc->lf_maph4 &&
                cmap[(size_t)my * rc->lf_b4stride + mx] != 0xffffffffU)
                fl |= STBV_AV1_EDGE_I444_LEFT_HAS_BOTTOM;
        }
    }
    return fl;
}

/* dav1d sm_flag()/sm_uv_flag(): ANGLE_SMOOTH_EDGE_FLAG when the neighbour
 * block (intra) uses one of the SMOOTH modes. */
static int stb_avif_recon_smooth_flag(const stbv_u8 *arr, unsigned int n,
                                      int idx)
{
    int m;
    if (!arr || (unsigned)idx >= n) return 0;
    m = arr[idx];
    return (m == STBV_AV1_INTRA_SMOOTH || m == STBV_AV1_INTRA_SMOOTH_V ||
            m == STBV_AV1_INTRA_SMOOTH_H) ? 512 : 0;
}

static int stb_avif_recon_edge_flags(struct stb_avif_scalar_recon *rc,
                                     int luma, int x4, int y4)
{
    int fl = rc->intra_edge_filter << 10;
    if (luma) {
        fl |= stb_avif_recon_smooth_flag(rc->above_mode, rc->above_n, x4);
        fl |= stb_avif_recon_smooth_flag(rc->left_mode, rc->left_n, y4);
    } else {
        fl |= stb_avif_recon_smooth_flag(rc->above_uvmode,
                                         rc->ss_hor ? (rc->above_n + 1) >> 1
                                                    : rc->above_n, x4);
        fl |= stb_avif_recon_smooth_flag(rc->left_uvmode,
                                         rc->ss_ver ? (rc->left_n + 1) >> 1
                                                    : rc->left_n, y4);
    }
    return fl;
}

/* Full-block intra prediction, written straight into the frame planes.
 * Runs at block_info time (before coefficients) so skipped transforms keep
 * a valid prediction; txb callbacks then add residual on top.
 * NOTE: stbv_av1_prepare_intra_edges_8 takes x/y/w/h in 4x4 units. */
static void stb_avif_recon_predict_block(struct stb_avif_scalar_recon *rc,
                                         int ss_hor, int ss_ver,
                                         int bx4, int by4, int bw4, int bh4,
                                         int has_chroma,
                                         int y_mode, int y_angle, int uv_mode)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    /* 8-aligned extent (dav1d f->bw/f->bh): prediction edge prep must
     * reach the reconstructed padded row/column. */
    const int fw4 = (rc->frame_w + 7) >> 3 << 1;
    const int fh4 = (rc->frame_h + 7) >> 3 << 1;
    int bw4c, bh4c, i;

    bw4c = fw4 - bx4; if (bw4c > bw4) bw4c = bw4;
    bh4c = fh4 - by4; if (bh4c > bh4) bh4c = bh4;
    if (bw4c <= 0 || bh4c <= 0) return;

    /* Luma prediction */
    {
        int x = bx4 << 2;
        int y = by4 << 2;
        int w = bw4 << 2;
        int h = bh4 << 2;
        int cw, ch;
        int mode = y_mode;
        int angle = y_angle;
        /* Intra-mode FILTER shares numeric 14 with IPRED_TOP_DC; map to the
         * real ipred FILTER and carry the filter-set index (y_angle). */
        int bd = rc->bit_depth;
        int impl;
        const int filt_idx = (mode == STBV_AV1_INTRA_FILTER) ? y_angle : 0;

        cw = rc->frame_w - x; if (cw > w) cw = w;
        ch = rc->frame_h - y; if (ch > h) ch = h;
        if (mode == STBV_AV1_INTRA_FILTER)
            mode = STBV_AV1_IPRED_FILTER;
        if (cw <= 0 || ch <= 0) return;
                impl = stbv_av1_prepare_intra_edges_16(bx4, stb_avif_recon_have_left(rc, 1, bx4),
                                              by4, stb_avif_recon_have_top(rc, 1, by4),
                                              fw4, fh4,
                                              stb_avif_recon_block_edge_flags(rc, 1, bx4, by4, bw4c, bh4c),
                                              rc->plane_y + y * rc->stride_y + x,
                                              rc->stride_y, NULL,
                                              mode, &angle,
                                              bw4c, bh4c,
                                               rc->intra_edge_filter, edge, bd);
        stbv_av1_ipred_run_16(impl, rc->pred, w, edge, w, h,
                             angle | stb_avif_recon_edge_flags(rc, 1, bx4, by4),
                             filt_idx, rc->frame_w - x, rc->frame_h - y, bd);
        for (i = 0; i < ch; i++)
            memcpy(rc->plane_y + (y + i) * rc->stride_y + x,
                   rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
    }

    /* Chroma prediction (chroma coords are the luma ones shifted) */
    if (has_chroma && rc->plane_u && rc->plane_v && uv_mode >= 0)
    {
        const int cfw4 = (fw4 + ss_hor) >> ss_hor;
        const int cfh4 = (fh4 + ss_ver) >> ss_ver;
        int cx4 = bx4 >> ss_hor;
        int cy4 = by4 >> ss_ver;
        int cbw4 = (bw4c + ss_hor) >> ss_hor;
        int cbh4 = (bh4c + ss_ver) >> ss_ver;
        int cm = uv_mode == STBV_AV1_INTRA_CFL ? STBV_AV1_INTRA_DC : uv_mode;
        int cangle = 0;
        int cimpl;
        int x = cx4 << 2;
        int y = cy4 << 2;
        int w = cbw4 << 2;
        int h = cbh4 << 2;
        int cw, ch;
        if (cbw4 <= 0 || cbh4 <= 0 || cx4 >= cfw4 || cy4 >= cfh4) return;
        cbw4 = cfw4 - cx4; if (cbw4 > (w >> 2)) cbw4 = w >> 2;
        cbh4 = cfh4 - cy4; if (cbh4 > (h >> 2)) cbh4 = h >> 2;
        if (cbw4 <= 0 || cbh4 <= 0) return;
        w = cbw4 << 2;
        h = cbh4 << 2;
        cw = ((rc->frame_w + ss_hor) >> ss_hor) - x; if (cw > w) cw = w;
        ch = ((rc->frame_h + ss_ver) >> ss_ver) - y; if (ch > h) ch = h;
        if (cw <= 0 || ch <= 0) return;
    /* Predict U and V separately: each plane has different reference edges. */
    {
        int pl_idx;
        for (pl_idx = 0; pl_idx < 2; pl_idx++) {
            stbv_u16 *cur_plane = pl_idx == 0 ? rc->plane_u : rc->plane_v;
            int cur_stride = pl_idx == 0 ? rc->stride_u : rc->stride_v;
            cimpl = stbv_av1_prepare_intra_edges_16(cx4, stb_avif_recon_have_left(rc, 0, cx4),
                                                   cy4, stb_avif_recon_have_top(rc, 0, cy4),
                                                   cfw4, cfh4,
                                                   stb_avif_recon_block_edge_flags(rc, 0, cx4, cy4, cbw4, cbh4),
                                                   cur_plane + y * cur_stride + x,
                                                   cur_stride, NULL,
                                                   cm, &cangle,
                                                   cbw4, cbh4,
                                                   rc->intra_edge_filter,
                                                   edge, rc->bit_depth);
            stbv_av1_ipred_run_16(cimpl, rc->pred, w, edge, w, h,
                                  cangle | stb_avif_recon_edge_flags(rc, 0, bx4, by4),
                                  0, (cfw4 - cx4) << 2, (cfh4 - cy4) << 2, rc->bit_depth);
            for (i = 0; i < ch; i++) {
                memcpy(cur_plane + (y + i) * cur_stride + x,
                       rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
            }
        }
    }
    }
}

static void stb_avif_recon_block_info(void *ud, int intra, int bs, int bx4, int by4, int has_chroma, int cbw4, int cbh4, int uv_tx, int tx0, int pal_sz_y, int pal_sz_uv, int skip, int y_mode, int y_angle, int uv_mode, int uv_angle, int cfl_alpha_u, int cfl_alpha_v, int ibc_mv_y, int ibc_mv_x)
{
    struct stb_avif_scalar_recon *rc;
    int bw4, bh4;
    (void)cbw4; (void)cbh4; (void)uv_tx;
    (void)pal_sz_y; (void)pal_sz_uv;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    /* Always record position so luma_txb/luma_pal can fill lf_blkid. */
    rc->cur_bx4 = bx4;
    rc->cur_by4 = by4;
    bw4 = stbv_av1_block_dimensions[bs][0];
    bh4 = stbv_av1_block_dimensions[bs][1];
    rc->cur_bw4 = bw4;
    rc->cur_bh4 = bh4;
    rc->cur_ltw4 = stbv_av1_tx_dims[tx0 >= 0 && tx0 < STBV_AV1_N_TX_SIZES
                                   ? tx0 : 0].w;
    rc->cur_lth4 = stbv_av1_tx_dims[tx0 >= 0 && tx0 < STBV_AV1_N_TX_SIZES
                                   ? tx0 : 0].h;
    rc->block_skip = skip;
    rc->has_chroma = has_chroma;
    if (!intra) {
        /* IBC block: copy already-reconstructed pixels from the current
         * frame at the reference position indicated by the MV.  MV is in
         * 1/8-pel luma units; for IBC it is always integer-pixel aligned
         * (mv_prec=-1). */
        int src_px_x, src_px_y;
        int dst_px_x, dst_px_y;
        int pw, ph, cw, ch;
        int ss_h = rc->ss_hor, ss_v = rc->ss_ver;
        int i;
        rc->is_ibc = 1;
        rc->ibc_mv_y = ibc_mv_y;
        rc->ibc_mv_x = ibc_mv_x;
        rc->y_mode = STBV_AV1_INTRA_DC;
        rc->y_angle = 0;
        rc->uv_mode = STBV_AV1_INTRA_DC;
        rc->uv_angle = 0;
        rc->cfl_alpha_u = 0;
        rc->cfl_alpha_v = 0;
        rc->pal_y = 0;
        rc->pal_uv = 0;
        /* Source position in pixel coords (matching dav1d mc: dx = bx*4 + mvx>>3). */
        src_px_x = bx4 * 4 + (ibc_mv_x >> 3);
        src_px_y = by4 * 4 + (ibc_mv_y >> 3);
        dst_px_x = bx4 * 4;
        dst_px_y = by4 * 4;
        pw = bw4 << 2;
        ph = bh4 << 2;
        /* Clamp to frame bounds. */
        if (src_px_x < 0) { pw += src_px_x; src_px_x = 0; }
        if (src_px_y < 0) { ph += src_px_y; src_px_y = 0; }
        if (src_px_x + pw > rc->frame_w) pw = rc->frame_w - src_px_x;
        if (src_px_y + ph > rc->frame_h) ph = rc->frame_h - src_px_y;
        cw = rc->frame_w - dst_px_x; if (cw > pw) cw = pw;
        ch = rc->frame_h - dst_px_y; if (ch > ph) ch = ph;
        /* Reference-availability check: source must be within decoded
         * region (above or to the left of current position).  AV1 decode
         * order guarantees this when src is within the current tile. */
        if (cw > 0 && ch > 0 && rc->plane_y) {
            for (i = 0; i < ch; i++)
                memmove(rc->plane_y + (size_t)(dst_px_y + i) * rc->stride_y + dst_px_x,
                       rc->plane_y + (size_t)(src_px_y + i) * rc->stride_y + src_px_x,
                       (size_t)cw * sizeof(stbv_u16));
        }
        /* Copy chroma if present. */
        if (has_chroma && rc->plane_u && rc->plane_v) {
            int cbx4_dst = bx4 >> ss_h;
            int cby4_dst = by4 >> ss_v;
            int cx_dst = cbx4_dst * 4;
            int cy_dst = cby4_dst * 4;
            int luma_src_x = cbx4_dst * (4 << ss_h) + (ibc_mv_x >> 3);
            int luma_src_y = cby4_dst * (4 << ss_v) + (ibc_mv_y >> 3);
            int cx_src = luma_src_x >> ss_h;
            int cy_src = luma_src_y >> ss_v;
            int cw4 = (bw4 + ss_h) >> ss_h;
            int ch4 = (bh4 + ss_v) >> ss_v;
            int cpw = cw4 << 2;
            int cph = ch4 << 2;
            int ccw, cch, j;
            if (cx_src < 0) { cpw += cx_src; cx_src = 0; }
            if (cy_src < 0) { cph += cy_src; cy_src = 0; }
            ccw = ((rc->frame_w + ss_h) >> ss_h) - cx_dst; if (ccw > cpw) ccw = cpw;
            cch = ((rc->frame_h + ss_v) >> ss_v) - cy_dst; if (cch > cph) cch = cph;
            if (ccw > 0 && cch > 0) {
                for (j = 0; j < 2; j++) {
                    stbv_u16 *plane = j == 0 ? rc->plane_u : rc->plane_v;
                    int stride = j == 0 ? rc->stride_u : rc->stride_v;
                    if (!plane) continue;
                    for (i = 0; i < cch; i++)
                        memmove(plane + (size_t)(cy_dst + i) * stride + cx_dst,
                               plane + (size_t)(cy_src + i) * stride + cx_src,
                               (size_t)ccw * sizeof(stbv_u16));
                }
            }
        }
        /* Fill lf_blkid for IBC skip blocks (same logic as intra skip). */
        if (skip && rc->lf_blkid && bw4 > 0 && bh4 > 0) {
            stbv_u32 blkid = ((stbv_u32)bx4 << 16) | (stbv_u32)by4;
            int ii, jj;
            for (ii = 0; ii < bh4 && (by4 + ii) < rc->lf_maph4; ii++)
                for (jj = 0; jj < bw4 && (bx4 + jj) < rc->lf_mapw4; jj++) {
                    size_t off = (size_t)(by4 + ii) * rc->lf_b4stride + (bx4 + jj);
                    rc->lf_blkid[off] = blkid;
                    rc->lf_txlw[off] = rc->cur_ltw4;
                    rc->lf_done[off] = 1;
                }
        }
        if (skip && has_chroma && rc->lf_blkid_c && bw4 > 0 && bh4 > 0) {
            int cbx4 = bx4 >> ss_h;
            int cby4 = by4 >> ss_v;
            int cbw4_u = (bw4 + ss_h) >> ss_h;
            int cbh4_u = (bh4 + ss_v) >> ss_v;
            int ii, jj;
            for (ii = 0; ii < cbh4_u && (cby4 + ii) < rc->lf_maph4; ii++)
                for (jj = 0; jj < cbw4_u && (cbx4 + jj) < rc->lf_mapw4; jj++) {
                    size_t off = (size_t)(cby4 + ii) * rc->lf_b4stride + (cbx4 + jj);
                    rc->lf_blkid_c[off] = ((stbv_u32)cbx4 << 16) | (stbv_u32)cby4;
                    rc->lf_txlw_c[off] = rc->cur_ltw4;
                }
        }
        return;
    }
    rc->is_ibc = 0;
    rc->y_mode = y_mode;
    rc->y_angle = y_angle;
    rc->uv_mode = uv_mode;
    rc->uv_angle = uv_angle;
    rc->cfl_alpha_u = cfl_alpha_u;
    rc->cfl_alpha_v = cfl_alpha_v;
    rc->block_skip = skip;
    rc->has_chroma = has_chroma;
    rc->pal_y = pal_sz_y;
    rc->pal_uv = pal_sz_uv;
    /* dav1d predicts PER TRANSFORM so every txb sees freshly reconstructed
     * neighbours; the txb callbacks do that.  Whole-block-skip blocks never
     * reach the txb callbacks, so predict them in one shot here (no residual
     * will be interleaved). */
    if (skip && !pal_sz_y)
        stb_avif_recon_predict_block(rc, rc->ss_hor, rc->ss_ver,
                                     bx4, by4, rc->cur_bw4, rc->cur_bh4,
                                     has_chroma,
                                     y_mode, y_angle, uv_mode);
    /* Skip blocks never reach luma_txb/chroma_txb, so their lf_blkid map
     * entries would stay zero (calloc default), colliding with blkid=0 at
     * (0,0) and creating false deblocking edges.  Fill the map here. */
    if (skip && rc->lf_blkid && rc->cur_bw4 > 0 && rc->cur_bh4 > 0) {
        stbv_u32 blkid = ((stbv_u32)bx4 << 16) | (stbv_u32)by4;
        int i, j;
        for (i = 0; i < rc->cur_bh4 && (by4 + i) < rc->lf_maph4; i++)
            for (j = 0; j < rc->cur_bw4 && (bx4 + j) < rc->lf_mapw4; j++) {
                size_t off = (size_t)(by4 + i) * rc->lf_b4stride + (bx4 + j);
                rc->lf_blkid[off] = blkid;
                rc->lf_txlw[off] = rc->cur_ltw4;
                rc->lf_done[off] = 1;
            }
    }
    if (skip && has_chroma && rc->lf_blkid_c && rc->cur_bw4 > 0 && rc->cur_bh4 > 0) {
        int cbx4 = bx4 >> rc->ss_hor;
        int cby4 = by4 >> rc->ss_ver;
        int cbw4_u = (rc->cur_bw4 + rc->ss_hor) >> rc->ss_hor;
        int cbh4_u = (rc->cur_bh4 + rc->ss_ver) >> rc->ss_ver;
        int i, j;
        for (i = 0; i < cbh4_u && (cby4 + i) < rc->lf_maph4; i++)
            for (j = 0; j < cbw4_u && (cbx4 + j) < rc->lf_mapw4; j++) {
                size_t off = (size_t)(cby4 + i) * rc->lf_b4stride + (cbx4 + j);
                rc->lf_blkid_c[off] = ((stbv_u32)cbx4 << 16) | (stbv_u32)cby4;
                rc->lf_txlw_c[off] = rc->cur_ltw4;
            }
    }
}

    /* Per-transform-block intra prediction written into the plane.
     * px4/py4/tw4/th4 are 4x4 units in the given plane's coordinate system;
     * pw/ph are the plane's pixel dimensions. */
    /* Palette blocks own their plane pixels: dav1d applies the palette
     * instead of intra prediction (recon_b_intra goto skip_y_pred), so
     * never let txb prediction clobber it here. */
static void stb_avif_recon_pred_rect(struct stb_avif_scalar_recon *rc,
                                     stbv_u16 *plane, int stride,
                                     int px4, int py4, int tw4, int th4,
                                     int pw, int ph,
                                     int mode_in, int angle_in)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    const int fw4 = (pw + 3) >> 2;
    const int fh4 = (ph + 3) >> 2;
    int w = tw4 << 2;
    int h = th4 << 2;
    int cw, ch, i;
    int mode = mode_in;
    int angle = angle_in;
    int impl;
    const int filt_idx = (mode == STBV_AV1_INTRA_FILTER) ? angle : 0;
    if (mode == STBV_AV1_INTRA_FILTER)
        mode = STBV_AV1_IPRED_FILTER;

    if (tw4 <= 0 || th4 <= 0 || px4 >= fw4 || py4 >= fh4) return;
    if (tw4 > fw4 - px4) tw4 = fw4 - px4;
    if (th4 > fh4 - py4) th4 = fh4 - py4;
    cw = pw - (px4 << 2); if (cw > w) cw = w;
    ch = ph - (py4 << 2); if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) return;

    impl = stbv_av1_prepare_intra_edges_16(px4, stb_avif_recon_have_left(rc, 1, px4),
                                          py4, stb_avif_recon_have_top(rc, 1, py4),
                                          fw4, fh4, 0,
                                          plane + (py4 << 2) * stride + (px4 << 2),
                                          stride, NULL,
                                          mode, &angle,
                                          tw4, th4, 0, edge, rc->bit_depth);
    stbv_av1_ipred_run_16(impl, rc->pred, w, edge, w, h, angle, filt_idx,
                         w, h, rc->bit_depth);
    for (i = 0; i < ch; i++)
        memcpy(plane + ((py4 << 2) + i) * stride + (px4 << 2),
               rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
}

/* Residual add: copy plane region to scratch, inverse-transform on top of it
 * (stbv_av1_inv_txfm_add8 adds residual in place), copy back clipped. */
static void stb_avif_recon_add_res(struct stb_avif_scalar_recon *rc,
                                   stbv_u16 *plane, int stride,
                                   int px, int py, int pw, int ph,
                                   int tx, int txtp, int eob, stbv_i32 *cf)
{
    int w = stbv_av1_tx_dims[tx].w << 2;
    int h = stbv_av1_tx_dims[tx].h << 2;
    int cw, ch, i;
    /* Clip against BUFFER capacity (stride/rows incl. padding), not the
     * visible frame: reconstruction must fill the full padded tx extent so
     * CFL AC gather on adjacent edge blocks never reads zero padding.
     * ph is the ALLOCATED row count (visible + pad). */
    cw = (stride - px); if (cw > w) cw = w;
    ch = ph - py; if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) return;
    for (i = 0; i < ch; i++) {
        memcpy(rc->pred + i * w, plane + (py + i) * stride + px,
               (size_t)(cw * sizeof(stbv_u16)));
        memset(rc->pred + i * w + cw, 0,
                              (size_t)((w - cw) * sizeof(stbv_u16)));
    }


    stbv_av1_inv_txfm_add16(rc->pred, w, cf, eob, tx, txtp, rc->bit_depth);
    for (i = 0; i < ch; i++)
        memcpy(plane + (py + i) * stride + px, rc->pred + i * w,
               (size_t)(cw * sizeof(stbv_u16)));
}

static void stb_avif_recon_predict_txb_luma(struct stb_avif_scalar_recon *rc, int x4, int y4, int tx);
static void stb_avif_recon_predict_txb_chroma(struct stb_avif_scalar_recon *rc, int pl, int x4, int y4, int tx);

static void stb_avif_extend_right_edge_u16(stbv_u16 *plane, int stride,
                                           int frame_w, int frame_h,
                                           int x4, int y4, int tx)
{
    int x0 = x4 << 2;
    int y0 = y4 << 2;
    int tw = stbv_av1_tx_dims[tx].w << 2;
    int th = stbv_av1_tx_dims[tx].h << 2;
    int aw = (frame_w + 7) & ~7;
    int yy, xx;
    if (!plane || x0 + tw < frame_w || x0 >= aw || y0 >= frame_h)
        return;
    if (y0 + th > frame_h) th = frame_h - y0;
    for (yy = 0; yy < th; yy++) {
        stbv_u16 v = plane[(size_t)(y0 + yy) * stride + frame_w - 1];
        for (xx = frame_w; xx < aw; xx++)
            plane[(size_t)(y0 + yy) * stride + xx] = v;
    }
}

static void stb_avif_recon_luma_txb(void *ud, int x4, int y4, int tx, int txtp, int eob, stbv_i32 *cf)
{
    struct stb_avif_scalar_recon *rc;
    int txw4 = stbv_av1_tx_dims[tx].w;
    int txh4 = stbv_av1_tx_dims[tx].h;
    (void)eob;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    (void)txw4; (void)txh4;
#ifdef STB_AVIF_PRED_ONLY
    (void)cf; (void)tx; (void)txtp;
#else
#ifndef STB_AVIF_NO_RESIDUAL
    /* Per-transform prediction from currently reconstructed neighbours
     * (dav1d recon_b_intra: intra_pred -> coefs -> itxfm_add).
     * Intra-frame "skip" suppresses only the residual; the prediction
     * itself must always be written.
     * For IBC blocks the reference pixels were already copied into the
     * plane by block_info, so skip intra prediction. */
    if (!rc->pal_y && !rc->is_ibc) {
        stb_avif_recon_predict_txb_luma(rc, x4, y4, tx);
        {
            int _w = stbv_av1_tx_dims[tx].w << 2;
            int _h = stbv_av1_tx_dims[tx].h << 2;
            int _cw = rc->stride_y - (x4 << 2);
            int _ch = (rc->frame_h + 64) - (y4 << 2);
            int _i;
            if (_cw > _w) _cw = _w;
            if (_ch > _h) _ch = _h;
            if (_cw > 0 && _ch > 0) {
                for (_i = 0; _i < _ch; _i++)
                    memcpy(rc->plane_y + (size_t)((y4 << 2) + _i) * rc->stride_y + (x4 << 2),
                           rc->pred + _i * _w, (size_t)(_cw * sizeof(stbv_u16)));
            }
        }
    }
    /* eob is dav1d-style 0-based LAST-coefficient index: 0 == DC-only
     * (coefficients present!), < 0 == none. */
    if (!rc->block_skip && eob >= 0)
    {
        stb_avif_recon_add_res(rc, rc->plane_y, rc->stride_y,
                               x4 << 2, y4 << 2, rc->frame_w,
                               rc->frame_h + 64,
                                 tx, txtp, eob, cf);
    }
    {
        /* record transform coverage for the deblocking pass */
        if (rc->lf_blkid) {
            int tw = stbv_av1_tx_dims[tx].w, th = stbv_av1_tx_dims[tx].h;
            int lx, ly;
            stbv_u32 id = ((stbv_u32)rc->cur_bx4 << 16) |
                          (stbv_u32)rc->cur_by4;
            for (ly = y4; ly < y4 + th && ly < rc->lf_maph4; ly++)
                for (lx = x4; lx < x4 + tw && lx < rc->lf_mapw4; lx++) {
                    rc->lf_blkid[(size_t)ly * rc->lf_b4stride + lx] = id;
                    rc->lf_txlw[(size_t)ly * rc->lf_b4stride + lx] =
                        (stbv_u8)stbv_av1_tx_dims[tx].lw;
                    rc->lf_done[(size_t)ly * rc->lf_b4stride + lx] = 1;
                }
        }
    }
    stb_avif_extend_right_edge_u16(rc->plane_y, rc->stride_y,
                                   rc->frame_w, rc->frame_h, x4, y4, tx);
#endif
#endif
}

/* Per-txb luma prediction straight into the plane. */
static void stb_avif_recon_predict_txb_luma(struct stb_avif_scalar_recon *rc,
                                            int x4, int y4, int tx)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    /* 8-aligned extent (dav1d f->bw/f->bh): prediction edge prep must
     * reach the reconstructed padded row/column. */
    const int fw4 = (rc->frame_w + 7) >> 3 << 1;
    const int fh4 = (rc->frame_h + 7) >> 3 << 1;
    int w = stbv_av1_tx_dims[tx].w << 2;
    int h = stbv_av1_tx_dims[tx].h << 2;
    int cw, ch, i;
    int mode = rc->y_mode;
    int angle = rc->y_angle;
    const int filt_idx = (mode == STBV_AV1_INTRA_FILTER) ? rc->y_angle : 0;
    int bd = rc->bit_depth;
    int impl;
    /* dav1d recon_b_intra hands prepare_intra_edges a snapshot of the row
     * above at every superblock top boundary and that path reads it
     * UNCLIPPED across the padded width; all other blocks read dst[-stride]
     * clipped to the 8-aligned frame width with edge replication.  We are
     * single-threaded and the plane row above is exactly what dav1d would
     * have snapshotted, so gate the same way and pass the row directly. */
    const stbv_u16 *sb_edge =
        (y4 > rc->tile_y4 && !(y4 & (rc->sb_step4 - 1))) ?
            rc->plane_y + (size_t)((y4 << 2) - 1) * rc->stride_y : NULL;

    if (mode == STBV_AV1_INTRA_FILTER)
        mode = STBV_AV1_IPRED_FILTER;
    if (x4 >= fw4 || y4 >= fh4) return;
    /* Write the FULL tx extent into the padded plane: later blocks'
     * intra edges read these pixels (dav1d recon_b_intra does the same).
     * Clip against BUFFER capacity (stride/allocated rows), never against
     * the 8-aligned frame size: clipping here left unwritten columns past
     * right-edge txbs, and the zeroed loads poisoned every subsequent
     * prediction that gathered its top edge across them (the dark
     * triangle / green fog). */
    cw = rc->stride_y - (x4 << 2); if (cw > w) cw = w;
    ch = (rc->frame_h + 64) - (y4 << 2); if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) return;
    impl = stbv_av1_prepare_intra_edges_16(x4, stb_avif_recon_have_left(rc, 1, x4),
                                          y4, stb_avif_recon_have_top(rc, 1, y4),
                                          fw4, fh4,
                                          stb_avif_recon_txb_edge_flags(
                                              rc, 1, rc->cur_bx4, rc->cur_by4,
                                              rc->cur_bw4, rc->cur_bh4,
                                              x4, y4,
                                              stbv_av1_tx_dims[tx].w,
                                              stbv_av1_tx_dims[tx].h),
                                          rc->plane_y + (y4 << 2) * rc->stride_y +
                                          (x4 << 2),
                                          rc->stride_y, sb_edge,
                                          mode, &angle,
                                          stbv_av1_tx_dims[tx].w,
                                          stbv_av1_tx_dims[tx].h,
                                          rc->intra_edge_filter, edge, bd);
    stbv_av1_ipred_run_16(impl, rc->pred, w, edge, w, h,
                          angle | stb_avif_recon_edge_flags(rc, 1, rc->cur_bx4, rc->cur_by4),
                          filt_idx, rc->frame_w - (x4 << 2), rc->frame_h - (y4 << 2), bd);
    for (i = 0; i < ch; i++)
        memcpy(rc->plane_y + ((y4 << 2) + i) * rc->stride_y + (x4 << 2),
               rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
}

static void stb_avif_recon_chroma_txb(void *ud, int pl, int x4, int y4, int tx, int txtp, int eob, stbv_i32 *cf)
{
    struct stb_avif_scalar_recon *rc;
    stbv_u16 *plane;
    int stride, pw, ph;
    int txw4 = stbv_av1_tx_dims[tx].w;
    int txh4 = stbv_av1_tx_dims[tx].h;
    (void)eob;
    rc = (struct stb_avif_scalar_recon *)ud;
    rc->cur_pl = pl;
    if (!rc || !rc->plane_u || !rc->plane_v) return;
    plane = pl == 0 ? rc->plane_u : rc->plane_v;
    stride = pl == 0 ? rc->stride_u : rc->stride_v;
    /* Chroma plane extent follows ACTUAL subsampling: full width for
     * 4:2:2/4:4:4, half height for 4:2:2/4:2:0. Hardcoded >>1 broke
     * 4:2:2 (right half of chroma never written). */
    /* pw unused for clipping now; ph = ALLOCATED chroma rows. */
    pw = (rc->frame_w + rc->ss_hor) >> rc->ss_hor;
    ph = ((rc->frame_h + rc->ss_ver) >> rc->ss_ver) + 32;
    (void)txw4; (void)txh4;
#ifdef STB_AVIF_PRED_ONLY
    (void)cf; (void)tx; (void)txtp;
#else
#ifndef STB_AVIF_NO_RESIDUAL
    /* Prediction always runs for intra; "skip" only suppresses the
     * residual (dav1d recon_b_intra semantics).  eob >= 0: DC-only
     * still carries a coefficient.  UV-palette blocks own the chroma
     * planes instead.  IBC blocks already have reference pixels in the
     * plane (copied by block_info), so skip intra prediction. */
    if (!rc->pal_uv && !rc->is_ibc) {
        stb_avif_recon_predict_txb_chroma(rc, pl, x4, y4, tx);
        /* Copy prediction from rc->pred into the plane so add_res adds
         * residual on top of the prediction, not stale plane data. */
        {
            int _w = stbv_av1_tx_dims[tx].w << 2;
            int _h = stbv_av1_tx_dims[tx].h << 2;
            int _cw = stride - (x4 << 2);
            int _ch = (ph + 32) - (y4 << 2);
            int _i;
            if (_cw > _w) _cw = _w;
            if (_ch > _h) _ch = _h;
            if (_cw > 0 && _ch > 0) {
                for (_i = 0; _i < _ch; _i++)
                    memcpy(plane + (size_t)((y4 << 2) + _i) * stride + (x4 << 2),
                           rc->pred + _i * _w, (size_t)(_cw * sizeof(stbv_u16)));
            }
        }
    }
    if (!rc->block_skip && eob >= 0)
        stb_avif_recon_add_res(rc, plane, stride,
                               x4 << 2, y4 << 2, stride, ph + 32,
                               tx, txtp, eob, cf);
    if (pl == 0)
        stb_avif_extend_right_edge_u16(rc->plane_u, rc->stride_u, pw, ph, x4, y4, tx);
    else
        stb_avif_extend_right_edge_u16(rc->plane_v, rc->stride_v, pw, ph, x4, y4, tx);
    if (pl == 0 && rc->lf_blkid_c && rc->has_chroma) {
        /* record this chroma txb's own extent (mapped to luma units);
         * identity = chroma-plane origin so chroma-internal boundaries
         * remain visible to the deblocker */
        int tw = stbv_av1_tx_dims[tx].w << rc->ss_hor;
        int th = stbv_av1_tx_dims[tx].h << rc->ss_ver;
        int lx0 = x4 << rc->ss_hor, ly0 = y4 << rc->ss_ver;
        int lx, ly;
        stbv_u32 id = ((stbv_u32)x4 << 16) | (stbv_u32)y4;
        for (ly = ly0; ly < ly0 + th && ly < rc->lf_maph4; ly++)
            for (lx = lx0; lx < lx0 + tw && lx < rc->lf_mapw4; lx++) {
                rc->lf_blkid_c[(size_t)ly * rc->lf_b4stride + lx] = id;
                rc->lf_txlw_c[(size_t)ly * rc->lf_b4stride + lx] =
                    (stbv_u8)stbv_av1_tx_dims[tx].lw;
            }
    }
#endif
#endif
}

/* Per-txb chroma prediction (UV mode; CFL currently falls back to DC). */
static void stb_avif_recon_predict_txb_chroma(struct stb_avif_scalar_recon *rc,
                                               int pl, int x4, int y4, int tx)
{
    stbv_u16 tl[640];
    stbv_u16 *edge = tl + 320;
    stbv_u16 *plane;
    int stride;
    const int pw = (rc->frame_w + rc->ss_hor) >> rc->ss_hor;
    const int ph = (rc->frame_h + rc->ss_ver) >> rc->ss_ver;
    /* 8-aligned (dav1d f->bw/f->bh), see note in block_edge_flags_run. */
    const int lfw4 = (rc->frame_w + 7) >> 3 << 1;
    const int lfh4 = (rc->frame_h + 7) >> 3 << 1;
    const int cfw4 = (lfw4 + rc->ss_hor) >> rc->ss_hor;
    const int cfh4 = (lfh4 + rc->ss_ver) >> rc->ss_ver;
    int cx4 = x4, cy4 = y4, cm, cangle = rc->uv_angle, cimpl;
    int w, h, cw, ch, i, j;
    if (!rc->plane_u || !rc->plane_v || !rc->has_chroma) return;
    plane = pl == 0 ? rc->plane_u : rc->plane_v;
    stride = pl == 0 ? rc->stride_u : rc->stride_v;
    cm = rc->uv_mode == STBV_AV1_INTRA_CFL ? STBV_AV1_INTRA_DC : rc->uv_mode;
    if (cx4 >= cfw4 || cy4 >= cfh4) return;
    w = stbv_av1_tx_dims[tx].w << 2;
    h = stbv_av1_tx_dims[tx].h << 2;
    /* Full padded extent (see predict_txb_luma note): clip against buffer
     * capacity, not the 8-aligned visible size. */
    cw = stride - (cx4 << 2);
    ch = ph + 32 - (cy4 << 2);
    if (cw > w) cw = w;
    if (ch > h) ch = h;
    if (cw <= 0 || ch <= 0) {
        return;
    }
    cimpl = stbv_av1_prepare_intra_edges_16(cx4, stb_avif_recon_have_left(rc, 0, cx4),
                                           cy4, stb_avif_recon_have_top(rc, 0, cy4),
                                           cfw4, cfh4,
                                           stb_avif_recon_txb_edge_flags(
                                               rc, 0, rc->cur_bx4, rc->cur_by4,
                                               rc->cur_bw4, rc->cur_bh4,
                                               x4, y4,
                                               stbv_av1_tx_dims[tx].w,
                                               stbv_av1_tx_dims[tx].h),
                                           plane + (cy4 << 2) * stride + (cx4 << 2),
                                           stride, NULL,
                                           cm, &cangle,
                                           stbv_av1_tx_dims[tx].w,
                                           stbv_av1_tx_dims[tx].h,
                                           rc->intra_edge_filter,
                                           edge, rc->bit_depth);
    stbv_av1_ipred_run_16(cimpl, rc->pred, w, edge, w, h,
                          cangle | stb_avif_recon_edge_flags(rc, 0, rc->cur_bx4 >> rc->ss_hor,
                    rc->cur_by4 >> rc->ss_ver),
                          0, (cfw4 - cx4) << 2, (cfh4 - cy4) << 2, rc->bit_depth);
    /* Chroma-from-luma, ported from dav1d recon_b_intra + cfl_ac_c +
     * cfl_pred: ONE block-wide AC array (built from the fully
     * reconstructed co-located luma) shared by both planes;
     * pred = edge-DC + alpha*ac with symmetric rounding; a plane with
     * zero alpha keeps its plain DC prediction. */
#ifdef STB_AVIF_TEST_NO_CFL
    if (0) {
#else
    if (rc->uv_mode == STBV_AV1_INTRA_CFL) {
#endif
        const int alpha = pl == 0 ? rc->cfl_alpha_u : rc->cfl_alpha_v;
        const int ss_h = rc->ss_hor, ss_v = rc->ss_ver;
        if (!alpha) {
            /* dav1d skips CFL entirely for this plane; the DC-family
             * prediction already written above is the final result. */
        } else {
            const int fw4 = (rc->frame_w + 7) >> 3 << 1;
            const int fh4 = (rc->frame_h + 7) >> 3 << 1;
            /* dav1d CFL gathers over the UNCLIPPED block dims (cbw4 =
             * b_dim-derived); clipping to the 8-aligned frame here shrank
             * right-edge blocks, corrupted the DC-subtraction and blew the
             * AC magnitudes up (saturated green fog). */
            int cw4u = rc->cur_bw4, ch4u = rc->cur_bh4;
            int cbw4, cbh4, W, H, i, j;
            const stbv_u16 mx = (stbv_u16)((1 << rc->bit_depth) - 1);
            (void)fw4; (void)fh4;
            cbw4 = (cw4u + ss_h) >> ss_h;
            cbh4 = (ch4u + ss_v) >> ss_v;
            W = cbw4 << 2;
            H = cbh4 << 2;
            if (W > 32 || H > 32 || cw4u <= 0 || ch4u <= 0) {
                /* out of contract; leave DC prediction */
            } else {
                const stbv_u16 *ysrc = rc->plane_y +
                    (((rc->cur_by4 & ~ss_v) << 2)) * rc->stride_y +
                    ((rc->cur_bx4 & ~ss_h) << 2);
                const int sh_l = 1 + !ss_v + !ss_h;
                int log2sz, x, y;
                long acc;
                if (!rc->cfl_ac_ok || rc->cfl_ac_bx != rc->cur_bx4 ||
                    rc->cfl_ac_by != rc->cur_by4) {
                    /* w_pad/h_pad from the UV transform dims (dav1d
                     * furthest_r/furthest_b use b->uvtx's t_dim);
                     * padded cols replicate. */
                    int twu = stbv_av1_tx_dims[tx].w;
                    int thu = stbv_av1_tx_dims[tx].h;
                    int furthest_r = ((cw4u << ss_h) + twu - 1) & ~(twu - 1);
                    int furthest_b = ((ch4u << ss_v) + thu - 1) & ~(thu - 1);
                    int w_pad = cbw4 - (furthest_r >> ss_h);
                    int h_pad = cbh4 - (furthest_b >> ss_v);
                    if (w_pad < 0) w_pad = 0;
                    if (h_pad < 0) h_pad = 0;
                    for (y = 0; y < H - 4 * h_pad; y++) {
                        const stbv_u16 *row0 = ysrc +
                            (y << ss_v) * rc->stride_y;
                        const stbv_u16 *row1 = row0 +
                            (ss_v ? rc->stride_y : 0);
                        for (x = 0; x < W - 4 * w_pad; x++) {
                            int s = row0[x << ss_h];
                            if (ss_h) s += row0[x * 2 + 1];
                            if (ss_v) {
                                s += row1[x << ss_h];
                                if (ss_h) s += row1[x * 2 + 1];
                            }
                            rc->cfl_ac[y * W + x] =
                                (stbv_i16)(s << sh_l);
                        }
                        for (; x < W; x++)
                            rc->cfl_ac[y * W + x] = rc->cfl_ac[y * W + x - 1];
                    }
                    for (; y < H; y++)
                        memcpy(rc->cfl_ac + y * W,
                               rc->cfl_ac + (y - 1) * W,
                               (size_t)(W * sizeof(stbv_i16)));
                    log2sz = stbv_av1_ipred_ctz((unsigned)W) +
                             stbv_av1_ipred_ctz((unsigned)H);
                    acc = (long)1 << log2sz >> 1;
                    for (y = 0; y < H; y++)
                        for (x = 0; x < W; x++)
                            acc += rc->cfl_ac[y * W + x];
                    acc >>= log2sz;
                    for (y = 0; y < H; y++)
                        for (x = 0; x < W; x++)
                            rc->cfl_ac[y * W + x] -= (stbv_i16)acc;
                    rc->cfl_ac_w = W;
                    rc->cfl_ac_h = H;
                    rc->cfl_ac_bx = rc->cur_bx4;
                    rc->cfl_ac_by = rc->cur_by4;
                    rc->cfl_ac_ok = 1;
                }
                {
                    const int off_x =
                        (x4 - (rc->cur_bx4 >> ss_h)) << 2;
                    const int off_y =
                        (y4 - (rc->cur_by4 >> ss_v)) << 2;
                    for (i = 0; i < ch; i++)
                        for (j = 0; j < cw; j++) {
                            int a = rc->cfl_ac[(off_y + i) * W + off_x + j];
                            int diff = alpha * a;
                            int adj = ((diff < 0 ? -diff : diff) + 32) >> 6;
                            int v = (int)rc->pred[i * w + j] +
                                    (diff < 0 ? -adj : adj);
                            rc->pred[i * w + j] =
                                (stbv_u16)(v < 0 ? 0 :
                                           (v > mx ? mx : v));
                        }
                }
            }
        }
    }
    for (i = 0; i < ch; i++) {
        memcpy(plane + ((cy4 << 2) + i) * stride + (cx4 << 2),
               rc->pred + i * w, (size_t)(cw * sizeof(stbv_u16)));
    }
}

static void stb_avif_recon_luma_pal(void *ud, const stbv_u8 *idx, int sz, int bw4, int bh4, const stbv_u16 *pal)
{
    struct stb_avif_scalar_recon *rc;
    /* planes are stbv_u16 */
    int x, y, w, h, cw, ch, i, j;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    x = rc->cur_bx4 << 2;
    y = rc->cur_by4 << 2;
    w = bw4 << 2;
    h = bh4 << 2;
    cw = rc->frame_w - x; if (cw > w) cw = w;
    ch = rc->frame_h - y; if (ch > h) ch = h;
    for (i = 0; i < ch; i++)
        for (j = 0; j < cw; j++) {
            int id = idx[i * w + j];
            rc->plane_y[(y + i) * rc->stride_y + x + j] =
                (stbv_u16)(id < sz ? pal[id] : 0);
        }
    /* record palette block identity for deblocking (no internal tx
     * boundaries: treat the entire palette block as one tx) */
    if (rc->lf_blkid) {
        stbv_u32 blkid = ((stbv_u32)rc->cur_bx4 << 16) |
                          (stbv_u32)rc->cur_by4;
        int bw4_log2 = 0, bh4_log2 = 0, tmp;
        tmp = bw4; while (tmp > 1) { tmp >>= 1; ++bw4_log2; }
        tmp = bh4; while (tmp > 1) { tmp >>= 1; ++bh4_log2; }
        for (i = 0; i < bh4 && (rc->cur_by4 + i) < rc->lf_maph4; i++)
            for (j = 0; j < bw4 && (rc->cur_bx4 + j) < rc->lf_mapw4; j++) {
                size_t off = (size_t)(rc->cur_by4 + i) * rc->lf_b4stride
                           + (rc->cur_bx4 + j);
                rc->lf_blkid[off] = blkid;
                rc->lf_txlw[off] = (stbv_u8)bw4_log2;
                rc->lf_done[off] = 1;
            }
        (void)bh4_log2;
    }
}

static void stb_avif_recon_chroma_pal(void *ud, int pl, const stbv_u8 *idx, int sz, int cbw4, int cbh4, const stbv_u16 *pal)
{
    struct stb_avif_scalar_recon *rc;
    int x, y, w, h, cw, ch, i, j;
    stbv_u16 *plane;
    int stride;
    rc = (struct stb_avif_scalar_recon *)ud;
    if (!rc) return;
    x = (rc->cur_bx4 >> rc->ss_hor) << 2;
    y = (rc->cur_by4 >> rc->ss_ver) << 2;
    w = cbw4 << 2;
    h = cbh4 << 2;
    plane = pl == 0 ? rc->plane_u : rc->plane_v;
    stride = pl == 0 ? rc->stride_u : rc->stride_v;
    if (!plane) return;
    cw = (((rc->frame_w + rc->ss_hor) >> rc->ss_hor)) - x; if (cw > w) cw = w;
    ch = (((rc->frame_h + rc->ss_ver) >> rc->ss_ver)) - y; if (ch > h) ch = h;
    for (i = 0; i < ch; i++)
        for (j = 0; j < cw; j++) {
            int id = idx[i * w + j];
            plane[(y + i) * stride + x + j] =
                (stbv_u16)(id < sz ? pal[id] : 0);
        }
    /* record chroma palette block identity for deblocking */
    if (pl == 0 && rc->lf_blkid_c && rc->has_chroma) {
        int cx4 = rc->cur_bx4 >> rc->ss_hor;
        int cy4 = rc->cur_by4 >> rc->ss_ver;
        int lw = cbw4 << rc->ss_hor;
        int lh = cbh4 << rc->ss_ver;
        int lx0 = cx4 << rc->ss_hor;
        int ly0 = cy4 << rc->ss_ver;
        int lx, ly;
        stbv_u32 id = ((stbv_u32)cx4 << 16) | (stbv_u32)cy4;
        int cbw4_log2 = 0, tmp;
        tmp = cbw4; while (tmp > 1) { tmp >>= 1; ++cbw4_log2; }
        for (ly = ly0; ly < ly0 + lh && ly < rc->lf_maph4; ly++)
            for (lx = lx0; lx < lx0 + lw && lx < rc->lf_mapw4; lx++) {
                rc->lf_blkid_c[(size_t)ly * rc->lf_b4stride + lx] = id;
                rc->lf_txlw_c[(size_t)ly * rc->lf_b4stride + lx] =
                    (stbv_u8)cbw4_log2;
            }
        (void)lh;
    }
}

static struct stb_avif_scalar_recon g_scalar_recon;
static stbv_av1_leaf_recon g_scalar_recon_cb;

static void stb_avif_row_reset_cb(void *opaque)
{
    stbv_av1_leaf_state_reset_row((stbv_av1_leaf_state *)opaque);
}

static int stb_avif_leaf_cb(struct stb_av1_tile_decoder *td, const struct stb_av1_tile_leaf_info *li, void *opaque)
{
    stbv_av1_leaf_state *state;
    stbv_av1_leaf_tx_result out;
    int r;
    state = (stbv_av1_leaf_state *)opaque;
    r = stbv_av1_decode_leaf_syntax(&td->msac, &td->cdf, state,
                                       td->seq, td->frame,
                                       li->bs, li->bx, li->by,
                                       &out, &g_scalar_recon_cb);
    return r;
}

static int stb_avif_decode_frame_scalar(struct stb_av1_tile_context *tc, const unsigned char *av1_data, size_t av1_size)
{
    struct stb_av1_internal_stream *stream;
    struct stbv_av1_leaf_state_arrays arrays;
    stbv_av1_leaf_state state;
    struct stb_avif_scalar_recon *recon;
    struct stb_av1_tile_decoder td;
    int r;
    int frame_w4, frame_h4, frame_w8, frame_h8;
    stbv_u8 *above_mode = 0, *left_mode = 0, *above_tx = 0, *left_tx = 0;
    stbv_u8 *above_tx_intra = 0, *left_tx_intra = 0;
    stbv_u8 *above_res = 0, *left_res = 0;
    int res_w4, res_h4;
    stbv_u32 *lf_blkid_map = 0, *lf_blkid_map_c = 0;
    stbv_u8 *lf_txlw_map = 0, *lf_txlw_map_c = 0;
    stbv_u8 *lf_done_map = 0;
    int *cdef_idx_grid = 0;
    int cdef_grid_stride = 0;
    stbv_av1_lr_mask lr_mask;
    int lr_mask_ok = 0;
    int bw8al, bh8al;
    stbv_u8 *above_cre0 = 0, *above_cre1 = 0, *left_cre0 = 0, *left_cre1 = 0;
    stbv_u8 *above_skip = 0, *left_skip = 0, *above_pal_sz = 0;
    stbv_u8 *left_pal_sz = 0, *above_pal_uv = 0, *left_pal_uv = 0;
    stbv_u8 *above_uvmode = 0, *left_uvmode = 0;
    stbv_u16 *above_pal0 = 0, *above_pal1 = 0, *left_pal0 = 0, *left_pal1 = 0;
    stbv_u16 *py16 = 0, *pu16 = 0, *pv16 = 0;
    stbv_u8 *above_seg_id = 0, *left_seg_id = 0;
    int *above_ibc_mv_y = 0, *above_ibc_mv_x = 0;
    stbv_u8 *above_ibc_valid = 0;
    int *left_ibc_mv_y = 0, *left_ibc_mv_x = 0;
    stbv_u8 *left_ibc_valid = 0;
    stbv_refmvs_cell *refmvs_r = 0;
    int cframe_w8 = 0, cframe_h8 = 0;
    int i, j, h2, w2;
    stream = (struct stb_av1_internal_stream *)stb_avif_calloc(1, sizeof(*stream));
    recon = (struct stb_avif_scalar_recon *)stb_avif_calloc(1, sizeof(*recon));
    if (!stream || !recon) { stb_avif_free(stream); stb_avif_free(recon); return -1; }
    r = stb_av1_parse_internal_stream(stream, av1_data, av1_size);
    if (r < 0 || !stream->have_seq || !stream->have_frame)
        { stb_avif_free(stream); stb_avif_free(recon); return -1; }
    if ((int)stream->frame.width[0] != tc->frame_width ||
        (int)stream->frame.height != tc->frame_height)
        { stb_avif_free(stream); stb_avif_free(recon); return -2; }
    if (!stream->tile_data || !stream->tile_size)
        { stb_avif_free(stream); stb_avif_free(recon); return -3; }

    /* grey safety net for regions the tile walk may not cover */
    for (i = 0; i < tc->frame_height; i++)
        for (j = 0; j < tc->frame_width; j++)
            tc->plane_y[i*tc->stride_y+j] = 128;
    if (tc->plane_u && tc->plane_v) {
        h2 = (tc->frame_height+1)>>1; w2 = (tc->frame_width+1)>>1;
        for (i = 0; i < h2; i++)
            for (j = 0; j < w2; j++) {
                tc->plane_u[i*tc->stride_u+j] = 128;
                tc->plane_v[i*tc->stride_v+j] = 128;
            }
    }

    frame_w4 = ((tc->frame_width + 7) >> 3) << 1;
    frame_h4 = ((tc->frame_height + 7) >> 3) << 1;
    /* Chroma neighbour contexts are indexed in 4px chroma units.  dav1d
     * keeps them per-superblock, so edge-clamped blocks still publish
     * marks for units that round past the frame edge; round the frame-
     * exact count up to a superblock multiple so those writes/reads are
     * in-bounds and identical to dav1d's. */
    frame_w8 = ((((tc->frame_width + 7) >> 3) + 15) & ~15);
    frame_h8 = ((((tc->frame_height + 7) >> 3) + 15) & ~15);
    cframe_w8 = frame_w8;
    cframe_h8 = frame_h8;
    above_mode = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_mode = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_tx = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_tx = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_tx_intra = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_tx_intra = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    /* Residual-context arrays must be superblock-padded like dav1d's
     * f->bw/f->lh row/col contexts: edge transforms write their full
     * extent (e.g. a 16x32 txb at the right frame edge marks columns
     * beyond the 8-aligned frame width) and later dc_sign/skip ctx
     * reads those positions. */
    {
        int sbstep4 = stream->seq.sb128 ? 32 : 16;
        res_w4 = ((frame_w4 + sbstep4 - 1) / sbstep4) * sbstep4;
        res_h4 = ((frame_h4 + sbstep4 - 1) / sbstep4) * sbstep4;
        bw8al = (int)(((tc->frame_width + 7U) & ~7U) >> 2);
        bh8al = (int)(((tc->frame_height + 7U) & ~7U) >> 2);
    }
    above_res = (stbv_u8*)stb_avif_calloc(res_w4, 1);
    left_res = (stbv_u8*)stb_avif_calloc(res_h4, 1);
    /* Chroma context/pal_uv arrays must cover the CHROMA plane extent
     * (== luma extent for 4:4:4), SB-rounded like dav1d's f->bw arrays;
     * frame_w8 alone only fits the subsampled case. */
    {
        int ss_h = stream->seq.ss_hor ? 1 : 0;
        int ss_v = stream->seq.ss_ver ? 1 : 0;
        int cfw4 = (frame_w4 + ss_h) >> ss_h;
        int cfh4 = (frame_h4 + ss_v) >> ss_v;
        /* Chroma context arrays are SB-padded on the CHROMA grid
         * (sbstep4>>ss per superblock), matching dav1d whose f->bw-derived
         * write clip never rejects in-extent marks. */
        int step_h = ((stream->seq.sb128 ? 32 : 16) >> ss_h);
        int step_v = ((stream->seq.sb128 ? 32 : 16) >> ss_v);
        cframe_w8 = ((cfw4 + step_h - 1) / step_h) * step_h;
        cframe_h8 = ((cfh4 + step_v - 1) / step_v) * step_v;
    }
    above_cre0 = (stbv_u8*)stb_avif_calloc(cframe_w8, 1);
    above_cre1 = (stbv_u8*)stb_avif_calloc(cframe_w8, 1);
    left_cre0 = (stbv_u8*)stb_avif_calloc(cframe_h8, 1);
    left_cre1 = (stbv_u8*)stb_avif_calloc(cframe_h8, 1);
    /* dav1d's BlockContexts default to 0x40 (reset_context) at tile init;
     * never-written positions must read as 'no residual', not zero. */
    memset(above_res, 0x40, (size_t)res_w4);
    memset(left_res, 0x40, (size_t)res_h4);
    memset(above_cre0, 0x40, (size_t)cframe_w8);
    memset(above_cre1, 0x40, (size_t)cframe_w8);
    memset(left_cre0, 0x40, (size_t)cframe_h8);
    memset(left_cre1, 0x40, (size_t)cframe_h8);
    above_skip = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_skip = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_pal_sz = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_pal_sz = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_pal_uv = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_pal_uv = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    above_uvmode = (stbv_u8*)stb_avif_calloc(
        stream->seq.ss_hor ? ((frame_w4 + 1) >> 1) : frame_w4, 1);
    left_uvmode = (stbv_u8*)stb_avif_calloc(
        stream->seq.ss_ver ? ((frame_h4 + 1) >> 1) : frame_h4, 1);
    above_pal0 = (stbv_u16*)stb_avif_calloc(frame_w4*8, 2);
    above_pal1 = (stbv_u16*)stb_avif_calloc(frame_w4*8, 2);
    left_pal0 = (stbv_u16*)stb_avif_calloc(frame_h4*8, 2);
    left_pal1 = (stbv_u16*)stb_avif_calloc(frame_h4*8, 2);
    above_seg_id = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_seg_id = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    /* IBC MV neighbour arrays for spatial prediction. */
    above_ibc_mv_y = (int*)stb_avif_calloc(frame_w4, sizeof(int));
    above_ibc_mv_x = (int*)stb_avif_calloc(frame_w4, sizeof(int));
    above_ibc_valid = (stbv_u8*)stb_avif_calloc(frame_w4, 1);
    left_ibc_mv_y = (int*)stb_avif_calloc(frame_h4, sizeof(int));
    left_ibc_mv_x = (int*)stb_avif_calloc(frame_h4, sizeof(int));
    left_ibc_valid = (stbv_u8*)stb_avif_calloc(frame_h4, 1);
    /* 2D refmvs block array for dav1d-compatible IBC MV prediction. */
    refmvs_r = (stbv_refmvs_cell*)stb_avif_calloc(
        (size_t)frame_h4 * frame_w4, sizeof(stbv_refmvs_cell));
    if (!above_mode || !left_mode || !above_tx || !left_tx || !above_res ||
        !left_res || !above_cre0 || !above_cre1 || !left_cre0 || !left_cre1 ||
        !above_skip || !left_skip || !above_pal_sz || !left_pal_sz ||
        !above_pal_uv || !left_pal_uv || !above_pal0 || !above_pal1 ||
        !left_pal0 || !left_pal1 || !above_seg_id || !left_seg_id ||
        !above_ibc_mv_y || !above_ibc_mv_x || !above_ibc_valid ||
        !left_ibc_mv_y || !left_ibc_mv_x || !left_ibc_valid ||
        !above_tx_intra || !left_tx_intra)
        { stb_avif_free(stream); stb_avif_free(recon); return -4; }
    memset(&arrays, 0, sizeof(arrays));
    arrays.above_mode = above_mode; arrays.above_mode_n = frame_w4;
    arrays.left_mode = left_mode; arrays.left_mode_n = frame_h4;
    arrays.above_tx = above_tx; arrays.above_tx_n = frame_w4;
    arrays.left_tx = left_tx; arrays.left_tx_n = frame_h4;
    arrays.above_tx_intra = above_tx_intra;
    arrays.left_tx_intra = left_tx_intra;
    arrays.above_res = above_res; arrays.above_res_n = res_w4;
    arrays.left_res = left_res; arrays.left_res_n = res_h4;
    /* dav1d's f->bw/f->bh are SB-ALIGNED unit counts, so its write clip
     * (imin(txw, f->bw - bx)) never rejects in-frame marks; context arrays
     * keep every mark including past-pixel-edge units.  Clipping disabled
     * (0 = use above_n/left_n). */
    /* dav1d recon_tmpl.c clips CODED residual-context writes to the
     * frame extent: luma imin(txw, f->bw - bx); chroma ctw =
     * imin(uvtx_w, (f->bw - bx + ss_hor) >> ss_hor).  With f->bw =
     * frame_w4 this is simply "units inside the chroma plane
     * extent". SKIP marking bypasses the clip entirely. */
    arrays.above_res_mark_n = bw8al;
    arrays.left_res_mark_n = bh8al;
    /* dav1d clips CODED residual marking to f->bw/f->bh (8-aligned px
     * width): luma imin(txw, f->bw - bx); chroma ctw =
     * imin(uvtx_w, (f->bw - bx + ss_hor) >> ss_hor).  Expressed as an
     * absolute column limit on our frame-wide arrays this is simply
     * bw8al (=f->bw) for luma and (bw8al+ss)>>ss for chroma. */
    arrays.above_cre_mark_n[0] = (bw8al + (stream->seq.ss_hor ? 1 : 0)) >>
                                 (stream->seq.ss_hor ? 1 : 0);
    arrays.above_cre_mark_n[1] = (bw8al + (stream->seq.ss_hor ? 1 : 0)) >>
                                 (stream->seq.ss_hor ? 1 : 0);
    arrays.left_cre_mark_n[0] = (bh8al + (stream->seq.ss_ver ? 1 : 0)) >>
                                 (stream->seq.ss_ver ? 1 : 0);
    arrays.left_cre_mark_n[1] = (bh8al + (stream->seq.ss_ver ? 1 : 0)) >>
                                 (stream->seq.ss_ver ? 1 : 0);
    arrays.above_cre[0] = above_cre0; arrays.above_cre_n[0] = cframe_w8;
    arrays.above_cre[1] = above_cre1; arrays.above_cre_n[1] = cframe_w8;
    arrays.left_cre[0] = left_cre0; arrays.left_cre_n[0] = cframe_h8;
    arrays.left_cre[1] = left_cre1; arrays.left_cre_n[1] = cframe_h8;
    arrays.above_skip = above_skip; arrays.above_skip_n = frame_w4;
    arrays.left_skip = left_skip; arrays.left_skip_n = frame_h4;
    arrays.above_pal_sz = above_pal_sz; arrays.above_pal_sz_n = frame_w4;
    arrays.left_pal_sz = left_pal_sz; arrays.left_pal_sz_n = frame_h4;
    arrays.above_pal_uv = above_pal_uv; arrays.above_pal_uv_n = frame_w4;
    arrays.left_pal_uv = left_pal_uv; arrays.left_pal_uv_n = frame_h4;
    arrays.above_pal[0] = above_pal0; arrays.above_pal[1] = above_pal1;
    arrays.left_pal[0] = left_pal0; arrays.left_pal[1] = left_pal1;
    arrays.above_pal_n = frame_w4; arrays.left_pal_n = frame_h4;
    arrays.above_seg_id = above_seg_id; arrays.above_seg_id_n = frame_w4;
    arrays.left_seg_id = left_seg_id; arrays.left_seg_id_n = frame_h4;
    arrays.above_ibc_mv_y = above_ibc_mv_y;
    arrays.above_ibc_mv_x = above_ibc_mv_x;
    arrays.above_ibc_valid = above_ibc_valid;
    arrays.above_ibc_mv_n = frame_w4;
    arrays.left_ibc_mv_y = left_ibc_mv_y;
    arrays.left_ibc_mv_x = left_ibc_mv_x;
    arrays.left_ibc_valid = left_ibc_valid;
    arrays.left_ibc_mv_n = frame_h4;
    arrays.refmvs_r = (stbv_u8*)refmvs_r;
    arrays.refmvs_stride = frame_w4;
    arrays.refmvs_h4 = frame_h4;
    arrays.refmvs_w4 = frame_w4;
    stbv_av1_leaf_state_init(&state, &arrays);
    state.cdef_idx_grid = NULL;
    state.cdef_grid_stride = 0;
    stb_av1_intra_state_set_uv(&state.intra, above_uvmode,
                               stream->seq.ss_hor ? ((frame_w4 + 1) >> 1)
                                                  : frame_w4,
                               left_uvmode,
                               stream->seq.ss_ver ? ((frame_h4 + 1) >> 1)
                                                  : frame_h4);

    /* Internal planes are stbv_u16 (bit-depth generic); converted back to
     * the 8-bit tc->planes after decoding. */
    py16 = (stbv_u16*)stb_avif_calloc(
        (size_t)tc->stride_y * (tc->frame_height + 64), sizeof(stbv_u16));
    if (!py16) { r = -5; goto oom16; }
    if (tc->plane_u && tc->plane_v) {
        int uvh2 = (stream->seq.ss_ver ? ((tc->frame_height + 1) >> 1)
                                      : tc->frame_height) + 32;
        pu16 = (stbv_u16*)stb_avif_calloc(
            (size_t)tc->stride_u * uvh2, sizeof(stbv_u16));
        pv16 = (stbv_u16*)stb_avif_calloc(
            (size_t)tc->stride_v * uvh2, sizeof(stbv_u16));
        if (!pu16 || !pv16) { r = -5; goto oom16; }
    }
    /* Deblocking maps: frame-sized 4x4-unit grid (SB-padded like the
     * residual context arrays). */
    {
        int mw = res_w4, mh = res_h4;
        lf_blkid_map = (stbv_u32*)stb_avif_calloc((size_t)mw * mh, sizeof(stbv_u32));
        lf_txlw_map = (stbv_u8*)stb_avif_calloc((size_t)mw * mh, 1);
        lf_blkid_map_c = (stbv_u32*)stb_avif_calloc((size_t)mw * mh, sizeof(stbv_u32));
        lf_txlw_map_c = (stbv_u8*)stb_avif_calloc((size_t)mw * mh, 1);
        lf_done_map = (stbv_u8*)stb_avif_calloc((size_t)mw * mh, 1);
        if (!lf_blkid_map || !lf_txlw_map ||
            !lf_blkid_map_c || !lf_txlw_map_c || !lf_done_map) { r = -5; goto oom16; }
        memset(lf_blkid_map_c, 0xFF, (size_t)mw * mh * sizeof(stbv_u32));
    }
    /* CDEF index grid: one entry per 64x64 block. */
    if (stream->seq.cdef) {
        cdef_grid_stride = (frame_w4 + 15) / 16;
        {
            int cdef_grid_rows = (frame_h4 + 15) / 16;
            cdef_idx_grid = (int*)stb_avif_calloc(
                (size_t)cdef_grid_stride * cdef_grid_rows, sizeof(int));
            if (!cdef_idx_grid) { r = -5; goto oom16; }
            /* Initialize to -1 (no CDEF / skip). */
            {
                int gi;
                int cdef_grid_total = cdef_grid_stride * cdef_grid_rows;
                for (gi = 0; gi < cdef_grid_total; gi++)
                    cdef_idx_grid[gi] = -1;
            }
        }
    }
    /* Wire the CDEF grid into the leaf state (after allocation). */
    state.cdef_idx_grid = cdef_idx_grid;
    state.cdef_grid_stride = cdef_grid_stride;

    /* Allocate LR mask for storing decoded restoration params. */
    if (stream->seq.restoration && !stream->frame.allow_intrabc) {
        int lr_usz[2];
        lr_usz[0] = stream->frame.restoration.unit_size[0];
        lr_usz[1] = stream->frame.restoration.unit_size[1];
        if (stbv_av1_lr_mask_alloc(&lr_mask, tc->frame_width, tc->frame_height,
                                   lr_usz, stream->seq.ss_hor ? 1 : 0,
                                   (stream->seq.layout == STB_AV1_LAYOUT_I420) ? 1 : 0) == 0) {
            lr_mask_ok = 1;
        }
    }
    memset(recon, 0, sizeof(*recon));
    recon->lf_blkid = lf_blkid_map;
    recon->lf_txlw = lf_txlw_map;
    recon->lf_blkid_c = lf_blkid_map_c;
    recon->lf_txlw_c = lf_txlw_map_c;
    recon->lf_done = lf_done_map;
    recon->lf_mapw4 = res_w4;
    recon->lf_maph4 = res_h4;
    recon->lf_b4stride = res_w4;
    recon->plane_y = py16;
    recon->plane_u = pu16;
    recon->plane_v = pv16;
    recon->stride_y = tc->stride_y;
    recon->stride_u = tc->stride_u;
    recon->stride_v = tc->stride_v;
    recon->bit_depth = 8 + stream->seq.hbd * 2;
    recon->ss_hor = stream->seq.ss_hor ? 1 : 0;
    recon->ss_ver = (stream->seq.layout == STB_AV1_LAYOUT_I420) ? 1 : 0;
    recon->frame_w = tc->frame_width;
    recon->frame_h = tc->frame_height;
    recon->tile_x4 = 0; recon->tile_y4 = 0;
    recon->tile_w4 = (int)((stream->frame.width[0] + 3U) >> 2);
    recon->tile_h4 = (int)((stream->frame.height + 3U) >> 2);
    recon->intra_edge_filter = stream->seq.intra_edge_filter ? 1 : 0;
    recon->sb_step4 = stream->seq.sb128 ? 32 : 16;
    recon->above_mode = above_mode;
    recon->left_mode = left_mode;
    recon->above_n = frame_w4;
    recon->left_n = frame_h4;
    recon->above_uvmode = above_uvmode;
    recon->left_uvmode = left_uvmode;
    g_scalar_recon = *recon;
    g_scalar_recon_cb.ud = &g_scalar_recon;
    g_scalar_recon_cb.cf = g_scalar_recon.cf;
    g_scalar_recon_cb.block_info = stb_avif_recon_block_info;
    g_scalar_recon_cb.luma_txb = stb_avif_recon_luma_txb;
    g_scalar_recon_cb.chroma_txb = stb_avif_recon_chroma_txb;
    g_scalar_recon_cb.luma_pal = stb_avif_recon_luma_pal;
    g_scalar_recon_cb.chroma_pal = stb_avif_recon_chroma_pal;

    memset(&td, 0, sizeof(td));
    td.seq = &stream->seq;
    td.frame = &stream->frame;
    r = 0;
    {
        unsigned int ti;
        unsigned int ntiles = stream->frame.tiling.cols * stream->frame.tiling.rows;
        unsigned int sb_log2 = 6U + stream->seq.sb128;
        unsigned int sb_size = 1U << sb_log2;
        for (ti = 0; ti < ntiles; ti++) {
            unsigned int tr, tcx;
            if (!stream->tile_seen[ti]) { r = -1; break; }
            tr = ti / stream->frame.tiling.cols;
            tcx = ti - tr * stream->frame.tiling.cols;
            recon->tile_x4 = (int)(stream->frame.tiling.col_start_sb[tcx] * (sb_size >> 2));
            recon->tile_y4 = (int)(stream->frame.tiling.row_start_sb[tr] * (sb_size >> 2));
            recon->tile_w4 = (int)((stream->frame.tiling.col_start_sb[tcx + 1] -
                                   stream->frame.tiling.col_start_sb[tcx]) * (sb_size >> 2));
            recon->tile_h4 = (int)((stream->frame.tiling.row_start_sb[tr + 1] -
                                   stream->frame.tiling.row_start_sb[tr]) * (sb_size >> 2));
            stbv_av1_leaf_state_init(&state, &arrays);
            state.last_qidx = (int)stream->frame.quant.yac;
            memset(state.last_delta_lf, 0, sizeof(state.last_delta_lf));
            /* leaf_state_init NULLs above/left_uvmode via intra_state_init;
             * re-establish them so chroma intra mode decode has proper
             * above/left contexts for each tile (tiles are independent). */
            stb_av1_intra_state_set_uv(&state.intra, above_uvmode,
                                       stream->seq.ss_hor ? ((frame_w4 + 1) >> 1)
                                                          : frame_w4,
                                       left_uvmode,
                                       stream->seq.ss_ver ? ((frame_h4 + 1) >> 1)
                                                          : frame_h4);
            /* Recon callbacks point at g_scalar_recon, so publish the
             * current tile's bounds before decoding its first leaf. */
            g_scalar_recon = *recon;
            td.lr_mask = lr_mask_ok ? &lr_mask : 0;
            r = stb_av1_decode_tile_at(&td, &stream->seq, &stream->frame,
                                       stream->tiles[ti].data, stream->tiles[ti].size,
                                       tcx, tr, stb_avif_leaf_cb, &state,
                                       stb_avif_row_reset_cb);
            if (r) { break; }
        }
    }

#ifdef STB_AVIF_DEBLOCK
    if (!r) {
        const struct stb_av1_framehdr *fh = &stream->frame;
        int lvl_yv = (int)fh->loopfilter.level_y[0];
        int lvl_yh = (int)fh->loopfilter.level_y[1];
        int lvl_u = (int)fh->loopfilter.level_u;
        int lvl_v = (int)fh->loopfilter.level_v;
        int sharp = (int)fh->loopfilter.sharpness;
        int maxv = (1 << recon->bit_depth) - 1;
        if (recon->ss_ver && !lvl_u) lvl_u = lvl_v;
        if (py16)
            stb_avif_deblock_plane_u16(py16, tc->stride_y,
                                       tc->frame_width, tc->frame_height,
                                       lvl_yv, lvl_yh, sharp, 0, maxv,
                                       recon->bit_depth - 8,
                                       lf_blkid_map, lf_txlw_map, res_w4,
                                       res_w4, res_h4, 0, 0,
                                       stream->frame.tiling.col_start_sb,
                                       (int)stream->frame.tiling.cols,
                                       stream->frame.tiling.row_start_sb,
                                       (int)stream->frame.tiling.rows,
                                       (int)(1U << (6U + stream->seq.sb128)));
        if (pu16 && !stream->seq.monochrome) {
            int cw = (tc->frame_width + (recon->ss_hor ? 1 : 0)) >> recon->ss_hor;
            int ch = (tc->frame_height + (recon->ss_ver ? 1 : 0)) >> recon->ss_ver;
            stb_avif_deblock_plane_u16(pu16, tc->stride_u, cw, ch,
                                       lvl_u, lvl_u, sharp, 1, maxv,
                                       recon->bit_depth - 8,
                                       lf_blkid_map_c, lf_txlw_map_c, res_w4,
                                       res_w4, res_h4,
                                       recon->ss_hor, recon->ss_ver,
                                       stream->frame.tiling.col_start_sb,
                                       (int)stream->frame.tiling.cols,
                                       stream->frame.tiling.row_start_sb,
                                       (int)stream->frame.tiling.rows,
                                       (int)(1U << (6U + stream->seq.sb128)));
            stb_avif_deblock_plane_u16(pv16, tc->stride_v, cw, ch,
                                       lvl_v ? lvl_v : lvl_u, lvl_v ? lvl_v : lvl_u,
                                       sharp, 1, maxv, recon->bit_depth - 8,
                                       lf_blkid_map_c, lf_txlw_map_c, res_w4,
                                       res_w4, res_h4,
                                       recon->ss_hor, recon->ss_ver,
                                       stream->frame.tiling.col_start_sb,
                                       (int)stream->frame.tiling.cols,
                                       stream->frame.tiling.row_start_sb,
                                       (int)stream->frame.tiling.rows,
                                       (int)(1U << (6U + stream->seq.sb128)));
        }
    }
#endif

    /* CDEF filtering (after deblocking, before loop restoration). */
    if (!r && stream->seq.cdef && cdef_idx_grid) {
        const struct stb_av1_framehdr *fh = &stream->frame;
        int y_pri_arr[8], y_sec_arr[8], uv_pri_arr[8], uv_sec_arr[8];
        int ci;
        for (ci = 0; ci < (1 << fh->cdef.n_bits); ci++) {
            y_pri_arr[ci] = (int)(fh->cdef.y_strength[ci] >> 2);
            y_sec_arr[ci] = (int)(fh->cdef.y_strength[ci] & 3);
            uv_pri_arr[ci] = (int)(fh->cdef.uv_strength[ci] >> 2);
            uv_sec_arr[ci] = (int)(fh->cdef.uv_strength[ci] & 3);
        }
        stb_av1_cdef_frame(py16, pu16, pv16,
                           tc->stride_y, tc->stride_u, tc->stride_v,
                           tc->frame_width, tc->frame_height,
                           recon->ss_hor, recon->ss_ver,
                           recon->bit_depth,
                           cdef_idx_grid, cdef_grid_stride,
                           y_pri_arr, y_sec_arr,
                           uv_pri_arr, uv_sec_arr,
                               (int)fh->cdef.damping);
    }

    /* Loop restoration filtering (after CDEF, before 8-bit conversion). */
    if (!r && lr_mask_ok && stream->seq.restoration && !stream->frame.allow_intrabc) {
        stb_av1_lr_frame(py16, pu16, pv16,
                         tc->stride_y, tc->stride_u, tc->stride_v,
                         tc->frame_width, tc->frame_height,
                         stream->seq.ss_hor ? 1 : 0,
                         (stream->seq.layout == STB_AV1_LAYOUT_I420) ? 1 : 0,
                         8 + stream->seq.hbd * 2,
                           &lr_mask);
    }

    /* Convert internal u16 planes to the caller's 8-bit planes.
     * Use truncating shift to match dav1d's u16->u8 conversion. */
    {
        const int bd = 8 + stream->seq.hbd * 2;
        const int sh = bd - 8;
        int w, h, hh, y0, x0;
        w = tc->frame_width;
        h = tc->frame_height;
        for (y0 = 0; y0 < h; y0++)
            for (x0 = 0; x0 < w; x0++) {
                unsigned v = (unsigned)py16[y0 * tc->stride_y + x0];
                v = v >> sh;
                tc->plane_y[y0 * tc->stride_y + x0] =
                    (stbv_u8)(v > 255u ? 255u : v);
            }
        if (pu16 && pv16 && tc->plane_u && tc->plane_v) {
            int ss_h = stream->seq.ss_hor ? 1 : 0;
            int ss_v = stream->seq.ss_ver ? 1 : 0;
            w = (tc->frame_width + ss_h) >> ss_h;
            h = (tc->frame_height + ss_v) >> ss_v;
            for (hh = 0; hh < h; hh++)
                for (x0 = 0; x0 < w; x0++) {
                    unsigned vu = (unsigned)pu16[hh * tc->stride_u + x0];
                    unsigned vv = (unsigned)pv16[hh * tc->stride_v + x0];
                    vu = vu >> sh;
                    vv = vv >> sh;
                    tc->plane_u[hh * tc->stride_u + x0] =
                        (stbv_u8)(vu > 255u ? 255u : vu);
                    tc->plane_v[hh * tc->stride_v + x0] =
                        (stbv_u8)(vv > 255u ? 255u : vv);
                }
        }
    }
oom16:
    if (py16) stb_avif_free_internal(py16);
    if (pu16) stb_avif_free_internal(pu16);
    if (pv16) stb_avif_free_internal(pv16);
    stb_avif_free_internal(above_mode); stb_avif_free_internal(left_mode);
    stb_avif_free_internal(above_tx); stb_avif_free_internal(left_tx);
    stb_avif_free_internal(above_tx_intra); stb_avif_free_internal(left_tx_intra);
    stb_avif_free_internal(above_res); stb_avif_free_internal(left_res);
    stb_avif_free_internal(above_cre0); stb_avif_free_internal(above_cre1);
    stb_avif_free_internal(left_cre0); stb_avif_free_internal(left_cre1);
    stb_avif_free_internal(above_skip); stb_avif_free_internal(left_skip);
    stb_avif_free_internal(above_seg_id); stb_avif_free_internal(left_seg_id);
    stb_avif_free_internal(above_ibc_mv_y); stb_avif_free_internal(above_ibc_mv_x);
    stb_avif_free_internal(above_ibc_valid);
    stb_avif_free_internal(left_ibc_mv_y); stb_avif_free_internal(left_ibc_mv_x);
    stb_avif_free_internal(left_ibc_valid);
    stb_avif_free_internal(refmvs_r);
    stb_avif_free_internal(above_pal_sz); stb_avif_free_internal(left_pal_sz);
    stb_avif_free_internal(above_pal_uv);
    stb_avif_free_internal(above_uvmode);
    stb_avif_free_internal(left_uvmode); stb_avif_free_internal(left_pal_uv);
    stb_avif_free_internal(above_pal0); stb_avif_free_internal(above_pal1);
    stb_avif_free_internal(left_pal0); stb_avif_free_internal(left_pal1);
    stb_avif_free_internal(cdef_idx_grid);
    stbv_av1_lr_mask_free(&lr_mask);
    stb_avif_free_internal(lf_blkid_map); stb_avif_free_internal(lf_blkid_map_c);
    stb_avif_free_internal(lf_txlw_map); stb_avif_free_internal(lf_txlw_map_c);
    stb_avif_free_internal(lf_done_map);
    stb_avif_free(stream);
    stb_avif_free(recon);
    return r;
}

#endif /* !STB_AVIF_USE_DAV1D */

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

unsigned char *stb_avif_load_from_file(const char *filePath,
                                       int *w, int *h, int *channels,
                                       int req_channels)
{
    FILE *f;
    unsigned char *buf;
    long file_size;
    unsigned char *result;

    if (!filePath) return NULL;
    f = fopen(filePath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) { fclose(f); return NULL; }

    buf = (unsigned char *)malloc((size_t)file_size);
    if (!buf) { fclose(f); return NULL; }

    if ((long)fread(buf, 1, (size_t)file_size, f) != file_size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    result = stb_avif_load_from_memory(buf, (int)file_size, w, h, channels,
                                       req_channels);
    free(buf);
    return result;
}

unsigned char *stb_avif_load_from_memory(const unsigned char *data, int len,
                                          int *x, int *y, int *channels,
                                          int req_channels)
{
    struct stb_av1_getbits gb;
    
    struct stb_avif_avif_info info;
    struct stb_av1_sequence_header sh;
    struct stb_av1_frame_header fh;
    struct stb_av1_tile_context tc;
    struct stb_av1_getbits obu_gb;
    struct stb_av1_bool_reader br;
    unsigned char *result = NULL;
    int output_channels;

    /* Free YUV planes from previous load. */
    if (stb_avif_g_last_yuv_y) { stb_avif_free_internal(stb_avif_g_last_yuv_y); stb_avif_g_last_yuv_y = NULL; }
    if (stb_avif_g_last_yuv_u) { stb_avif_free_internal(stb_avif_g_last_yuv_u); stb_avif_g_last_yuv_u = NULL; }
    if (stb_avif_g_last_yuv_v) { stb_avif_free_internal(stb_avif_g_last_yuv_v); stb_avif_g_last_yuv_v = NULL; }

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

    /* Validate input */
    STB_AVIF_CHECK(data != NULL && len >= 16, "Invalid input data");

    /* Detect IVF container (starts with "DKIF") */
    if (data[0] == 'D' && data[1] == 'K' && data[2] == 'I' && data[3] == 'F') {
        unsigned hdr_len;
        int ivf_w, ivf_h;
        unsigned pos;
        size_t total_av1 = 0;
        if (len < 32) goto error_exit;
        hdr_len = (unsigned)data[6] | ((unsigned)data[7] << 8);
        ivf_w = (int)((unsigned)data[12] | ((unsigned)data[13] << 8));
        ivf_h = (int)((unsigned)data[14] | ((unsigned)data[15] << 8));
        if ((int)hdr_len > len || ivf_w <= 0 || ivf_h <= 0) goto error_exit;
        info.width = ivf_w;
        info.height = ivf_h;
        /* Concatenate ALL frame payloads into a single AV1 bitstream */
        pos = hdr_len;
        while ((int)(pos + 12) <= len) {
            unsigned fsz = (unsigned)data[pos] | ((unsigned)data[pos+1] << 8) |
                           ((unsigned)data[pos+2] << 16) | ((unsigned)data[pos+3] << 24);
            if (fsz == 0 || (int)(pos + 12 + fsz) > len) break;
            total_av1 += fsz;
            pos += 12 + fsz;
        }
        if (total_av1 == 0) goto error_exit;
        {
            unsigned char *av1_buf = (unsigned char *)stb_avif_calloc(1, total_av1);
            unsigned char *dst;
            if (!av1_buf) goto error_exit;
            dst = av1_buf;
            pos = hdr_len;
            while ((int)(pos + 12) <= len) {
                unsigned fsz = (unsigned)data[pos] | ((unsigned)data[pos+1] << 8) |
                               ((unsigned)data[pos+2] << 16) | ((unsigned)data[pos+3] << 24);
                if (fsz == 0 || (int)(pos + 12 + fsz) > len) break;
                memcpy(dst, data + pos + 12, fsz);
                dst += fsz;
                pos += 12 + fsz;
            }
            info.av1_data = av1_buf;
            info.av1_size = total_av1;
            info.ivf_concat_buf = av1_buf;
        }
        /* Validate: first byte must look like a valid OBU header */
        if (total_av1 > 0) {
            unsigned char first = info.av1_data[0];
            int obu_type = (first >> 3) & 0xF;
            if ((first & 0x80) != 0 || obu_type == 0 || (first & 1) != 0) {
                stb_avif_error_msg = "Invalid IVF: first frame does not contain valid AV1 OBU data";
                goto error_exit;
            }
        }
        info.input = data;
        info.input_len = len;
        goto ivf_decoded;
    }

    /* Setup error handling */
    if (setjmp(stb_avif_jmp)) {
        goto error_exit;
    }

    stb_av1_getbits_init(&gb, data, (size_t)len);

    /* Look for ftyp box */
    STB_AVIF_CHECK(stb_avif_find_box(&gb, STB_AVIF_BOX_FTYP, 0, NULL),
                   "No ftyp box found");
    stb_avif_parse_ftyp(&gb, &info);

    /* Look for meta box */
    {
        struct stb_avif_box meta_hdr;
        stb_av1_getbits_init(&gb, data, (size_t)len);
        STB_AVIF_CHECK(stb_avif_find_box(&gb, STB_AVIF_BOX_META, 1, &meta_hdr),
                       "No meta box found");
        /* Save meta end position for parse_meta */
        info.meta_end_offset = (size_t)(meta_hdr.data_start + meta_hdr.data_size);
    }

    /* Parse the meta box to extract all AVIF metadata */
    stb_avif_parse_meta(&gb, &info);

    /* Verify we have image dimensions */
    STB_AVIF_CHECK(info.width > 0 && info.height > 0,
                   "Could not determine image dimensions");
    STB_AVIF_CHECK(info.width <= STB_AVIF_MAX_DIMENSION &&
                   info.height <= STB_AVIF_MAX_DIMENSION,
                   "Image too large");

    /* Verify we have compressed data */
    STB_AVIF_CHECK(info.av1_data != NULL && info.av1_size > 0,
                   "No AV1 compressed data found");

ivf_decoded:
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
    stb_av1_getbits_init(&obu_gb, info.av1_data, info.av1_size);

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

        while (more_obus && stb_av1_getbits_bytepos(&obu_gb) < stb_av1_getbits_size(&obu_gb)) {
            /* Parse OBU header */
            if (stb_av1_getbits_bytepos(&obu_gb) + 1 > stb_av1_getbits_size(&obu_gb))
                break;

            stb_av1_read_obu_header(&obu_gb, &obu_type,
                                     &obu_extension_flag, &obu_has_size_field);

            /* Read OBU size */
            obu_size = 0;
            if (obu_has_size_field) {
                obu_size = stb_av1_read_obu_size(&obu_gb);
            }

            /* Process based on type */
            switch (obu_type) {
                case STB_AV1_OBU_SEQUENCE_HEADER: {
#ifndef STB_AVIF_USE_DAV1D
                    /* Parse the sequence header with the authoritative
                     * raw-bit parser so sh is populated BEFORE the frame
                     * header parser runs (it reads sh->frame_width_bits,
                     * sh->enable_cdef, etc.). */
                    struct stb_av1_seqhdr sq;
                    struct stb_av1_getbits sq_gb;
                    const stbv_u8 *sq_data = obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb);
                    size_t sq_sz = (size_t)obu_size;
                    memset(&sq, 0, sizeof(sq));
                    stb_av1_getbits_init(&sq_gb, sq_data, sq_sz);
                    if (stb_av1_parse_seqhdr(&sq, &sq_gb) == 0) {
                        sh.frame_width_bits  = (int)sq.width_n_bits;
                        sh.frame_height_bits = (int)sq.height_n_bits;
                        sh.max_frame_width   = (int)sq.max_width;
                        sh.max_frame_height  = (int)sq.max_height;
                        sh.seq_profile                = (int)sq.profile;
                        sh.still_picture              = (int)sq.still_picture;
                        sh.reduced_still_picture_header = (int)sq.reduced_still_picture_header;
                        sh.enable_order_hint        = (int)sq.order_hint;
                        sh.enable_dist_wtd_comp     = 0;
                        sh.enable_masked_comp       = (int)sq.masked_compound;
                        sh.enable_intra_edge_filter = (int)sq.intra_edge_filter;
                        sh.enable_interintra_comp   = (int)sq.inter_intra;
                        sh.enable_dual_filter       = (int)sq.dual_filter;
                        sh.enable_jnt_comp          = (int)sq.jnt_comp;
                        sh.enable_superres          = (int)sq.super_res;
                        sh.enable_cdef              = (int)sq.cdef;
                        sh.enable_restoration       = (int)sq.restoration;
                        sh.film_grain_params_present = (int)sq.film_grain_present;
                        sh.timing_info_present         = (int)sq.timing_info_present;
                        sh.decoder_model_info_present  = (int)sq.decoder_model_info_present;
                        sh.display_model_info_present  = (int)sq.display_model_info_present;
                        sh.operating_points_cnt        = (int)sq.num_operating_points;
                        sh.color_description_present = (int)sq.color_description_present;
                        sh.color_primaries           = (int)sq.pri;
                        sh.transfer_characteristics  = (int)sq.trc;
                        sh.matrix_coefficients       = (int)sq.mtrx;
                        sh.color_range               = (int)sq.color_range;
                        sh.chroma_sample_position     = (int)sq.chr;
                        sh.monochrome    = sq.monochrome ? 1 : 0;
                        sh.bit_depth     = 8 + (int)sq.hbd * 2;
                        sh.subsampling_x = sq.ss_hor ? 1 : 0;
                        sh.subsampling_y = sq.ss_ver ? 1 : 0;
                        probe_seq_hbd  = sq.hbd;
                        probe_seq_mono = sq.monochrome ? 1 : 0;
                    }
#endif
                    seq_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME_HEADER:
                case STB_AV1_OBU_REDUNDANT_FRAME_HEADER: {
                    struct stb_av1_bool_reader fh_br;

                    stb_av1_bool_reader_init(&fh_br,
                                               obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb),
                                               (size_t)obu_size);

                    stb_av1_parse_frame_header(&fh, &sh, &fh_br);
                    frame_header_found = 1;
                    break;
                }
                case STB_AV1_OBU_FRAME: {
                    /* Combined frame header + tile group OBU */
                    /* For simplicity, we handle frame + tile group separately */
                    struct stb_av1_bool_reader frame_br;

                    stb_av1_bool_reader_init(&frame_br,
                                               obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb),
                                               (size_t)obu_size);

                    /* Frame OBU contains frame header followed by tile group data.
                       Parse frame header first. */
                    stb_av1_parse_frame_header(&fh, &sh, &frame_br);
                    frame_header_found = 1;

                    /* Remaining data in the OBU is tile group data.
                       We'll read it right here using the same reader. */
                    if (fh.show_existing_frame) {
                        /* nothing to decode */
                    } else {
                        /* The position in frame_br is now at the tile data.
                           Use it for tile decoding. */
                        /* Store the boolean reader position for tile decoding */
                        br = frame_br;
                    }
                    break;
                }
                case STB_AV1_OBU_TILE_GROUP: {
                    /* We already parsed frame header; this is tile data.
                       Transfer the boolean reader from current position. */
                    /* The tile group data starts at obu_gb position */
                    if (frame_header_found) {
                        br.data = obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb);
                        br.size = (size_t)obu_size;
                        br.pos = 0;
                        br.value = 0;
                        br.range = 128;
                        br.count = 0;
                        br.error = 0;
                        /* Re-init properly */
                        stb_av1_bool_reader_init(&br,
                                                   obu_gb.ptr_start + stb_av1_getbits_bytepos(&obu_gb),
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
                stb_av1_getbits_seek(&obu_gb, stb_av1_getbits_bytepos(&obu_gb) + (size_t)obu_size);
            } else if (obu_has_size_field) {
                /* OBU with has_size_field=1 and size=0 is valid (e.g. temporal delimiter).
                   Just skip the header+size bytes we already consumed. */
                /* Already advanced past header+size, nothing more to skip. */
            } else {
                /* No size field: determine from remaining data or break on unknown */
                if (stb_av1_getbits_bytepos(&obu_gb) < stb_av1_getbits_size(&obu_gb))
                    stb_av1_getbits_seek(&obu_gb, stb_av1_getbits_size(&obu_gb)); /* consume all remaining */
                else
                    break;
            }

            /* Check if we've found end of OBUs */
            if (stb_av1_getbits_bytepos(&obu_gb) >= stb_av1_getbits_size(&obu_gb))
                more_obus = 0;
        }

        /* Fallback: no sequence-header OBU in the item stream (AVIF
         * encoders often put it only inside av1C).  Parse the config
         * OBU from av1C now - AFTER the stream walk, so the stream's
         * own seq header always wins. */
        if (!seq_header_found && info.av1c_size > 0) {
            struct stb_av1_getbits config_gb;
            int config_obu_type, config_obu_ext, config_obu_hassize;
            stbv_u32 config_obu_sz;

            stb_av1_getbits_init(&config_gb, info.av1c_data, (size_t)info.av1c_size);
            stb_av1_read_obu_header(&config_gb, &config_obu_type,
                                     &config_obu_ext, &config_obu_hassize);
            if (config_obu_hassize)
                config_obu_sz = stb_av1_read_obu_size(&config_gb);
            else
                config_obu_sz = (stbv_u32)(info.av1c_size - stb_av1_getbits_bytepos(&config_gb));

            if (config_obu_type == STB_AV1_OBU_SEQUENCE_HEADER && config_obu_sz > 0) {
                seq_header_found = 1;
            }
        }

        /* For reduced still_picture_header, restore dimensions from ISPE */
        if (sh.reduced_still_picture_header && info.width > 0 && info.height > 0) {
            sh.max_frame_width = info.width;
            sh.max_frame_height = info.height;
        }
        STB_AVIF_CHECK(seq_header_found, "No AV1 sequence header found");
        sh_parsed_ok = 1;
    }

#ifndef STB_AVIF_USE_DAV1D
    /* Authoritative sequence-header parse: stb_av1_parse_internal_stream
     * uses raw-bit f(n) parsing that matches dav1d bit-for-bit.  It owns
     * ALL geometry/colour/filtering fields so we no longer need the
     * legacy bool-reader based parser above. */
    {
        struct stb_av1_internal_stream probe;
        if (stb_av1_parse_internal_stream(&probe, info.av1_data,
                                          info.av1_size) == 0 &&
            probe.have_seq) {
            const struct stb_av1_seqhdr *sq = &probe.seq;

            /* Geometry */
            sh.frame_width_bits  = (int)sq->width_n_bits;
            sh.frame_height_bits = (int)sq->height_n_bits;
            sh.max_frame_width   = (int)sq->max_width;
            sh.max_frame_height  = (int)sq->max_height;

            /* Profile / picture type */
            sh.seq_profile                = (int)sq->profile;
            sh.still_picture              = (int)sq->still_picture;
            sh.reduced_still_picture_header = (int)sq->reduced_still_picture_header;

            /* Tool flags (consumed by frame header parser) */
            sh.enable_order_hint        = (int)sq->order_hint;
            sh.enable_dist_wtd_comp     = 0;
            sh.enable_masked_comp       = (int)sq->masked_compound;
            sh.enable_intra_edge_filter = (int)sq->intra_edge_filter;
            sh.enable_interintra_comp   = (int)sq->inter_intra;
            sh.enable_dual_filter       = (int)sq->dual_filter;
            sh.enable_jnt_comp          = (int)sq->jnt_comp;
            sh.enable_superres          = (int)sq->super_res;
            sh.enable_cdef              = (int)sq->cdef;
            sh.enable_restoration       = (int)sq->restoration;
            sh.film_grain_params_present = (int)sq->film_grain_present;

            /* Timing / decoder model */
            sh.timing_info_present         = (int)sq->timing_info_present;
            sh.decoder_model_info_present  = (int)sq->decoder_model_info_present;
            sh.display_model_info_present  = (int)sq->display_model_info_present;
            sh.operating_points_cnt        = (int)sq->num_operating_points;

            /* Colour */
            sh.color_description_present = (int)sq->color_description_present;
            sh.color_primaries           = (int)sq->pri;
            sh.transfer_characteristics  = (int)sq->trc;
            sh.matrix_coefficients       = (int)sq->mtrx;
            sh.color_range               = (int)sq->color_range;
            sh.chroma_sample_position     = (int)sq->chr;

            /* Pixel format */
            sh.monochrome    = sq->monochrome ? 1 : 0;
            sh.bit_depth     = 8 + (int)sq->hbd * 2;
            sh.subsampling_x = sq->ss_hor ? 1 : 0;
            sh.subsampling_y = sq->ss_ver ? 1 : 0;

            probe_seq_hbd  = sq->hbd;
            probe_seq_mono = sq->monochrome ? 1 : 0;
            sh_parsed_ok   = 1;
        }
    }
#endif /* !STB_AVIF_USE_DAV1D */

    /* After sequence header parse, override info.width/height with the
     * real bitstream dimensions.  IVF headers or ispe boxes can be wrong
     * (e.g. thumbnail dimensions); the sequence header is authoritative. */
    if (sh.max_frame_width > 0 && sh.max_frame_height > 0) {
        info.width  = sh.max_frame_width;
        info.height = sh.max_frame_height;
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

    /* Geometry comes from the REAL OBU sequence header parsed above
     * (bit-exact vs dav1d on the whole sample set).  av1C is only a
     * fallback: some encoders write wrong subsampling there (e.g. 444
     * in av1C for a profile-2 stream whose seq header forces ss_x=1),
     * which used to corrupt plane allocation and YUV->RGB sampling.
     * Only fill fields when the OBU parse left them unset. */
    if (!sh_parsed_ok) {
        sh.monochrome = info.monochrome;
        sh.bit_depth = info.bit_depth;
        sh.subsampling_x = info.chroma_subsampling_x;
        sh.subsampling_y = info.chroma_subsampling_y;
    }

    /* Allocate image planes */
    info.stride_y = (info.width + 31) & ~31;
    info.stride_u = ((info.width >> sh.subsampling_x) + 31) & ~31;
    info.stride_v = info.stride_u;

    /* +64 rows of padding so intra edge gathering on partial bottom-edge
     * blocks (frame height not a multiple of 4/8) stays in bounds. */
    info.plane_y = (unsigned char *)stb_avif_calloc(
        (size_t)(info.stride_y * (info.height + 64)), 1);
    if (sh.monochrome) {
        info.plane_u = NULL;
        info.plane_v = NULL;
    } else {
        int uv_rows = (info.height + (1 << sh.subsampling_y) - 1) >> sh.subsampling_y;
        info.plane_u = (unsigned char *)stb_avif_calloc(
            (size_t)(info.stride_u * (uv_rows + 32)), 1);
        info.plane_v = (unsigned char *)stb_avif_calloc(
            (size_t)(info.stride_v * (uv_rows + 32)), 1);
    }

    /* Initialize tile context */
    tc.sh = &sh;
    tc.fh = &fh;
    tc.frame_width = fh.frame_width;
    tc.frame_height = fh.frame_height;
    tc.mb_cols = (tc.frame_width + 3) / 4;
    tc.mb_rows = (tc.frame_height + 3) / 4;
    tc.br = &br;
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
        int dav1d_cr, dav1d_mc;
        unsigned char *dav1d_y = NULL, *dav1d_u = NULL, *dav1d_v = NULL;
        int dav1d_ys, dav1d_us, dav1d_vs;
        int dav1d_ok;

        dav1d_ok = stb_avif_decode_with_dav1d(
            info.av1_data, info.av1_size,
            &dav1d_w, &dav1d_h,
            &dav1d_y, &dav1d_ys,
            &dav1d_u, &dav1d_us,
            &dav1d_v, &dav1d_vs,
            &dav1d_bd, &dav1d_mono, &dav1d_sx, &dav1d_sy,
            &dav1d_cr, &dav1d_mc);

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
            sh.color_range = dav1d_cr;
            sh.matrix_coefficients = dav1d_mc;
        } else {
            stb_avif_error_msg = "dav1d decode failed";
            goto error_exit;
        }
    }
#else
    {
        int r = stb_avif_decode_frame_scalar(&tc, info.av1_data, info.av1_size);
        if (r < 0) {
            stb_avif_error_msg = "scalar AV1 decode failed";
            goto error_exit;
        }
    }
#endif

    /* ---- auxiliary alpha item (AVIF auxl -> primary) ---- */
    if (info.alpha_item_id > 0 && !info.alpha_plane) {
        /* Locate the alpha item's coded data with a fresh iloc scan. */
        struct stb_av1_getbits ar_gb;
        size_t ameta_start = 0;
        int found_iloc_box = 0;
        {
            /* re-find the meta box payload start */
            size_t scan = 0;
            while (scan + 8 <= (size_t)len) {
                stbv_u32 bsz = ((stbv_u32)data[scan]<<24)|((stbv_u32)data[scan+1]<<16)|
                               ((stbv_u32)data[scan+2]<<8)|data[scan+3];
                stbv_u32 bty = ((stbv_u32)data[scan+4]<<24)|((stbv_u32)data[scan+5]<<16)|
                               ((stbv_u32)data[scan+6]<<8)|data[scan+7];
                if (bsz < 8) break;
                if (bty == STB_AVIF_FOURCC('m','e','t','a')) { ameta_start = scan + 8 + 4; break; }
                scan += bsz;
            }
        }
        if (ameta_start) {
            size_t ap = ameta_start;
            size_t aend = info.meta_end_offset;
            ap = ameta_start;
            while (ap + 8 <= aend) {
                stbv_u32 bsz = ((stbv_u32)data[ap]<<24)|((stbv_u32)data[ap+1]<<16)|
                               ((stbv_u32)data[ap+2]<<8)|data[ap+3];
                stbv_u32 bty = ((stbv_u32)data[ap+4]<<24)|((stbv_u32)data[ap+5]<<16)|
                               ((stbv_u32)data[ap+6]<<8)|data[ap+7];
                if (bsz < 8 || ap + bsz > aend + 4096) break;
                if (bty == STB_AVIF_FOURCC('i','l','o','c')) {
                    stbv_u32 aoff = 0; stbv_u64 asz = 0;
                                stb_av1_getbits_init(&ar_gb, data + ap + 8, bsz - 8);
                    stb_avif_parse_iloc(&ar_gb, &info, info.alpha_item_id,
                                        &aoff, &asz);
                    if (asz > 0 && aoff + asz <= (stbv_u64)len) {
                        info.alpha_av1 = info.input + aoff;
                        info.alpha_size = (size_t)asz;
                    }
                    found_iloc_box = 1;
                    break;
                }
                ap += bsz;
            }
        }
        (void)found_iloc_box;
        if (info.alpha_av1 && info.alpha_size > 0 &&
            !sh.monochrome) {
#ifndef STB_AVIF_USE_DAV1D
            /* Decode the mono alpha stream with the scalar path. */
            struct stb_av1_tile_context tc2;
            struct stb_av1_internal_stream astream;
            memset(&astream, 0, sizeof(astream));
            if (stb_av1_parse_internal_stream(&astream, info.alpha_av1,
                                              info.alpha_size) == 0 &&
                astream.have_seq) {
                /* C89: declarations first */
                int abd = 8 + (int)astream.seq.hbd * 2;
                int aw = fh.frame_width, ah = fh.frame_height;
                int astride = (aw + 31) & ~31;
                unsigned char *aplane;
                if (astream.seq.monochrome)
                    aplane = (unsigned char *)stb_avif_calloc(
                        (size_t)astride * (ah + 64), 1);
                else
                    aplane = NULL;
                if (aplane) {
                        memset(&tc2, 0, sizeof(tc2));
                        tc2.sh = &sh; tc2.fh = &fh;
                        tc2.frame_width = aw; tc2.frame_height = ah;
                        tc2.mb_cols = (aw + 3) / 4; tc2.mb_rows = (ah + 3) / 4;
                        tc2.qindex_y = fh.base_q_idx; tc2.qindex_u = fh.base_q_idx;
                        tc2.qindex_v = fh.base_q_idx;
                        tc2.plane_y = aplane;
                        tc2.stride_y = astride;
                        tc2.bit_depth = abd;
                        tc2.pixel_max = (1 << abd) - 1;
                        sh.bit_depth = abd;
                        sh.monochrome = 1;
                        if (stb_avif_decode_frame_scalar(&tc2,
                                info.alpha_av1, info.alpha_size) == 0) {
                            int yy2;
                            info.alpha_plane = (unsigned char *)stb_avif_malloc(
                                (size_t)aw * ah);
                            if (info.alpha_plane)
                                for (yy2 = 0; yy2 < ah; yy2++)
                                    memcpy(info.alpha_plane + (size_t)yy2 * aw,
                                           aplane + (size_t)yy2 * astride, aw);
                            info.alpha_stride = aw;
                        }
                        stb_avif_free_internal(aplane);
                        /* restore colour description clobbered above */
                        sh.bit_depth = 8 + (int)probe_seq_hbd * 2;
                        sh.monochrome = probe_seq_mono;
                    }
            }
#endif /* !STB_AVIF_USE_DAV1D */
        }
    }

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

                if (sh.monochrome || !info.plane_u || !info.plane_v) {
                    /* neutral chroma for monochrome -> grey = luma */
                    u_val = 128;
                    v_val = 128;
                } else if (sh.subsampling_x > 0 || sh.subsampling_y > 0) {
                    /* any subsampling (4:2:0 AND 4:2:2) needs scaled
                     * chroma coords; the old y-only test made 4:2:2
                     * sample chroma at full rate -> half-width
                     * stretched colour bands. */
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

                /* NOTE: planes are already scaled to 8-bit in the
                 * u16 -> u8 conversion above (lines ~3662-3745).
                 * Do NOT apply a second bit-depth shift here. */

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
                        /* AV1/H.273 matrix 0 is GBR: Y=G, Cb=B, Cr=R.
                         * u_val/v_val were centered above, so restore the
                         * unsigned plane values here. */
                        r = v_val + 128;
                        g = y_val;
                        b = u_val + 128;
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
                    if (info.alpha_plane)
                        result[(row * info.width + col) * 4 + 3] =
                            info.alpha_plane[(size_t)row * info.alpha_stride + col];
                    else
                        result[(row * info.width + col) * 4 + 3] = 255;
                }
            }
        }
    }

    stb_avif_g_last_alpha = info.alpha_plane;
    stb_avif_g_last_alpha_stride = info.alpha_plane ? info.alpha_stride : 0;

    /* Keep YUV planes alive for external access via stb_avif_last_yuv(). */
    stb_avif_g_last_yuv_y = info.plane_y;
    stb_avif_g_last_yuv_u = info.plane_u;
    stb_avif_g_last_yuv_v = info.plane_v;
    stb_avif_g_last_yuv_stride_y = info.stride_y;
    stb_avif_g_last_yuv_stride_u = info.stride_u;
    stb_avif_g_last_yuv_stride_v = info.stride_v;
    info.plane_y = NULL;  /* prevent free — ownership transferred to global */
    info.plane_u = NULL;
    info.plane_v = NULL;

    /* Set output parameters */
    *x = info.width;
    *y = info.height;
    *channels = output_channels;

    /* Cleanup */
    if (info.ivf_concat_buf) stb_avif_free_internal(info.ivf_concat_buf);
    /* plane_y/u/v ownership transferred to global above — do not free here */

    stb_avif_error_msg = "no error";
    return result;

error_exit:
    if (info.ivf_concat_buf) stb_avif_free_internal(info.ivf_concat_buf);
    if (info.plane_y) stb_avif_free_internal(info.plane_y);
    if (info.plane_u) stb_avif_free_internal(info.plane_u);
    if (info.plane_v) stb_avif_free_internal(info.plane_v);
    if (result) stb_avif_free_internal(result);
    return NULL;
}

#endif /* STB_AVIF_IMPLEMENTATION */