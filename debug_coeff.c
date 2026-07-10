#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    /* Override the stb_av1_decode_block to add debug output */
    /* Use a simple approach: write a PPM and compare */
    const char *fn = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "FAIL\n"); return 1; }
    /* Save as PPM */
    FILE *out = fopen("output_c89_test.ppm", "wb");
    fprintf(out, "P6\n%d %d\n255\n", w, h);
    fwrite(img, 1, (size_t)(w*h*3), out);
    fclose(out);
    fprintf(stderr, "Wrote %dx%d PPM\n", w, h);
    free(img); free(data);
    return 0;
}
