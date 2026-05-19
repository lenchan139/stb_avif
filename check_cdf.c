#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int w, h, ch;
    
    // Set debug output
    setenv("STBI_AVIF_DBG_BLOCKS", "/tmp/cdf_check.log", 1);
    
    unsigned char *px = stbi_avif_load(argv[1], &w, &h, &ch, 3);
    if (!px) {
        fprintf(stderr, "FAIL: %s\n", stbi_avif_failure_reason());
        return 1;
    }
    
    // Check the log for CDF corruption
    FILE *fp = fopen("/tmp/cdf_check.log", "r");
    if (fp) {
        char line[1024];
        int found_16 = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "BLK mi=(0,16)")) {
                found_16 = 1;
                printf("Found BLK mi=(0,16)\n");
            }
            if (found_16 && strstr(line, "coeff_base_cdf")) {
                printf("CDF at mi=(0,16): %s", line);
                break;
            }
        }
        fclose(fp);
    }
    
    free(px);
    return 0;
}
