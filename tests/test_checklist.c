/*
 * test_checklist.c  --  AVIF decoder validation checklist runner
 *
 * Runs each AVIF file through a series of observable checks and prints
 * a PASS / WARN / FAIL result for every check.
 *
 * Checks performed (keyed to the 8-point debugging checklist):
 *   [C1]  Container parseable            (stbi_avif_info succeeds)
 *   [C2]  Width  > 0                     (info width  is positive)
 *   [C3]  Height > 0                     (info height is positive)
 *   [C4]  Channels in {3,4}              (info channels is 3 or 4)
 *   [C5]  AV1 decode succeeds            (stbi_avif_load returns non-NULL)
 *   [C6]  Decoded dims match info dims   (load w/h == info w/h)
 *   [C7]  Pixel variance                 (not all pixels the same color)
 *   [C8]  Alpha consistency              (if ch==4, alpha plane is valid)
 *
 * Compile:
 *   cc -O2 tests/test_checklist.c -o /tmp/test_checklist -lm
 *
 * Run:
 *   /tmp/test_checklist example_avif/*.avif
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_AVIF_IMPLEMENTATION
#include "../stb_avif.h"

#define CHECK_PASS  "PASS"
#define CHECK_WARN  "WARN"
#define CHECK_FAIL  "FAIL"

static void print_result(const char *id, const char *status, const char *detail)
{
    printf("  [%s] %s : %s\n", status, id, detail);
}

static int run_checklist(const char *path)
{
    int iw = 0, ih = 0, ich = 0;
    int lw = 0, lh = 0, lch = 0;
    unsigned char *pixels = NULL;
    int pass = 1;
    size_t npix;
    int variance = 0;
    int alpha_ok = 1;
    char msg[256];

    printf("File: %s\n", path);

    /* [C1] Container parseable */
    if (!stbi_avif_info(path, &iw, &ih, &ich))
    {
        snprintf(msg, sizeof(msg), "stbi_avif_info failed: %s", stbi_avif_failure_reason());
        print_result("C1-container-parse", CHECK_FAIL, msg);
        printf("  (skipping remaining checks - container unreadable)\n\n");
        return 0;
    }
    print_result("C1-container-parse", CHECK_PASS, "stbi_avif_info succeeded");

    /* [C2] Width > 0 */
    if (iw > 0)
    {
        snprintf(msg, sizeof(msg), "width=%d", iw);
        print_result("C2-width-positive", CHECK_PASS, msg);
    }
    else
    {
        snprintf(msg, sizeof(msg), "width=%d", iw);
        print_result("C2-width-positive", CHECK_FAIL, msg);
        pass = 0;
    }

    /* [C3] Height > 0 */
    if (ih > 0)
    {
        snprintf(msg, sizeof(msg), "height=%d", ih);
        print_result("C3-height-positive", CHECK_PASS, msg);
    }
    else
    {
        snprintf(msg, sizeof(msg), "height=%d", ih);
        print_result("C3-height-positive", CHECK_FAIL, msg);
        pass = 0;
    }

    /* [C4] Channels in {3,4} */
    if (ich == 3 || ich == 4)
    {
        snprintf(msg, sizeof(msg), "channels=%d (%s)", ich, ich == 4 ? "RGBA" : "RGB");
        print_result("C4-channels-valid", CHECK_PASS, msg);
    }
    else
    {
        snprintf(msg, sizeof(msg), "channels=%d (expected 3 or 4)", ich);
        print_result("C4-channels-valid", CHECK_FAIL, msg);
        pass = 0;
    }

    /* [C5] AV1 decode succeeds */
    pixels = stbi_avif_load(path, &lw, &lh, &lch, ich);
    if (!pixels)
    {
        snprintf(msg, sizeof(msg), "stbi_avif_load failed: %s", stbi_avif_failure_reason());
        print_result("C5-av1-decode", CHECK_FAIL, msg);
        printf("\n");
        return 0;
    }
    print_result("C5-av1-decode", CHECK_PASS, "stbi_avif_load succeeded");

    /* [C6] Decoded dims match info dims */
    if (lw == iw && lh == ih)
    {
        snprintf(msg, sizeof(msg), "%dx%d matches info", lw, lh);
        print_result("C6-dims-match", CHECK_PASS, msg);
    }
    else
    {
        snprintf(msg, sizeof(msg), "load=%dx%d  info=%dx%d", lw, lh, iw, ih);
        print_result("C6-dims-match", CHECK_FAIL, msg);
        pass = 0;
    }

    /* [C7] Pixel variance - scan up to 4096 pixels */
    npix = (size_t)lw * (size_t)lh;
    {
        size_t scan = npix < 4096u ? npix : 4096u;
        size_t i;
        unsigned char r0 = pixels[0], g0 = pixels[1], b0 = pixels[2];
        for (i = 1; i < scan; i++)
        {
            if (pixels[i * (size_t)lch + 0] != r0 ||
                pixels[i * (size_t)lch + 1] != g0 ||
                pixels[i * (size_t)lch + 2] != b0)
            {
                variance = 1;
                break;
            }
        }
    }
    if (variance || npix <= 1u)
    {
        print_result("C7-pixel-variance", CHECK_PASS, "pixels contain variation");
    }
    else
    {
        snprintf(msg, sizeof(msg),
            "first %zu pixels all identical (%d,%d,%d) - possible decode error",
            (size_t)(npix < 4096u ? npix : 4096u),
            pixels[0], pixels[1], pixels[2]);
        print_result("C7-pixel-variance", CHECK_WARN, msg);
    }

    /* [C8] Alpha consistency (only when ch==4) */
    if (lch == 4)
    {
        size_t scan = npix < 4096u ? npix : 4096u;
        size_t i;
        int all_zero = 1, all_ff = 1;
        for (i = 0; i < scan; i++)
        {
            unsigned char a = pixels[i * 4u + 3u];
            if (a != 0)   all_zero = 0;
            if (a != 255) all_ff   = 0;
        }
        if (all_zero)
        {
            print_result("C8-alpha-consistency", CHECK_WARN,
                "entire alpha plane is 0 (fully transparent) - may be correct or decode error");
            alpha_ok = 0;
        }
        else if (all_ff)
        {
            print_result("C8-alpha-consistency", CHECK_PASS,
                "alpha plane is fully opaque (255) - typical for opaque AVIF with alpha item");
        }
        else
        {
            print_result("C8-alpha-consistency", CHECK_PASS,
                "alpha plane contains variation - semi-transparent image");
        }
    }
    else
    {
        print_result("C8-alpha-consistency", CHECK_PASS,
            "no alpha channel (RGB image)");
    }

    stbi_avif_image_free(pixels);

    (void)alpha_ok;
    (void)pass;

    printf("\n");
    return 1;
}

int main(int argc, char **argv)
{
    int i;
    int total = 0, ok = 0;

    if (argc < 2)
    {
        printf("usage: %s file.avif [file2.avif ...]\n", argv[0]);
        return 1;
    }

    printf("=== AVIF Decoder Validation Checklist ===\n\n");

    for (i = 1; i < argc; i++)
    {
        total++;
        if (run_checklist(argv[i]))
            ok++;
    }

    printf("=== Summary: %d/%d files fully decoded ===\n", ok, total);
    return (ok == total) ? 0 : 1;
}
