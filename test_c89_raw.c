#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(void) {
    const char *fn = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 4);
    if (!img) { fprintf(stderr, "FAIL: %s\n", stb_avif_failure_reason()); return 1; }
    fprintf(stderr, "C89: %dx%d ch=%d\n", w, h, ch);

    /* Read dav1d reference PPM (RGB) */
    FILE *fd = fopen("output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm", "rb");
    if (!fd) { fprintf(stderr, "no ref\n"); return 1; }
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    sscanf(hdr, "%d %d", &w, &h); fgets(hdr,256,fd);
    unsigned char *ref_rgb = (unsigned char *)malloc((size_t)(w*h*3));
    fread(ref_rgb, 1, (size_t)(w*h*3), fd); fclose(fd);

    /* Compare Y channel (luma): C89 RGBA[0] vs BT.601 Y from dav1d RGB */
    int i, y_diff = 0, y_max = 0, y_bad = 0;
    long long y_ssd = 0;
    for (i = 0; i < w * h; i++) {
        int cy = img[i*4 + 0]; /* C89: R = Y (since we decode to RGBA) */
        int dy = (77*ref_rgb[i*3] + 150*ref_rgb[i*3+1] + 29*ref_rgb[i*3+2] + 128) / 256;
        int d = cy - dy; if (d < 0) d = -d;
        if (d) y_bad++;
        if (d > y_max) y_max = d;
        y_ssd += d * d;
        /* Print first 48 Y values */
        if (i < 48) fprintf(stderr, "%3d%c", cy, i%12==11?'\n':' ');
    }
    fprintf(stderr, "\nC89 Y: %d/%d diff (%.1f%%), max=%d, MSE=%.1f\n",
        y_bad, w*h, 100.0*y_bad/(w*h), y_max, (double)y_ssd/(w*h));

    /* Compare dav1d Y (first 48) */
    for (i = 0; i < 48; i++) {
        int dy = (77*ref_rgb[i*3] + 150*ref_rgb[i*3+1] + 29*ref_rgb[i*3+2] + 128) / 256;
        fprintf(stderr, "%3d%c", dy, i%12==11?'\n':' ');
    }
    fprintf(stderr, "\nDAV Y: reference Y values above\n");

    free(img); free(data); free(ref_rgb);
    return y_max < 50 ? 0 : 1;
}
