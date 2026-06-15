#!/bin/sh
cd /Users/len/Downloads/stb_avif
cc -w -std=c89 -DSTB_AVIF_USE_C89_DAV1D -o test_c89_min test_c89_min.c -lm 1>/dev/null 2>/dev/null
echo "COMPILE_STATUS=$?"
