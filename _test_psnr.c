#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void) {
    const char *fn = "/Users/len/Downloads/stb_avif/example_avif/G-0trmKXsAA1sQZ-thumb.avif";
    const char *ref_fn = "/Users/len/Downloads/stb_avif/output_ppm_dav1d/G-0trmKXsAA1sQZ-thumb.ppm";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    if (!data) return 1;
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stdout, "DECODE_FAIL"); free(data); return 1; }
    FILE *fd = fopen(ref_fn, "rb"); if (!fd) { free(img); free(data); return 1; }
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    sscanf(hdr, "%d %d", &w, &h); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(w*h*3));
    if (!ref) { free(img); free(data); fclose(fd); return 1; }
    fread(ref, 1, (size_t)(w*h*3), fd); fclose(fd);
    int i; long long ssd = 0; int bad = 0, maxdiff = 0;
    for (i = 0; i < w * h * 3; i++) {
        int d = img[i] - ref[i]; if (d < 0) d = -d;
        if (d) bad++;
        if (d > maxdiff) maxdiff = d;
        ssd += (long long)d * d;
    }
    double mse = (double)ssd / (w * h * 3);
    double psnr = 10.0 * log10(255.0 * 255.0 / mse);
    fprintf(stdout, "diff=%d (%.1f%%) maxdiff=%d PSNR=%.2f\n", bad, 100.0*bad/(w*h*3), maxdiff, psnr);
    free(img); free(data); free(ref);
    return bad > (w*h*3/2) ? 1 : 0;
}
