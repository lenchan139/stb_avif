#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *filename = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f);
    fclose(f);

    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "decode failed\n"); return 1; }
    fprintf(stderr, "Decoded: %dx%d ch=%d\n", w, h, ch);

    /* Write PPM (3-channel RGB) */
    FILE *out = fopen("output_c89_rgb.ppm", "wb");
    fprintf(out, "P6\n%d %d\n255\n", w, h);
    fwrite(img, 1, (size_t)(w * h * 3), out);
    fclose(out);
    fprintf(stderr, "Written: output_c89_rgb.ppm\n");

    /* Read dav1d reference PPM */
    const char *dav_file = "output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm";
    FILE *fd = fopen(dav_file, "rb");
    if (!fd) { fprintf(stderr, "no dav1d ref\n"); return 1; }
    char hdr_buf[256];
    if (!fgets(hdr_buf, sizeof(hdr_buf), fd)) return 1;
    if (!fgets(hdr_buf, sizeof(hdr_buf), fd)) return 1;
    int dw, dh;
    sscanf(hdr_buf, "%d %d", &dw, &dh);
    if (!fgets(hdr_buf, sizeof(hdr_buf), fd)) return 1;
    unsigned char *dav_img = (unsigned char *)malloc((size_t)(dw * dh * 3));
    if (fread(dav_img, 1, (size_t)(dw * dh * 3), fd) != (size_t)(dw * dh * 3)) {
        fprintf(stderr, "short read\n"); return 1;
    }
    fclose(fd);
    fprintf(stderr, "DAV1D ref: %dx%d\n", dw, dh);

    /* Compare Y values (BT.601) */
    int x;
    int diff_y = 0, diff_all = 0;
    int max_y = 0, max_all = 0;
    long long ssd_y = 0, ssd_all = 0;
    for (x = 0; x < w * h; x++) {
        int cy = img[x*3];
        int dy = (77*dav_img[x*3] + 150*dav_img[x*3+1] + 29*dav_img[x*3+2] + 128) / 256;
        int d = cy - dy;
        if (d < 0) d = -d;
        if (d) diff_y++;
        if (d > max_y) max_y = d;
        ssd_y += d * d;
    }
    fprintf(stderr, "Y channel: %d/%d diff (%.1f%%), max=%d, ssd=%lld\n",
            diff_y, w*h, 100.0*diff_y/(w*h), max_y, ssd_y);

    /* Compare full RGB */
    for (x = 0; x < w * h * 3; x++) {
        int d = img[x] - dav_img[x];
        if (d < 0) d = -d;
        if (d) diff_all++;
        if (d > max_all) max_all = d;
        ssd_all += d * d;
    }
    fprintf(stderr, "RGB: %d/%d diff (%.1f%%), max=%d, ssd=%lld\n",
            diff_all, w*h*3, 100.0*diff_all/(w*h*3), max_all, ssd_all);

    if (diff_all == 0) {
        fprintf(stderr, "PERFECT MATCH\n");
    } else {
        fprintf(stderr, "First 32 C89 RGB: ");
        for (x = 0; x < 16; x++) fprintf(stderr, "%02x%02x%02x ", img[x*3], img[x*3+1], img[x*3+2]);
        fprintf(stderr, "\nFirst 32 DAV RGB: ");
        for (x = 0; x < 16; x++) fprintf(stderr, "%02x%02x%02x ", dav_img[x*3], dav_img[x*3+1], dav_img[x*3+2]);
        fprintf(stderr, "\n");
    }

    free(img);
    free(data);
    free(dav_img);
    return 0;
}
