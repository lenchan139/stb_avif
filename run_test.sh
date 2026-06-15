#!/bin/sh
cd "$(dirname "$0")"
./test_c89_decoder example_avif/fox.profile0.8bpc.yuv420.avif 2>&1 | tee /tmp/stb_test_out.txt
echo "EXITCODE=$?"
