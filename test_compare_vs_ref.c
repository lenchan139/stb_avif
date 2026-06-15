/* compare C89 decoder output vs reference PPM */
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *read_ppm(const char *path, int *w, int *h) {
    FILE *f = fopen(path, "rb");
    char header[32];
    int maxval;
    if (!f) return NULL;
    if (!fgets(header, sizeof(header), f)) { fclose(f); return NULL; }
    if (header[0] != 'P' || header[1] != '6') { fclose(f); return NULL; }
    if (!fscanf(f, "%d %d\n%d\n", w, h, &maxval)) { fclose(f); return NULL; }
    (void)maxval;
    {
        int size = (*w) * (*h) * 3;
        unsigned char *data = (unsigned char *)malloc((size_t)size);
        if (!data) { fclose(f); return NULL; }
        fread(data, 1, (size_t)size, f);
        fclose(f);
        return data;
    }
}

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
    const char *refs[] = {
        "output_ppm/fox.profile0.8bpc.yuv420.ppm",
        "output_ppm/fox.profile0.10bpc.yuv420.ppm",
        "output_ppm/kimono.ppm",
        "output_ppm/G-0trmKXsAA1sQZ-thumb.ppm",
        "output_ppm/G-0trmKXsAA1sQZ.ppm",
        "output_ppm/Gb5RU6RWoAAQQ1n.ppm",
        "output_ppm/red-at-12-oclock-with-color-profile-10bpc.ppm",
        "output_ppm/steam_2253100.ppm",
        NULL
    };
    int i;
    
    for (i = 0; files[i]; i++) {
        unsigned char *data;
        unsigned char *img_c89;
        unsigned char *img_ref;
        int w1, h1, c1, w2, h2;
        long len;
        FILE *f;
        int j, total, diff, maxdiff;
        int64_t ssd;
        
        f = fopen(files[i], "rb");
        if (!f) { printf("%-50s CANNOT OPEN\n", files[i]); continue; }
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = (unsigned char *)malloc((size_t)len);
        fread(data, 1, (size_t)len, f);
        fclose(f);

        img_c89 = stb_avif_load_from_memory(data, (int)len, &w1, &h1, &c1, 4);
        if (!img_c89) { printf("%-50s C89 FAIL: %s\n", files[i], stb_avif_failure_reason()); free(data); continue; }

        img_ref = read_ppm(refs[i], &w2, &h2);
        if (!img_ref) { printf("%-50s ref PPM not found\n", refs[i]); stb_avif_free(img_c89); free(data); continue; }

        if (w1 != w2 || h1 != h2) {
            printf("%-50s DIM MISMATCH: C89=%dx%d ref=%dx%d\n", files[i], w1, h1, w2, h2);
        } else {
            /* Convert RGBA to RGB for comparison */
            total = w1 * h1;
            diff = 0;
            maxdiff = 0;
            ssd = 0;
            for (j = 0; j < total; j++) {
                int r = img_c89[j*4+0];
                int g = img_c89[j*4+1];
                int b = img_c89[j*4+2];
                int r2 = img_ref[j*3+0];
                int g2 = img_ref[j*3+1];
                int b2 = img_ref[j*3+2];
                int d;
                d = r - r2; if (d < 0) d = -d; diff += (d > 0); if (d > maxdiff) maxdiff = d; ssd += (int64_t)d * d;
                d = g - g2; if (d < 0) d = -d; diff += (d > 0); if (d > maxdiff) maxdiff = d; ssd += (int64_t)d * d;
                d = b - b2; if (d < 0) d = -d; diff += (d > 0); if (d > maxdiff) maxdiff = d; ssd += (int64_t)d * d;
            }
            {
                double pct = 100.0 * diff / (total * 3);
                printf("%-50s %dx%d: %.1f%% diff, max=%d, ssd=%lld", files[i], w1, h1, pct, maxdiff, (long long)ssd);
                if (ssd == 0) printf(" PERFECT\n");
                else if (ssd < total * 4) printf(" VERY CLOSE\n");
                else if (ssd < total * 100) printf(" CLOSE\n");
                else if (ssd < total * 1000) printf(" SIMILAR\n");
                else printf(" BAD\n");
            }
        }

        free(img_ref);
        stb_avif_free(img_c89);
        free(data);
    }
    return 0;
}