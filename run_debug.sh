#!/bin/bash
# Run this in another terminal to compile and test

cd /Users/len/Downloads/gcp

cat > /tmp/debug_avif.c << 'EOFSRC'
#define STB_AVIF_IMPLEMENTATION
#define STBI_AVIF_DBG_BLOCKS
#include "/Users/len/Downloads/gcp/stb_avif.h"
#include <stdio.h>

int main(int argc, char** argv) {
    int x, y, c;
    FILE* fp = fopen("/tmp/dbg_levels.log", "w");
    if (!fp) { printf("Failed to open log\n"); return 1; }
    
    stbi_set_flip_vertically_on_load(0);
    stbi_set_dbg_blocks_fp(fp);
    
    printf("Loading AVIF...\n");
    unsigned char* data = stbi_avif_load(argv[1], &x, &y, &c, 3);
    if (data) {
        printf("Success: %dx%d\n", x, y);
        stbi_image_free(data);
    } else {
        printf("Failed: %s\n", stbi_failure_reason());
    }
    
    fclose(fp);
    printf("\nDebug log: /tmp/dbg_levels.log\n");
    return 0;
}
EOFSRC

echo "Compiling (this will take 3-5 minutes)..."
cc -O2 -o /tmp/debug_avif /tmp/debug_avif.c -lm -Wno-unused-function

echo "Running test..."
/tmp/debug_avif /Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif

echo ""
echo "=== ANALYZING DEBUG OUTPUT ==="
echo ""
echo "Levels initialization check:"
grep "LEVELS_INIT" /tmp/dbg_levels.log 2>/dev/null || echo "Not found"

echo ""
echo "Coefficient contexts at mi=(0,16):"
grep "COEFF_CTX_DBG" /tmp/dbg_levels.log | head -10 2>/dev/null || echo "Not found"

echo ""
echo "Levels array values at mi=(0,16):"
grep "LEVELS\[mi=(0,16)\]" /tmp/dbg_levels.log | head -10 2>/dev/null || echo "Not found"

echo ""
echo "Full debug log: /tmp/dbg_levels.log"
