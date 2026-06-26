#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Dump first block's coefficients for comparison */
int main(void)
{
    const char *filename = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f);
    fclose(f);

    /* Only decode, don't compare */
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 4);
    if (!img) { fprintf(stderr, "decode failed\n"); return 1; }

    /* Save raw YUV for analysis */
    FILE *fy = fopen("/tmp/c89_y.raw", "wb");
    if (fy) {
        for (int r = 0; r < h; r++)
            fwrite(img + r * w * 4, 1, w, fy);
        fclose(fy);
    }

    /* Save first block pixels */
    fprintf(stderr, "First block 16x16 Y pixels:\n");
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            fprintf(stderr, "%3d ", img[r * w * 4 + c * 4]);
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "\nImage size: %dx%d\n", w, h);

    /* Compare with dav1d reference PPM */
    const char *dav_file = "output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm";
    FILE *fd = fopen(dav_file, "rb");
    if (fd) {
        char hdr_buf[256];
        fgets(hdr_buf, sizeof(hdr_buf), fd);
        fgets(hdr_buf, sizeof(hdr_buf), fd);
        int dw, dh;
        sscanf(hdr_buf, "%d %d", &dw, &dh);
        fgets(hdr_buf, sizeof(hdr_buf), fd);
        unsigned char *dav_img = (unsigned char *)malloc((size_t)(dw * dh * 3));
        fread(dav_img, 1, (size_t)(dw * dh * 3), fd);
        fclose(fd);

        /* Compare first block Y from Dav1d (converted from RGB) */
        fprintf(stderr, "\nDav1d first block 16x16 Y pixels:\n");
        for (int r = 0; r < 16; r++) {
            for (int c = 0; c < 16; c++) {
                int rr = dav_img[r * dw * 3 + c * 3];
                int g = dav_img[r * dw * 3 + c * 3 + 1];
                int b = dav_img[r * dw * 3 + c * 3 + 2];
                int y = (77*rr + 150*g + 29*b + 128) / 256;
                fprintf(stderr, "%3d ", y);
            }
            fprintf(stderr, "\n");
        }

        /* Compute diff */
        int total_diff = 0, max_diff = 0;
        for (int r = 0; r < 800; r++)
            for (int c = 0; c < 1204; c++) {
                int rr = dav_img[r * dw * 3 + c * 3];
                int g = dav_img[r * dw * 3 + c * 3 + 1];
                int b = dav_img[r * dw * 3 + c * 3 + 2];
                int dy = (77*rr + 150*g + 29*b + 128) / 256;
                int cy = img[r * w * 4 + c * 4];
                int d = cy - dy;
                if (d < 0) d = -d;
                total_diff += d;
                if (d > max_diff) max_diff = d;
            }
        fprintf(stderr, "\nFull frame: avg diff=%.1f max diff=%d\n",
                (double)total_diff / (1204.0 * 800.0), max_diff);
        free(dav_img);
    }

    free(img);
    free(data);
    return 0;
}
