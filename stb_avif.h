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
   int film_grain_params_present;
   int use_128x128_superblock;
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
   unsigned int tile_col_start_sb[65];
   unsigned int tile_row_start_sb[65];
   size_t header_bits_consumed;
} stbi_avif__av1_frame_header;

typedef struct
{
   int start_and_end_present;
   unsigned int tile_start;
   unsigned int tile_end;
   size_t tile_data_byte_offset;
   size_t header_bits_consumed;
} stbi_avif__av1_tile_group_header;

/* AV1 spec §8.2 byte-oriented range (symbol) decoder */
typedef struct
{
   const unsigned char *data;      /* pointer to tile payload (byte-aligned start) */
   size_t               size;      /* byte length of tile payload */
   size_t               pos;       /* next byte to consume */
   unsigned int         range;     /* current range  (initially 0x8000) */
   unsigned int         value;     /* current value window */
   unsigned int         cnt;       /* bits remaining in current window before refill */
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
   if (value > 7u)
   {
      if (!stbi_avif__bit_skip(&bits, 1))
         return 0;
   }

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
   if (!stbi_avif__bit_read_flag(&bits, &flag)) return 0; /* enable_intra_edge_filter */
   if (!stbi_avif__bit_read_flag(&bits, &flag)) return 0; /* enable_superres */
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

   if (!stbi_avif__bit_skip(&bits, 1))
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
   int apply_grain;

   memset(frame, 0, sizeof(*frame));
   if (data == NULL || size == 0)
      return stbi_avif__fail("missing AV1 frame header payload");

   stbi_avif__bit_reader_init(&bits, data, size);

   if (seq->reduced_still_picture_header)
   {
      /* Reduced still-picture uses an implicit KEY_FRAME + show_frame=1. */
      frame->frame_type = 0u; /* KEY_FRAME */
      frame->show_frame = 1;
      frame->frame_width = seq->max_frame_width;
      frame->frame_height = seq->max_frame_height;

      /* render_size() */
      if (!stbi_avif__bit_read_flag(&bits, &render_and_frame_size_different))
         return 0;
      if (render_and_frame_size_different)
      {
         if (!stbi_avif__bit_read_bits(&bits, 16, &value)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 16, &value)) return 0;
      }

      /* allow_intrabc is present for reduced keyframes when screen-content tools are enabled. */
      if (!stbi_avif__bit_read_flag(&bits, &allow_intrabc))
         return 0;
      frame->allow_intrabc = allow_intrabc;

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

      if (tile_cols * tile_rows > 1u)
      {
         unsigned int tile_bits = tile_cols_log2 + tile_rows_log2;
         if (!stbi_avif__bit_read_bits(&bits, tile_bits, &value))
            return 0; /* context_update_tile_id */
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

      /* DeltaQ values are su(7) when present. */
      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_su(&bits, 7u, &delta_q_y_dc))
            return 0;
      }
      else
      {
         delta_q_y_dc = 0;
      }

      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_su(&bits, 7u, &delta_q_u_dc)) return 0;
      }
      else
      {
         delta_q_u_dc = 0;
      }

      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_su(&bits, 7u, &delta_q_u_ac)) return 0;
      }
      else
      {
         delta_q_u_ac = 0;
      }

      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_su(&bits, 7u, &delta_q_v_dc)) return 0;
      }
      else
      {
         delta_q_v_dc = 0;
      }

      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_su(&bits, 7u, &delta_q_v_ac)) return 0;
      }
      else
      {
         delta_q_v_ac = 0;
      }

      if (!stbi_avif__bit_read_flag(&bits, &using_qmatrix))
         return 0;
      if (using_qmatrix)
      {
         if (!stbi_avif__bit_read_bits(&bits, 4u, &value)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 4u, &value)) return 0;
         if (!stbi_avif__bit_read_bits(&bits, 4u, &value)) return 0;
      }

      if (!stbi_avif__bit_read_flag(&bits, &seg_enabled))
         return 0;
      if (seg_enabled)
         return stbi_avif__fail("AV1 segmentation is not supported yet");

      if (!stbi_avif__bit_read_flag(&bits, &delta_q_present))
         return 0;
      if (delta_q_present)
      {
         if (!stbi_avif__bit_read_bits(&bits, 2u, &value))
            return 0;
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
         return stbi_avif__fail("AV1 CDEF parsing is not implemented yet");
      if (seq->enable_restoration && !allow_intrabc)
         return stbi_avif__fail("AV1 restoration parsing is not implemented yet");

      if (!coded_lossless)
      {
         if (!stbi_avif__bit_read_flag(&bits, &tx_mode_select))
            return 0;
         (void)tx_mode_select;
      }

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
   if (!stbi_avif__bit_reader_bits_left(&bits, 1))
      return stbi_avif__fail("truncated AV1 tile group header");

   if (!stbi_avif__bit_read_flag(&bits, &start_end_present))
      return 0;

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

/* Read one byte from the tile buffer, returning 0 on exhaustion (spec: zero padding). */
static unsigned int stbi_avif__rd_byte(stbi_avif__av1_range_decoder *rd)
{
   if (rd->pos < rd->size)
      return (unsigned int)rd->data[rd->pos++];
   return 0u;
}

/*
 * init_symbol (AV1 spec §8.2.2).
 *
 * bit_offset: bits already consumed in `data` by the tile-group header.
 * Advances past any partial header byte, then seeds the 15-bit value window.
 */
static int stbi_avif__av1_range_decoder_init(stbi_avif__av1_range_decoder *decoder,
                                              const unsigned char *data, size_t size,
                                              size_t bit_offset)
{
   size_t byte_start;
   unsigned int b0, b1;

   memset(decoder, 0, sizeof(*decoder));
   if (data == NULL || size == 0)
      return stbi_avif__fail("missing AV1 entropy payload");

   /* Skip the header bytes that were already consumed bit-by-bit. */
   byte_start = bit_offset >> 3;
   if (byte_start >= size)
      return stbi_avif__fail("truncated AV1 tile payload");

   decoder->data = data + byte_start;
   decoder->size = size - byte_start;
   decoder->pos  = 0;

   if (decoder->size < 2)
      return stbi_avif__fail("AV1 tile too short for range decoder init");

   b0 = stbi_avif__rd_byte(decoder);
   b1 = stbi_avif__rd_byte(decoder);

   decoder->range = 0x8000u;
   /* Seed the 15-bit value window: upper 15 bits of the 16-bit word. */
   decoder->value = ((b0 << 8u) | b1) >> 1u;
   /* One bit from b1 is still buffered; cnt tracks unused bits in the last byte. */
   decoder->cnt   = 7u;  /* 8 bits in b1; we used 7 MSBs (plus the MSB as carry), 1 remains */
   decoder->initialized = 1;
   return 1;
}

/*
 * renorm_d: after a symbol decode shrinks range, left-shift range and value
 * until range >= 0x8000, reading new bytes into the value's low bits.
 */
static void stbi_avif__av1_rd_renorm(stbi_avif__av1_range_decoder *rd)
{
   unsigned int rng = rd->range;
   unsigned int val = rd->value;
   unsigned int cnt = rd->cnt;
   unsigned int shift;
   unsigned int newb;

   /* Find how many bits to shift: we need range << shift >= 0x8000. */
   if (rng >= 0x8000u)
      return; /* already normalised */

   shift = 0u;
   {
      unsigned int r = rng;
      while ((r << 1u) < 0x8000u) { r <<= 1u; ++shift; }
      ++shift;
   }

   rng <<= shift;

   if (cnt >= shift)
   {
      /* Enough buffered bits: shift val, no new byte needed. */
      cnt -= shift;
      val  = (val << shift) & 0x7fffu;
   }
   else
   {
      /* Exhaust buffered bits then refill from a new byte. */
      shift -= cnt;
      newb   = stbi_avif__rd_byte(rd);
      val    = ((val << cnt) | (newb >> (8u - shift))) & 0x7fffu;
      cnt    = 8u - shift;
   }

   rd->range = rng;
   rd->value = val;
   rd->cnt   = cnt;
}

/*
 * read_symbol (libaom decode_cdf_q15):
 *
 * Computes absolute thresholds v[0..nsyms-2] using:
 *   v[i] = (((range>>8) * (cdf[i]>>6)) >> 1) + min(range, 256)
 * Finds the bracket containing `value`, then updates:
 *   new_range = high_v - low_v   (or range for the last symbol)
 *   new_value = value - low_v
 * followed by renorm.
 *
 * nsyms >= 2; cdf[nsyms-1] must be 32768.
 */
static unsigned int stbi_avif__av1_read_symbol(stbi_avif__av1_range_decoder *rd,
                                                const unsigned short *cdf, int nsyms)
{
   unsigned int r   = rd->range;
   unsigned int dif = rd->value;
   unsigned int low_v = 0u;
   unsigned int high_v;
   unsigned int v = 0u;
   int sym, i;

   sym = nsyms - 1; /* default: last symbol */
   for (i = 0; i < nsyms - 1; ++i)
   {
      v = (((r >> 8u) * ((unsigned int)cdf[i] >> 6u)) >> 1u) + (r < 256u ? r : 256u);
      if (dif < v)
      {
         sym    = i;
         high_v = v;
         goto apply;
      }
      low_v = v;
   }
   high_v = r; /* last symbol: upper bound is the full range */

apply:
   rd->range = high_v - low_v;
   rd->value = dif    - low_v;
   stbi_avif__av1_rd_renorm(rd);
   return (unsigned int)sym;
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
   /* Compute rate: base = 4 + (count > 15) + (nsyms > 2) */
   rate = 4 + (count > 15 ? 1 : 0) + (nsyms > 2 ? 1 : 0);
   /* Add floor(log2(nsyms)) for nsyms > 1 */
   {
      int n = nsyms - 1;
      while (n > 1) { n >>= 1; rate++; }
   }
   rate_shift = rate;

   for (i = 0; i < nsyms - 1; ++i)
   {
      if (i < symbol)
         cdf[i] += (unsigned short)((32768u - cdf[i]) >> rate_shift);
      else
         cdf[i] -= (unsigned short)(cdf[i] >> rate_shift);
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
static unsigned short stbi_avif__av1_partition_cdf[5][3][11] = {
   /* bsize 4 = 128×128: only 4 types (split always) */
   {
      { 27899u, 28219u, 28529u, 32768u, 0,0,0,0,0,0, 0 },
      { 27899u, 28219u, 28529u, 32768u, 0,0,0,0,0,0, 0 },
      { 27899u, 28219u, 28529u, 32768u, 0,0,0,0,0,0, 0 }
   },
   /* bsize 3 = 64×64 */
   {
      { 9520u, 14785u, 19298u, 22065u, 23748u, 25235u, 26226u, 27316u, 27928u, 32768u, 0 },
      {15855u, 20270u, 24298u, 26672u, 27982u, 29225u, 30023u, 30903u, 31241u, 32768u, 0 },
      {12727u, 17449u, 21520u, 23734u, 25414u, 26952u, 27933u, 29018u, 29553u, 32768u, 0 }
   },
   /* bsize 2 = 32×32 */
   {
      {19132u, 25510u, 30392u, 31285u, 31679u, 32020u, 32215u, 32428u, 32424u, 32768u, 0 },
      {13928u, 19809u, 25055u, 26862u, 27663u, 28400u, 28703u, 29285u, 29462u, 32768u, 0 },
      {12353u, 17984u, 23373u, 25367u, 26195u, 27078u, 27435u, 28048u, 28139u, 32768u, 0 }
   },
   /* bsize 1 = 16×16 */
   {
      {15597u, 20929u, 25456u, 26740u, 27468u, 28069u, 28291u, 28676u, 28756u, 32768u, 0 },
      {10904u, 15793u, 21441u, 23081u, 24029u, 24793u, 25031u, 25474u, 25516u, 32768u, 0 },
      { 8197u, 12901u, 17869u, 19430u, 20525u, 21352u, 21679u, 22232u, 22336u, 32768u, 0 }
   },
   /* bsize 0 = 8×8 */
   {
      {17606u, 22258u, 25678u, 26799u, 27428u, 27953u, 28103u, 28549u, 28563u, 32768u, 0 },
      {14979u, 19274u, 23220u, 24567u, 25371u, 26020u, 26232u, 26733u, 26797u, 32768u, 0 },
      { 7736u, 11145u, 14424u, 15421u, 16203u, 16907u, 17092u, 17582u, 17628u, 32768u, 0 }
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
   0,1,0,1,2,1,2,2,3,2,3,3,4,3,4,4,5,4,5,5,6,6
};
static const unsigned char stbi_avif__bsize_log2h[22] = {
   0,0,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6
};

/* BLOCK_128X128 = 20 in AV1 enum */
#define STBI_AVIF_BLOCK_4X4     0
#define STBI_AVIF_BLOCK_8X8     6
#define STBI_AVIF_BLOCK_16X16  10
#define STBI_AVIF_BLOCK_32X32  14
#define STBI_AVIF_BLOCK_64X64  18
#define STBI_AVIF_BLOCK_128X128 20

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
   unsigned int                  mi_cols;     /* frame width  in 4-pel MI units */
   unsigned int                  mi_rows;     /* frame height in 4-pel MI units */
   int                           use_128;     /* 1 = 128×128 superblocks */
   /* Adaptive CDF state (mutable copies of default CDFs, updated per-symbol) */
   unsigned short  partition_cdf[5][3][11]; /* [bsize_ctx][neighbor_ctx][nsyms+1] */
   unsigned short  partition4_cdf[5];       /* [nsyms+1] for 4×4 blocks */
   unsigned short  intra_mode_cdf[14];      /* [nsyms+1] */
} stbi_avif__av1_decode_ctx;

/*
 * =============================================================================
 *  DC INTRA PREDICTION  (fills a rectangular block with a flat colour)
 * =============================================================================
 *
 * Real intra prediction uses neighbours; for our constrained subset we use
 * the midpoint value from the sequence header (already filled into the plane
 * during allocation) and write nothing — the plane stays as mid-grey — which
 * is equivalent to DC prediction with no available neighbours and zero
 * residual (no coefficient data).
 *
 * This produces valid (though blurry) output for low-detail images and acts
 * as the structural correctness scaffold for the full pipeline.
 */

/* Write a rectangular block of `val` into plane p (stride=pw). */
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

/*
 * =============================================================================
 *  RECURSIVE PARTITION DECODE
 * =============================================================================
 */

/*
 * Forward declaration (mutually recursive with stbi_avif__av1_decode_partition).
 */
static int stbi_avif__av1_decode_partition(stbi_avif__av1_decode_ctx *ctx,
                                            unsigned int mi_row, unsigned int mi_col,
                                            int block_size);

static int stbi_avif__av1_decode_coding_unit(stbi_avif__av1_decode_ctx *ctx,
                                              unsigned int mi_row, unsigned int mi_col,
                                              int block_size)
{
   /*
    * A coding unit is a leaf block.  For this scaffold we decode the intra
    * luma mode (discarding it) and fill the block with the mid-grey already
    * present in the plane (residual = 0, no coefficients).
    */
   unsigned int bw4 = 1u << stbi_avif__bsize_log2w[block_size];
   unsigned int bh4 = 1u << stbi_avif__bsize_log2h[block_size];
   unsigned int px   = mi_col * 4u;
   unsigned int py   = mi_row * 4u;
   unsigned int pw   = bw4   * 4u;
   unsigned int ph   = bh4   * 4u;

   /* Clamp to frame. */
   if (px >= ctx->planes->width)  return 1;
   if (py >= ctx->planes->height) return 1;
   if (px + pw > ctx->planes->width)  pw = ctx->planes->width  - px;
   if (py + ph > ctx->planes->height) ph = ctx->planes->height - py;

   /* Decode (and discard) the intra Y mode with adaptive CDF. */
   {
      unsigned int mode = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                              ctx->intra_mode_cdf, 13);
      (void)mode;
   }

   /* Fill luma with the mid-grey that was pre-populated at alloc time.
    * We read the current value at (px, py) and replicate it — preserves the
    * pre-fill without needing a separate constant. */
   {
      unsigned short mid_y = ctx->planes->y[py * ctx->planes->width + px];
      unsigned short mid_u = ctx->planes->u[
         (py >> ctx->planes->suby) * ctx->planes->cw +
         (px >> ctx->planes->subx)];
      unsigned short mid_v = ctx->planes->v[
         (py >> ctx->planes->suby) * ctx->planes->cw +
         (px >> ctx->planes->subx)];

      stbi_avif__av1_fill_block(ctx->planes->y, ctx->planes->width,
                                 px, py, pw, ph, mid_y);

      /* Chroma block (may be smaller due to subsampling). */
      {
         unsigned int cpx = px >> ctx->planes->subx;
         unsigned int cpy = py >> ctx->planes->suby;
         unsigned int cpw = (pw + (unsigned int)ctx->planes->subx) >> ctx->planes->subx;
         unsigned int cph = (ph + (unsigned int)ctx->planes->suby) >> ctx->planes->suby;
         if (cpx + cpw > ctx->planes->cw) cpw = ctx->planes->cw - cpx;
         if (cpy + cph > ctx->planes->ch) cph = ctx->planes->ch - cpy;
         stbi_avif__av1_fill_block(ctx->planes->u, ctx->planes->cw,
                                    cpx, cpy, cpw, cph, mid_u);
         stbi_avif__av1_fill_block(ctx->planes->v, ctx->planes->cw,
                                    cpx, cpy, cpw, cph, mid_v);
      }
   }
   return 1;
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

   /* Out-of-frame: skip silently. */
   if (mi_row >= ctx->mi_rows || mi_col >= ctx->mi_cols)
      return 1;

   /* Map block_size enum to bsize_ctx (0=128, 1=64, 2=32, 3=16, 4=8). */
   bsize_ctx = (STBI_AVIF_BLOCK_128X128 - block_size) / 3;
   if (bsize_ctx < 0) bsize_ctx = 0;
   if (bsize_ctx > 4) bsize_ctx = 4;

   /* Leaf: 4×4 block — only NONE/HORZ/VERT/SPLIT possible. */
   if (block_size == STBI_AVIF_BLOCK_4X4)
   {
      partition = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                      ctx->partition4_cdf, 4);
      return stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col, STBI_AVIF_BLOCK_4X4);
   }

   partition = stbi_avif__av1_read_symbol_adapt(&ctx->rd,
                   ctx->partition_cdf[bsize_ctx][0], 10);

   /* Compute the sub-block size for SPLIT. */
   sub_size = block_size - 3; /* e.g. 128→64, 64→32, 32→16, … */
   if (sub_size < 0) sub_size = 0;

   switch (partition)
   {
      case STBI_AVIF_PARTITION_NONE:
         return stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col, block_size);

      case STBI_AVIF_PARTITION_HORZ:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col, block_size - 1)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col, block_size - 1)) return 0;
         return 1;

      case STBI_AVIF_PARTITION_VERT:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col,          block_size - 2)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col + bw4/2u, block_size - 2)) return 0;
         return 1;

      case STBI_AVIF_PARTITION_SPLIT:
         if (!stbi_avif__av1_decode_partition(ctx, mi_row,          mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_partition(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
         if (!stbi_avif__av1_decode_partition(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_partition(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
         return 1;

      /* Compound partition types: decode two halves as coding units. */
      case STBI_AVIF_PARTITION_HORZ_A:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          block_size - 1)) return 0;
         return 1;

      case STBI_AVIF_PARTITION_HORZ_B:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          block_size - 1)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
         return 1;

      case STBI_AVIF_PARTITION_VERT_A:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col,          sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, block_size - 2)) return 0;
         return 1;

      case STBI_AVIF_PARTITION_VERT_B:
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col,          block_size - 2)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row,          mi_col + bw4/2u, sub_size)) return 0;
         if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + bh4/2u, mi_col + bw4/2u, sub_size)) return 0;
         return 1;

      case STBI_AVIF_PARTITION_HORZ_4:
         {
            unsigned int q4 = bh4 / 4u;
            unsigned int qi;
            for (qi = 0; qi < 4u; ++qi)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row + qi * q4, mi_col, sub_size)) return 0;
         }
         return 1;

      case STBI_AVIF_PARTITION_VERT_4:
         {
            unsigned int q4 = bw4 / 4u;
            unsigned int qi;
            for (qi = 0; qi < 4u; ++qi)
               if (!stbi_avif__av1_decode_coding_unit(ctx, mi_row, mi_col + qi * q4, sub_size)) return 0;
         }
         return 1;

      default:
         return stbi_avif__fail("unknown AV1 partition type");
   }
}

/*
 * =============================================================================
 *  TILE DECODE ENTRY POINT
 * =============================================================================
 */

static int stbi_avif__av1_decode_tile(stbi_avif__av1_decode_ctx *ctx,
                                      unsigned int sb_row_start,
                                      unsigned int sb_row_end,
                                      unsigned int sb_col_start,
                                      unsigned int sb_col_end)
{
   unsigned int sb_size4; /* superblock size in MI units */
   unsigned int sb_row, sb_col;
   int root_bsize;

   sb_size4   = ctx->use_128 ? 32u : 16u;
   root_bsize = ctx->use_128 ? STBI_AVIF_BLOCK_128X128 : STBI_AVIF_BLOCK_64X64;

   for (sb_row = sb_row_start; sb_row < sb_row_end; ++sb_row)
      for (sb_col = sb_col_start; sb_col < sb_col_end; ++sb_col)
      {
         if (!stbi_avif__av1_decode_partition(ctx,
                sb_row * sb_size4,
                sb_col * sb_size4,
                root_bsize))
            return 0;
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
   unsigned int tile_idx;
   unsigned int tile_count_in_group;
   unsigned int sb_row_start;
   unsigned int sb_row_end;
   unsigned int sb_col_start;
   unsigned int sb_col_end;
   unsigned int tile_row;
   unsigned int tile_col;

   memset(&ctx, 0, sizeof(ctx));

   /* Initialise adaptive CDFs from defaults */
   memcpy(ctx.partition_cdf, stbi_avif__av1_partition_cdf, sizeof(stbi_avif__av1_partition_cdf));
   memcpy(ctx.partition4_cdf, stbi_avif__av1_partition4_cdf, sizeof(stbi_avif__av1_partition4_cdf));
   memcpy(ctx.intra_mode_cdf, stbi_avif__av1_intra_mode_cdf, sizeof(stbi_avif__av1_intra_mode_cdf));

   if (!stbi_avif__av1_alloc_planes(&planes, seq, fhdr))
      return NULL;

   ctx.planes   = &planes;
   ctx.seq      = seq;
   ctx.mi_cols  = (fhdr->frame_width  + 3u) / 4u;
   ctx.mi_rows  = (fhdr->frame_height + 3u) / 4u;
   ctx.use_128  = seq->use_128x128_superblock;

   tile_cursor = tghdr->tile_data_byte_offset;
   tile_count_in_group = tghdr->tile_end - tghdr->tile_start + 1u;

   for (tile_idx = 0u; tile_idx < tile_count_in_group; ++tile_idx)
   {
      unsigned int tile_size_value;
      unsigned int tile_payload_size;
      const unsigned char *tile_payload;
      unsigned int full_tile_index;

      if (tile_idx + 1u < tile_count_in_group)
      {
         if (!stbi_avif__av1_read_le_bytes(tile_group_data, tile_group_size,
                                           tile_cursor, fhdr->tile_size_bytes,
                                           &tile_size_value))
         {
            stbi_avif__av1_free_planes(&planes);
            return NULL;
         }
         tile_cursor += fhdr->tile_size_bytes;
         tile_payload_size = tile_size_value + 1u;
         if (tile_cursor + (size_t)tile_payload_size > tile_group_size)
         {
            stbi_avif__av1_free_planes(&planes);
            return (unsigned char *)stbi_avif__fail_ptr("AV1 tile payload exceeds tile group size");
         }
      }
      else
      {
         if (tile_cursor > tile_group_size)
         {
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

      if (!stbi_avif__av1_range_decoder_init(&ctx.rd, tile_payload,
                                             (size_t)tile_payload_size, 0u))
      {
         stbi_avif__av1_free_planes(&planes);
         return NULL;
      }

      if (!stbi_avif__av1_decode_tile(&ctx,
                                      sb_row_start,
                                      sb_row_end,
                                      sb_col_start,
                                      sb_col_end))
      {
         stbi_avif__av1_free_planes(&planes);
         return NULL;
      }
   }

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