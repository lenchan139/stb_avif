#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const char *fn = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    /* Access internal info before calling load */
    extern struct stb_avif_avif_info stb_avif_info;
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "FAIL\n"); return 1; }
    printf("IMG[0..5]: %d %d %d %d %d %d\n", img[0], img[1], img[2], img[3], img[4], img[5]);
    /* Compare with reference */
    FILE *fd = fopen("output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm", "rb");
    if (!fd) return 1;
    char hdr[256]; fgets(hdr,256,fd); fgets(hdr,256,fd);
    int rw, rh; sscanf(hdr, "%d %d", &rw, &rh); fgets(hdr,256,fd);
    unsigned char *ref = (unsigned char *)malloc((size_t)(rw*rh*3));
    fread(ref, 1, (size_t)(rw*rh*3), fd); fclose(fd);
    printf("REF[0..5]: %d %d %d %d %d %d\n", ref[0], ref[1], ref[2], ref[3], ref[4], ref[5]);
    /* Show some more positions */
    int pos[] = {0, 100, 500, 1000, 5000, 10000, 50000, 100000};
    int i;
    for (i = 0; i < 8; i++) {
        int p = pos[i];
        if (p < w*h) printf("pos %d: IMG=(%d,%d,%d) REF=(%d,%d,%d)\n",
            p, img[p*3], img[p*3+1], img[p*3+2],
            ref[p*3], ref[p*3+1], ref[p*3+2]);
    }
    free(img); free(data); free(ref);
    return 0;
}
