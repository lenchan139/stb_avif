/* debug C89 decoder - check pixel values */
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
    unsigned char *img;
    int w, h, c;
    long len;
    FILE *f;
    
    f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    fread(data, 1, (size_t)len, f);
    fclose(f);

    img = stb_avif_load_from_memory(data, (int)len, &w, &h, &c, 4);
    if (!img) { printf("FAIL: %s\n", stb_avif_failure_reason()); return 1; }

    printf("Decoded: %dx%d ch=%d\n", w, h, c);

    /* Sample pixels */
    {
        int i;
        for (i = 0; i < 10; i++) {
            int idx = (i * w * h / 10) * 4;
            printf("pixel[%d]: R=%d G=%d B=%d\n", i, img[idx], img[idx+1], img[idx+2]);
        }
        
        /* Count unique values per channel */
        {
            int ch;
            for (ch = 0; ch < 3; ch++) {
                int min_v = 255, max_v = 0, unique = 0;
                unsigned char seen[256];
                int i;
                memset(seen, 0, 256);
                for (i = 0; i < w * h; i++) {
                    int v = img[i*4+ch];
                    if (v < min_v) min_v = v;
                    if (v > max_v) max_v = v;
                    if (!seen[v]) { seen[v] = 1; unique++; }
                }
                printf("Channel %c: min=%d max=%d unique=%d\n", "RGB"[ch], min_v, max_v, unique);
            }
        }
    }

    stb_avif_free(img);
    free(data);
    return 0;
}