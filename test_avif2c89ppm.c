/* test_avif2c89ppm.c - decode AVIF files to PPM using C89 internal decoder */
/* Compile: cc -std=c89 -o test_avif2c89ppm test_avif2c89ppm.c -lm */

#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int decode_to_ppm(const char *avif_path, const char *ppm_path, int req_channels)
{
    unsigned char *data;
    unsigned char *img;
    int w, h, c;
    long len;
    FILE *f;
    FILE *out;
    size_t bytes_read;
    int row;

    f = fopen(avif_path, "rb");
    if (!f) { fprintf(stderr, "  FAIL: Cannot open %s\n", avif_path); return 0; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    if (!data) { fclose(f); fprintf(stderr, "  FAIL: Out of memory\n"); return 0; }
    bytes_read = fread(data, 1, (size_t)len, f); fclose(f);
    if ((long)bytes_read != len) { fprintf(stderr, "  FAIL: Read error\n"); free(data); return 0; }

    img = stb_avif_load_from_memory(data, (int)len, &w, &h, &c, req_channels);
    if (!img) {
        fprintf(stderr, "  FAIL: %s - %s\n", avif_path, stb_avif_failure_reason());
        free(data); return 0;
    }

    out = fopen(ppm_path, "wb");
    if (!out) { fprintf(stderr, "  FAIL: Cannot write %s\n", ppm_path); stb_avif_free(img); free(data); return 0; }

    if (c == 1) {
        fprintf(out, "P5\n%d %d\n255\n", w, h);
        for (row = 0; row < h; row++)
            fwrite(img + row * w, 1, (size_t)(w), out);
    } else {
        fprintf(out, "P6\n%d %d\n255\n", w, h);
        for (row = 0; row < h; row++) {
            int col;
            for (col = 0; col < w; col++) {
                unsigned char *pix = img + (row * w + col) * c;
                fwrite(pix, 1, 3, out);
            }
        }
    }

    fclose(out);
    printf("  OK: %dx%d, %d chan -> %s\n", w, h, c, ppm_path);
    stb_avif_free(img); free(data);
    return 1;
}

void stb_avif_set_msac_trace(int on);
int main(int argc, char *argv[])
{
    int pass = 0, fail = 0, i;

    stb_avif_set_msac_trace(getenv("STB_TRACE") != NULL);
    const char *files[] = {
        "example_avif/fox.profile0.8bpc.yuv420.avif",
        "example_avif/fox.profile0.10bpc.yuv420.avif",
        "example_avif/kimono.avif",
        "example_avif/G-0trmKXsAA1sQZ-thumb.avif",
        "example_avif/G-0trmKXsAA1sQZ.avif",
        "example_avif/Gb5RU6RWoAAQQ1n.avif",
        "example_avif/red-at-12-oclock-with-color-profile-10bpc.avif",
        "example_avif/steam_2253100.avif"
    };
    int num;

    if (argc > 1) num = argc - 1;
    else num = (int)(sizeof(files) / sizeof(files[0]));

    printf("stb_avif C89 decoder -> PPM\n");
    printf("============================\n\n");
    system("mkdir -p output_ppm_c89");

    for (i = 0; i < num; i++) {
        const char *src;
        char dst[512];
        const char *base, *slash;

        printf("[%d/%d] ", i + 1, num);
        fflush(stdout);

        src = (argc > 1) ? argv[i + 1] : files[i];
        base = src;
        slash = strrchr(src, '/');
        if (slash) base = slash + 1;

        strcpy(dst, "output_ppm_c89/");
        strncat(dst, base, sizeof(dst) - 20);
        {
            char *dot = strrchr(dst, '.');
            if (dot) strcpy(dot, ".ppm");
            else strcat(dst, ".ppm");
        }

        if (decode_to_ppm(src, dst, 4)) pass++;
        else fail++;
    }

    printf("\n============================\n");
    printf("Results: %d passed, %d failed out of %d\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
