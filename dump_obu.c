/* dump OBU structure of an AVIF file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *path = "example_avif/fox.profile0.8bpc.yuv420.avif";
    FILE *f = fopen(path, "rb");
    unsigned char *data;
    long len;
    int i;
    
    f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)len);
    fread(data, 1, (size_t)len, f);
    fclose(f);

    printf("File size: %ld bytes\n", len);
    
    /* Find mdat box */
    {
        int pos = 0;
        while (pos < len - 8) {
            unsigned int box_size = (data[pos]<<24)|(data[pos+1]<<16)|(data[pos+2]<<8)|data[pos+3];
            char box_type[5] = {data[pos+4], data[pos+5], data[pos+6], data[pos+7], 0};
            if (box_size == 0) box_size = len - pos;
            if (box_size < 8) box_size = 8;
            printf("Box '%s' at offset %d, size %u\n", box_type, pos, box_size);
            
            if (strcmp(box_type, "mdat") == 0) {
                /* Dump OBU structure */
                int obu_pos = pos + 8;
                printf("\n  OBU structure (from mdat offset %d):\n", obu_pos);
                while (obu_pos < pos + box_size - 1 && obu_pos < len) {
                    int obu_start = obu_pos;
                    unsigned char header_byte = data[obu_pos++];
                    int obu_type = (header_byte >> 3) & 0xf;
                    int has_size = (header_byte >> 1) & 1;
                    int ext_flag = header_byte & 1;
                    const char *type_name = "UNKNOWN";
                    if (obu_type == 1) type_name = "SEQUENCE_HEADER";
                    else if (obu_type == 2) type_name = "TEMPORAL_DELIMITER";
                    else if (obu_type == 3) type_name = "FRAME_HEADER";
                    else if (obu_type == 4) type_name = "TILE_GROUP";
                    else if (obu_type == 6) type_name = "FRAME";
                    else if (obu_type == 15) type_name = "PADDING";
                    printf("    OBU '%s' at byte %d", type_name, obu_start - (pos + 8));
                    if (has_size) {
                        int sz = 0;
                        int sz_bytes = data[obu_pos++];
                        while (sz_bytes > 0 && obu_pos < len) {
                            sz = (sz << 8) | data[obu_pos++];
                            sz_bytes--;
                        }
                        printf(", size=%d", sz);
                        /* Dump first few bytes of payload */
                        printf(", payload starts at byte %d", obu_pos - (pos + 8));
                        printf(", first 8 payload bytes: ");
                        for (i = 0; i < 8 && obu_pos + i < obu_start + box_size - 8 && obu_pos + i < len; i++)
                            printf("%02x ", data[obu_pos + i]);
                        printf("\n");
                        obu_pos = obu_start + 8 + sz;
                    } else {
                        printf(", no size field\n");
                        break;
                    }
                }
            }
            pos += (int)box_size;
        }
    }
    
    free(data);
    return 0;
}