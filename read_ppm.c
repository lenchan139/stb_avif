/* read PPM and print specific pixels */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    const char *path = "output_ppm/fox.profile0.8bpc.yuv420.ppm";
    FILE *f = fopen(path, "rb");
    char header[32];
    int w, h, maxval;
    int i;
    
    if (!f) { printf("cannot open %s\n", path); return 1; }
    fgets(header, sizeof(header), f); /* P6 */
    fscanf(f, "%d %d\n%d\n", &w, &h, &maxval);
    printf("Size: %dx%d\n", w, h);
    
    /* Skip to specific positions */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    fread(header, 1, 17, f); /* header size */
    
    /* Read 20 evenly spaced pixels */
    printf("Samples: ");
    for (i = 0; i < 20; i++) {
        long pos = 17 + (i * w * h / 20) * 3;
        unsigned char rgb[3];
        fseek(f, pos, SEEK_SET);
        fread(rgb, 1, 3, f);
        printf("%d,%d,%d ", rgb[0], rgb[1], rgb[2]);
    }
    printf("\n");
    
    /* Corners */
    {
        unsigned char rgb[3];
        fseek(f, 17, SEEK_SET); /* TL */
        fread(rgb, 1, 3, f);
        printf("TL corner: R=%d G=%d B=%d\n", rgb[0], rgb[1], rgb[2]);
        
        fseek(f, 17 + (w-1) * 3, SEEK_SET); /* TR */
        fread(rgb, 1, 3, f);
        printf("TR corner: R=%d G=%d B=%d\n", rgb[0], rgb[1], rgb[2]);
        
        fseek(f, 17 + (w * (h-1)) * 3, SEEK_SET); /* BL */
        fread(rgb, 1, 3, f);
        printf("BL corner: R=%d G=%d B=%d\n", rgb[0], rgb[1], rgb[2]);
        
        fseek(f, 17 + (w * h - 1) * 3, SEEK_SET); /* BR */
        fread(rgb, 1, 3, f);
        printf("BR corner: R=%d G=%d B=%d\n", rgb[0], rgb[1], rgb[2]);
        
        fseek(f, 17 + (w/2 + w*(h/2)) * 3, SEEK_SET); /* center */
        fread(rgb, 1, 3, f);
        printf("Center: R=%d G=%d B=%d\n", rgb[0], rgb[1], rgb[2]);
    }
    
    fclose(f);
    return 0;
}