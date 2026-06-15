/* check C89 output pixel values in detail */
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
    int i;
    
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

    /* Check color variance */
    {
        int r_min=255,r_max=0,g_min=255,g_max=0,b_min=255,b_max=0;
        int unique_r=0,unique_g=0,unique_b=0;
        unsigned char seen_r[256],seen_g[256],seen_b[256];
        memset(seen_r,0,256); memset(seen_g,0,256); memset(seen_b,0,256);
        for (i = 0; i < w * h; i++) {
            int r = img[i*4], g = img[i*4+1], b = img[i*4+2];
            if (r < r_min) r_min = r; if (r > r_max) r_max = r;
            if (g < g_min) g_min = g; if (g > g_max) g_max = g;
            if (b < b_min) b_min = b; if (b > b_max) b_max = b;
            if (!seen_r[r]) { seen_r[r] = 1; unique_r++; }
            if (!seen_g[g]) { seen_g[g] = 1; unique_g++; }
            if (!seen_b[b]) { seen_b[b] = 1; unique_b++; }
        }
        printf("C89 Output:\n");
        printf("  R range: %d-%d (%d unique)\n", r_min, r_max, unique_r);
        printf("  G range: %d-%d (%d unique)\n", g_min, g_max, unique_g);
        printf("  B range: %d-%d (%d unique)\n", b_min, b_max, unique_b);
        printf("  Some pixels with nonzero B:\n");
        {
            int count = 0;
            for (i = 0; i < w * h && count < 10; i++) {
                if (img[i*4+2] > 10) {
                    printf("    pixel %d: R=%d G=%d B=%d\n", i, img[i*4], img[i*4+1], img[i*4+2]);
                    count++;
                }
            }
            if (count == 0) printf("    (none found)\n");
        }
    }

    stb_avif_free(img);
    free(data);
    return 0;
}