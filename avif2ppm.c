/* avif2ppm.c - decode a single AVIF file to PPM using C89 internal decoder */
/* Compile: cc -std=c89 -DSTB_AVIF_USE_C89_DAV1D -o avif2ppm avif2ppm.c -lm */
/* Usage:   ./avif2ppm input.avif output.ppm [channels] */

#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *inpath, *outpath;
    unsigned char *data, *img;
    int w, h, c, req;
    long len;
    FILE *f, *out;
    size_t n;
    int row;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.avif output.ppm [channels]\n", argv[0]);
        return 1;
    }
    inpath = argv[1];
    outpath = argv[2];
    req = (argc > 3) ? atoi(argv[3]) : 3;

    f = fopen(inpath, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", inpath); return 1; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    if (!data) { fclose(f); fprintf(stderr, "Out of memory\n"); return 1; }
    n = fread(data, 1, (size_t)len, f); fclose(f);
    if ((long)n != len) { fprintf(stderr, "Read error\n"); free(data); return 1; }

    img = stb_avif_load_from_memory(data, (int)len, &w, &h, &c, req);
    if (!img) {
        fprintf(stderr, "Decode failed: %s\n", stb_avif_failure_reason());
        free(data); return 1;
    }

    out = fopen(outpath, "wb");
    if (!out) {
        fprintf(stderr, "Cannot write %s\n", outpath);
        stb_avif_free(img); free(data); return 1;
    }

    if (c == 1) {
        fprintf(out, "P5\n%d %d\n255\n", w, h);
        for (row = 0; row < h; row++)
            fwrite(img + row * w, 1, (size_t)w, out);
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
    fprintf(stderr, "Decoded %s: %dx%d %d chan -> %s\n", inpath, w, h, c, outpath);
    stb_avif_free(img); free(data);
    return 0;
}
