/* c89_trace.c - decode AVIF with C89 decoder, dump msac trace to stderr.
 * Usage: c89_trace <file.avif> */
#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <file.avif>\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);

    stb_avif_set_msac_trace(1);

    int w, h, ch;
    unsigned char *img = stb_avif_load_from_memory(data, (int)sz, &w, &h, &ch, 4);
    if (!img) { fprintf(stderr, "[C89] DECODE FAIL: %s\n", stb_avif_failure_reason()); return 1; }
    fprintf(stderr, "[C89] OK %dx%d ch=%d\n", w, h, ch);
    stb_avif_free(img);
    free(data);
    return 0;
}
