#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_AVIF_IMPLEMENTATION
#include "../stb_avif.h"

static int write_ppm_rgb(const char *filename, const unsigned char *pixels, int width, int height, int channels)
{
   FILE *fp;
   int x;
   int y;

   if (filename == NULL || pixels == NULL || width <= 0 || height <= 0 || channels <= 0)
      return 0;

   fp = fopen(filename, "wb");
   if (fp == NULL)
      return 0;

   if (fprintf(fp, "P6\n%d %d\n255\n", width, height) < 0)
   {
      fclose(fp);
      return 0;
   }

   for (y = 0; y < height; ++y)
   {
      for (x = 0; x < width; ++x)
      {
         const unsigned char *p;
         p = pixels + ((y * width + x) * channels);
         if (fputc((int)p[0], fp) == EOF ||
             fputc((int)p[1], fp) == EOF ||
             fputc((int)p[2], fp) == EOF)
         {
            fclose(fp);
            return 0;
         }
      }
   }

   if (fclose(fp) != 0)
      return 0;
   return 1;
}

static int has_ext(const char *filename, const char *ext)
{
   size_t n;
   size_t e;
   if (filename == NULL || ext == NULL)
      return 0;
   n = strlen(filename);
   e = strlen(ext);
   if (n < e)
      return 0;
   return strcmp(filename + n - e, ext) == 0;
}

static void write_be32(unsigned char *out, unsigned long v)
{
   out[0] = (unsigned char)((v >> 24) & 255u);
   out[1] = (unsigned char)((v >> 16) & 255u);
   out[2] = (unsigned char)((v >> 8) & 255u);
   out[3] = (unsigned char)(v & 255u);
}

static unsigned long stbi_avif__crc32_update(unsigned long crc, const unsigned char *data, size_t len)
{
   size_t i;
   int bit;
   for (i = 0; i < len; ++i)
   {
      crc ^= (unsigned long)data[i];
      for (bit = 0; bit < 8; ++bit)
      {
         if (crc & 1u)
            crc = 0xedb88320u ^ (crc >> 1);
         else
            crc >>= 1;
      }
   }
   return crc;
}

static unsigned long stbi_avif__adler32(const unsigned char *data, size_t len)
{
   unsigned long s1;
   unsigned long s2;
   size_t i;

   s1 = 1u;
   s2 = 0u;
   for (i = 0; i < len; ++i)
   {
      s1 = (s1 + data[i]) % 65521u;
      s2 = (s2 + s1) % 65521u;
   }
   return (s2 << 16) | s1;
}

static int write_png_rgba(const char *filename, const unsigned char *pixels, int width, int height, int channels)
{
   FILE *fp;
   unsigned char sig[8];
   unsigned char ihdr[13];
   unsigned char iend[1];
   unsigned char *raw;
   unsigned char *zlib;
   unsigned char crc_bytes[4];
   unsigned long crc;
   unsigned long adler;
   size_t row_bytes;
   size_t raw_size;
   size_t zlib_size;
   size_t raw_pos;
   size_t zpos;
   size_t remain;
   size_t block_len;
   int y;
   int x;
   int color_type;
   int pixel_bytes;

   if (filename == NULL || pixels == NULL || width <= 0 || height <= 0 || channels < 3)
      return 0;

   color_type = (channels >= 4) ? 6 : 2;
   pixel_bytes = (color_type == 6) ? 4 : 3;
   row_bytes = (size_t)width * (size_t)pixel_bytes + 1u;
   raw_size = row_bytes * (size_t)height;

   raw = (unsigned char *)malloc(raw_size);
   if (raw == NULL)
      return 0;

   raw_pos = 0u;
   for (y = 0; y < height; ++y)
   {
      raw[raw_pos++] = 0u;
      for (x = 0; x < width; ++x)
      {
         const unsigned char *p = pixels + (((size_t)y * (size_t)width + (size_t)x) * (size_t)channels);
         raw[raw_pos++] = p[0];
         raw[raw_pos++] = p[1];
         raw[raw_pos++] = p[2];
         if (pixel_bytes == 4)
            raw[raw_pos++] = (channels >= 4) ? p[3] : 255u;
      }
   }

   zlib_size = 2u + raw_size + ((raw_size + 65534u) / 65535u) * 5u + 4u;
   zlib = (unsigned char *)malloc(zlib_size);
   if (zlib == NULL)
   {
      free(raw);
      return 0;
   }

   zpos = 0u;
   zlib[zpos++] = 0x78u;
   zlib[zpos++] = 0x01u;
   remain = raw_size;
   raw_pos = 0u;
   while (remain > 0u)
   {
      block_len = remain > 65535u ? 65535u : remain;
      zlib[zpos++] = (unsigned char)(remain <= 65535u ? 1u : 0u);
      zlib[zpos++] = (unsigned char)(block_len & 255u);
      zlib[zpos++] = (unsigned char)((block_len >> 8) & 255u);
      zlib[zpos++] = (unsigned char)((~block_len) & 255u);
      zlib[zpos++] = (unsigned char)(((~block_len) >> 8) & 255u);
      memcpy(zlib + zpos, raw + raw_pos, block_len);
      zpos += block_len;
      raw_pos += block_len;
      remain -= block_len;
   }

   adler = stbi_avif__adler32(raw, raw_size);
   free(raw);
   write_be32(crc_bytes, adler);
   memcpy(zlib + zpos, crc_bytes, 4u);
   zpos += 4u;

   fp = fopen(filename, "wb");
   if (fp == NULL)
   {
      free(zlib);
      return 0;
   }

   sig[0] = 137u; sig[1] = 80u; sig[2] = 78u; sig[3] = 71u;
   sig[4] = 13u;  sig[5] = 10u; sig[6] = 26u; sig[7] = 10u;
   if (fwrite(sig, 1u, 8u, fp) != 8u)
   {
      fclose(fp);
      free(zlib);
      return 0;
   }

   write_be32(crc_bytes, 13u);
   if (fwrite(crc_bytes, 1u, 4u, fp) != 4u || fwrite("IHDR", 1u, 4u, fp) != 4u)
   {
      fclose(fp);
      free(zlib);
      return 0;
   }
   write_be32(ihdr + 0, (unsigned long)width);
   write_be32(ihdr + 4, (unsigned long)height);
   ihdr[8] = 8u;
   ihdr[9] = (unsigned char)color_type;
   ihdr[10] = 0u;
   ihdr[11] = 0u;
   ihdr[12] = 0u;
   if (fwrite(ihdr, 1u, 13u, fp) != 13u)
   {
      fclose(fp);
      free(zlib);
      return 0;
   }
   crc = 0xffffffffu;
   crc = stbi_avif__crc32_update(crc, (const unsigned char *)"IHDR", 4u);
   crc = stbi_avif__crc32_update(crc, ihdr, 13u) ^ 0xffffffffu;
   write_be32(crc_bytes, crc);
   if (fwrite(crc_bytes, 1u, 4u, fp) != 4u)
   {
      fclose(fp);
      free(zlib);
      return 0;
   }

   write_be32(crc_bytes, (unsigned long)zpos);
   if (fwrite(crc_bytes, 1u, 4u, fp) != 4u || fwrite("IDAT", 1u, 4u, fp) != 4u || fwrite(zlib, 1u, zpos, fp) != zpos)
   {
      fclose(fp);
      free(zlib);
      return 0;
   }
   crc = 0xffffffffu;
   crc = stbi_avif__crc32_update(crc, (const unsigned char *)"IDAT", 4u);
   crc = stbi_avif__crc32_update(crc, zlib, zpos) ^ 0xffffffffu;
   write_be32(crc_bytes, crc);
   free(zlib);
   if (fwrite(crc_bytes, 1u, 4u, fp) != 4u)
   {
      fclose(fp);
      return 0;
   }

   write_be32(crc_bytes, 0u);
   iend[0] = 0u;
   if (fwrite(crc_bytes, 1u, 4u, fp) != 4u || fwrite("IEND", 1u, 4u, fp) != 4u)
   {
      fclose(fp);
      return 0;
   }
   crc = 0xffffffffu;
   crc = stbi_avif__crc32_update(crc, (const unsigned char *)"IEND", 4u) ^ 0xffffffffu;
   write_be32(crc_bytes, crc);
   if (fwrite(crc_bytes, 1u, 4u, fp) != 4u)
   {
      fclose(fp);
      return 0;
   }

   if (fclose(fp) != 0)
      return 0;
   return 1;
}

static int write_bmp_rgb(const char *filename, const unsigned char *pixels, int width, int height, int channels)
{
   FILE *fp;
   unsigned char header[54];
   unsigned char *row;
   int x;
   int y;
   int pad;
   unsigned long row_size;
   unsigned long image_size;
   unsigned long file_size;

   if (filename == NULL || pixels == NULL || width <= 0 || height <= 0 || channels < 3)
      return 0;

   pad = (4 - ((width * 3) & 3)) & 3;
   row_size = (unsigned long)(width * 3 + pad);
   image_size = row_size * (unsigned long)height;
   file_size = 54u + image_size;

   memset(header, 0, sizeof(header));
   header[0] = 'B';
   header[1] = 'M';
   header[2] = (unsigned char)(file_size & 255u);
   header[3] = (unsigned char)((file_size >> 8) & 255u);
   header[4] = (unsigned char)((file_size >> 16) & 255u);
   header[5] = (unsigned char)((file_size >> 24) & 255u);
   header[10] = 54u;
   header[14] = 40u;
   header[18] = (unsigned char)(width & 255);
   header[19] = (unsigned char)((width >> 8) & 255);
   header[20] = (unsigned char)((width >> 16) & 255);
   header[21] = (unsigned char)((width >> 24) & 255);
   header[22] = (unsigned char)(height & 255);
   header[23] = (unsigned char)((height >> 8) & 255);
   header[24] = (unsigned char)((height >> 16) & 255);
   header[25] = (unsigned char)((height >> 24) & 255);
   header[26] = 1u;
   header[28] = 24u;

   row = (unsigned char *)malloc(row_size);
   if (row == NULL)
      return 0;

   fp = fopen(filename, "wb");
   if (fp == NULL)
   {
      free(row);
      return 0;
   }

   if (fwrite(header, 1u, 54u, fp) != 54u)
   {
      fclose(fp);
      free(row);
      return 0;
   }

   for (y = height - 1; y >= 0; --y)
   {
      memset(row, 0, row_size);
      for (x = 0; x < width; ++x)
      {
         const unsigned char *p = pixels + (((size_t)y * (size_t)width + (size_t)x) * (size_t)channels);
         row[x * 3 + 0] = p[2];
         row[x * 3 + 1] = p[1];
         row[x * 3 + 2] = p[0];
      }
      if (fwrite(row, 1u, row_size, fp) != row_size)
      {
         fclose(fp);
         free(row);
         return 0;
      }
   }

   free(row);
   if (fclose(fp) != 0)
      return 0;
   return 1;
}

static int write_image(const char *filename, const unsigned char *pixels, int width, int height, int channels)
{
   if (has_ext(filename, ".ppm"))
      return write_ppm_rgb(filename, pixels, width, height, channels);
   if (has_ext(filename, ".bmp"))
      return write_bmp_rgb(filename, pixels, width, height, channels);
   if (has_ext(filename, ".png"))
      return write_png_rgba(filename, pixels, width, height, channels);
   return 0;
}

int main(int argc, char **argv)
{
   int width;
   int height;
   int channels;
   unsigned char *pixels;
   const char *output_path;

   if (argc < 2)
   {
      printf("usage: %s image.avif [output.ppm|output.png|output.bmp]\n", argv[0]);
      return 0;
   }

   output_path = NULL;
   if (argc >= 3)
      output_path = argv[2];

   if (!stbi_avif_info(argv[1], &width, &height, &channels))
   {
      printf("info failed: %s\n", stbi_avif_failure_reason());
      return 1;
   }

   printf("container parsed: %d x %d, channels=%d\n", width, height, channels);

   pixels = stbi_avif_load(argv[1], &width, &height, &channels, 4);
   if (pixels == NULL)
   {
      printf("decode failed: %s\n", stbi_avif_failure_reason());
      return 2;
   }

   if (output_path != NULL)
   {
      /* Basic sanity: verify image dimensions and pixel values are sane */
      {
         int px;
         int all_gray = 1;
         if (width <= 0 || height <= 0 || channels < 3)
         {
            printf("sanity fail: bad dimensions w=%d h=%d ch=%d\n", width, height, channels);
            stbi_avif_image_free(pixels);
            return 4;
         }
         /* Check that at least some pixels differ (not all-gray placeholder);
          * px increments by channels so px/channels is the pixel index (0..1023) */
         for (px = 0; px < width * height * channels && px < 1024 * channels; px += channels)
         {
            if (pixels[px] != pixels[0] || pixels[px+1] != pixels[1] || pixels[px+2] != pixels[2])
            {
               all_gray = 0;
               break;
            }
         }
         if (all_gray && width * height > 64)
            printf("warning: first 1024 pixels are all the same color - possible decode error\n");
      }
      printf("decode info: %d x %d, channels=%d\n", width, height, channels);
      if (!write_image(output_path, pixels, width, height, channels))
      {
         stbi_avif_image_free(pixels);
         printf("convert failed: could not write %s (supported: .ppm, .png, .bmp)\n", output_path);
         return 3;
      }
      printf("converted to: %s\n", output_path);
   }

   stbi_avif_image_free(pixels);

   printf("decode succeeded\n");
   return 0;
}