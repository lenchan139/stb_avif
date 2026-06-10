/*
 * Copyright (c) 2018, VideoLAN and dav1d authors
 * All rights reserved.
 * C89 port: type-punning helper macros for block context.
 */

#ifndef STB_AV1_SRC_CTX_H
#define STB_AV1_SRC_CTX_H

/* C89 type-punning unions (no ATTR_ALIAS, no stdint.h) */
typedef union { unsigned long long u64; unsigned char u8[8]; } StbAv1Alias64;
typedef union { unsigned int u32; unsigned char u8[4]; } StbAv1Alias32;
typedef union { unsigned short u16; unsigned char u8[2]; } StbAv1Alias16;
typedef union { unsigned char u8; } StbAv1Alias8;

/* Memset function pointer type */
typedef void (*StbAv1MemsetPow2Fn)(void *ptr, int value);
extern const StbAv1MemsetPow2Fn stb_av1_memset_pow2[6];

/* For smaller sizes use multiplication to broadcast bytes. */
#define STB_AV1_SET_CTX1(var, off, val) \
    (((StbAv1Alias8 *) &(var)[off])->u8 = (unsigned char)((val) * 0x01))
#define STB_AV1_SET_CTX2(var, off, val) \
    (((StbAv1Alias16 *) &(var)[off])->u16 = (unsigned short)((val) * 0x0101))
#define STB_AV1_SET_CTX4(var, off, val) \
    (((StbAv1Alias32 *) &(var)[off])->u32 = (unsigned int)((val) * 0x01010101U))
#define STB_AV1_SET_CTX8(var, off, val) \
    (((StbAv1Alias64 *) &(var)[off])->u64 = (unsigned long long)((val) * 0x0101010101010101ULL))

#endif /* STB_AV1_SRC_CTX_H */
