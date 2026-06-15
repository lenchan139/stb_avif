#!/bin/sh
cd /Users/len/Downloads/stb_avif
cc -w -std=c89 -DSTB_AVIF_USE_C89_DAV1D -o test_c89_min test_c89_min.c -lm 2>/dev/null 1>/dev/null
echo $? > doit_result.txt
if [ -f test_c89_min ]; then
  echo "BUILT" >> doit_result.txt
  ./test_c89_min 2>/dev/null 1>/dev/null
  echo "EXIT=$?" >> doit_result.txt
  cat stb_c89_result.txt >> doit_result.txt 2>/dev/null
else
  echo "BUILD_FAILED" >> doit_result.txt
fi
