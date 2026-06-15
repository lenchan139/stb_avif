/* compare pixel values between C89 and dav1d */
#define STB_AVIF_USE_DAV1D
#define STB_AVIF_IMPLEMENTATION
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

    printf("Dimensions: %dx%d\n", w, h);
    printf("TL corner:  R=%d G=%d B=%d\n", img[0], img[1], img[2]);
    printf("TR corner:  R=%d G=%d B=%d\n", img[(w-1)*4], img[(w-1)*4+1], img[(w-1)*4+2]);
    printf("Center:     R=%d G=%d B=%d\n", 
           img[(w/2 + w*(h/2))*4], 
           img[(w/2 + w*(h/2))*4+1],
           img[(w/2 + w*(h/2))*4+2]);

    /* Sample 20 evenly spaced pixels */
    printf("Samples: ");
    {
        int i;
        for (i = 0; i < 20; i++) {
            int idx = (i * w * h / 20) * 4;
            printf("%d,%d,%d ", img[idx], img[idx+1], img[idx+2]);
        }
    }
    printf("\n");

    stb_avif_free(img);
    free(data);
    return 0;
}