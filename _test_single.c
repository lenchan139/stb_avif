#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void) {
    const char *fn = "example_avif/fox.profile0.8bpc.yuv420.avif";
    const char *ref_fn = "output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm";
    FILE *f = fopen(fn, "rb"); if (!f) { printf("NO_FILE\n"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { printf("DECODE_FAIL\n"); free(data); return 1; }
    FILE *fd = fopen(ref_fn, "rb"); if (!fd) { printf("NO_REF\n"); free(img); free(data); return 1; }
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    int rw, rh; sscanf(hdr, "%d %d", &rw, &rh); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(rw*rh*3));
    fread(ref, 1, (size_t)(rw*rh*3), fd); fclose(fd);
    if (w != rw || h != rh) { printf("SIZE_MISMATCH %dx%d vs %dx%d\n", w, h, rw, rh); free(img); free(data); free(ref); return 1; }
    int i; long long ssd = 0; int bad = 0, maxdiff = 0;
    for (i = 0; i < w * h * 3; i++) {
        int d = img[i] - ref[i]; if (d < 0) d = -d;
        if (d) bad++; if (d > maxdiff) maxdiff = d;
        ssd += (long long)d * d;
    }
    double psnr = ssd ? 10.0 * log10(255.0 * 255.0 * (w * h * 3) / (double)ssd) : 99.99;
    printf("PSNR=%.2f diff=%d %.1f%% maxdiff=%d w=%d h=%d\n", psnr, bad, 100.0*bad/(w*h*3), maxdiff, w, h);
    free(img); free(data); free(ref);
    return 0;
}
