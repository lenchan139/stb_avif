/* dump dav1d decoder output to PPM for comparison */
#define STB_AVIF_USE_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *inpath = "example_avif/fox.profile0.8bpc.yuv420.avif";
    const char *outpath = "output_dav1d_ref.ppm";
    unsigned char *data;
    unsigned char *img;
    int w, h, c;
    long len;
    FILE *f;
    int i;
    FILE *out;
    
    f = fopen(inpath, "rb");
    if (!f) { printf("cannot open\n"); return 1; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    fread(data, 1, (size_t)len, f);
    fclose(f);

    img = stb_avif_load_from_memory(data, (int)len, &w, &h, &c, 4);
    if (!img) { printf("fail: %s\n", stb_avif_failure_reason()); free(data); return 1; }
    printf("Decoded: %dx%d ch=%d\n", w, h, c);

    out = fopen(outpath, "wb");
    if (!out) { printf("cannot write\n"); stb_avif_free(img); free(data); return 1; }
    fprintf(out, "P6\n%d %d\n255\n", w, h);
    fwrite(img, 1, (size_t)(w * h * c), out);
    fclose(out);
    printf("Written: %s\n", outpath);

    /* Sample some pixels */
    for (i = 0; i < 20; i++) {
        int idx = i * (w * h * c / 20);
        printf("  pixel[%d]: R=%d G=%d B=%d A=%d\n",
               i,
               img[idx + 0],
               img[idx + 1],
               img[idx + 2],
               img[idx + 3]);
    }

    stb_avif_free(img);
    free(data);
    return 0;
}