# stb_avif

This repository starts a pure C89, libc-only AVIF decoder in a stb-style single-header form.

Current implementation status:

- Public single-header API exists in `stb_avif.h`
- File and memory loaders are implemented
- Core AVIF container metadata parsing is implemented for still-image structure discovery
- Width and height are reported through `ispe` when the primary item is associated correctly
- AV1 `av1C` validation, OBU walking, and reduced still-picture sequence-header validation are implemented
- Frame reconstruction and RGBA output are not implemented yet

Current v1 target subset:

- Static AVIF only
- 8-bit only
- No animation
- No film grain
- No embedded alpha support
- Pure C89
- No external dependencies except libc

Example:

```c
#define STB_AVIF_IMPLEMENTATION
#include "stb_avif.h"

int w, h, n;
unsigned char *rgba = stbi_avif_load("image.avif", &w, &h, &n, 4);
if (!rgba) {
    printf("error: %s\n", stbi_avif_failure_reason());
}
```

Build the current test harness:

```sh
cc -std=c89 -Wall -Wextra -pedantic tests/test_decode.c -o tests/test_decode
```

Expected behavior today:

- `stbi_avif_info()` can succeed on constrained files with the required metadata
- `stbi_avif_load()` validates the AVIF container and constrained AV1 headers, then fails with `AV1 headers parsed, but frame reconstruction is not implemented yet`

Next implementation milestones:

1. Deepen frame-header parsing beyond the current reduced still-picture validation path
2. Implement intra-only reconstruction for the constrained subset
3. Add fixed-point YUV to RGBA conversion for 4:2:0, 4:2:2, and 4:4:4