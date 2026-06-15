#!/bin/sh
cd "$(dirname "$0")"
cc -std=c89 -Wall -DSTB_AVIF_USE_C89_DAV1D -o test_c89_min test_c89_min.c -lm 2>/tmp/stb_compile_err.txt
echo "COMPILE_EXIT=$?" > /tmp/stb_result.txt
if [ -f test_c89_min ]; then
    ./test_c89_min example_avif/fox.profile0.8bpc.yuv420.avif
    echo "RUN_EXIT=$?" >> /tmp/stb_result.txt
    cat /tmp/stb_c89_result.txt >> /tmp/stb_result.txt 2>/dev/null
else
    echo "COMPILE_FAILED" >> /tmp/stb_result.txt
    cat /tmp/stb_compile_err.txt >> /tmp/stb_result.txt
fi
echo "ALL_DONE" >> /tmp/stb_result.txt
