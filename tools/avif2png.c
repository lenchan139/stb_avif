/*
 * avif2png — minimal AVIF-to-PNG command-line converter.
 *
 * Depends only on stb_avif.h (no external libraries).
 *
 * Build:
 *   cc -O2 tools/avif2png.c -o avif2png -lm
 *
 * Usage:
 *   avif2png input.avif output.png
 *
 * Exit codes:
 *   0  success
 *   1  wrong usage
 *   2  decode error
 *   3  write error
 */

#define STB_AVIF_IMPLEMENTATION
#define STB_AVIF_WRITE_PNG
#include "../stb_avif.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
   int w, h, channels, ok;
   unsigned char *pixels;

   if (argc != 3)
   {
      fprintf(stderr, "usage: avif2png input.avif output.png\n");
      return 1;
   }

   pixels = stbi_avif_load(argv[1], &w, &h, &channels, 0);
   if (!pixels)
   {
      fprintf(stderr, "avif2png: failed to decode '%s': %s\n",
              argv[1], stbi_avif_failure_reason());
      return 2;
   }

   ok = stbi_avif_write_png(argv[2], pixels, w, h, channels);
   stbi_avif_image_free(pixels);

   if (!ok)
   {
      fprintf(stderr, "avif2png: failed to write '%s'\n", argv[2]);
      return 3;
   }

   printf("avif2png: %s -> %s (%d x %d, %d ch)\n",
          argv[1], argv[2], w, h, channels);
   return 0;
}
