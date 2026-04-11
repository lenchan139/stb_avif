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
} stbi_avif__av1_sequence_header;

typedef struct
{
   int saw_sequence_header;
   int saw_frame_header;
   stbi_avif__av1_sequence_header sequence_header;
} stbi_avif__av1_headers;

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

static int stbi_avif__parse_av1_sequence_header(const unsigned char *data, size_t size, stbi_avif__av1_sequence_header *header)
{
   stbi_avif__bit_reader bits;
   unsigned long value;
   unsigned long frame_width_bits_minus_1;
   unsigned long frame_height_bits_minus_1;
   int high_bitdepth;
   int twelve_bit;
   int color_description_present_flag;

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

   if (!stbi_avif__bit_skip(&bits, 3))
      return 0;

   if (!stbi_avif__bit_read_flag(&bits, &high_bitdepth))
      return 0;
   twelve_bit = 0;
   if (header->seq_profile == 2u && high_bitdepth)
   {
      if (!stbi_avif__bit_read_flag(&bits, &twelve_bit))
         return 0;
   }
   header->bit_depth = twelve_bit ? 12u : (high_bitdepth ? 10u : 8u);
   if (header->bit_depth != 8u)
      return stbi_avif__fail("only 8-bit AV1 content is supported");

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

   stbi_avif__parser_free(&parser);
   return (unsigned char *)stbi_avif__fail_ptr("AV1 headers parsed, but frame reconstruction is not implemented yet");
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