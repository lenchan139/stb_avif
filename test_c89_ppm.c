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
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "FAIL: %s\n", stb_avif_failure_reason()); return 1; }
    fprintf(stderr, "C89 Decoded: %dx%d ch=%d\n", w, h, ch);

    /* Read dav1d reference */
    FILE *fd = fopen("output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm", "rb");
    if (!fd) { fprintf(stderr, "no ref\n"); return 1; }
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    sscanf(hdr, "%d %d", &w, &h); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(w*h*3));
    fread(ref, 1, (size_t)(w*h*3), fd); fclose(fd);

    /* Compare pixel-by-pixel */
    int x, y, i;
    long long ssd = 0;
    int bad = 0, diff1 = 0, maxdiff = 0;
    for (i = 0; i < w * h * 3; i++) {
        int d = img[i] - ref[i];
        if (d < 0) d = -d;
        if (d) { bad++; if (d == 1) diff1++; }
        if (d > maxdiff) maxdiff = d;
        ssd += (long long)d * d;
    }
    double mse = (double)ssd / (w * h * 3);
    double psnr = 10.0 * log10(255.0 * 255.0 / mse);
    fprintf(stderr, "Total pixels: %d, diff: %d (%.1f%%), maxdiff=%d, PSNR=%.2f\n",
            w*h*3, bad, 100.0*bad/(w*h*3), maxdiff, psnr);

    /* Check specific pixel positions */
    int check_pos[] = {0, 100, 1000, w*4, w*10, w*20, h/2*w, -1};
    for (i = 0; check_pos[i] >= 0 && check_pos[i] < w*h; i++) {
        int p = check_pos[i];
        fprintf(stderr, "  pos %d: C89=(%d,%d,%d) DAV=(%d,%d,%d)\n",
            p, img[p*3],img[p*3+1],img[p*3+2],
            ref[p*3],ref[p*3+1],ref[p*3+2]);
    }

    /* Write PPM for visual check */
    FILE *out = fopen("output_c89_test.ppm", "wb");
    fprintf(out, "P6\n%d %d\n255\n", w, h);
    fwrite(img, 1, (size_t)(w*h*3), out); fclose(out);
    fprintf(stderr, "Wrote output_c89_test.ppm\n");

    free(img); free(data); free(ref);
    return bad > (w*h*3/2) ? 1 : 0;
}
