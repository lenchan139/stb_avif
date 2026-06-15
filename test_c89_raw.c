#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

#include <stdio.h>
#include <stdlib.h>

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

    /* first row Y from RGBA (R=G=B=Y) */
    int x;
    for (x = 0; x < 32 && x < w; x++)
        printf("%d ", img[x*4]);
    printf("\n");

    /* dump raw planes */
    /* The planes are stored as RGBA interleaved, but we can extract YUV if available */
    /* For now just check if first 100 pixels are all grey */
    int grey = 1;
    for (x = 0; x < w*h; x++)
        if (img[x*4] != img[x*4+1] || img[x*4+1] != img[x*4+2]) { grey = 0; break; }
    fprintf(stderr, "All grey: %s\n", grey ? "YES" : "NO");
    if (!grey) {
        /* find first non-grey pixel */
        for (x = 0; x < w*h; x++)
            if (img[x*4] != img[x*4+1] || img[x*4+1] != img[x*4+2])
                break;
        fprintf(stderr, "First non-grey at pixel %d: R=%d G=%d B=%d\n",
                x, img[x*4], img[x*4+1], img[x*4+2]);
    }

    /* Check unique values in R channel */
    int hist[256] = {0};
    for (x = 0; x < w*h; x++)
        if (img[x*4] < 256) hist[img[x*4]]++;
    int nuniq = 0;
    for (x = 0; x < 256; x++) if (hist[x]) nuniq++;
    fprintf(stderr, "Unique R values: %d\n", nuniq);

    free(img);
    free(data);
    return 0;
}
