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
    
    /* Find av1 data by parsing ISOBMFF manually */
    /* The AV1 data is in the mdat box */
    int i;
    for (i = 0; i < sz - 8; i++) {
        unsigned int box_sz = (data[i]<<24)|(data[i+1]<<16)|(data[i+2]<<8)|data[i+3];
        if (data[i+4]=='m' && data[i+5]=='d' && data[i+6]=='a' && data[i+7]=='t') {
            int header = (box_sz == 1) ? 16 : 8; /* large size or normal */
            unsigned char *av1 = data + i + header;
            int av1_sz = (box_sz == 1) ? 
                ((data[i+8]<<24)|(data[i+9]<<16)|(data[i+10]<<8)|data[i+11]) - header :
                box_sz - header;
            fprintf(stderr, "mdat at %d, av1_data at %d, size %d\n", i, i+header, av1_sz);
            fprintf(stderr, "First 16 bytes: ");
            int j;
            for (j = 0; j < 16 && j < av1_sz; j++)
                fprintf(stderr, "%02x ", av1[j]);
            fprintf(stderr, "\n");
            
            /* Parse OBUs */
            int pos = 0;
            while (pos < av1_sz - 1) {
                unsigned char obu_hdr = av1[pos];
                int obu_type = (obu_hdr >> 3) & 0xF;
                int has_size = (obu_hdr >> 1) & 1;
                char *type_name = "";
                switch (obu_type) {
                    case 0: type_name = "RESERVED"; break;
                    case 1: type_name = "SEQ_HDR"; break;
                    case 2: type_name = "TEMPORAL_DELIM"; break;
                    case 3: type_name = "FRAME_HDR"; break;
                    case 4: type_name = "TILE_GRP"; break;
                    case 5: type_name = "METADATA"; break;
                    case 6: type_name = "FRAME"; break;
                    case 7: type_name = "REDUNDANT_FRAME_HDR"; break;
                    case 8: type_name = "TILE_LIST"; break;
                    case 9: case 10: case 11: type_name = "RESERVED"; break;
                    case 12: type_name = "PADDING"; break;
                    case 13: type_name = "RESERVED"; break;
                    case 14: type_name = "RESERVED"; break;
                    case 15: type_name = "RESERVED"; break;
                }
                fprintf(stderr, "  OBU[%d]: type=%d(%s) hdr=0x%02x has_size=%d\n", 
                    pos, obu_type, type_name, obu_hdr, has_size);
                pos++; /* skip header byte */
                if (has_size) {
                    /* read LEB128 */
                    int sz_val = 0, shift = 0;
                    while (pos < av1_sz && (av1[pos-1] & 0x80)) {
                        sz_val |= (av1[pos] & 0x7f) << shift;
                        shift += 7;
                        if (av1[pos] & 0x80) pos++;
                    }
                    if (pos < av1_sz) {
                        sz_val |= (av1[pos] & 0x7f) << shift;
                        pos++; /* consume last LEB128 byte */
                    }
                    fprintf(stderr, "    size=%d (leb), data at pos=%d\n", sz_val, pos);
                    pos += sz_val;
                } else {
                    /* No size field - consume rest */
                    fprintf(stderr, "    no size field, consuming rest\n");
                    pos = av1_sz;
                }
                if (pos > av1_sz) pos = av1_sz;
                if (obu_type == 1 || obu_type == 3 || obu_type == 6) break;
            }
            break;
        }
    }
    free(data);
    return 0;
}
