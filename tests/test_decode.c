#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static int path_is_shell_safe(const char *path)
{
   const unsigned char *p;
   if (path == NULL)
      return 0;
   for (p = (const unsigned char *)path; *p != 0; ++p)
   {
      if (*p == '"' || *p == '\\' || *p == '`' || *p == '$')
         return 0;
   }
   return 1;
}

static int write_image(const char *filename, const unsigned char *pixels, int width, int height, int channels)
{
   int is_png;
   int is_jpg;
   char temp_ppm[256];
   char command[1024];
   const char *fmt;
   int rc;

   if (has_ext(filename, ".ppm"))
      return write_ppm_rgb(filename, pixels, width, height, channels);

   is_png = has_ext(filename, ".png");
   is_jpg = has_ext(filename, ".jpg") || has_ext(filename, ".jpeg");
   if (!is_png && !is_jpg)
      return 0;

   if (!path_is_shell_safe(filename))
      return 0;

   sprintf(temp_ppm, "/tmp/stb_avif_%lu_%u.ppm", (unsigned long)time(NULL), (unsigned int)rand());
   if (!write_ppm_rgb(temp_ppm, pixels, width, height, channels))
      return 0;

   fmt = is_png ? "png" : "jpeg";
   sprintf(command, "sips -s format %s \"%s\" --out \"%s\" >/dev/null 2>&1", fmt, temp_ppm, filename);
   rc = system(command);
   remove(temp_ppm);
   return rc == 0;
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
      printf("usage: %s image.avif [output.ppm|output.png|output.jpg]\n", argv[0]);
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
      if (!write_image(output_path, pixels, width, height, channels))
      {
         stbi_avif_image_free(pixels);
         printf("convert failed: could not write %s (supported: .ppm, .png, .jpg, .jpeg)\n", output_path);
         return 3;
      }
      printf("converted to: %s\n", output_path);
   }

   stbi_avif_image_free(pixels);

   printf("decode succeeded\n");
   return 0;
}