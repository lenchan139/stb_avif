#ifndef STB_AVIF_H
#define STB_AVIF_H

/*
   stb_avif.h - dependency-free AVIF container parser and decoder scaffold

   Current status:
   - Pure C89, libc only
   - stb-style single-header API
   - Parses core AVIF container metadata for still images
   - Reports width/height for constrained files when ispe is present
   - AV1 bitstream decoding is not implemented yet

   Usage:

     #define STB_AVIF_IMPLEMENTATION
     #include "stb_avif.h"

   Public domain / unlicense style dedication is intentionally omitted here.
*/

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *stbi_avif_load_from_memory(const unsigned char *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
unsigned char *stbi_avif_load(const char *filename, int *x, int *y, int *channels_in_file, int desired_channels);
int stbi_avif_info_from_memory(const unsigned char *buffer, int len, int *x, int *y, int *channels_in_file);
int stbi_avif_info(const char *filename, int *x, int *y, int *channels_in_file);
void stbi_avif_image_free(void *retval_from_stbi_avif_load);
const char *stbi_avif_failure_reason(void);

#ifdef __cplusplus
}
#endif

#endif

#ifdef STB_AVIF_IMPLEMENTATION

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef STBI_AVIF_MALLOC
#define STBI_AVIF_MALLOC(sz) malloc(sz)
#endif

#ifndef STBI_AVIF_REALLOC
#define STBI_AVIF_REALLOC(p,sz) realloc((p),(sz))
#endif

#ifndef STBI_AVIF_FREE
#define STBI_AVIF_FREE(p) free(p)
#endif

#define STBI_AVIF_CHANNELS 4
#define STBI_AVIF_MAX_ASSOCIATIONS 32

#define STBI_AVIF_FOURCC(a,b,c,d) \
   ((((unsigned long)(a)) << 24) | (((unsigned long)(b)) << 16) | (((unsigned long)(c)) << 8) | ((unsigned long)(d)))

typedef struct
{
   const unsigned char *data;
   size_t size;
} stbi_avif__buffer;

typedef struct
{
   size_t offset;
   size_t size;
   size_t header_size;
   unsigned long type;
} stbi_avif__box;

typedef struct
{
   unsigned long type;
   unsigned int width;
   unsigned int height;
   size_t data_offset;
   size_t data_size;
} stbi_avif__property;

typedef struct
{
   unsigned int item_id;
   unsigned long item_type;
} stbi_avif__item_info;

typedef struct
{
   unsigned int property_index;
   int essential;
} stbi_avif__association;

typedef struct
{
   unsigned int item_id;
   stbi_avif__association *entries;
   int count;
   int capacity;
} stbi_avif__item_assoc;

typedef struct
{
   unsigned int item_id;
   unsigned int construction_method;
   size_t base_offset;
   size_t extent_offset;
   size_t extent_length;
   int extent_count;
} stbi_avif__item_location;

typedef struct
{
   int saw_ftyp;
   int saw_meta;
   int has_avif_brand;
   unsigned int primary_item_id;
   unsigned int width;
   unsigned int height;
   int has_av1_config;
   size_t av1c_offset;
   size_t av1c_size;
   size_t payload_offset;
   size_t payload_size;
   stbi_avif__property *properties;
   int property_count;
   int property_capacity;
   stbi_avif__item_info *items;
   int item_count;
   int item_capacity;
   stbi_avif__item_assoc *assocs;
   int assoc_count;
   int assoc_capacity;
   stbi_avif__item_location *locations;
   int location_count;
   int location_capacity;
} stbi_avif__parser;

typedef struct
{
   const unsigned char *data;
   size_t size;
   size_t bit_offset;
} stbi_avif__bit_reader;

typedef struct
{
   unsigned int seq_profile;
   unsigned int max_frame_width;
   unsigned int max_frame_height;
   unsigned int bit_depth;
   unsigned int chroma_sample_position;
   unsigned int color_primaries;
   unsigned int transfer_characteristics;
   unsigned int matrix_coefficients;
   int still_picture;
   int reduced_still_picture_header;
   int monochrome;
   int subsampling_x;
   int subsampling_y;
   int color_range;
   int separate_uv_delta_q;
   int film_grain_params_present;
   int use_128x128_superblock;
   int enable_filter_intra;
   int enable_superres;
   int enable_cdef;
   int enable_restoration;
} stbi_avif__av1_sequence_header;

typedef struct
{
   int saw_sequence_header;
   int saw_frame_header;
   stbi_avif__av1_sequence_header sequence_header;
} stbi_avif__av1_headers;

typedef struct
{
   int saw_frame_header;
   int saw_tile_group;
   int frame_is_combined_obu;
   size_t frame_header_offset;
   size_t frame_header_size;
   size_t tile_group_offset;
   size_t tile_group_size;
} stbi_avif__av1_frame_index;

typedef struct
{
   unsigned int frame_type;
   int show_frame;
   unsigned int frame_width;
   unsigned int frame_height;
   unsigned int base_q_idx;
   unsigned int tile_cols;
   unsigned int tile_rows;
   unsigned int tile_cols_log2;
   unsigned int tile_rows_log2;
   unsigned int tile_size_bytes;
   int allow_intrabc;
   int allow_screen_content_tools;
   int reduced_tx_set;
   int tx_mode_select;
   unsigned int tile_col_start_sb[65];
   unsigned int tile_row_start_sb[65];
   size_t header_bits_consumed;
   /* Segmentation */
   int seg_enabled;
   int seg_feature_enabled[8][8]; /* [segment_id][feature_id] */
   int seg_feature_data[8][8];
   /* Quantization matrix */
   int using_qmatrix;
   int qm_y;
   int qm_u;
   int qm_v;
   /* Per-plane delta Q offsets */
   int delta_q_y_dc;
   int delta_q_u_dc;
   int delta_q_u_ac;
   int delta_q_v_dc;
   int delta_q_v_ac;
   int cdef_bits;
   int cdef_damping;
} stbi_avif__av1_frame_header;

typedef struct
{
   int start_and_end_present;
   unsigned int tile_start;
   unsigned int tile_end;
   size_t tile_data_byte_offset;
   size_t header_bits_consumed;
} stbi_avif__av1_tile_group_header;

/* AV1 range (symbol) decoder, matching AOM's daala EC implementation.
 * Uses a 32-bit dif window with XOR-based byte insertion. */
typedef struct
{
   const unsigned char *buf;       /* start of payload */
   const unsigned char *end;       /* end of payload */
   const unsigned char *bptr;      /* current read pointer */
   unsigned long long dif;    /* bit window (top 16 bits hold comparison value) */
   unsigned int         rng;       /* current range  (always >= 0x8000 after renorm) */
   int                  cnt;       /* number of unconsumed bits buffered */
   int                  initialized;
} stbi_avif__av1_range_decoder;

#define STBI_AVIF_AV1_OBU_SEQUENCE_HEADER 1
#define STBI_AVIF_AV1_OBU_TEMPORAL_DELIMITER 2
#define STBI_AVIF_AV1_OBU_FRAME_HEADER 3
#define STBI_AVIF_AV1_OBU_TILE_GROUP 4
#define STBI_AVIF_AV1_OBU_METADATA 5
#define STBI_AVIF_AV1_OBU_FRAME 6
#define STBI_AVIF_AV1_OBU_REDUNDANT_FRAME_HEADER 7
#define STBI_AVIF_AV1_OBU_PADDING 15

#define STBI_AVIF_AV1_CP_BT709 1
#define STBI_AVIF_AV1_TC_SRGB 13
#define STBI_AVIF_AV1_MC_IDENTITY 0

static const char *stbi_avif__g_failure_reason = "unknown error";

const char *stbi_avif_failure_reason(void)
{
   return stbi_avif__g_failure_reason;
}

void stbi_avif_image_free(void *retval_from_stbi_avif_load)
{
   STBI_AVIF_FREE(retval_from_stbi_avif_load);
}

static int stbi_avif__fail(const char *reason)
{
   stbi_avif__g_failure_reason = reason;
   return 0;
}

static void *stbi_avif__fail_ptr(const char *reason)
{
   stbi_avif__g_failure_reason = reason;
   return NULL;
}

static int stbi_avif__grow_array(void **array_ptr, int *capacity, size_t element_size, int min_needed)
{
   void *grown;
   int new_capacity;
   size_t byte_count;

   if (*capacity >= min_needed)
      return 1;

   new_capacity = (*capacity > 0) ? (*capacity * 2) : 8;
   while (new_capacity < min_needed)
   {
      if (new_capacity > 0x3fffffff)
         return stbi_avif__fail("array capacity overflow");
      new_capacity *= 2;
   }

   if ((size_t)new_capacity > ((size_t)-1) / element_size)
      return stbi_avif__fail("array byte size overflow");

   byte_count = ((size_t)new_capacity) * element_size;
   grown = STBI_AVIF_REALLOC(*array_ptr, byte_count);
   if (grown == NULL)
      return stbi_avif__fail("out of memory");

   *array_ptr = grown;
   *capacity = new_capacity;
   return 1;
}

static int stbi_avif__range_check(const stbi_avif__buffer *buffer, size_t offset, size_t count)
{
   if (offset > buffer->size)
      return 0;
   if (count > buffer->size - offset)
      return 0;
   return 1;
}

static unsigned long stbi_avif__read_be32(const unsigned char *p)
{
   return (((unsigned long)p[0]) << 24) |
          (((unsigned long)p[1]) << 16) |
          (((unsigned long)p[2]) << 8) |
          ((unsigned long)p[3]);
}

static unsigned int stbi_avif__read_be16(const unsigned char *p)
{
   return (unsigned int)((((unsigned int)p[0]) << 8) | ((unsigned int)p[1]));
}

static unsigned long stbi_avif__read_uleb128(const unsigned char *data, size_t size, size_t *consumed)
{
   unsigned long value;
   unsigned int shift;
   size_t i;

   value = 0;
   shift = 0;
   for (i = 0; i < size && i < 8; ++i)
   {
      unsigned int byte_value;

      byte_value = (unsigned int)data[i];
      value |= ((unsigned long)(byte_value & 0x7fu)) << shift;
      if ((byte_value & 0x80u) == 0)
      {
         *consumed = i + 1;
         return value;
      }
      shift += 7;
   }

   *consumed = 0;
   return 0;
}

static size_t stbi_avif__read_be_size(const unsigned char *p, int byte_count)
{
   size_t value;
   int i;

   value = 0;
   for (i = 0; i < byte_count; ++i)
      value = (value * 256u) + (size_t)p[i];
   return value;
}

static void stbi_avif__bit_reader_init(stbi_avif__bit_reader *reader, const unsigned char *data, size_t size)
{
   reader->data = data;
   reader->size = size;
   reader->bit_offset = 0;
}

static int stbi_avif__bit_reader_bits_left(const stbi_avif__bit_reader *reader, size_t bit_count)
{
   size_t total_bits;

   if (bit_count > ((size_t)-1) / 8u)
      return 0;
   total_bits = reader->size * 8u;
   if (reader->bit_offset > total_bits)
      return 0;
   return bit_count <= total_bits - reader->bit_offset;
}

static int stbi_avif__bit_read(stbi_avif__bit_reader *reader, unsigned int *bit)
{
   size_t byte_index;
   unsigned int bit_index;

   if (!stbi_avif__bit_reader_bits_left(reader, 1))
      return stbi_avif__fail("truncated AV1 bitstream");

   byte_index = reader->bit_offset >> 3;
   bit_index = 7u - (unsigned int)(reader->bit_offset & 7u);
   *bit = (((unsigned int)reader->data[byte_index]) >> bit_index) & 1u;
   reader->bit_offset += 1;
   return 1;
}

static int stbi_avif__bit_read_bits(stbi_avif__bit_reader *reader, unsigned int bit_count, unsigned long *value)
{
   unsigned long result;
   unsigned int bit;
   unsigned int i;

   if (bit_count > 32u)
      return stbi_avif__fail("unsupported AV1 bit width");

   result = 0;
   for (i = 0; i < bit_count; ++i)
   {
      if (!stbi_avif__bit_read(reader, &bit))
         return 0;
      result = (result << 1) | (unsigned long)bit;
   }

   *value = result;
   return 1;
}

static int stbi_avif__bit_skip(stbi_avif__bit_reader *reader, unsigned int bit_count)
{
   if (!stbi_avif__bit_reader_bits_left(reader, (size_t)bit_count))
      return stbi_avif__fail("truncated AV1 bitstream");
   reader->bit_offset += (size_t)bit_count;
   return 1;
}

static int stbi_avif__bit_read_flag(stbi_avif__bit_reader *reader, int *value)
{
   unsigned int bit;

   if (!stbi_avif__bit_read(reader, &bit))
      return 0;
   *value = (int)bit;
   return 1;
}

static unsigned int stbi_avif__ceil_log2_u32(unsigned int n)
{
   unsigned int v;
   unsigned int log2;

   if (n <= 1u)
      return 0u;

   v = n - 1u;
   log2 = 0u;
   while (v != 0u)
   {
      v >>= 1;
      ++log2;
   }
   return log2;
}

/* Decode AV1 ns(n) code (spec §4.10.10). */
static int stbi_avif__bit_read_ns(stbi_avif__bit_reader *reader,
                                  unsigned int n,
                                  unsigned int *value)
{
   unsigned int w;
   unsigned int m;
   unsigned long v;
   unsigned long extra;

   if (n <= 1u)
   {
      *value = 0u;
      return 1;
   }

   w = stbi_avif__ceil_log2_u32(n);
   m = (1u << w) - n;

   if (!stbi_avif__bit_read_bits(reader, w - 1u, &v))
      return 0;
   if (v < (unsigned long)m)
   {
      *value = (unsigned int)v;
      return 1;
   }

   if (!stbi_avif__bit_read_bits(reader, 1u, &extra))
      return 0;
   *value = (unsigned int)(((v << 1u) | extra) - (unsigned long)m);
   return 1;
}

static int stbi_avif__bit_read_su(stbi_avif__bit_reader *reader,
                                  unsigned int bits,
                                  int *value)
{
   unsigned long raw;
   unsigned long sign;
   unsigned long mag;

   if (bits == 0u || bits > 31u)
      return stbi_avif__fail("unsupported AV1 signed width");

   if (!stbi_avif__bit_read_bits(reader, bits, &raw))
      return 0;

   sign = 1ul << (bits - 1u);
   mag  = raw & (sign - 1u);
   if (raw & sign)
      *value = -(int)mag;
   else
      *value = (int)mag;
   return 1;
}

static int stbi_avif__av1_read_le_bytes(const unsigned char *data, size_t size,
                                        size_t offset, unsigned int byte_count,
                                        unsigned int *value)
{
   unsigned int v;
   unsigned int i;

   if (byte_count == 0u || byte_count > 4u)
      return stbi_avif__fail("unsupported AV1 tile size field width");
   if (offset + (size_t)byte_count > size)
      return stbi_avif__fail("truncated AV1 tile size field");

   v = 0u;
   for (i = 0u; i < byte_count; ++i)
      v |= ((unsigned int)data[offset + i]) << (8u * i);
   *value = v;
   return 1;
}

static int stbi_avif__parse_av1_sequence_header(const unsigned char *data, size_t size, stbi_avif__av1_sequence_header *header)
{
   stbi_avif__bit_reader bits;
   unsigned long value;
   unsigned long frame_width_bits_minus_1;
   unsigned long frame_height_bits_minus_1;
   int high_bitdepth;
   int twelve_bit;
   int color_description_present_flag;
   int flag;

   memset(header, 0, sizeof(*header));
   stbi_avif__bit_reader_init(&bits, data, size);

   if (!stbi_avif__bit_read_bits(&bits, 3, &value))
      return 0;
   header->seq_profile = (unsigned int)value;
   if (!stbi_avif__bit_read_flag(&bits, &header->still_picture))
      return 0;
   if (!stbi_avif__bit_read_flag(&bits, &header->reduced_still_picture_header))
      return 0;

   if (!header->still_picture)
      return stbi_avif__fail("only AV1 still-picture sequences are supported");
   if (!header->reduced_still_picture_header)
      return stbi_avif__fail("only reduced still-picture AV1 headers are supported");

   if (!stbi_avif__bit_read_bits(&bits, 5, &value))
      return 0;

   if (!stbi_avif__bit_read_bits(&bits, 4, &frame_width_bits_minus_1))
      return 0;
   if (!stbi_avif__bit_read_bits(&bits, 4, &frame_height_bits_minus_1))
      return 0;

   if (!stbi_avif__bit_read_bits(&bits, (unsigned int)frame_width_bits_minus_1 + 1u, &value))
      return 0;
   header->max_frame_width = (unsigned int)value + 1u;
   if (!stbi_avif__bit_read_bits(&bits, (unsigned int)frame_height_bits_minus_1 + 1u, &value))
      return 0;
   header->max_frame_height = (unsigned int)value + 1u;

   /*
    * AV1 spec §5.5.1 (after max_frame_height_minus_1):
    *
    * For reduced_still_picture_header = 1:
    *   use_128x128_superblock         f(1)   ← read and store
    *   enable_filter_intra            f(1)   ← skip
    *   enable_intra_edge_filter       f(1)   ← skip
    *   [if !reduced: inter tools block — ABSENT here]
    *   enable_superres                f(1)   ← skip
    *   enable_cdef                    f(1)   ← skip
    *   enable_restoration             f(1)   ← skip
    *   color_config() follows with high_bitdepth...
    */
   if (!stbi_avif__bit_read_flag(&bits, &header->use_128x128_superblock))
      return 0;
   if (!stbi_avif__bit_read_flag(&bits, &flag)) return 0; /* enable_filter_intra */
   header->enable_filter_intra = flag;
   if (!stbi_avif__bit_read_flag(&bits, &flag)) return 0; /* enable_intra_edge_filter */
   if (!stbi_avif__bit_read_flag(&bits, &header->enable_superres)) return 0;
   if (!stbi_avif__bit_read_flag(&bits, &header->enable_cdef)) return 0;
   if (!stbi_avif__bit_read_flag(&bits, &header->enable_restoration)) return 0;

   if (!stbi_avif__bit_read_flag(&bits, &high_bitdepth))
      return 0;
   twelve_bit = 0;
   if (header->seq_profile == 2u && high_bitdepth)
   {
      if (!stbi_avif__bit_read_flag(&bits, &twelve_bit))
         return 0;
   }
   header->bit_depth = twelve_bit ? 12u : (high_bitdepth ? 10u : 8u);

   if (header->seq_profile == 1u)
   {
      header->monochrome = 0;
   }
   else
   {
      if (!stbi_avif__bit_read_flag(&bits, &header->monochrome))
         return 0;
   }
   if (header->monochrome)
      return stbi_avif__fail("monochrome AV1 content is not supported");

   if (!stbi_avif__bit_read_flag(&bits, &color_description_present_flag))
      return 0;
   if (color_description_present_flag)
   {
      if (!stbi_avif__bit_read_bits(&bits, 8, &value))
         return 0;
      header->color_primaries = (unsigned int)value;
      if (!stbi_avif__bit_read_bits(&bits, 8, &value))
         return 0;
      header->transfer_characteristics = (unsigned int)value;
      if (!stbi_avif__bit_read_bits(&bits, 8, &value))
         return 0;
      header->matrix_coefficients = (unsigned int)value;
   }
   else
   {
      header->color_primaries = 2u;
      header->transfer_characteristics = 2u;
      header->matrix_coefficients = 2u;
   }

   if (color_description_present_flag &&
       header->color_primaries == STBI_AVIF_AV1_CP_BT709 &&
       header->transfer_characteristics == STBI_AVIF_AV1_TC_SRGB &&
       header->matrix_coefficients == STBI_AVIF_AV1_MC_IDENTITY)
   {
      header->color_range = 1;
      header->subsampling_x = 0;
      header->subsampling_y = 0;
      header->chroma_sample_position = 0u;
   }
   else
   {
      if (!stbi_avif__bit_read_flag(&bits, &header->color_range))
         return 0;
      if (header->seq_profile == 0u)
      {
         header->subsampling_x = 1;
         header->subsampling_y = 1;
      }
      else if (header->seq_profile == 1u)
      {
         header->subsampling_x = 0;
         header->subsampling_y = 0;
      }
      else
      {
         header->subsampling_x = 1;
         header->subsampling_y = 0;
      }

      if (header->subsampling_x && header->subsampling_y)
      {
         if (!stbi_avif__bit_read_bits(&bits, 2, &value))
            return 0;
         header->chroma_sample_position = (unsigned int)value;
      }
   }

   if (!stbi_avif__bit_read_flag(&bits, &header->separate_uv_delta_q))
      return 0;
   if (!stbi_avif__bit_read_flag(&bits, &header->film_grain_params_present))
      return 0;
   if (header->film_grain_params_present)
      return stbi_avif__fail("AV1 film grain is not supported");

   if ((header->subsampling_x == 0 && header->subsampling_y == 1) ||
       (header->subsampling_x != 0 && header->subsampling_x != 1) ||
       (header->subsampling_y != 0 && header->subsampling_y != 1))
      return stbi_avif__fail("unsupported AV1 chroma subsampling");

   return 1;
}

static int stbi_avif__parse_av1_obu_stream(const unsigned char *data, size_t size, stbi_avif__av1_headers *headers, int require_sequence_header)
{
   size_t offset;

   offset = 0;
   while (offset < size)
   {
      unsigned int obu_header;
      unsigned int obu_type;
      unsigned int extension_flag;
      unsigned int has_size_field;
      size_t header_size;
      size_t leb_size;
      unsigned long payload_size_long;
      size_t payload_size;
      const unsigned char *payload;

      if (size - offset < 1)
         return stbi_avif__fail("truncated AV1 OBU header");

      obu_header = (unsigned int)data[offset];
      if ((obu_header & 0x80u) != 0)
         return stbi_avif__fail("invalid AV1 forbidden bit");

      obu_type = (obu_header >> 3) & 15u;
      extension_flag = (obu_header >> 2) & 1u;
      has_size_field = (obu_header >> 1) & 1u;
      if ((obu_header & 1u) != 0)
         return stbi_avif__fail("invalid AV1 OBU reserved bit");

      header_size = 1;
      if (extension_flag)
      {
         unsigned int extension_byte;

         if (size - offset < 2)
            return stbi_avif__fail("truncated AV1 OBU extension");
         extension_byte = (unsigned int)data[offset + 1];
         if ((extension_byte >> 5) != 0)
            return stbi_avif__fail("temporal or spatial layering is not supported");
         header_size += 1;
      }

      if (!has_size_field)
         return stbi_avif__fail("AV1 OBU size field is required");
      payload_size_long = stbi_avif__read_uleb128(data + offset + header_size, size - offset - header_size, &leb_size);
      if (leb_size == 0)
         return stbi_avif__fail("invalid AV1 OBU size field");
      header_size += leb_size;
      payload_size = (size_t)payload_size_long;
      if (payload_size > size - offset - header_size)
         return stbi_avif__fail("AV1 OBU payload exceeds buffer bounds");

      payload = data + offset + header_size;
      if (obu_type == STBI_AVIF_AV1_OBU_SEQUENCE_HEADER)
      {
         if (!stbi_avif__parse_av1_sequence_header(payload, payload_size, &headers->sequence_header))
            return 0;
         headers->saw_sequence_header = 1;
      }
      else if (obu_type == STBI_AVIF_AV1_OBU_FRAME_HEADER || obu_type == STBI_AVIF_AV1_OBU_FRAME)
      {
         if (require_sequence_header && !headers->saw_sequence_header)
            return stbi_avif__fail("AV1 frame OBU appeared before sequence header");
         headers->saw_frame_header = 1;
      }
      else if (obu_type == STBI_AVIF_AV1_OBU_TEMPORAL_DELIMITER ||
               obu_type == STBI_AVIF_AV1_OBU_METADATA ||
               obu_type == STBI_AVIF_AV1_OBU_PADDING)
      {
      }
      else if (obu_type == STBI_AVIF_AV1_OBU_TILE_GROUP)
      {
         if (!headers->saw_frame_header)
            return stbi_avif__fail("AV1 tile group appeared before frame header");
      }
      else if (obu_type == STBI_AVIF_AV1_OBU_REDUNDANT_FRAME_HEADER)
      {
         return stbi_avif__fail("redundant AV1 frame headers are not supported");
      }
      else
      {
         return stbi_avif__fail("unsupported AV1 OBU type");
      }

      offset += header_size + payload_size;
   }

   return 1;
}

static int stbi_avif__parse_av1c_header(const stbi_avif__buffer *buffer, const stbi_avif__parser *parser)
{
   const unsigned char *data;
   unsigned int marker;
   unsigned int version;

   if (parser->av1c_size < 4u)
      return stbi_avif__fail("truncated av1C property");
   if (!stbi_avif__range_check(buffer, parser->av1c_offset, parser->av1c_size))
      return stbi_avif__fail("av1C property is out of bounds");

   data = buffer->data + parser->av1c_offset;
   marker = ((unsigned int)data[0] >> 7) & 1u;
   version = (unsigned int)data[0] & 0x7fu;
   if (marker != 1u)
      return stbi_avif__fail("invalid av1C marker");
   if (version != 1u)
      return stbi_avif__fail("unsupported av1C version");
   return 1;
}

static int stbi_avif__parse_av1_headers(const unsigned char *data, size_t size, const stbi_avif__parser *parser, stbi_avif__av1_headers *headers)
{
   stbi_avif__buffer buffer;

   memset(headers, 0, sizeof(*headers));
   buffer.data = data;
   buffer.size = size;

   if (!stbi_avif__parse_av1c_header(&buffer, parser))
      return 0;

   if (parser->av1c_size > 4u)
   {
      if (!stbi_avif__parse_av1_obu_stream(buffer.data + parser->av1c_offset + 4u, parser->av1c_size - 4u, headers, 0))
         return 0;
   }

   if (!stbi_avif__parse_av1_obu_stream(buffer.data + parser->payload_offset, parser->payload_size, headers, !headers->saw_sequence_header))
      return 0;

   if (!headers->saw_sequence_header)
      return stbi_avif__fail("missing AV1 sequence header OBU");
   if (!headers->saw_frame_header)
      return stbi_avif__fail("missing AV1 frame header OBU");
   if (headers->sequence_header.max_frame_width == 0u || headers->sequence_header.max_frame_height == 0u)
      return stbi_avif__fail("invalid AV1 frame size limits");
   if (parser->width > headers->sequence_header.max_frame_width || parser->height > headers->sequence_header.max_frame_height)
      return stbi_avif__fail("AVIF item dimensions exceed AV1 sequence limits");

   return 1;
}

static int stbi_avif__index_av1_frame_obus(const unsigned char *data, size_t size, stbi_avif__av1_frame_index *index)
{
   size_t offset;

   memset(index, 0, sizeof(*index));
   offset = 0;
   while (offset < size)
   {
      unsigned int obu_header;
      unsigned int obu_type;
      unsigned int extension_flag;
      unsigned int has_size_field;
      size_t header_size;
      size_t leb_size;
      unsigned long payload_size_long;
      size_t payload_size;

      if (size - offset < 1)
         return stbi_avif__fail("truncated AV1 OBU header in payload");

      obu_header = (unsigned int)data[offset];
      if ((obu_header & 0x80u) != 0)
         return stbi_avif__fail("invalid AV1 forbidden bit in payload");

      obu_type = (obu_header >> 3) & 15u;
      extension_flag = (obu_header >> 2) & 1u;
      has_size_field = (obu_header >> 1) & 1u;
      if ((obu_header & 1u) != 0)
         return stbi_avif__fail("invalid AV1 OBU reserved bit in payload");

      header_size = 1;
      if (extension_flag)
      {
         unsigned int extension_byte;

         if (size - offset < 2)
            return stbi_avif__fail("truncated AV1 OBU extension in payload");
         extension_byte = (unsigned int)data[offset + 1];
         if ((extension_byte >> 5) != 0)
            return stbi_avif__fail("temporal or spatial layering is not supported");
         header_size += 1;
      }

      if (!has_size_field)
         return stbi_avif__fail("AV1 OBU size field is required in payload");
      payload_size_long = stbi_avif__read_uleb128(data + offset + header_size, size - offset - header_size, &leb_size);
      if (leb_size == 0)
         return stbi_avif__fail("invalid AV1 OBU size field in payload");
      header_size += leb_size;
      payload_size = (size_t)payload_size_long;
      if (payload_size > size - offset - header_size)
         return stbi_avif__fail("AV1 OBU payload exceeds payload bounds");

      if (obu_type == STBI_AVIF_AV1_OBU_FRAME_HEADER)
      {
         if (!index->saw_frame_header)
         {
            index->saw_frame_header = 1;
            index->frame_header_offset = offset + header_size;
            index->frame_header_size = payload_size;
         }
      }
      else if (obu_type == STBI_AVIF_AV1_OBU_FRAME)
      {
         if (!index->saw_frame_header)
         {
            index->saw_frame_header = 1;
            index->frame_is_combined_obu = 1;
            index->frame_header_offset = offset + header_size;
            index->frame_header_size = payload_size;
         }
         if (!index->saw_tile_group)
         {
            index->saw_tile_group = 1;
            index->tile_group_offset = offset + header_size;
            index->tile_group_size = payload_size;
         }
      }
      else if (obu_type == STBI_AVIF_AV1_OBU_TILE_GROUP)
      {
         if (!index->saw_tile_group)
         {
            index->saw_tile_group = 1;
            index->tile_group_offset = offset + header_size;
            index->tile_group_size = payload_size;
         }
      }

      offset += header_size + payload_size;
   }

   if (!index->saw_frame_header)
      return stbi_avif__fail("missing AV1 frame header payload OBU");
   if (!index->saw_tile_group)
      return stbi_avif__fail("missing AV1 tile group payload OBU");

   return 1;
}

static int stbi_avif__parse_av1_frame_header(const unsigned char *data, size_t size, const stbi_avif__av1_sequence_header *seq, stbi_avif__av1_frame_header *frame)
{
   stbi_avif__bit_reader bits;
   unsigned long value;
   unsigned int sb_size;
   unsigned int sb_cols;
   unsigned int sb_rows;
   unsigned int max_tile_width_sb;
   unsigned int max_tile_area_sb;
   unsigned int i;
   unsigned int uniform_tile_spacing_flag;
   unsigned int tile_cols;
   unsigned int tile_rows;
   unsigned int tile_cols_log2;
   unsigned int tile_rows_log2;
   unsigned int start_sb;
   unsigned int tile_size_bytes_minus_1;
   unsigned int base_q_idx;
   int render_and_frame_size_different;
   int seg_enabled;
   int delta_q_present;
   int delta_lf_present;
   int coded_lossless;
   int using_qmatrix;
   int delta_q_y_dc;
   int delta_q_u_dc;
   int delta_q_u_ac;
   int delta_q_v_dc;
   int delta_q_v_ac;
   int allow_intrabc;
   int tx_mode_select;
   int reduced_tx_set;
   int apply_grain;

   memset(frame, 0, sizeof(*frame));
   if (data == NULL || size == 0)
      return stbi_avif__fail("missing AV1 frame header payload");

   stbi_avif__bit_reader_init(&bits, data, size);

   if (seq->reduced_still_picture_header)
   {
      int disable_cdf_update;
      int allow_screen_content_tools;
      int force_integer_mv_flag;

      /* Reduced still-picture uses an implicit KEY_FRAME + show_frame=1. */
      frame->frame_type = 0u; /* KEY_FRAME */
      frame->show_frame = 1;
      frame->frame_width = seq->max_frame_width;
      frame->frame_height = seq->max_frame_height;

      /* disable_cdf_update */
      if (!stbi_avif__bit_read_flag(&bits, &disable_cdf_update))
         return 0;

      /* allow_screen_content_tools (force_screen_content_tools==2 for reduced) */
      if (!stbi_avif__bit_read_flag(&bits, &allow_screen_content_tools))
         return 0;

      /* force_integer_mv (force_integer_mv==2 for reduced, only if screen content) */
      force_integer_mv_flag = 0;
      if (allow_screen_content_tools) {
         if (!stbi_avif__bit_read_flag(&bits, &force_integer_mv_flag))
            return 0;
      }

      /* frame_size_override_flag is implicitly 0 for reduced */
      /* setup_frame_size: no frame size bits (override=0) */
      /* setup_superres: read bits only if enable_superres */
      if (seq->enable_superres) {
         int use_superres;
         if (!stbi_avif__bit_read_flag(&bits, &use_superres))
            return 0;
         if (use_superres) {
            if (!stbi_avif__bit_read_bits(&bits, 3u, &value)) return 0; /* SUPERRES_SCALE_BITS=3 */
         }
      }

      /* render_size() */
      if (!stbi_avif__bit_read_flag(&bits, &render_and_frame_size_different))
         return 0;
      if (render_and_frame_size_different)
      {
         if (!stbi_avif__bit_read_bits(&bits, 16, &value)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 16, &value)) return 0;
      }

      /* allow_intrabc: only if allow_screen_content_tools and no superres */
      allow_intrabc = 0;
      if (allow_screen_content_tools) {
         if (!stbi_avif__bit_read_flag(&bits, &allow_intrabc))
            return 0;
      }
      frame->allow_intrabc = allow_intrabc;
      frame->allow_screen_content_tools = allow_screen_content_tools;
      fprintf(stderr, "reduced_fh: disable_cdf=%d screen_content=%d int_mv=%d allow_intrabc=%d\n",
         disable_cdf_update, allow_screen_content_tools, force_integer_mv_flag, allow_intrabc);

      /* tile_info() */
      sb_size = seq->use_128x128_superblock ? 128u : 64u;
      sb_cols = (frame->frame_width  + sb_size - 1u) / sb_size;
      sb_rows = (frame->frame_height + sb_size - 1u) / sb_size;
      max_tile_width_sb = 4096u / sb_size;
      max_tile_area_sb  = (4096u * 2304u) / (sb_size * sb_size);

      tile_cols = 0u;
      tile_rows = 0u;
      tile_cols_log2 = 0u;
      tile_rows_log2 = 0u;
      frame->tile_col_start_sb[0] = 0u;
      frame->tile_row_start_sb[0] = 0u;

      if (!stbi_avif__bit_read_bits(&bits, 1u, &value))
         return 0;
      uniform_tile_spacing_flag = (unsigned int)value;

      if (uniform_tile_spacing_flag)
      {
         unsigned int min_log2_tile_cols;
         unsigned int max_log2_tile_cols;
         unsigned int min_log2_tile_rows;
         unsigned int max_log2_tile_rows;

         min_log2_tile_cols = 0u;
         while ((max_tile_width_sb << min_log2_tile_cols) < sb_cols)
            ++min_log2_tile_cols;

         max_log2_tile_cols = 0u;
         while ((1u << max_log2_tile_cols) < sb_cols)
            ++max_log2_tile_cols;
         if (max_log2_tile_cols > 6u)
            max_log2_tile_cols = 6u;

         tile_cols_log2 = min_log2_tile_cols;
         while (tile_cols_log2 < max_log2_tile_cols)
         {
            int increment;
            if (!stbi_avif__bit_read_flag(&bits, &increment))
               return 0;
            if (!increment)
               break;
            ++tile_cols_log2;
         }

         tile_cols = 1u << tile_cols_log2;
         for (i = 0u; i <= tile_cols; ++i)
            frame->tile_col_start_sb[i] = (i * sb_cols) / tile_cols;

         min_log2_tile_rows = 0u;
         while ((max_tile_area_sb << (tile_cols_log2 + min_log2_tile_rows)) < (sb_rows * sb_cols))
            ++min_log2_tile_rows;

         max_log2_tile_rows = 0u;
         while ((1u << max_log2_tile_rows) < sb_rows)
            ++max_log2_tile_rows;
         if (max_log2_tile_rows > 6u)
            max_log2_tile_rows = 6u;

         tile_rows_log2 = min_log2_tile_rows;
         while (tile_rows_log2 < max_log2_tile_rows)
         {
            int increment;
            if (!stbi_avif__bit_read_flag(&bits, &increment))
               return 0;
            if (!increment)
               break;
            ++tile_rows_log2;
         }

         tile_rows = 1u << tile_rows_log2;
         for (i = 0u; i <= tile_rows; ++i)
            frame->tile_row_start_sb[i] = (i * sb_rows) / tile_rows;
      }
      else
      {
         start_sb = 0u;
         tile_cols = 0u;
         while (start_sb < sb_cols)
         {
            unsigned int max_w;
            unsigned int this_w;
            if (tile_cols >= 64u)
               return stbi_avif__fail("too many AV1 tile columns");
            max_w = sb_cols - start_sb;
            if (max_w > max_tile_width_sb)
               max_w = max_tile_width_sb;
            if (!stbi_avif__bit_read_ns(&bits, max_w, &this_w))
               return 0;
            this_w += 1u;
            start_sb += this_w;
            ++tile_cols;
            frame->tile_col_start_sb[tile_cols] = start_sb;
         }

         start_sb = 0u;
         tile_rows = 0u;
         while (start_sb < sb_rows)
         {
            unsigned int max_h;
            unsigned int this_h;
            unsigned int widest;
            unsigned int max_tile_height_sb;

            if (tile_rows >= 64u)
               return stbi_avif__fail("too many AV1 tile rows");

            widest = 1u;
            for (i = 0u; i < tile_cols; ++i)
            {
               unsigned int w = frame->tile_col_start_sb[i + 1u] - frame->tile_col_start_sb[i];
               if (w > widest)
                  widest = w;
            }
            max_tile_height_sb = max_tile_area_sb / widest;
            if (max_tile_height_sb == 0u)
               max_tile_height_sb = 1u;

            max_h = sb_rows - start_sb;
            if (max_h > max_tile_height_sb)
               max_h = max_tile_height_sb;
            if (!stbi_avif__bit_read_ns(&bits, max_h, &this_h))
               return 0;
            this_h += 1u;
            start_sb += this_h;
            ++tile_rows;
            frame->tile_row_start_sb[tile_rows] = start_sb;
         }

         tile_cols_log2 = stbi_avif__ceil_log2_u32(tile_cols);
         tile_rows_log2 = stbi_avif__ceil_log2_u32(tile_rows);
      }

      if (tile_cols == 0u || tile_rows == 0u)
         return stbi_avif__fail("invalid AV1 tile layout");
      if (tile_cols > 64u || tile_rows > 64u)
         return stbi_avif__fail("AV1 tile grid too large");

      frame->tile_cols = tile_cols;
      frame->tile_rows = tile_rows;
      frame->tile_cols_log2 = tile_cols_log2;
      frame->tile_rows_log2 = tile_rows_log2;
      fprintf(stderr, "TILES: cols=%u rows=%u (log2: %u,%u)\n",
         tile_cols, tile_rows, tile_cols_log2, tile_rows_log2);

      if (tile_cols * tile_rows > 1u)
      {
         if (!stbi_avif__bit_read_bits(&bits, 2u, &value))
            return 0; /* tile_size_bytes_minus_1 */
         tile_size_bytes_minus_1 = (unsigned int)value;
      }
      else
      {
         tile_size_bytes_minus_1 = 0u;
      }
      frame->tile_size_bytes = tile_size_bytes_minus_1 + 1u;

      /* quantization_params() */
      if (!stbi_avif__bit_read_bits(&bits, 8u, &value))
         return 0;
      base_q_idx = (unsigned int)value;
      frame->base_q_idx = base_q_idx;

      fprintf(stderr, "quant: base_q_idx=%u separate_uv_delta_q=%d\n",
         base_q_idx, seq->separate_uv_delta_q);

      /* DeltaQ values: 1-bit flag, if set: inv_signed_literal(6) = 6 magnitude + 1 sign LSB */
      {
         int diff_uv_delta = 0;

         /* read_delta_q helper: inline for C89 */
         #define STBI_AVIF__READ_DELTA_Q(bits_ptr, out_val) do { \
            int rdq_flag; \
            if (!stbi_avif__bit_read_flag(bits_ptr, &rdq_flag)) return 0; \
            if (rdq_flag) { \
               unsigned long rdq_mag; int rdq_sign; \
               if (!stbi_avif__bit_read_bits(bits_ptr, 6u, &rdq_mag)) return 0; \
               if (!stbi_avif__bit_read_flag(bits_ptr, &rdq_sign)) return 0; \
               (out_val) = rdq_sign ? -(int)rdq_mag : (int)rdq_mag; \
            } else { \
               (out_val) = 0; \
            } \
         } while(0)

         STBI_AVIF__READ_DELTA_Q(&bits, delta_q_y_dc);

         /* num_planes > 1 */
         if (seq->separate_uv_delta_q) {
            if (!stbi_avif__bit_read_flag(&bits, &diff_uv_delta)) return 0;
         }
         STBI_AVIF__READ_DELTA_Q(&bits, delta_q_u_dc);
         STBI_AVIF__READ_DELTA_Q(&bits, delta_q_u_ac);
         if (diff_uv_delta) {
            STBI_AVIF__READ_DELTA_Q(&bits, delta_q_v_dc);
            STBI_AVIF__READ_DELTA_Q(&bits, delta_q_v_ac);
         } else {
            delta_q_v_dc = delta_q_u_dc;
            delta_q_v_ac = delta_q_u_ac;
         }

         #undef STBI_AVIF__READ_DELTA_Q

         frame->delta_q_y_dc = delta_q_y_dc;
         frame->delta_q_u_dc = delta_q_u_dc;
         frame->delta_q_u_ac = delta_q_u_ac;
         frame->delta_q_v_dc = delta_q_v_dc;
         frame->delta_q_v_ac = delta_q_v_ac;
         fprintf(stderr, "delta_q: y_dc=%d u_dc=%d u_ac=%d v_dc=%d v_ac=%d\n",
            delta_q_y_dc, delta_q_u_dc, delta_q_u_ac, delta_q_v_dc, delta_q_v_ac);
      }

      if (!stbi_avif__bit_read_flag(&bits, &using_qmatrix))
         return 0;
      frame->using_qmatrix = using_qmatrix;
      if (using_qmatrix)
      {
         unsigned long qm_y_val, qm_u_val, qm_v_val;
         if (!stbi_avif__bit_read_bits(&bits, 4u, &qm_y_val)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 4u, &qm_u_val)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 4u, &qm_v_val)) return 0;
         frame->qm_y = (int)qm_y_val;
         frame->qm_u = (int)qm_u_val;
         frame->qm_v = (int)qm_v_val;
         fprintf(stderr, "QM: y=%d u=%d v=%d\n", frame->qm_y, frame->qm_u, frame->qm_v);
      }

      if (!stbi_avif__bit_read_flag(&bits, &seg_enabled))
         return 0;
      frame->seg_enabled = seg_enabled;

      /* segmentation_params() — for key frames, update_map=1, update_data=1 implicitly */
      if (seg_enabled)
      {
         /* SEG_LVL_MAX=8, MAX_SEGMENTS=8 */
         /* seg_feature_data_max:  {255, 63, 63, 63, 63, 7, 0, 0} */
         /* seg_feature_signed:    {1,   1,  1,  1,  1,  0, 0, 0} */
         static const int seg_data_max[8]    = { 255, 63, 63, 63, 63, 7, 0, 0 };
         static const int seg_data_signed[8] = { 1,   1,  1,  1,  1,  0, 0, 0 };
         int si, fi;
         for (si = 0; si < 8; ++si) {
            for (fi = 0; fi < 8; ++fi) {
               int feat_enabled;
               if (!stbi_avif__bit_read_flag(&bits, &feat_enabled))
                  return 0;
               frame->seg_feature_enabled[si][fi] = feat_enabled;
               if (feat_enabled) {
                  int dmax = seg_data_max[fi];
                  if (dmax > 0) {
                     /* get_unsigned_bits(dmax) = bits needed */
                     int ubits = 0;
                     { unsigned int tmp = (unsigned int)dmax; while (tmp) { ++ubits; tmp >>= 1; } }
                     if (seg_data_signed[fi]) {
                        /* inv_signed_literal: read ubits bits of magnitude, then 1 sign bit */
                        unsigned long mag_val;
                        int sign_bit;
                        if (!stbi_avif__bit_read_bits(&bits, (unsigned int)ubits, &mag_val))
                           return 0;
                        if (!stbi_avif__bit_read_flag(&bits, &sign_bit))
                           return 0;
                        {
                           int sval = (int)mag_val;
                           if (sign_bit) sval = -sval;
                           if (sval < -dmax) sval = -dmax;
                           if (sval > dmax) sval = dmax;
                           frame->seg_feature_data[si][fi] = sval;
                        }
                     } else {
                        unsigned long uval;
                        if (!stbi_avif__bit_read_bits(&bits, (unsigned int)ubits, &uval))
                           return 0;
                        frame->seg_feature_data[si][fi] = (int)uval;
                     }
                  } else {
                     frame->seg_feature_data[si][fi] = 0;
                  }
               } else {
                  frame->seg_feature_data[si][fi] = 0;
               }
            }
         }
         fprintf(stderr, "SEG: enabled, seg0_altq=%d seg1_altq=%d\n",
            frame->seg_feature_data[0][0], frame->seg_feature_data[1][0]);
      }

      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      fprintf(stderr, "delta_q_present(per-SB)=%d seg_enabled=%d using_qmatrix=%d\n", delta_q_present, seg_enabled, using_qmatrix);
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_bits(&bits, 2u, &value))
            return 0;
         fprintf(stderr, "delta_q_res=%u\n", (unsigned)value);
      }

      delta_lf_present = 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_flag(&bits, &delta_lf_present))
            return 0;
         if (delta_lf_present)
         {
            if (!stbi_avif__bit_read_bits(&bits, 2u, &value)) return 0;
            if (!stbi_avif__bit_read_flag(&bits, &seg_enabled)) return 0;
         }
      }

      coded_lossless = (base_q_idx == 0u &&
                        delta_q_y_dc == 0 &&
                        delta_q_u_dc == 0 &&
                        delta_q_u_ac == 0 &&
                        delta_q_v_dc == 0 &&
                        delta_q_v_ac == 0 &&
                        !using_qmatrix);

      /* loop_filter_params() are omitted when allow_intrabc == 1. */
      if (!allow_intrabc && !coded_lossless)
      {
         int loop_filter_delta_enabled;
         int loop_filter_delta_update;
         if (!stbi_avif__bit_read_bits(&bits, 6u, &value)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 6u, &value)) return 0;
         if (!seq->monochrome)
         {
            if (!stbi_avif__bit_read_bits(&bits, 6u, &value)) return 0;
            if (!stbi_avif__bit_read_bits(&bits, 6u, &value)) return 0;
         }
         if (!stbi_avif__bit_read_bits(&bits, 3u, &value)) return 0;
         if (!stbi_avif__bit_read_flag(&bits, &loop_filter_delta_enabled)) return 0;
         if (loop_filter_delta_enabled)
         {
            if (!stbi_avif__bit_read_flag(&bits, &loop_filter_delta_update)) return 0;
            if (loop_filter_delta_update)
            {
               for (i = 0u; i < 8u; ++i)
               {
                  int update;
                  if (!stbi_avif__bit_read_flag(&bits, &update)) return 0;
                  if (update && !stbi_avif__bit_read_su(&bits, 7u, &delta_q_y_dc)) return 0;
               }
               for (i = 0u; i < 2u; ++i)
               {
                  int update;
                  if (!stbi_avif__bit_read_flag(&bits, &update)) return 0;
                  if (update && !stbi_avif__bit_read_su(&bits, 7u, &delta_q_y_dc)) return 0;
               }
            }
         }
      }

      if (seq->enable_cdef && !allow_intrabc && !coded_lossless)
      {
         /* cdef_params() */
         unsigned long cdef_damping_minus_3, cdef_bits_val;
         int nb_cdef_strengths;
         int ci;
         if (!stbi_avif__bit_read_bits(&bits, 2u, &cdef_damping_minus_3)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 2u, &cdef_bits_val)) return 0;
         frame->cdef_damping = (int)cdef_damping_minus_3 + 3;
         frame->cdef_bits = (int)cdef_bits_val;
         nb_cdef_strengths = 1 << frame->cdef_bits;
         for (ci = 0; ci < nb_cdef_strengths; ++ci) {
            if (!stbi_avif__bit_read_bits(&bits, 6u, &value)) return 0; /* Y strength */
            if (!stbi_avif__bit_read_bits(&bits, 6u, &value)) return 0; /* UV strength */
         }
         fprintf(stderr, "CDEF: damping=%d bits=%d strengths=%d\n",
            frame->cdef_damping, frame->cdef_bits, nb_cdef_strengths);
      }
      if (seq->enable_restoration && !allow_intrabc)
         return stbi_avif__fail("AV1 restoration parsing is not implemented yet");

      if (!coded_lossless)
      {
         if (!stbi_avif__bit_read_flag(&bits, &tx_mode_select))
            return 0;
         frame->tx_mode_select = tx_mode_select;
      }

      if (!stbi_avif__bit_read_flag(&bits, &reduced_tx_set))
         return 0;
      frame->reduced_tx_set = reduced_tx_set;

      if (seq->film_grain_params_present)
      {
         if (!stbi_avif__bit_read_flag(&bits, &apply_grain))
            return 0;
         if (apply_grain)
            return stbi_avif__fail("AV1 film grain is not supported yet");
      }

      frame->header_bits_consumed = bits.bit_offset;
      return 1;
   }

   return stbi_avif__fail("non-reduced AV1 frame headers are not supported yet");
}

static int stbi_avif__parse_av1_tile_group_header(const unsigned char *data, size_t size,
                                                  size_t bit_offset,
                                                  const stbi_avif__av1_frame_header *frame,
                                                  stbi_avif__av1_tile_group_header *tile_group)
{
   stbi_avif__bit_reader bits;
   int start_end_present;
   unsigned int tile_bits;
   unsigned long value;
   size_t aligned_bit_offset;

   memset(tile_group, 0, sizeof(*tile_group));
   if (data == NULL || size == 0)
      return stbi_avif__fail("missing AV1 tile group payload");
   if (frame == NULL)
      return stbi_avif__fail("missing AV1 frame header context");

   stbi_avif__bit_reader_init(&bits, data, size);
   bits.bit_offset = bit_offset;
   fprintf(stderr, "TILE_GROUP: bit_offset_from_frame=%lu\n", (unsigned long)bit_offset);

   /* In OBU_FRAME, tile-group syntax starts on the next byte after frame header trailing bits. */
   if (bits.bit_offset & 7u)
      bits.bit_offset += 8u - (bits.bit_offset & 7u);

   if (!stbi_avif__bit_reader_bits_left(&bits, 1))
      return stbi_avif__fail("truncated AV1 tile group header");

   /* AV1 spec §5.11.1: tile_start_and_end_present_flag is only read when NumTiles > 1 */
   {
      unsigned int num_tiles = frame->tile_cols * frame->tile_rows;
      if (num_tiles > 1u) {
         if (!stbi_avif__bit_read_flag(&bits, &start_end_present))
            return 0;
      } else {
         start_end_present = 0;
      }
   }

   tile_group->start_and_end_present = start_end_present;
   if (start_end_present)
   {
      tile_bits = frame->tile_cols_log2 + frame->tile_rows_log2;
      if (!stbi_avif__bit_read_bits(&bits, tile_bits, &value))
         return 0;
      tile_group->tile_start = (unsigned int)value;
      if (!stbi_avif__bit_read_bits(&bits, tile_bits, &value))
         return 0;
      tile_group->tile_end = (unsigned int)value;
   }
   else
   {
      tile_group->tile_start = 0u;
      tile_group->tile_end = frame->tile_cols * frame->tile_rows - 1u;
   }
   aligned_bit_offset = bits.bit_offset;
   if (aligned_bit_offset & 7u)
      aligned_bit_offset += 8u - (aligned_bit_offset & 7u);

   tile_group->header_bits_consumed = bits.bit_offset;
   tile_group->tile_data_byte_offset = aligned_bit_offset >> 3;
   fprintf(stderr, "TILE_GROUP: tile_data_byte_offset=%lu start_end=%u\n",
      (unsigned long)tile_group->tile_data_byte_offset, start_end_present);

   if (tile_group->tile_end < tile_group->tile_start)
      return stbi_avif__fail("invalid AV1 tile group range");
   if (tile_group->tile_end >= frame->tile_cols * frame->tile_rows)
      return stbi_avif__fail("AV1 tile group index out of range");

   if (!stbi_avif__bit_reader_bits_left(&bits, 1))
      return stbi_avif__fail("missing AV1 tile payload bytes");

   return 1;
}

/*
 * =============================================================================
 *  AV1 od_ec RANGE DECODER  (libaom entdec.c, §8.2 normative)
 * =============================================================================
 *
 * Model: `value` is always a 15-bit integer representing which fraction of
 * `range` the current code point falls in.  Each symbol read selects an
 * interval, subtracts the lower bound, and renormalises so range >= 0x8000
 * again by left-shifting and reading fresh bytes from the tile buffer.
 */

/* Refill the 64-bit dif window from the byte stream (matches dav1d ctx_refill). */
static void stbi_avif__av1_rd_refill(stbi_avif__av1_range_decoder *rd)
{
   int c;
   unsigned long long dif;
   const unsigned char *bptr;
   const unsigned char *end;
   dif   = rd->dif;
   bptr  = rd->bptr;
   end   = rd->end;
   c = 40 - rd->cnt;  /* = EC_WIN_SIZE - cnt - 24, first free bit position */
   do {
      if (bptr >= end) {
         /* fill remaining bits with 1s (complement of 0x00 stream) */
         dif |= ~(~(unsigned long long)0xFFu << c);
         break;
      }
      dif |= (unsigned long long)((unsigned int)bptr[0] ^ 0xFFu) << c;
      bptr++;
      c -= 8;
   } while (c >= 0);
   rd->dif  = dif;
   rd->cnt  = 40 - c;  /* = EC_WIN_SIZE - c - 24 */
   rd->bptr = bptr;
}

/*
 * init (matches AOM od_ec_dec_init).
 * bit_offset: bits already consumed by the tile-group header.
 */
static int stbi_avif__av1_range_decoder_init(stbi_avif__av1_range_decoder *decoder,
                                              const unsigned char *data, size_t size,
                                              size_t bit_offset)
{
   size_t byte_start;
   memset(decoder, 0, sizeof(*decoder));
   if (data == NULL || size == 0)
      return stbi_avif__fail("missing AV1 entropy payload");

   byte_start = bit_offset >> 3;
   if (byte_start >= size)
      return stbi_avif__fail("truncated AV1 tile payload");

   decoder->buf  = data + byte_start;
   decoder->end  = data + size;
   decoder->bptr = decoder->buf;
   decoder->dif  = 0ULL;
   decoder->rng  = 0x8000u;
   decoder->cnt  = -15;
   decoder->initialized = 1;
   stbi_avif__av1_rd_refill(decoder);
   fprintf(stderr, "RD_INIT: dif=%llu rng=%u cnt=%d b0=%02x b1=%02x b2=%02x\n", (unsigned long long)decoder->dif, decoder->rng, decoder->cnt, decoder->buf[0], decoder->buf[1], decoder->buf[2]);
   return 1;
}

/*
 * Normalize: left-shift range to restore invariant rng >= 0x8000.
 * Matches AOM od_ec_dec_normalize. Returns ret (the decoded symbol).
 */
static unsigned int stbi_avif__av1_rd_normalize(stbi_avif__av1_range_decoder *rd,
                                                 unsigned long long dif, unsigned int rng,
                                                 unsigned int ret)
{
   int d;
   unsigned int r;
   /* Count leading zeros in 16-bit representation of rng: d = 16 - ilog(rng) */
   d = 0;
   r = rng;
   while (r < 0x8000u) { r <<= 1u; ++d; }
   rd->cnt -= d;
   rd->dif  = (dif << d);
   rd->rng  = rng << d;
   if (rd->cnt < 0) stbi_avif__av1_rd_refill(rd);
   return ret;
}

/*
 * read_symbol (matches AOM od_ec_decode_cdf_q15):
 *
 * CDF array has nsyms values stored in ascending order with cdf[nsyms-1]=32768.
 * Internally converts to AOM's ICDF convention for threshold computation.
 *
 * nsyms >= 2; cdf[nsyms-1] must be 32768.
 */
static int stbi_avif__dbg_sym_cnt = 0;
static int stbi_avif__dbg_sym_max = 200;
static unsigned int stbi_avif__av1_read_symbol(stbi_avif__av1_range_decoder *rd,
                                                const unsigned short *cdf, int nsyms)
{
   unsigned int r, c, u, v, ret;
   unsigned long long dif;
   int sym;

   r   = rd->rng;
   dif = rd->dif;
   c   = (unsigned int)(dif >> 48);   /* top 16 bits of dif window */

   v   = r;
   sym = -1;
   do {
      u = v;
      ++sym;
      /* v = ((r >> 8) * (icdf >> EC_PROB_SHIFT) >> 1) + EC_MIN_PROB * (N - sym)
         where icdf = 32768 - cdf[sym], EC_PROB_SHIFT=6, EC_MIN_PROB=4,
         N = nsyms - 1. Note: EC_MIN_PROB term is 4*(N-sym) matching dav1d's
         4*(n_symbols-val) with n_symbols = nsyms-1 (terminal 32768 not counted). */
      {
         unsigned int icdf_val = 32768u - (unsigned int)cdf[sym];
         unsigned int Nv = (unsigned int)(nsyms - 1);
         v = (((r >> 8u) * (icdf_val >> 6u)) >> 1u) + 4u * (Nv - (unsigned int)sym);
      }
      if (stbi_avif__dbg_sym_cnt == 6) {
         fprintf(stderr, "  ITER sym=%d cdf[%d]=%u icdf=%u v=%u u=%u r=%u c=%u\n",
            sym, sym, (unsigned)cdf[sym], 32768u-(unsigned)cdf[sym], v, u, r, c);
      }
   } while (c < v);

   r   = u - v;
   dif -= (unsigned long long)v << 48;
   ret = stbi_avif__av1_rd_normalize(rd, dif, r, (unsigned int)sym);
   if (stbi_avif__dbg_sym_cnt < stbi_avif__dbg_sym_max) {
      fprintf(stderr, "SYM[%d]: c=%u v=%u u=%u ret=%u rng=%u dif=%llu nsyms=%d cdf[0]=%u\n",
         stbi_avif__dbg_sym_cnt, c, v, u, ret, rd->rng, (unsigned long long)rd->dif, nsyms, (unsigned)cdf[0]);
      stbi_avif__dbg_sym_cnt++;
   }
   return ret;
}

/*
 * AV1 spec §8.2.7  update_cdf(cdf, symbol, nsyms)
 *
 * CDF arrays have `nsyms + 1` elements: indices 0..nsyms-1 are the cumulative
 * probabilities (in [1,32767] with cdf[nsyms-1] fixed at 32768), and index
 * nsyms stores the update count (starts at 0, used to compute the adaptation rate).
 *
 *   rate = 3 + (count > 15) + (nsyms > 2) + floor(log2(nsyms))
 *   For each i in [0, nsyms-1]:
 *     if i < symbol:  cdf[i] += (32768 - cdf[i]) >> rate
 *     else:           cdf[i] -= cdf[i] >> rate
 *   count = min(count + 1, 32)
 */
static void stbi_avif__av1_update_cdf(unsigned short *cdf, int symbol, int nsyms)
{
   int count, rate;
   int i;
   int rate_shift;

   count = (int)cdf[nsyms]; /* update counter stored in the extra slot */
   /* rate = 4 + (count >> 4) + (nsyms > 3)   [AOM spec] */
   rate = 4 + (count >> 4) + (nsyms > 3 ? 1 : 0);
   rate_shift = rate;

   for (i = 0; i < nsyms - 1; ++i)
   {
      if (i < symbol)
         cdf[i] -= (unsigned short)(cdf[i] >> rate_shift);
      else
         cdf[i] += (unsigned short)((32768u - cdf[i]) >> rate_shift);
   }

   if (count < 32)
      cdf[nsyms] = (unsigned short)(count + 1);
}

/*
 * Read a symbol using a MUTABLE CDF that is updated after each decode.
 * This is the adaptive version used for all main syntax elements.
 */
static unsigned int stbi_avif__av1_read_symbol_adapt(stbi_avif__av1_range_decoder *rd,
                                                       unsigned short *cdf, int nsyms)
{
   unsigned int sym = stbi_avif__av1_read_symbol(rd, cdf, nsyms);
   stbi_avif__av1_update_cdf(cdf, (int)sym, nsyms);
   return sym;
}

/* Read N literal bits from the range decoder (MSB first, each bit is 50/50) */
static unsigned int stbi_avif__av1_read_literal(stbi_avif__av1_range_decoder *rd,
                                                 unsigned int nbits)
{
   static const unsigned short half_cdf[3] = { 16384u, 32768u, 0u };
   unsigned int result = 0u;
   unsigned int i;
   for (i = 0; i < nbits; ++i) {
      result = (result << 1u) | stbi_avif__av1_read_symbol(rd, half_cdf, 2);
   }
   return result;
}

/* Read a uniform symbol in [0, n) from the range decoder */
static unsigned int stbi_avif__av1_read_uniform(stbi_avif__av1_range_decoder *rd,
                                                 unsigned int n)
{
   /* l = get_unsigned_bits(n) = floor(log2(n)) + 1 */
   unsigned int l = 0, m, v, t;
   t = n;
   while (t > 0u) { ++l; t >>= 1u; }
   m = (1u << l) - n;
   v = stbi_avif__av1_read_literal(rd, l - 1u);
   if (v < m)
      return v;
   return (v << 1u) - m + stbi_avif__av1_read_literal(rd, 1);
}

/*
 * =============================================================================
 *  DEFAULT CDF TABLES  (AV1 spec Appendix B)
 * =============================================================================
 *
 * Each CDF array has nsyms values + 1 count slot (initialised to 0).
 *
 * Partition CDFs: indexed by [bsize_ctx][neighbor_ctx] where:
 *   bsize_ctx in {0=128, 1=64, 2=32, 3=16, 4=8}
 *   neighbor_ctx in {0,1,2} (0=neither neighbour present, 1=one, 2=both)
 * For 4×4 blocks only 4 partition types exist (NONE/HORZ/VERT/SPLIT).
 *
 * We store only one representative context per block size (context 0) to keep
 * code size manageable.  Context index 0 is the "no prior data available" row
 * which is appropriate for a first-pass scaffold.
 *
 * Values from AV1 spec Table B.1.
 */

/* 10-symbol partition CDF for block sizes 128 down to 8 (one context per size) */
/* [bsize_ctx 0..4][10 probs + 1 count] */
static unsigned short stbi_avif__av1_partition_cdf[5][4][11] = {
   /* bsize_ctx 0 = 128×128: CDF8 (8 symbols), AOM bsl=4, flat indices 16-19 */
   {
      { 27899u, 28219u, 28529u, 32484u, 32539u, 32619u, 32639u, 32768u, 0, 0, 0 },
      {  6607u,  6990u,  8268u, 32060u, 32219u, 32338u, 32371u, 32768u, 0, 0, 0 },
      {  5429u,  6676u,  7122u, 32027u, 32227u, 32531u, 32582u, 32768u, 0, 0, 0 },
      {   711u,   966u,  1172u, 32448u, 32538u, 32617u, 32664u, 32768u, 0, 0, 0 }
   },
   /* bsize_ctx 1 = 64×64: CDF10 (10 symbols), AOM bsl=3, flat indices 12-15 */
   {
      { 20137u, 21547u, 23078u, 29566u, 29837u, 30261u, 30524u, 30892u, 31724u, 32768u, 0 },
      {  6732u,  7490u,  9497u, 27944u, 28250u, 28515u, 28969u, 29630u, 30104u, 32768u, 0 },
      {  5945u,  7663u,  8348u, 28683u, 29117u, 29749u, 30064u, 30298u, 32238u, 32768u, 0 },
      {   870u,  1212u,  1487u, 31198u, 31394u, 31574u, 31743u, 31881u, 32332u, 32768u, 0 }
   },
   /* bsize_ctx 2 = 32×32: CDF10 (10 symbols), AOM bsl=2, flat indices 8-11 */
   {
      { 18462u, 20920u, 23124u, 27647u, 28227u, 29049u, 29519u, 30178u, 31544u, 32768u, 0 },
      {  7689u,  9060u, 12056u, 24992u, 25660u, 26182u, 26951u, 28041u, 29052u, 32768u, 0 },
      {  6015u,  9009u, 10062u, 24544u, 25409u, 26545u, 27071u, 27526u, 32047u, 32768u, 0 },
      {  1394u,  2208u,  2796u, 28614u, 29061u, 29466u, 29840u, 30185u, 31899u, 32768u, 0 }
   },
   /* bsize_ctx 3 = 16×16: CDF10 (10 symbols), AOM bsl=1, flat indices 4-7 */
   {
      { 15597u, 20929u, 24571u, 26706u, 27664u, 28821u, 29601u, 30571u, 31902u, 32768u, 0 },
      {  7925u, 11043u, 16785u, 22470u, 23971u, 25043u, 26651u, 28701u, 29834u, 32768u, 0 },
      {  5414u, 13269u, 15111u, 20488u, 22360u, 24500u, 25537u, 26336u, 32117u, 32768u, 0 },
      {  2662u,  6362u,  8614u, 20860u, 23053u, 24778u, 26436u, 27829u, 31171u, 32768u, 0 }
   },
   /* bsize_ctx 4 = 8×8: CDF4 (4 symbols), AOM bsl=0, flat indices 0-3 */
   {
      { 19132u, 25510u, 30392u, 32768u, 0, 0, 0, 0, 0, 0, 0 },
      { 13928u, 19855u, 28540u, 32768u, 0, 0, 0, 0, 0, 0, 0 },
      { 12522u, 23679u, 28629u, 32768u, 0, 0, 0, 0, 0, 0, 0 },
      {  9896u, 18783u, 25853u, 32768u, 0, 0, 0, 0, 0, 0, 0 }
   }
};

/* intra Y mode CDF (13 symbols) — one context used for simplicity */
static unsigned short stbi_avif__av1_intra_mode_cdf[14] = {
    1535u,  8035u, 10156u, 11572u,
   13623u, 17080u, 19782u, 22089u,
   23988u, 25950u, 27015u, 28078u,
   32768u, 0
};

/* 4-way partition CDF for 4×4 blocks */
static unsigned short stbi_avif__av1_partition4_cdf[5] = {
   19132u, 25510u, 30392u, 32768u, 0
};

/*
 * =============================================================================
 *  INTRA-FRAME DECODE TABLES AND IMPLEMENTATION
 * =============================================================================
 */

/* AV1 intra mode context mapping (13 modes -> 5 ctx) */
static const unsigned char stbi_avif__av1_intra_mode_ctx[13] = {
   0, 1, 2, 3, 4, 4, 4, 4, 3, 0, 1, 2, 0
};

/* KF Y mode CDF [5 above_ctx][5 left_ctx][14] */
static unsigned short stbi_avif__av1_kf_y_mode_cdf[5][5][14] = {
   {
      {15588u, 17027u, 19338u, 20218u, 20682u, 21110u, 21825u, 23244u, 24189u, 28165u, 29093u, 30466u, 32768u, 0u},
      {12016u, 18066u, 19516u, 20303u, 20719u, 21444u, 21888u, 23032u, 24434u, 28658u, 30172u, 31409u, 32768u, 0u},
      {10052u, 10771u, 22296u, 22788u, 23055u, 23239u, 24133u, 25620u, 26160u, 29336u, 29929u, 31567u, 32768u, 0u},
      {14091u, 15406u, 16442u, 18808u, 19136u, 19546u, 19998u, 22096u, 24746u, 29585u, 30958u, 32462u, 32768u, 0u},
      {12122u, 13265u, 15603u, 16501u, 18609u, 20033u, 22391u, 25583u, 26437u, 30261u, 31073u, 32475u, 32768u, 0u},
   },
   {
      {10023u, 19585u, 20848u, 21440u, 21832u, 22760u, 23089u, 24023u, 25381u, 29014u, 30482u, 31436u, 32768u, 0u},
      {5983u, 24099u, 24560u, 24886u, 25066u, 25795u, 25913u, 26423u, 27610u, 29905u, 31276u, 31794u, 32768u, 0u},
      {7444u, 12781u, 20177u, 20728u, 21077u, 21607u, 22170u, 23405u, 24469u, 27915u, 29090u, 30492u, 32768u, 0u},
      {8537u, 14689u, 15432u, 17087u, 17408u, 18172u, 18408u, 19825u, 24649u, 29153u, 31096u, 32210u, 32768u, 0u},
      {7543u, 14231u, 15496u, 16195u, 17905u, 20717u, 21984u, 24516u, 26001u, 29675u, 30981u, 31994u, 32768u, 0u},
   },
   {
      {12613u, 13591u, 21383u, 22004u, 22312u, 22577u, 23401u, 25055u, 25729u, 29538u, 30305u, 32077u, 32768u, 0u},
      {9687u, 13470u, 18506u, 19230u, 19604u, 20147u, 20695u, 22062u, 23219u, 27743u, 29211u, 30907u, 32768u, 0u},
      {6183u, 6505u, 26024u, 26252u, 26366u, 26434u, 27082u, 28354u, 28555u, 30467u, 30794u, 32086u, 32768u, 0u},
      {10718u, 11734u, 14954u, 17224u, 17565u, 17924u, 18561u, 21523u, 23878u, 28975u, 30287u, 32252u, 32768u, 0u},
      {9194u, 9858u, 16501u, 17263u, 18424u, 19171u, 21563u, 25961u, 26561u, 30072u, 30737u, 32463u, 32768u, 0u},
   },
   {
      {12602u, 14399u, 15488u, 18381u, 18778u, 19315u, 19724u, 21419u, 25060u, 29696u, 30917u, 32409u, 32768u, 0u},
      {8203u, 13821u, 14524u, 17105u, 17439u, 18131u, 18404u, 19468u, 25225u, 29485u, 31158u, 32342u, 32768u, 0u},
      {8451u, 9731u, 15004u, 17643u, 18012u, 18425u, 19070u, 21538u, 24605u, 29118u, 30078u, 32018u, 32768u, 0u},
      {7714u, 9048u, 9516u, 16667u, 16817u, 16994u, 17153u, 18767u, 26743u, 30389u, 31536u, 32528u, 32768u, 0u},
      {8843u, 10280u, 11496u, 15317u, 16652u, 17943u, 19108u, 22718u, 25769u, 29953u, 30983u, 32485u, 32768u, 0u},
   },
   {
      {12578u, 13671u, 15979u, 16834u, 19075u, 20913u, 22989u, 25449u, 26219u, 30214u, 31150u, 32477u, 32768u, 0u},
      {9563u, 13626u, 15080u, 15892u, 17756u, 20863u, 22207u, 24236u, 25380u, 29653u, 31143u, 32277u, 32768u, 0u},
      {8356u, 8901u, 17616u, 18256u, 19350u, 20106u, 22598u, 25947u, 26466u, 29900u, 30523u, 32261u, 32768u, 0u},
      {10835u, 11815u, 13124u, 16042u, 17018u, 18039u, 18947u, 22753u, 24615u, 29489u, 30883u, 32482u, 32768u, 0u},
      {7618u, 8288u, 9859u, 10509u, 15386u, 18657u, 22903u, 28776u, 29180u, 31355u, 31802u, 32593u, 32768u, 0u},
   },
};

/* UV mode CDF: no-CFL [13 y_mode][14], CFL [13 y_mode][15] */
static unsigned short stbi_avif__av1_uv_mode_cdf_no_cfl[13][14] = {
   {22631u, 24152u, 25378u, 25661u, 25986u, 26520u, 27055u, 27923u, 28244u, 30059u, 30941u, 31961u, 32768u, 0u},
   {9513u, 26881u, 26973u, 27046u, 27118u, 27664u, 27739u, 27824u, 28359u, 29505u, 29800u, 31796u, 32768u, 0u},
   {9845u, 9915u, 28663u, 28704u, 28757u, 28780u, 29198u, 29822u, 29854u, 30764u, 31777u, 32029u, 32768u, 0u},
   {13639u, 13897u, 14171u, 25331u, 25606u, 25727u, 25953u, 27148u, 28577u, 30612u, 31355u, 32493u, 32768u, 0u},
   {9764u, 9835u, 9930u, 9954u, 25386u, 27053u, 27958u, 28148u, 28243u, 31101u, 31744u, 32363u, 32768u, 0u},
   {11825u, 13589u, 13677u, 13720u, 15048u, 29213u, 29301u, 29458u, 29711u, 31161u, 31441u, 32550u, 32768u, 0u},
   {14175u, 14399u, 16608u, 16821u, 17718u, 17775u, 28551u, 30200u, 30245u, 31837u, 32342u, 32667u, 32768u, 0u},
   {12885u, 13038u, 14978u, 15590u, 15673u, 15748u, 16176u, 29128u, 29267u, 30643u, 31961u, 32461u, 32768u, 0u},
   {12026u, 13661u, 13874u, 15305u, 15490u, 15726u, 15995u, 16273u, 28443u, 30388u, 30767u, 32416u, 32768u, 0u},
   {19052u, 19840u, 20579u, 20916u, 21150u, 21467u, 21885u, 22719u, 23174u, 28861u, 30379u, 32175u, 32768u, 0u},
   {18627u, 19649u, 20974u, 21219u, 21492u, 21816u, 22199u, 23119u, 23527u, 27053u, 31397u, 32148u, 32768u, 0u},
   {17026u, 19004u, 19997u, 20339u, 20586u, 21103u, 21349u, 21907u, 22482u, 25896u, 26541u, 31819u, 32768u, 0u},
   {12124u, 13759u, 14959u, 14992u, 15007u, 15051u, 15078u, 15166u, 15255u, 15753u, 16039u, 16606u, 32768u, 0u},
};
static unsigned short stbi_avif__av1_uv_mode_cdf_cfl[13][15] = {
   {10407u, 11208u, 12900u, 13181u, 13823u, 14175u, 14899u, 15656u, 15986u, 20086u, 20995u, 22455u, 24212u, 32768u, 0u},
   {4532u, 19780u, 20057u, 20215u, 20428u, 21071u, 21199u, 21451u, 22099u, 24228u, 24693u, 27032u, 29472u, 32768u, 0u},
   {5273u, 5379u, 20177u, 20270u, 20385u, 20439u, 20949u, 21695u, 21774u, 23138u, 24256u, 24703u, 26679u, 32768u, 0u},
   {6740u, 7167u, 7662u, 14152u, 14536u, 14785u, 15034u, 16741u, 18371u, 21520u, 22206u, 23389u, 24182u, 32768u, 0u},
   {4987u, 5368u, 5928u, 6068u, 19114u, 20315u, 21857u, 22253u, 22411u, 24911u, 25380u, 26027u, 26376u, 32768u, 0u},
   {5370u, 6889u, 7247u, 7393u, 9498u, 21114u, 21402u, 21753u, 21981u, 24780u, 25386u, 26517u, 27176u, 32768u, 0u},
   {4816u, 4961u, 7204u, 7326u, 8765u, 8930u, 20169u, 20682u, 20803u, 23188u, 23763u, 24455u, 24940u, 32768u, 0u},
   {6608u, 6740u, 8529u, 9049u, 9257u, 9356u, 9735u, 18827u, 19059u, 22336u, 23204u, 23964u, 24793u, 32768u, 0u},
   {5998u, 7419u, 7781u, 8933u, 9255u, 9549u, 9753u, 10417u, 18898u, 22494u, 23139u, 24764u, 25989u, 32768u, 0u},
   {10660u, 11298u, 12550u, 12957u, 13322u, 13624u, 14040u, 15004u, 15534u, 20714u, 21789u, 23443u, 24861u, 32768u, 0u},
   {10522u, 11530u, 12552u, 12963u, 13378u, 13779u, 14245u, 15235u, 15902u, 20102u, 22696u, 23774u, 25838u, 32768u, 0u},
   {10099u, 10691u, 12639u, 13049u, 13386u, 13665u, 14125u, 15163u, 15636u, 19676u, 20474u, 23519u, 25208u, 32768u, 0u},
   {3144u, 5087u, 7382u, 7504u, 7593u, 7690u, 7801u, 8064u, 8232u, 9248u, 9875u, 10521u, 29048u, 32768u, 0u},
};

/* Angle delta CDF [8 directional modes][8] (7 symbols + count) */
static unsigned short stbi_avif__av1_angle_delta_cdf[8][8] = {
   {2180u, 5032u, 7567u, 22776u, 26989u, 30217u, 32768u, 0u},
   {2301u, 5608u, 8801u, 23487u, 26974u, 30330u, 32768u, 0u},
   {3780u, 11018u, 13699u, 19354u, 23083u, 31286u, 32768u, 0u},
   {4581u, 11226u, 15147u, 17138u, 21834u, 28397u, 32768u, 0u},
   {1737u, 10927u, 14509u, 19588u, 22745u, 28823u, 32768u, 0u},
   {2664u, 10176u, 12485u, 17650u, 21600u, 30495u, 32768u, 0u},
   {2240u, 11096u, 15453u, 20341u, 22561u, 28917u, 32768u, 0u},
   {3605u, 10428u, 12459u, 17676u, 21244u, 30655u, 32768u, 0u},
};

/* Intra TX type CDF: 780 total values */
/* Intra TX type CDF set 1: [4 tx_sizes][13 modes][8] (7 symbols) */
static unsigned short stbi_avif__av1_intra_tx_cdf_set1[4][13][8] = {
   {
      {1535u, 8035u, 9461u, 12751u, 23467u, 27825u, 32768u, 0u},
      {564u, 3335u, 9709u, 10870u, 18143u, 28094u, 32768u, 0u},
      {672u, 3247u, 3676u, 11982u, 19415u, 23127u, 32768u, 0u},
      {5279u, 13885u, 15487u, 18044u, 23527u, 30252u, 32768u, 0u},
      {4423u, 6074u, 7985u, 10416u, 25693u, 29298u, 32768u, 0u},
      {1486u, 4241u, 9460u, 10662u, 16456u, 27694u, 32768u, 0u},
      {439u, 2838u, 3522u, 6737u, 18058u, 23754u, 32768u, 0u},
      {1190u, 4233u, 4855u, 11670u, 20281u, 24377u, 32768u, 0u},
      {1045u, 4312u, 8647u, 10159u, 18644u, 29335u, 32768u, 0u},
      {202u, 3734u, 4747u, 7298u, 17127u, 24016u, 32768u, 0u},
      {447u, 4312u, 6819u, 8884u, 16010u, 23858u, 32768u, 0u},
      {277u, 4369u, 5255u, 8905u, 16465u, 22271u, 32768u, 0u},
      {3409u, 5436u, 10599u, 15599u, 19687u, 24040u, 32768u, 0u},
   },
   {
      {1870u, 13742u, 14530u, 16498u, 23770u, 27698u, 32768u, 0u},
      {326u, 8796u, 14632u, 15079u, 19272u, 27486u, 32768u, 0u},
      {484u, 7576u, 7712u, 14443u, 19159u, 22591u, 32768u, 0u},
      {1126u, 15340u, 15895u, 17023u, 20896u, 30279u, 32768u, 0u},
      {655u, 4854u, 5249u, 5913u, 22099u, 27138u, 32768u, 0u},
      {1299u, 6458u, 8885u, 9290u, 14851u, 25497u, 32768u, 0u},
      {311u, 5295u, 5552u, 6885u, 16107u, 22672u, 32768u, 0u},
      {883u, 8059u, 8270u, 11258u, 17289u, 21549u, 32768u, 0u},
      {741u, 7580u, 9318u, 10345u, 16688u, 29046u, 32768u, 0u},
      {110u, 7406u, 7915u, 9195u, 16041u, 23329u, 32768u, 0u},
      {363u, 7974u, 9357u, 10673u, 15629u, 24474u, 32768u, 0u},
      {153u, 7647u, 8112u, 9936u, 15307u, 19996u, 32768u, 0u},
      {3511u, 6332u, 11165u, 15335u, 19323u, 23594u, 32768u, 0u},
   },
   {
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
   },
   {
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
      {4681u, 9362u, 14043u, 18725u, 23406u, 28087u, 32768u, 0u},
   },
};

/* Intra TX type CDF set 2: [4 tx_sizes][13 modes][6] (5 symbols) */
static unsigned short stbi_avif__av1_intra_tx_cdf_set2[4][13][6] = {
   {
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
   },
   {
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
   },
   {
      {1127u, 12814u, 22772u, 27483u, 32768u, 0u},
      {145u, 6761u, 11980u, 26667u, 32768u, 0u},
      {362u, 5887u, 11678u, 16725u, 32768u, 0u},
      {385u, 15213u, 18587u, 30693u, 32768u, 0u},
      {25u, 2914u, 23134u, 27903u, 32768u, 0u},
      {60u, 4470u, 11749u, 23991u, 32768u, 0u},
      {37u, 3332u, 14511u, 21448u, 32768u, 0u},
      {157u, 6320u, 13036u, 17439u, 32768u, 0u},
      {119u, 6719u, 12906u, 29396u, 32768u, 0u},
      {47u, 5537u, 12576u, 21499u, 32768u, 0u},
      {269u, 6076u, 11258u, 23115u, 32768u, 0u},
      {83u, 5615u, 12001u, 17228u, 32768u, 0u},
      {1968u, 5556u, 12023u, 18547u, 32768u, 0u},
   },
   {
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
      {6554u, 13107u, 19661u, 26214u, 32768u, 0u},
   },
};

/* DC sign CDF [4 Q_CTX][2 plane][3 ctx][3] */
static unsigned short stbi_avif__av1_dc_sign_cdf[4][2][3][3] = {
   {
      {
         {16000u, 32768u, 0u},
         {13056u, 32768u, 0u},
         {18816u, 32768u, 0u},
      },
      {
         {15232u, 32768u, 0u},
         {12928u, 32768u, 0u},
         {17280u, 32768u, 0u},
      },
   },
   {
      {
         {16000u, 32768u, 0u},
         {13056u, 32768u, 0u},
         {18816u, 32768u, 0u},
      },
      {
         {15232u, 32768u, 0u},
         {12928u, 32768u, 0u},
         {17280u, 32768u, 0u},
      },
   },
   {
      {
         {16000u, 32768u, 0u},
         {13056u, 32768u, 0u},
         {18816u, 32768u, 0u},
      },
      {
         {15232u, 32768u, 0u},
         {12928u, 32768u, 0u},
         {17280u, 32768u, 0u},
      },
   },
   {
      {
         {16000u, 32768u, 0u},
         {13056u, 32768u, 0u},
         {18816u, 32768u, 0u},
      },
      {
         {15232u, 32768u, 0u},
         {12928u, 32768u, 0u},
         {17280u, 32768u, 0u},
      },
   },
};

/* TXB skip CDF [4 Q_CTX][5 TX_SIZES][13 ctx][3] */
static unsigned short stbi_avif__av1_txb_skip_cdf[4][5][13][3] = {
   {
      {
         {31849u, 32768u, 0u},
         {5892u, 32768u, 0u},
         {12112u, 32768u, 0u},
         {21935u, 32768u, 0u},
         {20289u, 32768u, 0u},
         {27473u, 32768u, 0u},
         {32487u, 32768u, 0u},
         {7654u, 32768u, 0u},
         {19473u, 32768u, 0u},
         {29984u, 32768u, 0u},
         {9961u, 32768u, 0u},
         {30242u, 32768u, 0u},
         {32117u, 32768u, 0u},
      },
      {
         {31548u, 32768u, 0u},
         {1549u, 32768u, 0u},
         {10130u, 32768u, 0u},
         {16656u, 32768u, 0u},
         {18591u, 32768u, 0u},
         {26308u, 32768u, 0u},
         {32537u, 32768u, 0u},
         {5403u, 32768u, 0u},
         {18096u, 32768u, 0u},
         {30003u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {29957u, 32768u, 0u},
         {5391u, 32768u, 0u},
         {18039u, 32768u, 0u},
         {23566u, 32768u, 0u},
         {22431u, 32768u, 0u},
         {25822u, 32768u, 0u},
         {32197u, 32768u, 0u},
         {3778u, 32768u, 0u},
         {15336u, 32768u, 0u},
         {28981u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {17920u, 32768u, 0u},
         {1818u, 32768u, 0u},
         {7282u, 32768u, 0u},
         {25273u, 32768u, 0u},
         {10923u, 32768u, 0u},
         {31554u, 32768u, 0u},
         {32624u, 32768u, 0u},
         {1366u, 32768u, 0u},
         {15628u, 32768u, 0u},
         {30462u, 32768u, 0u},
         {146u, 32768u, 0u},
         {5132u, 32768u, 0u},
         {31657u, 32768u, 0u},
      },
      {
         {6308u, 32768u, 0u},
         {117u, 32768u, 0u},
         {1638u, 32768u, 0u},
         {2161u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {10923u, 32768u, 0u},
         {30247u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
   },
   {
      {
         {30371u, 32768u, 0u},
         {7570u, 32768u, 0u},
         {13155u, 32768u, 0u},
         {20751u, 32768u, 0u},
         {20969u, 32768u, 0u},
         {27067u, 32768u, 0u},
         {32013u, 32768u, 0u},
         {5495u, 32768u, 0u},
         {17942u, 32768u, 0u},
         {28280u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {31782u, 32768u, 0u},
         {1836u, 32768u, 0u},
         {10689u, 32768u, 0u},
         {17604u, 32768u, 0u},
         {21622u, 32768u, 0u},
         {27518u, 32768u, 0u},
         {32399u, 32768u, 0u},
         {4419u, 32768u, 0u},
         {16294u, 32768u, 0u},
         {28345u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {31901u, 32768u, 0u},
         {10311u, 32768u, 0u},
         {18047u, 32768u, 0u},
         {24806u, 32768u, 0u},
         {23288u, 32768u, 0u},
         {27914u, 32768u, 0u},
         {32296u, 32768u, 0u},
         {4215u, 32768u, 0u},
         {15756u, 32768u, 0u},
         {28341u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {26726u, 32768u, 0u},
         {1045u, 32768u, 0u},
         {11703u, 32768u, 0u},
         {20590u, 32768u, 0u},
         {18554u, 32768u, 0u},
         {25970u, 32768u, 0u},
         {31938u, 32768u, 0u},
         {5583u, 32768u, 0u},
         {21313u, 32768u, 0u},
         {29390u, 32768u, 0u},
         {641u, 32768u, 0u},
         {22265u, 32768u, 0u},
         {31452u, 32768u, 0u},
      },
      {
         {26584u, 32768u, 0u},
         {188u, 32768u, 0u},
         {8847u, 32768u, 0u},
         {24519u, 32768u, 0u},
         {22938u, 32768u, 0u},
         {30583u, 32768u, 0u},
         {32608u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
   },
   {
      {
         {29614u, 32768u, 0u},
         {9068u, 32768u, 0u},
         {12924u, 32768u, 0u},
         {19538u, 32768u, 0u},
         {17737u, 32768u, 0u},
         {24619u, 32768u, 0u},
         {30642u, 32768u, 0u},
         {4119u, 32768u, 0u},
         {16026u, 32768u, 0u},
         {25657u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {31957u, 32768u, 0u},
         {3230u, 32768u, 0u},
         {11153u, 32768u, 0u},
         {18123u, 32768u, 0u},
         {20143u, 32768u, 0u},
         {26536u, 32768u, 0u},
         {31986u, 32768u, 0u},
         {3050u, 32768u, 0u},
         {14603u, 32768u, 0u},
         {25155u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {32363u, 32768u, 0u},
         {10692u, 32768u, 0u},
         {19090u, 32768u, 0u},
         {24357u, 32768u, 0u},
         {24442u, 32768u, 0u},
         {28312u, 32768u, 0u},
         {32169u, 32768u, 0u},
         {3648u, 32768u, 0u},
         {15690u, 32768u, 0u},
         {26815u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {30669u, 32768u, 0u},
         {3832u, 32768u, 0u},
         {11663u, 32768u, 0u},
         {18889u, 32768u, 0u},
         {19782u, 32768u, 0u},
         {23313u, 32768u, 0u},
         {31330u, 32768u, 0u},
         {5124u, 32768u, 0u},
         {18719u, 32768u, 0u},
         {28468u, 32768u, 0u},
         {3082u, 32768u, 0u},
         {20982u, 32768u, 0u},
         {29443u, 32768u, 0u},
      },
      {
         {28573u, 32768u, 0u},
         {3183u, 32768u, 0u},
         {17802u, 32768u, 0u},
         {25977u, 32768u, 0u},
         {26677u, 32768u, 0u},
         {27832u, 32768u, 0u},
         {32387u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
   },
   {
      {
         {26887u, 32768u, 0u},
         {6729u, 32768u, 0u},
         {10361u, 32768u, 0u},
         {17442u, 32768u, 0u},
         {15045u, 32768u, 0u},
         {22478u, 32768u, 0u},
         {29072u, 32768u, 0u},
         {2713u, 32768u, 0u},
         {11861u, 32768u, 0u},
         {20773u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {31903u, 32768u, 0u},
         {2044u, 32768u, 0u},
         {7528u, 32768u, 0u},
         {14618u, 32768u, 0u},
         {16182u, 32768u, 0u},
         {24168u, 32768u, 0u},
         {31037u, 32768u, 0u},
         {2786u, 32768u, 0u},
         {11194u, 32768u, 0u},
         {20155u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {32510u, 32768u, 0u},
         {8430u, 32768u, 0u},
         {17318u, 32768u, 0u},
         {24154u, 32768u, 0u},
         {23674u, 32768u, 0u},
         {28789u, 32768u, 0u},
         {32139u, 32768u, 0u},
         {3440u, 32768u, 0u},
         {13117u, 32768u, 0u},
         {22702u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
      {
         {31671u, 32768u, 0u},
         {2056u, 32768u, 0u},
         {11746u, 32768u, 0u},
         {16852u, 32768u, 0u},
         {18635u, 32768u, 0u},
         {24715u, 32768u, 0u},
         {31484u, 32768u, 0u},
         {4656u, 32768u, 0u},
         {16074u, 32768u, 0u},
         {24704u, 32768u, 0u},
         {1806u, 32768u, 0u},
         {14645u, 32768u, 0u},
         {25336u, 32768u, 0u},
      },
      {
         {31539u, 32768u, 0u},
         {8433u, 32768u, 0u},
         {20576u, 32768u, 0u},
         {27904u, 32768u, 0u},
         {27852u, 32768u, 0u},
         {30026u, 32768u, 0u},
         {32441u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
         {16384u, 32768u, 0u},
      },
   },
};

/* EOB extra CDF [4 Q_CTX][5 TX_SIZES][2 plane][9 ctx][3] */
static unsigned short stbi_avif__av1_eob_extra_cdf[4][5][2][9][3] = {
   {
      {
         {
            {16961u, 32768u, 0u},
            {17223u, 32768u, 0u},
            {7621u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {19069u, 32768u, 0u},
            {22525u, 32768u, 0u},
            {13377u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {20401u, 32768u, 0u},
            {17025u, 32768u, 0u},
            {12845u, 32768u, 0u},
            {12873u, 32768u, 0u},
            {14094u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {20681u, 32768u, 0u},
            {20701u, 32768u, 0u},
            {15250u, 32768u, 0u},
            {15017u, 32768u, 0u},
            {14928u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {23905u, 32768u, 0u},
            {17194u, 32768u, 0u},
            {16170u, 32768u, 0u},
            {17695u, 32768u, 0u},
            {13826u, 32768u, 0u},
            {15810u, 32768u, 0u},
            {12036u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {23959u, 32768u, 0u},
            {20799u, 32768u, 0u},
            {19021u, 32768u, 0u},
            {16203u, 32768u, 0u},
            {17886u, 32768u, 0u},
            {14144u, 32768u, 0u},
            {12010u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {27399u, 32768u, 0u},
            {16327u, 32768u, 0u},
            {18071u, 32768u, 0u},
            {19584u, 32768u, 0u},
            {20721u, 32768u, 0u},
            {18432u, 32768u, 0u},
            {19560u, 32768u, 0u},
            {10150u, 32768u, 0u},
            {8805u, 32768u, 0u},
         },
         {
            {24932u, 32768u, 0u},
            {20833u, 32768u, 0u},
            {12027u, 32768u, 0u},
            {16670u, 32768u, 0u},
            {19914u, 32768u, 0u},
            {15106u, 32768u, 0u},
            {17662u, 32768u, 0u},
            {13783u, 32768u, 0u},
            {28756u, 32768u, 0u},
         },
      },
      {
         {
            {23406u, 32768u, 0u},
            {21845u, 32768u, 0u},
            {18432u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {17096u, 32768u, 0u},
            {12561u, 32768u, 0u},
            {17320u, 32768u, 0u},
            {22395u, 32768u, 0u},
            {21370u, 32768u, 0u},
         },
         {
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {17471u, 32768u, 0u},
            {20223u, 32768u, 0u},
            {11357u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {20335u, 32768u, 0u},
            {21667u, 32768u, 0u},
            {14818u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {20430u, 32768u, 0u},
            {20662u, 32768u, 0u},
            {15367u, 32768u, 0u},
            {16970u, 32768u, 0u},
            {14657u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {22117u, 32768u, 0u},
            {22028u, 32768u, 0u},
            {18650u, 32768u, 0u},
            {16042u, 32768u, 0u},
            {15885u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {22409u, 32768u, 0u},
            {21012u, 32768u, 0u},
            {15650u, 32768u, 0u},
            {17395u, 32768u, 0u},
            {15469u, 32768u, 0u},
            {20205u, 32768u, 0u},
            {19511u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {24220u, 32768u, 0u},
            {22480u, 32768u, 0u},
            {17737u, 32768u, 0u},
            {18916u, 32768u, 0u},
            {19268u, 32768u, 0u},
            {18412u, 32768u, 0u},
            {18844u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {25991u, 32768u, 0u},
            {20314u, 32768u, 0u},
            {17731u, 32768u, 0u},
            {19678u, 32768u, 0u},
            {18649u, 32768u, 0u},
            {17307u, 32768u, 0u},
            {21798u, 32768u, 0u},
            {17549u, 32768u, 0u},
            {15630u, 32768u, 0u},
         },
         {
            {26585u, 32768u, 0u},
            {21469u, 32768u, 0u},
            {20432u, 32768u, 0u},
            {17735u, 32768u, 0u},
            {19280u, 32768u, 0u},
            {15235u, 32768u, 0u},
            {20297u, 32768u, 0u},
            {22471u, 32768u, 0u},
            {28997u, 32768u, 0u},
         },
      },
      {
         {
            {26605u, 32768u, 0u},
            {11304u, 32768u, 0u},
            {16726u, 32768u, 0u},
            {16560u, 32768u, 0u},
            {20866u, 32768u, 0u},
            {23524u, 32768u, 0u},
            {19878u, 32768u, 0u},
            {13469u, 32768u, 0u},
            {23084u, 32768u, 0u},
         },
         {
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {18983u, 32768u, 0u},
            {20512u, 32768u, 0u},
            {14885u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {20090u, 32768u, 0u},
            {19444u, 32768u, 0u},
            {17286u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {19139u, 32768u, 0u},
            {21487u, 32768u, 0u},
            {18959u, 32768u, 0u},
            {20910u, 32768u, 0u},
            {19089u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {20536u, 32768u, 0u},
            {20664u, 32768u, 0u},
            {20625u, 32768u, 0u},
            {19123u, 32768u, 0u},
            {14862u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {19833u, 32768u, 0u},
            {21502u, 32768u, 0u},
            {17485u, 32768u, 0u},
            {20267u, 32768u, 0u},
            {18353u, 32768u, 0u},
            {23329u, 32768u, 0u},
            {21478u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {22041u, 32768u, 0u},
            {23434u, 32768u, 0u},
            {20001u, 32768u, 0u},
            {20554u, 32768u, 0u},
            {20951u, 32768u, 0u},
            {20145u, 32768u, 0u},
            {15562u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {23312u, 32768u, 0u},
            {21607u, 32768u, 0u},
            {16526u, 32768u, 0u},
            {18957u, 32768u, 0u},
            {18034u, 32768u, 0u},
            {18934u, 32768u, 0u},
            {24247u, 32768u, 0u},
            {16921u, 32768u, 0u},
            {17080u, 32768u, 0u},
         },
         {
            {26579u, 32768u, 0u},
            {24910u, 32768u, 0u},
            {18637u, 32768u, 0u},
            {19800u, 32768u, 0u},
            {20388u, 32768u, 0u},
            {9887u, 32768u, 0u},
            {15642u, 32768u, 0u},
            {30198u, 32768u, 0u},
            {24721u, 32768u, 0u},
         },
      },
      {
         {
            {26998u, 32768u, 0u},
            {16737u, 32768u, 0u},
            {17838u, 32768u, 0u},
            {18922u, 32768u, 0u},
            {19515u, 32768u, 0u},
            {18636u, 32768u, 0u},
            {17333u, 32768u, 0u},
            {15776u, 32768u, 0u},
            {22658u, 32768u, 0u},
         },
         {
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {20177u, 32768u, 0u},
            {20789u, 32768u, 0u},
            {20262u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {21416u, 32768u, 0u},
            {20855u, 32768u, 0u},
            {23410u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {20238u, 32768u, 0u},
            {21057u, 32768u, 0u},
            {19159u, 32768u, 0u},
            {22337u, 32768u, 0u},
            {20159u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {20125u, 32768u, 0u},
            {20559u, 32768u, 0u},
            {21707u, 32768u, 0u},
            {22296u, 32768u, 0u},
            {17333u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {19941u, 32768u, 0u},
            {20527u, 32768u, 0u},
            {21470u, 32768u, 0u},
            {22487u, 32768u, 0u},
            {19558u, 32768u, 0u},
            {22354u, 32768u, 0u},
            {20331u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
         {
            {22752u, 32768u, 0u},
            {25006u, 32768u, 0u},
            {22075u, 32768u, 0u},
            {21576u, 32768u, 0u},
            {17740u, 32768u, 0u},
            {21690u, 32768u, 0u},
            {19211u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
      {
         {
            {21442u, 32768u, 0u},
            {22358u, 32768u, 0u},
            {18503u, 32768u, 0u},
            {20291u, 32768u, 0u},
            {19945u, 32768u, 0u},
            {21294u, 32768u, 0u},
            {21178u, 32768u, 0u},
            {19400u, 32768u, 0u},
            {10556u, 32768u, 0u},
         },
         {
            {24648u, 32768u, 0u},
            {24949u, 32768u, 0u},
            {20708u, 32768u, 0u},
            {23905u, 32768u, 0u},
            {20501u, 32768u, 0u},
            {9558u, 32768u, 0u},
            {9423u, 32768u, 0u},
            {30365u, 32768u, 0u},
            {19253u, 32768u, 0u},
         },
      },
      {
         {
            {26064u, 32768u, 0u},
            {22098u, 32768u, 0u},
            {19613u, 32768u, 0u},
            {20525u, 32768u, 0u},
            {17595u, 32768u, 0u},
            {16618u, 32768u, 0u},
            {20497u, 32768u, 0u},
            {18989u, 32768u, 0u},
            {15513u, 32768u, 0u},
         },
         {
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
            {16384u, 32768u, 0u},
         },
      },
   },
};

/* EOB multi16 CDF [4 Q_CTX][2 plane][2 ctx][6] */
static unsigned short stbi_avif__av1_eob_multi16_cdf[4][2][2][6] = {
   {
      {
         {840u, 1039u, 1980u, 4895u, 32768u, 0u},
         {370u, 671u, 1883u, 4471u, 32768u, 0u},
      },
      {
         {3247u, 4950u, 9688u, 14563u, 32768u, 0u},
         {1904u, 3354u, 7763u, 14647u, 32768u, 0u},
      },
   },
   {
      {
         {2125u, 2551u, 5165u, 8946u, 32768u, 0u},
         {513u, 765u, 1859u, 6339u, 32768u, 0u},
      },
      {
         {7637u, 9498u, 14259u, 19108u, 32768u, 0u},
         {2497u, 4096u, 8866u, 16993u, 32768u, 0u},
      },
   },
   {
      {
         {4016u, 4897u, 8881u, 14968u, 32768u, 0u},
         {716u, 1105u, 2646u, 10056u, 32768u, 0u},
      },
      {
         {11139u, 13270u, 18241u, 23566u, 32768u, 0u},
         {3192u, 5032u, 10297u, 19755u, 32768u, 0u},
      },
   },
   {
      {
         {6708u, 8958u, 14746u, 22133u, 32768u, 0u},
         {1222u, 2074u, 4783u, 15410u, 32768u, 0u},
      },
      {
         {19575u, 21766u, 26044u, 29709u, 32768u, 0u},
         {7297u, 10767u, 19273u, 28194u, 32768u, 0u},
      },
   },
};

/* EOB multi32 CDF [4 Q_CTX][2 plane][2 ctx][7] */
static unsigned short stbi_avif__av1_eob_multi32_cdf[4][2][2][7] = {
   {
      {
         {400u, 520u, 977u, 2102u, 6542u, 32768u, 0u},
         {210u, 405u, 1315u, 3326u, 7537u, 32768u, 0u},
      },
      {
         {2636u, 4273u, 7588u, 11794u, 20401u, 32768u, 0u},
         {1786u, 3179u, 6902u, 11357u, 19054u, 32768u, 0u},
      },
   },
   {
      {
         {989u, 1249u, 2019u, 4151u, 10785u, 32768u, 0u},
         {313u, 441u, 1099u, 2917u, 8562u, 32768u, 0u},
      },
      {
         {8394u, 10352u, 13932u, 18855u, 26014u, 32768u, 0u},
         {2578u, 4124u, 8181u, 13670u, 24234u, 32768u, 0u},
      },
   },
   {
      {
         {2515u, 3003u, 4452u, 8162u, 16041u, 32768u, 0u},
         {574u, 821u, 1836u, 5089u, 13128u, 32768u, 0u},
      },
      {
         {13468u, 16303u, 20361u, 25105u, 29281u, 32768u, 0u},
         {3542u, 5502u, 10415u, 16760u, 25644u, 32768u, 0u},
      },
   },
   {
      {
         {4617u, 5709u, 8446u, 13584u, 23135u, 32768u, 0u},
         {1156u, 1702u, 3675u, 9274u, 20539u, 32768u, 0u},
      },
      {
         {22086u, 24282u, 27010u, 29770u, 31743u, 32768u, 0u},
         {7699u, 10897u, 20891u, 26926u, 31628u, 32768u, 0u},
      },
   },
};

/* EOB multi64 CDF [4 Q_CTX][2 plane][2 ctx][8] */
static unsigned short stbi_avif__av1_eob_multi64_cdf[4][2][2][8] = {
   {
      {
         {329u, 498u, 1101u, 1784u, 3265u, 7758u, 32768u, 0u},
         {335u, 730u, 1459u, 5494u, 8755u, 12997u, 32768u, 0u},
      },
      {
         {3505u, 5304u, 10086u, 13814u, 17684u, 23370u, 32768u, 0u},
         {1563u, 2700u, 4876u, 10911u, 14706u, 22480u, 32768u, 0u},
      },
   },
   {
      {
         {1260u, 1446u, 2253u, 3712u, 6652u, 13369u, 32768u, 0u},
         {401u, 605u, 1029u, 2563u, 5845u, 12626u, 32768u, 0u},
      },
      {
         {8609u, 10612u, 14624u, 18714u, 22614u, 29024u, 32768u, 0u},
         {1923u, 3127u, 5867u, 9703u, 14277u, 27100u, 32768u, 0u},
      },
   },
   {
      {
         {2374u, 2772u, 4583u, 7276u, 12288u, 19706u, 32768u, 0u},
         {497u, 810u, 1315u, 3000u, 7004u, 15641u, 32768u, 0u},
      },
      {
         {15050u, 17126u, 21410u, 24886u, 28156u, 30726u, 32768u, 0u},
         {4034u, 6290u, 10235u, 14982u, 21214u, 28491u, 32768u, 0u},
      },
   },
   {
      {
         {6307u, 7541u, 12060u, 16358u, 22553u, 27865u, 32768u, 0u},
         {1289u, 2320u, 3971u, 7926u, 14153u, 24291u, 32768u, 0u},
      },
      {
         {24212u, 25708u, 28268u, 30035u, 31307u, 32049u, 32768u, 0u},
         {8726u, 12378u, 19409u, 26450u, 30038u, 32462u, 32768u, 0u},
      },
   },
};

/* EOB multi128 CDF [4 Q_CTX][2 plane][2 ctx][9] */
static unsigned short stbi_avif__av1_eob_multi128_cdf[4][2][2][9] = {
   {
      {
         {219u, 482u, 1140u, 2091u, 3680u, 6028u, 12586u, 32768u, 0u},
         {371u, 699u, 1254u, 4830u, 9479u, 12562u, 17497u, 32768u, 0u},
      },
      {
         {5245u, 7456u, 12880u, 15852u, 20033u, 23932u, 27608u, 32768u, 0u},
         {2054u, 3472u, 5869u, 14232u, 18242u, 20590u, 26752u, 32768u, 0u},
      },
   },
   {
      {
         {685u, 933u, 1488u, 2714u, 4766u, 8562u, 19254u, 32768u, 0u},
         {217u, 352u, 618u, 2303u, 5261u, 9969u, 17472u, 32768u, 0u},
      },
      {
         {8045u, 11200u, 15497u, 19595u, 23948u, 27408u, 30938u, 32768u, 0u},
         {2310u, 4160u, 7471u, 14997u, 17931u, 20768u, 30240u, 32768u, 0u},
      },
   },
   {
      {
         {1366u, 1738u, 2527u, 5016u, 9355u, 15797u, 24643u, 32768u, 0u},
         {354u, 558u, 944u, 2760u, 7287u, 14037u, 21779u, 32768u, 0u},
      },
      {
         {13627u, 16246u, 20173u, 24429u, 27948u, 30415u, 31863u, 32768u, 0u},
         {6275u, 9889u, 14769u, 23164u, 27988u, 30493u, 32272u, 32768u, 0u},
      },
   },
   {
      {
         {3472u, 4885u, 7489u, 12481u, 18517u, 24536u, 29635u, 32768u, 0u},
         {886u, 1731u, 3271u, 8469u, 15569u, 22126u, 28383u, 32768u, 0u},
      },
      {
         {24313u, 26062u, 28385u, 30107u, 31217u, 31898u, 32345u, 32768u, 0u},
         {9165u, 13282u, 21150u, 30286u, 31894u, 32571u, 32712u, 32768u, 0u},
      },
   },
};

/* EOB multi256 CDF [4 Q_CTX][2 plane][2 ctx][10] */
static unsigned short stbi_avif__av1_eob_multi256_cdf[4][2][2][10] = {
   {
      {
         {310u, 584u, 1887u, 3589u, 6168u, 8611u, 11352u, 15652u, 32768u, 0u},
         {998u, 1850u, 2998u, 5604u, 17341u, 19888u, 22899u, 25583u, 32768u, 0u},
      },
      {
         {2520u, 3240u, 5952u, 8870u, 12577u, 17558u, 19954u, 24168u, 32768u, 0u},
         {2203u, 4130u, 7435u, 10739u, 20652u, 23681u, 25609u, 27261u, 32768u, 0u},
      },
   },
   {
      {
         {1448u, 2109u, 4151u, 6263u, 9329u, 13260u, 17944u, 23300u, 32768u, 0u},
         {399u, 1019u, 1749u, 3038u, 10444u, 15546u, 22739u, 27294u, 32768u, 0u},
      },
      {
         {6402u, 8148u, 12623u, 15072u, 18728u, 22847u, 26447u, 29377u, 32768u, 0u},
         {1674u, 3252u, 5734u, 10159u, 22397u, 23802u, 24821u, 30940u, 32768u, 0u},
      },
   },
   {
      {
         {3089u, 3920u, 6038u, 9460u, 14266u, 19881u, 25766u, 29176u, 32768u, 0u},
         {1084u, 2358u, 3488u, 5122u, 11483u, 18103u, 26023u, 29799u, 32768u, 0u},
      },
      {
         {11514u, 13794u, 17480u, 20754u, 24361u, 27378u, 29492u, 31277u, 32768u, 0u},
         {6571u, 9610u, 15516u, 21826u, 29092u, 30829u, 31842u, 32708u, 32768u, 0u},
      },
   },
   {
      {
         {5348u, 7113u, 11820u, 15924u, 22106u, 26777u, 30334u, 31757u, 32768u, 0u},
         {2453u, 4474u, 6307u, 8777u, 16474u, 22975u, 29000u, 31547u, 32768u, 0u},
      },
      {
         {23110u, 24597u, 27140u, 28894u, 30167u, 30927u, 31392u, 32094u, 32768u, 0u},
         {9998u, 17661u, 25178u, 28097u, 31308u, 32038u, 32403u, 32695u, 32768u, 0u},
      },
   },
};

/* EOB multi512 CDF [4 Q_CTX][2 plane][2 ctx][11] */
static unsigned short stbi_avif__av1_eob_multi512_cdf[4][2][2][11] = {
   {
      {
         {641u, 983u, 3707u, 5430u, 10234u, 14958u, 18788u, 23412u, 26061u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
      {
         {5095u, 6446u, 9996u, 13354u, 16017u, 17986u, 20919u, 26129u, 29140u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
   },
   {
      {
         {1230u, 2278u, 5035u, 7776u, 11871u, 15346u, 19590u, 24584u, 28749u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
      {
         {7265u, 9979u, 15819u, 19250u, 21780u, 23846u, 26478u, 28396u, 31811u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
   },
   {
      {
         {2624u, 3936u, 6480u, 9686u, 13979u, 17726u, 23267u, 28410u, 31078u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
      {
         {12015u, 14769u, 19588u, 22052u, 24222u, 25812u, 27300u, 29219u, 32114u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
   },
   {
      {
         {5927u, 7809u, 10923u, 14597u, 19439u, 24135u, 28456u, 31142u, 32060u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
      {
         {21093u, 23043u, 25742u, 27658u, 29097u, 29716u, 30073u, 30820u, 31956u, 32768u, 0u},
         {3277u, 6554u, 9830u, 13107u, 16384u, 19661u, 22938u, 26214u, 29491u, 32768u, 0u},
      },
   },
};

/* EOB multi1024 CDF [4 Q_CTX][2 plane][2 ctx][12] */
static unsigned short stbi_avif__av1_eob_multi1024_cdf[4][2][2][12] = {
   {
      {
         {393u, 421u, 751u, 1623u, 3160u, 6352u, 13345u, 18047u, 22571u, 25830u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
      {
         {1865u, 1988u, 2930u, 4242u, 10533u, 16538u, 21354u, 27255u, 28546u, 31784u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
   },
   {
      {
         {696u, 948u, 3145u, 5702u, 9706u, 13217u, 17851u, 21856u, 25692u, 28034u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
      {
         {2672u, 3591u, 9330u, 17084u, 22725u, 24284u, 26527u, 28027u, 28377u, 30876u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
   },
   {
      {
         {2784u, 3831u, 7041u, 10521u, 14847u, 18844u, 23155u, 26682u, 29229u, 31045u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
      {
         {9577u, 12466u, 17739u, 20750u, 22061u, 23215u, 24601u, 25483u, 25843u, 32056u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
   },
   {
      {
         {6698u, 8334u, 11961u, 15762u, 20186u, 23862u, 27434u, 29326u, 31082u, 32050u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
      {
         {20569u, 22426u, 25569u, 26859u, 28053u, 28913u, 29486u, 29724u, 29807u, 32570u, 32768u, 0u},
         {2979u, 5958u, 8937u, 11916u, 14895u, 17873u, 20852u, 23831u, 26810u, 29789u, 32768u, 0u},
      },
   },
};

/* Coeff base EOB CDF [4 Q_CTX][5 TX_SIZES][2 plane][4 ctx][4] */
static unsigned short stbi_avif__av1_coeff_base_eob_cdf[4][5][2][4][4] = {
   {
      {
         {
            {17837u, 29055u, 32768u, 0u},
            {29600u, 31446u, 32768u, 0u},
            {30844u, 31878u, 32768u, 0u},
            {24926u, 28948u, 32768u, 0u},
         },
         {
            {21365u, 30026u, 32768u, 0u},
            {30512u, 32423u, 32768u, 0u},
            {31658u, 32621u, 32768u, 0u},
            {29630u, 31881u, 32768u, 0u},
         },
      },
      {
         {
            {5717u, 26477u, 32768u, 0u},
            {30491u, 31703u, 32768u, 0u},
            {31550u, 32158u, 32768u, 0u},
            {29648u, 31491u, 32768u, 0u},
         },
         {
            {12608u, 27820u, 32768u, 0u},
            {30680u, 32225u, 32768u, 0u},
            {30809u, 32335u, 32768u, 0u},
            {31299u, 32423u, 32768u, 0u},
         },
      },
      {
         {
            {1786u, 12612u, 32768u, 0u},
            {30663u, 31625u, 32768u, 0u},
            {32339u, 32468u, 32768u, 0u},
            {31148u, 31833u, 32768u, 0u},
         },
         {
            {18857u, 23865u, 32768u, 0u},
            {31428u, 32428u, 32768u, 0u},
            {31744u, 32373u, 32768u, 0u},
            {31775u, 32526u, 32768u, 0u},
         },
      },
      {
         {
            {1787u, 2532u, 32768u, 0u},
            {30832u, 31662u, 32768u, 0u},
            {31824u, 32682u, 32768u, 0u},
            {32133u, 32569u, 32768u, 0u},
         },
         {
            {13751u, 22235u, 32768u, 0u},
            {32089u, 32409u, 32768u, 0u},
            {27084u, 27920u, 32768u, 0u},
            {29291u, 32594u, 32768u, 0u},
         },
      },
      {
         {
            {1725u, 3449u, 32768u, 0u},
            {31102u, 31935u, 32768u, 0u},
            {32457u, 32613u, 32768u, 0u},
            {32412u, 32649u, 32768u, 0u},
         },
         {
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {17560u, 29888u, 32768u, 0u},
            {29671u, 31549u, 32768u, 0u},
            {31007u, 32056u, 32768u, 0u},
            {27286u, 30006u, 32768u, 0u},
         },
         {
            {26594u, 31212u, 32768u, 0u},
            {31208u, 32582u, 32768u, 0u},
            {31835u, 32637u, 32768u, 0u},
            {30595u, 32206u, 32768u, 0u},
         },
      },
      {
         {
            {15239u, 29932u, 32768u, 0u},
            {31315u, 32095u, 32768u, 0u},
            {32130u, 32434u, 32768u, 0u},
            {30864u, 31996u, 32768u, 0u},
         },
         {
            {26279u, 30968u, 32768u, 0u},
            {31142u, 32495u, 32768u, 0u},
            {31713u, 32540u, 32768u, 0u},
            {31929u, 32594u, 32768u, 0u},
         },
      },
      {
         {
            {2644u, 25198u, 32768u, 0u},
            {32038u, 32451u, 32768u, 0u},
            {32639u, 32695u, 32768u, 0u},
            {32166u, 32518u, 32768u, 0u},
         },
         {
            {17187u, 27668u, 32768u, 0u},
            {31714u, 32550u, 32768u, 0u},
            {32283u, 32678u, 32768u, 0u},
            {31930u, 32563u, 32768u, 0u},
         },
      },
      {
         {
            {1044u, 2257u, 32768u, 0u},
            {30755u, 31923u, 32768u, 0u},
            {32208u, 32693u, 32768u, 0u},
            {32244u, 32615u, 32768u, 0u},
         },
         {
            {21317u, 26207u, 32768u, 0u},
            {29133u, 30868u, 32768u, 0u},
            {29311u, 31231u, 32768u, 0u},
            {29657u, 31087u, 32768u, 0u},
         },
      },
      {
         {
            {478u, 1834u, 32768u, 0u},
            {31005u, 31987u, 32768u, 0u},
            {32317u, 32724u, 32768u, 0u},
            {30865u, 32648u, 32768u, 0u},
         },
         {
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {20092u, 30774u, 32768u, 0u},
            {30695u, 32020u, 32768u, 0u},
            {31131u, 32103u, 32768u, 0u},
            {28666u, 30870u, 32768u, 0u},
         },
         {
            {27258u, 31095u, 32768u, 0u},
            {31804u, 32623u, 32768u, 0u},
            {31763u, 32528u, 32768u, 0u},
            {31438u, 32506u, 32768u, 0u},
         },
      },
      {
         {
            {18049u, 30489u, 32768u, 0u},
            {31706u, 32286u, 32768u, 0u},
            {32163u, 32473u, 32768u, 0u},
            {31550u, 32184u, 32768u, 0u},
         },
         {
            {27116u, 30842u, 32768u, 0u},
            {31971u, 32598u, 32768u, 0u},
            {32088u, 32576u, 32768u, 0u},
            {32067u, 32664u, 32768u, 0u},
         },
      },
      {
         {
            {12854u, 29093u, 32768u, 0u},
            {32272u, 32558u, 32768u, 0u},
            {32667u, 32729u, 32768u, 0u},
            {32306u, 32585u, 32768u, 0u},
         },
         {
            {25476u, 30366u, 32768u, 0u},
            {32169u, 32687u, 32768u, 0u},
            {32479u, 32689u, 32768u, 0u},
            {31673u, 32634u, 32768u, 0u},
         },
      },
      {
         {
            {2809u, 19301u, 32768u, 0u},
            {32205u, 32622u, 32768u, 0u},
            {32338u, 32730u, 32768u, 0u},
            {31786u, 32616u, 32768u, 0u},
         },
         {
            {22737u, 29105u, 32768u, 0u},
            {30810u, 32362u, 32768u, 0u},
            {30014u, 32627u, 32768u, 0u},
            {30528u, 32574u, 32768u, 0u},
         },
      },
      {
         {
            {935u, 3382u, 32768u, 0u},
            {30789u, 31909u, 32768u, 0u},
            {32466u, 32756u, 32768u, 0u},
            {30860u, 32513u, 32768u, 0u},
         },
         {
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {22497u, 31198u, 32768u, 0u},
            {31715u, 32495u, 32768u, 0u},
            {31606u, 32337u, 32768u, 0u},
            {30388u, 31990u, 32768u, 0u},
         },
         {
            {27877u, 31584u, 32768u, 0u},
            {32170u, 32728u, 32768u, 0u},
            {32155u, 32688u, 32768u, 0u},
            {32219u, 32702u, 32768u, 0u},
         },
      },
      {
         {
            {21457u, 31043u, 32768u, 0u},
            {31951u, 32483u, 32768u, 0u},
            {32153u, 32562u, 32768u, 0u},
            {31473u, 32215u, 32768u, 0u},
         },
         {
            {27558u, 31151u, 32768u, 0u},
            {32020u, 32640u, 32768u, 0u},
            {32097u, 32575u, 32768u, 0u},
            {32242u, 32719u, 32768u, 0u},
         },
      },
      {
         {
            {19980u, 30591u, 32768u, 0u},
            {32219u, 32597u, 32768u, 0u},
            {32581u, 32706u, 32768u, 0u},
            {31803u, 32287u, 32768u, 0u},
         },
         {
            {26473u, 30507u, 32768u, 0u},
            {32431u, 32723u, 32768u, 0u},
            {32196u, 32611u, 32768u, 0u},
            {31588u, 32528u, 32768u, 0u},
         },
      },
      {
         {
            {24647u, 30463u, 32768u, 0u},
            {32412u, 32695u, 32768u, 0u},
            {32468u, 32720u, 32768u, 0u},
            {31269u, 32523u, 32768u, 0u},
         },
         {
            {28482u, 31505u, 32768u, 0u},
            {32152u, 32701u, 32768u, 0u},
            {31732u, 32598u, 32768u, 0u},
            {31767u, 32712u, 32768u, 0u},
         },
      },
      {
         {
            {12358u, 24977u, 32768u, 0u},
            {31331u, 32385u, 32768u, 0u},
            {32634u, 32756u, 32768u, 0u},
            {30411u, 32548u, 32768u, 0u},
         },
         {
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
            {10923u, 21845u, 32768u, 0u},
         },
      },
   },
};

/* Coeff base CDF [4 Q_CTX][5 TX_SIZES][2 plane][42 ctx][5] (8400 values) */
static unsigned short stbi_avif__av1_coeff_base_cdf[4][5][2][42][5] = {
   {
      {
         {
            {4034u, 8930u, 12727u, 32768u, 0u},
            {18082u, 29741u, 31877u, 32768u, 0u},
            {12596u, 26124u, 30493u, 32768u, 0u},
            {9446u, 21118u, 27005u, 32768u, 0u},
            {6308u, 15141u, 21279u, 32768u, 0u},
            {2463u, 6357u, 9783u, 32768u, 0u},
            {20667u, 30546u, 31929u, 32768u, 0u},
            {13043u, 26123u, 30134u, 32768u, 0u},
            {8151u, 18757u, 24778u, 32768u, 0u},
            {5255u, 12839u, 18632u, 32768u, 0u},
            {2820u, 7206u, 11161u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {15736u, 27553u, 30604u, 32768u, 0u},
            {11210u, 23794u, 28787u, 32768u, 0u},
            {5947u, 13874u, 19701u, 32768u, 0u},
            {4215u, 9323u, 13891u, 32768u, 0u},
            {2833u, 6462u, 10059u, 32768u, 0u},
            {19605u, 30393u, 31582u, 32768u, 0u},
            {13523u, 26252u, 30248u, 32768u, 0u},
            {8446u, 18622u, 24512u, 32768u, 0u},
            {3818u, 10343u, 15974u, 32768u, 0u},
            {1481u, 4117u, 6796u, 32768u, 0u},
            {22649u, 31302u, 32190u, 32768u, 0u},
            {14829u, 27127u, 30449u, 32768u, 0u},
            {8313u, 17702u, 23304u, 32768u, 0u},
            {3022u, 8301u, 12786u, 32768u, 0u},
            {1536u, 4412u, 7184u, 32768u, 0u},
            {22354u, 29774u, 31372u, 32768u, 0u},
            {14723u, 25472u, 29214u, 32768u, 0u},
            {6673u, 13745u, 18662u, 32768u, 0u},
            {2068u, 5766u, 9322u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {6302u, 16444u, 21761u, 32768u, 0u},
            {23040u, 31538u, 32475u, 32768u, 0u},
            {15196u, 28452u, 31496u, 32768u, 0u},
            {10020u, 22946u, 28514u, 32768u, 0u},
            {6533u, 16862u, 23501u, 32768u, 0u},
            {3538u, 9816u, 15076u, 32768u, 0u},
            {24444u, 31875u, 32525u, 32768u, 0u},
            {15881u, 28924u, 31635u, 32768u, 0u},
            {9922u, 22873u, 28466u, 32768u, 0u},
            {6527u, 16966u, 23691u, 32768u, 0u},
            {4114u, 11303u, 17220u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {20201u, 30770u, 32209u, 32768u, 0u},
            {14754u, 28071u, 31258u, 32768u, 0u},
            {8378u, 20186u, 26517u, 32768u, 0u},
            {5916u, 15299u, 21978u, 32768u, 0u},
            {4268u, 11583u, 17901u, 32768u, 0u},
            {24361u, 32025u, 32581u, 32768u, 0u},
            {18673u, 30105u, 31943u, 32768u, 0u},
            {10196u, 22244u, 27576u, 32768u, 0u},
            {5495u, 14349u, 20417u, 32768u, 0u},
            {2676u, 7415u, 11498u, 32768u, 0u},
            {24678u, 31958u, 32585u, 32768u, 0u},
            {18629u, 29906u, 31831u, 32768u, 0u},
            {9364u, 20724u, 26315u, 32768u, 0u},
            {4641u, 12318u, 18094u, 32768u, 0u},
            {2758u, 7387u, 11579u, 32768u, 0u},
            {25433u, 31842u, 32469u, 32768u, 0u},
            {18795u, 29289u, 31411u, 32768u, 0u},
            {7644u, 17584u, 23592u, 32768u, 0u},
            {3408u, 9014u, 15047u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {4536u, 10072u, 14001u, 32768u, 0u},
            {25459u, 31416u, 32206u, 32768u, 0u},
            {16605u, 28048u, 30818u, 32768u, 0u},
            {11008u, 22857u, 27719u, 32768u, 0u},
            {6915u, 16268u, 22315u, 32768u, 0u},
            {2625u, 6812u, 10537u, 32768u, 0u},
            {24257u, 31788u, 32499u, 32768u, 0u},
            {16880u, 29454u, 31879u, 32768u, 0u},
            {11958u, 25054u, 29778u, 32768u, 0u},
            {7916u, 18718u, 25084u, 32768u, 0u},
            {3383u, 8777u, 13446u, 32768u, 0u},
            {22720u, 31603u, 32393u, 32768u, 0u},
            {14960u, 28125u, 31335u, 32768u, 0u},
            {9731u, 22210u, 27928u, 32768u, 0u},
            {6304u, 15832u, 22277u, 32768u, 0u},
            {2910u, 7818u, 12166u, 32768u, 0u},
            {20375u, 30627u, 32131u, 32768u, 0u},
            {13904u, 27284u, 30887u, 32768u, 0u},
            {9368u, 21558u, 27144u, 32768u, 0u},
            {5937u, 14966u, 21119u, 32768u, 0u},
            {2667u, 7225u, 11319u, 32768u, 0u},
            {23970u, 31470u, 32378u, 32768u, 0u},
            {17173u, 29734u, 32018u, 32768u, 0u},
            {12795u, 25441u, 29965u, 32768u, 0u},
            {8981u, 19680u, 25893u, 32768u, 0u},
            {4728u, 11372u, 16902u, 32768u, 0u},
            {24287u, 31797u, 32439u, 32768u, 0u},
            {16703u, 29145u, 31696u, 32768u, 0u},
            {10833u, 23554u, 28725u, 32768u, 0u},
            {6468u, 16566u, 23057u, 32768u, 0u},
            {2415u, 6562u, 10278u, 32768u, 0u},
            {26610u, 32395u, 32659u, 32768u, 0u},
            {18590u, 30498u, 32117u, 32768u, 0u},
            {12420u, 25756u, 29950u, 32768u, 0u},
            {7639u, 18746u, 24710u, 32768u, 0u},
            {3001u, 8086u, 12347u, 32768u, 0u},
            {25076u, 32064u, 32580u, 32768u, 0u},
            {17946u, 30128u, 32028u, 32768u, 0u},
            {12024u, 24985u, 29378u, 32768u, 0u},
            {7517u, 18390u, 24304u, 32768u, 0u},
            {3243u, 8781u, 13331u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {6037u, 16771u, 21957u, 32768u, 0u},
            {24774u, 31704u, 32426u, 32768u, 0u},
            {16830u, 28589u, 31056u, 32768u, 0u},
            {10602u, 22828u, 27760u, 32768u, 0u},
            {6733u, 16829u, 23071u, 32768u, 0u},
            {3250u, 8914u, 13556u, 32768u, 0u},
            {25582u, 32220u, 32668u, 32768u, 0u},
            {18659u, 30342u, 32223u, 32768u, 0u},
            {12546u, 26149u, 30515u, 32768u, 0u},
            {8420u, 20451u, 26801u, 32768u, 0u},
            {4636u, 12420u, 18344u, 32768u, 0u},
            {27581u, 32362u, 32639u, 32768u, 0u},
            {18987u, 30083u, 31978u, 32768u, 0u},
            {11327u, 24248u, 29084u, 32768u, 0u},
            {7264u, 17719u, 24120u, 32768u, 0u},
            {3995u, 10768u, 16169u, 32768u, 0u},
            {25893u, 31831u, 32487u, 32768u, 0u},
            {16577u, 28587u, 31379u, 32768u, 0u},
            {10189u, 22748u, 28182u, 32768u, 0u},
            {6832u, 17094u, 23556u, 32768u, 0u},
            {3708u, 10110u, 15334u, 32768u, 0u},
            {25904u, 32282u, 32656u, 32768u, 0u},
            {19721u, 30792u, 32276u, 32768u, 0u},
            {12819u, 26243u, 30411u, 32768u, 0u},
            {8572u, 20614u, 26891u, 32768u, 0u},
            {5364u, 14059u, 20467u, 32768u, 0u},
            {26580u, 32438u, 32677u, 32768u, 0u},
            {20852u, 31225u, 32340u, 32768u, 0u},
            {12435u, 25700u, 29967u, 32768u, 0u},
            {8691u, 20825u, 26976u, 32768u, 0u},
            {4446u, 12209u, 17269u, 32768u, 0u},
            {27350u, 32429u, 32696u, 32768u, 0u},
            {21372u, 30977u, 32272u, 32768u, 0u},
            {12673u, 25270u, 29853u, 32768u, 0u},
            {9208u, 20925u, 26640u, 32768u, 0u},
            {5018u, 13351u, 18732u, 32768u, 0u},
            {27351u, 32479u, 32713u, 32768u, 0u},
            {21398u, 31209u, 32387u, 32768u, 0u},
            {12162u, 25047u, 29842u, 32768u, 0u},
            {7896u, 18691u, 25319u, 32768u, 0u},
            {4670u, 12882u, 18881u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {5487u, 10460u, 13708u, 32768u, 0u},
            {21597u, 28303u, 30674u, 32768u, 0u},
            {11037u, 21953u, 26476u, 32768u, 0u},
            {8147u, 17962u, 22952u, 32768u, 0u},
            {5242u, 13061u, 18532u, 32768u, 0u},
            {1889u, 5208u, 8182u, 32768u, 0u},
            {26774u, 32133u, 32590u, 32768u, 0u},
            {17844u, 29564u, 31767u, 32768u, 0u},
            {11690u, 24438u, 29171u, 32768u, 0u},
            {7542u, 18215u, 24459u, 32768u, 0u},
            {2993u, 8050u, 12319u, 32768u, 0u},
            {28023u, 32328u, 32591u, 32768u, 0u},
            {18651u, 30126u, 31954u, 32768u, 0u},
            {12164u, 25146u, 29589u, 32768u, 0u},
            {7762u, 18530u, 24771u, 32768u, 0u},
            {3492u, 9183u, 13920u, 32768u, 0u},
            {27591u, 32008u, 32491u, 32768u, 0u},
            {17149u, 28853u, 31510u, 32768u, 0u},
            {11485u, 24003u, 28860u, 32768u, 0u},
            {7697u, 18086u, 24210u, 32768u, 0u},
            {3075u, 7999u, 12218u, 32768u, 0u},
            {28268u, 32482u, 32654u, 32768u, 0u},
            {19631u, 31051u, 32404u, 32768u, 0u},
            {13860u, 27260u, 31020u, 32768u, 0u},
            {9605u, 21613u, 27594u, 32768u, 0u},
            {4876u, 12162u, 17908u, 32768u, 0u},
            {27248u, 32316u, 32576u, 32768u, 0u},
            {18955u, 30457u, 32075u, 32768u, 0u},
            {11824u, 23997u, 28795u, 32768u, 0u},
            {7346u, 18196u, 24647u, 32768u, 0u},
            {3403u, 9247u, 14111u, 32768u, 0u},
            {29711u, 32655u, 32735u, 32768u, 0u},
            {21169u, 31394u, 32417u, 32768u, 0u},
            {13487u, 27198u, 30957u, 32768u, 0u},
            {8828u, 21683u, 27614u, 32768u, 0u},
            {4270u, 11451u, 17038u, 32768u, 0u},
            {28708u, 32578u, 32731u, 32768u, 0u},
            {20120u, 31241u, 32482u, 32768u, 0u},
            {13692u, 27550u, 31321u, 32768u, 0u},
            {9418u, 22514u, 28439u, 32768u, 0u},
            {4999u, 13283u, 19462u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {5673u, 14302u, 19711u, 32768u, 0u},
            {26251u, 30701u, 31834u, 32768u, 0u},
            {12782u, 23783u, 27803u, 32768u, 0u},
            {9127u, 20657u, 25808u, 32768u, 0u},
            {6368u, 16208u, 21462u, 32768u, 0u},
            {2465u, 7177u, 10822u, 32768u, 0u},
            {29961u, 32563u, 32719u, 32768u, 0u},
            {18318u, 29891u, 31949u, 32768u, 0u},
            {11361u, 24514u, 29357u, 32768u, 0u},
            {7900u, 19603u, 25607u, 32768u, 0u},
            {4002u, 10590u, 15546u, 32768u, 0u},
            {29637u, 32310u, 32595u, 32768u, 0u},
            {18296u, 29913u, 31809u, 32768u, 0u},
            {10144u, 21515u, 26871u, 32768u, 0u},
            {5358u, 14322u, 20394u, 32768u, 0u},
            {3067u, 8362u, 13346u, 32768u, 0u},
            {28652u, 32470u, 32676u, 32768u, 0u},
            {17538u, 30771u, 32209u, 32768u, 0u},
            {13924u, 26882u, 30494u, 32768u, 0u},
            {10496u, 22837u, 27869u, 32768u, 0u},
            {7236u, 16396u, 21621u, 32768u, 0u},
            {30743u, 32687u, 32746u, 32768u, 0u},
            {23006u, 31676u, 32489u, 32768u, 0u},
            {14494u, 27828u, 31120u, 32768u, 0u},
            {10174u, 22801u, 28352u, 32768u, 0u},
            {6242u, 15281u, 21043u, 32768u, 0u},
            {25817u, 32243u, 32720u, 32768u, 0u},
            {18618u, 31367u, 32325u, 32768u, 0u},
            {13997u, 28318u, 31878u, 32768u, 0u},
            {12255u, 26534u, 31383u, 32768u, 0u},
            {9561u, 21588u, 28450u, 32768u, 0u},
            {28188u, 32635u, 32724u, 32768u, 0u},
            {22060u, 32365u, 32728u, 32768u, 0u},
            {18102u, 30690u, 32528u, 32768u, 0u},
            {14196u, 28864u, 31999u, 32768u, 0u},
            {12262u, 25792u, 30865u, 32768u, 0u},
            {24176u, 32109u, 32628u, 32768u, 0u},
            {18280u, 29681u, 31963u, 32768u, 0u},
            {10205u, 23703u, 29664u, 32768u, 0u},
            {7889u, 20025u, 27676u, 32768u, 0u},
            {6060u, 16743u, 23970u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {5141u, 7096u, 8260u, 32768u, 0u},
            {27186u, 29022u, 29789u, 32768u, 0u},
            {6668u, 12568u, 15682u, 32768u, 0u},
            {2172u, 6181u, 8638u, 32768u, 0u},
            {1126u, 3379u, 4531u, 32768u, 0u},
            {443u, 1361u, 2254u, 32768u, 0u},
            {26083u, 31153u, 32436u, 32768u, 0u},
            {13486u, 24603u, 28483u, 32768u, 0u},
            {6508u, 14840u, 19910u, 32768u, 0u},
            {3386u, 8800u, 13286u, 32768u, 0u},
            {1530u, 4322u, 7054u, 32768u, 0u},
            {29639u, 32080u, 32548u, 32768u, 0u},
            {15897u, 27552u, 30290u, 32768u, 0u},
            {8588u, 20047u, 25383u, 32768u, 0u},
            {4889u, 13339u, 19269u, 32768u, 0u},
            {2240u, 6871u, 10498u, 32768u, 0u},
            {28165u, 32197u, 32517u, 32768u, 0u},
            {20735u, 30427u, 31568u, 32768u, 0u},
            {14325u, 24671u, 27692u, 32768u, 0u},
            {5119u, 12554u, 17805u, 32768u, 0u},
            {1810u, 5441u, 8261u, 32768u, 0u},
            {31212u, 32724u, 32748u, 32768u, 0u},
            {23352u, 31766u, 32545u, 32768u, 0u},
            {14669u, 27570u, 31059u, 32768u, 0u},
            {8492u, 20894u, 27272u, 32768u, 0u},
            {3644u, 10194u, 15204u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {2461u, 7013u, 9371u, 32768u, 0u},
            {24749u, 29600u, 30986u, 32768u, 0u},
            {9466u, 19037u, 22417u, 32768u, 0u},
            {3584u, 9280u, 14400u, 32768u, 0u},
            {1505u, 3929u, 5433u, 32768u, 0u},
            {677u, 1500u, 2736u, 32768u, 0u},
            {23987u, 30702u, 32117u, 32768u, 0u},
            {13554u, 24571u, 29263u, 32768u, 0u},
            {6211u, 14556u, 21155u, 32768u, 0u},
            {3135u, 10972u, 15625u, 32768u, 0u},
            {2435u, 7127u, 11427u, 32768u, 0u},
            {31300u, 32532u, 32550u, 32768u, 0u},
            {14757u, 30365u, 31954u, 32768u, 0u},
            {4405u, 11612u, 18553u, 32768u, 0u},
            {580u, 4132u, 7322u, 32768u, 0u},
            {1695u, 10169u, 14124u, 32768u, 0u},
            {30008u, 32282u, 32591u, 32768u, 0u},
            {19244u, 30108u, 31748u, 32768u, 0u},
            {11180u, 24158u, 29555u, 32768u, 0u},
            {5650u, 14972u, 19209u, 32768u, 0u},
            {2114u, 5109u, 8456u, 32768u, 0u},
            {31856u, 32716u, 32748u, 32768u, 0u},
            {23012u, 31664u, 32572u, 32768u, 0u},
            {13694u, 26656u, 30636u, 32768u, 0u},
            {8142u, 19508u, 26093u, 32768u, 0u},
            {4253u, 10955u, 16724u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {601u, 983u, 1311u, 32768u, 0u},
            {18725u, 23406u, 28087u, 32768u, 0u},
            {5461u, 8192u, 10923u, 32768u, 0u},
            {3781u, 15124u, 21425u, 32768u, 0u},
            {2587u, 7761u, 12072u, 32768u, 0u},
            {106u, 458u, 810u, 32768u, 0u},
            {22282u, 29710u, 31894u, 32768u, 0u},
            {8508u, 20926u, 25984u, 32768u, 0u},
            {3726u, 12713u, 18083u, 32768u, 0u},
            {1620u, 7112u, 10893u, 32768u, 0u},
            {729u, 2236u, 3495u, 32768u, 0u},
            {30163u, 32474u, 32684u, 32768u, 0u},
            {18304u, 30464u, 32000u, 32768u, 0u},
            {11443u, 26526u, 29647u, 32768u, 0u},
            {6007u, 15292u, 21299u, 32768u, 0u},
            {2234u, 6703u, 8937u, 32768u, 0u},
            {30954u, 32177u, 32571u, 32768u, 0u},
            {17363u, 29562u, 31076u, 32768u, 0u},
            {9686u, 22464u, 27410u, 32768u, 0u},
            {8192u, 16384u, 21390u, 32768u, 0u},
            {1755u, 8046u, 11264u, 32768u, 0u},
            {31168u, 32734u, 32748u, 32768u, 0u},
            {22486u, 31441u, 32471u, 32768u, 0u},
            {12833u, 25627u, 29738u, 32768u, 0u},
            {6980u, 17379u, 23122u, 32768u, 0u},
            {3111u, 8887u, 13479u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {6041u, 11854u, 15927u, 32768u, 0u},
            {20326u, 30905u, 32251u, 32768u, 0u},
            {14164u, 26831u, 30725u, 32768u, 0u},
            {9760u, 20647u, 26585u, 32768u, 0u},
            {6416u, 14953u, 21219u, 32768u, 0u},
            {2966u, 7151u, 10891u, 32768u, 0u},
            {23567u, 31374u, 32254u, 32768u, 0u},
            {14978u, 27416u, 30946u, 32768u, 0u},
            {9434u, 20225u, 26254u, 32768u, 0u},
            {6658u, 14558u, 20535u, 32768u, 0u},
            {3916u, 8677u, 12989u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {18088u, 29545u, 31587u, 32768u, 0u},
            {13062u, 25843u, 30073u, 32768u, 0u},
            {8940u, 16827u, 22251u, 32768u, 0u},
            {7654u, 13220u, 17973u, 32768u, 0u},
            {5733u, 10316u, 14456u, 32768u, 0u},
            {22879u, 31388u, 32114u, 32768u, 0u},
            {15215u, 27993u, 30955u, 32768u, 0u},
            {9397u, 19445u, 24978u, 32768u, 0u},
            {3442u, 9813u, 15344u, 32768u, 0u},
            {1368u, 3936u, 6532u, 32768u, 0u},
            {25494u, 32033u, 32406u, 32768u, 0u},
            {16772u, 27963u, 30718u, 32768u, 0u},
            {9419u, 18165u, 23260u, 32768u, 0u},
            {2677u, 7501u, 11797u, 32768u, 0u},
            {1516u, 4344u, 7170u, 32768u, 0u},
            {26556u, 31454u, 32101u, 32768u, 0u},
            {17128u, 27035u, 30108u, 32768u, 0u},
            {8324u, 15344u, 20249u, 32768u, 0u},
            {1903u, 5696u, 9469u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8455u, 19003u, 24368u, 32768u, 0u},
            {23563u, 32021u, 32604u, 32768u, 0u},
            {16237u, 29446u, 31935u, 32768u, 0u},
            {10724u, 23999u, 29358u, 32768u, 0u},
            {6725u, 17528u, 24416u, 32768u, 0u},
            {3927u, 10927u, 16825u, 32768u, 0u},
            {26313u, 32288u, 32634u, 32768u, 0u},
            {17430u, 30095u, 32095u, 32768u, 0u},
            {11116u, 24606u, 29679u, 32768u, 0u},
            {7195u, 18384u, 25269u, 32768u, 0u},
            {4726u, 12852u, 19315u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {22822u, 31648u, 32483u, 32768u, 0u},
            {16724u, 29633u, 31929u, 32768u, 0u},
            {10261u, 23033u, 28725u, 32768u, 0u},
            {7029u, 17840u, 24528u, 32768u, 0u},
            {4867u, 13886u, 21502u, 32768u, 0u},
            {25298u, 31892u, 32491u, 32768u, 0u},
            {17809u, 29330u, 31512u, 32768u, 0u},
            {9668u, 21329u, 26579u, 32768u, 0u},
            {4774u, 12956u, 18976u, 32768u, 0u},
            {2322u, 7030u, 11540u, 32768u, 0u},
            {25472u, 31920u, 32543u, 32768u, 0u},
            {17957u, 29387u, 31632u, 32768u, 0u},
            {9196u, 20593u, 26400u, 32768u, 0u},
            {4680u, 12705u, 19202u, 32768u, 0u},
            {2917u, 8456u, 13436u, 32768u, 0u},
            {26471u, 32059u, 32574u, 32768u, 0u},
            {18458u, 29783u, 31909u, 32768u, 0u},
            {8400u, 19464u, 25956u, 32768u, 0u},
            {3812u, 10973u, 17206u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {6779u, 13743u, 17678u, 32768u, 0u},
            {24806u, 31797u, 32457u, 32768u, 0u},
            {17616u, 29047u, 31372u, 32768u, 0u},
            {11063u, 23175u, 28003u, 32768u, 0u},
            {6521u, 16110u, 22324u, 32768u, 0u},
            {2764u, 7504u, 11654u, 32768u, 0u},
            {25266u, 32367u, 32637u, 32768u, 0u},
            {19054u, 30553u, 32175u, 32768u, 0u},
            {12139u, 25212u, 29807u, 32768u, 0u},
            {7311u, 18162u, 24704u, 32768u, 0u},
            {3397u, 9164u, 14074u, 32768u, 0u},
            {25988u, 32208u, 32522u, 32768u, 0u},
            {16253u, 28912u, 31526u, 32768u, 0u},
            {9151u, 21387u, 27372u, 32768u, 0u},
            {5688u, 14915u, 21496u, 32768u, 0u},
            {2717u, 7627u, 12004u, 32768u, 0u},
            {23144u, 31855u, 32443u, 32768u, 0u},
            {16070u, 28491u, 31325u, 32768u, 0u},
            {8702u, 20467u, 26517u, 32768u, 0u},
            {5243u, 13956u, 20367u, 32768u, 0u},
            {2621u, 7335u, 11567u, 32768u, 0u},
            {26636u, 32340u, 32630u, 32768u, 0u},
            {19990u, 31050u, 32341u, 32768u, 0u},
            {13243u, 26105u, 30315u, 32768u, 0u},
            {8588u, 19521u, 25918u, 32768u, 0u},
            {4717u, 11585u, 17304u, 32768u, 0u},
            {25844u, 32292u, 32582u, 32768u, 0u},
            {19090u, 30635u, 32097u, 32768u, 0u},
            {11963u, 24546u, 28939u, 32768u, 0u},
            {6218u, 16087u, 22354u, 32768u, 0u},
            {2340u, 6608u, 10426u, 32768u, 0u},
            {28046u, 32576u, 32694u, 32768u, 0u},
            {21178u, 31313u, 32296u, 32768u, 0u},
            {13486u, 26184u, 29870u, 32768u, 0u},
            {7149u, 17871u, 23723u, 32768u, 0u},
            {2833u, 7958u, 12259u, 32768u, 0u},
            {27710u, 32528u, 32686u, 32768u, 0u},
            {20674u, 31076u, 32268u, 32768u, 0u},
            {12413u, 24955u, 29243u, 32768u, 0u},
            {6676u, 16927u, 23097u, 32768u, 0u},
            {2966u, 8333u, 12919u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8639u, 19339u, 24429u, 32768u, 0u},
            {24404u, 31837u, 32525u, 32768u, 0u},
            {16997u, 29425u, 31784u, 32768u, 0u},
            {11253u, 24234u, 29149u, 32768u, 0u},
            {6751u, 17394u, 24028u, 32768u, 0u},
            {3490u, 9830u, 15191u, 32768u, 0u},
            {26283u, 32471u, 32714u, 32768u, 0u},
            {19599u, 31168u, 32442u, 32768u, 0u},
            {13146u, 26954u, 30893u, 32768u, 0u},
            {8214u, 20588u, 26890u, 32768u, 0u},
            {4699u, 13081u, 19300u, 32768u, 0u},
            {28212u, 32458u, 32669u, 32768u, 0u},
            {18594u, 30316u, 32100u, 32768u, 0u},
            {11219u, 24408u, 29234u, 32768u, 0u},
            {6865u, 17656u, 24149u, 32768u, 0u},
            {3678u, 10362u, 16006u, 32768u, 0u},
            {25825u, 32136u, 32616u, 32768u, 0u},
            {17313u, 29853u, 32021u, 32768u, 0u},
            {11197u, 24471u, 29472u, 32768u, 0u},
            {6947u, 17781u, 24405u, 32768u, 0u},
            {3768u, 10660u, 16261u, 32768u, 0u},
            {27352u, 32500u, 32706u, 32768u, 0u},
            {20850u, 31468u, 32469u, 32768u, 0u},
            {14021u, 27707u, 31133u, 32768u, 0u},
            {8964u, 21748u, 27838u, 32768u, 0u},
            {5437u, 14665u, 21187u, 32768u, 0u},
            {26304u, 32492u, 32698u, 32768u, 0u},
            {20409u, 31380u, 32385u, 32768u, 0u},
            {13682u, 27222u, 30632u, 32768u, 0u},
            {8974u, 21236u, 26685u, 32768u, 0u},
            {4234u, 11665u, 16934u, 32768u, 0u},
            {26273u, 32357u, 32711u, 32768u, 0u},
            {20672u, 31242u, 32441u, 32768u, 0u},
            {14172u, 27254u, 30902u, 32768u, 0u},
            {9870u, 21898u, 27275u, 32768u, 0u},
            {5164u, 13506u, 19270u, 32768u, 0u},
            {26725u, 32459u, 32728u, 32768u, 0u},
            {20991u, 31442u, 32527u, 32768u, 0u},
            {13071u, 26434u, 30811u, 32768u, 0u},
            {8184u, 20090u, 26742u, 32768u, 0u},
            {4803u, 13255u, 19895u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {7555u, 14942u, 18501u, 32768u, 0u},
            {24410u, 31178u, 32287u, 32768u, 0u},
            {14394u, 26738u, 30253u, 32768u, 0u},
            {8413u, 19554u, 25195u, 32768u, 0u},
            {4766u, 12924u, 18785u, 32768u, 0u},
            {2029u, 5806u, 9207u, 32768u, 0u},
            {26776u, 32364u, 32663u, 32768u, 0u},
            {18732u, 29967u, 31931u, 32768u, 0u},
            {11005u, 23786u, 28852u, 32768u, 0u},
            {6466u, 16909u, 23510u, 32768u, 0u},
            {3044u, 8638u, 13419u, 32768u, 0u},
            {29208u, 32582u, 32704u, 32768u, 0u},
            {20068u, 30857u, 32208u, 32768u, 0u},
            {12003u, 25085u, 29595u, 32768u, 0u},
            {6947u, 17750u, 24189u, 32768u, 0u},
            {3245u, 9103u, 14007u, 32768u, 0u},
            {27359u, 32465u, 32669u, 32768u, 0u},
            {19421u, 30614u, 32174u, 32768u, 0u},
            {11915u, 25010u, 29579u, 32768u, 0u},
            {6950u, 17676u, 24074u, 32768u, 0u},
            {3007u, 8473u, 13096u, 32768u, 0u},
            {29002u, 32676u, 32735u, 32768u, 0u},
            {22102u, 31849u, 32576u, 32768u, 0u},
            {14408u, 28009u, 31405u, 32768u, 0u},
            {9027u, 21679u, 27931u, 32768u, 0u},
            {4694u, 12678u, 18748u, 32768u, 0u},
            {28216u, 32528u, 32682u, 32768u, 0u},
            {20849u, 31264u, 32318u, 32768u, 0u},
            {12756u, 25815u, 29751u, 32768u, 0u},
            {7565u, 18801u, 24923u, 32768u, 0u},
            {3509u, 9533u, 14477u, 32768u, 0u},
            {30133u, 32687u, 32739u, 32768u, 0u},
            {23063u, 31910u, 32515u, 32768u, 0u},
            {14588u, 28051u, 31132u, 32768u, 0u},
            {9085u, 21649u, 27457u, 32768u, 0u},
            {4261u, 11654u, 17264u, 32768u, 0u},
            {29518u, 32691u, 32748u, 32768u, 0u},
            {22451u, 31959u, 32613u, 32768u, 0u},
            {14864u, 28722u, 31700u, 32768u, 0u},
            {9695u, 22964u, 28716u, 32768u, 0u},
            {4932u, 13358u, 19502u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {6465u, 16958u, 21688u, 32768u, 0u},
            {25199u, 31514u, 32360u, 32768u, 0u},
            {14774u, 27149u, 30607u, 32768u, 0u},
            {9257u, 21438u, 26972u, 32768u, 0u},
            {5723u, 15183u, 21882u, 32768u, 0u},
            {3150u, 8879u, 13731u, 32768u, 0u},
            {26989u, 32262u, 32682u, 32768u, 0u},
            {17396u, 29937u, 32085u, 32768u, 0u},
            {11387u, 24901u, 29784u, 32768u, 0u},
            {7289u, 18821u, 25548u, 32768u, 0u},
            {3734u, 10577u, 16086u, 32768u, 0u},
            {29728u, 32501u, 32695u, 32768u, 0u},
            {17431u, 29701u, 31903u, 32768u, 0u},
            {9921u, 22826u, 28300u, 32768u, 0u},
            {5896u, 15434u, 22068u, 32768u, 0u},
            {3430u, 9646u, 14757u, 32768u, 0u},
            {28614u, 32511u, 32705u, 32768u, 0u},
            {19364u, 30638u, 32263u, 32768u, 0u},
            {13129u, 26254u, 30402u, 32768u, 0u},
            {8754u, 20484u, 26440u, 32768u, 0u},
            {4378u, 11607u, 17110u, 32768u, 0u},
            {30292u, 32671u, 32744u, 32768u, 0u},
            {21780u, 31603u, 32501u, 32768u, 0u},
            {14314u, 27829u, 31291u, 32768u, 0u},
            {9611u, 22327u, 28263u, 32768u, 0u},
            {4890u, 13087u, 19065u, 32768u, 0u},
            {25862u, 32567u, 32733u, 32768u, 0u},
            {20794u, 32050u, 32567u, 32768u, 0u},
            {17243u, 30625u, 32254u, 32768u, 0u},
            {13283u, 27628u, 31474u, 32768u, 0u},
            {9669u, 22532u, 28918u, 32768u, 0u},
            {27435u, 32697u, 32748u, 32768u, 0u},
            {24922u, 32390u, 32714u, 32768u, 0u},
            {21449u, 31504u, 32536u, 32768u, 0u},
            {16392u, 29729u, 31832u, 32768u, 0u},
            {11692u, 24884u, 29076u, 32768u, 0u},
            {24193u, 32290u, 32735u, 32768u, 0u},
            {18909u, 31104u, 32563u, 32768u, 0u},
            {12236u, 26841u, 31403u, 32768u, 0u},
            {8171u, 21840u, 29082u, 32768u, 0u},
            {7224u, 17280u, 25275u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {3078u, 6839u, 9890u, 32768u, 0u},
            {13837u, 20450u, 24479u, 32768u, 0u},
            {5914u, 14222u, 19328u, 32768u, 0u},
            {3866u, 10267u, 14762u, 32768u, 0u},
            {2612u, 7208u, 11042u, 32768u, 0u},
            {1067u, 2991u, 4776u, 32768u, 0u},
            {25817u, 31646u, 32529u, 32768u, 0u},
            {13708u, 26338u, 30385u, 32768u, 0u},
            {7328u, 18585u, 24870u, 32768u, 0u},
            {4691u, 13080u, 19276u, 32768u, 0u},
            {1825u, 5253u, 8352u, 32768u, 0u},
            {29386u, 32315u, 32624u, 32768u, 0u},
            {17160u, 29001u, 31360u, 32768u, 0u},
            {9602u, 21862u, 27396u, 32768u, 0u},
            {5915u, 15772u, 22148u, 32768u, 0u},
            {2786u, 7779u, 12047u, 32768u, 0u},
            {29246u, 32450u, 32663u, 32768u, 0u},
            {18696u, 29929u, 31818u, 32768u, 0u},
            {10510u, 23369u, 28560u, 32768u, 0u},
            {6229u, 16499u, 23125u, 32768u, 0u},
            {2608u, 7448u, 11705u, 32768u, 0u},
            {30753u, 32710u, 32748u, 32768u, 0u},
            {21638u, 31487u, 32503u, 32768u, 0u},
            {12937u, 26854u, 30870u, 32768u, 0u},
            {8182u, 20596u, 26970u, 32768u, 0u},
            {3637u, 10269u, 15497u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {5244u, 12150u, 16906u, 32768u, 0u},
            {20486u, 26858u, 29701u, 32768u, 0u},
            {7756u, 18317u, 23735u, 32768u, 0u},
            {3452u, 9256u, 13146u, 32768u, 0u},
            {2020u, 5206u, 8229u, 32768u, 0u},
            {1801u, 4993u, 7903u, 32768u, 0u},
            {27051u, 31858u, 32531u, 32768u, 0u},
            {15988u, 27531u, 30619u, 32768u, 0u},
            {9188u, 21484u, 26719u, 32768u, 0u},
            {6273u, 17186u, 23800u, 32768u, 0u},
            {3108u, 9355u, 14764u, 32768u, 0u},
            {31076u, 32520u, 32680u, 32768u, 0u},
            {18119u, 30037u, 31850u, 32768u, 0u},
            {10244u, 22969u, 27472u, 32768u, 0u},
            {4692u, 14077u, 19273u, 32768u, 0u},
            {3694u, 11677u, 17556u, 32768u, 0u},
            {30060u, 32581u, 32720u, 32768u, 0u},
            {21011u, 30775u, 32120u, 32768u, 0u},
            {11931u, 24820u, 29289u, 32768u, 0u},
            {7119u, 17662u, 24356u, 32768u, 0u},
            {3833u, 10706u, 16304u, 32768u, 0u},
            {31954u, 32731u, 32748u, 32768u, 0u},
            {23913u, 31724u, 32489u, 32768u, 0u},
            {15520u, 28060u, 31286u, 32768u, 0u},
            {11517u, 23008u, 28571u, 32768u, 0u},
            {6193u, 14508u, 20629u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {1035u, 2807u, 4156u, 32768u, 0u},
            {13162u, 18138u, 20939u, 32768u, 0u},
            {2696u, 6633u, 8755u, 32768u, 0u},
            {1373u, 4161u, 6853u, 32768u, 0u},
            {1099u, 2746u, 4716u, 32768u, 0u},
            {340u, 1021u, 1599u, 32768u, 0u},
            {22826u, 30419u, 32135u, 32768u, 0u},
            {10395u, 21762u, 26942u, 32768u, 0u},
            {4726u, 12407u, 17361u, 32768u, 0u},
            {2447u, 7080u, 10593u, 32768u, 0u},
            {1227u, 3717u, 6011u, 32768u, 0u},
            {28156u, 31424u, 31934u, 32768u, 0u},
            {16915u, 27754u, 30373u, 32768u, 0u},
            {9148u, 20990u, 26431u, 32768u, 0u},
            {5950u, 15515u, 21148u, 32768u, 0u},
            {2492u, 7327u, 11526u, 32768u, 0u},
            {30602u, 32477u, 32670u, 32768u, 0u},
            {20026u, 29955u, 31568u, 32768u, 0u},
            {11220u, 23628u, 28105u, 32768u, 0u},
            {6652u, 17019u, 22973u, 32768u, 0u},
            {3064u, 8536u, 13043u, 32768u, 0u},
            {31769u, 32724u, 32748u, 32768u, 0u},
            {22230u, 30887u, 32373u, 32768u, 0u},
            {12234u, 25079u, 29731u, 32768u, 0u},
            {7326u, 18816u, 25353u, 32768u, 0u},
            {3933u, 10907u, 16616u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {8896u, 16227u, 20630u, 32768u, 0u},
            {23629u, 31782u, 32527u, 32768u, 0u},
            {15173u, 27755u, 31321u, 32768u, 0u},
            {10158u, 21233u, 27382u, 32768u, 0u},
            {6420u, 14857u, 21558u, 32768u, 0u},
            {3269u, 8155u, 12646u, 32768u, 0u},
            {24835u, 32009u, 32496u, 32768u, 0u},
            {16509u, 28421u, 31579u, 32768u, 0u},
            {10957u, 21514u, 27418u, 32768u, 0u},
            {7881u, 15930u, 22096u, 32768u, 0u},
            {5388u, 10960u, 15918u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {20745u, 30773u, 32093u, 32768u, 0u},
            {15200u, 27221u, 30861u, 32768u, 0u},
            {13032u, 20873u, 25667u, 32768u, 0u},
            {12285u, 18663u, 23494u, 32768u, 0u},
            {11563u, 17481u, 21489u, 32768u, 0u},
            {26260u, 31982u, 32320u, 32768u, 0u},
            {15397u, 28083u, 31100u, 32768u, 0u},
            {9742u, 19217u, 24824u, 32768u, 0u},
            {3261u, 9629u, 15362u, 32768u, 0u},
            {1480u, 4322u, 7499u, 32768u, 0u},
            {27599u, 32256u, 32460u, 32768u, 0u},
            {16857u, 27659u, 30774u, 32768u, 0u},
            {9551u, 18290u, 23748u, 32768u, 0u},
            {3052u, 8933u, 14103u, 32768u, 0u},
            {2021u, 5910u, 9787u, 32768u, 0u},
            {29005u, 32015u, 32392u, 32768u, 0u},
            {17677u, 27694u, 30863u, 32768u, 0u},
            {9204u, 17356u, 23219u, 32768u, 0u},
            {2403u, 7516u, 12814u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {10808u, 22056u, 26896u, 32768u, 0u},
            {25739u, 32313u, 32676u, 32768u, 0u},
            {17288u, 30203u, 32221u, 32768u, 0u},
            {11359u, 24878u, 29896u, 32768u, 0u},
            {6949u, 17767u, 24893u, 32768u, 0u},
            {4287u, 11796u, 18071u, 32768u, 0u},
            {27880u, 32521u, 32705u, 32768u, 0u},
            {19038u, 31004u, 32414u, 32768u, 0u},
            {12564u, 26345u, 30768u, 32768u, 0u},
            {8269u, 19947u, 26779u, 32768u, 0u},
            {5674u, 14657u, 21674u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {25742u, 32319u, 32671u, 32768u, 0u},
            {19557u, 31164u, 32454u, 32768u, 0u},
            {13381u, 26381u, 30755u, 32768u, 0u},
            {10101u, 21466u, 26722u, 32768u, 0u},
            {9209u, 19650u, 26825u, 32768u, 0u},
            {27107u, 31917u, 32432u, 32768u, 0u},
            {18056u, 28893u, 31203u, 32768u, 0u},
            {10200u, 21434u, 26764u, 32768u, 0u},
            {4660u, 12913u, 19502u, 32768u, 0u},
            {2368u, 6930u, 12504u, 32768u, 0u},
            {26960u, 32158u, 32613u, 32768u, 0u},
            {18628u, 30005u, 32031u, 32768u, 0u},
            {10233u, 22442u, 28232u, 32768u, 0u},
            {5471u, 14630u, 21516u, 32768u, 0u},
            {3235u, 10767u, 17109u, 32768u, 0u},
            {27696u, 32440u, 32692u, 32768u, 0u},
            {20032u, 31167u, 32438u, 32768u, 0u},
            {8700u, 21341u, 28442u, 32768u, 0u},
            {5662u, 14831u, 21795u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {9704u, 17294u, 21132u, 32768u, 0u},
            {26762u, 32278u, 32633u, 32768u, 0u},
            {18382u, 29620u, 31819u, 32768u, 0u},
            {10891u, 23475u, 28723u, 32768u, 0u},
            {6358u, 16583u, 23309u, 32768u, 0u},
            {3248u, 9118u, 14141u, 32768u, 0u},
            {27204u, 32573u, 32699u, 32768u, 0u},
            {19818u, 30824u, 32329u, 32768u, 0u},
            {11772u, 25120u, 30041u, 32768u, 0u},
            {6995u, 18033u, 25039u, 32768u, 0u},
            {3752u, 10442u, 16098u, 32768u, 0u},
            {27222u, 32256u, 32559u, 32768u, 0u},
            {15356u, 28399u, 31475u, 32768u, 0u},
            {8821u, 20635u, 27057u, 32768u, 0u},
            {5511u, 14404u, 21239u, 32768u, 0u},
            {2935u, 8222u, 13051u, 32768u, 0u},
            {24875u, 32120u, 32529u, 32768u, 0u},
            {15233u, 28265u, 31445u, 32768u, 0u},
            {8605u, 20570u, 26932u, 32768u, 0u},
            {5431u, 14413u, 21196u, 32768u, 0u},
            {2994u, 8341u, 13223u, 32768u, 0u},
            {28201u, 32604u, 32700u, 32768u, 0u},
            {21041u, 31446u, 32456u, 32768u, 0u},
            {13221u, 26213u, 30475u, 32768u, 0u},
            {8255u, 19385u, 26037u, 32768u, 0u},
            {4930u, 12585u, 18830u, 32768u, 0u},
            {28768u, 32448u, 32627u, 32768u, 0u},
            {19705u, 30561u, 32021u, 32768u, 0u},
            {11572u, 23589u, 28220u, 32768u, 0u},
            {5532u, 15034u, 21446u, 32768u, 0u},
            {2460u, 7150u, 11456u, 32768u, 0u},
            {29874u, 32619u, 32699u, 32768u, 0u},
            {21621u, 31071u, 32201u, 32768u, 0u},
            {12511u, 24747u, 28992u, 32768u, 0u},
            {6281u, 16395u, 22748u, 32768u, 0u},
            {3246u, 9278u, 14497u, 32768u, 0u},
            {29715u, 32625u, 32712u, 32768u, 0u},
            {20958u, 31011u, 32283u, 32768u, 0u},
            {11233u, 23671u, 28806u, 32768u, 0u},
            {6012u, 16128u, 22868u, 32768u, 0u},
            {3427u, 9851u, 15414u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {11016u, 22111u, 26794u, 32768u, 0u},
            {25946u, 32357u, 32677u, 32768u, 0u},
            {17890u, 30452u, 32252u, 32768u, 0u},
            {11678u, 25142u, 29816u, 32768u, 0u},
            {6720u, 17534u, 24584u, 32768u, 0u},
            {4230u, 11665u, 17820u, 32768u, 0u},
            {28400u, 32623u, 32747u, 32768u, 0u},
            {21164u, 31668u, 32575u, 32768u, 0u},
            {13572u, 27388u, 31182u, 32768u, 0u},
            {8234u, 20750u, 27358u, 32768u, 0u},
            {5065u, 14055u, 20897u, 32768u, 0u},
            {28981u, 32547u, 32705u, 32768u, 0u},
            {18681u, 30543u, 32239u, 32768u, 0u},
            {10919u, 24075u, 29286u, 32768u, 0u},
            {6431u, 17199u, 24077u, 32768u, 0u},
            {3819u, 10464u, 16618u, 32768u, 0u},
            {26870u, 32467u, 32693u, 32768u, 0u},
            {19041u, 30831u, 32347u, 32768u, 0u},
            {11794u, 25211u, 30016u, 32768u, 0u},
            {6888u, 18019u, 24970u, 32768u, 0u},
            {4370u, 12363u, 18992u, 32768u, 0u},
            {29578u, 32670u, 32744u, 32768u, 0u},
            {23159u, 32007u, 32613u, 32768u, 0u},
            {15315u, 28669u, 31676u, 32768u, 0u},
            {9298u, 22607u, 28782u, 32768u, 0u},
            {6144u, 15913u, 22968u, 32768u, 0u},
            {28110u, 32499u, 32669u, 32768u, 0u},
            {21574u, 30937u, 32015u, 32768u, 0u},
            {12759u, 24818u, 28727u, 32768u, 0u},
            {6545u, 16761u, 23042u, 32768u, 0u},
            {3649u, 10597u, 16833u, 32768u, 0u},
            {28163u, 32552u, 32728u, 32768u, 0u},
            {22101u, 31469u, 32464u, 32768u, 0u},
            {13160u, 25472u, 30143u, 32768u, 0u},
            {7303u, 18684u, 25468u, 32768u, 0u},
            {5241u, 13975u, 20955u, 32768u, 0u},
            {28400u, 32631u, 32744u, 32768u, 0u},
            {22104u, 31793u, 32603u, 32768u, 0u},
            {13557u, 26571u, 30846u, 32768u, 0u},
            {7749u, 19861u, 26675u, 32768u, 0u},
            {4873u, 14030u, 21234u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {9800u, 17635u, 21073u, 32768u, 0u},
            {26153u, 31885u, 32527u, 32768u, 0u},
            {15038u, 27852u, 31006u, 32768u, 0u},
            {8718u, 20564u, 26486u, 32768u, 0u},
            {5128u, 14076u, 20514u, 32768u, 0u},
            {2636u, 7566u, 11925u, 32768u, 0u},
            {27551u, 32504u, 32701u, 32768u, 0u},
            {18310u, 30054u, 32100u, 32768u, 0u},
            {10211u, 23420u, 29082u, 32768u, 0u},
            {6222u, 16876u, 23916u, 32768u, 0u},
            {3462u, 9954u, 15498u, 32768u, 0u},
            {29991u, 32633u, 32721u, 32768u, 0u},
            {19883u, 30751u, 32201u, 32768u, 0u},
            {11141u, 24184u, 29285u, 32768u, 0u},
            {6420u, 16940u, 23774u, 32768u, 0u},
            {3392u, 9753u, 15118u, 32768u, 0u},
            {28465u, 32616u, 32712u, 32768u, 0u},
            {19850u, 30702u, 32244u, 32768u, 0u},
            {10983u, 24024u, 29223u, 32768u, 0u},
            {6294u, 16770u, 23582u, 32768u, 0u},
            {3244u, 9283u, 14509u, 32768u, 0u},
            {30023u, 32717u, 32748u, 32768u, 0u},
            {22940u, 32032u, 32626u, 32768u, 0u},
            {14282u, 27928u, 31473u, 32768u, 0u},
            {8562u, 21327u, 27914u, 32768u, 0u},
            {4846u, 13393u, 19919u, 32768u, 0u},
            {29981u, 32590u, 32695u, 32768u, 0u},
            {20465u, 30963u, 32166u, 32768u, 0u},
            {11479u, 23579u, 28195u, 32768u, 0u},
            {5916u, 15648u, 22073u, 32768u, 0u},
            {3031u, 8605u, 13398u, 32768u, 0u},
            {31146u, 32691u, 32739u, 32768u, 0u},
            {23106u, 31724u, 32444u, 32768u, 0u},
            {13783u, 26738u, 30439u, 32768u, 0u},
            {7852u, 19468u, 25807u, 32768u, 0u},
            {3860u, 11124u, 16853u, 32768u, 0u},
            {31014u, 32724u, 32748u, 32768u, 0u},
            {23629u, 32109u, 32628u, 32768u, 0u},
            {14747u, 28115u, 31403u, 32768u, 0u},
            {8545u, 21242u, 27478u, 32768u, 0u},
            {4574u, 12781u, 19067u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {9185u, 19694u, 24688u, 32768u, 0u},
            {26081u, 31985u, 32621u, 32768u, 0u},
            {16015u, 29000u, 31787u, 32768u, 0u},
            {10542u, 23690u, 29206u, 32768u, 0u},
            {6732u, 17945u, 24677u, 32768u, 0u},
            {3916u, 11039u, 16722u, 32768u, 0u},
            {28224u, 32566u, 32744u, 32768u, 0u},
            {19100u, 31138u, 32485u, 32768u, 0u},
            {12528u, 26620u, 30879u, 32768u, 0u},
            {7741u, 20277u, 26885u, 32768u, 0u},
            {4566u, 12845u, 18990u, 32768u, 0u},
            {29933u, 32593u, 32718u, 32768u, 0u},
            {17670u, 30333u, 32155u, 32768u, 0u},
            {10385u, 23600u, 28909u, 32768u, 0u},
            {6243u, 16236u, 22407u, 32768u, 0u},
            {3976u, 10389u, 16017u, 32768u, 0u},
            {28377u, 32561u, 32738u, 32768u, 0u},
            {19366u, 31175u, 32482u, 32768u, 0u},
            {13327u, 27175u, 31094u, 32768u, 0u},
            {8258u, 20769u, 27143u, 32768u, 0u},
            {4703u, 13198u, 19527u, 32768u, 0u},
            {31086u, 32706u, 32748u, 32768u, 0u},
            {22853u, 31902u, 32583u, 32768u, 0u},
            {14759u, 28186u, 31419u, 32768u, 0u},
            {9284u, 22382u, 28348u, 32768u, 0u},
            {5585u, 15192u, 21868u, 32768u, 0u},
            {28291u, 32652u, 32746u, 32768u, 0u},
            {19849u, 32107u, 32571u, 32768u, 0u},
            {14834u, 26818u, 29214u, 32768u, 0u},
            {10306u, 22594u, 28672u, 32768u, 0u},
            {6615u, 17384u, 23384u, 32768u, 0u},
            {28947u, 32604u, 32745u, 32768u, 0u},
            {25625u, 32289u, 32646u, 32768u, 0u},
            {18758u, 28672u, 31403u, 32768u, 0u},
            {10017u, 23430u, 28523u, 32768u, 0u},
            {6862u, 15269u, 22131u, 32768u, 0u},
            {23933u, 32509u, 32739u, 32768u, 0u},
            {19927u, 31495u, 32631u, 32768u, 0u},
            {11903u, 26023u, 30621u, 32768u, 0u},
            {7026u, 20094u, 27252u, 32768u, 0u},
            {5998u, 18106u, 24437u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {4456u, 11274u, 15533u, 32768u, 0u},
            {21219u, 29079u, 31616u, 32768u, 0u},
            {11173u, 23774u, 28567u, 32768u, 0u},
            {7282u, 18293u, 24263u, 32768u, 0u},
            {4890u, 13286u, 19115u, 32768u, 0u},
            {1890u, 5508u, 8659u, 32768u, 0u},
            {26651u, 32136u, 32647u, 32768u, 0u},
            {14630u, 28254u, 31455u, 32768u, 0u},
            {8716u, 21287u, 27395u, 32768u, 0u},
            {5615u, 15331u, 22008u, 32768u, 0u},
            {2675u, 7700u, 12150u, 32768u, 0u},
            {29954u, 32526u, 32690u, 32768u, 0u},
            {16126u, 28982u, 31633u, 32768u, 0u},
            {9030u, 21361u, 27352u, 32768u, 0u},
            {5411u, 14793u, 21271u, 32768u, 0u},
            {2943u, 8422u, 13163u, 32768u, 0u},
            {29539u, 32601u, 32730u, 32768u, 0u},
            {18125u, 30385u, 32201u, 32768u, 0u},
            {10422u, 24090u, 29468u, 32768u, 0u},
            {6468u, 17487u, 24438u, 32768u, 0u},
            {2970u, 8653u, 13531u, 32768u, 0u},
            {30912u, 32715u, 32748u, 32768u, 0u},
            {20666u, 31373u, 32497u, 32768u, 0u},
            {12509u, 26640u, 30917u, 32768u, 0u},
            {8058u, 20629u, 27290u, 32768u, 0u},
            {4231u, 12006u, 18052u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {10202u, 20633u, 25484u, 32768u, 0u},
            {27336u, 31445u, 32352u, 32768u, 0u},
            {12420u, 24384u, 28552u, 32768u, 0u},
            {7648u, 18115u, 23856u, 32768u, 0u},
            {5662u, 14341u, 19902u, 32768u, 0u},
            {3611u, 10328u, 15390u, 32768u, 0u},
            {30945u, 32616u, 32736u, 32768u, 0u},
            {18682u, 30505u, 32253u, 32768u, 0u},
            {11513u, 25336u, 30203u, 32768u, 0u},
            {7449u, 19452u, 26148u, 32768u, 0u},
            {4482u, 13051u, 18886u, 32768u, 0u},
            {32022u, 32690u, 32747u, 32768u, 0u},
            {18578u, 30501u, 32146u, 32768u, 0u},
            {11249u, 23368u, 28631u, 32768u, 0u},
            {5645u, 16958u, 22158u, 32768u, 0u},
            {5009u, 11444u, 16637u, 32768u, 0u},
            {31357u, 32710u, 32748u, 32768u, 0u},
            {21552u, 31494u, 32504u, 32768u, 0u},
            {13891u, 27677u, 31340u, 32768u, 0u},
            {9051u, 22098u, 28172u, 32768u, 0u},
            {5190u, 13377u, 19486u, 32768u, 0u},
            {32364u, 32740u, 32748u, 32768u, 0u},
            {24839u, 31907u, 32551u, 32768u, 0u},
            {17160u, 28779u, 31696u, 32768u, 0u},
            {12452u, 24137u, 29602u, 32768u, 0u},
            {6165u, 15389u, 22477u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {2575u, 7281u, 11077u, 32768u, 0u},
            {14002u, 20866u, 25402u, 32768u, 0u},
            {6343u, 15056u, 19658u, 32768u, 0u},
            {4474u, 11858u, 17041u, 32768u, 0u},
            {2865u, 8299u, 12534u, 32768u, 0u},
            {1344u, 3949u, 6391u, 32768u, 0u},
            {24720u, 31239u, 32459u, 32768u, 0u},
            {12585u, 25356u, 29968u, 32768u, 0u},
            {7181u, 18246u, 24444u, 32768u, 0u},
            {5025u, 13667u, 19885u, 32768u, 0u},
            {2521u, 7304u, 11605u, 32768u, 0u},
            {29908u, 32252u, 32584u, 32768u, 0u},
            {17421u, 29156u, 31575u, 32768u, 0u},
            {9889u, 22188u, 27782u, 32768u, 0u},
            {5878u, 15647u, 22123u, 32768u, 0u},
            {2814u, 8665u, 13323u, 32768u, 0u},
            {30183u, 32568u, 32713u, 32768u, 0u},
            {18528u, 30195u, 32049u, 32768u, 0u},
            {10982u, 24606u, 29657u, 32768u, 0u},
            {6957u, 18165u, 25231u, 32768u, 0u},
            {3508u, 10118u, 15468u, 32768u, 0u},
            {31761u, 32736u, 32748u, 32768u, 0u},
            {21041u, 31328u, 32546u, 32768u, 0u},
            {12568u, 26732u, 31166u, 32768u, 0u},
            {8052u, 20720u, 27733u, 32768u, 0u},
            {4336u, 12192u, 18396u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {7062u, 16472u, 22319u, 32768u, 0u},
            {24538u, 32261u, 32674u, 32768u, 0u},
            {13675u, 28041u, 31779u, 32768u, 0u},
            {8590u, 20674u, 27631u, 32768u, 0u},
            {5685u, 14675u, 22013u, 32768u, 0u},
            {3655u, 9898u, 15731u, 32768u, 0u},
            {26493u, 32418u, 32658u, 32768u, 0u},
            {16376u, 29342u, 32090u, 32768u, 0u},
            {10594u, 22649u, 28970u, 32768u, 0u},
            {8176u, 17170u, 24303u, 32768u, 0u},
            {5605u, 12694u, 19139u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {23888u, 31902u, 32542u, 32768u, 0u},
            {18612u, 29687u, 31987u, 32768u, 0u},
            {16245u, 24852u, 29249u, 32768u, 0u},
            {15765u, 22608u, 27559u, 32768u, 0u},
            {19895u, 24699u, 27510u, 32768u, 0u},
            {28401u, 32212u, 32457u, 32768u, 0u},
            {15274u, 27825u, 30980u, 32768u, 0u},
            {9364u, 18128u, 24332u, 32768u, 0u},
            {2283u, 8193u, 15082u, 32768u, 0u},
            {1228u, 3972u, 7881u, 32768u, 0u},
            {29455u, 32469u, 32620u, 32768u, 0u},
            {17981u, 28245u, 31388u, 32768u, 0u},
            {10921u, 20098u, 26240u, 32768u, 0u},
            {3743u, 11829u, 18657u, 32768u, 0u},
            {2374u, 9593u, 15715u, 32768u, 0u},
            {31068u, 32466u, 32635u, 32768u, 0u},
            {20321u, 29572u, 31971u, 32768u, 0u},
            {10771u, 20255u, 27119u, 32768u, 0u},
            {2795u, 10410u, 17361u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {9320u, 22102u, 27840u, 32768u, 0u},
            {27057u, 32464u, 32724u, 32768u, 0u},
            {16331u, 30268u, 32309u, 32768u, 0u},
            {10319u, 23935u, 29720u, 32768u, 0u},
            {6189u, 16448u, 24106u, 32768u, 0u},
            {3589u, 10884u, 18808u, 32768u, 0u},
            {29026u, 32624u, 32748u, 32768u, 0u},
            {19226u, 31507u, 32587u, 32768u, 0u},
            {12692u, 26921u, 31203u, 32768u, 0u},
            {7049u, 19532u, 27635u, 32768u, 0u},
            {7727u, 15669u, 23252u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {28056u, 32625u, 32748u, 32768u, 0u},
            {22383u, 32075u, 32669u, 32768u, 0u},
            {15417u, 27098u, 31749u, 32768u, 0u},
            {18127u, 26493u, 27190u, 32768u, 0u},
            {5461u, 16384u, 21845u, 32768u, 0u},
            {27982u, 32091u, 32584u, 32768u, 0u},
            {19045u, 29868u, 31972u, 32768u, 0u},
            {10397u, 22266u, 27932u, 32768u, 0u},
            {5990u, 13697u, 21500u, 32768u, 0u},
            {1792u, 6912u, 15104u, 32768u, 0u},
            {28198u, 32501u, 32718u, 32768u, 0u},
            {21534u, 31521u, 32569u, 32768u, 0u},
            {11109u, 25217u, 30017u, 32768u, 0u},
            {5671u, 15124u, 26151u, 32768u, 0u},
            {4681u, 14043u, 18725u, 32768u, 0u},
            {28688u, 32580u, 32741u, 32768u, 0u},
            {22576u, 32079u, 32661u, 32768u, 0u},
            {10627u, 22141u, 28340u, 32768u, 0u},
            {9362u, 14043u, 28087u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {7754u, 16948u, 22142u, 32768u, 0u},
            {25670u, 32330u, 32691u, 32768u, 0u},
            {15663u, 29225u, 31994u, 32768u, 0u},
            {9878u, 23288u, 29158u, 32768u, 0u},
            {6419u, 17088u, 24336u, 32768u, 0u},
            {3859u, 11003u, 17039u, 32768u, 0u},
            {27562u, 32595u, 32725u, 32768u, 0u},
            {17575u, 30588u, 32399u, 32768u, 0u},
            {10819u, 24838u, 30309u, 32768u, 0u},
            {7124u, 18686u, 25916u, 32768u, 0u},
            {4479u, 12688u, 19340u, 32768u, 0u},
            {28385u, 32476u, 32673u, 32768u, 0u},
            {15306u, 29005u, 31938u, 32768u, 0u},
            {8937u, 21615u, 28322u, 32768u, 0u},
            {5982u, 15603u, 22786u, 32768u, 0u},
            {3620u, 10267u, 16136u, 32768u, 0u},
            {27280u, 32464u, 32667u, 32768u, 0u},
            {15607u, 29160u, 32004u, 32768u, 0u},
            {9091u, 22135u, 28740u, 32768u, 0u},
            {6232u, 16632u, 24020u, 32768u, 0u},
            {4047u, 11377u, 17672u, 32768u, 0u},
            {29220u, 32630u, 32718u, 32768u, 0u},
            {19650u, 31220u, 32462u, 32768u, 0u},
            {13050u, 26312u, 30827u, 32768u, 0u},
            {9228u, 20870u, 27468u, 32768u, 0u},
            {6146u, 15149u, 21971u, 32768u, 0u},
            {30169u, 32481u, 32623u, 32768u, 0u},
            {17212u, 29311u, 31554u, 32768u, 0u},
            {9911u, 21311u, 26882u, 32768u, 0u},
            {4487u, 13314u, 20372u, 32768u, 0u},
            {2570u, 7772u, 12889u, 32768u, 0u},
            {30924u, 32613u, 32708u, 32768u, 0u},
            {19490u, 30206u, 32107u, 32768u, 0u},
            {11232u, 23998u, 29276u, 32768u, 0u},
            {6769u, 17955u, 25035u, 32768u, 0u},
            {4398u, 12623u, 19214u, 32768u, 0u},
            {30609u, 32627u, 32722u, 32768u, 0u},
            {19370u, 30582u, 32287u, 32768u, 0u},
            {10457u, 23619u, 29409u, 32768u, 0u},
            {6443u, 17637u, 24834u, 32768u, 0u},
            {4645u, 13236u, 20106u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8626u, 20271u, 26216u, 32768u, 0u},
            {26707u, 32406u, 32711u, 32768u, 0u},
            {16999u, 30329u, 32286u, 32768u, 0u},
            {11445u, 25123u, 30286u, 32768u, 0u},
            {6411u, 18828u, 25601u, 32768u, 0u},
            {6801u, 12458u, 20248u, 32768u, 0u},
            {29918u, 32682u, 32748u, 32768u, 0u},
            {20649u, 31739u, 32618u, 32768u, 0u},
            {12879u, 27773u, 31581u, 32768u, 0u},
            {7896u, 21751u, 28244u, 32768u, 0u},
            {5260u, 14870u, 23698u, 32768u, 0u},
            {29252u, 32593u, 32731u, 32768u, 0u},
            {17072u, 30460u, 32294u, 32768u, 0u},
            {10653u, 24143u, 29365u, 32768u, 0u},
            {6536u, 17490u, 23983u, 32768u, 0u},
            {4929u, 13170u, 20085u, 32768u, 0u},
            {28137u, 32518u, 32715u, 32768u, 0u},
            {18171u, 30784u, 32407u, 32768u, 0u},
            {11437u, 25436u, 30459u, 32768u, 0u},
            {7252u, 18534u, 26176u, 32768u, 0u},
            {4126u, 13353u, 20978u, 32768u, 0u},
            {31162u, 32726u, 32748u, 32768u, 0u},
            {23017u, 32222u, 32701u, 32768u, 0u},
            {15629u, 29233u, 32046u, 32768u, 0u},
            {9387u, 22621u, 29480u, 32768u, 0u},
            {6922u, 17616u, 25010u, 32768u, 0u},
            {28838u, 32265u, 32614u, 32768u, 0u},
            {19701u, 30206u, 31920u, 32768u, 0u},
            {11214u, 22410u, 27933u, 32768u, 0u},
            {5320u, 14177u, 23034u, 32768u, 0u},
            {5049u, 12881u, 17827u, 32768u, 0u},
            {27484u, 32471u, 32734u, 32768u, 0u},
            {21076u, 31526u, 32561u, 32768u, 0u},
            {12707u, 26303u, 31211u, 32768u, 0u},
            {8169u, 21722u, 28219u, 32768u, 0u},
            {6045u, 19406u, 27042u, 32768u, 0u},
            {27753u, 32572u, 32745u, 32768u, 0u},
            {20832u, 31878u, 32653u, 32768u, 0u},
            {13250u, 27356u, 31674u, 32768u, 0u},
            {7718u, 21508u, 29858u, 32768u, 0u},
            {7209u, 18350u, 25559u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {7876u, 16901u, 21741u, 32768u, 0u},
            {24001u, 31898u, 32625u, 32768u, 0u},
            {14529u, 27959u, 31451u, 32768u, 0u},
            {8273u, 20818u, 27258u, 32768u, 0u},
            {5278u, 14673u, 21510u, 32768u, 0u},
            {2983u, 8843u, 14039u, 32768u, 0u},
            {28016u, 32574u, 32732u, 32768u, 0u},
            {17471u, 30306u, 32301u, 32768u, 0u},
            {10224u, 24063u, 29728u, 32768u, 0u},
            {6602u, 17954u, 25052u, 32768u, 0u},
            {4002u, 11585u, 17759u, 32768u, 0u},
            {30190u, 32634u, 32739u, 32768u, 0u},
            {17497u, 30282u, 32270u, 32768u, 0u},
            {10229u, 23729u, 29538u, 32768u, 0u},
            {6344u, 17211u, 24440u, 32768u, 0u},
            {3849u, 11189u, 17108u, 32768u, 0u},
            {28570u, 32583u, 32726u, 32768u, 0u},
            {17521u, 30161u, 32238u, 32768u, 0u},
            {10153u, 23565u, 29378u, 32768u, 0u},
            {6455u, 17341u, 24443u, 32768u, 0u},
            {3907u, 11042u, 17024u, 32768u, 0u},
            {30689u, 32715u, 32748u, 32768u, 0u},
            {21546u, 31840u, 32610u, 32768u, 0u},
            {13547u, 27581u, 31459u, 32768u, 0u},
            {8912u, 21757u, 28309u, 32768u, 0u},
            {5548u, 15080u, 22046u, 32768u, 0u},
            {30783u, 32540u, 32685u, 32768u, 0u},
            {17540u, 29528u, 31668u, 32768u, 0u},
            {10160u, 21468u, 26783u, 32768u, 0u},
            {4724u, 13393u, 20054u, 32768u, 0u},
            {2702u, 8174u, 13102u, 32768u, 0u},
            {31648u, 32686u, 32742u, 32768u, 0u},
            {20954u, 31094u, 32337u, 32768u, 0u},
            {12420u, 25698u, 30179u, 32768u, 0u},
            {7304u, 19320u, 26248u, 32768u, 0u},
            {4366u, 12261u, 18864u, 32768u, 0u},
            {31581u, 32723u, 32748u, 32768u, 0u},
            {21373u, 31586u, 32525u, 32768u, 0u},
            {12744u, 26625u, 30885u, 32768u, 0u},
            {7431u, 20322u, 26950u, 32768u, 0u},
            {4692u, 13323u, 20111u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {7833u, 18369u, 24095u, 32768u, 0u},
            {26650u, 32273u, 32702u, 32768u, 0u},
            {16371u, 29961u, 32191u, 32768u, 0u},
            {11055u, 24082u, 29629u, 32768u, 0u},
            {6892u, 18644u, 25400u, 32768u, 0u},
            {5006u, 13057u, 19240u, 32768u, 0u},
            {29834u, 32666u, 32748u, 32768u, 0u},
            {19577u, 31335u, 32570u, 32768u, 0u},
            {12253u, 26509u, 31122u, 32768u, 0u},
            {7991u, 20772u, 27711u, 32768u, 0u},
            {5677u, 15910u, 23059u, 32768u, 0u},
            {30109u, 32532u, 32720u, 32768u, 0u},
            {16747u, 30166u, 32252u, 32768u, 0u},
            {10134u, 23542u, 29184u, 32768u, 0u},
            {5791u, 16176u, 23556u, 32768u, 0u},
            {4362u, 10414u, 17284u, 32768u, 0u},
            {29492u, 32626u, 32748u, 32768u, 0u},
            {19894u, 31402u, 32525u, 32768u, 0u},
            {12942u, 27071u, 30869u, 32768u, 0u},
            {8346u, 21216u, 27405u, 32768u, 0u},
            {6572u, 17087u, 23859u, 32768u, 0u},
            {32035u, 32735u, 32748u, 32768u, 0u},
            {22957u, 31838u, 32618u, 32768u, 0u},
            {14724u, 28572u, 31772u, 32768u, 0u},
            {10364u, 23999u, 29553u, 32768u, 0u},
            {7004u, 18433u, 25655u, 32768u, 0u},
            {27528u, 32277u, 32681u, 32768u, 0u},
            {16959u, 31171u, 32096u, 32768u, 0u},
            {10486u, 23593u, 27962u, 32768u, 0u},
            {8192u, 16384u, 23211u, 32768u, 0u},
            {8937u, 17873u, 20852u, 32768u, 0u},
            {27715u, 32002u, 32615u, 32768u, 0u},
            {15073u, 29491u, 31676u, 32768u, 0u},
            {11264u, 24576u, 28672u, 32768u, 0u},
            {2341u, 18725u, 23406u, 32768u, 0u},
            {7282u, 18204u, 25486u, 32768u, 0u},
            {28547u, 32213u, 32657u, 32768u, 0u},
            {20788u, 29773u, 32239u, 32768u, 0u},
            {6780u, 21469u, 30508u, 32768u, 0u},
            {5958u, 14895u, 23831u, 32768u, 0u},
            {16384u, 21845u, 27307u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {5992u, 14304u, 19765u, 32768u, 0u},
            {22612u, 31238u, 32456u, 32768u, 0u},
            {13456u, 27162u, 31087u, 32768u, 0u},
            {8001u, 20062u, 26504u, 32768u, 0u},
            {5168u, 14105u, 20764u, 32768u, 0u},
            {2632u, 7771u, 12385u, 32768u, 0u},
            {27034u, 32344u, 32709u, 32768u, 0u},
            {15850u, 29415u, 31997u, 32768u, 0u},
            {9494u, 22776u, 28841u, 32768u, 0u},
            {6151u, 16830u, 23969u, 32768u, 0u},
            {3461u, 10039u, 15722u, 32768u, 0u},
            {30134u, 32569u, 32731u, 32768u, 0u},
            {15638u, 29422u, 31945u, 32768u, 0u},
            {9150u, 21865u, 28218u, 32768u, 0u},
            {5647u, 15719u, 22676u, 32768u, 0u},
            {3402u, 9772u, 15477u, 32768u, 0u},
            {28530u, 32586u, 32735u, 32768u, 0u},
            {17139u, 30298u, 32292u, 32768u, 0u},
            {10200u, 24039u, 29685u, 32768u, 0u},
            {6419u, 17674u, 24786u, 32768u, 0u},
            {3544u, 10225u, 15824u, 32768u, 0u},
            {31333u, 32726u, 32748u, 32768u, 0u},
            {20618u, 31487u, 32544u, 32768u, 0u},
            {12901u, 27217u, 31232u, 32768u, 0u},
            {8624u, 21734u, 28171u, 32768u, 0u},
            {5104u, 14191u, 20748u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {11206u, 21090u, 26561u, 32768u, 0u},
            {28759u, 32279u, 32671u, 32768u, 0u},
            {14171u, 27952u, 31569u, 32768u, 0u},
            {9743u, 22907u, 29141u, 32768u, 0u},
            {6871u, 17886u, 24868u, 32768u, 0u},
            {4960u, 13152u, 19315u, 32768u, 0u},
            {31077u, 32661u, 32748u, 32768u, 0u},
            {19400u, 31195u, 32515u, 32768u, 0u},
            {12752u, 26858u, 31040u, 32768u, 0u},
            {8370u, 22098u, 28591u, 32768u, 0u},
            {5457u, 15373u, 22298u, 32768u, 0u},
            {31697u, 32706u, 32748u, 32768u, 0u},
            {17860u, 30657u, 32333u, 32768u, 0u},
            {12510u, 24812u, 29261u, 32768u, 0u},
            {6180u, 19124u, 24722u, 32768u, 0u},
            {5041u, 13548u, 17959u, 32768u, 0u},
            {31552u, 32716u, 32748u, 32768u, 0u},
            {21908u, 31769u, 32623u, 32768u, 0u},
            {14470u, 28201u, 31565u, 32768u, 0u},
            {9493u, 22982u, 28608u, 32768u, 0u},
            {6858u, 17240u, 24137u, 32768u, 0u},
            {32543u, 32752u, 32756u, 32768u, 0u},
            {24286u, 32097u, 32666u, 32768u, 0u},
            {15958u, 29217u, 32024u, 32768u, 0u},
            {10207u, 24234u, 29958u, 32768u, 0u},
            {6929u, 18305u, 25652u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
      {
         {
            {4137u, 10847u, 15682u, 32768u, 0u},
            {17824u, 27001u, 30058u, 32768u, 0u},
            {10204u, 22796u, 28291u, 32768u, 0u},
            {6076u, 15935u, 22125u, 32768u, 0u},
            {3852u, 10937u, 16816u, 32768u, 0u},
            {2252u, 6324u, 10131u, 32768u, 0u},
            {25840u, 32016u, 32662u, 32768u, 0u},
            {15109u, 28268u, 31531u, 32768u, 0u},
            {9385u, 22231u, 28340u, 32768u, 0u},
            {6082u, 16672u, 23479u, 32768u, 0u},
            {3318u, 9427u, 14681u, 32768u, 0u},
            {30594u, 32574u, 32718u, 32768u, 0u},
            {16836u, 29552u, 31859u, 32768u, 0u},
            {9556u, 22542u, 28356u, 32768u, 0u},
            {6305u, 16725u, 23540u, 32768u, 0u},
            {3376u, 9895u, 15184u, 32768u, 0u},
            {29383u, 32617u, 32745u, 32768u, 0u},
            {18891u, 30809u, 32401u, 32768u, 0u},
            {11688u, 25942u, 30687u, 32768u, 0u},
            {7468u, 19469u, 26651u, 32768u, 0u},
            {3909u, 11358u, 17012u, 32768u, 0u},
            {31564u, 32736u, 32748u, 32768u, 0u},
            {20906u, 31611u, 32600u, 32768u, 0u},
            {13191u, 27621u, 31537u, 32768u, 0u},
            {8768u, 22029u, 28676u, 32768u, 0u},
            {5079u, 14109u, 20906u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
};

/* Coeff LPS/BR CDF [4 Q_CTX][5 TX_SIZES][2 plane][21 ctx][5] (4200 values) */
static unsigned short stbi_avif__av1_coeff_br_cdf[4][5][2][21][5] = {
   {
      {
         {
            {14298u, 20718u, 24174u, 32768u, 0u},
            {12536u, 19601u, 23789u, 32768u, 0u},
            {8712u, 15051u, 19503u, 32768u, 0u},
            {6170u, 11327u, 15434u, 32768u, 0u},
            {4742u, 8926u, 12538u, 32768u, 0u},
            {3803u, 7317u, 10546u, 32768u, 0u},
            {1696u, 3317u, 4871u, 32768u, 0u},
            {14392u, 19951u, 22756u, 32768u, 0u},
            {15978u, 23218u, 26818u, 32768u, 0u},
            {12187u, 19474u, 23889u, 32768u, 0u},
            {9176u, 15640u, 20259u, 32768u, 0u},
            {7068u, 12655u, 17028u, 32768u, 0u},
            {5656u, 10442u, 14472u, 32768u, 0u},
            {2580u, 4992u, 7244u, 32768u, 0u},
            {12136u, 18049u, 21426u, 32768u, 0u},
            {13784u, 20721u, 24481u, 32768u, 0u},
            {10836u, 17621u, 21900u, 32768u, 0u},
            {8372u, 14444u, 18847u, 32768u, 0u},
            {6523u, 11779u, 16000u, 32768u, 0u},
            {5337u, 9898u, 13760u, 32768u, 0u},
            {3034u, 5860u, 8462u, 32768u, 0u},
         },
         {
            {15967u, 22905u, 26286u, 32768u, 0u},
            {13534u, 20654u, 24579u, 32768u, 0u},
            {9504u, 16092u, 20535u, 32768u, 0u},
            {6975u, 12568u, 16903u, 32768u, 0u},
            {5364u, 10091u, 14020u, 32768u, 0u},
            {4357u, 8370u, 11857u, 32768u, 0u},
            {2506u, 4934u, 7218u, 32768u, 0u},
            {23032u, 28815u, 30936u, 32768u, 0u},
            {19540u, 26704u, 29719u, 32768u, 0u},
            {15158u, 22969u, 27097u, 32768u, 0u},
            {11408u, 18865u, 23650u, 32768u, 0u},
            {8885u, 15448u, 20250u, 32768u, 0u},
            {7108u, 12853u, 17416u, 32768u, 0u},
            {4231u, 8041u, 11480u, 32768u, 0u},
            {19823u, 26490u, 29156u, 32768u, 0u},
            {18890u, 25929u, 28932u, 32768u, 0u},
            {15660u, 23491u, 27433u, 32768u, 0u},
            {12147u, 19776u, 24488u, 32768u, 0u},
            {9728u, 16774u, 21649u, 32768u, 0u},
            {7919u, 14277u, 19066u, 32768u, 0u},
            {5440u, 10170u, 14185u, 32768u, 0u},
         },
      },
      {
         {
            {14406u, 20862u, 24414u, 32768u, 0u},
            {11824u, 18907u, 23109u, 32768u, 0u},
            {8257u, 14393u, 18803u, 32768u, 0u},
            {5860u, 10747u, 14778u, 32768u, 0u},
            {4475u, 8486u, 11984u, 32768u, 0u},
            {3606u, 6954u, 10043u, 32768u, 0u},
            {1736u, 3410u, 5048u, 32768u, 0u},
            {14430u, 20046u, 22882u, 32768u, 0u},
            {15593u, 22899u, 26709u, 32768u, 0u},
            {12102u, 19368u, 23811u, 32768u, 0u},
            {9059u, 15584u, 20262u, 32768u, 0u},
            {6999u, 12603u, 17048u, 32768u, 0u},
            {5684u, 10497u, 14553u, 32768u, 0u},
            {2822u, 5438u, 7862u, 32768u, 0u},
            {15785u, 21585u, 24359u, 32768u, 0u},
            {18347u, 25229u, 28266u, 32768u, 0u},
            {14974u, 22487u, 26389u, 32768u, 0u},
            {11423u, 18681u, 23271u, 32768u, 0u},
            {8863u, 15350u, 20008u, 32768u, 0u},
            {7153u, 12852u, 17278u, 32768u, 0u},
            {3707u, 7036u, 9982u, 32768u, 0u},
         },
         {
            {15460u, 21696u, 25469u, 32768u, 0u},
            {12170u, 19249u, 23191u, 32768u, 0u},
            {8723u, 15027u, 19332u, 32768u, 0u},
            {6428u, 11704u, 15874u, 32768u, 0u},
            {4922u, 9292u, 13052u, 32768u, 0u},
            {4139u, 7695u, 11010u, 32768u, 0u},
            {2291u, 4508u, 6598u, 32768u, 0u},
            {19856u, 26920u, 29828u, 32768u, 0u},
            {17923u, 25289u, 28792u, 32768u, 0u},
            {14278u, 21968u, 26297u, 32768u, 0u},
            {10910u, 18136u, 22950u, 32768u, 0u},
            {8423u, 14815u, 19627u, 32768u, 0u},
            {6771u, 12283u, 16774u, 32768u, 0u},
            {4074u, 7750u, 11081u, 32768u, 0u},
            {19852u, 26074u, 28672u, 32768u, 0u},
            {19371u, 26110u, 28989u, 32768u, 0u},
            {16265u, 23873u, 27663u, 32768u, 0u},
            {12758u, 20378u, 24952u, 32768u, 0u},
            {10095u, 17098u, 21961u, 32768u, 0u},
            {8250u, 14628u, 19451u, 32768u, 0u},
            {5205u, 9745u, 13622u, 32768u, 0u},
         },
      },
      {
         {
            {10563u, 16233u, 19763u, 32768u, 0u},
            {9794u, 16022u, 19804u, 32768u, 0u},
            {6750u, 11945u, 15759u, 32768u, 0u},
            {4963u, 9186u, 12752u, 32768u, 0u},
            {3845u, 7435u, 10627u, 32768u, 0u},
            {3051u, 6085u, 8834u, 32768u, 0u},
            {1311u, 2596u, 3830u, 32768u, 0u},
            {11246u, 16404u, 19689u, 32768u, 0u},
            {12315u, 18911u, 22731u, 32768u, 0u},
            {10557u, 17095u, 21289u, 32768u, 0u},
            {8136u, 14006u, 18249u, 32768u, 0u},
            {6348u, 11474u, 15565u, 32768u, 0u},
            {5196u, 9655u, 13400u, 32768u, 0u},
            {2349u, 4526u, 6587u, 32768u, 0u},
            {13337u, 18730u, 21569u, 32768u, 0u},
            {19306u, 26071u, 28882u, 32768u, 0u},
            {15952u, 23540u, 27254u, 32768u, 0u},
            {12409u, 19934u, 24430u, 32768u, 0u},
            {9760u, 16706u, 21389u, 32768u, 0u},
            {8004u, 14220u, 18818u, 32768u, 0u},
            {4138u, 7794u, 10961u, 32768u, 0u},
         },
         {
            {10870u, 16684u, 20949u, 32768u, 0u},
            {9664u, 15230u, 18680u, 32768u, 0u},
            {6886u, 12109u, 15408u, 32768u, 0u},
            {4825u, 8900u, 12305u, 32768u, 0u},
            {3630u, 7162u, 10314u, 32768u, 0u},
            {3036u, 6429u, 9387u, 32768u, 0u},
            {1671u, 3296u, 4940u, 32768u, 0u},
            {13819u, 19159u, 23026u, 32768u, 0u},
            {11984u, 19108u, 23120u, 32768u, 0u},
            {10690u, 17210u, 21663u, 32768u, 0u},
            {7984u, 14154u, 18333u, 32768u, 0u},
            {6868u, 12294u, 16124u, 32768u, 0u},
            {5274u, 8994u, 12868u, 32768u, 0u},
            {2988u, 5771u, 8424u, 32768u, 0u},
            {19736u, 26647u, 29141u, 32768u, 0u},
            {18933u, 26070u, 28984u, 32768u, 0u},
            {15779u, 23048u, 27200u, 32768u, 0u},
            {12638u, 20061u, 24532u, 32768u, 0u},
            {10692u, 17545u, 22220u, 32768u, 0u},
            {9217u, 15251u, 20054u, 32768u, 0u},
            {5078u, 9284u, 12594u, 32768u, 0u},
         },
      },
      {
         {
            {2331u, 3662u, 5244u, 32768u, 0u},
            {2891u, 4771u, 6145u, 32768u, 0u},
            {4598u, 7623u, 9729u, 32768u, 0u},
            {3520u, 6845u, 9199u, 32768u, 0u},
            {3417u, 6119u, 9324u, 32768u, 0u},
            {2601u, 5412u, 7385u, 32768u, 0u},
            {600u, 1173u, 1744u, 32768u, 0u},
            {7672u, 13286u, 17469u, 32768u, 0u},
            {4232u, 7792u, 10793u, 32768u, 0u},
            {2915u, 5317u, 7397u, 32768u, 0u},
            {2318u, 4356u, 6152u, 32768u, 0u},
            {2127u, 4000u, 5554u, 32768u, 0u},
            {1850u, 3478u, 5275u, 32768u, 0u},
            {977u, 1933u, 2843u, 32768u, 0u},
            {18280u, 24387u, 27989u, 32768u, 0u},
            {15852u, 22671u, 26185u, 32768u, 0u},
            {13845u, 20951u, 24789u, 32768u, 0u},
            {11055u, 17966u, 22129u, 32768u, 0u},
            {9138u, 15422u, 19801u, 32768u, 0u},
            {7454u, 13145u, 17456u, 32768u, 0u},
            {3370u, 6393u, 9013u, 32768u, 0u},
         },
         {
            {5842u, 9229u, 10838u, 32768u, 0u},
            {2313u, 3491u, 4276u, 32768u, 0u},
            {2998u, 6104u, 7496u, 32768u, 0u},
            {2420u, 7447u, 9868u, 32768u, 0u},
            {3034u, 8495u, 10923u, 32768u, 0u},
            {4076u, 8937u, 10975u, 32768u, 0u},
            {1086u, 2370u, 3299u, 32768u, 0u},
            {9714u, 17254u, 20444u, 32768u, 0u},
            {8543u, 13698u, 17123u, 32768u, 0u},
            {4918u, 9007u, 11910u, 32768u, 0u},
            {4129u, 7532u, 10553u, 32768u, 0u},
            {2364u, 5533u, 8058u, 32768u, 0u},
            {1834u, 3546u, 5563u, 32768u, 0u},
            {1473u, 2908u, 4133u, 32768u, 0u},
            {15405u, 21193u, 25619u, 32768u, 0u},
            {15691u, 21952u, 26561u, 32768u, 0u},
            {12962u, 19194u, 24165u, 32768u, 0u},
            {10272u, 17855u, 22129u, 32768u, 0u},
            {8588u, 15270u, 20718u, 32768u, 0u},
            {8682u, 14669u, 19500u, 32768u, 0u},
            {4870u, 9636u, 13205u, 32768u, 0u},
         },
      },
      {
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {14995u, 21341u, 24749u, 32768u, 0u},
            {13158u, 20289u, 24601u, 32768u, 0u},
            {8941u, 15326u, 19876u, 32768u, 0u},
            {6297u, 11541u, 15807u, 32768u, 0u},
            {4817u, 9029u, 12776u, 32768u, 0u},
            {3731u, 7273u, 10627u, 32768u, 0u},
            {1847u, 3617u, 5354u, 32768u, 0u},
            {14472u, 19659u, 22343u, 32768u, 0u},
            {16806u, 24162u, 27533u, 32768u, 0u},
            {12900u, 20404u, 24713u, 32768u, 0u},
            {9411u, 16112u, 20797u, 32768u, 0u},
            {7056u, 12697u, 17148u, 32768u, 0u},
            {5544u, 10339u, 14460u, 32768u, 0u},
            {2954u, 5704u, 8319u, 32768u, 0u},
            {12464u, 18071u, 21354u, 32768u, 0u},
            {15482u, 22528u, 26034u, 32768u, 0u},
            {12070u, 19269u, 23624u, 32768u, 0u},
            {8953u, 15406u, 20106u, 32768u, 0u},
            {7027u, 12730u, 17220u, 32768u, 0u},
            {5887u, 10913u, 15140u, 32768u, 0u},
            {3793u, 7278u, 10447u, 32768u, 0u},
         },
         {
            {15571u, 22232u, 25749u, 32768u, 0u},
            {14506u, 21575u, 25374u, 32768u, 0u},
            {10189u, 17089u, 21569u, 32768u, 0u},
            {7316u, 13301u, 17915u, 32768u, 0u},
            {5783u, 10912u, 15190u, 32768u, 0u},
            {4760u, 9155u, 13088u, 32768u, 0u},
            {2993u, 5966u, 8774u, 32768u, 0u},
            {23424u, 28903u, 30778u, 32768u, 0u},
            {20775u, 27666u, 30290u, 32768u, 0u},
            {16474u, 24410u, 28299u, 32768u, 0u},
            {12471u, 20180u, 24987u, 32768u, 0u},
            {9410u, 16487u, 21439u, 32768u, 0u},
            {7536u, 13614u, 18529u, 32768u, 0u},
            {5048u, 9586u, 13549u, 32768u, 0u},
            {21090u, 27290u, 29756u, 32768u, 0u},
            {20796u, 27402u, 30026u, 32768u, 0u},
            {17819u, 25485u, 28969u, 32768u, 0u},
            {13860u, 21909u, 26462u, 32768u, 0u},
            {11002u, 18494u, 23529u, 32768u, 0u},
            {8953u, 15929u, 20897u, 32768u, 0u},
            {6448u, 11918u, 16454u, 32768u, 0u},
         },
      },
      {
         {
            {15999u, 22208u, 25449u, 32768u, 0u},
            {13050u, 19988u, 24122u, 32768u, 0u},
            {8594u, 14864u, 19378u, 32768u, 0u},
            {6033u, 11079u, 15238u, 32768u, 0u},
            {4554u, 8683u, 12347u, 32768u, 0u},
            {3672u, 7139u, 10337u, 32768u, 0u},
            {1900u, 3771u, 5576u, 32768u, 0u},
            {15788u, 21340u, 23949u, 32768u, 0u},
            {16825u, 24235u, 27758u, 32768u, 0u},
            {12873u, 20402u, 24810u, 32768u, 0u},
            {9590u, 16363u, 21094u, 32768u, 0u},
            {7352u, 13209u, 17733u, 32768u, 0u},
            {5960u, 10989u, 15184u, 32768u, 0u},
            {3232u, 6234u, 9007u, 32768u, 0u},
            {15761u, 20716u, 23224u, 32768u, 0u},
            {19318u, 25989u, 28759u, 32768u, 0u},
            {15529u, 23094u, 26929u, 32768u, 0u},
            {11662u, 18989u, 23641u, 32768u, 0u},
            {8955u, 15568u, 20366u, 32768u, 0u},
            {7281u, 13106u, 17708u, 32768u, 0u},
            {4248u, 8059u, 11440u, 32768u, 0u},
         },
         {
            {14899u, 21217u, 24503u, 32768u, 0u},
            {13519u, 20283u, 24047u, 32768u, 0u},
            {9429u, 15966u, 20365u, 32768u, 0u},
            {6700u, 12355u, 16652u, 32768u, 0u},
            {5088u, 9704u, 13716u, 32768u, 0u},
            {4243u, 8154u, 11731u, 32768u, 0u},
            {2702u, 5364u, 7861u, 32768u, 0u},
            {22745u, 28388u, 30454u, 32768u, 0u},
            {20235u, 27146u, 29922u, 32768u, 0u},
            {15896u, 23715u, 27637u, 32768u, 0u},
            {11840u, 19350u, 24131u, 32768u, 0u},
            {9122u, 15932u, 20880u, 32768u, 0u},
            {7488u, 13581u, 18362u, 32768u, 0u},
            {5114u, 9568u, 13370u, 32768u, 0u},
            {20845u, 26553u, 28932u, 32768u, 0u},
            {20981u, 27372u, 29884u, 32768u, 0u},
            {17781u, 25335u, 28785u, 32768u, 0u},
            {13760u, 21708u, 26297u, 32768u, 0u},
            {10975u, 18415u, 23365u, 32768u, 0u},
            {9045u, 15789u, 20686u, 32768u, 0u},
            {6130u, 11199u, 15423u, 32768u, 0u},
         },
      },
      {
         {
            {13549u, 19724u, 23158u, 32768u, 0u},
            {11844u, 18382u, 22246u, 32768u, 0u},
            {7919u, 13619u, 17773u, 32768u, 0u},
            {5486u, 10143u, 13946u, 32768u, 0u},
            {4166u, 7983u, 11324u, 32768u, 0u},
            {3364u, 6506u, 9427u, 32768u, 0u},
            {1598u, 3160u, 4674u, 32768u, 0u},
            {15281u, 20979u, 23781u, 32768u, 0u},
            {14939u, 22119u, 25952u, 32768u, 0u},
            {11363u, 18407u, 22812u, 32768u, 0u},
            {8609u, 14857u, 19370u, 32768u, 0u},
            {6737u, 12184u, 16480u, 32768u, 0u},
            {5506u, 10263u, 14262u, 32768u, 0u},
            {2990u, 5786u, 8380u, 32768u, 0u},
            {20249u, 25253u, 27417u, 32768u, 0u},
            {21070u, 27518u, 30001u, 32768u, 0u},
            {16854u, 24469u, 28074u, 32768u, 0u},
            {12864u, 20486u, 25000u, 32768u, 0u},
            {9962u, 16978u, 21778u, 32768u, 0u},
            {8074u, 14338u, 19048u, 32768u, 0u},
            {4494u, 8479u, 11906u, 32768u, 0u},
         },
         {
            {13960u, 19617u, 22829u, 32768u, 0u},
            {11150u, 17341u, 21228u, 32768u, 0u},
            {7150u, 12964u, 17190u, 32768u, 0u},
            {5331u, 10002u, 13867u, 32768u, 0u},
            {4167u, 7744u, 11057u, 32768u, 0u},
            {3480u, 6629u, 9646u, 32768u, 0u},
            {1883u, 3784u, 5686u, 32768u, 0u},
            {18752u, 25660u, 28912u, 32768u, 0u},
            {16968u, 24586u, 28030u, 32768u, 0u},
            {13520u, 21055u, 25313u, 32768u, 0u},
            {10453u, 17626u, 22280u, 32768u, 0u},
            {8386u, 14505u, 19116u, 32768u, 0u},
            {6742u, 12595u, 17008u, 32768u, 0u},
            {4273u, 8140u, 11499u, 32768u, 0u},
            {22120u, 27827u, 30233u, 32768u, 0u},
            {20563u, 27358u, 29895u, 32768u, 0u},
            {17076u, 24644u, 28153u, 32768u, 0u},
            {13362u, 20942u, 25309u, 32768u, 0u},
            {10794u, 17965u, 22695u, 32768u, 0u},
            {9014u, 15652u, 20319u, 32768u, 0u},
            {5708u, 10512u, 14497u, 32768u, 0u},
         },
      },
      {
         {
            {5705u, 10930u, 15725u, 32768u, 0u},
            {7946u, 12765u, 16115u, 32768u, 0u},
            {6801u, 12123u, 16226u, 32768u, 0u},
            {5462u, 10135u, 14200u, 32768u, 0u},
            {4189u, 8011u, 11507u, 32768u, 0u},
            {3191u, 6229u, 9408u, 32768u, 0u},
            {1057u, 2137u, 3212u, 32768u, 0u},
            {10018u, 17067u, 21491u, 32768u, 0u},
            {7380u, 12582u, 16453u, 32768u, 0u},
            {6068u, 10845u, 14339u, 32768u, 0u},
            {5098u, 9198u, 12555u, 32768u, 0u},
            {4312u, 8010u, 11119u, 32768u, 0u},
            {3700u, 6966u, 9781u, 32768u, 0u},
            {1693u, 3326u, 4887u, 32768u, 0u},
            {18757u, 24930u, 27774u, 32768u, 0u},
            {17648u, 24596u, 27817u, 32768u, 0u},
            {14707u, 22052u, 26026u, 32768u, 0u},
            {11720u, 18852u, 23292u, 32768u, 0u},
            {9357u, 15952u, 20525u, 32768u, 0u},
            {7810u, 13753u, 18210u, 32768u, 0u},
            {3879u, 7333u, 10328u, 32768u, 0u},
         },
         {
            {8278u, 13242u, 15922u, 32768u, 0u},
            {10547u, 15867u, 18919u, 32768u, 0u},
            {9106u, 15842u, 20609u, 32768u, 0u},
            {6833u, 13007u, 17218u, 32768u, 0u},
            {4811u, 9712u, 13923u, 32768u, 0u},
            {3985u, 7352u, 11128u, 32768u, 0u},
            {1688u, 3458u, 5262u, 32768u, 0u},
            {12951u, 21861u, 26510u, 32768u, 0u},
            {9788u, 16044u, 20276u, 32768u, 0u},
            {6309u, 11244u, 14870u, 32768u, 0u},
            {5183u, 9349u, 12566u, 32768u, 0u},
            {4389u, 8229u, 11492u, 32768u, 0u},
            {3633u, 6945u, 10620u, 32768u, 0u},
            {3600u, 6847u, 9907u, 32768u, 0u},
            {21748u, 28137u, 30255u, 32768u, 0u},
            {19436u, 26581u, 29560u, 32768u, 0u},
            {16359u, 24201u, 27953u, 32768u, 0u},
            {13961u, 21693u, 25871u, 32768u, 0u},
            {11544u, 18686u, 23322u, 32768u, 0u},
            {9372u, 16462u, 20952u, 32768u, 0u},
            {6138u, 11210u, 15390u, 32768u, 0u},
         },
      },
      {
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {16138u, 22223u, 25509u, 32768u, 0u},
            {15347u, 22430u, 26332u, 32768u, 0u},
            {9614u, 16736u, 21332u, 32768u, 0u},
            {6600u, 12275u, 16907u, 32768u, 0u},
            {4811u, 9424u, 13547u, 32768u, 0u},
            {3748u, 7809u, 11420u, 32768u, 0u},
            {2254u, 4587u, 6890u, 32768u, 0u},
            {15196u, 20284u, 23177u, 32768u, 0u},
            {18317u, 25469u, 28451u, 32768u, 0u},
            {13918u, 21651u, 25842u, 32768u, 0u},
            {10052u, 17150u, 21995u, 32768u, 0u},
            {7499u, 13630u, 18587u, 32768u, 0u},
            {6158u, 11417u, 16003u, 32768u, 0u},
            {4014u, 7785u, 11252u, 32768u, 0u},
            {15048u, 21067u, 24384u, 32768u, 0u},
            {18202u, 25346u, 28553u, 32768u, 0u},
            {14302u, 22019u, 26356u, 32768u, 0u},
            {10839u, 18139u, 23166u, 32768u, 0u},
            {8715u, 15744u, 20806u, 32768u, 0u},
            {7536u, 13576u, 18544u, 32768u, 0u},
            {5413u, 10335u, 14498u, 32768u, 0u},
         },
         {
            {17394u, 24501u, 27895u, 32768u, 0u},
            {15889u, 23420u, 27185u, 32768u, 0u},
            {11561u, 19133u, 23870u, 32768u, 0u},
            {8285u, 14812u, 19844u, 32768u, 0u},
            {6496u, 12043u, 16550u, 32768u, 0u},
            {4771u, 9574u, 13677u, 32768u, 0u},
            {3603u, 6830u, 10144u, 32768u, 0u},
            {21656u, 27704u, 30200u, 32768u, 0u},
            {21324u, 27915u, 30511u, 32768u, 0u},
            {17327u, 25336u, 28997u, 32768u, 0u},
            {13417u, 21381u, 26033u, 32768u, 0u},
            {10132u, 17425u, 22338u, 32768u, 0u},
            {8580u, 15016u, 19633u, 32768u, 0u},
            {5694u, 11477u, 16411u, 32768u, 0u},
            {24116u, 29780u, 31450u, 32768u, 0u},
            {23853u, 29695u, 31591u, 32768u, 0u},
            {20085u, 27614u, 30428u, 32768u, 0u},
            {15326u, 24335u, 28575u, 32768u, 0u},
            {11814u, 19472u, 24810u, 32768u, 0u},
            {10221u, 18611u, 24767u, 32768u, 0u},
            {7689u, 14558u, 20321u, 32768u, 0u},
         },
      },
      {
         {
            {16214u, 22380u, 25770u, 32768u, 0u},
            {14213u, 21304u, 25295u, 32768u, 0u},
            {9213u, 15823u, 20455u, 32768u, 0u},
            {6395u, 11758u, 16139u, 32768u, 0u},
            {4779u, 9187u, 13066u, 32768u, 0u},
            {3821u, 7501u, 10953u, 32768u, 0u},
            {2293u, 4567u, 6795u, 32768u, 0u},
            {15859u, 21283u, 23820u, 32768u, 0u},
            {18404u, 25602u, 28726u, 32768u, 0u},
            {14325u, 21980u, 26206u, 32768u, 0u},
            {10669u, 17937u, 22720u, 32768u, 0u},
            {8297u, 14642u, 19447u, 32768u, 0u},
            {6746u, 12389u, 16893u, 32768u, 0u},
            {4324u, 8251u, 11770u, 32768u, 0u},
            {16532u, 21631u, 24475u, 32768u, 0u},
            {20667u, 27150u, 29668u, 32768u, 0u},
            {16728u, 24510u, 28175u, 32768u, 0u},
            {12861u, 20645u, 25332u, 32768u, 0u},
            {10076u, 17361u, 22417u, 32768u, 0u},
            {8395u, 14940u, 19963u, 32768u, 0u},
            {5731u, 10683u, 14912u, 32768u, 0u},
         },
         {
            {14433u, 21155u, 24938u, 32768u, 0u},
            {14658u, 21716u, 25545u, 32768u, 0u},
            {9923u, 16824u, 21557u, 32768u, 0u},
            {6982u, 13052u, 17721u, 32768u, 0u},
            {5419u, 10503u, 15050u, 32768u, 0u},
            {4852u, 9162u, 13014u, 32768u, 0u},
            {3271u, 6395u, 9630u, 32768u, 0u},
            {22210u, 27833u, 30109u, 32768u, 0u},
            {20750u, 27368u, 29821u, 32768u, 0u},
            {16894u, 24828u, 28573u, 32768u, 0u},
            {13247u, 21276u, 25757u, 32768u, 0u},
            {10038u, 17265u, 22563u, 32768u, 0u},
            {8587u, 14947u, 20327u, 32768u, 0u},
            {5645u, 11371u, 15252u, 32768u, 0u},
            {22027u, 27526u, 29714u, 32768u, 0u},
            {23098u, 29146u, 31221u, 32768u, 0u},
            {19886u, 27341u, 30272u, 32768u, 0u},
            {15609u, 23747u, 28046u, 32768u, 0u},
            {11993u, 20065u, 24939u, 32768u, 0u},
            {9637u, 18267u, 23671u, 32768u, 0u},
            {7625u, 13801u, 19144u, 32768u, 0u},
         },
      },
      {
         {
            {14438u, 20798u, 24089u, 32768u, 0u},
            {12621u, 19203u, 23097u, 32768u, 0u},
            {8177u, 14125u, 18402u, 32768u, 0u},
            {5674u, 10501u, 14456u, 32768u, 0u},
            {4236u, 8239u, 11733u, 32768u, 0u},
            {3447u, 6750u, 9806u, 32768u, 0u},
            {1986u, 3950u, 5864u, 32768u, 0u},
            {16208u, 22099u, 24930u, 32768u, 0u},
            {16537u, 24025u, 27585u, 32768u, 0u},
            {12780u, 20381u, 24867u, 32768u, 0u},
            {9767u, 16612u, 21416u, 32768u, 0u},
            {7686u, 13738u, 18398u, 32768u, 0u},
            {6333u, 11614u, 15964u, 32768u, 0u},
            {3941u, 7571u, 10836u, 32768u, 0u},
            {22819u, 27422u, 29202u, 32768u, 0u},
            {22224u, 28514u, 30721u, 32768u, 0u},
            {17660u, 25433u, 28913u, 32768u, 0u},
            {13574u, 21482u, 26002u, 32768u, 0u},
            {10629u, 17977u, 22938u, 32768u, 0u},
            {8612u, 15298u, 20265u, 32768u, 0u},
            {5607u, 10491u, 14596u, 32768u, 0u},
         },
         {
            {13569u, 19800u, 23206u, 32768u, 0u},
            {13128u, 19924u, 23869u, 32768u, 0u},
            {8329u, 14841u, 19403u, 32768u, 0u},
            {6130u, 10976u, 15057u, 32768u, 0u},
            {4682u, 8839u, 12518u, 32768u, 0u},
            {3656u, 7409u, 10588u, 32768u, 0u},
            {2577u, 5099u, 7412u, 32768u, 0u},
            {22427u, 28684u, 30585u, 32768u, 0u},
            {20913u, 27750u, 30139u, 32768u, 0u},
            {15840u, 24109u, 27834u, 32768u, 0u},
            {12308u, 20029u, 24569u, 32768u, 0u},
            {10216u, 16785u, 21458u, 32768u, 0u},
            {8309u, 14203u, 19113u, 32768u, 0u},
            {6043u, 11168u, 15307u, 32768u, 0u},
            {23166u, 28901u, 30998u, 32768u, 0u},
            {21899u, 28405u, 30751u, 32768u, 0u},
            {18413u, 26091u, 29443u, 32768u, 0u},
            {15233u, 23114u, 27352u, 32768u, 0u},
            {12683u, 20472u, 25288u, 32768u, 0u},
            {10702u, 18259u, 23409u, 32768u, 0u},
            {8125u, 14464u, 19226u, 32768u, 0u},
         },
      },
      {
         {
            {9040u, 14786u, 18360u, 32768u, 0u},
            {9979u, 15718u, 19415u, 32768u, 0u},
            {7913u, 13918u, 18311u, 32768u, 0u},
            {5859u, 10889u, 15184u, 32768u, 0u},
            {4593u, 8677u, 12510u, 32768u, 0u},
            {3820u, 7396u, 10791u, 32768u, 0u},
            {1730u, 3471u, 5192u, 32768u, 0u},
            {11803u, 18365u, 22709u, 32768u, 0u},
            {11419u, 18058u, 22225u, 32768u, 0u},
            {9418u, 15774u, 20243u, 32768u, 0u},
            {7539u, 13325u, 17657u, 32768u, 0u},
            {6233u, 11317u, 15384u, 32768u, 0u},
            {5137u, 9656u, 13545u, 32768u, 0u},
            {2977u, 5774u, 8349u, 32768u, 0u},
            {21207u, 27246u, 29640u, 32768u, 0u},
            {19547u, 26578u, 29497u, 32768u, 0u},
            {16169u, 23871u, 27690u, 32768u, 0u},
            {12820u, 20458u, 25018u, 32768u, 0u},
            {10224u, 17332u, 22214u, 32768u, 0u},
            {8526u, 15048u, 19884u, 32768u, 0u},
            {5037u, 9410u, 13118u, 32768u, 0u},
         },
         {
            {12339u, 17329u, 20140u, 32768u, 0u},
            {13505u, 19895u, 23225u, 32768u, 0u},
            {9847u, 16944u, 21564u, 32768u, 0u},
            {7280u, 13256u, 18348u, 32768u, 0u},
            {4712u, 10009u, 14454u, 32768u, 0u},
            {4361u, 7914u, 12477u, 32768u, 0u},
            {2870u, 5628u, 7995u, 32768u, 0u},
            {20061u, 25504u, 28526u, 32768u, 0u},
            {15235u, 22878u, 26145u, 32768u, 0u},
            {12985u, 19958u, 24155u, 32768u, 0u},
            {9782u, 16641u, 21403u, 32768u, 0u},
            {9456u, 16360u, 20760u, 32768u, 0u},
            {6855u, 12940u, 18557u, 32768u, 0u},
            {5661u, 10564u, 15002u, 32768u, 0u},
            {25656u, 30602u, 31894u, 32768u, 0u},
            {22570u, 29107u, 31092u, 32768u, 0u},
            {18917u, 26423u, 29541u, 32768u, 0u},
            {15940u, 23649u, 27754u, 32768u, 0u},
            {12803u, 20581u, 25219u, 32768u, 0u},
            {11082u, 18695u, 23376u, 32768u, 0u},
            {7939u, 14373u, 19005u, 32768u, 0u},
         },
      },
      {
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
   {
      {
         {
            {18315u, 24289u, 27551u, 32768u, 0u},
            {16854u, 24068u, 27835u, 32768u, 0u},
            {10140u, 17927u, 23173u, 32768u, 0u},
            {6722u, 12982u, 18267u, 32768u, 0u},
            {4661u, 9826u, 14706u, 32768u, 0u},
            {3832u, 8165u, 12294u, 32768u, 0u},
            {2795u, 6098u, 9245u, 32768u, 0u},
            {17145u, 23326u, 26672u, 32768u, 0u},
            {20733u, 27680u, 30308u, 32768u, 0u},
            {16032u, 24461u, 28546u, 32768u, 0u},
            {11653u, 20093u, 25081u, 32768u, 0u},
            {9290u, 16429u, 22086u, 32768u, 0u},
            {7796u, 14598u, 19982u, 32768u, 0u},
            {6502u, 12378u, 17441u, 32768u, 0u},
            {21681u, 27732u, 30320u, 32768u, 0u},
            {22389u, 29044u, 31261u, 32768u, 0u},
            {19027u, 26731u, 30087u, 32768u, 0u},
            {14739u, 23755u, 28624u, 32768u, 0u},
            {11358u, 20778u, 25511u, 32768u, 0u},
            {10995u, 18073u, 24190u, 32768u, 0u},
            {9162u, 14990u, 20617u, 32768u, 0u},
         },
         {
            {21425u, 27952u, 30388u, 32768u, 0u},
            {18062u, 25838u, 29034u, 32768u, 0u},
            {11956u, 19881u, 24808u, 32768u, 0u},
            {7718u, 15000u, 20980u, 32768u, 0u},
            {5702u, 11254u, 16143u, 32768u, 0u},
            {4898u, 9088u, 16864u, 32768u, 0u},
            {3679u, 6776u, 11907u, 32768u, 0u},
            {23294u, 30160u, 31663u, 32768u, 0u},
            {24397u, 29896u, 31836u, 32768u, 0u},
            {19245u, 27128u, 30593u, 32768u, 0u},
            {13202u, 19825u, 26404u, 32768u, 0u},
            {11578u, 19297u, 23957u, 32768u, 0u},
            {8073u, 13297u, 21370u, 32768u, 0u},
            {5461u, 10923u, 19745u, 32768u, 0u},
            {27367u, 30521u, 31934u, 32768u, 0u},
            {24904u, 30671u, 31940u, 32768u, 0u},
            {23075u, 28460u, 31299u, 32768u, 0u},
            {14400u, 23658u, 30417u, 32768u, 0u},
            {13885u, 23882u, 28325u, 32768u, 0u},
            {14746u, 22938u, 27853u, 32768u, 0u},
            {5461u, 16384u, 27307u, 32768u, 0u},
         },
      },
      {
         {
            {18274u, 24813u, 27890u, 32768u, 0u},
            {15537u, 23149u, 27003u, 32768u, 0u},
            {9449u, 16740u, 21827u, 32768u, 0u},
            {6700u, 12498u, 17261u, 32768u, 0u},
            {4988u, 9866u, 14198u, 32768u, 0u},
            {4236u, 8147u, 11902u, 32768u, 0u},
            {2867u, 5860u, 8654u, 32768u, 0u},
            {17124u, 23171u, 26101u, 32768u, 0u},
            {20396u, 27477u, 30148u, 32768u, 0u},
            {16573u, 24629u, 28492u, 32768u, 0u},
            {12749u, 20846u, 25674u, 32768u, 0u},
            {10233u, 17878u, 22818u, 32768u, 0u},
            {8525u, 15332u, 20363u, 32768u, 0u},
            {6283u, 11632u, 16255u, 32768u, 0u},
            {20466u, 26511u, 29286u, 32768u, 0u},
            {23059u, 29174u, 31191u, 32768u, 0u},
            {19481u, 27263u, 30241u, 32768u, 0u},
            {15458u, 23631u, 28137u, 32768u, 0u},
            {12416u, 20608u, 25693u, 32768u, 0u},
            {10261u, 18011u, 23261u, 32768u, 0u},
            {8016u, 14655u, 19666u, 32768u, 0u},
         },
         {
            {17616u, 24586u, 28112u, 32768u, 0u},
            {15809u, 23299u, 27155u, 32768u, 0u},
            {10767u, 18890u, 23793u, 32768u, 0u},
            {7727u, 14255u, 18865u, 32768u, 0u},
            {6129u, 11926u, 16882u, 32768u, 0u},
            {4482u, 9704u, 14861u, 32768u, 0u},
            {3277u, 7452u, 11522u, 32768u, 0u},
            {22956u, 28551u, 30730u, 32768u, 0u},
            {22724u, 28937u, 30961u, 32768u, 0u},
            {18467u, 26324u, 29580u, 32768u, 0u},
            {13234u, 20713u, 25649u, 32768u, 0u},
            {11181u, 17592u, 22481u, 32768u, 0u},
            {8291u, 18358u, 24576u, 32768u, 0u},
            {7568u, 11881u, 14984u, 32768u, 0u},
            {24948u, 29001u, 31147u, 32768u, 0u},
            {25674u, 30619u, 32151u, 32768u, 0u},
            {20841u, 26793u, 29603u, 32768u, 0u},
            {14669u, 24356u, 28666u, 32768u, 0u},
            {11334u, 23593u, 28219u, 32768u, 0u},
            {8922u, 14762u, 22873u, 32768u, 0u},
            {8301u, 13544u, 20535u, 32768u, 0u},
         },
      },
      {
         {
            {17113u, 23733u, 27081u, 32768u, 0u},
            {14139u, 21406u, 25452u, 32768u, 0u},
            {8552u, 15002u, 19776u, 32768u, 0u},
            {5871u, 11120u, 15378u, 32768u, 0u},
            {4455u, 8616u, 12253u, 32768u, 0u},
            {3469u, 6910u, 10386u, 32768u, 0u},
            {2255u, 4553u, 6782u, 32768u, 0u},
            {18224u, 24376u, 27053u, 32768u, 0u},
            {19290u, 26710u, 29614u, 32768u, 0u},
            {14936u, 22991u, 27184u, 32768u, 0u},
            {11238u, 18951u, 23762u, 32768u, 0u},
            {8786u, 15617u, 20588u, 32768u, 0u},
            {7317u, 13228u, 18003u, 32768u, 0u},
            {5101u, 9512u, 13493u, 32768u, 0u},
            {22639u, 28222u, 30210u, 32768u, 0u},
            {23216u, 29331u, 31307u, 32768u, 0u},
            {19075u, 26762u, 29895u, 32768u, 0u},
            {15014u, 23113u, 27457u, 32768u, 0u},
            {11938u, 19857u, 24752u, 32768u, 0u},
            {9942u, 17280u, 22282u, 32768u, 0u},
            {7167u, 13144u, 17752u, 32768u, 0u},
         },
         {
            {15820u, 22738u, 26488u, 32768u, 0u},
            {13530u, 20885u, 25216u, 32768u, 0u},
            {8395u, 15530u, 20452u, 32768u, 0u},
            {6574u, 12321u, 16380u, 32768u, 0u},
            {5353u, 10419u, 14568u, 32768u, 0u},
            {4613u, 8446u, 12381u, 32768u, 0u},
            {3440u, 7158u, 9903u, 32768u, 0u},
            {24247u, 29051u, 31224u, 32768u, 0u},
            {22118u, 28058u, 30369u, 32768u, 0u},
            {16498u, 24768u, 28389u, 32768u, 0u},
            {12920u, 21175u, 26137u, 32768u, 0u},
            {10730u, 18619u, 25352u, 32768u, 0u},
            {10187u, 16279u, 22791u, 32768u, 0u},
            {9310u, 14631u, 22127u, 32768u, 0u},
            {24970u, 30558u, 32057u, 32768u, 0u},
            {24801u, 29942u, 31698u, 32768u, 0u},
            {22432u, 28453u, 30855u, 32768u, 0u},
            {19054u, 25680u, 29580u, 32768u, 0u},
            {14392u, 23036u, 28109u, 32768u, 0u},
            {12495u, 20947u, 26650u, 32768u, 0u},
            {12442u, 20326u, 26214u, 32768u, 0u},
         },
      },
      {
         {
            {12162u, 18785u, 22648u, 32768u, 0u},
            {12749u, 19697u, 23806u, 32768u, 0u},
            {8580u, 15297u, 20346u, 32768u, 0u},
            {6169u, 11749u, 16543u, 32768u, 0u},
            {4836u, 9391u, 13448u, 32768u, 0u},
            {3821u, 7711u, 11613u, 32768u, 0u},
            {2228u, 4601u, 7070u, 32768u, 0u},
            {16319u, 24725u, 28280u, 32768u, 0u},
            {15698u, 23277u, 27168u, 32768u, 0u},
            {12726u, 20368u, 25047u, 32768u, 0u},
            {9912u, 17015u, 21976u, 32768u, 0u},
            {7888u, 14220u, 19179u, 32768u, 0u},
            {6777u, 12284u, 17018u, 32768u, 0u},
            {4492u, 8590u, 12252u, 32768u, 0u},
            {23249u, 28904u, 30947u, 32768u, 0u},
            {21050u, 27908u, 30512u, 32768u, 0u},
            {17440u, 25340u, 28949u, 32768u, 0u},
            {14059u, 22018u, 26541u, 32768u, 0u},
            {11288u, 18903u, 23898u, 32768u, 0u},
            {9411u, 16342u, 21428u, 32768u, 0u},
            {6278u, 11588u, 15944u, 32768u, 0u},
         },
         {
            {13981u, 20067u, 23226u, 32768u, 0u},
            {16922u, 23580u, 26783u, 32768u, 0u},
            {11005u, 19039u, 24487u, 32768u, 0u},
            {7389u, 14218u, 19798u, 32768u, 0u},
            {5598u, 11505u, 17206u, 32768u, 0u},
            {6090u, 11213u, 15659u, 32768u, 0u},
            {3820u, 7371u, 10119u, 32768u, 0u},
            {21082u, 26925u, 29675u, 32768u, 0u},
            {21262u, 28627u, 31128u, 32768u, 0u},
            {18392u, 26454u, 30437u, 32768u, 0u},
            {14870u, 22910u, 27096u, 32768u, 0u},
            {12620u, 19484u, 24908u, 32768u, 0u},
            {9290u, 16553u, 22802u, 32768u, 0u},
            {6668u, 14288u, 20004u, 32768u, 0u},
            {27704u, 31055u, 31949u, 32768u, 0u},
            {24709u, 29978u, 31788u, 32768u, 0u},
            {21668u, 29264u, 31657u, 32768u, 0u},
            {18295u, 26968u, 30074u, 32768u, 0u},
            {16399u, 24422u, 29313u, 32768u, 0u},
            {14347u, 23026u, 28104u, 32768u, 0u},
            {12370u, 19806u, 24477u, 32768u, 0u},
         },
      },
      {
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
         {
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
            {8192u, 16384u, 24576u, 32768u, 0u},
         },
      },
   },
};


/* AV1 DC quantizer lookup (8-bit) [256 entries] */
static const short stbi_avif__av1_dc_qlookup[256] = {
   4, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19,
   20, 21, 22, 23, 24, 25, 26, 26, 27, 28, 29, 30, 31, 32, 32, 33,
   34, 35, 36, 37, 38, 38, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47,
   48, 48, 49, 50, 51, 52, 53, 53, 54, 55, 56, 57, 57, 58, 59, 60,
   61, 62, 62, 63, 64, 65, 66, 66, 67, 68, 69, 70, 70, 71, 72, 73,
   74, 74, 75, 76, 77, 78, 78, 79, 80, 81, 81, 82, 83, 84, 85, 85,
   87, 88, 90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 108, 110,
   111, 113, 114, 116, 117, 118, 120, 121, 123, 125, 127, 129, 131, 134, 136, 138,
   140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 161, 164, 166, 169, 172, 174,
   177, 180, 182, 185, 187, 190, 192, 195, 199, 202, 205, 208, 211, 214, 217, 220,
   223, 226, 230, 233, 237, 240, 243, 247, 250, 253, 257, 261, 265, 269, 272, 276,
   280, 284, 288, 292, 296, 300, 304, 309, 313, 317, 322, 326, 330, 335, 340, 344,
   349, 354, 359, 364, 369, 374, 379, 384, 389, 395, 400, 406, 411, 417, 423, 429,
   435, 441, 447, 454, 461, 467, 475, 482, 489, 497, 505, 513, 522, 530, 539, 549,
   559, 569, 579, 590, 602, 614, 626, 640, 654, 668, 684, 700, 717, 736, 755, 775,
   796, 819, 843, 869, 896, 925, 955, 988, 1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
};

/* AV1 AC quantizer lookup (8-bit) [256 entries] */
static const short stbi_avif__av1_ac_qlookup[256] = {
   4, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
   23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
   39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
   55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
   71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
   87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102,
   104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126, 128, 130, 132, 134,
   136, 138, 140, 142, 144, 146, 148, 150, 152, 155, 158, 161, 164, 167, 170, 173,
   176, 179, 182, 185, 188, 191, 194, 197, 200, 203, 207, 211, 215, 219, 223, 227,
   231, 235, 239, 243, 247, 251, 255, 260, 265, 270, 275, 280, 285, 290, 295, 300,
   305, 311, 317, 323, 329, 335, 341, 347, 353, 359, 366, 373, 380, 387, 394, 401,
   408, 416, 424, 432, 440, 448, 456, 465, 474, 483, 492, 501, 510, 520, 530, 540,
   550, 560, 571, 582, 593, 604, 615, 627, 639, 651, 663, 676, 689, 702, 715, 729,
   743, 757, 771, 786, 801, 816, 832, 848, 864, 881, 898, 915, 933, 951, 969, 988,
   1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151, 1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
   1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567, 1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};

/* AV1 DC quantizer lookup (10-bit) [256 entries] */
static const short stbi_avif__av1_dc_qlookup_10[256] = {
   4, 9, 10, 13, 15, 17, 20, 22, 25, 28, 31, 34, 37, 40, 43, 47,
   50, 53, 57, 60, 64, 68, 71, 75, 78, 82, 86, 90, 93, 97, 101, 105,
   109, 113, 116, 120, 124, 128, 132, 136, 140, 143, 147, 151, 155, 159, 163, 166,
   170, 174, 178, 182, 185, 189, 193, 197, 200, 204, 208, 212, 215, 219, 223, 226,
   230, 233, 237, 241, 244, 248, 251, 255, 259, 262, 266, 269, 273, 276, 280, 283,
   287, 290, 293, 297, 300, 304, 307, 310, 314, 317, 321, 324, 327, 331, 334, 337,
   343, 350, 356, 362, 369, 375, 381, 387, 394, 400, 406, 412, 418, 424, 430, 436,
   442, 448, 454, 460, 466, 472, 478, 484, 490, 499, 507, 516, 525, 533, 542, 550,
   559, 567, 576, 584, 592, 601, 609, 617, 625, 634, 644, 655, 666, 676, 687, 698,
   708, 718, 729, 739, 749, 759, 770, 782, 795, 807, 819, 831, 844, 856, 868, 880,
   891, 906, 920, 933, 947, 961, 975, 988, 1001, 1015, 1030, 1045, 1061, 1076, 1090, 1105,
   1120, 1137, 1153, 1170, 1186, 1202, 1218, 1236, 1253, 1271, 1288, 1306, 1323, 1342, 1361, 1379,
   1398, 1416, 1436, 1456, 1476, 1496, 1516, 1537, 1559, 1580, 1601, 1624, 1647, 1670, 1692, 1717,
   1741, 1766, 1791, 1817, 1844, 1871, 1900, 1929, 1958, 1990, 2021, 2054, 2088, 2123, 2159, 2197,
   2236, 2276, 2319, 2363, 2410, 2458, 2508, 2561, 2616, 2675, 2737, 2802, 2871, 2944, 3020, 3102,
   3188, 3280, 3375, 3478, 3586, 3702, 3823, 3953, 4089, 4236, 4394, 4559, 4737, 4929, 5130, 5347,
};

/* AV1 AC quantizer lookup (10-bit) [256 entries] */
static const short stbi_avif__av1_ac_qlookup_10[256] = {
   4, 9, 11, 13, 16, 18, 21, 24, 27, 30, 33, 37, 40, 44, 48, 51,
   55, 59, 63, 67, 71, 75, 79, 83, 88, 92, 96, 100, 105, 109, 114, 118,
   122, 127, 131, 136, 140, 145, 149, 154, 158, 163, 168, 172, 177, 181, 186, 190,
   195, 199, 204, 208, 213, 217, 222, 226, 231, 235, 240, 244, 249, 253, 258, 262,
   267, 271, 275, 280, 284, 289, 293, 297, 302, 306, 311, 315, 319, 324, 328, 332,
   337, 341, 345, 349, 354, 358, 362, 367, 371, 375, 379, 384, 388, 392, 396, 401,
   409, 417, 425, 433, 441, 449, 458, 466, 474, 482, 490, 498, 506, 514, 523, 531,
   539, 547, 555, 563, 571, 579, 588, 596, 604, 616, 628, 640, 652, 664, 676, 688,
   700, 713, 725, 737, 749, 761, 773, 785, 797, 809, 825, 841, 857, 873, 889, 905,
   922, 938, 954, 970, 986, 1002, 1018, 1038, 1058, 1078, 1098, 1118, 1138, 1158, 1178, 1198,
   1218, 1242, 1266, 1290, 1314, 1338, 1362, 1386, 1411, 1435, 1463, 1491, 1519, 1547, 1575, 1603,
   1631, 1663, 1695, 1727, 1759, 1791, 1823, 1859, 1895, 1931, 1967, 2003, 2039, 2079, 2119, 2159,
   2199, 2239, 2283, 2327, 2371, 2415, 2459, 2507, 2555, 2603, 2651, 2703, 2755, 2807, 2859, 2915,
   2971, 3027, 3083, 3143, 3203, 3263, 3327, 3391, 3455, 3523, 3591, 3659, 3731, 3803, 3876, 3952,
   4028, 4104, 4184, 4264, 4348, 4432, 4516, 4604, 4692, 4784, 4876, 4972, 5068, 5168, 5268, 5372,
   5476, 5584, 5692, 5804, 5916, 6032, 6148, 6268, 6388, 6512, 6640, 6768, 6900, 7036, 7172, 7312,
};

/* Default diagonal scan order for TX sizes */
static const unsigned short stbi_avif__av1_scan_4x4[16] = {
   0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15,
};

static const unsigned short stbi_avif__av1_scan_8x8[64] = {
   0, 8, 1, 2, 9, 16, 24, 17, 10, 3, 4, 11, 18, 25, 32, 40,
   33, 26, 19, 12, 5, 6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35,
   28, 21, 14, 7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30,
   23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63,
};

static const unsigned short stbi_avif__av1_scan_16x16[256] = {
   0, 16, 1, 2, 17, 32, 48, 33, 18, 3, 4, 19, 34, 49, 64, 80,
   65, 50, 35, 20, 5, 6, 21, 36, 51, 66, 81, 96, 112, 97, 82, 67,
   52, 37, 22, 7, 8, 23, 38, 53, 68, 83, 98, 113, 128, 144, 129, 114,
   99, 84, 69, 54, 39, 24, 9, 10, 25, 40, 55, 70, 85, 100, 115, 130,
   145, 160, 176, 161, 146, 131, 116, 101, 86, 71, 56, 41, 26, 11, 12, 27,
   42, 57, 72, 87, 102, 117, 132, 147, 162, 177, 192, 208, 193, 178, 163, 148,
   133, 118, 103, 88, 73, 58, 43, 28, 13, 14, 29, 44, 59, 74, 89, 104,
   119, 134, 149, 164, 179, 194, 209, 224, 240, 225, 210, 195, 180, 165, 150, 135,
   120, 105, 90, 75, 60, 45, 30, 15, 31, 46, 61, 76, 91, 106, 121, 136,
   151, 166, 181, 196, 211, 226, 241, 242, 227, 212, 197, 182, 167, 152, 137, 122,
   107, 92, 77, 62, 47, 63, 78, 93, 108, 123, 138, 153, 168, 183, 198, 213,
   228, 243, 244, 229, 214, 199, 184, 169, 154, 139, 124, 109, 94, 79, 95, 110,
   125, 140, 155, 170, 185, 200, 215, 230, 245, 246, 231, 216, 201, 186, 171, 156,
   141, 126, 111, 127, 142, 157, 172, 187, 202, 217, 232, 247, 248, 233, 218, 203,
   188, 173, 158, 143, 159, 174, 189, 204, 219, 234, 249, 250, 235, 220, 205, 190,
   175, 191, 206, 221, 236, 251, 252, 237, 222, 207, 223, 238, 253, 254, 239, 255,
};

static const unsigned short stbi_avif__av1_scan_32x32[1024] = {
   0, 32, 1, 2, 33, 64, 96, 65, 34, 3, 4, 35, 66, 97, 128, 160,
   129, 98, 67, 36, 5, 6, 37, 68, 99, 130, 161, 192, 224, 193, 162, 131,
   100, 69, 38, 7, 8, 39, 70, 101, 132, 163, 194, 225, 256, 288, 257, 226,
   195, 164, 133, 102, 71, 40, 9, 10, 41, 72, 103, 134, 165, 196, 227, 258,
   289, 320, 352, 321, 290, 259, 228, 197, 166, 135, 104, 73, 42, 11, 12, 43,
   74, 105, 136, 167, 198, 229, 260, 291, 322, 353, 384, 416, 385, 354, 323, 292,
   261, 230, 199, 168, 137, 106, 75, 44, 13, 14, 45, 76, 107, 138, 169, 200,
   231, 262, 293, 324, 355, 386, 417, 448, 480, 449, 418, 387, 356, 325, 294, 263,
   232, 201, 170, 139, 108, 77, 46, 15, 16, 47, 78, 109, 140, 171, 202, 233,
   264, 295, 326, 357, 388, 419, 450, 481, 512, 544, 513, 482, 451, 420, 389, 358,
   327, 296, 265, 234, 203, 172, 141, 110, 79, 48, 17, 18, 49, 80, 111, 142,
   173, 204, 235, 266, 297, 328, 359, 390, 421, 452, 483, 514, 545, 576, 608, 577,
   546, 515, 484, 453, 422, 391, 360, 329, 298, 267, 236, 205, 174, 143, 112, 81,
   50, 19, 20, 51, 82, 113, 144, 175, 206, 237, 268, 299, 330, 361, 392, 423,
   454, 485, 516, 547, 578, 609, 640, 672, 641, 610, 579, 548, 517, 486, 455, 424,
   393, 362, 331, 300, 269, 238, 207, 176, 145, 114, 83, 52, 21, 22, 53, 84,
   115, 146, 177, 208, 239, 270, 301, 332, 363, 394, 425, 456, 487, 518, 549, 580,
   611, 642, 673, 704, 736, 705, 674, 643, 612, 581, 550, 519, 488, 457, 426, 395,
   364, 333, 302, 271, 240, 209, 178, 147, 116, 85, 54, 23, 24, 55, 86, 117,
   148, 179, 210, 241, 272, 303, 334, 365, 396, 427, 458, 489, 520, 551, 582, 613,
   644, 675, 706, 737, 768, 800, 769, 738, 707, 676, 645, 614, 583, 552, 521, 490,
   459, 428, 397, 366, 335, 304, 273, 242, 211, 180, 149, 118, 87, 56, 25, 26,
   57, 88, 119, 150, 181, 212, 243, 274, 305, 336, 367, 398, 429, 460, 491, 522,
   553, 584, 615, 646, 677, 708, 739, 770, 801, 832, 864, 833, 802, 771, 740, 709,
   678, 647, 616, 585, 554, 523, 492, 461, 430, 399, 368, 337, 306, 275, 244, 213,
   182, 151, 120, 89, 58, 27, 28, 59, 90, 121, 152, 183, 214, 245, 276, 307,
   338, 369, 400, 431, 462, 493, 524, 555, 586, 617, 648, 679, 710, 741, 772, 803,
   834, 865, 896, 928, 897, 866, 835, 804, 773, 742, 711, 680, 649, 618, 587, 556,
   525, 494, 463, 432, 401, 370, 339, 308, 277, 246, 215, 184, 153, 122, 91, 60,
   29, 30, 61, 92, 123, 154, 185, 216, 247, 278, 309, 340, 371, 402, 433, 464,
   495, 526, 557, 588, 619, 650, 681, 712, 743, 774, 805, 836, 867, 898, 929, 960,
   992, 961, 930, 899, 868, 837, 806, 775, 744, 713, 682, 651, 620, 589, 558, 527,
   496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124, 93, 62, 31,
   63, 94, 125, 156, 187, 218, 249, 280, 311, 342, 373, 404, 435, 466, 497, 528,
   559, 590, 621, 652, 683, 714, 745, 776, 807, 838, 869, 900, 931, 962, 993, 994,
   963, 932, 901, 870, 839, 808, 777, 746, 715, 684, 653, 622, 591, 560, 529, 498,
   467, 436, 405, 374, 343, 312, 281, 250, 219, 188, 157, 126, 95, 127, 158, 189,
   220, 251, 282, 313, 344, 375, 406, 437, 468, 499, 530, 561, 592, 623, 654, 685,
   716, 747, 778, 809, 840, 871, 902, 933, 964, 995, 996, 965, 934, 903, 872, 841,
   810, 779, 748, 717, 686, 655, 624, 593, 562, 531, 500, 469, 438, 407, 376, 345,
   314, 283, 252, 221, 190, 159, 191, 222, 253, 284, 315, 346, 377, 408, 439, 470,
   501, 532, 563, 594, 625, 656, 687, 718, 749, 780, 811, 842, 873, 904, 935, 966,
   997, 998, 967, 936, 905, 874, 843, 812, 781, 750, 719, 688, 657, 626, 595, 564,
   533, 502, 471, 440, 409, 378, 347, 316, 285, 254, 223, 255, 286, 317, 348, 379,
   410, 441, 472, 503, 534, 565, 596, 627, 658, 689, 720, 751, 782, 813, 844, 875,
   906, 937, 968, 999, 1000, 969, 938, 907, 876, 845, 814, 783, 752, 721, 690, 659,
   628, 597, 566, 535, 504, 473, 442, 411, 380, 349, 318, 287, 319, 350, 381, 412,
   443, 474, 505, 536, 567, 598, 629, 660, 691, 722, 753, 784, 815, 846, 877, 908,
   939, 970, 1001, 1002, 971, 940, 909, 878, 847, 816, 785, 754, 723, 692, 661, 630,
   599, 568, 537, 506, 475, 444, 413, 382, 351, 383, 414, 445, 476, 507, 538, 569,
   600, 631, 662, 693, 724, 755, 786, 817, 848, 879, 910, 941, 972, 1003, 1004, 973,
   942, 911, 880, 849, 818, 787, 756, 725, 694, 663, 632, 601, 570, 539, 508, 477,
   446, 415, 447, 478, 509, 540, 571, 602, 633, 664, 695, 726, 757, 788, 819, 850,
   881, 912, 943, 974, 1005, 1006, 975, 944, 913, 882, 851, 820, 789, 758, 727, 696,
   665, 634, 603, 572, 541, 510, 479, 511, 542, 573, 604, 635, 666, 697, 728, 759,
   790, 821, 852, 883, 914, 945, 976, 1007, 1008, 977, 946, 915, 884, 853, 822, 791,
   760, 729, 698, 667, 636, 605, 574, 543, 575, 606, 637, 668, 699, 730, 761, 792,
   823, 854, 885, 916, 947, 978, 1009, 1010, 979, 948, 917, 886, 855, 824, 793, 762,
   731, 700, 669, 638, 607, 639, 670, 701, 732, 763, 794, 825, 856, 887, 918, 949,
   980, 1011, 1012, 981, 950, 919, 888, 857, 826, 795, 764, 733, 702, 671, 703, 734,
   765, 796, 827, 858, 889, 920, 951, 982, 1013, 1014, 983, 952, 921, 890, 859, 828,
   797, 766, 735, 767, 798, 829, 860, 891, 922, 953, 984, 1015, 1016, 985, 954, 923,
   892, 861, 830, 799, 831, 862, 893, 924, 955, 986, 1017, 1018, 987, 956, 925, 894,
   863, 895, 926, 957, 988, 1019, 1020, 989, 958, 927, 959, 990, 1021, 1022, 991, 1023,
};

/* TX type identifiers */
#define STBI_AVIF_TX_DCT_DCT        0
#define STBI_AVIF_TX_ADST_DCT       1
#define STBI_AVIF_TX_DCT_ADST       2
#define STBI_AVIF_TX_ADST_ADST      3
#define STBI_AVIF_TX_FLIPADST_DCT   4
#define STBI_AVIF_TX_DCT_FLIPADST   5
#define STBI_AVIF_TX_FLIPADST_FLIPADST 6
#define STBI_AVIF_TX_ADST_FLIPADST  7
#define STBI_AVIF_TX_FLIPADST_ADST  8
#define STBI_AVIF_TX_IDTX           9
#define STBI_AVIF_TX_V_DCT          10
#define STBI_AVIF_TX_H_DCT          11
#define STBI_AVIF_TX_V_ADST         12
#define STBI_AVIF_TX_H_ADST         13
#define STBI_AVIF_TX_V_FLIPADST     14
#define STBI_AVIF_TX_H_FLIPADST     15

/* TX size enum */
#define STBI_AVIF_TX_4X4   0
#define STBI_AVIF_TX_8X8   1
#define STBI_AVIF_TX_16X16 2
#define STBI_AVIF_TX_32X32 3
#define STBI_AVIF_TX_64X64 4

/* Mapping from TX size to ext_tx_set for intra */
/* Set 0: only DCT_DCT, Set 1: 7 types (DTT4_IDTX_1DDCT), Set 2: 5 types (DTT4_IDTX) */
static const int stbi_avif__av1_ext_tx_set_intra[5] = { 1, 1, 2, 0, 0 };
/* Number of TX types per set: set0=1, set1=7, set2=5 */
static const int stbi_avif__av1_num_tx_types[3] = { 1, 7, 5 };
/* av1_ext_tx_inv: CDF symbol → TX_TYPE for each ext_tx_set
 * Set 1 (DTT4_IDTX_1DDCT, 7 syms): from av1_ext_tx_inv[3]
 * Set 2 (DTT4_IDTX, 5 syms): from av1_ext_tx_inv[2] */
static const int stbi_avif__av1_ext_tx_inv_set1[7] = { 9, 0, 10, 11, 3, 1, 2 };
static const int stbi_avif__av1_ext_tx_inv_set2[5] = { 9, 0, 3, 1, 2 };

/* Map base_q_idx to TOKEN_CDF_Q_CTX (0-3) */
static int stbi_avif__av1_get_q_ctx(unsigned int base_q_idx)
{
   if (base_q_idx <= 20u) return 0;
   if (base_q_idx <= 60u) return 1;
   if (base_q_idx <= 120u) return 2;
   return 3;
}

/*
 * AV1 coefficient context functions — using padded level buffer.
 * The padded buffer has stride = (1 << bhl) + 4 (TX_PAD_HOR=4).
 * Conversion from raster index to padded index:
 *   padded_idx = idx + (idx >> bhl) * 4
 * i.e. each row of width (1<<bhl) is followed by 4 padding bytes.
 */
#define STBI_AVIF_TX_PAD_HOR 4

static int stbi_avif__av1_get_padded_idx(int idx, int bhl)
{
   return idx + ((idx >> bhl) << 2);
}

/* nz_map_ctx_offset tables for square TX sizes (from AOM reference) */
static const signed char stbi_avif__av1_nz_map_ctx_off_4x4[16] = {
  0, 1, 6, 6, 1, 6, 6, 21, 6, 6, 21, 21, 6, 21, 21, 21
};
static const signed char stbi_avif__av1_nz_map_ctx_off_8x8[64] = {
  0,  1,  6,  6,  21, 21, 21, 21, 1,  6,  6,  21, 21, 21, 21, 21,
  6,  6,  21, 21, 21, 21, 21, 21, 6,  21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21
};
static const signed char stbi_avif__av1_nz_map_ctx_off_16x16[256] = {
  0,  1,  6,  6,  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  1,  6,  6,  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  6,  6,  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  6,  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21
};

/* For 32x32: compute ctx_offset on the fly (mostly 21) */
static int stbi_avif__av1_nz_map_ctx_off_32(int idx)
{
   int row = idx & 31, col = idx >> 5;
   if (idx == 0) return 0;
   if (row + col == 1) return 1;
   if (row < 2 && col < 2) return 6;
   if (row < 4 && col == 0) return 6;
   if (col < 4 && row == 0) return 6;
   return 21;
}

/* get_lower_levels_ctx_eob: context for the coefficient at the EOB position */
static int stbi_avif__av1_get_lower_levels_ctx_eob(int bhl, int width, int scan_idx)
{
   if (scan_idx == 0) return 0;
   if (scan_idx <= (width << bhl) / 8) return 1;
   if (scan_idx <= (width << bhl) / 4) return 2;
   return 3;
}

/* get_lower_levels_ctx_2d: context for coefficients at positions < EOB */
static int stbi_avif__av1_get_lower_levels_ctx_2d(const unsigned char *levels,
   int coeff_idx, int bhl, int txw)
{
   int padded_pos, mag, ctx;
   int offset;
   int txh = 1 << bhl;
   int stride = txh + STBI_AVIF_TX_PAD_HOR;
   int x = coeff_idx >> bhl;  /* column */
   int y = coeff_idx - (x << bhl);  /* row */
   int xcl = x < 4 ? x : 4;
   int ycl = y < 4 ? y : 4;
   /* dav1d-style 5x5 offset table indexed by [shape][y_clamped][x_clamped] */
   static const signed char nz_map_5x5[3][5][5] = {
      { /* w == h (square) */
         {  0,  1,  6,  6, 21 },
         {  1,  6,  6, 21, 21 },
         {  6,  6, 21, 21, 21 },
         {  6, 21, 21, 21, 21 },
         { 21, 21, 21, 21, 21 },
      }, { /* w > h (wider) */
         {  0, 16,  6,  6, 21 },
         { 16, 16,  6, 21, 21 },
         { 16, 16, 21, 21, 21 },
         { 16, 16, 21, 21, 21 },
         { 16, 16, 21, 21, 21 },
      }, { /* w < h (taller) */
         {  0, 11, 11, 11, 11 },
         { 11, 11, 11, 11, 11 },
         {  6,  6, 21, 21, 21 },
         {  6, 21, 21, 21, 21 },
         { 21, 21, 21, 21, 21 },
      },
   };
   int shape = (txw == txh) ? 0 : (txw > txh) ? 1 : 2;
   padded_pos = coeff_idx + (x << 2);  /* padded_idx = col*txh + row + col*4 */
   offset = nz_map_5x5[shape][ycl][xcl];
   /* 5 neighbors in padded layout: (+1,0), (0,+1), (+1,+1), (+2,0), (0,+2) */
   {
      int a = (int)levels[padded_pos + 1];
      int b = (int)levels[padded_pos + stride];
      int c = (int)levels[padded_pos + stride + 1];
      int d = (int)levels[padded_pos + 2];
      int e = (int)levels[padded_pos + stride * 2];
      if (a > 3) a = 3;
      if (b > 3) b = 3;
      if (c > 3) c = 3;
      if (d > 3) d = 3;
      if (e > 3) e = 3;
      mag = a + b + c + d + e;
   }
   ctx = (mag + 1) >> 1;
   if (ctx > 4) ctx = 4;
   return ctx + offset;
}

/* get_lower_levels_ctx_1d: for TX_CLASS_H/V non-2D transforms */
static int stbi_avif__av1_get_lower_levels_ctx_1d(const unsigned char *levels,
   int coeff_idx, int bhl)
{
   int txh = 1 << bhl;
   int stride = txh + STBI_AVIF_TX_PAD_HOR;
   int col = coeff_idx >> bhl;
   int row = coeff_idx - (col << bhl);
   int padded_pos = col * stride + row;
   int y = row < 2 ? row : 2;
   int offset = 26 + y * 5;
   int a = (int)levels[padded_pos + 1];
   int bn = (int)levels[padded_pos + stride];
   int c = (int)levels[padded_pos + 2];
   int d = (int)levels[padded_pos + 3];
   int e = (int)levels[padded_pos + 4];
   int mag, ctx;
   if (a > 3) a = 3;
   if (bn > 3) bn = 3;
   if (c > 3) c = 3;
   if (d > 3) d = 3;
   if (e > 3) e = 3;
   mag = a + bn + c + d + e;
   ctx = (mag + 1) >> 1;
   if (ctx > 4) ctx = 4;
   return ctx + offset;
}

/* get_br_ctx_2d: base-range context for coefficients at positions > 0 */
static int stbi_avif__av1_get_br_ctx_2d(const unsigned char *levels,
   int coeff_idx, int bhl)
{
   int col, row, stride, padded_pos, mag;
   col = coeff_idx >> bhl;
   row = coeff_idx - (col << bhl);
   stride = (1 << bhl) + STBI_AVIF_TX_PAD_HOR;
   padded_pos = col * stride + row;
   {
      int a = (int)levels[padded_pos + 1];
      int b = (int)levels[padded_pos + stride];
      int c = (int)levels[padded_pos + 1 + stride];
      if (a > 15) a = 15;
      if (b > 15) b = 15;
      if (c > 15) c = 15;
      mag = a + b + c;
   }
   mag = (mag + 1) >> 1;
   if (mag > 6) mag = 6;
   if ((row | col) < 2) return mag + 7;
   return mag + 14;
}

/* get_br_ctx for position 0 (DC) */
static int stbi_avif__av1_get_br_ctx_dc(const unsigned char *levels, int bhl)
{
   int stride = (1 << bhl) + STBI_AVIF_TX_PAD_HOR;
   int a = (int)levels[1];
   int b = (int)levels[stride];
   int c = (int)levels[stride + 1];
   int mag;
   mag = a + b + c;
   mag = (mag + 1) >> 1;
   if (mag > 6) mag = 6;
   return mag;
}

/* Skip flag CDF [3 ctx][3] */
/* AOM default_skip_cdfs[3], AOM_CDF2 values */
static unsigned short stbi_avif__av1_skip_cdf[3][3] = {
   {31671u, 32768u, 0u},
   {16515u, 32768u, 0u},
   { 4576u, 32768u, 0u}
};

/* TX partition/split CDF [5 ctx][3] */
static unsigned short stbi_avif__av1_txfm_partition_cdf[7][3][3] = {
   /* cat 0 */ { {28581u,32768u,0u}, {23846u,32768u,0u}, {20847u,32768u,0u} },
   /* cat 1 */ { {24315u,32768u,0u}, {18196u,32768u,0u}, {12133u,32768u,0u} },
   /* cat 2 */ { {18791u,32768u,0u}, {10887u,32768u,0u}, {11005u,32768u,0u} },
   /* cat 3 */ { {27179u,32768u,0u}, {20004u,32768u,0u}, {11281u,32768u,0u} },
   /* cat 4 */ { {26549u,32768u,0u}, {19308u,32768u,0u}, {14224u,32768u,0u} },
   /* cat 5 */ { {28015u,32768u,0u}, {21546u,32768u,0u}, {14400u,32768u,0u} },
   /* cat 6 */ { {28165u,32768u,0u}, {22401u,32768u,0u}, {16088u,32768u,0u} }
};

/* Palette Y mode CDF [7 bsize_ctx][3 mode_ctx][CDF_SIZE(2)] */
static unsigned short stbi_avif__av1_palette_y_mode_cdf[7][3][3] = {
   { {31676u,32768u,0u}, {3419u,32768u,0u}, {1261u,32768u,0u} },
   { {31912u,32768u,0u}, {2859u,32768u,0u}, {980u,32768u,0u} },
   { {31823u,32768u,0u}, {3400u,32768u,0u}, {781u,32768u,0u} },
   { {32030u,32768u,0u}, {3561u,32768u,0u}, {904u,32768u,0u} },
   { {32309u,32768u,0u}, {7337u,32768u,0u}, {1462u,32768u,0u} },
   { {32265u,32768u,0u}, {4015u,32768u,0u}, {1521u,32768u,0u} },
   { {32450u,32768u,0u}, {7946u,32768u,0u}, {129u,32768u,0u} }
};

/* Palette UV mode CDF [2 ctx][CDF_SIZE(2)] */
static unsigned short stbi_avif__av1_palette_uv_mode_cdf[2][3] = {
   {32461u,32768u,0u}, {21488u,32768u,0u}
};

/* Palette Y size CDF [7 bsize_ctx][CDF_SIZE(7)] — 7 symbols, size = symbol + 2 → [2..8] */
static unsigned short stbi_avif__av1_palette_y_size_cdf[7][8] = {
   {7952u,13000u,18149u,21478u,25527u,29241u,32768u,0u},
   {7139u,11421u,16195u,19544u,23666u,28073u,32768u,0u},
   {7788u,12741u,17325u,20500u,24315u,28530u,32768u,0u},
   {8271u,14064u,18246u,21564u,25071u,28533u,32768u,0u},
   {12725u,19180u,21863u,24839u,27535u,30120u,32768u,0u},
   {9711u,14888u,16923u,21052u,25661u,27875u,32768u,0u},
   {14940u,20797u,21678u,24186u,27033u,28999u,32768u,0u}
};

/* Palette UV size CDF [7 bsize_ctx][CDF_SIZE(7)] */
static unsigned short stbi_avif__av1_palette_uv_size_cdf[7][8] = {
   {8713u,19979u,27128u,29609u,31331u,32272u,32768u,0u},
   {5839u,15573u,23581u,26947u,29848u,31700u,32768u,0u},
   {4426u,11260u,17999u,21483u,25863u,29430u,32768u,0u},
   {3228u,9464u,14993u,18089u,22523u,27420u,32768u,0u},
   {3768u,8886u,13091u,17852u,22495u,27207u,32768u,0u},
   {2464u,8451u,12861u,21632u,25525u,28555u,32768u,0u},
   {1269u,5435u,10433u,18963u,21700u,25865u,32768u,0u}
};

/* Palette Y color index CDF [7 sizes][5 ctx][CDF_SIZE(8)] — max 8 colors */
static unsigned short stbi_avif__av1_palette_y_color_index_cdf[7][5][9] = {
   /* size 2 (n=2) */
   { {28710u,32768u,0u,0u,0u,0u,0u,0u,0u}, {16384u,32768u,0u,0u,0u,0u,0u,0u,0u}, {10553u,32768u,0u,0u,0u,0u,0u,0u,0u}, {27036u,32768u,0u,0u,0u,0u,0u,0u,0u}, {31603u,32768u,0u,0u,0u,0u,0u,0u,0u} },
   /* size 3 (n=3) */
   { {27877u,30490u,32768u,0u,0u,0u,0u,0u,0u}, {11532u,25697u,32768u,0u,0u,0u,0u,0u,0u}, {6544u,30234u,32768u,0u,0u,0u,0u,0u,0u}, {23018u,28072u,32768u,0u,0u,0u,0u,0u,0u}, {31915u,32385u,32768u,0u,0u,0u,0u,0u,0u} },
   /* size 4 */
   { {25572u,28046u,30045u,32768u,0u,0u,0u,0u,0u}, {9478u,21590u,27256u,32768u,0u,0u,0u,0u,0u}, {7248u,26837u,29824u,32768u,0u,0u,0u,0u,0u}, {19167u,24486u,28349u,32768u,0u,0u,0u,0u,0u}, {31400u,31825u,32250u,32768u,0u,0u,0u,0u,0u} },
   /* size 5 */
   { {24779u,26955u,28576u,30282u,32768u,0u,0u,0u,0u}, {8669u,20364u,24073u,28093u,32768u,0u,0u,0u,0u}, {4255u,27565u,29377u,31067u,32768u,0u,0u,0u,0u}, {19864u,23674u,26716u,29530u,32768u,0u,0u,0u,0u}, {31646u,31893u,32147u,32426u,32768u,0u,0u,0u,0u} },
   /* size 6 */
   { {23132u,25407u,26970u,28435u,30073u,32768u,0u,0u,0u}, {7443u,17242u,20717u,24762u,27982u,32768u,0u,0u,0u}, {6300u,24862u,26944u,28784u,30671u,32768u,0u,0u,0u}, {18916u,22895u,25267u,27435u,29652u,32768u,0u,0u,0u}, {31270u,31550u,31808u,32059u,32353u,32768u,0u,0u,0u} },
   /* size 7 */
   { {23105u,25199u,26464u,27684u,28931u,30318u,32768u,0u,0u}, {6950u,15447u,18952u,22681u,25567u,28563u,32768u,0u,0u}, {7560u,23474u,25490u,27203u,28921u,30708u,32768u,0u,0u}, {18544u,22373u,24457u,26195u,28119u,30045u,32768u,0u,0u}, {31198u,31451u,31670u,31882u,32123u,32391u,32768u,0u,0u} },
   /* size 8 */
   { {21689u,23883u,25163u,26352u,27506u,28827u,30195u,32768u,0u}, {6892u,15385u,17840u,21606u,24287u,26753u,29204u,32768u,0u}, {5651u,23182u,25042u,26518u,27982u,29392u,30900u,32768u,0u}, {19349u,22578u,24418u,25994u,27524u,29031u,30448u,32768u,0u}, {31028u,31270u,31504u,31705u,31927u,32153u,32392u,32768u,0u} }
};

/* Palette UV color index CDF [7 sizes][5 ctx][CDF_SIZE(8)] */
static unsigned short stbi_avif__av1_palette_uv_color_index_cdf[7][5][9] = {
   { {29089u,32768u,0u,0u,0u,0u,0u,0u,0u}, {16384u,32768u,0u,0u,0u,0u,0u,0u,0u}, {8713u,32768u,0u,0u,0u,0u,0u,0u,0u}, {29257u,32768u,0u,0u,0u,0u,0u,0u,0u}, {31610u,32768u,0u,0u,0u,0u,0u,0u,0u} },
   { {25257u,29145u,32768u,0u,0u,0u,0u,0u,0u}, {12287u,27293u,32768u,0u,0u,0u,0u,0u,0u}, {7033u,27960u,32768u,0u,0u,0u,0u,0u,0u}, {20145u,25405u,32768u,0u,0u,0u,0u,0u,0u}, {30608u,31639u,32768u,0u,0u,0u,0u,0u,0u} },
   { {24210u,27175u,29903u,32768u,0u,0u,0u,0u,0u}, {9888u,22386u,27214u,32768u,0u,0u,0u,0u,0u}, {5901u,26053u,29293u,32768u,0u,0u,0u,0u,0u}, {18318u,22152u,28333u,32768u,0u,0u,0u,0u,0u}, {30459u,31136u,31926u,32768u,0u,0u,0u,0u,0u} },
   { {22980u,25479u,27781u,29986u,32768u,0u,0u,0u,0u}, {8413u,21408u,24859u,28874u,32768u,0u,0u,0u,0u}, {2257u,29449u,30594u,31598u,32768u,0u,0u,0u,0u}, {19189u,21202u,25915u,28620u,32768u,0u,0u,0u,0u}, {31844u,32044u,32281u,32518u,32768u,0u,0u,0u,0u} },
   { {22217u,24567u,26637u,28683u,30548u,32768u,0u,0u,0u}, {7307u,16406u,19636u,24632u,28424u,32768u,0u,0u,0u}, {4441u,25064u,26879u,28942u,30919u,32768u,0u,0u,0u}, {17210u,20528u,23319u,26750u,29582u,32768u,0u,0u,0u}, {30674u,30953u,31396u,31735u,32207u,32768u,0u,0u,0u} },
   { {21239u,23168u,25044u,26962u,28705u,30506u,32768u,0u,0u}, {6545u,15012u,18004u,21817u,25503u,28701u,32768u,0u,0u}, {3448u,26295u,27437u,28704u,30126u,31442u,32768u,0u,0u}, {15889u,18323u,21704u,24698u,26976u,29690u,32768u,0u,0u}, {30988u,31204u,31479u,31734u,31983u,32325u,32768u,0u,0u} },
   { {21442u,23288u,24758u,26246u,27649u,28980u,30563u,32768u,0u}, {5863u,14933u,17552u,20668u,23683u,26411u,29273u,32768u,0u}, {3415u,25810u,26877u,27990u,29223u,30394u,31618u,32768u,0u}, {17965u,20084u,22232u,23974u,26274u,28402u,30390u,32768u,0u}, {31190u,31329u,31516u,31679u,31825u,32026u,32322u,32768u,0u} }
};

/* Palette color index context lookup [9] */
static const int stbi_avif__av1_palette_color_index_ctx_lookup[9] = {
   -1, -1, 0, -1, -1, 4, 3, 2, 1
};

/* Filter intra CDFs [22 block_sizes][CDF_SIZE(2)] */
static unsigned short stbi_avif__av1_filter_intra_cdfs[22][3] = {
   {4621u,32768u,0u}, {6743u,32768u,0u}, {5893u,32768u,0u},
   {7866u,32768u,0u}, {12551u,32768u,0u}, {9394u,32768u,0u},
   {12408u,32768u,0u}, {14301u,32768u,0u}, {12756u,32768u,0u},
   {22343u,32768u,0u}, {16384u,32768u,0u}, {16384u,32768u,0u},
   {16384u,32768u,0u}, {16384u,32768u,0u}, {16384u,32768u,0u},
   {16384u,32768u,0u}, {12770u,32768u,0u}, {10368u,32768u,0u},
   {20229u,32768u,0u}, {18101u,32768u,0u}, {16384u,32768u,0u},
   {16384u,32768u,0u}
};

/* Filter intra mode CDF [CDF_SIZE(5)] */
static unsigned short stbi_avif__av1_filter_intra_mode_cdf[6] = {
   8949u, 12776u, 17211u, 29558u, 32768u, 0u
};

/* CFL sign CDF [9] (8 symbols + count reserved) */
static unsigned short stbi_avif__av1_cfl_sign_cdf[9] = {
   1418u, 2123u, 13340u, 18405u, 26972u, 28343u, 32294u, 32768u, 0u
};

/* CFL alpha CDF [6 ctx][16] (15 symbols + count) */
static unsigned short stbi_avif__av1_cfl_alpha_cdf[6][17] = {
   { 7637u, 20719u, 31401u, 32481u, 32657u, 32688u, 32692u, 32696u, 32700u, 32704u, 32708u, 32712u, 32716u, 32720u, 32724u, 32768u, 0u},
   {14365u, 23603u, 28135u, 31168u, 32167u, 32395u, 32487u, 32573u, 32620u, 32647u, 32668u, 32672u, 32676u, 32680u, 32684u, 32768u, 0u},
   {11532u, 22380u, 28445u, 31360u, 32349u, 32523u, 32584u, 32649u, 32673u, 32677u, 32681u, 32685u, 32689u, 32693u, 32697u, 32768u, 0u},
   {26990u, 31402u, 32282u, 32571u, 32692u, 32696u, 32700u, 32704u, 32708u, 32712u, 32716u, 32720u, 32724u, 32728u, 32732u, 32768u, 0u},
   {17248u, 26058u, 28904u, 30608u, 31305u, 31877u, 32126u, 32321u, 32394u, 32464u, 32516u, 32560u, 32576u, 32593u, 32622u, 32768u, 0u},
   {14738u, 21678u, 25779u, 27901u, 29024u, 30302u, 30980u, 31843u, 32144u, 32413u, 32520u, 32594u, 32622u, 32656u, 32660u, 32768u, 0u}
};

/* TX size CDF [4 cat][3 ctx][4] (max 3 symbols + count) */
static unsigned short stbi_avif__av1_tx_size_cdf[4][3][4] = {
   /* cat 0 (BLOCK_8X8): 2 symbols */
   {
      {19968u, 32768u, 0u, 0u},
      {19968u, 32768u, 0u, 0u},
      {24320u, 32768u, 0u, 0u}
   },
   /* cat 1 (BLOCK_16X16): 3 symbols */
   {
      {12272u, 30172u, 32768u, 0u},
      {12272u, 30172u, 32768u, 0u},
      {18677u, 30848u, 32768u, 0u}
   },
   /* cat 2 (BLOCK_32X32): 3 symbols */
   {
      {12986u, 15180u, 32768u, 0u},
      {12986u, 15180u, 32768u, 0u},
      {24302u, 25602u, 32768u, 0u}
   },
   /* cat 3 (BLOCK_64X64+): 3 symbols */
   {
      {5782u, 11475u, 32768u, 0u},
      {5782u, 11475u, 32768u, 0u},
      {16803u, 22759u, 32768u, 0u}
   }
};



/* Flat 50/50 CDF for reading raw bits */
static const unsigned short stbi_avif__av1_half_cdf[3] = { 16384u, 32768u, 0u };


/*
 * =============================================================================
 *  AV1 PLANE STORAGE
 * =============================================================================
 */

/*
 * Holds the decoded luma (Y) and chroma (U, V) planes as 16-bit unsigned
 * samples.  For 8-bit content the values are in [0,255]; for 10-bit in [0,1023].
 * Stride is always equal to width (no padding) so pixel(x,y) = plane[y*w+x].
 */
typedef struct
{
   unsigned short *y;       /* luma plane,   frame_width  × frame_height  */
   unsigned short *u;       /* cb   plane,   chroma_width × chroma_height */
   unsigned short *v;       /* cr   plane,   chroma_width × chroma_height */
   unsigned int    width;   /* luma width  */
   unsigned int    height;  /* luma height */
   unsigned int    cw;      /* chroma width  */
   unsigned int    ch;      /* chroma height */
   int             subx;    /* horizontal chroma subsampling flag (0 or 1) */
   int             suby;    /* vertical   chroma subsampling flag (0 or 1) */
   unsigned int    bit_depth;
} stbi_avif__av1_planes;

static int stbi_avif__av1_alloc_planes(stbi_avif__av1_planes *planes,
                                        const stbi_avif__av1_sequence_header *seq,
                                        const stbi_avif__av1_frame_header    *fhdr)
{
   unsigned int w  = fhdr->frame_width;
   unsigned int h  = fhdr->frame_height;
   unsigned int cw = seq->subsampling_x ? ((w + 1u) >> 1) : w;
   unsigned int ch = seq->subsampling_y ? ((h + 1u) >> 1) : h;
   size_t y_count, c_count;

   memset(planes, 0, sizeof(*planes));
   if (w == 0u || h == 0u)
      return stbi_avif__fail("zero frame dimensions");

   y_count = (size_t)w  * (size_t)h;
   c_count = (size_t)cw * (size_t)ch;

   planes->y = (unsigned short *)STBI_AVIF_MALLOC(y_count * sizeof(unsigned short));
   planes->u = (unsigned short *)STBI_AVIF_MALLOC(c_count * sizeof(unsigned short));
   planes->v = (unsigned short *)STBI_AVIF_MALLOC(c_count * sizeof(unsigned short));
   if (!planes->y || !planes->u || !planes->v)
   {
      STBI_AVIF_FREE(planes->y);
      STBI_AVIF_FREE(planes->u);
      STBI_AVIF_FREE(planes->v);
      return stbi_avif__fail("out of memory (plane allocation)");
   }

   planes->width     = w;
   planes->height    = h;
   planes->cw        = cw;
   planes->ch        = ch;
   planes->subx      = seq->subsampling_x;
   planes->suby      = seq->subsampling_y;
   planes->bit_depth = seq->bit_depth;

   /* Fill with mid-grey (128 or 512) so untouched blocks produce visible output. */
   {
      unsigned short mid = (unsigned short)(1u << (seq->bit_depth - 1u));
      size_t i;
      for (i = 0; i < y_count; ++i) planes->y[i] = mid;
      for (i = 0; i < c_count; ++i) planes->u[i] = mid;
      for (i = 0; i < c_count; ++i) planes->v[i] = mid;
   }
   return 1;
}

static void stbi_avif__av1_free_planes(stbi_avif__av1_planes *planes)
{
   STBI_AVIF_FREE(planes->y);
   STBI_AVIF_FREE(planes->u);
   STBI_AVIF_FREE(planes->v);
   memset(planes, 0, sizeof(*planes));
}

/*
 * =============================================================================
 *  MINIMAL CDF TABLES  (AV1 spec Appendix B default CDFs)
 * =============================================================================
 *
 * We only need the tables for symbols we actually decode in the constrained
 * subset: partition type at each block size from 128×128 down to 4×4.
 *
 * For this first round we decode partition types and skip coefficient decoding
 * (fill blocks with the prediction signal = DC from above/left, which for intra
 * still images at moderate quality typically gives ~correct colours in the flat
 * regions and visible colour-block artefacts in detail regions).
 *
 * AV1 default CDFs are documented in the spec Table 5 et seq.  We use the
 * 10-entry partition CDF for blocks that are not the root.
 */

/* Partition types in AV1 (10 values for non-root, 4 for 4×4) */
#define STBI_AVIF_PARTITION_NONE        0
#define STBI_AVIF_PARTITION_HORZ        1
#define STBI_AVIF_PARTITION_VERT        2
#define STBI_AVIF_PARTITION_SPLIT       3
#define STBI_AVIF_PARTITION_HORZ_A      4
#define STBI_AVIF_PARTITION_HORZ_B      5
#define STBI_AVIF_PARTITION_VERT_A      6
#define STBI_AVIF_PARTITION_VERT_B      7
#define STBI_AVIF_PARTITION_HORZ_4      8
#define STBI_AVIF_PARTITION_VERT_4      9
/*
 * =============================================================================
 *  BLOCK SIZES
 * =============================================================================
 */

/* Log2 of block dimension for each AV1 BlockSize enum value. */
static const unsigned char stbi_avif__bsize_log2w[22] = {
   /* 4x4,4x8,8x4,8x8,8x16,16x8,16x16,16x32,32x16,32x32,32x64,64x32,
      64x64,64x128,128x64,128x128, 4x16,16x4,8x32,32x8,16x64,64x16 */
   0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5, 0,2,1,3,2,4
};
static const unsigned char stbi_avif__bsize_log2h[22] = {
   0,1,0,1,2,1,2,3,2,3,4,3,4,5,4,5, 2,0,3,1,4,2
};

/*
 * Max TX width/height log2 for each block size (Y plane, luma 4:4:4).
 * Indexed: [bs] → (max_txw_log2, max_txh_log2) where 0=4px,1=8px,2=16px,3=32px (capped at 32).
 * Matches dav1d max_txfm_size_for_bs[][0] for 4:4:4 luma.
 */
static const unsigned char stbi_avif__bsize_max_txw[22] = {
   /* 4x4  4x8  8x4  8x8  8x16 16x8 16x16 16x32 32x16 32x32 32x64 64x32 */
      0,   0,   1,   1,   1,   2,   2,    2,    3,    3,    3,    3,
   /* 64x64 64x128 128x64 128x128   4x16 16x4  8x32  32x8  16x64 64x16 */
      3,    3,      3,      3,       0,   2,    1,    3,    2,    3
};
static const unsigned char stbi_avif__bsize_max_txh[22] = {
   /* 4x4  4x8  8x4  8x8  8x16 16x8 16x16 16x32 32x16 32x32 32x64 64x32 */
      0,   1,   0,   1,   2,   1,   2,    3,    2,    3,    3,    3,
   /* 64x64 64x128 128x64 128x128   4x16 16x4  8x32  32x8  16x64 64x16 */
      3,    3,      3,      3,       2,   0,    3,    1,    3,    2
};

/* AV1 BLOCK_SIZE enum values (matches AOM) */
#define STBI_AVIF_BLOCK_4X4     0
#define STBI_AVIF_BLOCK_8X8     3
#define STBI_AVIF_BLOCK_16X16   6
#define STBI_AVIF_BLOCK_32X32   9
#define STBI_AVIF_BLOCK_64X64  12
#define STBI_AVIF_BLOCK_128X128 15

/*
 * =============================================================================
 *  DECODE CONTEXT
 * =============================================================================
 */


typedef struct
{
   stbi_avif__av1_range_decoder  rd;
   stbi_avif__av1_planes        *planes;
   const stbi_avif__av1_sequence_header *seq;
   unsigned int                  mi_cols;
   unsigned int                  mi_rows;
   int                           use_128;
   unsigned int                  base_q_idx;
   int                           q_ctx;
   int                           dc_qstep_y;
   int                           ac_qstep_y;
   int                           dc_qstep_uv;
   int                           ac_qstep_uv;
   unsigned char                *above_modes;
   unsigned char                *left_modes;
   unsigned char                *above_partition_ctx;
   unsigned char                 left_partition_ctx[32]; /* max 128px/4 = 32 mi */
   unsigned char                *above_skip;       /* per mi-col skip context */
   unsigned char                *left_skip;        /* per mi-row skip context */
   signed char                  *above_tx_intra;   /* per mi-col TX lw (log2 width -2), -1=none */
   signed char                  *left_tx_intra;    /* per mi-row TX lh, -1=none */
   unsigned char                *above_entropy[3]; /* per-plane above entropy ctx */
   unsigned char                 left_entropy[3][32]; /* per-plane left entropy ctx */
   int                           reduced_tx_set;
   int                           tx_mode_select;
   int                           allow_screen_content_tools;
   int                           cdef_bits;
   unsigned int                  sb_size_mi;
   int                           cdef_transmitted[4];

   /* Adaptive CDF state */
   unsigned short  partition_cdf[5][4][11];
   unsigned short  partition4_cdf[5];
   unsigned short  kf_y_mode_cdf[5][5][14];
   unsigned short  uv_mode_cdf_no_cfl[13][14];
   unsigned short  uv_mode_cdf_cfl[13][15];
   unsigned short  angle_delta_cdf[8][8];
   unsigned short  intra_tx_cdf_set1[4][13][8];
   unsigned short  intra_tx_cdf_set2[4][13][6];
   unsigned short  skip_cdf[3][3];
   unsigned short  txfm_partition_cdf[7][3][3];
   unsigned short  cfl_sign_cdf[9];
   unsigned short  cfl_alpha_cdf[6][17];
   unsigned short  tx_size_cdf[4][3][4];

   /* Coefficient CDFs */
   unsigned short  txb_skip_cdf[5][13][3];
   unsigned short  dc_sign_cdf[2][3][3];
   unsigned short  eob_extra_cdf[5][2][9][3];
   unsigned short  eob_multi16_cdf[2][2][6];
   unsigned short  eob_multi32_cdf[2][2][7];
   unsigned short  eob_multi64_cdf[2][2][8];
   unsigned short  eob_multi128_cdf[2][2][9];
   unsigned short  eob_multi256_cdf[2][2][10];
   unsigned short  eob_multi512_cdf[2][2][11];
   unsigned short  eob_multi1024_cdf[2][2][12];
   unsigned short  coeff_base_eob_cdf[5][2][4][4];
   unsigned short  coeff_base_cdf[5][2][42][5];
   unsigned short  coeff_br_cdf[5][2][21][5];
   unsigned short  palette_y_mode_cdf[7][3][3]; /* [bsize_ctx][mode_ctx][CDF_SIZE(2)] */
   unsigned short  palette_uv_mode_cdf[2][3];   /* [ctx][CDF_SIZE(2)] */
   unsigned short  palette_y_size_cdf[7][8];    /* [bsize_ctx][CDF_SIZE(7)] */
   unsigned short  palette_uv_size_cdf[7][8];   /* [bsize_ctx][CDF_SIZE(7)] */
   unsigned short  palette_y_color_index_cdf[7][5][9]; /* [n-2][ctx][CDF_SIZE(8)] */
   unsigned short  palette_uv_color_index_cdf[7][5][9];
   unsigned short  filter_intra_cdfs[22][3];     /* [block_size][CDF_SIZE(2)] */
   unsigned short  filter_intra_mode_cdf[6];     /* [CDF_SIZE(5)] */
} stbi_avif__av1_decode_ctx;


/*
 * =============================================================================
 *  DC INTRA PREDICTION
 * =============================================================================
 */

static void stbi_avif__av1_fill_block(unsigned short *p,
                                       unsigned int pw,
                                       unsigned int bx, unsigned int by,
                                       unsigned int bw, unsigned int bh,
                                       unsigned short val)
{
   unsigned int y, x;
   for (y = 0; y < bh; ++y)
      for (x = 0; x < bw; ++x)
         p[(by + y) * pw + (bx + x)] = val;
}

static unsigned short stbi_avif__av1_clip_sample(int v, unsigned int bit_depth)
{
   int maxv = (int)((1u << bit_depth) - 1u);
   if (v < 0)
      return 0;
   if (v > maxv)
      return (unsigned short)maxv;
   return (unsigned short)v;
}

static int stbi_avif__av1_paeth_predictor(int a, int b, int c)
{
   int p;
   int pa;
   int pb;
   int pc;

   p = a + b - c;
   pa = p > a ? p - a : a - p;
   pb = p > b ? p - b : b - p;
   pc = p > c ? p - c : c - p;
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

/* AV1 filter_intra taps: [mode][8_output_positions][7_weights]
 * For each 4x2 sub-block, compute 8 output pixels using 7 reference samples:
 *   p0 = top-left diagonal
 *   p1..p4 = 4 samples from the row above the top of the current sub-block
 *   p5 = left sample at row 0 of sub-block
 *   p6 = left sample at row 1 of sub-block
 * Result = (sum of weights * refs + 8) >> 4, clamped to [0, maxval] */
static const signed char stbi_avif__filter_intra_taps[5][8][7] = {
   { /* FILTER_DC */
      {-6, 10,  0,  0,  0, 12,  0},
      {-5,  2, 10,  0,  0,  9,  0},
      {-3,  1,  1, 10,  0,  7,  0},
      {-3,  1,  1,  2, 10,  5,  0},
      {-4,  6,  0,  0,  0,  2, 12},
      {-3,  2,  6,  0,  0,  2,  9},
      {-3,  2,  2,  6,  0,  2,  7},
      {-3,  1,  2,  2,  6,  3,  5},
   },
   { /* FILTER_V */
      {-10, 16,  0,  0,  0, 10,  0},
      { -6,  0, 16,  0,  0,  6,  0},
      { -4,  0,  0, 16,  0,  4,  0},
      { -2,  0,  0,  0, 16,  2,  0},
      {-10, 16,  0,  0,  0,  0, 10},
      { -6,  0, 16,  0,  0,  0,  6},
      { -4,  0,  0, 16,  0,  0,  4},
      { -2,  0,  0,  0, 16,  0,  2},
   },
   { /* FILTER_H */
      {-8,  8,  0,  0,  0, 16,  0},
      {-8,  0,  8,  0,  0, 16,  0},
      {-8,  0,  0,  8,  0, 16,  0},
      {-8,  0,  0,  0,  8, 16,  0},
      {-4,  4,  0,  0,  0,  0, 16},
      {-4,  0,  4,  0,  0,  0, 16},
      {-4,  0,  0,  4,  0,  0, 16},
      {-4,  0,  0,  0,  4,  0, 16},
   },
   { /* FILTER_D157 */
      {-2,  8,  0,  0,  0, 10,  0},
      {-1,  3,  8,  0,  0,  6,  0},
      {-1,  2,  3,  8,  0,  4,  0},
      { 0,  1,  2,  3,  8,  2,  0},
      {-1,  4,  0,  0,  0,  3, 10},
      {-1,  3,  4,  0,  0,  4,  6},
      {-1,  2,  3,  4,  0,  4,  4},
      {-1,  2,  2,  3,  4,  3,  3},
   },
   { /* FILTER_PAETH */
      {-12, 14,  0,  0,  0, 14,  0},
      {-10,  0, 14,  0,  0, 12,  0},
      { -9,  0,  0, 14,  0, 11,  0},
      { -8,  0,  0,  0, 14, 10,  0},
      {-10, 12,  0,  0,  0,  0, 14},
      { -9,  1, 12,  0,  0,  0, 12},
      { -8,  0,  0, 12,  0,  1, 11},
      { -7,  0,  0,  1, 12,  1,  9},
   },
};

/* Filter intra prediction.
 * Processes the block in 4x2 sub-blocks left-to-right, top-to-bottom.
 * For each sub-block uses the 7-tap filter from above to compute 8 output pixels.
 * After writing a sub-block, uses its output as references for subsequent sub-blocks. */
static void stbi_avif__av1_filter_intra_predict(unsigned short *plane, unsigned int stride,
   unsigned int plane_w, unsigned int plane_h,
   unsigned int bx, unsigned int by, unsigned int bw, unsigned int bh,
   unsigned int filter_mode, unsigned int bit_depth)
{
   unsigned int sbr, sbc, xx, yy;
   unsigned int maxv = (1u << bit_depth) - 1u;
   unsigned int mid = 1u << (bit_depth - 1u);
   const signed char (*taps)[7] = stbi_avif__filter_intra_taps[filter_mode < 5u ? filter_mode : 0u];

   for (sbr = 0; sbr < bh; sbr += 2u) {
      for (sbc = 0; sbc < bw; sbc += 4u) {
         unsigned int py0 = by + sbr;
         unsigned int px0 = bx + sbc;
         int p0, p1, p2, p3, p4, p5, p6;

         /* p0: top-left diagonal */
         if (sbr == 0u && sbc == 0u) {
            p0 = (by > 0u && bx > 0u) ? (int)plane[(by-1u)*stride + bx-1u] :
                 (by > 0u) ? (int)plane[(by-1u)*stride + bx] :
                 (bx > 0u) ? (int)plane[by*stride + bx-1u] : (int)mid;
         } else if (sbr == 0u) {
            /* sbc > 0, sbr == 0: top-left is already-written output at (py0-1, px0-1)
             * but py0 = by, so py0-1 = by-1. If by==0, no row above, use same-row left */
            p0 = (by > 0u) ? (int)plane[(by-1u)*stride + px0-1u]
                           : (int)plane[by*stride + px0-1u]; /* fallback: same row, just left of us */
         } else {
            /* sbr > 0: top-left = sample just above-left of current top row, already written */
            p0 = (int)plane[(py0-1u)*stride + (px0 > 0u ? px0-1u : px0)];
         }

         /* p1..p4: 4 samples from the row above current sub-block */
#define GETTOP(cx) \
         (sbr == 0u ? \
            (by > 0u ? (int)plane[(by-1u)*stride + ((cx) < plane_w ? (cx) : plane_w-1u)] : \
                       (bx > 0u ? (int)plane[by*stride + bx-1u] : (int)mid)) : \
            (int)plane[(py0-1u)*stride + ((cx) < plane_w ? (cx) : plane_w-1u)])
         p1 = GETTOP(px0);
         p2 = GETTOP(px0 + 1u);
         p3 = GETTOP(px0 + 2u);
         p4 = GETTOP(px0 + 3u);
#undef GETTOP

         /* p5: left neighbor at row 0 */
         p5 = (sbc == 0u) ?
              (bx > 0u ? (int)plane[py0*stride + bx-1u] :
                         (by > 0u ? (int)plane[(by-1u)*stride + bx] : (int)mid)) :
              (int)plane[py0*stride + px0-1u];

         /* p6: left neighbor at row 1 */
         {
            unsigned int row1 = (py0+1u < plane_h) ? py0+1u : py0;
            p6 = (sbc == 0u) ?
                 (bx > 0u ? (int)plane[row1*stride + bx-1u] :
                            (by > 0u ? (int)plane[(by-1u)*stride + bx] : (int)mid)) :
                 (int)plane[row1*stride + px0-1u];
         }

         /* Apply taps: positions 0..3=row0, 4..7=row1 */
         for (yy = 0u; yy < 2u; yy++) {
            for (xx = 0u; xx < 4u; xx++) {
               const signed char *w = taps[yy * 4u + xx];
               int acc = w[0]*p0 + w[1]*p1 + w[2]*p2 + w[3]*p3 + w[4]*p4 + w[5]*p5 + w[6]*p6;
               unsigned int oy = py0 + yy;
               unsigned int ox = px0 + xx;
               acc = (acc + 8) >> 4;
               if (acc < 0) acc = 0;
               if (acc > (int)maxv) acc = (int)maxv;
               if (oy < plane_h && ox < plane_w)
                  plane[oy*stride + ox] = (unsigned short)acc;
            }
         }
      }
   }
}

/* AV1 smooth intra prediction weights (from AV1 spec / dav1d_sm_weights).
 * Indexed by [bs] where bs = block size in pixels (2,4,8,16,32,64).
 * We store starting from index 2 (bs=2). Offset = bs (i.e., sm_weights[bs+x]). */
static const unsigned char stbi_avif__sm_weights[128] = {
    /* bs=2: indices 0..1 (unused offset, but +2 for bs=2) */
    0, 0,
    /* bs=2 */
    255, 128,
    /* bs=4 */
    255, 149,  85,  64,
    /* bs=8 */
    255, 197, 146, 105,  73,  50,  37,  32,
    /* bs=16 */
    255, 225, 196, 170, 145, 123, 102,  84,
     68,  54,  43,  33,  26,  20,  17,  16,
    /* bs=32 */
    255, 240, 225, 210, 196, 182, 169, 157,
    145, 133, 122, 111, 101,  92,  83,  74,
     66,  59,  52,  45,  39,  34,  29,  25,
     21,  17,  14,  12,  10,   9,   8,   8,
    /* bs=64 */
    255, 248, 240, 233, 225, 218, 210, 203,
    196, 189, 182, 176, 169, 163, 156, 150,
    144, 138, 133, 127, 121, 116, 111, 106,
    101,  96,  91,  86,  82,  77,  73,  69,
     65,  61,  57,  54,  50,  47,  44,  41,
     38,  35,  32,  29,  27,  25,  22,  20,
     18,  16,  15,  13,  12,  10,   9,   8,
      7,   6,   6,   5,   5,   4,   4,   4
};

static void stbi_avif__av1_predict_block(unsigned short *p,
                                          unsigned int stride,
                                          unsigned int plane_w,
                                          unsigned int plane_h,
                                          unsigned int bx,
                                          unsigned int by,
                                          unsigned int bw,
                                          unsigned int bh,
                                          unsigned int bit_depth,
                                          unsigned int mode)
{
   unsigned short top[256];
   unsigned short left[256];
   unsigned short top_left;
   unsigned int ref_count;
   unsigned int i;
   unsigned int x;
   unsigned int y;
   int base;
   int amp;
   int dc;
   int have_top = (by > 0u) ? 1 : 0;
   int have_left = (bx > 0u) ? 1 : 0;

   base = (int)(1u << (bit_depth - 1u));
   amp = (bit_depth > 8u) ? (8 << (bit_depth - 8u)) : 8;

   ref_count = bw + bh + 2u;
   if (ref_count > 256u)
      ref_count = 256u;

   /* Fill left[] reference array: if no left col, fill with top[0] or base+1 */
   for (i = 0u; i < ref_count; ++i) {
      unsigned int ly = by + i;
      if (ly >= plane_h) ly = plane_h - 1u;
      if (have_left)
         left[i] = p[ly * stride + (bx - 1u)];
      else if (have_top)
         left[i] = p[(by - 1u) * stride + bx]; /* fill with top[0] */
      else
         left[i] = (unsigned short)(base + 1); /* DC_128+1 = (1<<bd)/2 + 1 */
   }

   /* Fill top[] reference array: if no top row, fill with left[0] or base-1 */
   for (i = 0u; i < ref_count; ++i) {
      unsigned int tx = bx + i;
      if (tx >= plane_w) tx = plane_w - 1u;
      if (have_top)
         top[i] = p[(by - 1u) * stride + tx];
      else if (have_left)
         top[i] = p[by * stride + (bx - 1u)]; /* fill with left[0] at current row */
      else
         top[i] = (unsigned short)(base - 1); /* (1<<bd)/2 - 1 */
   }

   /* Top-left pixel per dav1d: if have_left → left[0], if have_top → top[0], else base */
   if (have_left && have_top)
      top_left = p[(by - 1u) * stride + (bx - 1u)];
   else if (have_left)
      top_left = p[by * stride + (bx - 1u)];  /* left[0] */
   else if (have_top)
      top_left = p[(by - 1u) * stride + bx];  /* top[0] */
   else
      top_left = (unsigned short)base;

   /* Mode conversion for DC and PAETH when references unavailable (per AV1 spec) */
   if (mode == 0u) { /* DC_PRED */
      if (!have_left && !have_top) mode = 13u; /* DC_128 (use base) */
      else if (!have_left)         mode = 14u; /* TOP_DC_PRED (use only top) */
      else if (!have_top)          mode = 15u; /* LEFT_DC_PRED (use only left) */
   } else if (mode == 12u) { /* PAETH */
      if (!have_left && !have_top) mode = 13u; /* DC_128 */
      else if (!have_left)         mode = 1u;  /* VERT_PRED */
      else if (!have_top)          mode = 2u;  /* HOR_PRED */
   }

   dc = base;
   if (bw > 0u && bh > 0u)
   {
      unsigned int count = 0u;
      int sum = 0;
      if (have_top) {
         for (i = 0u; i < bw && i < ref_count; ++i)
         {
            sum += (int)top[i];
            ++count;
         }
      }
      if (have_left) {
         for (i = 0u; i < bh && i < ref_count; ++i)
         {
            sum += (int)left[i];
            ++count;
         }
      }
      if (count > 0u)
         dc = sum / (int)count;
   }

   for (y = 0u; y < bh; ++y)
   {
      for (x = 0u; x < bw; ++x)
      {
         int val;
         unsigned int di = x + y;
         unsigned int tix = x < ref_count ? x : (ref_count - 1u);
         unsigned int tiy = y < ref_count ? y : (ref_count - 1u);
         unsigned int dix = di < ref_count ? di : (ref_count - 1u);

         switch (mode)
         {
            case 0u: /* DC */
               val = dc;
               break;
            case 13u: /* DC_128: base value, no neighbors */
               val = base;
               break;
            case 14u: /* TOP_DC_PRED: top only */
               val = dc;  /* dc was computed using top[] only */
               break;
            case 15u: /* LEFT_DC_PRED: left only */
               val = dc;  /* dc was computed using left[] only */
               break;
            case 1u: /* V */
               val = (int)top[tix] + ((int)(2u * x + 1u) * amp) / (int)(2u * (bw ? bw : 1u)) - amp / 2;
               break;
            case 2u: /* H */
               val = (int)left[tiy] + ((int)(2u * y + 1u) * amp) / (int)(2u * (bh ? bh : 1u)) - amp / 2;
               break;
            case 3u: /* D45 */
               val = (int)top[dix] + ((int)di * amp) / (int)((bw + bh) ? (bw + bh) : 1u) - amp / 2;
               break;
            case 4u: /* D135 */
               val = ((int)top[tix] + (int)left[tiy] + (int)top_left) / 3 + (((int)y - (int)x) * amp) / (int)((bw + bh) ? (bw + bh) : 1u);
               break;
            case 5u: /* D113 */
               val = (2 * (int)top[tix] + (int)top[dix] + (int)left[tiy]) / 4;
               break;
            case 6u: /* D157 */
               val = (2 * (int)left[tiy] + (int)left[dix] + (int)top[tix]) / 4;
               break;
            case 7u: /* D203 */
               val = (3 * (int)left[tiy] + (int)top[tix]) / 4 - (((int)x - (int)y) * amp) / (int)((bw + bh) ? (bw + bh) : 1u);
               break;
            case 8u: /* D67 */
               val = (3 * (int)top[tix] + (int)left[tiy]) / 4 + (((int)x - (int)y) * amp) / (int)((bw + bh) ? (bw + bh) : 1u);
               break;
            case 9u: /* SMOOTH */
               {
                  /* AV1 spec: pred = (w_ver[y]*top[x] + (256-w_ver[y])*bottom
                   *                 + w_hor[x]*left[y] + (256-w_hor[x])*right + 256) >> 9 */
                  unsigned int sm_bw = bw > 64u ? 64u : bw;
                  unsigned int sm_bh = bh > 64u ? 64u : bh;
                  unsigned int sm_x = bw > sm_bw ? (x * sm_bw) / bw : x;
                  unsigned int sm_y = bh > sm_bh ? (y * sm_bh) / bh : y;
                  int w_h = (int)stbi_avif__sm_weights[sm_bw + sm_x];
                  int w_v = (int)stbi_avif__sm_weights[sm_bh + sm_y];
                  int right_px = (int)top[(bw - 1u) < ref_count ? (bw - 1u) : (ref_count - 1u)];
                  int bottom_px = (int)left[(bh - 1u) < ref_count ? (bh - 1u) : (ref_count - 1u)];
                  val = (w_v * (int)top[tix] + (256 - w_v) * bottom_px +
                         w_h * (int)left[tiy] + (256 - w_h) * right_px + 256) >> 9;
               }
               break;
            case 10u: /* SMOOTH_V */
               {
                  /* pred = (w_ver[y]*top[x] + (256-w_ver[y])*bottom + 128) >> 8 */
                   unsigned int sm_bh = bh > 64u ? 64u : bh;
                   unsigned int sm_y = bh > sm_bh ? (y * sm_bh) / bh : y;
                   int w_v = (int)stbi_avif__sm_weights[sm_bh + sm_y];
                  int bottom_px = (int)left[(bh - 1u) < ref_count ? (bh - 1u) : (ref_count - 1u)];
                  val = (w_v * (int)top[tix] + (256 - w_v) * bottom_px + 128) >> 8;
               }
               break;
            case 11u: /* SMOOTH_H */
               {
                  /* pred = (w_hor[x]*left[y] + (256-w_hor[x])*right + 128) >> 8 */
                   unsigned int sm_bw = bw > 64u ? 64u : bw;
                   unsigned int sm_x = bw > sm_bw ? (x * sm_bw) / bw : x;
                   int w_h = (int)stbi_avif__sm_weights[sm_bw + sm_x];
                  int right_px = (int)top[(bw - 1u) < ref_count ? (bw - 1u) : (ref_count - 1u)];
                  val = (w_h * (int)left[tiy] + (256 - w_h) * right_px + 128) >> 8;
               }
               break;
            case 12u: /* PAETH */
               val = stbi_avif__av1_paeth_predictor((int)top[tix], (int)left[tiy], (int)top_left);
               break;
            default:
               val = dc;
               break;
         }

         p[(by + y) * stride + (bx + x)] = stbi_avif__av1_clip_sample(val, bit_depth);
      }
   }
}

/*
 * Minimal intra-only chroma refinement.
 *
 * In still-picture AVIFs the chroma signal often tracks local luma structure.
 * Until full UV residual decoding is implemented, apply a small luma-coupled
 * correction so predicted U/V blocks carry visible colour variation.
 */


/*
 * =============================================================================
 *  INVERSE DCT TRANSFORMS
 * =============================================================================
 *
 * Fixed-point AV1 inverse DCT using the spec's butterfly decomposition.
 * All intermediate values are scaled by 2^(bit_depth-1) or similar.
 * We use 32-bit int arithmetic throughout.
 *
 * The AV1 spec uses cos/sin values scaled by 2^14 (SINPI = 2^14).
 * Round: (a * c + (1 << 13)) >> 14
 */

#define STBI_AVIF_ROUND_SHIFT(a, b) (((a) + (1 << ((b) - 1))) >> (b))

/* AOM-compatible half butterfly: round((a*x + b*y) >> cos_bit) */
#define STBI_AVIF_HALF_BTF(w0, in0, w1, in1, cos_bit) \
   ((int)((((long)(w0) * (long)(in0)) + ((long)(w1) * (long)(in1)) + \
   (1L << ((cos_bit) - 1))) >> (cos_bit)))

/*
 * AV1 inverse DCT using AOM's exact stage-based implementation.
 * cos_bit = 12, cospi values = round(cos(pi*j/128) * 4096)
 *
 * cospi table for cos_bit=12 (index j -> round(cos(pi*j/128)*4096)):
 */
static const int stbi_avif__cospi[64] = {
   4096, 4095, 4091, 4085, 4076, 4065, 4052, 4036,
   4017, 3996, 3973, 3948, 3920, 3889, 3857, 3822,
   3784, 3745, 3703, 3659, 3612, 3564, 3513, 3461,
   3406, 3349, 3290, 3229, 3166, 3102, 3035, 2967,
   2896, 2824, 2751, 2675, 2598, 2520, 2440, 2359,
   2276, 2191, 2106, 2019, 1931, 1842, 1751, 1660,
   1567, 1474, 1380, 1285, 1189, 1092,  995,  897,
    799,  700,  601,  501,  401,  301,  201,  101
};

#define COS_BIT 12

static void stbi_avif__av1_idct4(const int *input, int *output)
{
   const int *c = stbi_avif__cospi;
   int bf0[4], bf1[4];
   /* stage 1: input permutation */
   bf1[0] = input[0]; bf1[1] = input[2]; bf1[2] = input[1]; bf1[3] = input[3];
   /* stage 2 */
   bf0[0] = STBI_AVIF_HALF_BTF(c[32], bf1[0],  c[32], bf1[1], COS_BIT);
   bf0[1] = STBI_AVIF_HALF_BTF(c[32], bf1[0], -c[32], bf1[1], COS_BIT);
   bf0[2] = STBI_AVIF_HALF_BTF(c[48], bf1[2], -c[16], bf1[3], COS_BIT);
   bf0[3] = STBI_AVIF_HALF_BTF(c[16], bf1[2],  c[48], bf1[3], COS_BIT);
   /* stage 3 */
   output[0] = bf0[0] + bf0[3];
   output[1] = bf0[1] + bf0[2];
   output[2] = bf0[1] - bf0[2];
   output[3] = bf0[0] - bf0[3];
}

static void stbi_avif__av1_idct8(const int *input, int *output)
{
   const int *c = stbi_avif__cospi;
   int bf0[8], bf1[8], step[8];
   /* stage 1: input permutation */
   bf1[0]=input[0]; bf1[1]=input[4]; bf1[2]=input[2]; bf1[3]=input[6];
   bf1[4]=input[1]; bf1[5]=input[5]; bf1[6]=input[3]; bf1[7]=input[7];
   /* stage 2 */
   step[0]=bf1[0]; step[1]=bf1[1]; step[2]=bf1[2]; step[3]=bf1[3];
   step[4] = STBI_AVIF_HALF_BTF(c[56], bf1[4], -c[8],  bf1[7], COS_BIT);
   step[5] = STBI_AVIF_HALF_BTF(c[24], bf1[5], -c[40], bf1[6], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF(c[40], bf1[5],  c[24], bf1[6], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[8],  bf1[4],  c[56], bf1[7], COS_BIT);
   /* stage 3 */
   bf0[0] = STBI_AVIF_HALF_BTF(c[32], step[0],  c[32], step[1], COS_BIT);
   bf0[1] = STBI_AVIF_HALF_BTF(c[32], step[0], -c[32], step[1], COS_BIT);
   bf0[2] = STBI_AVIF_HALF_BTF(c[48], step[2], -c[16], step[3], COS_BIT);
   bf0[3] = STBI_AVIF_HALF_BTF(c[16], step[2],  c[48], step[3], COS_BIT);
   bf0[4] = step[4] + step[5];
   bf0[5] = step[4] - step[5];
   bf0[6] = -step[6] + step[7];
   bf0[7] = step[6] + step[7];
   /* stage 4 */
   step[0] = bf0[0] + bf0[3];
   step[1] = bf0[1] + bf0[2];
   step[2] = bf0[1] - bf0[2];
   step[3] = bf0[0] - bf0[3];
   step[4] = bf0[4];
   step[5] = STBI_AVIF_HALF_BTF(-c[32], bf0[5], c[32], bf0[6], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF( c[32], bf0[5], c[32], bf0[6], COS_BIT);
   step[7] = bf0[7];
   /* stage 5 */
   output[0] = step[0] + step[7];
   output[1] = step[1] + step[6];
   output[2] = step[2] + step[5];
   output[3] = step[3] + step[4];
   output[4] = step[3] - step[4];
   output[5] = step[2] - step[5];
   output[6] = step[1] - step[6];
   output[7] = step[0] - step[7];
}

static void stbi_avif__av1_idct16(const int *input, int *output)
{
   const int *c = stbi_avif__cospi;
   int bf0[16], bf1[16], step[16];
   /* stage 1: input permutation */
   bf1[0]=input[0];  bf1[1]=input[8];  bf1[2]=input[4];  bf1[3]=input[12];
   bf1[4]=input[2];  bf1[5]=input[10]; bf1[6]=input[6];  bf1[7]=input[14];
   bf1[8]=input[1];  bf1[9]=input[9];  bf1[10]=input[5]; bf1[11]=input[13];
   bf1[12]=input[3]; bf1[13]=input[11]; bf1[14]=input[7]; bf1[15]=input[15];
   /* stage 2 */
   step[0]=bf1[0]; step[1]=bf1[1]; step[2]=bf1[2]; step[3]=bf1[3];
   step[4]=bf1[4]; step[5]=bf1[5]; step[6]=bf1[6]; step[7]=bf1[7];
   step[8]  = STBI_AVIF_HALF_BTF(c[60], bf1[8],  -c[4],  bf1[15], COS_BIT);
   step[9]  = STBI_AVIF_HALF_BTF(c[28], bf1[9],  -c[36], bf1[14], COS_BIT);
   step[10] = STBI_AVIF_HALF_BTF(c[44], bf1[10], -c[20], bf1[13], COS_BIT);
   step[11] = STBI_AVIF_HALF_BTF(c[12], bf1[11], -c[52], bf1[12], COS_BIT);
   step[12] = STBI_AVIF_HALF_BTF(c[52], bf1[11],  c[12], bf1[12], COS_BIT);
   step[13] = STBI_AVIF_HALF_BTF(c[20], bf1[10],  c[44], bf1[13], COS_BIT);
   step[14] = STBI_AVIF_HALF_BTF(c[36], bf1[9],   c[28], bf1[14], COS_BIT);
   step[15] = STBI_AVIF_HALF_BTF(c[4],  bf1[8],   c[60], bf1[15], COS_BIT);
   /* stage 3 */
   bf0[0]=step[0]; bf0[1]=step[1]; bf0[2]=step[2]; bf0[3]=step[3];
   bf0[4] = STBI_AVIF_HALF_BTF(c[56], step[4], -c[8],  step[7], COS_BIT);
   bf0[5] = STBI_AVIF_HALF_BTF(c[24], step[5], -c[40], step[6], COS_BIT);
   bf0[6] = STBI_AVIF_HALF_BTF(c[40], step[5],  c[24], step[6], COS_BIT);
   bf0[7] = STBI_AVIF_HALF_BTF(c[8],  step[4],  c[56], step[7], COS_BIT);
   bf0[8]  = step[8] + step[9];
   bf0[9]  = step[8] - step[9];
   bf0[10] = -step[10] + step[11];
   bf0[11] = step[10] + step[11];
   bf0[12] = step[12] + step[13];
   bf0[13] = step[12] - step[13];
   bf0[14] = -step[14] + step[15];
   bf0[15] = step[14] + step[15];
   /* stage 4 */
   step[0] = STBI_AVIF_HALF_BTF(c[32], bf0[0],  c[32], bf0[1], COS_BIT);
   step[1] = STBI_AVIF_HALF_BTF(c[32], bf0[0], -c[32], bf0[1], COS_BIT);
   step[2] = STBI_AVIF_HALF_BTF(c[48], bf0[2], -c[16], bf0[3], COS_BIT);
   step[3] = STBI_AVIF_HALF_BTF(c[16], bf0[2],  c[48], bf0[3], COS_BIT);
   step[4] = bf0[4] + bf0[5];
   step[5] = bf0[4] - bf0[5];
   step[6] = -bf0[6] + bf0[7];
   step[7] = bf0[6] + bf0[7];
   step[8]  = bf0[8];
   step[9]  = STBI_AVIF_HALF_BTF(-c[16], bf0[9],  c[48], bf0[14], COS_BIT);
   step[10] = STBI_AVIF_HALF_BTF(-c[48], bf0[10], -c[16], bf0[13], COS_BIT);
   step[11] = bf0[11];
   step[12] = bf0[12];
   step[13] = STBI_AVIF_HALF_BTF(-c[16], bf0[10], c[48], bf0[13], COS_BIT);
   step[14] = STBI_AVIF_HALF_BTF( c[48], bf0[9],  c[16], bf0[14], COS_BIT);
   step[15] = bf0[15];
   /* stage 5 */
   bf0[0] = step[0] + step[3];
   bf0[1] = step[1] + step[2];
   bf0[2] = step[1] - step[2];
   bf0[3] = step[0] - step[3];
   bf0[4] = step[4];
   bf0[5] = STBI_AVIF_HALF_BTF(-c[32], step[5], c[32], step[6], COS_BIT);
   bf0[6] = STBI_AVIF_HALF_BTF( c[32], step[5], c[32], step[6], COS_BIT);
   bf0[7] = step[7];
   bf0[8]  = step[8] + step[11];
   bf0[9]  = step[9] + step[10];
   bf0[10] = step[9] - step[10];
   bf0[11] = step[8] - step[11];
   bf0[12] = -step[12] + step[15];
   bf0[13] = -step[13] + step[14];
   bf0[14] = step[13] + step[14];
   bf0[15] = step[12] + step[15];
   /* stage 6 */
   step[0] = bf0[0] + bf0[7];
   step[1] = bf0[1] + bf0[6];
   step[2] = bf0[2] + bf0[5];
   step[3] = bf0[3] + bf0[4];
   step[4] = bf0[3] - bf0[4];
   step[5] = bf0[2] - bf0[5];
   step[6] = bf0[1] - bf0[6];
   step[7] = bf0[0] - bf0[7];
   step[8]  = bf0[8];
   step[9]  = bf0[9];
   step[10] = STBI_AVIF_HALF_BTF(-c[32], bf0[10], c[32], bf0[13], COS_BIT);
   step[11] = STBI_AVIF_HALF_BTF(-c[32], bf0[11], c[32], bf0[12], COS_BIT);
   step[12] = STBI_AVIF_HALF_BTF( c[32], bf0[11], c[32], bf0[12], COS_BIT);
   step[13] = STBI_AVIF_HALF_BTF( c[32], bf0[10], c[32], bf0[13], COS_BIT);
   step[14] = bf0[14];
   step[15] = bf0[15];
   /* stage 7 */
   output[0]  = step[0] + step[15];
   output[1]  = step[1] + step[14];
   output[2]  = step[2] + step[13];
   output[3]  = step[3] + step[12];
   output[4]  = step[4] + step[11];
   output[5]  = step[5] + step[10];
   output[6]  = step[6] + step[9];
   output[7]  = step[7] + step[8];
   output[8]  = step[7] - step[8];
   output[9]  = step[6] - step[9];
   output[10] = step[5] - step[10];
   output[11] = step[4] - step[11];
   output[12] = step[3] - step[12];
   output[13] = step[2] - step[13];
   output[14] = step[1] - step[14];
   output[15] = step[0] - step[15];
}

static void stbi_avif__av1_idct32(const int *input, int *output)
{
   const int *c = stbi_avif__cospi;
   int bf0[32], bf1[32], step[32];
   /* stage 1: input permutation */
   bf1[0]=input[0];   bf1[1]=input[16];  bf1[2]=input[8];   bf1[3]=input[24];
   bf1[4]=input[4];   bf1[5]=input[20];  bf1[6]=input[12];  bf1[7]=input[28];
   bf1[8]=input[2];   bf1[9]=input[18];  bf1[10]=input[10]; bf1[11]=input[26];
   bf1[12]=input[6];  bf1[13]=input[22]; bf1[14]=input[14]; bf1[15]=input[30];
   bf1[16]=input[1];  bf1[17]=input[17]; bf1[18]=input[9];  bf1[19]=input[25];
   bf1[20]=input[5];  bf1[21]=input[21]; bf1[22]=input[13]; bf1[23]=input[29];
   bf1[24]=input[3];  bf1[25]=input[19]; bf1[26]=input[11]; bf1[27]=input[27];
   bf1[28]=input[7];  bf1[29]=input[23]; bf1[30]=input[15]; bf1[31]=input[31];
   /* stage 2 */
   step[0]=bf1[0]; step[1]=bf1[1]; step[2]=bf1[2]; step[3]=bf1[3];
   step[4]=bf1[4]; step[5]=bf1[5]; step[6]=bf1[6]; step[7]=bf1[7];
   step[8]=bf1[8]; step[9]=bf1[9]; step[10]=bf1[10]; step[11]=bf1[11];
   step[12]=bf1[12]; step[13]=bf1[13]; step[14]=bf1[14]; step[15]=bf1[15];
   step[16] = STBI_AVIF_HALF_BTF(c[62], bf1[16], -c[2],  bf1[31], COS_BIT);
   step[17] = STBI_AVIF_HALF_BTF(c[30], bf1[17], -c[34], bf1[30], COS_BIT);
   step[18] = STBI_AVIF_HALF_BTF(c[46], bf1[18], -c[18], bf1[29], COS_BIT);
   step[19] = STBI_AVIF_HALF_BTF(c[14], bf1[19], -c[50], bf1[28], COS_BIT);
   step[20] = STBI_AVIF_HALF_BTF(c[54], bf1[20], -c[10], bf1[27], COS_BIT);
   step[21] = STBI_AVIF_HALF_BTF(c[22], bf1[21], -c[42], bf1[26], COS_BIT);
   step[22] = STBI_AVIF_HALF_BTF(c[38], bf1[22], -c[26], bf1[25], COS_BIT);
   step[23] = STBI_AVIF_HALF_BTF(c[6],  bf1[23], -c[58], bf1[24], COS_BIT);
   step[24] = STBI_AVIF_HALF_BTF(c[58], bf1[23],  c[6],  bf1[24], COS_BIT);
   step[25] = STBI_AVIF_HALF_BTF(c[26], bf1[22],  c[38], bf1[25], COS_BIT);
   step[26] = STBI_AVIF_HALF_BTF(c[42], bf1[21],  c[22], bf1[26], COS_BIT);
   step[27] = STBI_AVIF_HALF_BTF(c[10], bf1[20],  c[54], bf1[27], COS_BIT);
   step[28] = STBI_AVIF_HALF_BTF(c[50], bf1[19],  c[14], bf1[28], COS_BIT);
   step[29] = STBI_AVIF_HALF_BTF(c[18], bf1[18],  c[46], bf1[29], COS_BIT);
   step[30] = STBI_AVIF_HALF_BTF(c[34], bf1[17],  c[30], bf1[30], COS_BIT);
   step[31] = STBI_AVIF_HALF_BTF(c[2],  bf1[16],  c[62], bf1[31], COS_BIT);
   /* stage 3 */
   bf0[0]=step[0]; bf0[1]=step[1]; bf0[2]=step[2]; bf0[3]=step[3];
   bf0[4]=step[4]; bf0[5]=step[5]; bf0[6]=step[6]; bf0[7]=step[7];
   bf0[8]  = STBI_AVIF_HALF_BTF(c[60], step[8],  -c[4],  step[15], COS_BIT);
   bf0[9]  = STBI_AVIF_HALF_BTF(c[28], step[9],  -c[36], step[14], COS_BIT);
   bf0[10] = STBI_AVIF_HALF_BTF(c[44], step[10], -c[20], step[13], COS_BIT);
   bf0[11] = STBI_AVIF_HALF_BTF(c[12], step[11], -c[52], step[12], COS_BIT);
   bf0[12] = STBI_AVIF_HALF_BTF(c[52], step[11],  c[12], step[12], COS_BIT);
   bf0[13] = STBI_AVIF_HALF_BTF(c[20], step[10],  c[44], step[13], COS_BIT);
   bf0[14] = STBI_AVIF_HALF_BTF(c[36], step[9],   c[28], step[14], COS_BIT);
   bf0[15] = STBI_AVIF_HALF_BTF(c[4],  step[8],   c[60], step[15], COS_BIT);
   bf0[16] = step[16] + step[17];
   bf0[17] = step[16] - step[17];
   bf0[18] = -step[18] + step[19];
   bf0[19] = step[18] + step[19];
   bf0[20] = step[20] + step[21];
   bf0[21] = step[20] - step[21];
   bf0[22] = -step[22] + step[23];
   bf0[23] = step[22] + step[23];
   bf0[24] = step[24] + step[25];
   bf0[25] = step[24] - step[25];
   bf0[26] = -step[26] + step[27];
   bf0[27] = step[26] + step[27];
   bf0[28] = step[28] + step[29];
   bf0[29] = step[28] - step[29];
   bf0[30] = -step[30] + step[31];
   bf0[31] = step[30] + step[31];
   /* stage 4 */
   step[0]=bf0[0]; step[1]=bf0[1]; step[2]=bf0[2]; step[3]=bf0[3];
   step[4] = STBI_AVIF_HALF_BTF(c[56], bf0[4], -c[8],  bf0[7], COS_BIT);
   step[5] = STBI_AVIF_HALF_BTF(c[24], bf0[5], -c[40], bf0[6], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF(c[40], bf0[5],  c[24], bf0[6], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[8],  bf0[4],  c[56], bf0[7], COS_BIT);
   step[8]  = bf0[8] + bf0[9];
   step[9]  = bf0[8] - bf0[9];
   step[10] = -bf0[10] + bf0[11];
   step[11] = bf0[10] + bf0[11];
   step[12] = bf0[12] + bf0[13];
   step[13] = bf0[12] - bf0[13];
   step[14] = -bf0[14] + bf0[15];
   step[15] = bf0[14] + bf0[15];
   step[16] = bf0[16];
   step[17] = STBI_AVIF_HALF_BTF(-c[8],  bf0[17], c[56], bf0[30], COS_BIT);
   step[18] = STBI_AVIF_HALF_BTF(-c[56], bf0[18], -c[8], bf0[29], COS_BIT);
   step[19] = bf0[19];
   step[20] = bf0[20];
   step[21] = STBI_AVIF_HALF_BTF(-c[40], bf0[21], c[24], bf0[26], COS_BIT);
   step[22] = STBI_AVIF_HALF_BTF(-c[24], bf0[22], -c[40], bf0[25], COS_BIT);
   step[23] = bf0[23];
   step[24] = bf0[24];
   step[25] = STBI_AVIF_HALF_BTF(-c[40], bf0[22], c[24], bf0[25], COS_BIT);
   step[26] = STBI_AVIF_HALF_BTF( c[24], bf0[21], c[40], bf0[26], COS_BIT);
   step[27] = bf0[27];
   step[28] = bf0[28];
   step[29] = STBI_AVIF_HALF_BTF(-c[8],  bf0[18], c[56], bf0[29], COS_BIT);
   step[30] = STBI_AVIF_HALF_BTF( c[56], bf0[17], c[8],  bf0[30], COS_BIT);
   step[31] = bf0[31];
   /* stage 5 */
   bf0[0] = STBI_AVIF_HALF_BTF(c[32], step[0],  c[32], step[1], COS_BIT);
   bf0[1] = STBI_AVIF_HALF_BTF(c[32], step[0], -c[32], step[1], COS_BIT);
   bf0[2] = STBI_AVIF_HALF_BTF(c[48], step[2], -c[16], step[3], COS_BIT);
   bf0[3] = STBI_AVIF_HALF_BTF(c[16], step[2],  c[48], step[3], COS_BIT);
   bf0[4] = step[4] + step[5];
   bf0[5] = step[4] - step[5];
   bf0[6] = -step[6] + step[7];
   bf0[7] = step[6] + step[7];
   bf0[8]  = step[8];
   bf0[9]  = STBI_AVIF_HALF_BTF(-c[16], step[9],  c[48], step[14], COS_BIT);
   bf0[10] = STBI_AVIF_HALF_BTF(-c[48], step[10], -c[16], step[13], COS_BIT);
   bf0[11] = step[11];
   bf0[12] = step[12];
   bf0[13] = STBI_AVIF_HALF_BTF(-c[16], step[10], c[48], step[13], COS_BIT);
   bf0[14] = STBI_AVIF_HALF_BTF( c[48], step[9],  c[16], step[14], COS_BIT);
   bf0[15] = step[15];
   bf0[16] = step[16] + step[19];
   bf0[17] = step[17] + step[18];
   bf0[18] = step[17] - step[18];
   bf0[19] = step[16] - step[19];
   bf0[20] = -step[20] + step[23];
   bf0[21] = -step[21] + step[22];
   bf0[22] = step[21] + step[22];
   bf0[23] = step[20] + step[23];
   bf0[24] = step[24] + step[27];
   bf0[25] = step[25] + step[26];
   bf0[26] = step[25] - step[26];
   bf0[27] = step[24] - step[27];
   bf0[28] = -step[28] + step[31];
   bf0[29] = -step[29] + step[30];
   bf0[30] = step[29] + step[30];
   bf0[31] = step[28] + step[31];
   /* stage 6 */
   step[0] = bf0[0] + bf0[3];
   step[1] = bf0[1] + bf0[2];
   step[2] = bf0[1] - bf0[2];
   step[3] = bf0[0] - bf0[3];
   step[4] = bf0[4];
   step[5] = STBI_AVIF_HALF_BTF(-c[32], bf0[5], c[32], bf0[6], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF( c[32], bf0[5], c[32], bf0[6], COS_BIT);
   step[7] = bf0[7];
   step[8]  = bf0[8] + bf0[11];
   step[9]  = bf0[9] + bf0[10];
   step[10] = bf0[9] - bf0[10];
   step[11] = bf0[8] - bf0[11];
   step[12] = -bf0[12] + bf0[15];
   step[13] = -bf0[13] + bf0[14];
   step[14] = bf0[13] + bf0[14];
   step[15] = bf0[12] + bf0[15];
   step[16] = bf0[16];
   step[17] = bf0[17];
   step[18] = STBI_AVIF_HALF_BTF(-c[16], bf0[18], c[48], bf0[29], COS_BIT);
   step[19] = STBI_AVIF_HALF_BTF(-c[16], bf0[19], c[48], bf0[28], COS_BIT);
   step[20] = STBI_AVIF_HALF_BTF(-c[48], bf0[20], -c[16], bf0[27], COS_BIT);
   step[21] = STBI_AVIF_HALF_BTF(-c[48], bf0[21], -c[16], bf0[26], COS_BIT);
   step[22] = bf0[22];
   step[23] = bf0[23];
   step[24] = bf0[24];
   step[25] = bf0[25];
   step[26] = STBI_AVIF_HALF_BTF(-c[16], bf0[21], c[48], bf0[26], COS_BIT);
   step[27] = STBI_AVIF_HALF_BTF(-c[16], bf0[20], c[48], bf0[27], COS_BIT);
   step[28] = STBI_AVIF_HALF_BTF( c[48], bf0[19], c[16], bf0[28], COS_BIT);
   step[29] = STBI_AVIF_HALF_BTF( c[48], bf0[18], c[16], bf0[29], COS_BIT);
   step[30] = bf0[30];
   step[31] = bf0[31];
   /* stage 7 */
   bf0[0] = step[0] + step[7];
   bf0[1] = step[1] + step[6];
   bf0[2] = step[2] + step[5];
   bf0[3] = step[3] + step[4];
   bf0[4] = step[3] - step[4];
   bf0[5] = step[2] - step[5];
   bf0[6] = step[1] - step[6];
   bf0[7] = step[0] - step[7];
   bf0[8]  = step[8];
   bf0[9]  = step[9];
   bf0[10] = STBI_AVIF_HALF_BTF(-c[32], step[10], c[32], step[13], COS_BIT);
   bf0[11] = STBI_AVIF_HALF_BTF(-c[32], step[11], c[32], step[12], COS_BIT);
   bf0[12] = STBI_AVIF_HALF_BTF( c[32], step[11], c[32], step[12], COS_BIT);
   bf0[13] = STBI_AVIF_HALF_BTF( c[32], step[10], c[32], step[13], COS_BIT);
   bf0[14] = step[14];
   bf0[15] = step[15];
   bf0[16] = step[16] + step[23];
   bf0[17] = step[17] + step[22];
   bf0[18] = step[18] + step[21];
   bf0[19] = step[19] + step[20];
   bf0[20] = step[19] - step[20];
   bf0[21] = step[18] - step[21];
   bf0[22] = step[17] - step[22];
   bf0[23] = step[16] - step[23];
   bf0[24] = -step[24] + step[31];
   bf0[25] = -step[25] + step[30];
   bf0[26] = -step[26] + step[29];
   bf0[27] = -step[27] + step[28];
   bf0[28] = step[27] + step[28];
   bf0[29] = step[26] + step[29];
   bf0[30] = step[25] + step[30];
   bf0[31] = step[24] + step[31];
   /* stage 8 */
   step[0]  = bf0[0] + bf0[15];
   step[1]  = bf0[1] + bf0[14];
   step[2]  = bf0[2] + bf0[13];
   step[3]  = bf0[3] + bf0[12];
   step[4]  = bf0[4] + bf0[11];
   step[5]  = bf0[5] + bf0[10];
   step[6]  = bf0[6] + bf0[9];
   step[7]  = bf0[7] + bf0[8];
   step[8]  = bf0[7] - bf0[8];
   step[9]  = bf0[6] - bf0[9];
   step[10] = bf0[5] - bf0[10];
   step[11] = bf0[4] - bf0[11];
   step[12] = bf0[3] - bf0[12];
   step[13] = bf0[2] - bf0[13];
   step[14] = bf0[1] - bf0[14];
   step[15] = bf0[0] - bf0[15];
   step[16] = bf0[16];
   step[17] = bf0[17];
   step[18] = bf0[18];
   step[19] = bf0[19];
   step[20] = STBI_AVIF_HALF_BTF(-c[32], bf0[20], c[32], bf0[27], COS_BIT);
   step[21] = STBI_AVIF_HALF_BTF(-c[32], bf0[21], c[32], bf0[26], COS_BIT);
   step[22] = STBI_AVIF_HALF_BTF(-c[32], bf0[22], c[32], bf0[25], COS_BIT);
   step[23] = STBI_AVIF_HALF_BTF(-c[32], bf0[23], c[32], bf0[24], COS_BIT);
   step[24] = STBI_AVIF_HALF_BTF( c[32], bf0[23], c[32], bf0[24], COS_BIT);
   step[25] = STBI_AVIF_HALF_BTF( c[32], bf0[22], c[32], bf0[25], COS_BIT);
   step[26] = STBI_AVIF_HALF_BTF( c[32], bf0[21], c[32], bf0[26], COS_BIT);
   step[27] = STBI_AVIF_HALF_BTF( c[32], bf0[20], c[32], bf0[27], COS_BIT);
   step[28] = bf0[28];
   step[29] = bf0[29];
   step[30] = bf0[30];
   step[31] = bf0[31];
   /* stage 9 */
   output[0]  = step[0] + step[31];
   output[1]  = step[1] + step[30];
   output[2]  = step[2] + step[29];
   output[3]  = step[3] + step[28];
   output[4]  = step[4] + step[27];
   output[5]  = step[5] + step[26];
   output[6]  = step[6] + step[25];
   output[7]  = step[7] + step[24];
   output[8]  = step[8] + step[23];
   output[9]  = step[9] + step[22];
   output[10] = step[10] + step[21];
   output[11] = step[11] + step[20];
   output[12] = step[12] + step[19];
   output[13] = step[13] + step[18];
   output[14] = step[14] + step[17];
   output[15] = step[15] + step[16];
   output[16] = step[15] - step[16];
   output[17] = step[14] - step[17];
   output[18] = step[13] - step[18];
   output[19] = step[12] - step[19];
   output[20] = step[11] - step[20];
   output[21] = step[10] - step[21];
   output[22] = step[9] - step[22];
   output[23] = step[8] - step[23];
   output[24] = step[7] - step[24];
   output[25] = step[6] - step[25];
   output[26] = step[5] - step[26];
   output[27] = step[4] - step[27];
   output[28] = step[3] - step[28];
   output[29] = step[2] - step[29];
   output[30] = step[1] - step[30];
   output[31] = step[0] - step[31];
}

/* ---- IADST (Asymmetric DST) ---- */

static const int stbi_avif__sinpi[5] = { 0, 1321, 2482, 3344, 3803 };

static void stbi_avif__av1_iadst4(const int *input, int *output)
{
   const int *s = stbi_avif__sinpi;
   long x0 = input[0], x1 = input[1], x2 = input[2], x3 = input[3];
   long s0, s1, s2, s3, s4, s5, s6, s7;
   if (!(x0 | x1 | x2 | x3)) { output[0]=output[1]=output[2]=output[3]=0; return; }
   s0 = s[1] * x0; s1 = s[2] * x0; s2 = s[3] * x1;
   s3 = s[4] * x2; s4 = s[1] * x2; s5 = s[2] * x3; s6 = s[4] * x3;
   s7 = (x0 - x2) + x3;
   s0 = s0 + s3; s1 = s1 - s4; s3 = s2; s2 = s[3] * s7;
   s0 = s0 + s5; s1 = s1 - s6;
   x0 = s0 + s3; x1 = s1 + s3; x2 = s2; x3 = s0 + s1 - s3;
   output[0] = (int)((x0 + (1L << (COS_BIT-1))) >> COS_BIT);
   output[1] = (int)((x1 + (1L << (COS_BIT-1))) >> COS_BIT);
   output[2] = (int)((x2 + (1L << (COS_BIT-1))) >> COS_BIT);
   output[3] = (int)((x3 + (1L << (COS_BIT-1))) >> COS_BIT);
}

static void stbi_avif__av1_iadst8(const int *input, int *output)
{
   const int *c = stbi_avif__cospi;
   int bf1[8], step[8];
   /* stage 1: input reorder */
   bf1[0]=input[7]; bf1[1]=input[0]; bf1[2]=input[5]; bf1[3]=input[2];
   bf1[4]=input[3]; bf1[5]=input[4]; bf1[6]=input[1]; bf1[7]=input[6];
   /* stage 2 */
   step[0] = STBI_AVIF_HALF_BTF(c[4],  bf1[0], c[60], bf1[1], COS_BIT);
   step[1] = STBI_AVIF_HALF_BTF(c[60], bf1[0],-c[4],  bf1[1], COS_BIT);
   step[2] = STBI_AVIF_HALF_BTF(c[20], bf1[2], c[44], bf1[3], COS_BIT);
   step[3] = STBI_AVIF_HALF_BTF(c[44], bf1[2],-c[20], bf1[3], COS_BIT);
   step[4] = STBI_AVIF_HALF_BTF(c[36], bf1[4], c[28], bf1[5], COS_BIT);
   step[5] = STBI_AVIF_HALF_BTF(c[28], bf1[4],-c[36], bf1[5], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF(c[52], bf1[6], c[12], bf1[7], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[12], bf1[6],-c[52], bf1[7], COS_BIT);
   /* stage 3 */
   bf1[0]=step[0]+step[4]; bf1[1]=step[1]+step[5]; bf1[2]=step[2]+step[6]; bf1[3]=step[3]+step[7];
   bf1[4]=step[0]-step[4]; bf1[5]=step[1]-step[5]; bf1[6]=step[2]-step[6]; bf1[7]=step[3]-step[7];
   /* stage 4 */
   step[0]=bf1[0]; step[1]=bf1[1]; step[2]=bf1[2]; step[3]=bf1[3];
   step[4] = STBI_AVIF_HALF_BTF(c[16], bf1[4], c[48], bf1[5], COS_BIT);
   step[5] = STBI_AVIF_HALF_BTF(c[48], bf1[4],-c[16], bf1[5], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF(-c[48],bf1[6], c[16], bf1[7], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[16], bf1[6], c[48], bf1[7], COS_BIT);
   /* stage 5 */
   bf1[0]=step[0]+step[2]; bf1[1]=step[1]+step[3]; bf1[2]=step[0]-step[2]; bf1[3]=step[1]-step[3];
   bf1[4]=step[4]+step[6]; bf1[5]=step[5]+step[7]; bf1[6]=step[4]-step[6]; bf1[7]=step[5]-step[7];
   /* stage 6 */
   step[0]=bf1[0]; step[1]=bf1[1];
   step[2] = STBI_AVIF_HALF_BTF(c[32], bf1[2], c[32], bf1[3], COS_BIT);
   step[3] = STBI_AVIF_HALF_BTF(c[32], bf1[2],-c[32], bf1[3], COS_BIT);
   step[4]=bf1[4]; step[5]=bf1[5];
   step[6] = STBI_AVIF_HALF_BTF(c[32], bf1[6], c[32], bf1[7], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[32], bf1[6],-c[32], bf1[7], COS_BIT);
   /* stage 7: output with sign flips */
   output[0]= step[0]; output[1]=-step[4]; output[2]= step[6]; output[3]=-step[2];
   output[4]= step[3]; output[5]=-step[7]; output[6]= step[5]; output[7]=-step[1];
}

static void stbi_avif__av1_iadst16(const int *input, int *output)
{
   const int *c = stbi_avif__cospi;
   int bf1[16], step[16];
   /* stage 1 */
   bf1[0]=input[15]; bf1[1]=input[0];  bf1[2]=input[13]; bf1[3]=input[2];
   bf1[4]=input[11]; bf1[5]=input[4];  bf1[6]=input[9];  bf1[7]=input[6];
   bf1[8]=input[7];  bf1[9]=input[8];  bf1[10]=input[5]; bf1[11]=input[10];
   bf1[12]=input[3]; bf1[13]=input[12]; bf1[14]=input[1]; bf1[15]=input[14];
   /* stage 2 */
   step[0] = STBI_AVIF_HALF_BTF(c[2],  bf1[0], c[62], bf1[1], COS_BIT);
   step[1] = STBI_AVIF_HALF_BTF(c[62], bf1[0],-c[2],  bf1[1], COS_BIT);
   step[2] = STBI_AVIF_HALF_BTF(c[10], bf1[2], c[54], bf1[3], COS_BIT);
   step[3] = STBI_AVIF_HALF_BTF(c[54], bf1[2],-c[10], bf1[3], COS_BIT);
   step[4] = STBI_AVIF_HALF_BTF(c[18], bf1[4], c[46], bf1[5], COS_BIT);
   step[5] = STBI_AVIF_HALF_BTF(c[46], bf1[4],-c[18], bf1[5], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF(c[26], bf1[6], c[38], bf1[7], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[38], bf1[6],-c[26], bf1[7], COS_BIT);
   step[8] = STBI_AVIF_HALF_BTF(c[34], bf1[8], c[30], bf1[9], COS_BIT);
   step[9] = STBI_AVIF_HALF_BTF(c[30], bf1[8],-c[34], bf1[9], COS_BIT);
   step[10]= STBI_AVIF_HALF_BTF(c[42], bf1[10],c[22], bf1[11],COS_BIT);
   step[11]= STBI_AVIF_HALF_BTF(c[22], bf1[10],-c[42],bf1[11],COS_BIT);
   step[12]= STBI_AVIF_HALF_BTF(c[50], bf1[12],c[14], bf1[13],COS_BIT);
   step[13]= STBI_AVIF_HALF_BTF(c[14], bf1[12],-c[50],bf1[13],COS_BIT);
   step[14]= STBI_AVIF_HALF_BTF(c[58], bf1[14],c[6],  bf1[15],COS_BIT);
   step[15]= STBI_AVIF_HALF_BTF(c[6],  bf1[14],-c[58],bf1[15],COS_BIT);
   /* stage 3 */
   bf1[0]=step[0]+step[8];   bf1[1]=step[1]+step[9];
   bf1[2]=step[2]+step[10];  bf1[3]=step[3]+step[11];
   bf1[4]=step[4]+step[12];  bf1[5]=step[5]+step[13];
   bf1[6]=step[6]+step[14];  bf1[7]=step[7]+step[15];
   bf1[8]=step[0]-step[8];   bf1[9]=step[1]-step[9];
   bf1[10]=step[2]-step[10]; bf1[11]=step[3]-step[11];
   bf1[12]=step[4]-step[12]; bf1[13]=step[5]-step[13];
   bf1[14]=step[6]-step[14]; bf1[15]=step[7]-step[15];
   /* stage 4 */
   step[0]=bf1[0]; step[1]=bf1[1]; step[2]=bf1[2]; step[3]=bf1[3];
   step[4]=bf1[4]; step[5]=bf1[5]; step[6]=bf1[6]; step[7]=bf1[7];
   step[8] = STBI_AVIF_HALF_BTF(c[8],  bf1[8], c[56], bf1[9], COS_BIT);
   step[9] = STBI_AVIF_HALF_BTF(c[56], bf1[8],-c[8],  bf1[9], COS_BIT);
   step[10]= STBI_AVIF_HALF_BTF(c[40], bf1[10],c[24], bf1[11],COS_BIT);
   step[11]= STBI_AVIF_HALF_BTF(c[24], bf1[10],-c[40],bf1[11],COS_BIT);
   step[12]= STBI_AVIF_HALF_BTF(-c[56],bf1[12],c[8],  bf1[13],COS_BIT);
   step[13]= STBI_AVIF_HALF_BTF(c[8],  bf1[12],c[56], bf1[13],COS_BIT);
   step[14]= STBI_AVIF_HALF_BTF(-c[24],bf1[14],c[40], bf1[15],COS_BIT);
   step[15]= STBI_AVIF_HALF_BTF(c[40], bf1[14],c[24], bf1[15],COS_BIT);
   /* stage 5 */
   bf1[0]=step[0]+step[4];   bf1[1]=step[1]+step[5];
   bf1[2]=step[2]+step[6];   bf1[3]=step[3]+step[7];
   bf1[4]=step[0]-step[4];   bf1[5]=step[1]-step[5];
   bf1[6]=step[2]-step[6];   bf1[7]=step[3]-step[7];
   bf1[8]=step[8]+step[12];  bf1[9]=step[9]+step[13];
   bf1[10]=step[10]+step[14]; bf1[11]=step[11]+step[15];
   bf1[12]=step[8]-step[12]; bf1[13]=step[9]-step[13];
   bf1[14]=step[10]-step[14]; bf1[15]=step[11]-step[15];
   /* stage 6 */
   step[0]=bf1[0]; step[1]=bf1[1]; step[2]=bf1[2]; step[3]=bf1[3];
   step[4] = STBI_AVIF_HALF_BTF(c[16], bf1[4], c[48], bf1[5], COS_BIT);
   step[5] = STBI_AVIF_HALF_BTF(c[48], bf1[4],-c[16], bf1[5], COS_BIT);
   step[6] = STBI_AVIF_HALF_BTF(-c[48],bf1[6], c[16], bf1[7], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[16], bf1[6], c[48], bf1[7], COS_BIT);
   step[8]=bf1[8]; step[9]=bf1[9]; step[10]=bf1[10]; step[11]=bf1[11];
   step[12]= STBI_AVIF_HALF_BTF(c[16], bf1[12],c[48], bf1[13],COS_BIT);
   step[13]= STBI_AVIF_HALF_BTF(c[48], bf1[12],-c[16],bf1[13],COS_BIT);
   step[14]= STBI_AVIF_HALF_BTF(-c[48],bf1[14],c[16], bf1[15],COS_BIT);
   step[15]= STBI_AVIF_HALF_BTF(c[16], bf1[14],c[48], bf1[15],COS_BIT);
   /* stage 7 */
   bf1[0]=step[0]+step[2];   bf1[1]=step[1]+step[3];
   bf1[2]=step[0]-step[2];   bf1[3]=step[1]-step[3];
   bf1[4]=step[4]+step[6];   bf1[5]=step[5]+step[7];
   bf1[6]=step[4]-step[6];   bf1[7]=step[5]-step[7];
   bf1[8]=step[8]+step[10];  bf1[9]=step[9]+step[11];
   bf1[10]=step[8]-step[10]; bf1[11]=step[9]-step[11];
   bf1[12]=step[12]+step[14]; bf1[13]=step[13]+step[15];
   bf1[14]=step[12]-step[14]; bf1[15]=step[13]-step[15];
   /* stage 8 */
   step[0]=bf1[0]; step[1]=bf1[1];
   step[2] = STBI_AVIF_HALF_BTF(c[32], bf1[2], c[32], bf1[3], COS_BIT);
   step[3] = STBI_AVIF_HALF_BTF(c[32], bf1[2],-c[32], bf1[3], COS_BIT);
   step[4]=bf1[4]; step[5]=bf1[5];
   step[6] = STBI_AVIF_HALF_BTF(c[32], bf1[6], c[32], bf1[7], COS_BIT);
   step[7] = STBI_AVIF_HALF_BTF(c[32], bf1[6],-c[32], bf1[7], COS_BIT);
   step[8]=bf1[8]; step[9]=bf1[9];
   step[10]= STBI_AVIF_HALF_BTF(c[32], bf1[10],c[32], bf1[11],COS_BIT);
   step[11]= STBI_AVIF_HALF_BTF(c[32], bf1[10],-c[32],bf1[11],COS_BIT);
   step[12]=bf1[12]; step[13]=bf1[13];
   step[14]= STBI_AVIF_HALF_BTF(c[32], bf1[14],c[32], bf1[15],COS_BIT);
   step[15]= STBI_AVIF_HALF_BTF(c[32], bf1[14],-c[32],bf1[15],COS_BIT);
   /* stage 9: output with sign flips */
   output[0] = step[0];  output[1] =-step[8];  output[2] = step[12]; output[3] =-step[4];
   output[4] = step[6];  output[5] =-step[14]; output[6] = step[10]; output[7] =-step[2];
   output[8] = step[3];  output[9] =-step[11]; output[10]= step[15]; output[11]=-step[7];
   output[12]= step[5];  output[13]=-step[13]; output[14]= step[9];  output[15]=-step[1];
}

/* ---- Identity transforms ---- */
#define STBI_AVIF_NEW_SQRT2 5793
#define STBI_AVIF_NEW_SQRT2_BITS 12

static void stbi_avif__av1_iidentity4(const int *input, int *output)
{
   int i;
   for (i = 0; i < 4; ++i)
      output[i] = (int)(((long)STBI_AVIF_NEW_SQRT2 * input[i] + (1L << (STBI_AVIF_NEW_SQRT2_BITS-1))) >> STBI_AVIF_NEW_SQRT2_BITS);
}
static void stbi_avif__av1_iidentity8(const int *input, int *output)
{
   int i;
   for (i = 0; i < 8; ++i) output[i] = input[i] * 2;
}
static void stbi_avif__av1_iidentity16(const int *input, int *output)
{
   int i;
   for (i = 0; i < 16; ++i)
      output[i] = (int)(((long)STBI_AVIF_NEW_SQRT2 * 2 * input[i] + (1L << (STBI_AVIF_NEW_SQRT2_BITS-1))) >> STBI_AVIF_NEW_SQRT2_BITS);
}
static void stbi_avif__av1_iidentity32(const int *input, int *output)
{
   int i;
   for (i = 0; i < 32; ++i) output[i] = input[i] * 4;
}

/*
 * 2D inverse transform: row transform then column transform.
 * Input: coefficients in scan order (already descanned to 2D).
 * Output: residual block.
 */
/* Rectangular 2D inverse transform. txw = width in pixels (4,8,16,32),
   txh = height in pixels (4,8,16,32).
   Coefficients are laid out as coeffs[row * txw + col], row in [0,txh), col in [0,txw).
   tx_type selects sub-transforms for rows/columns.
*/
static void stbi_avif__av1_inverse_transform_2d_rect(int *coeffs, int txw, int txh, int tx_type)
{
   int buf[32];
   int temp[32 * 32];
   int i, j;
   int row_shift;
   void (*row_fn)(const int *, int *);
   void (*col_fn)(const int *, int *);
   int ud_flip = 0, lr_flip = 0;

   /* tx_type: 0=DCT_DCT, 1=ADST_DCT, 2=DCT_ADST, 3=ADST_ADST,
    * 4=FLIPADST_DCT, 5=DCT_FLIPADST, 6=FLIPADST_FLIPADST,
    * 7=ADST_FLIPADST, 8=FLIPADST_ADST, 9=IDTX, 10=V_DCT, 11=H_DCT */
   /* Row transform (horizontal, operates on txw-point data) */
   switch (tx_type) {
      case 2: case 5: case 7:
         if (txw <= 4)       row_fn = stbi_avif__av1_iadst4;
         else if (txw <= 8)  row_fn = stbi_avif__av1_iadst8;
         else if (txw <= 16) row_fn = stbi_avif__av1_iadst16;
         else                row_fn = stbi_avif__av1_idct32;
         if (tx_type == 5 || tx_type == 7) lr_flip = 1;
         break;
      case 9: case 10:
         if (txw <= 4)       row_fn = stbi_avif__av1_iidentity4;
         else if (txw <= 8)  row_fn = stbi_avif__av1_iidentity8;
         else if (txw <= 16) row_fn = stbi_avif__av1_iidentity16;
         else                row_fn = stbi_avif__av1_iidentity32;
         break;
      default:
         if (txw <= 4)       row_fn = stbi_avif__av1_idct4;
         else if (txw <= 8)  row_fn = stbi_avif__av1_idct8;
         else if (txw <= 16) row_fn = stbi_avif__av1_idct16;
         else                row_fn = stbi_avif__av1_idct32;
         break;
   }
   /* Column transform (vertical, operates on txh-point data) */
   switch (tx_type) {
      case 1: case 4: case 8:
         if (txh <= 4)       col_fn = stbi_avif__av1_iadst4;
         else if (txh <= 8)  col_fn = stbi_avif__av1_iadst8;
         else if (txh <= 16) col_fn = stbi_avif__av1_iadst16;
         else                col_fn = stbi_avif__av1_idct32;
         if (tx_type == 4 || tx_type == 8) ud_flip = 1;
         break;
      case 9: case 11:
         if (txh <= 4)       col_fn = stbi_avif__av1_iidentity4;
         else if (txh <= 8)  col_fn = stbi_avif__av1_iidentity8;
         else if (txh <= 16) col_fn = stbi_avif__av1_iidentity16;
         else                col_fn = stbi_avif__av1_iidentity32;
         break;
      default:
         if (txh <= 4)       col_fn = stbi_avif__av1_idct4;
         else if (txh <= 8)  col_fn = stbi_avif__av1_idct8;
         else if (txh <= 16) col_fn = stbi_avif__av1_idct16;
         else                col_fn = stbi_avif__av1_idct32;
         break;
   }

   /* row_shift (intermediate shift after row transforms) per AV1 spec.
    * dav1d: inv_txfm_fn84/32/16 shift parameter. */
   {
      int lw = 0, lh = 0, t = txw; while (t > 1) { ++lw; t >>= 1; }
      t = txh; while (t > 1) { ++lh; t >>= 1; }
      /* sum of log2(w)+log2(h): 4→0,5→0,6→1,7→1,8→2,9→1,10→2 */
      switch (lw + lh) {
         case 4: row_shift = 0; break;
         case 5: row_shift = 0; break;
         case 6: row_shift = 1; break;
         case 7: row_shift = 1; break;
         case 9: row_shift = 1; break;
         default: row_shift = 2; break;
      }
   }

   {
      /* rect2 scaling: non-square 2:1 ratio requires (x*181+128)>>8 on input */
      int is_rect2 = (txw == txh * 2) || (txh == txw * 2);
      /* Row transforms: for each row i of the txh×txw coeff block,
         read txw values, apply rect2 scale if needed, then txw-point row transform */
      for (i = 0; i < txh; ++i) {
         int out[32];
         if (is_rect2) {
            for (j = 0; j < txw; ++j) buf[j] = (coeffs[i * txw + j] * 181 + 128) >> 8;
         } else {
            for (j = 0; j < txw; ++j) buf[j] = coeffs[i * txw + j];
         }
         row_fn(buf, out);
         if (row_shift > 0) {
            for (j = 0; j < txw; ++j) temp[i * txw + j] = STBI_AVIF_ROUND_SHIFT(out[j], row_shift);
         } else {
            for (j = 0; j < txw; ++j) temp[i * txw + j] = out[j];
         }
      }
   }

   /* Column transforms: for each column j (0..txw-1), apply txh-point col transform */
   for (j = 0; j < txw; ++j) {
      int out[32];
      int src_col = lr_flip ? (txw - 1 - j) : j;
      for (i = 0; i < txh; ++i) buf[i] = temp[i * txw + src_col];
      col_fn(buf, out);
      if (ud_flip) {
         for (i = 0; i < txh; ++i) coeffs[i * txw + j] = STBI_AVIF_ROUND_SHIFT(out[txh - 1 - i], 4);
      } else {
         for (i = 0; i < txh; ++i) coeffs[i * txw + j] = STBI_AVIF_ROUND_SHIFT(out[i], 4);
      }
   }
}

static void stbi_avif__av1_inverse_transform_2d(int *coeffs, int sz, int tx_type)
{
   stbi_avif__av1_inverse_transform_2d_rect(coeffs, sz, sz, tx_type);
}

/*
 * =============================================================================
 *  COEFFICIENT DECODE  (AV1 spec §5.11.39 coeffs())
 * =============================================================================
 */

/*
 * Read transform coefficients for one transform block.
 * Returns the number of non-zero coefficients, or -1 on error.
 */
/*
 * Read transform coefficients AFTER txb_skip has already been checked.
 * Called when we know the block is not all-zero.
 * tx2dszctx = min(log2w,3) + min(log2h,3), used to select eob_bin CDF.
 * tx_ctx = max(log2w, log2h), used for coeff_base/br/skip CDFs.
 * txw, txh: actual TX dimensions in pixels (4..32).
 */
static int s_coeff_blk_cnt = 0; /* debug: counts plane=0 coeff blocks */

static int stbi_avif__av1_read_coeffs_after_skip(
   stbi_avif__av1_decode_ctx *ctx,
   int plane,          /* 0=Y, 1=U, 2=V */
   int tx2dszctx,      /* eob_bin context: min(log2w,3)+min(log2h,3) */
   int tx_ctx,         /* coeff CDF context: max(log2w, log2h) */
   int tx_type,
   int txw, int txh,   /* actual TX width / height in pixels */
   int *coeffs_out,    /* output: dequantized coefficient array (txw*txh) */
   int dc_qstep,
   int ac_qstep,
   int dc_sign_ctx,    /* DC sign context from neighbors (0,1,2) */
   int *out_cul_level) /* output: cul_level for entropy context update */
{
   int bhl, area;
   int tx_class = 0;
   int plane_type;
   int eob_pt, eob, k;
   int c;
   int cul_level_sum = 0;
   int dc_dqval = 0;
   int this_blk;  /* debug: block number for this Y-plane invocation */
   /* Padded level buffer: stride = txw + TX_PAD_HOR, extra rows for padding */
   unsigned char levels[(64 + STBI_AVIF_TX_PAD_HOR) * (64 + 2) + 8];
   unsigned short *scan_buf = NULL; /* dynamic scan for non-square TX */
   const unsigned short *scan;
   unsigned int sym;

   this_blk = (plane == 0) ? s_coeff_blk_cnt : -1;
   (void)tx_type;

   /* Compute tx_class: 0=2D, 1=H, 2=V */
   {
      static const signed char tclut[16] = {
         0,0,0,0,0,0,0,0,0,0,2,1,2,1,2,1
      };
      tx_class = (tx_type >= 0 && tx_type < 16) ? (int)tclut[tx_type] : 0;
   }

   /* For rectangular TX, width may differ from height. We operate on txw×txh. */
   bhl = 0; { int t = txh; while (t > 1) { ++bhl; t >>= 1; } } /* log2(txh) */
   area = txw * txh;
   plane_type = plane > 0 ? 1 : 0;

   memset(levels, 0, sizeof(levels));
   memset(coeffs_out, 0, (size_t)area * sizeof(int));

   /* Pick scan order: use static tables for square TX; generate diagonal scan for rect */
   if (tx_class == 0 && txw == txh) {
      switch (txw) {
         case  4: scan = stbi_avif__av1_scan_4x4;   break;
         case  8: scan = stbi_avif__av1_scan_8x8;   break;
         case 16: scan = stbi_avif__av1_scan_16x16; break;
         default: scan = stbi_avif__av1_scan_32x32; break;
      }
   } else {
      scan_buf = (unsigned short *)STBI_AVIF_MALLOC((size_t)area * sizeof(unsigned short));
      if (!scan_buf) return 0;
      if (tx_class == 1) {
         int i, r, cl;
         for (i = 0; i < area; ++i) {
            r = i / txw; cl = i % txw;
            scan_buf[i] = (unsigned short)(cl * txh + r);
         }
      } else if (tx_class == 2) {
         int i;
         for (i = 0; i < area; ++i) scan_buf[i] = (unsigned short)i;
      } else {
         int n = 0, d, row, rowmax, rowmin;
         for (d = 0; d < txw + txh - 1; ++d) {
            rowmax = d < txh ? d : txh - 1;
            rowmin = d - txw + 1 > 0 ? d - txw + 1 : 0;
            for (row = rowmin; row <= rowmax; ++row)
               scan_buf[n++] = (unsigned short)((d - row) * txh + row);
         }
      }
      scan = scan_buf;
   }

   /* 2. Read end-of-block position */
   /* tx2dszctx: 0=4x4,1={4x8,8x4},2={8x8,...},3={...16x32},4={16x16},5={32x16},6={32x32+} */
   /* eob_multi CDF nsyms = 5+tx2dszctx (our format: dav1d n_symbols+1 for sentinel) */
   {
      int eob_multi_ctx = (tx_class != 0) ? 1 : 0;
      unsigned int eob_pt_sym;
      if (plane == 0) { static int ec=0; if(ec<15){fprintf(stderr,"  PRE_EOB: tx2dsz=%d rng=%u dif=%llu\n",tx2dszctx,ctx->rd.rng,ctx->rd.dif);ec++;} }

      switch (tx2dszctx) {
         case 0:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi16_cdf[plane_type][eob_multi_ctx], 5);
            break;
         case 1:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi32_cdf[plane_type][eob_multi_ctx], 6);
            break;
         case 2:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi64_cdf[plane_type][eob_multi_ctx], 7);
            break;
         case 3:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi128_cdf[plane_type][eob_multi_ctx], 8);
            break;
         case 4:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi256_cdf[plane_type][eob_multi_ctx], 9);
            break;
         case 5:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi512_cdf[plane_type][eob_multi_ctx], 10);
            break;
         default:
            eob_pt_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                            ctx->eob_multi1024_cdf[plane_type][eob_multi_ctx], 11);
            break;
      }
      eob_pt = (int)eob_pt_sym; /* keep for compatibility below */
   }

   /* 3. Compute EOB from eob_pt_sym (0-indexed symbol) using dav1d formula:
      sym=0 → eob=0, sym=1 → eob=1,
      sym>1 → eob_bin=sym-2, hi_bit=adapt_bool, eob=((hi_bit|2)<<eob_bin)|literal(eob_bin)
      Then eob+1 to convert from 0-based scan position to 1-based count (our convention). */
   {
      if (eob_pt <= 1) {
         eob = eob_pt; /* sym 0 → eob=0, sym 1 → eob=1 */
      } else {
         int eob_bin = eob_pt - 2;
         int ts2 = tx_ctx < 4 ? tx_ctx : 4;
         int hi_bit, lo_bits;
         if (eob_bin >= 9) eob_bin = 8;
         hi_bit = (int)stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                     ctx->eob_extra_cdf[ts2][plane_type][eob_bin], 2);
         lo_bits = (eob_bin > 0) ? (int)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)eob_bin) : 0;
         eob = ((hi_bit | 2) << eob_bin) | lo_bits;
      }
      eob += 1; /* convert to 1-based (our internal eob convention: scan[eob-1] is last nonzero) */
   }
   if (eob > area) eob = area;
   if (eob < 1) { STBI_AVIF_FREE(scan_buf); return 0; }

   if (eob > 0 && plane == 0) {
      s_coeff_blk_cnt++;
      if (this_blk < 5) {
         fprintf(stderr, "  COEFF plane=%d tx2dsz=%d txw=%d txh=%d eob_pt=%d eob=%d area=%d dc_q=%d ac_q=%d rd_dif=%llu rd_rng=%u blk=%d\n",
            plane, tx2dszctx, txw, txh, eob_pt, eob, area, dc_qstep, ac_qstep, ctx->rd.dif, ctx->rd.rng, this_blk);
      }
   }

   /* 4. Read coefficient levels (reverse scan order) */

   /* 4a. Read the EOB position coefficient (scan index eob-1) */
   {
      int pos = (int)scan[eob - 1];
      int coeff_ctx = stbi_avif__av1_get_lower_levels_ctx_eob(bhl, txw, eob - 1);
      int level;
      int ts = tx_ctx < 4 ? tx_ctx : 4;
      if (plane == 0 && this_blk < 3) {
            fprintf(stderr, "  EOB_COEFF: scan_idx=%d pos=%d ctx=%d ts=%d pt=%d cdf=[%u %u %u %u]\n",
               eob-1, pos, coeff_ctx, ts, plane_type,
               ctx->coeff_base_eob_cdf[ts][plane_type][coeff_ctx][0],
               ctx->coeff_base_eob_cdf[ts][plane_type][coeff_ctx][1],
               ctx->coeff_base_eob_cdf[ts][plane_type][coeff_ctx][2],
               ctx->coeff_base_eob_cdf[ts][plane_type][coeff_ctx][3]);
      }
      sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->coeff_base_eob_cdf[ts][plane_type][coeff_ctx], 3);
      level = (int)sym + 1; /* EOB coeff is always >= 1 */
      if (plane == 0 && this_blk < 3) {
            fprintf(stderr, "  EOB_LVL: base_sym=%u level=%d rng=%u\n", sym, level, ctx->rd.rng);
      }
      if (level > 2) { /* NUM_BASE_LEVELS = 2 */
         /* BR context for EOB position */
         int col = pos >> bhl, row = pos - (col << bhl);
         int br_ctx;
         if (pos == 0) br_ctx = 0;
         else if (tx_class != 0 ? (row != 0) : ((row | col) < 2)) br_ctx = 7;
         else br_ctx = 14;
         if (plane == 0 && this_blk < 3) {
               fprintf(stderr, "  EOB_BR: pos=%d row=%d col=%d br_ctx=%d\n", pos, row, col, br_ctx);
         }
         {
            int ts2 = tx_ctx < 4 ? tx_ctx : 3;
            if (plane == 0 && this_blk < 3) {
                  fprintf(stderr, "  BR_LOOP: ts2=%d br_ctx=%d cdf=[%u %u %u] cnt=%u rng=%u dif_init=%llu\n",
                     ts2, br_ctx,
                     ctx->coeff_br_cdf[ts2][plane_type][br_ctx][0],
                     ctx->coeff_br_cdf[ts2][plane_type][br_ctx][1],
                     ctx->coeff_br_cdf[ts2][plane_type][br_ctx][2],
                     ctx->coeff_br_cdf[ts2][plane_type][br_ctx][3],
                     ctx->rd.rng, ctx->rd.dif);
            }
            for (k = 0; k < 4; ++k) {
               sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx], 4);
               if (plane == 0 && this_blk < 3) {
                     fprintf(stderr, "  BR_SYM k=%d sym=%u level=%d rng=%u dif=%llu cdf=[%u %u %u] cnt=%u\n", k, sym, level+(int)sym, ctx->rd.rng, ctx->rd.dif,
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx][0],
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx][1],
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx][2],
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx][3]);
               }
               level += (int)sym;
               if (sym < 3) break;
            }
         }
      }
      levels[stbi_avif__av1_get_padded_idx(pos, bhl)] =
         (unsigned char)(level < 255 ? level : 255);
   }

   /* 4b. Read remaining coefficients (scan indices eob-2 down to 1) */
   if (eob > 1) {
      int ts = tx_ctx < 4 ? tx_ctx : 4;
      int ts2 = tx_ctx < 4 ? tx_ctx : 3;
      for (c = eob - 2; c >= 1; --c) {
         int pos = (int)scan[c];
         int coeff_ctx = (tx_class != 0)
            ? stbi_avif__av1_get_lower_levels_ctx_1d(levels, pos, bhl)
            : stbi_avif__av1_get_lower_levels_ctx_2d(levels, pos, bhl, txw);
         int level;
         if (coeff_ctx >= 42) coeff_ctx = 41;
         if (plane == 0 && this_blk < 3) {
               fprintf(stderr, "  RC c=%d pos=%d ctx=%d lvl_before_sym? cdf=[%u %u %u %u %u] rng=%u\n",
                  c, pos, coeff_ctx,
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][0],
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][1],
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][2],
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][3],
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][4], ctx->rd.rng);
         }
         sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx], 4);
         level = (int)sym;
         if (plane == 0 && this_blk < 3) {
            fprintf(stderr, "  RC_SYM c=%d pos=%d sym=%u rng=%u\n", c, pos, sym, ctx->rd.rng);
         }
         if (level > 2) { /* NUM_BASE_LEVELS = 2 */
            int br_ctx = stbi_avif__av1_get_br_ctx_2d(levels, pos, bhl);
            for (k = 0; k < 4; ++k) {
               sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx], 4);
               level += (int)sym;
               if (plane == 0 && this_blk < 3) {
                  fprintf(stderr, "  RC_BR c=%d k=%d sym=%u level=%d rng=%u\n", c, k, sym, level, ctx->rd.rng);
               }
               if (sym < 3) break;
            }
         }
         if (plane == 0 && this_blk < 3 && level > 2) {
            fprintf(stderr, "  RC_FINAL c=%d pos=%d level=%d\n", c, pos, level);
         }
         levels[stbi_avif__av1_get_padded_idx(pos, bhl)] =
            (unsigned char)(level < 255 ? level : 255);
      }

      /* 4c. Read DC coefficient (scan index 0) */
      {
         int pos = (int)scan[0];
         /* DC: TX_CLASS_2D=0, else use 1d ctx */
         int coeff_ctx = (tx_class != 0)
            ? stbi_avif__av1_get_lower_levels_ctx_1d(levels, pos, bhl)
            : 0;
         int level;
         if (coeff_ctx >= 42) coeff_ctx = 41;
         if (plane == 0 && this_blk < 3) {
            fprintf(stderr, "  DC c=0 pos=%d ctx=%d cdf=[%u %u %u %u %u] rng=%u\n",
               pos, coeff_ctx,
               ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][0],
               ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][1],
               ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][2],
               ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][3],
               ctx->coeff_base_cdf[ts][plane_type][coeff_ctx][4], ctx->rd.rng);
         }
         sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                  ctx->coeff_base_cdf[ts][plane_type][coeff_ctx], 4);
         level = (int)sym;
         if (plane == 0 && this_blk < 3) {
            fprintf(stderr, "  DC_SYM sym=%u level=%d rng=%u\n", sym, level, ctx->rd.rng);
         }
         if (level > 2) {
            int padded_dc = stbi_avif__av1_get_padded_idx(pos, bhl);
            int br_ctx = stbi_avif__av1_get_br_ctx_dc(levels + padded_dc, bhl);
            if (plane == 0 && this_blk < 3) {
               int stride_dc = (1 << bhl) + STBI_AVIF_TX_PAD_HOR;
               fprintf(stderr, "  DC_BR br_ctx=%d lv[1]=%d lv[stride]=%d lv[stride+1]=%d cdf=[%u %u %u] cnt=%u rng=%u\n",
                  br_ctx,
                  (int)levels[padded_dc+1], (int)levels[padded_dc+stride_dc], (int)levels[padded_dc+stride_dc+1],
                  ctx->coeff_br_cdf[ts2][plane_type][br_ctx][0],
                  ctx->coeff_br_cdf[ts2][plane_type][br_ctx][1],
                  ctx->coeff_br_cdf[ts2][plane_type][br_ctx][2],
                  ctx->coeff_br_cdf[ts2][plane_type][br_ctx][3], ctx->rd.rng);
            }
            for (k = 0; k < 4; ++k) {
               sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                        ctx->coeff_br_cdf[ts2][plane_type][br_ctx], 4);
               level += (int)sym;
               if (plane == 0 && this_blk < 3) {
                  fprintf(stderr, "  DC_BR_SYM k=%d sym=%u level=%d rng=%u\n", k, sym, level, ctx->rd.rng);
               }
               if (sym < 3) break;
            }
         }
         levels[stbi_avif__av1_get_padded_idx(pos, bhl)] =
            (unsigned char)(level < 255 ? level : 255);
      }
   }

   /* Debug: print first few levels */
   if (plane == 0 && this_blk < 3) {
      int di;
      fprintf(stderr, "  LEVELS[scan 0..9] blk=%d: ", this_blk);
      for (di = 0; di < 10 && di < eob; ++di) {
         int pos = (int)scan[di];
         fprintf(stderr, "%d ", (int)levels[stbi_avif__av1_get_padded_idx(pos, bhl)]);
      }
      fprintf(stderr, "\n");
   }

   /* 5. Read signs and dequantize */
   {
      for (c = 0; c < eob; ++c) {
         int pos = (int)scan[c];
         int padded = stbi_avif__av1_get_padded_idx(pos, bhl);
         int lvl = (int)levels[padded];
         int sign, qstep, dequant_val;

         if (lvl == 0) continue;
         cul_level_sum += lvl;

         /* Read sign */
         if (c == 0) {
            sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                     ctx->dc_sign_cdf[plane_type][dc_sign_ctx], 2);
            sign = (int)sym;
            if (lvl > 0) { static int sc=0; if(sc<5) { fprintf(stderr, "  DC_SIGN: sym=%u rng=%u lvl=%d\n", sym, ctx->rd.rng, lvl); sc++; } }
         } else {
            sym = stbi_avif__av1_read_symbol(&ctx->rd,
                     stbi_avif__av1_half_cdf, 2);
            sign = (int)sym;
         }

         /* Read Golomb remainder for large levels */
         if (lvl >= 15) { /* MAX_BASE_BR_RANGE = 15 */
            int golomb_len = 0;
            unsigned int golomb_bit;
            int remainder = 0;
            while (golomb_len < 32) {
               golomb_bit = stbi_avif__av1_read_symbol(&ctx->rd,
                              stbi_avif__av1_half_cdf, 2);
               if (golomb_bit) break;  /* stop on 1 */
               ++golomb_len;
            }
            for (k = golomb_len - 1; k >= 0; --k) {
               golomb_bit = stbi_avif__av1_read_symbol(&ctx->rd,
                              stbi_avif__av1_half_cdf, 2);
               remainder |= ((int)golomb_bit << k);
            }
            lvl += (1 << golomb_len) - 1 + remainder;
            if (pos == 0) {
               static int dc_golomb_dbg = 0;
               if (dc_golomb_dbg < 3) {
                  fprintf(stderr, "  DC_GOLOMB: base_lvl=15 golomb_len=%d remainder=%d final_lvl=%d rng=%u\n",
                     golomb_len, remainder, lvl, ctx->rd.rng);
                  dc_golomb_dbg++;
               }
            }
         }

         /* Dequantize */
         qstep = (pos == 0) ? dc_qstep : ac_qstep;
         dequant_val = lvl * qstep;
         /* Apply TX scale: shift >>1 for 32x32 and larger */
         if (tx2dszctx >= 6)
            dequant_val >>= 2;
         else if (tx2dszctx >= 5 || (txw >= 32 || txh >= 32))
            dequant_val >>= 1;
         if (sign) dequant_val = -dequant_val;

         if (pos < area) {
            /* pos is column-major: col * txh + row. Convert to row-major for IDCT. */
            int col = pos >> bhl; /* bhl = log2(txh), so col = pos / txh */
            int row = pos - (col << bhl);
            coeffs_out[row * txw + col] = dequant_val;
         }

         /* Track DC value for cul_level sign */
         if (pos == 0)
            dc_dqval = dequant_val;
      }
   }

   /* Compute cul_level: sum of levels clamped to 7, with DC sign in bits 3+ */
   {
      int cl = cul_level_sum;
      if (cl > 7) cl = 7;  /* COEFF_CONTEXT_MASK = 7 */
      /* set_dc_sign: dc_dqval < 0 → bit3=1 (sign=1); dc_dqval > 0 → bit3=2 (sign=2) */
      if (dc_dqval < 0)
         cl |= (1 << 3);
      else if (dc_dqval > 0)
         cl |= (2 << 3);
      if (out_cul_level) *out_cul_level = cl;
   }

   STBI_AVIF_FREE(scan_buf);
   return eob;
}

/*
 * Full coefficient read: reads txb_skip + coefficients.
 * Used for UV planes where TX type is not read between skip and coefficients.
 * tx2dszctx = min(log2w,3) + min(log2h,3) for eob_bin CDF selection.
 * txw, txh: actual TX dimensions in pixels (power-of-2, 4..32).
 */
static int stbi_avif__av1_read_coeffs(
   stbi_avif__av1_decode_ctx *ctx,
   int plane,
   int tx_size,        /* square CDF index 0..4 for txb_skip */
   int tx_type,
   int tx2dszctx,      /* eob_bin context = min(log2w,3)+min(log2h,3) */
   int tx_ctx,         /* coeff CDF context = max(log2w, log2h) */
   int txw, int txh,   /* actual TX width/height in pixels */
   int *coeffs_out,
   int dc_qstep,
   int ac_qstep,
   int txb_skip_ctx,
   int dc_sign_ctx,
   int *out_cul_level)
{
   unsigned int sym;
   int ts = tx_size;
   if (ts > 4) ts = 4;
   if (plane > 0 && txb_skip_ctx >= 7) fprintf(stderr, "  UV_SKIP_DECODE: plane=%d ts=%d ctx=%d cdf0=%u rng_pre=%u\n",
      plane, ts, txb_skip_ctx, (unsigned)ctx->txb_skip_cdf[ts][txb_skip_ctx][0], ctx->rd.rng);
   sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
            ctx->txb_skip_cdf[ts][txb_skip_ctx], 2);
   if (plane > 0 && txb_skip_ctx >= 7) fprintf(stderr, "  UV_SKIP_RESULT: sym=%u rng_post=%u\n", sym, ctx->rd.rng);
   if (sym) {
      memset(coeffs_out, 0, (size_t)(txw * txh) * sizeof(int));
      if (out_cul_level) *out_cul_level = 0;
      return 0;
   }
   return stbi_avif__av1_read_coeffs_after_skip(ctx, plane, tx2dszctx, tx_ctx, tx_type,
      txw, txh, coeffs_out, dc_qstep, ac_qstep, dc_sign_ctx, out_cul_level);
}

static int stbi_avif__dbg_recon_cnt = 0;

/*
 * Reconstruct a transform block: inverse transform + add to prediction plane.
 * txw, txh: actual TX dimensions in pixels (already capped at 32).
 */
static void stbi_avif__av1_reconstruct_tx_block(
   unsigned short *plane, unsigned int stride,
   unsigned int plane_w, unsigned int plane_h,
   unsigned int bx, unsigned int by,
   unsigned int txw, unsigned int txh,
   int *coeffs, int tx_type,
   unsigned int bit_depth)
{
   unsigned int x, y;
   int w = (int)txw, h = (int)txh;
   if ((unsigned int)w > plane_w - bx) w = (int)(plane_w - bx);
   if ((unsigned int)h > plane_h - by) h = (int)(plane_h - by);

   /* Inverse transform in place */
   if (stbi_avif__dbg_recon_cnt < 3) {
      fprintf(stderr, "  PRE-IDCT bx=%u by=%u txw=%d txh=%d coeff[0]=%d coeff[1]=%d\n",
         bx, by, (int)txw, (int)txh, coeffs[0], coeffs[1]);
   }
   stbi_avif__av1_inverse_transform_2d_rect(coeffs, (int)txw, (int)txh, tx_type);

   if (stbi_avif__dbg_recon_cnt < 3) {
      fprintf(stderr, "  POST-IDCT bx=%u by=%u txw=%d txh=%d coeff[0]=%d coeff[1]=%d\n",
         bx, by, (int)txw, (int)txh, coeffs[0], coeffs[1]);
      stbi_avif__dbg_recon_cnt++;
   }

   /* Add residual to prediction */
   for (y = 0; y < (unsigned int)h; ++y)
   {
      for (x = 0; x < (unsigned int)w; ++x)
      {
         int pred = (int)plane[(by + y) * stride + (bx + x)];
         int res  = coeffs[y * (int)txw + x];
         if (stbi_avif__dbg_recon_cnt <= 3 && by + y == 0 && bx + x == 0)
            fprintf(stderr, "  RECON[0,0]: pred=%d res=%d sum=%d\n", pred, res, pred+res);
         plane[(by + y) * stride + (bx + x)] = stbi_avif__av1_clip_sample(pred + res, bit_depth);
      }
   }
}



/*
 * =============================================================================
 *  RECURSIVE PARTITION DECODE (WITH FULL INTRA COEFFICIENT DECODE)
 * =============================================================================
 */

static int stbi_avif__av1_decode_partition(stbi_avif__av1_decode_ctx *ctx,
                                            unsigned int mi_row, unsigned int mi_col,
                                            int block_size);

static int stbi_avif__av1_decode_coding_unit(stbi_avif__av1_decode_ctx *ctx,
                                              unsigned int mi_row, unsigned int mi_col,
                                              int block_size)
{
   unsigned int bw4 = 1u << stbi_avif__bsize_log2w[block_size];
   unsigned int bh4 = 1u << stbi_avif__bsize_log2h[block_size];
   unsigned int px   = mi_col * 4u;
   unsigned int py   = mi_row * 4u;
   unsigned int pw   = bw4   * 4u;
   unsigned int ph   = bh4   * 4u;
   unsigned int above_mode, left_mode;
   unsigned int above_ctx, left_ctx;
   unsigned int y_mode, uv_mode;
   int skip;
   unsigned int tx_size;      /* square tx index 0..3 (for CDF table index) */
   unsigned int tx_split[2];  /* tx split bitmask depth0/depth1 */
   unsigned int max_tx_log2w = 0, max_tx_log2h = 0; /* max TX dims for block */
   unsigned int tx_log2w;     /* actual tx width  in log2 pixels: 0=4,1=8,2=16,3=32 */
   unsigned int tx_log2h;     /* actual tx height in log2 pixels: 0=4,1=8,2=16,3=32 */
   int coeffs[32 * 32];
   unsigned int cpx, cpy, cpw, cph, uv_tx_size, uv_tx_sz, uv_tx_szw, uv_tx_szh, uv_mode_raw;
   int palette_y_size, palette_uv_size;
   unsigned short palette_y_colors[8];
   unsigned short palette_uv_u_colors[8], palette_uv_v_colors[8];
   unsigned char palette_y_map[64 * 64]; /* max block 64x64 in 4x4 units */
   unsigned char palette_uv_map[64 * 64];

   if (px >= ctx->planes->width)  return 1;
   if (py >= ctx->planes->height) return 1;
   if (py < 64u && px < 128u) fprintf(stderr, "DCU(%u,%u) bs=%d rng=%u\n", px, py, block_size, ctx->rd.rng);
   palette_y_size = 0;
   palette_uv_size = 0;
   if (px + pw > ctx->planes->width)  pw = ctx->planes->width  - px;
   if (py + ph > ctx->planes->height) ph = ctx->planes->height - py;

   /* Chroma dimensions */
   cpx = px >> ctx->planes->subx;
   cpy = py >> ctx->planes->suby;
   cpw = (pw + (unsigned int)ctx->planes->subx) >> ctx->planes->subx;
   cph = (ph + (unsigned int)ctx->planes->suby) >> ctx->planes->suby;
   if (cpx + cpw > ctx->planes->cw) cpw = ctx->planes->cw - cpx;
   if (cpy + cph > ctx->planes->ch) cph = ctx->planes->ch - cpy;

   /* ======== MODE INFO ======== */

   /* KF Y mode context */
   above_mode = (mi_row > 0 && mi_col < ctx->mi_cols) ? ctx->above_modes[mi_col] : 0u;
   left_mode  = (mi_col > 0 && mi_row < ctx->mi_rows) ? ctx->left_modes[mi_row]  : 0u;
   if (above_mode >= 13u) above_mode = 0u;
   if (left_mode >= 13u) left_mode = 0u;
   above_ctx = (unsigned int)stbi_avif__av1_intra_mode_ctx[above_mode];
   left_ctx  = (unsigned int)stbi_avif__av1_intra_mode_ctx[left_mode];

   /* Skip flag — context is above_skip + left_skip */
   {
      unsigned int skip_ctx_above = (mi_row > 0u) ? ctx->above_skip[mi_col] : 0u;
      unsigned int skip_ctx_left  = (mi_col > 0u) ? ctx->left_skip[mi_row]  : 0u;
      unsigned int skip_ctx = skip_ctx_above + skip_ctx_left;
      skip = (int)stbi_avif__av1_read_symbol_adapt(&ctx->rd, ctx->skip_cdf[skip_ctx], 2);
   }

   /* CDEF index: read for first non-skip block in each 64x64 CDEF unit.
    * With 64x64 SBs, this is the first non-skip block at SB origin. */
   if (ctx->cdef_bits > 0 && !skip) {
      unsigned int sb_mask = (ctx->sb_size_mi - 1u);
      unsigned int mi_row_in_sb = mi_row & sb_mask;
      unsigned int mi_col_in_sb = mi_col & sb_mask;
      /* CDEF unit is 64x64 pixels = 16 mi */
      unsigned int cdef_mask = 15u;
      unsigned int cdef_row = mi_row & cdef_mask;
      unsigned int cdef_col = mi_col & cdef_mask;
      unsigned int cdef_index = 0u;
      if (ctx->sb_size_mi == 32u) {
         /* 128x128 SB: 4 CDEF units per SB */
         cdef_index = ((mi_row & 16u) ? 2u : 0u) + ((mi_col & 16u) ? 1u : 0u);
      }
      if (mi_row_in_sb == 0u && mi_col_in_sb == 0u) {
         /* Reset CDEF transmitted flags at SB start */
         ctx->cdef_transmitted[0] = 0;
         ctx->cdef_transmitted[1] = 0;
         ctx->cdef_transmitted[2] = 0;
         ctx->cdef_transmitted[3] = 0;
      }
      if (!ctx->cdef_transmitted[cdef_index] && cdef_row == 0u && cdef_col == 0u) {
         /* Actually we need to check if this is the first non-skip block in this CDEF unit,
          * not just the origin. Simplify: read at the CDEF unit origin block. */
      }
      if (!ctx->cdef_transmitted[cdef_index]) {
         unsigned int cdef_val;
         cdef_val = stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)ctx->cdef_bits);
         ctx->cdef_transmitted[cdef_index] = 1;
         (void)cdef_val; /* We don't apply CDEF filtering */
      }
   }

   /* Y intra mode */
   y_mode = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->kf_y_mode_cdf[above_ctx][left_ctx], 13);

   if (py < 64u) fprintf(stderr, "CU(%u,%u) bs=%d skip=%d y_mode=%u actx=%u lctx=%u rd_dif=%llu rd_rng=%u\n",
      px, py, block_size, skip, y_mode, above_ctx, left_ctx, (unsigned long long)ctx->rd.dif, ctx->rd.rng);

   /* Update mode map and skip context */
   {
      unsigned int mi_c, mi_r;
      unsigned char sk = (unsigned char)skip;
      for (mi_c = mi_col; mi_c < mi_col + bw4 && mi_c < ctx->mi_cols; ++mi_c) {
         ctx->above_modes[mi_c] = (unsigned char)y_mode;
         ctx->above_skip[mi_c]  = sk;
      }
      for (mi_r = mi_row; mi_r < mi_row + bh4 && mi_r < ctx->mi_rows; ++mi_r) {
         ctx->left_modes[mi_r] = (unsigned char)y_mode;
         ctx->left_skip[mi_r]  = sk;
      }
   }

   /* Angle delta for directional Y modes (1-8) on blocks >= BLOCK_8X8 */
   if (y_mode >= 1u && y_mode <= 8u && block_size >= STBI_AVIF_BLOCK_8X8) {
      stbi_avif__av1_read_symbol_adapt(&ctx->rd,
         ctx->angle_delta_cdf[y_mode - 1u], 7);
   }

   /* UV mode (must be read before residual per AV1 spec) */
   uv_mode = 0u;
   {
      int cfl_allowed = (pw <= 32u && ph <= 32u); /* CFL only for blocks ≤ 32x32 */
      if (cpw >= 4u && cph >= 4u) {
         if (cfl_allowed)
            uv_mode = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->uv_mode_cdf_cfl[y_mode < 13 ? y_mode : 0], 14);
         else
            uv_mode = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->uv_mode_cdf_no_cfl[y_mode < 13 ? y_mode : 0], 13);
      }
   }

   /* UV angle delta */
   if (uv_mode >= 1u && uv_mode <= 8u && block_size >= STBI_AVIF_BLOCK_8X8)
      stbi_avif__av1_read_symbol_adapt(&ctx->rd, ctx->angle_delta_cdf[uv_mode - 1u], 7);
   if (py < 32u) fprintf(stderr, "  post-uvmode: uv_mode=%u rng=%u dif=%llu\n", uv_mode, ctx->rd.rng, (unsigned long long)ctx->rd.dif);

   /* CFL_PRED */
   uv_mode_raw = uv_mode;
   if (uv_mode == 13u) {
      if (py < 32u) fprintf(stderr, "  pre-CFL-sign: rng=%u dif=%llu cdf={%u,%u,%u,%u,%u,%u,%u}\n",
         ctx->rd.rng, (unsigned long long)ctx->rd.dif,
         ctx->cfl_sign_cdf[0], ctx->cfl_sign_cdf[1], ctx->cfl_sign_cdf[2],
         ctx->cfl_sign_cdf[3], ctx->cfl_sign_cdf[4], ctx->cfl_sign_cdf[5], ctx->cfl_sign_cdf[6]);
      unsigned int cfl_sign_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd, ctx->cfl_sign_cdf, 8) + 1u;
      unsigned int sign_u = cfl_sign_sym / 3u;
      unsigned int sign_v = cfl_sign_sym - sign_u * 3u;
      if (py < 32u) fprintf(stderr, "  post-CFL-sign: sym=%u sign_u=%u sign_v=%u rng=%u dif=%llu\n", cfl_sign_sym-1u, sign_u, sign_v, ctx->rd.rng, (unsigned long long)ctx->rd.dif);
      if (sign_u > 0u) {
         unsigned int alpha_ctx_u = (sign_u == 2u) ? 3u + sign_v : sign_v;
         stbi_avif__av1_read_symbol_adapt(&ctx->rd, ctx->cfl_alpha_cdf[alpha_ctx_u], 16);
         if (py < 32u) fprintf(stderr, "  post-CFL-alpha-u: ctx=%u rng=%u dif=%llu\n", alpha_ctx_u, ctx->rd.rng, (unsigned long long)ctx->rd.dif);
      }
      if (sign_v > 0u) {
         unsigned int alpha_ctx_v = (sign_v == 2u) ? 3u + sign_u : sign_u;
         stbi_avif__av1_read_symbol_adapt(&ctx->rd, ctx->cfl_alpha_cdf[alpha_ctx_v], 16);
         if (py < 32u) fprintf(stderr, "  post-CFL-alpha-v: ctx=%u rng=%u dif=%llu\n", alpha_ctx_v, ctx->rd.rng, (unsigned long long)ctx->rd.dif);
      }
      uv_mode = 0u; /* CFL uses DC_PRED as base for prediction */
      if (py < 32u) fprintf(stderr, "  post-CFL: rng=%u dif=%llu\n", ctx->rd.rng, (unsigned long long)ctx->rd.dif);
   }

   /* Palette mode info (AV1 spec: read between CFL and TX size) */
   {
      int allow_palette = ctx->allow_screen_content_tools
                          && pw <= 64u && ph <= 64u
                          && block_size >= STBI_AVIF_BLOCK_8X8;
      if (allow_palette) {
         int pal_bctx = (int)stbi_avif__bsize_log2w[block_size]
                       + (int)stbi_avif__bsize_log2h[block_size] - 2;
         if (pal_bctx < 0) pal_bctx = 0;
         if (pal_bctx > 6) pal_bctx = 6;
         if (y_mode == 0u) { /* DC_PRED */
            unsigned int pal_flag = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->palette_y_mode_cdf[pal_bctx][0], 2);
            if (pal_flag) {
               /* Read palette Y size: 7-symbol CDF, result = symbol + 2 → [2..8] */
               unsigned int pal_size_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                  ctx->palette_y_size_cdf[pal_bctx], 7);
               palette_y_size = (int)pal_size_sym + 2;

               /* Read palette Y colors (cache=0 since we don't track neighbors) */
               {
                  int n = palette_y_size, idx = 0, bd = ctx->planes->bit_depth;
                  /* No cache (n_cache=0), so read all colors directly */
                  palette_y_colors[idx++] = (unsigned short)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bd);
                  if (idx < n) {
                     int min_bits = bd - 3;
                     int bits = min_bits + (int)stbi_avif__av1_read_literal(&ctx->rd, 2);
                     int range = (1 << bd) - (int)palette_y_colors[idx - 1] - 1;
                     for (; idx < n; ++idx) {
                        int delta = (int)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bits) + 1;
                        int val = (int)palette_y_colors[idx - 1] + delta;
                        if (val > (1 << bd) - 1) val = (1 << bd) - 1;
                        palette_y_colors[idx] = (unsigned short)val;
                        range -= (val - (int)palette_y_colors[idx - 1]);
                        if (range > 0) {
                           int log2r = 0, t = range;
                           while (t > 1) { ++log2r; t >>= 1; }
                           /* ceil_log2 */
                           if ((1 << log2r) < range) ++log2r;
                           if (log2r < bits) bits = log2r;
                        } else {
                           bits = 0;
                        }
                     }
                  }
               }

               /* Read palette Y color map */
               {
                  int n = palette_y_size;
                  int rows = (int)ph / 4; /* block height in 4x4 units = MI units */
                  int cols = (int)pw / 4;
                  int plane_w = cols; /* for palette map stride */
                  int i, j;
                  static const int weights[3] = { 2, 1, 2 };
                  static const int hash_muls[3] = { 1, 2, 2 };
                  if (rows < 1) rows = 1;
                  if (cols < 1) cols = 1;

                  /* First pixel: uniform */
                  palette_y_map[0] = (unsigned char)stbi_avif__av1_read_uniform(&ctx->rd, (unsigned int)n);

                  /* Wavefront decode */
                  for (i = 1; i < rows + cols - 1; ++i) {
                     int jstart = i < cols - 1 ? i : cols - 1;
                     int jend = i - rows + 1 > 0 ? i - rows + 1 : 0;
                     for (j = jstart; j >= jend; --j) {
                        int r = i - j, c = j;
                        int nb[3], scores[8], k, max_idx, max_score;
                        unsigned char color_order[8];
                        int color_ctx, color_idx, ctx_hash;

                        nb[0] = (c - 1 >= 0) ? (int)palette_y_map[r * plane_w + c - 1] : -1;
                        nb[1] = (c - 1 >= 0 && r - 1 >= 0) ? (int)palette_y_map[(r-1) * plane_w + c - 1] : -1;
                        nb[2] = (r - 1 >= 0) ? (int)palette_y_map[(r-1) * plane_w + c] : -1;

                        for (k = 0; k < 8; ++k) { scores[k] = 0; color_order[k] = (unsigned char)k; }
                        for (k = 0; k < 3; ++k)
                           if (nb[k] >= 0) scores[nb[k]] += weights[k];

                        /* Sort top 3 by score (descending) */
                        for (k = 0; k < 3; ++k) {
                           int m;
                           max_score = scores[k]; max_idx = k;
                           for (m = k + 1; m < n; ++m)
                              if (scores[m] > max_score) { max_score = scores[m]; max_idx = m; }
                           if (max_idx != k) {
                              int tmp_s = scores[max_idx]; unsigned char tmp_c = color_order[max_idx];
                              for (m = max_idx; m > k; --m) { scores[m] = scores[m-1]; color_order[m] = color_order[m-1]; }
                              scores[k] = tmp_s; color_order[k] = tmp_c;
                           }
                        }

                        ctx_hash = 0;
                        for (k = 0; k < 3; ++k) ctx_hash += scores[k] * hash_muls[k];
                        if (ctx_hash < 0 || ctx_hash > 8) return stbi_avif__fail("palette ctx_hash out of range");
                        color_ctx = stbi_avif__av1_palette_color_index_ctx_lookup[ctx_hash];
                        if (color_ctx < 0 || color_ctx >= 5) {
                           fprintf(stderr, "BAD Y color_ctx=%d hash=%d r=%d c=%d n=%d nb=[%d,%d,%d] scores=[%d,%d,%d]\n",
                              color_ctx, ctx_hash, r, c, n, nb[0], nb[1], nb[2], scores[0], scores[1], scores[2]);
                           return stbi_avif__fail("palette color_ctx out of range");
                        }

                        color_idx = (int)stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                           ctx->palette_y_color_index_cdf[n - 2][color_ctx], n);
                        palette_y_map[r * plane_w + c] = color_order[color_idx];
                     }
                  }
                  /* Extend last col/row */
                  if (cols < plane_w) {
                     for (i = 0; i < rows; ++i)
                        for (j = cols; j < plane_w; ++j)
                           palette_y_map[i * plane_w + j] = palette_y_map[i * plane_w + cols - 1];
                  }
                  for (i = rows; i < (int)ph / 4; ++i)
                     for (j = 0; j < plane_w; ++j)
                        palette_y_map[i * plane_w + j] = palette_y_map[(rows - 1) * plane_w + j];
               }
            }
         }
         if (uv_mode_raw == 0u && cpw >= 4u && cph >= 4u) { /* UV DC_PRED (not CFL) */
            unsigned int pal_uv_ctx = palette_y_size > 0 ? 1u : 0u;
            unsigned int pal_uv_flag = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->palette_uv_mode_cdf[pal_uv_ctx], 2);
            if (pal_uv_flag) {
               /* Read palette UV size */
               unsigned int pal_uv_size_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                  ctx->palette_uv_size_cdf[pal_bctx], 7);
               palette_uv_size = (int)pal_uv_size_sym + 2;

               /* Read U colors (no cache) */
               {
                  int n = palette_uv_size, idx = 0, bd = ctx->planes->bit_depth;
                  palette_uv_u_colors[idx++] = (unsigned short)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bd);
                  if (idx < n) {
                     int min_bits = bd - 3;
                     int bits = min_bits + (int)stbi_avif__av1_read_literal(&ctx->rd, 2);
                     int range = (1 << bd) - (int)palette_uv_u_colors[idx - 1];
                     for (; idx < n; ++idx) {
                        int delta = (int)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bits);
                        int val = (int)palette_uv_u_colors[idx - 1] + delta;
                        if (val > (1 << bd) - 1) val = (1 << bd) - 1;
                        palette_uv_u_colors[idx] = (unsigned short)val;
                        range -= (val - (int)palette_uv_u_colors[idx - 1]);
                        if (range > 0) {
                           int log2r = 0, t = range;
                           while (t > 1) { ++log2r; t >>= 1; }
                           if ((1 << log2r) < range) ++log2r;
                           if (log2r < bits) bits = log2r;
                        } else {
                           bits = 0;
                        }
                     }
                  }
               }

               /* Read V colors */
               {
                  int n = palette_uv_size, bd = ctx->planes->bit_depth;
                  unsigned int v_delta_flag = stbi_avif__av1_read_literal(&ctx->rd, 1);
                  if (v_delta_flag) {
                     int min_bits_v = bd - 4;
                     int max_val = 1 << bd;
                     int bits = min_bits_v + (int)stbi_avif__av1_read_literal(&ctx->rd, 2);
                     int vi;
                     palette_uv_v_colors[0] = (unsigned short)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bd);
                     for (vi = 1; vi < n; ++vi) {
                        int delta = (int)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bits);
                        if (delta && stbi_avif__av1_read_literal(&ctx->rd, 1)) delta = -delta;
                        {
                           int val = (int)palette_uv_v_colors[vi - 1] + delta;
                           if (val < 0) val += max_val;
                           if (val >= max_val) val -= max_val;
                           palette_uv_v_colors[vi] = (unsigned short)val;
                        }
                     }
                  } else {
                     int vi;
                     for (vi = 0; vi < n; ++vi)
                        palette_uv_v_colors[vi] = (unsigned short)stbi_avif__av1_read_literal(&ctx->rd, (unsigned int)bd);
                  }
               }

               /* Read UV color map — same wavefront as Y but on chroma dimensions */
               {
                  int n = palette_uv_size;
                  int rows = (int)cph / 4;
                  int cols = (int)cpw / 4;
                  int plane_w = cols;
                  int i, j;
                  static const int weights[3] = { 2, 1, 2 };
                  static const int hash_muls[3] = { 1, 2, 2 };
                  if (rows < 1) rows = 1;
                  if (cols < 1) cols = 1;

                  palette_uv_map[0] = (unsigned char)stbi_avif__av1_read_uniform(&ctx->rd, (unsigned int)n);

                  for (i = 1; i < rows + cols - 1; ++i) {
                     int jstart = i < cols - 1 ? i : cols - 1;
                     int jend = i - rows + 1 > 0 ? i - rows + 1 : 0;
                     for (j = jstart; j >= jend; --j) {
                        int r = i - j, c = j;
                        int nb[3], scores[8], k, max_idx, max_score;
                        unsigned char color_order[8];
                        int color_ctx, color_idx, ctx_hash;

                        nb[0] = (c - 1 >= 0) ? (int)palette_uv_map[r * plane_w + c - 1] : -1;
                        nb[1] = (c - 1 >= 0 && r - 1 >= 0) ? (int)palette_uv_map[(r-1) * plane_w + c - 1] : -1;
                        nb[2] = (r - 1 >= 0) ? (int)palette_uv_map[(r-1) * plane_w + c] : -1;

                        for (k = 0; k < 8; ++k) { scores[k] = 0; color_order[k] = (unsigned char)k; }
                        for (k = 0; k < 3; ++k)
                           if (nb[k] >= 0) scores[nb[k]] += weights[k];

                        for (k = 0; k < 3; ++k) {
                           int m;
                           max_score = scores[k]; max_idx = k;
                           for (m = k + 1; m < n; ++m)
                              if (scores[m] > max_score) { max_score = scores[m]; max_idx = m; }
                           if (max_idx != k) {
                              int tmp_s = scores[max_idx]; unsigned char tmp_c = color_order[max_idx];
                              for (m = max_idx; m > k; --m) { scores[m] = scores[m-1]; color_order[m] = color_order[m-1]; }
                              scores[k] = tmp_s; color_order[k] = tmp_c;
                           }
                        }

                        ctx_hash = 0;
                        for (k = 0; k < 3; ++k) ctx_hash += scores[k] * hash_muls[k];
                        color_ctx = stbi_avif__av1_palette_color_index_ctx_lookup[ctx_hash];

                        color_idx = (int)stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                           ctx->palette_uv_color_index_cdf[n - 2][color_ctx], n);
                        palette_uv_map[r * plane_w + c] = color_order[color_idx];
                     }
                  }
                  /* Extend last col/row */
                  if (cols < plane_w) {
                     for (i = 0; i < rows; ++i)
                        for (j = cols; j < plane_w; ++j)
                           palette_uv_map[i * plane_w + j] = palette_uv_map[i * plane_w + cols - 1];
                  }
                  for (i = rows; i < (int)cph / 4; ++i)
                     for (j = 0; j < plane_w; ++j)
                        palette_uv_map[i * plane_w + j] = palette_uv_map[(rows - 1) * plane_w + j];
               }
            }
         }
      }
   }

   if (py < 64u) fprintf(stderr, "  post-pal: y_pal=%d uv_pal=%d rng=%u bx=%u by=%u\n", palette_y_size, palette_uv_size, ctx->rd.rng, px, py);

   /* Filter intra mode info */
   {
   unsigned int fi_flag = 0, fi_mode = 0;
   if (ctx->seq->enable_filter_intra && y_mode == 0u
       && palette_y_size == 0
       && pw <= 32u && ph <= 32u) {
      fi_flag = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
         ctx->filter_intra_cdfs[block_size < 22 ? block_size : 0], 2);
      if (fi_flag) {
         fi_mode = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
            ctx->filter_intra_mode_cdf, 5);
      }
   }
   if (py < 64u) fprintf(stderr, "  post-filterintra: rng=%u bx=%u by=%u pw=%u ph=%u fi=%u fm=%u\n", ctx->rd.rng, px, py, pw, ph, fi_flag, fi_mode);

   /* TX size: read depth symbol from tx_size_cdf, matching dav1d decode.c.
    * For intra tx_mode_select, read 1 symbol per block. depth=0,1,2 reductions.
    */
   tx_split[0] = 0u; tx_split[1] = 0u;
   {
      unsigned int bsi = block_size < 22u ? block_size : 0u;
      unsigned int mxw = stbi_avif__bsize_max_txw[bsi];
      unsigned int mxh = stbi_avif__bsize_max_txh[bsi];
      unsigned int max_dim = mxw > mxh ? mxw : mxh;  /* max(lw,lh) = t_dim->max */

      tx_log2w = mxw;
      tx_log2h = mxh;
      max_tx_log2w = mxw;
      max_tx_log2h = mxh;
      tx_size = mxw < mxh ? mxw : mxh;

      if (ctx->tx_mode_select && !skip && block_size > STBI_AVIF_BLOCK_4X4
          && max_dim > 0u) {
         /* Read depth from tx_size_cdf[max_dim-1][tctx], nsyms=min(max_dim,2) */
         /* tctx: (left_tx_intra[mi_row] >= mxh) + (above_tx_intra[mi_col] >= mxw) */
         unsigned int mi_col_ctx = mi_col;
         if (ctx->mi_cols > 0u && mi_col_ctx >= ctx->mi_cols)
            mi_col_ctx = ctx->mi_cols - 1u;
         int s_a = (int)ctx->above_tx_intra[mi_col_ctx];
         int s_l = (int)ctx->left_tx_intra[mi_row];
         int tctx = (s_a >= (int)mxw ? 1 : 0) + (s_l >= (int)mxh ? 1 : 0);
         int nsyms = (max_dim < 2u) ? 1 : 2;
         int depth;
         unsigned int cur_lw, cur_lh;
         unsigned int mi_c, mi_r;
         if (max_dim - 1u < 4u) {
            depth = (int)stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->tx_size_cdf[max_dim - 1u][tctx], nsyms + 1);
         } else {
            depth = 0;
         }
         if (py < 32u) fprintf(stderr, "  TX_SPLIT0 y=%u x=%u cat=%u ctx=%d split=%d rng=%u\n",
            0u, 0u, max_dim - 1u, tctx, depth, ctx->rd.rng);
         /* Apply depth reductions: square TX halves both dims, rect TX halves the larger */
         cur_lw = mxw; cur_lh = mxh;
         while (depth-- > 0) {
            if (cur_lw == cur_lh) {
               /* square: halve both to get smaller square */
               if (cur_lw > 0u) { cur_lw--; cur_lh--; }
            } else if (cur_lw > cur_lh) {
               /* rect wider: halve width to make square */
               cur_lw--;
            } else {
               /* rect taller: halve height to make square */
               cur_lh--;
            }
         }
         tx_log2w = cur_lw;
         tx_log2h = cur_lh;
         tx_size = tx_log2w < tx_log2h ? tx_log2w : tx_log2h;
         /* Update above/left tx_intra context for the block */
         for (mi_c = mi_col; mi_c < mi_col + bw4 && mi_c < ctx->mi_cols; ++mi_c)
            ctx->above_tx_intra[mi_c] = (signed char)tx_log2w;
         for (mi_r = mi_row; mi_r < mi_row + bh4 && mi_r < ctx->mi_rows; ++mi_r)
            ctx->left_tx_intra[mi_r] = (signed char)tx_log2h;
         if (py < 32u) fprintf(stderr, "  TX_SIZE_FINAL: log2w=%u log2h=%u split0=%u split1=%u rng=%u\n",
            tx_log2w, tx_log2h, tx_split[0], tx_split[1], ctx->rd.rng);
      }
   }

   /* above/left tx_intra updated inside TX size tree read above.
    * For non-switchable or skip blocks, update entire block now. */
   if (!ctx->tx_mode_select || skip || block_size <= STBI_AVIF_BLOCK_4X4) {
      unsigned int mi_c, mi_r;
      for (mi_c = mi_col; mi_c < mi_col + bw4 && mi_c < ctx->mi_cols; ++mi_c)
         ctx->above_tx_intra[mi_c] = (signed char)max_tx_log2w;
      for (mi_r = mi_row; mi_r < mi_row + bh4 && mi_r < ctx->mi_rows; ++mi_r)
         ctx->left_tx_intra[mi_r] = (signed char)max_tx_log2h;
   }

   /* UV TX size: for YUV444 matches Y TX dims; for subsampled, use chroma dims.
    * UV TX = max txfm that fits the chroma block (like dav1d uvtx), capped at 32px. */
   {
      /* UV TX size is based on chroma block dimensions, not Y TX sub-tile size.
       * dav1d: b->uvtx = dav1d_max_txfm_size_for_bs[bs][layout] */
      unsigned int uv_log2w = 2, uv_log2h = 2;
      if (cpw >= 32) uv_log2w = 3; else if (cpw >= 16) uv_log2w = 2; else if (cpw >= 8) uv_log2w = 1;
      if (cph >= 32) uv_log2h = 3; else if (cph >= 16) uv_log2h = 2; else if (cph >= 8) uv_log2h = 1;
      /* Cap at 32px = log2 of 3 */
      if (uv_log2w > 3u) uv_log2w = 3u;
      if (uv_log2h > 3u) uv_log2h = 3u;
      /* uv_tx_size = max(log2w, log2h) for CDF index (txb_skip etc.) */
      uv_tx_size = uv_log2w > uv_log2h ? uv_log2w : uv_log2h;
      uv_tx_szw = 4u << uv_log2w;
      uv_tx_szh = 4u << uv_log2h;
      uv_tx_sz = uv_tx_szw < uv_tx_szh ? uv_tx_szw : uv_tx_szh; /* keep for any square-only uses */
   }

   /* ======== PREDICTION ======== */

   /* Predict Y */
   if (palette_y_size > 0) {
      /* Fill Y from palette color map */
      unsigned int mi_r, mi_c;
      int map_w = (int)pw / 4;
      if (map_w < 1) map_w = 1;
      for (mi_r = 0; mi_r < ph / 4u; ++mi_r) {
         for (mi_c = 0; mi_c < pw / 4u; ++mi_c) {
            unsigned short color = palette_y_colors[palette_y_map[mi_r * map_w + mi_c]];
            unsigned int sr, sc;
            for (sr = 0; sr < 4u && py + mi_r * 4u + sr < ctx->planes->height; ++sr)
               for (sc = 0; sc < 4u && px + mi_c * 4u + sc < ctx->planes->width; ++sc)
                  ctx->planes->y[(py + mi_r * 4u + sr) * ctx->planes->width + px + mi_c * 4u + sc] = color;
         }
      }
   } else if (fi_flag) {
      stbi_avif__av1_filter_intra_predict(ctx->planes->y, ctx->planes->width,
         ctx->planes->width, ctx->planes->height,
         px, py, pw, ph, fi_mode, ctx->planes->bit_depth);
   } else {
      stbi_avif__av1_predict_block(ctx->planes->y, ctx->planes->width,
         ctx->planes->width, ctx->planes->height,
         px, py, pw, ph, ctx->planes->bit_depth, y_mode);
   }

   /* Predict UV */
   if (cpw > 0u && cph > 0u) {
      if (palette_uv_size > 0) {
         /* Fill UV from palette color map */
         unsigned int mi_r, mi_c;
         int map_w = (int)cpw / 4;
         if (map_w < 1) map_w = 1;
         for (mi_r = 0; mi_r < cph / 4u; ++mi_r) {
            for (mi_c = 0; mi_c < cpw / 4u; ++mi_c) {
               unsigned short u_color = palette_uv_u_colors[palette_uv_map[mi_r * map_w + mi_c]];
               unsigned short v_color = palette_uv_v_colors[palette_uv_map[mi_r * map_w + mi_c]];
               unsigned int sr, sc;
               for (sr = 0; sr < 4u && cpy + mi_r * 4u + sr < ctx->planes->ch; ++sr) {
                  for (sc = 0; sc < 4u && cpx + mi_c * 4u + sc < ctx->planes->cw; ++sc) {
                     ctx->planes->u[(cpy + mi_r * 4u + sr) * ctx->planes->cw + cpx + mi_c * 4u + sc] = u_color;
                     ctx->planes->v[(cpy + mi_r * 4u + sr) * ctx->planes->cw + cpx + mi_c * 4u + sc] = v_color;
                  }
               }
            }
         }
      } else {
         stbi_avif__av1_predict_block(ctx->planes->u, ctx->planes->cw,
            ctx->planes->cw, ctx->planes->ch, cpx, cpy, cpw, cph,
            ctx->planes->bit_depth, uv_mode);
         stbi_avif__av1_predict_block(ctx->planes->v, ctx->planes->cw,
            ctx->planes->cw, ctx->planes->ch, cpx, cpy, cpw, cph,
            ctx->planes->bit_depth, uv_mode);
      }
   }

   /* ======== RESIDUAL ======== */
   if (!skip) {
      unsigned int tx_w = 4u << tx_log2w;  /* actual tx width in pixels */
      unsigned int tx_h = 4u << tx_log2h;  /* actual tx height in pixels */
      unsigned int tx_row, tx_col;
      unsigned int tx_w_mi = tx_w / 4u;
      unsigned int tx_h_mi = tx_h / 4u;
      /* tx2dszctx = min(txw_log2,3) + min(txh_log2,3) for eob_bin CDF selection */
      int tx2dszctx = (int)(tx_log2w < 3u ? tx_log2w : 3u)
                    + (int)(tx_log2h < 3u ? tx_log2h : 3u);
      /* tx_ctx = max(log2w, log2h), used for skip/coeff_base/coeff_br CDFs */
      int tx_ctx = (int)(tx_log2w > tx_log2h ? tx_log2w : tx_log2h);
      unsigned int sb_mi_val = ctx->use_128 ? 32u : 16u;
      static const signed char dc_signs[3] = { 0, -1, 1 };
      static const signed char dc_sign_contexts[65] = {
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         0,
         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
      };
      static const unsigned char skip_contexts[5][5] = {
         { 1, 2, 2, 2, 3 }, { 2, 4, 4, 4, 5 }, { 2, 4, 4, 4, 5 },
         { 2, 4, 4, 4, 5 }, { 3, 5, 5, 5, 6 }
      };

      /* Y residual (skip if palette) */
      if (palette_y_size == 0)
      for (tx_row = 0; tx_row < ph; tx_row += tx_h) {
         for (tx_col = 0; tx_col < pw; tx_col += tx_w) {
            unsigned int txb_skip;
            int ts_skip = tx_ctx < 5 ? tx_ctx : 4;
            int txb_skip_ctx, dc_sign_ctx_y;
            unsigned int mi_tx_col = (px + tx_col) / 4u;
            unsigned int mi_tx_row = (py + tx_row) / 4u;
            unsigned int ti;

            /* Compute dc_sign_ctx from neighbor entropy bytes */
            {
               int dc_sign_sum = 0;
               for (ti = 0; ti < tx_w_mi && mi_tx_col + ti < ctx->mi_cols; ti++) {
                  unsigned int s = ((unsigned char)ctx->above_entropy[0][mi_tx_col + ti]) >> 3;
                  if (s <= 2) dc_sign_sum += dc_signs[s];
               }
               for (ti = 0; ti < tx_h_mi && (mi_tx_row % sb_mi_val) + ti < sb_mi_val; ti++) {
                  unsigned int s = ((unsigned char)ctx->left_entropy[0][(mi_tx_row % sb_mi_val) + ti]) >> 3;
                  if (s <= 2) dc_sign_sum += dc_signs[s];
               }
               dc_sign_ctx_y = dc_sign_contexts[dc_sign_sum + 32];
            }

            /* Compute txb_skip_ctx */
            {
               unsigned int bw_px = pw, bh_px = ph;
               if (bw_px == tx_w && bh_px == tx_h) {
                  txb_skip_ctx = 0;
               } else {
                  int top = 0, lft = 0;
                  for (ti = 0; ti < tx_w_mi && mi_tx_col + ti < ctx->mi_cols; ti++)
                     top |= ctx->above_entropy[0][mi_tx_col + ti];
                  top &= 0x07; /* COEFF_CONTEXT_MASK = 7 */
                  if (top > 4) top = 4;
                  for (ti = 0; ti < tx_h_mi && (mi_tx_row % sb_mi_val) + ti < sb_mi_val; ti++)
                     lft |= ctx->left_entropy[0][(mi_tx_row % sb_mi_val) + ti];
                  lft &= 0x07;
                  if (lft > 4) lft = 4;
                  txb_skip_ctx = skip_contexts[top][lft];
               }
            }

            if (py < 32u) fprintf(stderr, "  PRE_TXB bx=%u by=%u row=%u col=%u txw=%u txh=%u ctx=%d rng=%u dif=%llu cdf0=%u\n",
               px, py, tx_row, tx_col, tx_w, tx_h, txb_skip_ctx, ctx->rd.rng, (unsigned long long)ctx->rd.dif, ctx->txb_skip_cdf[ts_skip][txb_skip_ctx][0]);
            txb_skip = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
               ctx->txb_skip_cdf[ts_skip][txb_skip_ctx], 2);
            if (py < 32u) fprintf(stderr, "  TXB bx=%u by=%u row=%u col=%u txw=%u txh=%u skip=%u ctx=%d rng=%u dif=%llu\n",
               px, py, tx_row, tx_col, tx_w, tx_h, txb_skip, txb_skip_ctx, ctx->rd.rng, (unsigned long long)ctx->rd.dif);
            if (!txb_skip) {
               unsigned int tx_type_sym = 0;
               int tx_type_actual = 0;
               int eob, cul_level = 0;
               /* Read TX type for intra luma. Forced DCT_DCT if max TX >= 32px (log2 >= 5).
                * For TX_16X16 or reduced_tx_set: use txtp_intra2 (4 syms).
                * For TX_4X4/TX_8X8 (!reduced): use txtp_intra1 (6 syms). */
               {
                  unsigned int max_log2 = tx_log2w > tx_log2h ? tx_log2w : tx_log2h;
                  unsigned int min_log2 = tx_log2w < tx_log2h ? tx_log2w : tx_log2h;
                  /* tx_log2w/h: 0=4px,1=8px,2=16px,3=32px (4px units).
                   * Force DCT_DCT for TX >= 32px (max_log2 >= 3).
                   * Decode TX type only for max_log2 <= 2 (up to 16px). */
                  if (max_log2 <= 2u) {
                     /* min_log2: 0=TX_4X4,1=TX_8X8,2=TX_16X16 — direct CDF index */
                     if (py < 32u) fprintf(stderr, "  PRE_TX_TYPE: rng=%u dif=%llu min_log2=%u\n", ctx->rd.rng, ctx->rd.dif, min_log2);
                     if (ctx->reduced_tx_set || min_log2 >= 2u) {
                        /* use txtp_intra2 (nsyms=5) */
                        tx_type_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                           ctx->intra_tx_cdf_set2[min_log2 < 4u ? min_log2 : 3u][y_mode < 13 ? y_mode : 0], 5);
                        tx_type_actual = stbi_avif__av1_ext_tx_inv_set2[tx_type_sym < 5 ? tx_type_sym : 0];
                     } else {
                        /* use txtp_intra1 (nsyms=7) */
                        tx_type_sym = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                           ctx->intra_tx_cdf_set1[min_log2 < 4u ? min_log2 : 3u][y_mode < 13 ? y_mode : 0], 7);
                        tx_type_actual = stbi_avif__av1_ext_tx_inv_set1[tx_type_sym < 7 ? tx_type_sym : 0];
                     }
                  }
               }
               if (py < 32u) fprintf(stderr, "  TX_TYPE: sym=%u actual=%d log2w=%u log2h=%u reduced=%d rng_after=%u dif_after=%llu\n", tx_type_sym, tx_type_actual, tx_log2w, tx_log2h, ctx->reduced_tx_set, ctx->rd.rng, ctx->rd.dif);
               if (py < 32u) fprintf(stderr, "  TX_TYPE_FINAL: sym=%u actual=%d reduced=%d\n", tx_type_sym, tx_type_actual, ctx->reduced_tx_set);
               eob = stbi_avif__av1_read_coeffs_after_skip(ctx, 0, tx2dszctx, tx_ctx, tx_type_actual,
                  (int)tx_w, (int)tx_h, coeffs, ctx->dc_qstep_y, ctx->ac_qstep_y,
                  dc_sign_ctx_y, &cul_level);
               if (py < 32u) fprintf(stderr, "  AFTER_COEFF: eob=%d rng=%u dif=%llu cul=%d\n", eob, ctx->rd.rng, ctx->rd.dif, cul_level);
               /* Update entropy context with cul_level */
               for (ti = 0; ti < tx_w_mi && mi_tx_col + ti < ctx->mi_cols; ti++)
                  ctx->above_entropy[0][mi_tx_col + ti] = (unsigned char)cul_level;
               for (ti = 0; ti < tx_h_mi && (mi_tx_row % sb_mi_val) + ti < sb_mi_val; ti++)
                  ctx->left_entropy[0][(mi_tx_row % sb_mi_val) + ti] = (unsigned char)cul_level;
               if (eob > 0) {
                  if (py < 32u) fprintf(stderr, "  RECON: dc_coeff=%d pred_at=(%u,%u) pred_val=%u\n",
                     coeffs[0], px+tx_col, py+tx_row,
                     (unsigned)ctx->planes->y[(py+tx_row)*ctx->planes->width+(px+tx_col)]);
                  stbi_avif__av1_reconstruct_tx_block(ctx->planes->y, ctx->planes->width,
                     ctx->planes->width, ctx->planes->height,
                     px + tx_col, py + tx_row, tx_w, tx_h, coeffs,
                     tx_type_actual, ctx->planes->bit_depth);
                  if (py < 32u) fprintf(stderr, "  AFTER_RECON: val_at=(%u,%u)=%u\n",
                     px+tx_col, py+tx_row,
                     (unsigned)ctx->planes->y[(py+tx_row)*ctx->planes->width+(px+tx_col)]);
               }
            } else {
               /* txb_skip: zero entropy ctx */
               for (ti = 0; ti < tx_w_mi && mi_tx_col + ti < ctx->mi_cols; ti++)
                  ctx->above_entropy[0][mi_tx_col + ti] = 0;
               for (ti = 0; ti < tx_h_mi && (mi_tx_row % sb_mi_val) + ti < sb_mi_val; ti++)
                  ctx->left_entropy[0][(mi_tx_row % sb_mi_val) + ti] = 0;
            }
         }
      }

      /* U and V residual (skip if palette) */
      if (py < 32u) fprintf(stderr, "  PRE-UV: px=%u py=%u rng=%u\n", px, py, ctx->rd.rng);
      if (cpw > 0u && cph > 0u && palette_uv_size == 0) {
         unsigned int uv_tx_row, uv_tx_col;
         unsigned int uv_w_mi = uv_tx_szw / 4u;
         unsigned int uv_h_mi = uv_tx_szh / 4u;
         /* num_pels_log2: log2(bw*bh) for chroma plane_bsize vs tx_bsize */
         unsigned int cpw_mi = cpw / 4u, cph_mi = cph / 4u;
         int p;
         for (p = 1; p <= 2; ++p) {
            unsigned short *plane_buf = (p == 1) ? ctx->planes->u : ctx->planes->v;
            for (uv_tx_row = 0; uv_tx_row < cph; uv_tx_row += uv_tx_szh) {
               for (uv_tx_col = 0; uv_tx_col < cpw; uv_tx_col += uv_tx_szw) {
                  unsigned int mi_tx_col_uv = (cpx + uv_tx_col) / 4u;
                  unsigned int mi_tx_row_uv = (cpy + uv_tx_row) / 4u;
                  unsigned int ti;
                  int dc_sign_ctx_uv, txb_skip_ctx_uv, cul_level_uv = 0;
                  int eob;

                  /* Compute dc_sign_ctx for UV */
                  {
                     int dc_sign_sum = 0;
                     for (ti = 0; ti < uv_w_mi && mi_tx_col_uv + ti < (ctx->mi_cols >> (unsigned)ctx->planes->subx); ti++) {
                        unsigned int s = ((unsigned char)ctx->above_entropy[p][mi_tx_col_uv + ti]) >> 3;
                        if (s <= 2) dc_sign_sum += dc_signs[s];
                     }
                     { unsigned int sb_uv_h = sb_mi_val >> (unsigned)ctx->planes->suby;
                     for (ti = 0; ti < uv_h_mi && (mi_tx_row_uv % sb_uv_h) + ti < sb_uv_h; ti++) {
                        unsigned int s = ((unsigned char)ctx->left_entropy[p][(mi_tx_row_uv % sb_uv_h) + ti]) >> 3;
                        if (s <= 2) dc_sign_sum += dc_signs[s];
                     }}
                     dc_sign_ctx_uv = dc_sign_contexts[dc_sign_sum + 32];
                  }

                  /* Compute txb_skip_ctx for UV: get_entropy_context(tx_size,a,l) + offset */
                  {
                     int above_ec = 0, left_ec = 0;
                     /* For square TX: OR together txb_w_unit entries and check nonzero */
                     for (ti = 0; ti < uv_w_mi && mi_tx_col_uv + ti < (ctx->mi_cols >> (unsigned)ctx->planes->subx); ti++)
                        above_ec |= ctx->above_entropy[p][mi_tx_col_uv + ti];
                     above_ec = above_ec != 0 ? 1 : 0;
                     { unsigned int sb_uv_h = sb_mi_val >> (unsigned)ctx->planes->suby;
                     for (ti = 0; ti < uv_h_mi && (mi_tx_row_uv % sb_uv_h) + ti < sb_uv_h; ti++)
                        left_ec |= ctx->left_entropy[p][(mi_tx_row_uv % sb_uv_h) + ti];
                     left_ec = left_ec != 0 ? 1 : 0; }
                     {
                        /* ctx_offset: 10 if plane_bsize > tx_bsize, else 7 */
                        int ctx_offset = (cpw_mi > uv_w_mi || cph_mi > uv_h_mi) ? 10 : 7;
                        txb_skip_ctx_uv = (above_ec + left_ec) + ctx_offset;
                     }
                  }

                  if (cpy < 32u) fprintf(stderr, "  UV_PRE_COEFF p=%d bx=%u by=%u rng=%u dif=%llu skip_ctx=%d ts=%d\n",
                     p, cpx+uv_tx_col, cpy+uv_tx_row, ctx->rd.rng, ctx->rd.dif, txb_skip_ctx_uv, (int)uv_tx_size);
                  {
                  int uv_log2w_tx = (uv_tx_szw==32?3:uv_tx_szw==16?2:uv_tx_szw==8?1:0);
                  int uv_log2h_tx = (uv_tx_szh==32?3:uv_tx_szh==16?2:uv_tx_szh==8?1:0);
                  int uv_tx2dszctx = (uv_log2w_tx<3?uv_log2w_tx:3) + (uv_log2h_tx<3?uv_log2h_tx:3);
                  int uv_tx_ctx = uv_log2w_tx > uv_log2h_tx ? uv_log2w_tx : uv_log2h_tx;
                  int uv_ts = uv_tx_ctx; /* max(log2w,log2h) for txb_skip CDF index */
                  eob = stbi_avif__av1_read_coeffs(ctx, p, uv_ts, 0,
                     uv_tx2dszctx,
                     uv_tx_ctx,
                     (int)uv_tx_szw, (int)uv_tx_szh,
                     coeffs, ctx->dc_qstep_uv, ctx->ac_qstep_uv,
                     txb_skip_ctx_uv, dc_sign_ctx_uv, &cul_level_uv);
                  if (cpy < 32u) fprintf(stderr, "  UV_COEFF p=%d bx=%u by=%u eob=%d rng=%u dif=%llu\n",
                     p, cpx+uv_tx_col, cpy+uv_tx_row, eob, ctx->rd.rng, ctx->rd.dif);

                  /* Update entropy context with cul_level */
                  for (ti = 0; ti < uv_w_mi && mi_tx_col_uv + ti < (ctx->mi_cols >> (unsigned)ctx->planes->subx); ti++)
                     ctx->above_entropy[p][mi_tx_col_uv + ti] = (unsigned char)cul_level_uv;
                  { unsigned int sb_uv_h = sb_mi_val >> (unsigned)ctx->planes->suby;
                  for (ti = 0; ti < uv_h_mi && (mi_tx_row_uv % sb_uv_h) + ti < sb_uv_h; ti++)
                     ctx->left_entropy[p][(mi_tx_row_uv % sb_uv_h) + ti] = (unsigned char)cul_level_uv; }

                  if (eob > 0)
                     stbi_avif__av1_reconstruct_tx_block(plane_buf, ctx->planes->cw,
                        ctx->planes->cw, ctx->planes->ch,
                        cpx + uv_tx_col, cpy + uv_tx_row, uv_tx_szw, uv_tx_szh,
                        coeffs, 0, ctx->planes->bit_depth);
                  }
               }
            }
         }
      }
   }

   } /* end fi_flag/fi_mode scope */

   return 1;
}

/* Partition context lookup: above and left values per AV1 BLOCK_SIZE.
   Index by block_size enum (0..21). */
static const unsigned char stbi_avif__partition_ctx_above[22] = {
   31,31,30,30,30,28,28,28,24,24,24,16,16,16,0,0, 31,28,30,24,28,16
};
static const unsigned char stbi_avif__partition_ctx_left[22] = {
   31,30,31,30,28,30,28,24,28,24,16,24,16,0,16,0, 28,31,24,30,16,28
};

/* Number of symbols in the partition CDF for each bsize_ctx.
   Our format stores (real_nsym+1) values including terminal 32768, so we pass
   (real_nsym+1) as nsym. dav1d uses real_nsym (without terminal in count).
   dav1d: bsize 128x128=7syms, 64x64/32x32/16x16=9syms, 8x8=3syms.
   Our nsyms (including terminal): { 8, 10, 10, 10, 4 }. */
static const int stbi_avif__partition_nsym[5] = { 8, 10, 10, 10, 4 };

static void stbi_avif__update_partition_ctx(stbi_avif__av1_decode_ctx *ctx,
                                             unsigned int mi_row, unsigned int mi_col,
                                             int subsize, int bsize)
{
   unsigned int bw = 1u << stbi_avif__bsize_log2w[bsize];
   unsigned int bh = 1u << stbi_avif__bsize_log2h[bsize];
   unsigned char above_val = stbi_avif__partition_ctx_above[subsize];
   unsigned char left_val  = stbi_avif__partition_ctx_left[subsize];
   unsigned int sb_mi = ctx->use_128 ? 32u : 16u;
   unsigned int i;
   for (i = 0; i < bw && mi_col + i < ctx->mi_cols; i++)
      ctx->above_partition_ctx[mi_col + i] = above_val;
   for (i = 0; i < bh && (mi_row % sb_mi) + i < sb_mi; i++)
      ctx->left_partition_ctx[(mi_row % sb_mi) + i] = left_val;
}

static int stbi_avif__av1_decode_partition(stbi_avif__av1_decode_ctx *ctx,
                                            unsigned int mi_row, unsigned int mi_col,
                                            int block_size)
{
   unsigned int bw4 = 1u << stbi_avif__bsize_log2w[block_size];
   unsigned int bh4 = 1u << stbi_avif__bsize_log2h[block_size];
   unsigned int partition;
   int sub_size;
   int bsize_ctx; /* 0=128×128 … 4=8×8 */
   int bsl, above, left, part_ctx;
   unsigned int sb_mi;

   /* Out-of-frame: skip silently. */
   if (mi_row >= ctx->mi_rows || mi_col >= ctx->mi_cols)
      return 1;

   /* Map block_size enum to bsize_ctx (0=128, 1=64, 2=32, 3=16, 4=8). */
   if      (block_size >= STBI_AVIF_BLOCK_128X128) bsize_ctx = 0;
   else if (block_size >= STBI_AVIF_BLOCK_64X64)   bsize_ctx = 1;
   else if (block_size >= STBI_AVIF_BLOCK_32X32)   bsize_ctx = 2;
   else if (block_size >= STBI_AVIF_BLOCK_16X16)   bsize_ctx = 3;
   else                                             bsize_ctx = 4;

   /* Compute partition sub-context from neighbors. */
   bsl = stbi_avif__bsize_log2w[block_size] - 1; /* mi_size_wide_log2 - log2(8x8 mi) */
   if (bsl < 0) bsl = 0;
   sb_mi = ctx->use_128 ? 32u : 16u;
   above = (ctx->above_partition_ctx[mi_col] >> bsl) & 1;
   left  = (ctx->left_partition_ctx[mi_row % sb_mi] >> bsl) & 1;
   part_ctx = left * 2 + above;

   /* Leaf: blocks smaller than 8×8 — always PARTITION_NONE, no read. */
   if (block_size < STBI_AVIF_BLOCK_8X8)
   {
      if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col, block_size)) return 0;
      stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, block_size, block_size);
      return 1;
   }

   /* AV1 spec: if block extends past frame edge, force split (no symbol read).
    * For 8x8 blocks that extend: PARTITION_NONE is forced (handled above). */
   {
      int extends_right = (mi_col + bw4) > ctx->mi_cols;
      int extends_down  = (mi_row + bh4) > ctx->mi_rows;
      if (extends_right || extends_down) {
         /* Force split */
         sub_size = block_size - 3;
         if (sub_size < 0) sub_size = 0;
         if (block_size == STBI_AVIF_BLOCK_8X8) {
            /* 8x8 that extends: just decode the visible sub-blocks */
            if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col, sub_size)) return 0;
            if (mi_col + bw4/2u < ctx->mi_cols)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col + bw4/2u, sub_size)) return 0;
            if (mi_row + bh4/2u < ctx->mi_rows)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col, sub_size)) return 0;
            if (mi_col + bw4/2u < ctx->mi_cols && mi_row + bh4/2u < ctx->mi_rows)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
            stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, sub_size, block_size);
         } else {
            if (!stbi_avif__av1_decode_partition(ctx, mi_row,          mi_col,          sub_size)) return 0;
            if (!stbi_avif__av1_decode_partition(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
            if (!stbi_avif__av1_decode_partition(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
            if (!stbi_avif__av1_decode_partition(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
         }
         return 1;
      }
   }

   if (mi_row < 16u && mi_col < 32u) fprintf(stderr, "PRE-PART(%u,%u) bs=%d bctx=%d pctx=%d nsym=%d rng=%u\n",
      mi_row*4, mi_col*4, block_size, bsize_ctx, part_ctx, stbi_avif__partition_nsym[bsize_ctx], ctx->rd.rng);
   partition = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                   ctx->partition_cdf[bsize_ctx][part_ctx],
                   stbi_avif__partition_nsym[bsize_ctx]);

   if (mi_row < 16u && mi_col < 32u) fprintf(stderr, "PART(%u,%u) bs=%d bctx=%d pctx=%d nsym=%d part=%u rd_dif=%llu rd_rng=%u\n",
      mi_row*4, mi_col*4, block_size, bsize_ctx, part_ctx, stbi_avif__partition_nsym[bsize_ctx], partition, (unsigned long long)ctx->rd.dif, ctx->rd.rng);

   /* Compute the sub-block size for SPLIT. */
   sub_size = block_size - 3; /* e.g. 128→64, 64→32, 32→16, … */
   if (sub_size < 0) sub_size = 0;

   switch (partition)
   {
      case STBI_AVIF_PARTITION_NONE:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col, block_size)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, block_size, block_size);
         return 1;

      case STBI_AVIF_PARTITION_HORZ:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col, block_size - 1)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col, block_size - 1)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, block_size - 1, block_size);
         return 1;

      case STBI_AVIF_PARTITION_VERT:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col,          block_size - 2)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col + bw4/2u, block_size - 2)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, block_size - 2, block_size);
         return 1;

      case STBI_AVIF_PARTITION_SPLIT:
         if (block_size == STBI_AVIF_BLOCK_8X8) {
            /* 8x8 SPLIT to 4x4: sub-partitions don't recurse */
            if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          sub_size)) return 0;
            if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
            if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
            if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
            stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, sub_size, block_size);
         } else {
            if (!stbi_avif__av1_decode_partition(ctx, mi_row,          mi_col,          sub_size)) return 0;
            if (!stbi_avif__av1_decode_partition(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
            if (!stbi_avif__av1_decode_partition(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
            if (!stbi_avif__av1_decode_partition(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
            /* sub-partitions already updated their own contexts */
         }
         return 1;

      /* Compound partition types */
      case STBI_AVIF_PARTITION_HORZ_A:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          block_size - 1)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, sub_size, block_size);
         stbi_avif__update_partition_ctx(ctx, mi_row + bh4/2u, mi_col, block_size - 1, block_size - 1);
         return 1;

      case STBI_AVIF_PARTITION_HORZ_B:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          block_size - 1)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, block_size - 1, block_size - 1);
         stbi_avif__update_partition_ctx(ctx, mi_row + bh4/2u, mi_col, sub_size, block_size - 1);
         return 1;

      case STBI_AVIF_PARTITION_VERT_A:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, block_size - 2)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, sub_size, block_size - 2);
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col + bw4/2u, block_size - 2, block_size - 2);
         return 1;

      case STBI_AVIF_PARTITION_VERT_B:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          block_size - 2)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, block_size - 2, block_size - 2);
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col + bw4/2u, sub_size, block_size - 2);
         return 1;

      case STBI_AVIF_PARTITION_HORZ_4:
         {
            unsigned int q4 = bh4 / 4u;
            unsigned int qi;
            for (qi = 0; qi < 4u; ++qi)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + qi * q4, mi_col, sub_size)) return 0;
         }
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, sub_size, block_size);
         return 1;

      case STBI_AVIF_PARTITION_VERT_4:
         {
            unsigned int q4 = bw4 / 4u;
            unsigned int qi;
            for (qi = 0; qi < 4u; ++qi)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col + qi * q4, sub_size)) return 0;
         }
         stbi_avif__update_partition_ctx(ctx, mi_row, mi_col, sub_size, block_size);
         return 1;

      default:
         return stbi_avif__fail("unknown AV1 partition type");
   }
}

static int stbi_avif__av1_decode_tile(stbi_avif__av1_decode_ctx *ctx,
                                      unsigned int sb_row_start,
                                      unsigned int sb_row_end,
                                      unsigned int sb_col_start,
                                      unsigned int sb_col_end)
{
   unsigned int sb_size4;
   unsigned int sb_row, sb_col;
   int root_bsize;

   sb_size4   = ctx->use_128 ? 32u : 16u;
   root_bsize = ctx->use_128 ? STBI_AVIF_BLOCK_128X128 : STBI_AVIF_BLOCK_64X64;

   for (sb_row = sb_row_start; sb_row < sb_row_end; ++sb_row) {
      /* Reset left context at start of each SB row */
      memset(ctx->left_partition_ctx, 0, sizeof(ctx->left_partition_ctx));
      memset(ctx->left_tx_intra, -1, ctx->mi_rows * sizeof(*ctx->left_tx_intra));
      memset(ctx->left_entropy, 0, sizeof(ctx->left_entropy));
      memset(ctx->left_modes, 0, ctx->mi_rows);
      for (sb_col = sb_col_start; sb_col < sb_col_end; ++sb_col)
      {
         if (!stbi_avif__av1_decode_partition(ctx,
                sb_row * sb_size4,
                sb_col * sb_size4,
                root_bsize))
            return 0;
      }
   }
   return 1;
}

/*
 * =============================================================================
 *  YUV → RGBA CONVERSION
 * =============================================================================
 *
 * BT.709 limited-range coefficients (the most common for SDR still images):
 *   Y' ∈ [16, 235]  → full [0,255];  Cb,Cr ∈ [16,240] → [-0.5,+0.5]
 *
 *   R = 1.164(Y-16) + 1.793(Cr-128)
 *   G = 1.164(Y-16) - 0.213(Cb-128) - 0.534(Cr-128)
 *   B = 1.164(Y-16) + 2.115(Cb-128)
 *
 * All values are scaled by 2^14 for integer arithmetic.
 * For full-range content (color_range==1), the Y/Cb/Cr full-swing formulae:
 *   R = Y + 1.402(Cr-128)
 *   G = Y - 0.344(Cb-128) - 0.714(Cr-128)
 *   B = Y + 1.772(Cb-128)
 *
 * For 10-bit input we shift the "128" bias to 512 and scale output by >>2
 * before clamping to [0,255].
 */

static unsigned char stbi_avif__clamp_u8(int v)
{
   if (v < 0)   return 0;
   if (v > 255) return 255;
   return (unsigned char)v;
}

static unsigned char *stbi_avif__av1_planes_to_rgba(const stbi_avif__av1_planes *p,
                                                      int matrix_coefficients,
                                                      int color_range)
{
   unsigned int w  = p->width;
   unsigned int h  = p->height;
   unsigned char *out;
   unsigned int iy, ix;
   out = (unsigned char *)STBI_AVIF_MALLOC((size_t)w * (size_t)h * 4u);
   if (!out)
   {
      stbi_avif__fail("out of memory (RGBA output)");
      return NULL;
   }

   for (iy = 0; iy < h; ++iy)
   {
      const unsigned short *yrow = p->y + iy * w;
      const unsigned short *urow = p->u + (iy >> p->suby) * p->cw;
      const unsigned short *vrow = p->v + (iy >> p->suby) * p->cw;
      unsigned char *drow = out + iy * w * 4u;

      for (ix = 0; ix < w; ++ix)
      {
         int Y, U, V;
         int R, G, B;
         unsigned int cx = ix >> p->subx;

         if (p->bit_depth > 8u)
         {
            /* 10-bit: shift to 8-bit by dropping 2 LSBs */
            Y = (int)(yrow[ix] >> 2);
            U = (int)(urow[cx] >> 2);
            V = (int)(vrow[cx] >> 2);
         }
         else
         {
            Y = (int)yrow[ix];
            U = (int)urow[cx];
            V = (int)vrow[cx];
         }

         if (matrix_coefficients == STBI_AVIF_AV1_MC_IDENTITY)
         {
            /* Identity: YUV directly maps to GBR (AV1 identity places G in Y, B in U, R in V) */
            R = V;
            G = Y;
            B = U;
         }
         else if (color_range)
         {
            /* Full swing BT.709/BT.601 */
            int yf = Y;
            int uf = U - 128;
            int vf = V - 128;
            /* ×2^14 fixed-point */
            R = (yf * 16384          + vf * 22970) >> 14;
            G = (yf * 16384 - uf *  5638 - vf * 11700) >> 14;
            B = (yf * 16384 + uf * 29032         ) >> 14;
         }
         else
         {
            /* Limited range BT.709 */
            int yf = Y  - 16;
            int uf = U - 128;
            int vf = V - 128;
            R = (yf * 19077          + vf * 26149) >> 14;
            G = (yf * 19077 - uf *  6419 - vf * 13320) >> 14;
            B = (yf * 19077 + uf * 33050         ) >> 14;
         }

         drow[ix * 4u + 0u] = stbi_avif__clamp_u8(R);
         drow[ix * 4u + 1u] = stbi_avif__clamp_u8(G);
         drow[ix * 4u + 2u] = stbi_avif__clamp_u8(B);
         drow[ix * 4u + 3u] = 255u;
      }
   }
   return out;
}

/*
 * =============================================================================
 *  TOP-LEVEL DECODE FUNCTION
 * =============================================================================
 */

static unsigned char *stbi_avif__av1_decode(
   const unsigned char *tile_group_data, size_t tile_group_size,
   const stbi_avif__av1_sequence_header *seq,
   const stbi_avif__av1_frame_header    *fhdr,
   const stbi_avif__av1_tile_group_header *tghdr)
{
   stbi_avif__av1_planes planes;
   stbi_avif__av1_decode_ctx ctx;
   unsigned char *rgba;
   size_t tile_cursor;
   unsigned int tile_idx, tile_count_in_group;
   unsigned int sb_row_start, sb_row_end, sb_col_start, sb_col_end;
   unsigned int tile_row, tile_col;
   int q_ctx;
   short dc_q, ac_q;

   memset(&ctx, 0, sizeof(ctx));
   q_ctx = stbi_avif__av1_get_q_ctx(fhdr->base_q_idx);
   ctx.base_q_idx = fhdr->base_q_idx;
   ctx.q_ctx = q_ctx;

   fprintf(stderr, "base_q_idx=%u q_ctx=%d bit_depth=%u reduced_tx_set=%d tx_mode_select=%d\n", fhdr->base_q_idx, q_ctx, seq->bit_depth, fhdr->reduced_tx_set, fhdr->tx_mode_select);

   if (seq->bit_depth <= 8u) {
      dc_q = stbi_avif__av1_dc_qlookup[fhdr->base_q_idx < 256u ? fhdr->base_q_idx : 255u];
      ac_q = stbi_avif__av1_ac_qlookup[fhdr->base_q_idx < 256u ? fhdr->base_q_idx : 255u];
   } else {
      dc_q = stbi_avif__av1_dc_qlookup_10[fhdr->base_q_idx < 256u ? fhdr->base_q_idx : 255u];
      ac_q = stbi_avif__av1_ac_qlookup_10[fhdr->base_q_idx < 256u ? fhdr->base_q_idx : 255u];
   }
   ctx.dc_qstep_y  = (int)dc_q;
   ctx.ac_qstep_y  = (int)ac_q;
   ctx.dc_qstep_uv = (int)dc_q;
   ctx.ac_qstep_uv = (int)ac_q;

   memcpy(ctx.partition_cdf, stbi_avif__av1_partition_cdf, sizeof(stbi_avif__av1_partition_cdf));
   memcpy(ctx.partition4_cdf, stbi_avif__av1_partition4_cdf, sizeof(stbi_avif__av1_partition4_cdf));
   memcpy(ctx.kf_y_mode_cdf, stbi_avif__av1_kf_y_mode_cdf, sizeof(stbi_avif__av1_kf_y_mode_cdf));
   memcpy(ctx.uv_mode_cdf_no_cfl, stbi_avif__av1_uv_mode_cdf_no_cfl, sizeof(stbi_avif__av1_uv_mode_cdf_no_cfl));
   memcpy(ctx.uv_mode_cdf_cfl, stbi_avif__av1_uv_mode_cdf_cfl, sizeof(stbi_avif__av1_uv_mode_cdf_cfl));
   memcpy(ctx.angle_delta_cdf, stbi_avif__av1_angle_delta_cdf, sizeof(stbi_avif__av1_angle_delta_cdf));
   memcpy(ctx.intra_tx_cdf_set1, stbi_avif__av1_intra_tx_cdf_set1, sizeof(stbi_avif__av1_intra_tx_cdf_set1));
   memcpy(ctx.intra_tx_cdf_set2, stbi_avif__av1_intra_tx_cdf_set2, sizeof(stbi_avif__av1_intra_tx_cdf_set2));
   memcpy(ctx.skip_cdf, stbi_avif__av1_skip_cdf, sizeof(stbi_avif__av1_skip_cdf));
   memcpy(ctx.txfm_partition_cdf, stbi_avif__av1_txfm_partition_cdf, sizeof(stbi_avif__av1_txfm_partition_cdf));
   memcpy(ctx.cfl_sign_cdf, stbi_avif__av1_cfl_sign_cdf, sizeof(stbi_avif__av1_cfl_sign_cdf));
   memcpy(ctx.cfl_alpha_cdf, stbi_avif__av1_cfl_alpha_cdf, sizeof(stbi_avif__av1_cfl_alpha_cdf));
   memcpy(ctx.tx_size_cdf, stbi_avif__av1_tx_size_cdf, sizeof(stbi_avif__av1_tx_size_cdf));
   memcpy(ctx.palette_y_mode_cdf, stbi_avif__av1_palette_y_mode_cdf, sizeof(stbi_avif__av1_palette_y_mode_cdf));
   memcpy(ctx.palette_uv_mode_cdf, stbi_avif__av1_palette_uv_mode_cdf, sizeof(stbi_avif__av1_palette_uv_mode_cdf));
   memcpy(ctx.palette_y_size_cdf, stbi_avif__av1_palette_y_size_cdf, sizeof(stbi_avif__av1_palette_y_size_cdf));
   memcpy(ctx.palette_uv_size_cdf, stbi_avif__av1_palette_uv_size_cdf, sizeof(stbi_avif__av1_palette_uv_size_cdf));
   memcpy(ctx.palette_y_color_index_cdf, stbi_avif__av1_palette_y_color_index_cdf, sizeof(stbi_avif__av1_palette_y_color_index_cdf));
   memcpy(ctx.palette_uv_color_index_cdf, stbi_avif__av1_palette_uv_color_index_cdf, sizeof(stbi_avif__av1_palette_uv_color_index_cdf));
   memcpy(ctx.filter_intra_cdfs, stbi_avif__av1_filter_intra_cdfs, sizeof(stbi_avif__av1_filter_intra_cdfs));
   memcpy(ctx.filter_intra_mode_cdf, stbi_avif__av1_filter_intra_mode_cdf, sizeof(stbi_avif__av1_filter_intra_mode_cdf));

   memcpy(ctx.txb_skip_cdf, stbi_avif__av1_txb_skip_cdf[q_ctx], sizeof(ctx.txb_skip_cdf));
   memcpy(ctx.dc_sign_cdf, stbi_avif__av1_dc_sign_cdf[q_ctx], sizeof(ctx.dc_sign_cdf));
   memcpy(ctx.eob_extra_cdf, stbi_avif__av1_eob_extra_cdf[q_ctx], sizeof(ctx.eob_extra_cdf));
   memcpy(ctx.eob_multi16_cdf, stbi_avif__av1_eob_multi16_cdf[q_ctx], sizeof(ctx.eob_multi16_cdf));
   memcpy(ctx.eob_multi32_cdf, stbi_avif__av1_eob_multi32_cdf[q_ctx], sizeof(ctx.eob_multi32_cdf));
   memcpy(ctx.eob_multi64_cdf, stbi_avif__av1_eob_multi64_cdf[q_ctx], sizeof(ctx.eob_multi64_cdf));
   memcpy(ctx.eob_multi128_cdf, stbi_avif__av1_eob_multi128_cdf[q_ctx], sizeof(ctx.eob_multi128_cdf));
   memcpy(ctx.eob_multi256_cdf, stbi_avif__av1_eob_multi256_cdf[q_ctx], sizeof(ctx.eob_multi256_cdf));
   memcpy(ctx.eob_multi512_cdf, stbi_avif__av1_eob_multi512_cdf[q_ctx], sizeof(ctx.eob_multi512_cdf));
   memcpy(ctx.eob_multi1024_cdf, stbi_avif__av1_eob_multi1024_cdf[q_ctx], sizeof(ctx.eob_multi1024_cdf));
   memcpy(ctx.coeff_base_eob_cdf, stbi_avif__av1_coeff_base_eob_cdf[q_ctx], sizeof(ctx.coeff_base_eob_cdf));
   memcpy(ctx.coeff_base_cdf, stbi_avif__av1_coeff_base_cdf[q_ctx], sizeof(ctx.coeff_base_cdf));
   memcpy(ctx.coeff_br_cdf, stbi_avif__av1_coeff_br_cdf[q_ctx], sizeof(ctx.coeff_br_cdf));

   if (!stbi_avif__av1_alloc_planes(&planes, seq, fhdr))
      return NULL;

   ctx.planes   = &planes;
   ctx.seq      = seq;
   ctx.mi_cols  = (fhdr->frame_width  + 3u) / 4u;
   ctx.mi_rows  = (fhdr->frame_height + 3u) / 4u;
   ctx.use_128  = seq->use_128x128_superblock;
   ctx.reduced_tx_set = fhdr->reduced_tx_set;
   ctx.tx_mode_select = fhdr->tx_mode_select;
   ctx.allow_screen_content_tools = fhdr->allow_screen_content_tools;
   ctx.cdef_bits = fhdr->cdef_bits;
   ctx.sb_size_mi = seq->use_128x128_superblock ? 32u : 16u;

   ctx.above_modes = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   ctx.left_modes  = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_rows);
   ctx.above_partition_ctx = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   ctx.above_skip  = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   ctx.left_skip   = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_rows);
   ctx.above_tx_intra = (signed char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   ctx.left_tx_intra  = (signed char *)STBI_AVIF_MALLOC(ctx.mi_rows);
   ctx.above_entropy[0] = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   ctx.above_entropy[1] = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   ctx.above_entropy[2] = (unsigned char *)STBI_AVIF_MALLOC(ctx.mi_cols);
   if (!ctx.above_modes || !ctx.left_modes || !ctx.above_partition_ctx ||
       !ctx.above_skip || !ctx.left_skip || !ctx.above_tx_intra || !ctx.left_tx_intra ||
       !ctx.above_entropy[0] || !ctx.above_entropy[1] || !ctx.above_entropy[2]) {
      STBI_AVIF_FREE(ctx.above_modes);
      STBI_AVIF_FREE(ctx.left_modes);
      STBI_AVIF_FREE(ctx.above_partition_ctx);
      STBI_AVIF_FREE(ctx.above_skip);
      STBI_AVIF_FREE(ctx.left_skip);
      STBI_AVIF_FREE(ctx.above_tx_intra);
      STBI_AVIF_FREE(ctx.left_tx_intra);
      STBI_AVIF_FREE(ctx.above_entropy[0]);
      STBI_AVIF_FREE(ctx.above_entropy[1]);
      STBI_AVIF_FREE(ctx.above_entropy[2]);
      stbi_avif__av1_free_planes(&planes);
      return (unsigned char *)stbi_avif__fail_ptr("out of memory (mode maps)");
   }
   memset(ctx.above_modes, 0, ctx.mi_cols);
   memset(ctx.left_modes, 0, ctx.mi_rows);
   memset(ctx.above_partition_ctx, 0, ctx.mi_cols);
   memset(ctx.above_skip, 0, ctx.mi_cols);
   memset(ctx.left_skip, 0, ctx.mi_rows * sizeof(*ctx.left_skip));
   memset(ctx.above_tx_intra, -1, ctx.mi_cols);
   memset(ctx.left_tx_intra, -1, ctx.mi_rows * sizeof(*ctx.left_tx_intra));
   memset(ctx.above_entropy[0], 0, ctx.mi_cols);
   memset(ctx.above_entropy[1], 0, ctx.mi_cols);
   memset(ctx.above_entropy[2], 0, ctx.mi_cols);
   memset(ctx.left_entropy, 0, sizeof(ctx.left_entropy));
   memset(ctx.left_partition_ctx, 0, sizeof(ctx.left_partition_ctx));

   tile_cursor = tghdr->tile_data_byte_offset;
   tile_count_in_group = tghdr->tile_end - tghdr->tile_start + 1u;

   for (tile_idx = 0u; tile_idx < tile_count_in_group; ++tile_idx)
   {
      unsigned int tile_size_value, tile_payload_size;
      const unsigned char *tile_payload;
      unsigned int full_tile_index;

      if (tile_idx + 1u < tile_count_in_group) {
         if (!stbi_avif__av1_read_le_bytes(tile_group_data, tile_group_size,
                                           tile_cursor, fhdr->tile_size_bytes,
                                           &tile_size_value)) {
            STBI_AVIF_FREE(ctx.above_modes); STBI_AVIF_FREE(ctx.left_modes);
            STBI_AVIF_FREE(ctx.above_partition_ctx); STBI_AVIF_FREE(ctx.above_skip); STBI_AVIF_FREE(ctx.left_skip); STBI_AVIF_FREE(ctx.above_tx_intra); STBI_AVIF_FREE(ctx.left_tx_intra); STBI_AVIF_FREE(ctx.above_entropy[0]); STBI_AVIF_FREE(ctx.above_entropy[1]); STBI_AVIF_FREE(ctx.above_entropy[2]);
            stbi_avif__av1_free_planes(&planes); return NULL;
         }
         tile_cursor += fhdr->tile_size_bytes;
         tile_payload_size = tile_size_value + 1u;
         if (tile_cursor + (size_t)tile_payload_size > tile_group_size) {
            tile_payload_size = (unsigned int)(tile_group_size - tile_cursor);
            tile_count_in_group = tile_idx + 1u;
         }
      } else {
         if (tile_cursor > tile_group_size) {
            STBI_AVIF_FREE(ctx.above_modes); STBI_AVIF_FREE(ctx.left_modes);
            STBI_AVIF_FREE(ctx.above_partition_ctx); STBI_AVIF_FREE(ctx.above_skip); STBI_AVIF_FREE(ctx.left_skip); STBI_AVIF_FREE(ctx.above_tx_intra); STBI_AVIF_FREE(ctx.left_tx_intra); STBI_AVIF_FREE(ctx.above_entropy[0]); STBI_AVIF_FREE(ctx.above_entropy[1]); STBI_AVIF_FREE(ctx.above_entropy[2]);
            stbi_avif__av1_free_planes(&planes);
            return (unsigned char *)stbi_avif__fail_ptr("invalid AV1 tile payload offset");
         }
         tile_payload_size = (unsigned int)(tile_group_size - tile_cursor);
      }

      tile_payload = tile_group_data + tile_cursor;
      tile_cursor += tile_payload_size;
      full_tile_index = tghdr->tile_start + tile_idx;
      tile_row = full_tile_index / fhdr->tile_cols;
      tile_col = full_tile_index % fhdr->tile_cols;
      sb_row_start = fhdr->tile_row_start_sb[tile_row];
      sb_row_end   = fhdr->tile_row_start_sb[tile_row + 1u];
      sb_col_start = fhdr->tile_col_start_sb[tile_col];
      sb_col_end   = fhdr->tile_col_start_sb[tile_col + 1u];

      if (tile_payload_size < 2u) continue;

      fprintf(stderr, "TILE payload[0..3]: %02x %02x %02x %02x  size=%u\n",
         tile_payload[0], tile_payload[1],
         tile_payload_size > 2 ? tile_payload[2] : 0,
         tile_payload_size > 3 ? tile_payload[3] : 0,
         tile_payload_size);

      if (!stbi_avif__av1_range_decoder_init(&ctx.rd, tile_payload,
                                             (size_t)tile_payload_size, 0u)) {
         STBI_AVIF_FREE(ctx.above_modes); STBI_AVIF_FREE(ctx.left_modes);
         STBI_AVIF_FREE(ctx.above_partition_ctx); STBI_AVIF_FREE(ctx.above_skip); STBI_AVIF_FREE(ctx.left_skip); STBI_AVIF_FREE(ctx.above_tx_intra); STBI_AVIF_FREE(ctx.left_tx_intra); STBI_AVIF_FREE(ctx.above_entropy[0]); STBI_AVIF_FREE(ctx.above_entropy[1]); STBI_AVIF_FREE(ctx.above_entropy[2]);
         stbi_avif__av1_free_planes(&planes); return NULL;
      }

      memset(ctx.above_modes, 0, ctx.mi_cols);
      memset(ctx.left_modes, 0, ctx.mi_rows);
      memset(ctx.above_tx_intra, -1, ctx.mi_cols);
      memset(ctx.left_tx_intra, -1, ctx.mi_rows * sizeof(*ctx.left_tx_intra));

      if (!stbi_avif__av1_decode_tile(&ctx, sb_row_start, sb_row_end,
                                      sb_col_start, sb_col_end)) {
         STBI_AVIF_FREE(ctx.above_modes); STBI_AVIF_FREE(ctx.left_modes);
         STBI_AVIF_FREE(ctx.above_partition_ctx); STBI_AVIF_FREE(ctx.above_skip); STBI_AVIF_FREE(ctx.left_skip); STBI_AVIF_FREE(ctx.above_tx_intra); STBI_AVIF_FREE(ctx.left_tx_intra); STBI_AVIF_FREE(ctx.above_entropy[0]); STBI_AVIF_FREE(ctx.above_entropy[1]); STBI_AVIF_FREE(ctx.above_entropy[2]);
         stbi_avif__av1_free_planes(&planes); return NULL;
      }
   }

   STBI_AVIF_FREE(ctx.above_modes);
   STBI_AVIF_FREE(ctx.left_modes);
   STBI_AVIF_FREE(ctx.above_partition_ctx); STBI_AVIF_FREE(ctx.above_skip); STBI_AVIF_FREE(ctx.left_skip); STBI_AVIF_FREE(ctx.above_tx_intra); STBI_AVIF_FREE(ctx.left_tx_intra); STBI_AVIF_FREE(ctx.above_entropy[0]); STBI_AVIF_FREE(ctx.above_entropy[1]); STBI_AVIF_FREE(ctx.above_entropy[2]);

   rgba = stbi_avif__av1_planes_to_rgba(&planes,
                                         (int)seq->matrix_coefficients,
                                         seq->color_range);
   stbi_avif__av1_free_planes(&planes);
   return rgba;
}


static int stbi_avif__parse_box_header(const stbi_avif__buffer *buffer, size_t offset, size_t limit, stbi_avif__box *box)
{
   unsigned long small_size;
   size_t total_size;
   size_t header_size;

   if (offset > limit || limit > buffer->size)
      return stbi_avif__fail("invalid parse limit");
   if (limit - offset < 8)
      return stbi_avif__fail("truncated box header");

   small_size = stbi_avif__read_be32(buffer->data + offset);
   box->type = stbi_avif__read_be32(buffer->data + offset + 4);
   header_size = 8;
   total_size = (size_t)small_size;

   if (small_size == 1)
   {
      if (limit - offset < 16)
         return stbi_avif__fail("truncated large box header");
      total_size = stbi_avif__read_be_size(buffer->data + offset + 8, 8);
      header_size = 16;
   }
   else if (small_size == 0)
   {
      total_size = limit - offset;
   }

   if (total_size < header_size)
      return stbi_avif__fail("invalid box size");
   if (total_size > limit - offset)
      return stbi_avif__fail("box exceeds parent bounds");

   box->offset = offset;
   box->size = total_size;
   box->header_size = header_size;
   return 1;
}

static stbi_avif__item_info *stbi_avif__find_item(stbi_avif__parser *parser, unsigned int item_id)
{
   int i;
   for (i = 0; i < parser->item_count; ++i)
      if (parser->items[i].item_id == item_id)
         return &parser->items[i];
   return NULL;
}

static stbi_avif__item_assoc *stbi_avif__find_assoc(stbi_avif__parser *parser, unsigned int item_id)
{
   int i;
   for (i = 0; i < parser->assoc_count; ++i)
      if (parser->assocs[i].item_id == item_id)
         return &parser->assocs[i];
   return NULL;
}

static stbi_avif__item_location *stbi_avif__find_location(stbi_avif__parser *parser, unsigned int item_id)
{
   int i;
   for (i = 0; i < parser->location_count; ++i)
      if (parser->locations[i].item_id == item_id)
         return &parser->locations[i];
   return NULL;
}

static int stbi_avif__append_property(stbi_avif__parser *parser, const stbi_avif__property *property)
{
   if (!stbi_avif__grow_array((void **)&parser->properties, &parser->property_capacity, sizeof(parser->properties[0]), parser->property_count + 1))
      return 0;
   parser->properties[parser->property_count++] = *property;
   return 1;
}

static int stbi_avif__append_item(stbi_avif__parser *parser, const stbi_avif__item_info *item)
{
   if (!stbi_avif__grow_array((void **)&parser->items, &parser->item_capacity, sizeof(parser->items[0]), parser->item_count + 1))
      return 0;
   parser->items[parser->item_count++] = *item;
   return 1;
}

static int stbi_avif__append_location(stbi_avif__parser *parser, const stbi_avif__item_location *location)
{
   if (!stbi_avif__grow_array((void **)&parser->locations, &parser->location_capacity, sizeof(parser->locations[0]), parser->location_count + 1))
      return 0;
   parser->locations[parser->location_count++] = *location;
   return 1;
}

static int stbi_avif__append_assoc_entry(stbi_avif__parser *parser, unsigned int item_id, unsigned int property_index, int essential)
{
   stbi_avif__item_assoc *assoc;
   stbi_avif__association entry;

   assoc = stbi_avif__find_assoc(parser, item_id);
   if (assoc == NULL)
   {
      if (!stbi_avif__grow_array((void **)&parser->assocs, &parser->assoc_capacity, sizeof(parser->assocs[0]), parser->assoc_count + 1))
         return 0;
      assoc = &parser->assocs[parser->assoc_count++];
      assoc->item_id = item_id;
      assoc->entries = NULL;
      assoc->count = 0;
      assoc->capacity = 0;
   }

   if (assoc->count >= STBI_AVIF_MAX_ASSOCIATIONS)
      return stbi_avif__fail("too many property associations");

   if (!stbi_avif__grow_array((void **)&assoc->entries, &assoc->capacity, sizeof(assoc->entries[0]), assoc->count + 1))
      return 0;

   entry.property_index = property_index;
   entry.essential = essential;
   assoc->entries[assoc->count++] = entry;
   return 1;
}

static void stbi_avif__parser_free(stbi_avif__parser *parser)
{
   int i;
   for (i = 0; i < parser->assoc_count; ++i)
      STBI_AVIF_FREE(parser->assocs[i].entries);
   STBI_AVIF_FREE(parser->properties);
   STBI_AVIF_FREE(parser->items);
   STBI_AVIF_FREE(parser->assocs);
   STBI_AVIF_FREE(parser->locations);
   memset(parser, 0, sizeof(*parser));
}

static int stbi_avif__parse_ftyp(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   size_t payload_size;
   size_t cursor;
   unsigned long brand;

   payload = box->offset + box->header_size;
   payload_size = box->size - box->header_size;
   if (payload_size < 8)
      return stbi_avif__fail("ftyp box is too small");

   brand = stbi_avif__read_be32(buffer->data + payload);
   if (brand == STBI_AVIF_FOURCC('a','v','i','f') || brand == STBI_AVIF_FOURCC('a','v','i','s'))
      parser->has_avif_brand = 1;

   cursor = payload + 8;
   while (cursor + 4 <= payload + payload_size)
   {
      brand = stbi_avif__read_be32(buffer->data + cursor);
      if (brand == STBI_AVIF_FOURCC('a','v','i','f') || brand == STBI_AVIF_FOURCC('a','v','i','s'))
         parser->has_avif_brand = 1;
      cursor += 4;
   }

   parser->saw_ftyp = 1;
   return 1;
}

static int stbi_avif__parse_pitm(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   unsigned int version;

   payload = box->offset + box->header_size;
   if (!stbi_avif__range_check(buffer, payload, 4))
      return stbi_avif__fail("truncated pitm full box header");

   version = (unsigned int)buffer->data[payload];
   payload += 4;

   if (version == 0)
   {
      if (!stbi_avif__range_check(buffer, payload, 2))
         return stbi_avif__fail("truncated pitm item id");
      parser->primary_item_id = stbi_avif__read_be16(buffer->data + payload);
   }
   else
   {
      if (!stbi_avif__range_check(buffer, payload, 4))
         return stbi_avif__fail("truncated pitm item id");
      parser->primary_item_id = (unsigned int)stbi_avif__read_be32(buffer->data + payload);
   }

   return 1;
}

static int stbi_avif__parse_infe(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   unsigned int version;
   stbi_avif__item_info item;

   payload = box->offset + box->header_size;
   if (!stbi_avif__range_check(buffer, payload, 4))
      return stbi_avif__fail("truncated infe full box header");

   version = (unsigned int)buffer->data[payload];
   payload += 4;

   if (version < 2)
      return 1;

   memset(&item, 0, sizeof(item));
   if (version == 2)
   {
      if (!stbi_avif__range_check(buffer, payload, 8))
         return stbi_avif__fail("truncated infe v2 entry");
      item.item_id = stbi_avif__read_be16(buffer->data + payload);
      payload += 2;
      payload += 2;
      item.item_type = stbi_avif__read_be32(buffer->data + payload);
   }
   else
   {
      if (!stbi_avif__range_check(buffer, payload, 12))
         return stbi_avif__fail("truncated infe v3 entry");
      item.item_id = (unsigned int)stbi_avif__read_be32(buffer->data + payload);
      payload += 4;
      payload += 2;
      item.item_type = stbi_avif__read_be32(buffer->data + payload);
   }

   return stbi_avif__append_item(parser, &item);
}

static int stbi_avif__parse_iinf(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   size_t limit;
   unsigned int version;
   unsigned long entry_count;
   unsigned long i;
   stbi_avif__box child;

   payload = box->offset + box->header_size;
   limit = box->offset + box->size;
   if (!stbi_avif__range_check(buffer, payload, 4))
      return stbi_avif__fail("truncated iinf full box header");

   version = (unsigned int)buffer->data[payload];
   payload += 4;
   if (version == 0)
   {
      if (!stbi_avif__range_check(buffer, payload, 2))
         return stbi_avif__fail("truncated iinf entry count");
      entry_count = stbi_avif__read_be16(buffer->data + payload);
      payload += 2;
   }
   else
   {
      if (!stbi_avif__range_check(buffer, payload, 4))
         return stbi_avif__fail("truncated iinf entry count");
      entry_count = stbi_avif__read_be32(buffer->data + payload);
      payload += 4;
   }

   for (i = 0; i < entry_count && payload < limit; ++i)
   {
      if (!stbi_avif__parse_box_header(buffer, payload, limit, &child))
         return 0;
      if (child.type == STBI_AVIF_FOURCC('i','n','f','e'))
      {
         if (!stbi_avif__parse_infe(buffer, &child, parser))
            return 0;
      }
      payload += child.size;
   }

   return 1;
}

static int stbi_avif__parse_iloc(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   size_t limit;
   unsigned int version;
   unsigned int offset_size;
   unsigned int length_size;
   unsigned int base_offset_size;
   unsigned int index_size;
   unsigned long item_count;
   unsigned long i;

   payload = box->offset + box->header_size;
   limit = box->offset + box->size;
   if (!stbi_avif__range_check(buffer, payload, 6))
      return stbi_avif__fail("truncated iloc header");

   version = (unsigned int)buffer->data[payload];
   payload += 4;
   offset_size = ((unsigned int)buffer->data[payload] >> 4) & 15u;
   length_size = ((unsigned int)buffer->data[payload]) & 15u;
   ++payload;
   base_offset_size = ((unsigned int)buffer->data[payload] >> 4) & 15u;
   index_size = ((unsigned int)buffer->data[payload]) & 15u;
   ++payload;

   if (offset_size > sizeof(size_t) || length_size > sizeof(size_t) || base_offset_size > sizeof(size_t) || index_size > sizeof(size_t))
      return stbi_avif__fail("iloc field size exceeds host size_t");

   if (version < 2)
   {
      if (!stbi_avif__range_check(buffer, payload, 2))
         return stbi_avif__fail("truncated iloc item count");
      item_count = stbi_avif__read_be16(buffer->data + payload);
      payload += 2;
   }
   else
   {
      if (!stbi_avif__range_check(buffer, payload, 4))
         return stbi_avif__fail("truncated iloc item count");
      item_count = stbi_avif__read_be32(buffer->data + payload);
      payload += 4;
   }

   for (i = 0; i < item_count; ++i)
   {
      stbi_avif__item_location location;
      unsigned long extent_count;
      unsigned long j;

      memset(&location, 0, sizeof(location));
      if (version < 2)
      {
         if (!stbi_avif__range_check(buffer, payload, 2))
            return stbi_avif__fail("truncated iloc item id");
         location.item_id = stbi_avif__read_be16(buffer->data + payload);
         payload += 2;
      }
      else
      {
         if (!stbi_avif__range_check(buffer, payload, 4))
            return stbi_avif__fail("truncated iloc item id");
         location.item_id = (unsigned int)stbi_avif__read_be32(buffer->data + payload);
         payload += 4;
      }

      if (version == 1 || version == 2)
      {
         if (!stbi_avif__range_check(buffer, payload, 2))
            return stbi_avif__fail("truncated iloc construction method");
         location.construction_method = stbi_avif__read_be16(buffer->data + payload) & 15u;
         payload += 2;
      }

      if (!stbi_avif__range_check(buffer, payload, 2 + base_offset_size))
         return stbi_avif__fail("truncated iloc base offset");
      payload += 2;
      location.base_offset = stbi_avif__read_be_size(buffer->data + payload, (int)base_offset_size);
      payload += base_offset_size;

      if (!stbi_avif__range_check(buffer, payload, 2))
         return stbi_avif__fail("truncated iloc extent count");
      extent_count = stbi_avif__read_be16(buffer->data + payload);
      payload += 2;
      location.extent_count = (int)extent_count;

      for (j = 0; j < extent_count; ++j)
      {
         if (version == 1 || version == 2)
         {
            if (!stbi_avif__range_check(buffer, payload, index_size))
               return stbi_avif__fail("truncated iloc extent index");
            payload += index_size;
         }

         if (!stbi_avif__range_check(buffer, payload, offset_size + length_size))
            return stbi_avif__fail("truncated iloc extent");

         if (j == 0)
         {
            location.extent_offset = stbi_avif__read_be_size(buffer->data + payload, (int)offset_size);
            location.extent_length = stbi_avif__read_be_size(buffer->data + payload + offset_size, (int)length_size);
         }
         payload += offset_size + length_size;
      }

      if (!stbi_avif__append_location(parser, &location))
         return 0;
      if (payload > limit)
         return stbi_avif__fail("iloc exceeds parent bounds");
   }

   return 1;
}

static int stbi_avif__parse_ipco(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   size_t limit;
   stbi_avif__box child;

   payload = box->offset + box->header_size;
   limit = box->offset + box->size;
   while (payload < limit)
   {
      stbi_avif__property property;
      size_t child_payload;

      if (!stbi_avif__parse_box_header(buffer, payload, limit, &child))
         return 0;

      memset(&property, 0, sizeof(property));
      property.type = child.type;

      if (child.type == STBI_AVIF_FOURCC('i','s','p','e'))
      {
         child_payload = child.offset + child.header_size;
         if (!stbi_avif__range_check(buffer, child_payload, 12))
            return stbi_avif__fail("truncated ispe property");
         property.width = (unsigned int)stbi_avif__read_be32(buffer->data + child_payload + 4);
         property.height = (unsigned int)stbi_avif__read_be32(buffer->data + child_payload + 8);
      }
      else if (child.type == STBI_AVIF_FOURCC('a','v','1','C'))
      {
         child_payload = child.offset + child.header_size;
         if (!stbi_avif__range_check(buffer, child_payload, 4))
            return stbi_avif__fail("truncated av1C property");
         parser->has_av1_config = 1;
         property.data_offset = child_payload;
         property.data_size = child.size - child.header_size;
      }

      if (!stbi_avif__append_property(parser, &property))
         return 0;
      payload += child.size;
   }

   return 1;
}

static int stbi_avif__parse_ipma(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   unsigned int version;
   unsigned int flags;
   unsigned long entry_count;
   unsigned long i;

   payload = box->offset + box->header_size;
   if (!stbi_avif__range_check(buffer, payload, 4))
      return stbi_avif__fail("truncated ipma full box header");

   version = (unsigned int)buffer->data[payload];
   flags = (((unsigned int)buffer->data[payload + 1]) << 16) |
           (((unsigned int)buffer->data[payload + 2]) << 8) |
           ((unsigned int)buffer->data[payload + 3]);
   payload += 4;

   if (!stbi_avif__range_check(buffer, payload, 4))
      return stbi_avif__fail("truncated ipma entry count");
   entry_count = stbi_avif__read_be32(buffer->data + payload);
   payload += 4;

   for (i = 0; i < entry_count; ++i)
   {
      unsigned int item_id;
      unsigned int association_count;
      unsigned int j;

      if (version < 1)
      {
         if (!stbi_avif__range_check(buffer, payload, 2))
            return stbi_avif__fail("truncated ipma item id");
         item_id = stbi_avif__read_be16(buffer->data + payload);
         payload += 2;
      }
      else
      {
         if (!stbi_avif__range_check(buffer, payload, 4))
            return stbi_avif__fail("truncated ipma item id");
         item_id = (unsigned int)stbi_avif__read_be32(buffer->data + payload);
         payload += 4;
      }

      if (!stbi_avif__range_check(buffer, payload, 1))
         return stbi_avif__fail("truncated ipma association count");
      association_count = (unsigned int)buffer->data[payload++];

      for (j = 0; j < association_count; ++j)
      {
         unsigned int essential;
         unsigned int property_index;

         if ((flags & 1u) != 0)
         {
            unsigned int raw_value;
            if (!stbi_avif__range_check(buffer, payload, 2))
               return stbi_avif__fail("truncated ipma association");
            raw_value = stbi_avif__read_be16(buffer->data + payload);
            essential = (raw_value >> 15) & 1u;
            property_index = raw_value & 0x7fffu;
            payload += 2;
         }
         else
         {
            unsigned int raw_value2;
            if (!stbi_avif__range_check(buffer, payload, 1))
               return stbi_avif__fail("truncated ipma association");
            raw_value2 = (unsigned int)buffer->data[payload++];
            essential = (raw_value2 >> 7) & 1u;
            property_index = raw_value2 & 0x7fu;
         }

         if (!stbi_avif__append_assoc_entry(parser, item_id, property_index, (int)essential))
            return 0;
      }
   }

   return 1;
}

static int stbi_avif__parse_iprp(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   size_t limit;
   stbi_avif__box child;

   payload = box->offset + box->header_size;
   limit = box->offset + box->size;
   while (payload < limit)
   {
      if (!stbi_avif__parse_box_header(buffer, payload, limit, &child))
         return 0;
      if (child.type == STBI_AVIF_FOURCC('i','p','c','o'))
      {
         if (!stbi_avif__parse_ipco(buffer, &child, parser))
            return 0;
      }
      else if (child.type == STBI_AVIF_FOURCC('i','p','m','a'))
      {
         if (!stbi_avif__parse_ipma(buffer, &child, parser))
            return 0;
      }
      payload += child.size;
   }
   return 1;
}

static int stbi_avif__parse_meta(const stbi_avif__buffer *buffer, const stbi_avif__box *box, stbi_avif__parser *parser)
{
   size_t payload;
   size_t limit;
   stbi_avif__box child;

   payload = box->offset + box->header_size;
   limit = box->offset + box->size;
   if (!stbi_avif__range_check(buffer, payload, 4))
      return stbi_avif__fail("truncated meta full box header");
   payload += 4;

   while (payload < limit)
   {
      if (!stbi_avif__parse_box_header(buffer, payload, limit, &child))
         return 0;
      if (child.type == STBI_AVIF_FOURCC('p','i','t','m'))
      {
         if (!stbi_avif__parse_pitm(buffer, &child, parser))
            return 0;
      }
      else if (child.type == STBI_AVIF_FOURCC('i','i','n','f'))
      {
         if (!stbi_avif__parse_iinf(buffer, &child, parser))
            return 0;
      }
      else if (child.type == STBI_AVIF_FOURCC('i','l','o','c'))
      {
         if (!stbi_avif__parse_iloc(buffer, &child, parser))
            return 0;
      }
      else if (child.type == STBI_AVIF_FOURCC('i','p','r','p'))
      {
         if (!stbi_avif__parse_iprp(buffer, &child, parser))
            return 0;
      }
      payload += child.size;
   }

   parser->saw_meta = 1;
   return 1;
}

static int stbi_avif__resolve_primary(const stbi_avif__buffer *buffer, stbi_avif__parser *parser)
{
   stbi_avif__item_info *item;
   stbi_avif__item_assoc *assoc;
   stbi_avif__item_location *location;
   int i;

   if (parser->primary_item_id == 0)
      return stbi_avif__fail("missing primary item id");

   item = stbi_avif__find_item(parser, parser->primary_item_id);
   if (item != NULL && item->item_type != 0 && item->item_type != STBI_AVIF_FOURCC('a','v','0','1'))
      return stbi_avif__fail("primary item is not av01");

   assoc = stbi_avif__find_assoc(parser, parser->primary_item_id);
   if (assoc == NULL)
      return stbi_avif__fail("missing primary item property associations");

   for (i = 0; i < assoc->count; ++i)
   {
      unsigned int property_index;
      stbi_avif__property *property;

      property_index = assoc->entries[i].property_index;
      if (property_index == 0 || property_index > (unsigned int)parser->property_count)
         continue;
      property = &parser->properties[property_index - 1];
      if (property->type == STBI_AVIF_FOURCC('i','s','p','e'))
      {
         parser->width = property->width;
         parser->height = property->height;
      }
      else if (property->type == STBI_AVIF_FOURCC('a','v','1','C'))
      {
         parser->has_av1_config = 1;
         parser->av1c_offset = property->data_offset;
         parser->av1c_size = property->data_size;
      }
   }

   if (parser->width == 0 || parser->height == 0)
      return stbi_avif__fail("missing ispe dimensions for primary item");
   if (!parser->has_av1_config)
      return stbi_avif__fail("missing av1C configuration");
   location = stbi_avif__find_location(parser, parser->primary_item_id);
   if (location == NULL)
      return stbi_avif__fail("missing iloc entry for primary item");
   if (location->construction_method != 0)
      return stbi_avif__fail("only file-offset iloc construction is supported");
   if (location->extent_count != 1)
      return stbi_avif__fail("only single-extent AVIF items are supported");

   if (location->base_offset > ((size_t)-1) - location->extent_offset)
      return stbi_avif__fail("primary item offset overflow");
   parser->payload_offset = location->base_offset + location->extent_offset;
   parser->payload_size = location->extent_length;
   if (!stbi_avif__range_check(buffer, parser->payload_offset, parser->payload_size))
      return stbi_avif__fail("primary item payload is out of bounds");

   return 1;
}

static int stbi_avif__parse_file(const unsigned char *data, size_t size, stbi_avif__parser *parser)
{
   stbi_avif__buffer buffer;
   size_t cursor;
   stbi_avif__box box;

   memset(parser, 0, sizeof(*parser));
   buffer.data = data;
   buffer.size = size;

   cursor = 0;
   while (cursor < size)
   {
      if (!stbi_avif__parse_box_header(&buffer, cursor, size, &box))
      {
         stbi_avif__parser_free(parser);
         return 0;
      }

      if (box.type == STBI_AVIF_FOURCC('f','t','y','p'))
      {
         if (!stbi_avif__parse_ftyp(&buffer, &box, parser))
         {
            stbi_avif__parser_free(parser);
            return 0;
         }
      }
      else if (box.type == STBI_AVIF_FOURCC('m','e','t','a'))
      {
         if (!stbi_avif__parse_meta(&buffer, &box, parser))
         {
            stbi_avif__parser_free(parser);
            return 0;
         }
      }

      cursor += box.size;
   }

   if (!parser->saw_ftyp)
   {
      stbi_avif__parser_free(parser);
      return stbi_avif__fail("missing ftyp box");
   }
   if (!parser->has_avif_brand)
   {
      stbi_avif__parser_free(parser);
      return stbi_avif__fail("file is not branded as avif or avis");
   }
   if (!parser->saw_meta)
   {
      stbi_avif__parser_free(parser);
      return stbi_avif__fail("missing meta box");
   }
   if (!stbi_avif__resolve_primary(&buffer, parser))
   {
      stbi_avif__parser_free(parser);
      return 0;
   }

   return 1;
}

int stbi_avif_info_from_memory(const unsigned char *buffer, int len, int *x, int *y, int *channels_in_file)
{
   stbi_avif__parser parser;
   int ok;

   if (buffer == NULL || len <= 0)
      return stbi_avif__fail("invalid AVIF buffer");

   ok = stbi_avif__parse_file(buffer, (size_t)len, &parser);
   if (!ok)
      return 0;

   if (x != NULL)
      *x = (int)parser.width;
   if (y != NULL)
      *y = (int)parser.height;
   if (channels_in_file != NULL)
      *channels_in_file = STBI_AVIF_CHANNELS;

   stbi_avif__parser_free(&parser);
   return 1;
}

unsigned char *stbi_avif_load_from_memory(const unsigned char *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels)
{
   stbi_avif__parser parser;
   stbi_avif__av1_headers headers;
   stbi_avif__av1_frame_index frame_index;
   stbi_avif__av1_frame_header frame_header;
   stbi_avif__av1_tile_group_header tile_group;
   int ok;

   (void)desired_channels;
   if (buffer == NULL || len <= 0)
      return (unsigned char *)stbi_avif__fail_ptr("invalid AVIF buffer");

   ok = stbi_avif__parse_file(buffer, (size_t)len, &parser);
   if (!ok)
      return NULL;

   if (x != NULL)
      *x = (int)parser.width;
   if (y != NULL)
      *y = (int)parser.height;
   if (channels_in_file != NULL)
      *channels_in_file = STBI_AVIF_CHANNELS;

    ok = stbi_avif__parse_av1_headers(buffer, (size_t)len, &parser, &headers);
    if (!ok)
    {
       stbi_avif__parser_free(&parser);
       return NULL;
    }

   ok = stbi_avif__index_av1_frame_obus(buffer + parser.payload_offset, parser.payload_size, &frame_index);
   if (!ok)
   {
      stbi_avif__parser_free(&parser);
      return NULL;
   }

   ok = stbi_avif__parse_av1_frame_header(buffer + parser.payload_offset + frame_index.frame_header_offset,
                                          frame_index.frame_header_size,
                                          &headers.sequence_header,
                                          &frame_header);
   if (!ok)
   {
      stbi_avif__parser_free(&parser);
      return NULL;
   }

   ok = stbi_avif__parse_av1_tile_group_header(buffer + parser.payload_offset + frame_index.tile_group_offset,
                                               frame_index.tile_group_size,
                                               frame_index.frame_is_combined_obu ? frame_header.header_bits_consumed : 0u,
                                               &frame_header,
                                               &tile_group);
   if (!ok)
   {
      stbi_avif__parser_free(&parser);
      return NULL;
   }

   /* Full decode: plane allocation + superblock traversal + YUV→RGBA. */
   {
      const unsigned char *tile_data = buffer + parser.payload_offset + frame_index.tile_group_offset;
      size_t               tile_size = frame_index.tile_group_size;
      unsigned char *rgba;
      stbi_avif__parser_free(&parser);
      rgba = stbi_avif__av1_decode(
                tile_data,
                tile_size,
                &headers.sequence_header,
                &frame_header,
                &tile_group);
      return rgba;
   }
}

static int stbi_avif__read_file(const char *filename, unsigned char **out_data, size_t *out_size)
{
   FILE *fp;
   long length;
   unsigned char *data;
   size_t read_count;

   *out_data = NULL;
   *out_size = 0;
   fp = fopen(filename, "rb");
   if (fp == NULL)
      return stbi_avif__fail("could not open file");

   if (fseek(fp, 0, SEEK_END) != 0)
   {
      fclose(fp);
      return stbi_avif__fail("could not seek file");
   }

   length = ftell(fp);
   if (length < 0)
   {
      fclose(fp);
      return stbi_avif__fail("could not determine file size");
   }

   if (fseek(fp, 0, SEEK_SET) != 0)
   {
      fclose(fp);
      return stbi_avif__fail("could not rewind file");
   }

   data = (unsigned char *)STBI_AVIF_MALLOC((size_t)length);
   if (data == NULL)
   {
      fclose(fp);
      return stbi_avif__fail("out of memory");
   }

   read_count = fread(data, 1, (size_t)length, fp);
   fclose(fp);
   if (read_count != (size_t)length)
   {
      STBI_AVIF_FREE(data);
      return stbi_avif__fail("could not read file");
   }

   *out_data = data;
   *out_size = (size_t)length;
   return 1;
}

int stbi_avif_info(const char *filename, int *x, int *y, int *channels_in_file)
{
   unsigned char *data;
   size_t size;
   int result;

   if (filename == NULL)
      return stbi_avif__fail("invalid file name");
   if (!stbi_avif__read_file(filename, &data, &size))
      return 0;

   result = stbi_avif_info_from_memory(data, (int)size, x, y, channels_in_file);
   STBI_AVIF_FREE(data);
   return result;
}

unsigned char *stbi_avif_load(const char *filename, int *x, int *y, int *channels_in_file, int desired_channels)
{
   unsigned char *data;
   size_t size;
   unsigned char *result;

   if (filename == NULL)
      return (unsigned char *)stbi_avif__fail_ptr("invalid file name");
   if (!stbi_avif__read_file(filename, &data, &size))
      return NULL;

   result = stbi_avif_load_from_memory(data, (int)size, x, y, channels_in_file, desired_channels);
   STBI_AVIF_FREE(data);
   return result;
}

#endif
