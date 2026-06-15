/* minimal C89 test - print key decoded values */
#define STB_AVIF_IMPLEMENTATION
#define STB_AVIF_USE_C89_DAV1D
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *path = "example_avif/fox.profile0.8bpc.yuv420.avif";
    unsigned char *data;
    int w, h, c;
    long len;
    FILE *f;
    FILE *outf;

    f = fopen(path, "rb");
    if (!f) { printf("FAIL cannot open\n"); return 1; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    fread(data, 1, (size_t)len, f);
    fclose(f);

    outf = fopen("stb_c89_result.txt", "w");
    if (!outf) { free(data); printf("FAIL\n"); return 1; }

    unsigned char *img = stb_avif_load_from_memory(data, (int)len, &w, &h, &c, 4);
    if (!img) {
        fprintf(outf, "FAIL decode: %s\n", stb_avif_failure_reason());
        fclose(outf);
        free(data);
        return 1;
    }

    fprintf(outf, "OK %dx%d ch=%d\n", w, h, c);

    /* Sample some pixels */
    {
        int samples[5] = {0, w*h/4, w*h/2, 3*w*h/4, w*h-1};
        int i;
        for (i = 0; i < 5; i++) {
            int idx = samples[i] * 4;
            fprintf(outf, "pixel[%d]: R=%d G=%d B=%d A=%d\n",
                   samples[i], img[idx], img[idx+1], img[idx+2], img[idx+3]);
        }
    }

    fclose(outf);
    stb_avif_free(img);
    free(data);
    return 0;
}