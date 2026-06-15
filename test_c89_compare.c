#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 4);
    if (!img) { fprintf(stderr, "decode failed\n"); return 1; }
    fprintf(stderr, "Decoded: %dx%d ch=%d\n", w, h, ch);

    /* Read dav1d reference PPM */
    const char *dav_file = "output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm";
    FILE *fd = fopen(dav_file, "rb");
    if (!fd) { fprintf(stderr, "no dav1d ref\n"); return 1; }
    char hdr_buf[256];
    fgets(hdr_buf, sizeof(hdr_buf), fd);   /* skip P6 */
    fgets(hdr_buf, sizeof(hdr_buf), fd);   /* width height */
    int dw, dh;
    sscanf(hdr_buf, "%d %d", &dw, &dh);
    fgets(hdr_buf, sizeof(hdr_buf), fd);   /* skip maxval */
    unsigned char *dav_img = (unsigned char *)malloc((size_t)(dw * dh * 3));
    fread(dav_img, 1, (size_t)(dw * dh * 3), fd);
    fclose(fd);
    fprintf(stderr, "DAV1D ref: %dx%d\n", dw, dh);

    /* Compare first 32 Y values from C89 (R=G=B=Y for grey) vs dav1d */
    int x;
    fprintf(stderr, "C89 Y (first 32): ");
    for (x = 0; x < 32; x++) fprintf(stderr, "%d ", img[x*4]);
    fprintf(stderr, "\n");

    /* Convert dav1d RGB to Y (BT.601) */
    fprintf(stderr, "DAV Y (first 32): ");
    for (x = 0; x < 32; x++) {
        int r = dav_img[x*3];
        int g = dav_img[x*3+1];
        int b = dav_img[x*3+2];
        int y = (77*r + 150*g + 29*b + 128) / 256;
        fprintf(stderr, "%d ", y);
    }
    fprintf(stderr, "\n");

    /* Check correlation: find shift that minimizes diff */
    int best_shift = 0, best_diff = 999999999;
    int max_s = (w < 200) ? w : 200;
    for (int s = 0; s < max_s; s++) {
        int diff = 0;
        int n = (w*3 - s*3 < 1000) ? (w*3 - s*3)/3 : 300;
        if (n < 1) n = 1;
        for (x = 0; x < n; x++) {
            int cy = img[(x+s)*4];
            int r = dav_img[x*3], g = dav_img[x*3+1], b = dav_img[x*3+2];
            int dy = (77*r + 150*g + 29*b + 128) / 256;
            int d = cy - dy;
            if (d < 0) d = -d;
            diff += d;
        }
        if (diff < best_diff) { best_diff = diff; best_shift = s; }
    }
    fprintf(stderr, "Best shift: %d pixels (avg diff=%.1f)\n", best_shift, (double)best_diff/300.0);

    free(img);
    free(data);
    free(dav_img);
    return 0;
}
