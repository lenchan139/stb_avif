#define STB_AVIF_USE_C89_DAV1D
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const char *fn = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    
    /* Find av1 data - look for mdat box */
    int i;
    for (i = 0; i < sz - 4; i++) {
        if (data[i] == 'm' && data[i+1] == 'd' && data[i+2] == 'a' && data[i+3] == 't') {
            unsigned char *av1 = data + i + 8; /* skip mdat header */
            int av1_sz = sz - i - 8;
            /* Parse first few bytes to see AV1 OBU headers */
            fprintf(stderr, "Found mdat at offset %d, size %d\n", i, av1_sz);
            fprintf(stderr, "First bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                av1[0], av1[1], av1[2], av1[3], av1[4], av1[5], av1[6], av1[7]);
            /* Parse first OBU header */
            int obu_type = (av1[0] >> 3) & 0xF;
            int has_size = (av1[0] >> 1) & 1;
            fprintf(stderr, "First OBU: type=%d has_size=%d\n", obu_type, has_size);
            if (has_size) {
                /* Read LEB128 size */
                int sz_val = 0, shift = 0, pos = 1;
                while (pos < 10) {
                    sz_val |= (av1[pos] & 0x7f) << shift;
                    if (!(av1[pos] & 0x80)) break;
                    shift += 7; pos++;
                }
                fprintf(stderr, "OBU size=%d (leb128 bytes=%d)\n", sz_val, pos+1);
                /* Next OBU */
                int next = 1 + pos;
                obu_type = (av1[next] >> 3) & 0xF;
                has_size = (av1[next] >> 1) & 1;
                fprintf(stderr, "Second OBU: type=%d has_size=%d at offset %d\n", obu_type, has_size, next);
            }
            break;
        }
    }
    free(data);
    return 0;
}
