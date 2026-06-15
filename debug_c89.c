/* debug C89 decoder - dump parsed values */
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
    FILE *outf;
    
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

    outf = fopen("debug_output.ppm", "wb");
    fprintf(outf, "P6\n%d %d\n255\n", w, h);
    fwrite(img, 1, (size_t)(w * h * c), outf);
    fclose(outf);

    /* Check corners and center */
    printf("Dimensions: %dx%d\n", w, h);
    printf("TL corner:  R=%d G=%d B=%d\n", img[0], img[1], img[2]);
    printf("TR corner:  R=%d G=%d B=%d\n", img[(w-1)*4], img[(w-1)*4+1], img[(w-1)*4+2]);
    printf("BL corner:  R=%d G=%d B=%d\n", img[(w*(h-1))*4], img[(w*(h-1))*4+1], img[(w*(h-1))*4+2]);
    printf("BR corner:  R=%d G=%d B=%d\n", img[(w*h-1)*4], img[(w*h-1)*4+1], img[(w*h-1)*4+2]);
    printf("Center:     R=%d G=%d B=%d\n", 
           img[(w/2 + w*(h/2))*4], 
           img[(w/2 + w*(h/2))*4+1],
           img[(w/2 + w*(h/2))*4+2]);

    /* Check how many unique pixel values there are */
    {
        int i, j;
        int unique = 0;
        unsigned char seen[256];
        memset(seen, 0, 256);
        for (i = 0; i < w * h * c; i += 4) {
            int v = img[i]; /* R channel */
            if (!seen[v]) { seen[v] = 1; unique++; }
        }
        printf("Unique R values: %d / 256\n", unique);
        
        /* Sample 20 evenly spaced pixels */
        printf("Samples: ");
        for (i = 0; i < 20; i++) {
            int idx = (i * w * h / 20) * 4;
            printf("%d,%d,%d ", img[idx], img[idx+1], img[idx+2]);
        }
        printf("\n");
    }

    stb_avif_free(img);
    free(data);
    return 0;
}