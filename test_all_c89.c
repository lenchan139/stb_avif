/* test all avif files with C89 decoder */
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *files[] = {
        "example_avif/fox.profile0.8bpc.yuv420.avif",
        "example_avif/fox.profile0.10bpc.yuv420.avif",
        "example_avif/kimono.avif",
        "example_avif/G-0trmKXsAA1sQZ-thumb.avif",
        "example_avif/G-0trmKXsAA1sQZ.avif",
        "example_avif/Gb5RU6RWoAAQQ1n.avif",
        "example_avif/red-at-12-oclock-with-color-profile-10bpc.avif",
        "example_avif/steam_2253100.avif",
        NULL
    };
    int i;
    
    for (i = 0; files[i]; i++) {
        const char *path = files[i];
        unsigned char *data;
        unsigned char *img;
        int w, h, c;
        long len;
        FILE *f;
        
        f = fopen(path, "rb");
        if (!f) { printf("%-60s CANNOT OPEN\n", path); continue; }
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = (unsigned char *)malloc((size_t)len);
        if (!data) { fclose(f); printf("%-60s OOM\n", path); continue; }
        fread(data, 1, (size_t)len, f);
        fclose(f);

        img = stb_avif_load_from_memory(data, (int)len, &w, &h, &c, 4);
        if (!img) {
            printf("%-60s FAIL: %s\n", path, stb_avif_failure_reason());
        } else {
            printf("%-60s OK %dx%d ch=%d\n", path, w, h, c);
            stb_avif_free(img);
        }
        free(data);
    }
    return 0;
}