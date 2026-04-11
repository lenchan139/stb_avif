#include <stdio.h>

#define STB_AVIF_IMPLEMENTATION
#include "../stb_avif.h"

int main(int argc, char **argv)
{
   int width;
   int height;
   int channels;
   unsigned char *pixels;

   if (argc < 2)
   {
      printf("usage: %s image.avif\n", argv[0]);
      return 0;
   }

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

   stbi_avif_image_free(pixels);
   printf("decode succeeded\n");
   return 0;
}