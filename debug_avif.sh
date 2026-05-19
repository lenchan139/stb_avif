#!/bin/bash
# AVIF Decoder Debug Workflow Script
# Usage: ./debug_avif.sh [command]

set -e
TEST_IMAGE="/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif"
DECODER_SRC="/Users/len/Downloads/gcp/stb_avif.h"
DUMP_RGB="/tmp/dump_rgb"

case "${1:-}" in
  build)
    echo "=== Building decoder ==="
    cat > /tmp/dump_rgb.c << 'EOF'
#define STB_AVIF_IMPLEMENTATION
#include "/Users/len/Downloads/gcp/stb_avif.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
   int w, h, ch;
   unsigned char *px = stbi_avif_load(argv[1], &w, &h, &ch, 3);
   if (!px) { fprintf(stderr, "FAIL: %s\n", stbi_avif_failure_reason()); return 1; }
   if (argc > 2) {
      fwrite(&w, 4, 1, stdout); fwrite(&h, 4, 1, stdout);
      fwrite(px, 3, (size_t)(w*h), stdout);
   }
   free(px);
   return 0;
}
EOF
    cc -O0 -g -o "$DUMP_RGB" /tmp/dump_rgb.c -Wno-unused-function -Wno-implicit-function-declaration
    echo "Built: $DUMP_RGB"
    ;;

  reference)
    echo "=== Generating reference output ==="
    python3 << 'EOF'
import av, numpy as np
from PIL import Image
c = av.open('/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif')
frame = next(c.decode(video=0))
frame.to_image().save('/tmp/ref.png')
print(f"Reference: {frame.width}x{frame.height}")
EOF
    ;;

  compare)
    echo "=== Comparing outputs ==="
    python3 << 'EOF'
import subprocess, struct, numpy as np
from PIL import Image

data = subprocess.check_output(['/tmp/dump_rgb', '/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif', 'output'])
w = struct.unpack_from('<i', data, 0)[0]
h = struct.unpack_from('<i', data, 4)[0]
rgb = np.frombuffer(data[8:], dtype=np.uint8).reshape(h, w, 3)
ref = np.array(Image.open('/tmp/ref.png').convert('RGB'))

diff = np.abs(rgb.astype(int) - ref.astype(int))
print(f"Dimensions: {w}x{h}")
print(f"Max error: {diff.max()} | Mean error: {diff.mean():.2f}")
print(f"Pixels with error > 50: {(diff.max(axis=2) > 50).sum()}")
print(f"\nError by 64-col bands:")
for c in range(0, w, 64):
    band = diff[:, c:min(c+64, w)]
    print(f"  cols {c:3d}-{min(c+63,w-1):3d}: max={band.max():3d} mean={band.mean():5.1f}")
EOF
    ;;

  visual)
    echo "=== Creating visual comparison ==="
    python3 << 'EOF'
import subprocess, struct, numpy as np
from PIL import Image

data = subprocess.check_output(['/tmp/dump_rgb', '/Users/len/Downloads/gcp/example_avif/fox.profile0.8bpc.yuv420.avif', 'output'])
w, h = struct.unpack_from('<ii', data, 0)
rgb = np.frombuffer(data[8:], dtype=np.uint8).reshape(h, w, 3)
ref = np.array(Image.open('/tmp/ref.png').convert('RGB'))

# Side-by-side at error boundary
crop = np.hstack([ref[:120, 50:300], rgb[:120, 50:300]])
Image.fromarray(crop).save('/tmp/compare.png')
print("Saved: /tmp/compare.png")
EOF
    open /tmp/compare.png 2>/dev/null || echo "View: /tmp/compare.png"
    ;;

  entropy)
    echo "=== Running entropy trace ==="
    rm -f /tmp/dbg.log
    STBI_AVIF_DBG_BLOCKS=/tmp/dbg.log "$DUMP_RGB" "$TEST_IMAGE" >/dev/null 2>&1 || true
    echo "Log saved: /tmp/dbg.log"
    echo "Lines: $(wc -l < /tmp/dbg.log)"
    echo "BLK entries: $(grep -c '^BLK' /tmp/dbg.log)"
    ;;

  analyze)
    echo "=== Analyzing entropy trace ==="
    if [ ! -f /tmp/dbg.log ]; then
      echo "Run: $0 entropy first"
      exit 1
    fi
    python3 << 'EOF'
import re

with open('/tmp/dbg.log') as f:
    lines = f.readlines()

# Find first significant error point
blk_re = re.compile(r'BLK mi=\((\d+),(\d+)\) bs=(\d+).*rd_bytes_left=(\d+) rd_rng=(\d+)')

print("First 10 blocks at mi_row=0:")
for line in lines[:200]:
    m = blk_re.match(line.strip())
    if m and m.group(1) == '0':
        row, col, bs, bytes_left, rng = m.groups()
        print(f"  mi=({row},{col}) bs={bs} bytes_left={bytes_left} rng={rng}")

# Check for anomalies
print("\nPartition decisions at mi=(0,16):")
for line in lines:
    if 'PART mi=(0,16)' in line:
        print(f"  {line.strip()}")
EOF
    ;;

  *)
    echo "AVIF Debug Workflow"
    echo ""
    echo "Commands:"
    echo "  build      - Build the decoder"
    echo "  reference  - Generate reference output with PyAV"
    echo "  compare    - Compare decoder output vs reference"
    echo "  visual     - Create visual comparison image"
    echo "  entropy    - Run with entropy tracing enabled"
    echo "  analyze    - Analyze entropy trace"
    echo ""
    echo "Typical workflow: build -> reference -> compare"
    ;;
esac
