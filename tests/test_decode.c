#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char **argv)
{
   int width;
   int height;
   int channels;
   unsigned char *pixels;
   const char *output_ppm;

   if (argc < 2)
   {
      printf("usage: %s image.avif [output.ppm]\n", argv[0]);
      return 0;
   }

   output_ppm = NULL;
   if (argc >= 3)
      output_ppm = argv[2];

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

   if (output_ppm != NULL)
   {
      if (!write_ppm_rgb(output_ppm, pixels, width, height, channels))
      {
         stbi_avif_image_free(pixels);
         printf("convert failed: could not write %s\n", output_ppm);
         return 3;
      }
      printf("converted to: %s\n", output_ppm);
   }

   stbi_avif_image_free(pixels);

   printf("decode succeeded\n");
   return 0;
}