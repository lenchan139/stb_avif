#!/bin/sh
cd "$(dirname "$0")"
cc -std=c89 -DSTB_AVIF_USE_C89_DAV1D -o test_c89_min test_c89_min.c -lm 2>/dev/null
echo "COMPILE_OK" > result.txt
./test_c89_min example_avif/fox.profile0.8bpc.yuv420.avif 2>/dev/null
echo "RUN_EXIT=$?" >> result.txt
cat stb_c89_result.txt >> result.txt 2>/dev/null
