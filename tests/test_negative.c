/*
 * test_negative.c — negative / robustness test suite for stb_avif.h
 *
 * Each test passes a malformed or minimal byte sequence to the decoder and
 * verifies that:
 *   - the call returns NULL (decode) or 0 (info),
 *   - stbi_avif_failure_reason() returns a non-empty string,
 *   - no crash occurs.
 *
 * Build:
 *   cc -std=c89 -Wall -Wextra -pedantic tests/test_negative.c -o tests/test_negative -lm
 *
 * Exit code 0 = all tests passed, non-zero = at least one test failed.
 */

#define STB_AVIF_IMPLEMENTATION
#define STB_AVIF_WRITE_PNG
#include "../stb_avif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */

static int failures = 0;

static void check_decode_fails(const char *name,
                                const unsigned char *buf, int len)
{
   int w = 0, h = 0, ch = 0;
   unsigned char *out;

   out = stbi_avif_load_from_memory(buf, len, &w, &h, &ch, 4);
   if (out != NULL)
   {
      printf("FAIL [decode %s]: expected NULL but got pixels\n", name);
      stbi_avif_image_free(out);
      ++failures;
   }
   else if (!stbi_avif_failure_reason() || stbi_avif_failure_reason()[0] == '\0')
   {
      printf("FAIL [decode %s]: NULL returned but failure_reason is empty\n", name);
      ++failures;
   }
   else
   {
      printf("PASS [decode %s]: %s\n", name, stbi_avif_failure_reason());
   }
}

static void check_info_fails(const char *name,
                              const unsigned char *buf, int len)
{
   int w = 0, h = 0, ch = 0;
   int ok;

   ok = stbi_avif_info_from_memory(buf, len, &w, &h, &ch);
   if (ok)
   {
      printf("FAIL [info %s]: expected 0 but got %d\n", name, ok);
      ++failures;
   }
   else if (!stbi_avif_failure_reason() || stbi_avif_failure_reason()[0] == '\0')
   {
      printf("FAIL [info %s]: 0 returned but failure_reason is empty\n", name);
      ++failures;
   }
   else
   {
      printf("PASS [info %s]: %s\n", name, stbi_avif_failure_reason());
   }
}

static void check_cdf_update_rate_cases(void)
{
   unsigned short cdf3[4];
   unsigned short cdf5[6];

   /* 3 symbols (+count): expected rate uses no size bump (rate=4 at count=0). */
   cdf3[0] = 16384u;
   cdf3[1] = 24576u;
   cdf3[2] = 32768u;
   cdf3[3] = 0u; /* count */
   stbi_avif__av1_update_cdf(cdf3, 1, 3);
   if (cdf3[0] != 15360u)
   {
      printf("FAIL [cdf_rate_n3]: got %u expected 15360\n", (unsigned)cdf3[0]);
      ++failures;
   }
   else
   {
      printf("PASS [cdf_rate_n3]\n");
   }

   /* 5 symbols (+count): expected +1 size bump (rate=5 at count=0). */
   cdf5[0] = 16384u;
   cdf5[1] = 20000u;
   cdf5[2] = 24000u;
   cdf5[3] = 28000u;
   cdf5[4] = 32768u;
   cdf5[5] = 0u; /* count */
   stbi_avif__av1_update_cdf(cdf5, 1, 5);
   if (cdf5[0] != 15872u)
   {
      printf("FAIL [cdf_rate_n5]: got %u expected 15872\n", (unsigned)cdf5[0]);
      ++failures;
   }
   else
   {
      printf("PASS [cdf_rate_n5]\n");
   }
}

/* --------------------------------------------------------------------- */
/*  Shared test data buffers                                               */
/* --------------------------------------------------------------------- */

/* Minimal valid ftyp box (20 bytes): size=20, type='ftyp', brand='avif',
 * minor=0, compat='avif' */
static const unsigned char k_ftyp_avif[20] = {
   0x00, 0x00, 0x00, 0x14,  /* size = 20 */
   0x66, 0x74, 0x79, 0x70,  /* 'ftyp' */
   0x61, 0x76, 0x69, 0x66,  /* 'avif' major brand */
   0x00, 0x00, 0x00, 0x00,  /* minor version */
   0x61, 0x76, 0x69, 0x66   /* 'avif' compat brand */
};

/* ftyp box with wrong brand ('mp4f') */
static const unsigned char k_ftyp_mp4f[20] = {
   0x00, 0x00, 0x00, 0x14,
   0x66, 0x74, 0x79, 0x70,
   0x6d, 0x70, 0x34, 0x66,  /* 'mp4f' */
   0x00, 0x00, 0x00, 0x00,
   0x6d, 0x70, 0x34, 0x66
};

/* A box claiming extended (8-byte) size of 0x0000000100000000 (4 GB+),
 * which on a 32-bit platform must be rejected; on a 64-bit platform the
 * box size would exceed the buffer size and also be rejected. */
static const unsigned char k_large_box[16] = {
   0x00, 0x00, 0x00, 0x01,  /* small_size = 1 -> extended */
   0x66, 0x74, 0x79, 0x70,  /* 'ftyp' */
   0x00, 0x00, 0x00, 0x01,  /* high 32 bits = 1  (>4 GB) */
   0x00, 0x00, 0x00, 0x00   /* low  32 bits = 0 */
};

/* Extended-size box with high 32 bits = 0 but low 32 bits indicating a
 * size larger than the 16-byte buffer (= 32 bytes), so it exceeds bounds */
static const unsigned char k_large_box_oob[16] = {
   0x00, 0x00, 0x00, 0x01,
   0x66, 0x74, 0x79, 0x70,
   0x00, 0x00, 0x00, 0x00,  /* high = 0 */
   0x00, 0x00, 0x00, 0x20   /* low  = 32 > 16 */
};

/* ftyp + meta stub that is truncated before pitm content */
static const unsigned char k_ftyp_plus_truncated_meta[28] = {
   /* ftyp (20 bytes) */
   0x00, 0x00, 0x00, 0x14,
   0x66, 0x74, 0x79, 0x70,
   0x61, 0x76, 0x69, 0x66,
   0x00, 0x00, 0x00, 0x00,
   0x61, 0x76, 0x69, 0x66,
   /* meta (8 bytes header only, no children) */
   0x00, 0x00, 0x00, 0x08,
   0x6d, 0x65, 0x74, 0x61   /* 'meta' */
};

/* --------------------------------------------------------------------- */

int main(void)
{
   unsigned char zeros[64];
   unsigned char garbage[64];
   unsigned char truncated_ftyp[10];
   int i;

   memset(zeros, 0, sizeof(zeros));

   for (i = 0; i < (int)sizeof(garbage); ++i)
      garbage[i] = (unsigned char)((i * 83 + 17) & 0xFF); /* simple deterministic fill */

   memset(truncated_ftyp, 0, sizeof(truncated_ftyp));
   /* write size=20 (ftyp would need 20 bytes but we only provide 10) */
   truncated_ftyp[0] = 0x00; truncated_ftyp[1] = 0x00;
   truncated_ftyp[2] = 0x00; truncated_ftyp[3] = 0x14;
   truncated_ftyp[4] = 0x66; truncated_ftyp[5] = 0x74;
   truncated_ftyp[6] = 0x79; truncated_ftyp[7] = 0x70; /* 'ftyp' */

   printf("==> Negative / robustness tests\n");

   check_cdf_update_rate_cases();

   /* --- NULL / empty input ------------------------------------------- */
   check_decode_fails("null_buffer",  NULL, 0);
   check_decode_fails("zero_length",  zeros, 0);
   check_info_fails("null_buffer_info",  NULL, 0);

   /* --- Garbage data -------------------------------------------------- */
   check_decode_fails("all_zeros",    zeros,   (int)sizeof(zeros));
   check_decode_fails("random_bytes", garbage, (int)sizeof(garbage));

   /* --- Truncated box header ----------------------------------------- */
   /* Only 4 bytes — not enough for a box header (needs 8) */
   check_decode_fails("only_4_bytes", zeros, 4);

   /* ftyp with correct size field but buffer cut short */
   check_decode_fails("truncated_ftyp", truncated_ftyp, 10);

   /* --- Wrong AVIF brand --------------------------------------------- */
   check_decode_fails("wrong_brand_mp4f",
                      k_ftyp_mp4f, (int)sizeof(k_ftyp_mp4f));

   /* --- Extended (8-byte) box size overflows / out-of-bounds ---------- */
   check_decode_fails("large_box_4gb_plus",
                      k_large_box, (int)sizeof(k_large_box));
   check_decode_fails("large_box_oob",
                      k_large_box_oob, (int)sizeof(k_large_box_oob));

   /* --- Valid ftyp but no meta box ------------------------------------ */
   check_decode_fails("ftyp_no_meta",
                      k_ftyp_avif, (int)sizeof(k_ftyp_avif));

   /* --- ftyp + truncated meta ---------------------------------------- */
   check_decode_fails("ftyp_truncated_meta",
                      k_ftyp_plus_truncated_meta,
                      (int)sizeof(k_ftyp_plus_truncated_meta));

   /* --- NULL filename ------------------------------------------------- */
   {
      int w = 0, h = 0, ch = 0;
      unsigned char *out = stbi_avif_load(NULL, &w, &h, &ch, 0);
      if (out != NULL)
      {
         printf("FAIL [null_filename]: expected NULL\n");
         stbi_avif_image_free(out);
         ++failures;
      }
      else
      {
         printf("PASS [null_filename]: %s\n", stbi_avif_failure_reason());
      }
   }
   {
      int w = 0, h = 0, ch = 0;
      int ok = stbi_avif_info(NULL, &w, &h, &ch);
      if (ok)
      {
         printf("FAIL [null_filename_info]: expected 0\n");
         ++failures;
      }
      else
      {
         printf("PASS [null_filename_info]: %s\n", stbi_avif_failure_reason());
      }
   }

   /* --- Non-existent file -------------------------------------------- */
   {
      int w = 0, h = 0, ch = 0;
      unsigned char *out = stbi_avif_load(
         "/nonexistent/path/to/file.avif", &w, &h, &ch, 0);
      if (out != NULL)
      {
         printf("FAIL [no_such_file]: expected NULL\n");
         stbi_avif_image_free(out);
         ++failures;
      }
      else
      {
         printf("PASS [no_such_file]: %s\n", stbi_avif_failure_reason());
      }
   }

   /* --- Empty file ---------------------------------------------------- */
   {
      FILE *fp;
      int w = 0, h = 0, ch = 0;
      unsigned char *out;
      const char *tmp = "/tmp/stb_avif_test_empty.avif";

      fp = fopen(tmp, "wb");
      if (fp) fclose(fp);  /* create zero-byte file */

      out = stbi_avif_load(tmp, &w, &h, &ch, 0);
      if (out != NULL)
      {
         printf("FAIL [empty_file]: expected NULL\n");
         stbi_avif_image_free(out);
         ++failures;
      }
      else
      {
         printf("PASS [empty_file]: %s\n", stbi_avif_failure_reason());
      }
      remove(tmp);
   }

   /* --- PNG writer: invalid arguments --------------------------------- */
   {
      unsigned char px[4];
      int png_len = 0;
      unsigned char *png;

      memset(px, 128, sizeof(px));

      /* NULL pixels */
      png = stbi_avif_write_png_to_memory(NULL, 1, 1, 4, &png_len);
      if (png != NULL)
      {
         printf("FAIL [png_null_pixels]: expected NULL\n");
         stbi_avif_image_free(png);
         ++failures;
      }
      else
         printf("PASS [png_null_pixels]\n");

      /* zero width */
      png = stbi_avif_write_png_to_memory(px, 0, 1, 4, &png_len);
      if (png != NULL)
      {
         printf("FAIL [png_zero_width]: expected NULL\n");
         stbi_avif_image_free(png);
         ++failures;
      }
      else
         printf("PASS [png_zero_width]\n");

      /* zero height */
      png = stbi_avif_write_png_to_memory(px, 1, 0, 4, &png_len);
      if (png != NULL)
      {
         printf("FAIL [png_zero_height]: expected NULL\n");
         stbi_avif_image_free(png);
         ++failures;
      }
      else
         printf("PASS [png_zero_height]\n");

      /* negative width */
      png = stbi_avif_write_png_to_memory(px, -1, 1, 4, &png_len);
      if (png != NULL)
      {
         printf("FAIL [png_neg_width]: expected NULL\n");
         stbi_avif_image_free(png);
         ++failures;
      }
      else
         printf("PASS [png_neg_width]\n");

      /* bad channel count */
      png = stbi_avif_write_png_to_memory(px, 1, 1, 2, &png_len);
      if (png != NULL)
      {
         printf("FAIL [png_bad_channels]: expected NULL\n");
         stbi_avif_image_free(png);
         ++failures;
      }
      else
         printf("PASS [png_bad_channels]\n");

      /* NULL out_len */
      png = stbi_avif_write_png_to_memory(px, 1, 1, 4, NULL);
      if (png != NULL)
      {
         printf("FAIL [png_null_outlen]: expected NULL\n");
         stbi_avif_image_free(png);
         ++failures;
      }
      else
         printf("PASS [png_null_outlen]\n");
   }

   /* --- PNG writer: successful round-trip (1x1 pixel) ---------------- */
   {
      unsigned char px_rgba[4];
      unsigned char px_rgb[3];
      unsigned char px_gray[1];
      int png_len = 0;
      unsigned char *png;

      px_rgba[0] = 255u; px_rgba[1] = 128u; px_rgba[2] = 0u; px_rgba[3] = 200u;
      px_rgb[0]  = 64u;  px_rgb[1]  = 32u;  px_rgb[2]  = 16u;
      px_gray[0] = 77u;

      /* RGBA */
      png = stbi_avif_write_png_to_memory(px_rgba, 1, 1, 4, &png_len);
      if (png == NULL || png_len < 67)  /* minimum valid PNG is ~67 bytes */
      {
         printf("FAIL [png_roundtrip_rgba]: got NULL or too small (%d bytes)\n", png_len);
         ++failures;
      }
      else
      {
         /* Check PNG signature */
         if (png[0] == 137u && png[1] == 80u && png[2] == 78u && png[3] == 71u)
            printf("PASS [png_roundtrip_rgba]: %d bytes\n", png_len);
         else
         {
            printf("FAIL [png_roundtrip_rgba]: bad signature\n");
            ++failures;
         }
         stbi_avif_image_free(png);
      }

      /* RGB */
      png = stbi_avif_write_png_to_memory(px_rgb, 1, 1, 3, &png_len);
      if (png == NULL || png[0] != 137u)
      {
         printf("FAIL [png_roundtrip_rgb]: got NULL or bad signature\n");
         ++failures;
      }
      else
      {
         printf("PASS [png_roundtrip_rgb]: %d bytes\n", png_len);
         stbi_avif_image_free(png);
      }

      /* Grayscale */
      png = stbi_avif_write_png_to_memory(px_gray, 1, 1, 1, &png_len);
      if (png == NULL || png[0] != 137u)
      {
         printf("FAIL [png_roundtrip_gray]: got NULL or bad signature\n");
         ++failures;
      }
      else
      {
         printf("PASS [png_roundtrip_gray]: %d bytes\n", png_len);
         stbi_avif_image_free(png);
      }
   }

   /* --- PNG writer: write to file ------------------------------------- */
   {
      unsigned char px[4];
      const char *tmp = "/tmp/stb_avif_test_out.png";
      int ok_write;

      memset(px, 100, sizeof(px));
      ok_write = stbi_avif_write_png(tmp, px, 1, 1, 4);
      if (!ok_write)
      {
         printf("FAIL [png_write_file]: write returned 0\n");
         ++failures;
      }
      else
      {
         /* Verify file exists and has PNG signature */
         FILE *fp2 = fopen(tmp, "rb");
         if (fp2 == NULL)
         {
            printf("FAIL [png_write_file]: could not re-open written file\n");
            ++failures;
         }
         else
         {
            unsigned char sig[4];
            size_t nr = fread(sig, 1, 4, fp2);
            fclose(fp2);
            remove(tmp);
            if (nr == 4u && sig[0] == 137u && sig[1] == 80u)
               printf("PASS [png_write_file]\n");
            else
            {
               printf("FAIL [png_write_file]: bad signature bytes\n");
               ++failures;
            }
         }
      }
   }

   /* --- PNG write to NULL filename ------------------------------------ */
   {
      unsigned char px[4];
      memset(px, 0, sizeof(px));
      if (stbi_avif_write_png(NULL, px, 1, 1, 4) != 0)
      {
         printf("FAIL [png_write_null_filename]: expected 0\n");
         ++failures;
      }
      else
         printf("PASS [png_write_null_filename]\n");
   }

   printf("\n");
   if (failures == 0)
   {
      printf("==> All negative tests passed.\n");
      return 0;
   }
   printf("==> %d negative test(s) FAILED.\n", failures);
   return 1;
}
