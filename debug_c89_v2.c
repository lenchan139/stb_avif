#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int debug_eob_total = 0, debug_coeff_total = 0, debug_coeff_nz = 0;
int debug_blocks = 0;

/* Override the reconstruct_block to count coefficients */
#undef stb_av1_reconstruct_block
static void stb_av1_reconstruct_block_debug(struct stb_av1_tile_context *tc,
    int *coeffs, int tx_w, int tx_h, int tx_type,
    unsigned char *pred, int pred_stride,
    unsigned char *dst, int dst_stride)
{
    int i, nz = 0;
    for (i = 0; i < tx_w * tx_h; i++)
        if (coeffs[i]) nz++;
    debug_coeff_total += tx_w * tx_h;
    debug_coeff_nz += nz;
    debug_blocks++;
}

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
    fprintf(stderr, "Decoding...\n");
    unsigned char *img = stb_avif_load_from_memory(data, (size_t)sz, &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "decode failed: %s\n", stb_avif_failure_reason()); return 1; }
    fprintf(stderr, "Done: %dx%d ch=%d\n", w, h, ch);
    fprintf(stderr, "Blocks: %d, Coeff total: %d, NZ: %d (%.2f%%)\n",
        debug_blocks, debug_coeff_total, debug_coeff_nz,
        debug_coeff_total ? 100.0 * debug_coeff_nz / debug_coeff_total : 0);

    /* Compare with dav1d reference */
    FILE *fd = fopen("output_ppm_dav1d/fox.profile0.8bpc.yuv420.ppm", "rb");
    if (!fd) { fprintf(stderr, "no dav1d ref\n"); return 1; }
    char hdr[256];
    if (!fgets(hdr, sizeof(hdr), fd) || !fgets(hdr, sizeof(hdr), fd)) return 1;
    int dw, dh;
    sscanf(hdr, "%d %d", &dw, &dh);
    fgets(hdr, sizeof(hdr), fd);
    unsigned char *dav = (unsigned char *)malloc((size_t)(dw * dh * 3));
    fread(dav, 1, (size_t)(dw * dh * 3), fd);
    fclose(fd);

    /* Compare first 64 RGB pixels */
    int x;
    fprintf(stderr, "\nC89 RGB (first 32): ");
    for (x = 0; x < 32; x++) fprintf(stderr, "%02x%02x%02x ", img[x*3], img[x*3+1], img[x*3+2]);
    fprintf(stderr, "\nDAV RGB (first 32): ");
    for (x = 0; x < 32; x++) fprintf(stderr, "%02x%02x%02x ", dav[x*3], dav[x*3+1], dav[x*3+2]);
    fprintf(stderr, "\n");

    /* Compute PSNR for first row */
    long long ssd = 0;
    int maxdiff = 0;
    for (x = 0; x < w * h * 3 && x < 1204*800*3; x++) {
        int d = img[x] - dav[x];
        if (d < 0) d = -d;
        ssd += d * d;
        if (d > maxdiff) maxdiff = d;
    }
    double mse = (double)ssd / (w * h * 3);
    double psnr = 10.0 * log10(255.0 * 255.0 / mse);
    fprintf(stderr, "\nMSE=%.2f PSNR=%.2fdB maxdiff=%d\n", mse, psnr, maxdiff);

    free(img);
    free(data);
    free(dav);
    return 0;
}
