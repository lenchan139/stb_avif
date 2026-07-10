#include <stdio.h>
#include <stdlib.h>
/* Raw AV1 OBU scanner */
int main(void) {
    const char *fn = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(fn, "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    fread(data, 1, (size_t)sz, f); fclose(f);
    
    /* Find mdat */
    int av1_start = 0, av1_sz = 0;
    int i;
    for (i = 0; i < sz - 8; i++) {
        unsigned int box_sz = (data[i]<<24)|(data[i+1]<<16)|(data[i+2]<<8)|data[i+3];
        if (data[i+4]=='m' && data[i+5]=='d' && data[i+6]=='a' && data[i+7]=='t') {
            int header = (box_sz == 1) ? 16 : 8;
            av1_start = i + header;
            av1_sz = (box_sz == 1) ? ((int)((data[i+8]<<24)|(data[i+9]<<16)|(data[i+10]<<8)|data[i+11])) - header : (int)box_sz - header;
            fprintf(stderr, "av1_data at %d, size %d\n", av1_start, av1_sz);
            break;
        }
    }
    if (!av1_sz) return 1;
    
    unsigned char *av1 = data + av1_start;
    int pos = 0, obu_num = 0;
    while (pos < av1_sz - 1 && obu_num < 20) {
        unsigned char obu_hdr = av1[pos];
        int obu_type = (obu_hdr >> 3) & 0xF;
        int has_size = (obu_hdr >> 1) & 1;
        char *type_name = "";
        switch (obu_type) {
            case 1: type_name = "SEQ_HDR"; break;
            case 2: type_name = "TEMPORAL_DELIM"; break;
            case 3: type_name = "FRAME_HDR"; break;
            case 4: type_name = "TILE_GRP"; break;
            case 5: type_name = "METADATA"; break;
            case 6: type_name = "FRAME"; break;
            case 7: type_name = "REDUNDANT_FRAME_HDR"; break;
            case 12: type_name = "PADDING"; break;
        }
        pos++; /* skip header */
        int sz_val = 0;
        if (has_size) {
            int shift = 0, leb_bytes = 0;
            while (pos < av1_sz) {
                sz_val |= (av1[pos] & 0x7f) << shift;
                shift += 7; leb_bytes++;
                if (!(av1[pos++] & 0x80)) break;
            }
            fprintf(stderr, "OBU %d: type=%d(%s) pos_before=%d size=%d\n", obu_num, obu_type, type_name, pos-leb_bytes-1, sz_val);
            pos += sz_val;
        } else {
            fprintf(stderr, "OBU %d: type=%d(%s) pos_before=%d NO_SIZE\n", obu_num, obu_type, type_name, pos-1);
            break;
        }
        obu_num++;
    }
    free(data);
    return 0;
}
