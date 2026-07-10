#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const char *fn = "example_avif/red-at-12-oclock-with-color-profile-10bpc.avif";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "FAIL\n"); return 1; }
    printf("IMG[0..5]: %d %d %d %d %d %d\n", img[0], img[1], img[2], img[3], img[4], img[5]);
    /* Compare with ref */
    FILE *fd = fopen("output_ppm_dav1d/red-at-12-oclock-with-color-profile-10bpc.ppm", "rb");
    if (!fd) return 1;
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    int rw, rh; sscanf(hdr, "%d %d", &rw, &rh); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(rw*rh*3));
    fread(ref, 1, (size_t)(rw*rh*3), fd); fclose(fd);
    printf("REF[0..5]: %d %d %d %d %d %d\n", ref[0], ref[1], ref[2], ref[3], ref[4], ref[5]);
    int i; long long ssd = 0;
    for (i = 0; i < w*h*3; i++) { int d = img[i]-ref[i]; if(d<0)d=-d; ssd += (long long)d*d; }
    double psnr = 10.0 * log10(255.0*255.0*(rw*rh*3)/(double)ssd);
    printf("PSNR=%.2f dB\n", psnr);
    free(img); free(data); free(ref);
    return 0;
}
