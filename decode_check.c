#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>

int main(int argc, char** argv) {
    int x, y, c;
    const char* input = argv[1] ? argv[1] : "example_avif/fox.profile0.8bpc.yuv420.avif";
    
    printf("Decoding: %s\n", input);
    unsigned char* data = stbi_avif_load(input, &x, &y, &c, 3);
    
    if (!data) {
        printf("Failed: %s\n", stbi_failure_reason());
        return 1;
    }
    
    printf("Decoded: %dx%d, %d channels\n", x, y, c);
    
    /* Print sample pixels */
    printf("\nSample pixels (RGB):\n");
    printf("  Top-left (0,0):       [%3d, %3d, %3d]\n", data[0], data[1], data[2]);
    printf("  (100,50):             [%3d, %3d, %3d]\n", 
           data[(50*x + 100)*3], data[(50*x + 100)*3 + 1], data[(50*x + 100)*3 + 2]);
    printf("  (200,100):            [%3d, %3d, %3d]\n",
           data[(100*x + 200)*3], data[(100*x + 200)*3 + 1], data[(100*x + 200)*3 + 2]);
    printf("  Center (%d,%d):       [%3d, %3d, %3d]\n", x/2, y/2,
           data[((y/2)*x + x/2)*3], data[((y/2)*x + x/2)*3 + 1], data[((y/2)*x + x/2)*3 + 2]);
    
    /* Check for all-black or all-white */
    int black = 0, white = 0, colorful = 0;
    for (int i = 0; i < x*y*3; i += 100) {
        if (data[i] < 10 && data[i+1] < 10 && data[i+2] < 10) black++;
        else if (data[i] > 245 && data[i+1] > 245 && data[i+2] > 245) white++;
        else colorful++;
    }
    printf("\nPixel distribution (sampled every 100th):\n");
    printf("  Black-ish: %d, White-ish: %d, Colorful: %d\n", black, white, colorful);
    
    /* Write simple PPM */
    FILE* fp = fopen("/tmp/test_output.ppm", "wb");
    if (fp) {
        fprintf(fp, "P6\n%d %d\n255\n", x, y);
        fwrite(data, 1, x*y*3, fp);
        fclose(fp);
        printf("\nOutput written to: /tmp/test_output.ppm\n");
    }
    
    stbi_image_free(data);
    return 0;
}
