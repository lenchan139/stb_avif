#!/bin/sh
cd "$(dirname "$0")"
cc -w -std=c89 -DSTB_AVIF_USE_C89_DAV1D -o test_c89_min test_c89_min.c -lm 2>/dev/null
echo "COMPILED" > build.log
./test_c89_min 2>/dev/null
echo "RUN_DONE" >> build.log
cat stb_c89_result.txt >> build.log 2>/dev/null
echo "END" >> build.log
