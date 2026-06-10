/* C89 port: compatibility macros & functions from dav1d's common headers */
/* Include this to provide imin, imax, clz, ALIGN, case_set_* etc. */

#ifndef STB_AV1_COMMON_COMPAT_H
#define STB_AV1_COMMON_COMPAT_H

/* Math helpers (from intops.h) */
static int imin(const int a, const int b) { return a < b ? a : b; }
static int imax(const int a, const int b) { return a > b ? a : b; }
static int ulog2(const unsigned v) {
    int r = 0;
    unsigned tmp = v;
    while (tmp >>= 1) r++;
    return r;
}

/* ALIGN macros (from attributes.h) */
#define ALIGN(x, a) x
#define ALIGN_STK_16(type, name, sz) type name[sz]

/* case_set macros (from ctx.h) */
#ifndef STB_AV1_SET_CTX1
#include "dav1d_ref/ctx_c89.h"
#endif

/* memset_pow2 extern (declared in ctx_c89.h) */

#endif /* STB_AV1_COMMON_COMPAT_H */
