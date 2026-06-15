/* compare C89 decoder output vs dav1d */
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *path = argc > 1 ? argv[1] : "example_avif/fox.profile0.8bpc.yuv420.avif";
    unsigned char *data;
    unsigned char *img_c89, *img_dav1d;
    int w1, h1, c1, w2, h2, c2;
    long len;
    FILE *f;
    int i, total, diff, maxdiff;
    int64_t ssd;
    
    f = fopen(path, "rb");
    if (!f) { printf("cannot open\n"); return 1; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    fread(data, 1, (size_t)len, f);
    fclose(f);

    img_c89 = stb_avif_load_from_memory(data, (int)len, &w1, &h1, &c1, 4);
    if (!img_c89) { printf("C89 fail: %s\n", stb_avif_failure_reason()); free(data); return 1; }
    printf("C89: %dx%d ch=%d\n", w1, h1, c1);

    /* Decode with dav1d */
    {
        #define STB_AVIF_USE_DAV1D
        #undef STB_AVIF_IMPLEMENTATION
        #include "stb_avif.h"
        #define STB_AVIF_IMPLEMENTATION
        img_dav1d = stb_avif_load_from_memory(data, (int)len, &w2, &h2, &c2, 4);
        if (!img_dav1d) { printf("dav1d fail: %s\n", stb_avif_failure_reason()); stb_avif_free(img_c89); free(data); return 1; }
        printf("dav1d: %dx%d ch=%d\n", w2, h2, c2);
    }

    if (w1 != w2 || h1 != h2 || c1 != c2) {
        printf("MISMATCH dimensions\n");
    } else {
        total = w1 * h1 * c1;
        diff = 0;
        maxdiff = 0;
        ssd = 0;
        for (i = 0; i < total; i++) {
            int d = (int)img_c89[i] - (int)img_dav1d[i];
            if (d < 0) d = -d;
            if (d > 0) diff++;
            if (d > maxdiff) maxdiff = d;
            ssd += (int64_t)d * (int64_t)d;
        }
        printf("Pixels different: %d / %d (%.1f%%)\n", diff, total, 100.0 * diff / total);
        printf("Max diff: %d\n", maxdiff);
        printf("SSD: %lld\n", (long long)ssd);
        if (ssd == 0) printf("PERFECT MATCH!\n");
        else if (ssd < total * 4) printf("VERY CLOSE\n");
        else if (ssd < total * 100) printf("CLOSE\n");
        else if (ssd < total * 1000) printf("SOMEWHAT SIMILAR\n");
        else printf("VERY DIFFERENT\n");
    }

    stb_avif_free(img_c89);
    stb_avif_free(img_dav1d);
    free(data);
    return 0;
}