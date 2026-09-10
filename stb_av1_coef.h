/*
 * stb_av1_coef.h - scalar AV1 coefficient decoder
 *
 * Coefficient syntax derived from dav1d 1.5.4 src/recon_tmpl.c.
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef STB_AV1_COEF_H
#define STB_AV1_COEF_H

#include <string.h>

#ifndef STBV_U16_DEFINED
#error "stb_av1_coef.h requires stbv_u16"
#endif
#ifndef STBV_U32_DEFINED
#error "stb_av1_coef.h requires stbv_u32"
#endif
#ifndef STBV_I32_DEFINED
#error "stb_av1_coef.h requires stbv_i32"
#endif

/* tx_class: 0 = 2D, 1 = horizontal, 2 = vertical. */

static unsigned stbv_av1_coef_hi_tok(struct stb_av1_msac *s, stbv_u16 *cdf)
{
    unsigned t;
    unsigned v;
    t = stb_av1_msac_symbol(s, cdf, 3);
    v = 3 + t;
    if (t == 3) {
        t = stb_av1_msac_symbol(s, cdf, 3);
        v = 6 + t;
        if (t == 3) {
            t = stb_av1_msac_symbol(s, cdf, 3);
            v = 9 + t;
            if (t == 3)
                v = 12 + stb_av1_msac_symbol(s, cdf, 3);
        }
    }
    return v;
}

static unsigned stbv_av1_coef_golomb(struct stb_av1_msac *s)
{
    unsigned len = 0;
    unsigned v = 1;
    while (!stb_av1_msac_bool_equi(s) && len < 32U)
        len++;
    while (len--)
        v = (v << 1) | stb_av1_msac_bool_equi(s);
    return v - 1U;
}

/* dav1d_lo_ctx_offsets[3][5][5]: 0 = square, 1 = w > h, 2 = w < h.  Indexed
 * with dav1d's nonsquare + (tx & nonsquare), nonsquare = tx >= RTX_4X8. */
static const unsigned char stbv_av1_lo_ctx_offsets[3][5][5] = {
    {
        {  0,  1,  6,  6, 21 },
        {  1,  6,  6, 21, 21 },
        {  6,  6, 21, 21, 21 },
        {  6, 21, 21, 21, 21 },
        { 21, 21, 21, 21, 21 }
    }, {
        {  0, 16,  6,  6, 21 },
        { 16, 16,  6, 21, 21 },
        { 16, 16, 21, 21, 21 },
        { 16, 16, 21, 21, 21 },
        { 16, 16, 21, 21, 21 }
    }, {
        {  0, 11, 11, 11, 11 },
        { 11, 11, 11, 11, 11 },
        {  6,  6, 21, 21, 21 },
        {  6, 21, 21, 21, 21 },
        { 21, 21, 21, 21, 21 }
    }
};

static unsigned stbv_av1_coef_lo_ctx(const unsigned char *levels,
                                      unsigned *hi_mag,
                                      unsigned x, unsigned y,
                                      unsigned stride, int tx_class,
                                      const unsigned char (*ctx_offsets)[5])
{
    unsigned mag = levels[stride + 0] + levels[1];
    unsigned off;
    if (tx_class == 0) {
        mag += levels[stride + 1];
        *hi_mag = mag;
        mag += levels[2 * stride] + levels[2];
        off = ctx_offsets[y > 4 ? 4 : y][x > 4 ? 4 : x];
    } else {
        mag += levels[2];
        *hi_mag = mag;
        mag += levels[3] + levels[4];
        off = 26U + (y > 1U ? 10U : y * 5U);
    }
    return off + (mag > 512U ? 4U : (mag + 64U) >> 7);
}

static const stbv_u16 stbv_av1_scan_4x4[16] = {
    0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15
};

static const stbv_u16 stbv_av1_scan_8x8[64] = {
    0, 8, 1, 2, 9, 16, 24, 17, 10, 3, 4, 11, 18, 25, 32, 40,
    33, 26, 19, 12, 5, 6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35,
    28, 21, 14, 7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30,
    23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63
};

static const stbv_u16 stbv_av1_scan_16x16[256] = {
    0, 16, 1, 2, 17, 32, 48, 33, 18, 3, 4, 19, 34, 49, 64, 80,
    65, 50, 35, 20, 5, 6, 21, 36, 51, 66, 81, 96, 112, 97, 82, 67,
    52, 37, 22, 7, 8, 23, 38, 53, 68, 83, 98, 113, 128, 144, 129, 114,
    99, 84, 69, 54, 39, 24, 9, 10, 25, 40, 55, 70, 85, 100, 115, 130,
    145, 160, 176, 161, 146, 131, 116, 101, 86, 71, 56, 41, 26, 11, 12, 27,
    42, 57, 72, 87, 102, 117, 132, 147, 162, 177, 192, 208, 193, 178, 163, 148,
    133, 118, 103, 88, 73, 58, 43, 28, 13, 14, 29, 44, 59, 74, 89, 104,
    119, 134, 149, 164, 179, 194, 209, 224, 240, 225, 210, 195, 180, 165, 150, 135,
    120, 105, 90, 75, 60, 45, 30, 15, 31, 46, 61, 76, 91, 106, 121, 136,
    151, 166, 181, 196, 211, 226, 241, 242, 227, 212, 197, 182, 167, 152, 137, 122,
    107, 92, 77, 62, 47, 63, 78, 93, 108, 123, 138, 153, 168, 183, 198, 213,
    228, 243, 244, 229, 214, 199, 184, 169, 154, 139, 124, 109, 94, 79, 95, 110,
    125, 140, 155, 170, 185, 200, 215, 230, 245, 246, 231, 216, 201, 186, 171, 156,
    141, 126, 111, 127, 142, 157, 172, 187, 202, 217, 232, 247, 248, 233, 218, 203,
    188, 173, 158, 143, 159, 174, 189, 204, 219, 234, 249, 250, 235, 220, 205, 190,
    175, 191, 206, 221, 236, 251, 252, 237, 222, 207, 223, 238, 253, 254, 239, 255
};

static const stbv_u16 stbv_av1_scan_32x32[1024] = {
    0, 32, 1, 2, 33, 64, 96, 65, 34, 3, 4, 35, 66, 97, 128, 160,
    129, 98, 67, 36, 5, 6, 37, 68, 99, 130, 161, 192, 224, 193, 162, 131,
    100, 69, 38, 7, 8, 39, 70, 101, 132, 163, 194, 225, 256, 288, 257, 226,
    195, 164, 133, 102, 71, 40, 9, 10, 41, 72, 103, 134, 165, 196, 227, 258,
    289, 320, 352, 321, 290, 259, 228, 197, 166, 135, 104, 73, 42, 11, 12, 43,
    74, 105, 136, 167, 198, 229, 260, 291, 322, 353, 384, 416, 385, 354, 323, 292,
    261, 230, 199, 168, 137, 106, 75, 44, 13, 14, 45, 76, 107, 138, 169, 200,
    231, 262, 293, 324, 355, 386, 417, 448, 480, 449, 418, 387, 356, 325, 294, 263,
    232, 201, 170, 139, 108, 77, 46, 15, 16, 47, 78, 109, 140, 171, 202, 233,
    264, 295, 326, 357, 388, 419, 450, 481, 512, 544, 513, 482, 451, 420, 389, 358,
    327, 296, 265, 234, 203, 172, 141, 110, 79, 48, 17, 18, 49, 80, 111, 142,
    173, 204, 235, 266, 297, 328, 359, 390, 421, 452, 483, 514, 545, 576, 608, 577,
    546, 515, 484, 453, 422, 391, 360, 329, 298, 267, 236, 205, 174, 143, 112, 81,
    50, 19, 20, 51, 82, 113, 144, 175, 206, 237, 268, 299, 330, 361, 392, 423,
    454, 485, 516, 547, 578, 609, 640, 672, 641, 610, 579, 548, 517, 486, 455, 424,
    393, 362, 331, 300, 269, 238, 207, 176, 145, 114, 83, 52, 21, 22, 53, 84,
    115, 146, 177, 208, 239, 270, 301, 332, 363, 394, 425, 456, 487, 518, 549, 580,
    611, 642, 673, 704, 736, 705, 674, 643, 612, 581, 550, 519, 488, 457, 426, 395,
    364, 333, 302, 271, 240, 209, 178, 147, 116, 85, 54, 23, 24, 55, 86, 117,
    148, 179, 210, 241, 272, 303, 334, 365, 396, 427, 458, 489, 520, 551, 582, 613,
    644, 675, 706, 737, 768, 800, 769, 738, 707, 676, 645, 614, 583, 552, 521, 490,
    459, 428, 397, 366, 335, 304, 273, 242, 211, 180, 149, 118, 87, 56, 25, 26,
    57, 88, 119, 150, 181, 212, 243, 274, 305, 336, 367, 398, 429, 460, 491, 522,
    553, 584, 615, 646, 677, 708, 739, 770, 801, 832, 864, 833, 802, 771, 740, 709,
    678, 647, 616, 585, 554, 523, 492, 461, 430, 399, 368, 337, 306, 275, 244, 213,
    182, 151, 120, 89, 58, 27, 28, 59, 90, 121, 152, 183, 214, 245, 276, 307,
    338, 369, 400, 431, 462, 493, 524, 555, 586, 617, 648, 679, 710, 741, 772, 803,
    834, 865, 896, 928, 897, 866, 835, 804, 773, 742, 711, 680, 649, 618, 587, 556,
    525, 494, 463, 432, 401, 370, 339, 308, 277, 246, 215, 184, 153, 122, 91, 60,
    29, 30, 61, 92, 123, 154, 185, 216, 247, 278, 309, 340, 371, 402, 433, 464,
    495, 526, 557, 588, 619, 650, 681, 712, 743, 774, 805, 836, 867, 898, 929, 960,
    992, 961, 930, 899, 868, 837, 806, 775, 744, 713, 682, 651, 620, 589, 558, 527,
    496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124, 93, 62, 31,
    63, 94, 125, 156, 187, 218, 249, 280, 311, 342, 373, 404, 435, 466, 497, 528,
    559, 590, 621, 652, 683, 714, 745, 776, 807, 838, 869, 900, 931, 962, 993, 994,
    963, 932, 901, 870, 839, 808, 777, 746, 715, 684, 653, 622, 591, 560, 529, 498,
    467, 436, 405, 374, 343, 312, 281, 250, 219, 188, 157, 126, 95, 127, 158, 189,
    220, 251, 282, 313, 344, 375, 406, 437, 468, 499, 530, 561, 592, 623, 654, 685,
    716, 747, 778, 809, 840, 871, 902, 933, 964, 995, 996, 965, 934, 903, 872, 841,
    810, 779, 748, 717, 686, 655, 624, 593, 562, 531, 500, 469, 438, 407, 376, 345,
    314, 283, 252, 221, 190, 159, 191, 222, 253, 284, 315, 346, 377, 408, 439, 470,
    501, 532, 563, 594, 625, 656, 687, 718, 749, 780, 811, 842, 873, 904, 935, 966,
    997, 998, 967, 936, 905, 874, 843, 812, 781, 750, 719, 688, 657, 626, 595, 564,
    533, 502, 471, 440, 409, 378, 347, 316, 285, 254, 223, 255, 286, 317, 348, 379,
    410, 441, 472, 503, 534, 565, 596, 627, 658, 689, 720, 751, 782, 813, 844, 875,
    906, 937, 968, 999, 1000, 969, 938, 907, 876, 845, 814, 783, 752, 721, 690, 659,
    628, 597, 566, 535, 504, 473, 442, 411, 380, 349, 318, 287, 319, 350, 381, 412,
    443, 474, 505, 536, 567, 598, 629, 660, 691, 722, 753, 784, 815, 846, 877, 908,
    939, 970, 1001, 1002, 971, 940, 909, 878, 847, 816, 785, 754, 723, 692, 661, 630,
    599, 568, 537, 506, 475, 444, 413, 382, 351, 383, 414, 445, 476, 507, 538, 569,
    600, 631, 662, 693, 724, 755, 786, 817, 848, 879, 910, 941, 972, 1003, 1004, 973,
    942, 911, 880, 849, 818, 787, 756, 725, 694, 663, 632, 601, 570, 539, 508, 477,
    446, 415, 447, 478, 509, 540, 571, 602, 633, 664, 695, 726, 757, 788, 819, 850,
    881, 912, 943, 974, 1005, 1006, 975, 944, 913, 882, 851, 820, 789, 758, 727, 696,
    665, 634, 603, 572, 541, 510, 479, 511, 542, 573, 604, 635, 666, 697, 728, 759,
    790, 821, 852, 883, 914, 945, 976, 1007, 1008, 977, 946, 915, 884, 853, 822, 791,
    760, 729, 698, 667, 636, 605, 574, 543, 575, 606, 637, 668, 699, 730, 761, 792,
    823, 854, 885, 916, 947, 978, 1009, 1010, 979, 948, 917, 886, 855, 824, 793, 762,
    731, 700, 669, 638, 607, 639, 670, 701, 732, 763, 794, 825, 856, 887, 918, 949,
    980, 1011, 1012, 981, 950, 919, 888, 857, 826, 795, 764, 733, 702, 671, 703, 734,
    765, 796, 827, 858, 889, 920, 951, 982, 1013, 1014, 983, 952, 921, 890, 859, 828,
    797, 766, 735, 767, 798, 829, 860, 891, 922, 953, 984, 1015, 1016, 985, 954, 923,
    892, 861, 830, 799, 831, 862, 893, 924, 955, 986, 1017, 1018, 987, 956, 925, 894,
    863, 895, 926, 957, 988, 1019, 1020, 989, 958, 927, 959, 990, 1021, 1022, 991, 1023
};

static const stbv_u16 stbv_av1_scan_4x8[32] = {
    0, 8, 1, 16, 9, 2, 24, 17, 10, 3, 25, 18, 11, 4, 26, 19, 12, 5, 27, 20, 13, 6, 28, 21, 
    14, 7, 29, 22, 15, 30, 23, 31
};

static const stbv_u16 stbv_av1_scan_8x4[32] = {
    0, 1, 4, 2, 5, 8, 3, 6, 9, 12, 7, 10, 13, 16, 11, 14, 17, 20, 15, 18, 21, 24, 19, 22, 25, 
    28, 23, 26, 29, 27, 30, 31
};

static const stbv_u16 stbv_av1_scan_8x16[128] = {
    0, 16, 1, 32, 17, 2, 48, 33, 18, 3, 64, 49, 34, 19, 4, 80, 65, 50, 35, 20, 5, 96, 81, 66, 
    51, 36, 21, 6, 112, 97, 82, 67, 52, 37, 22, 7, 113, 98, 83, 68, 53, 38, 23, 8, 114, 99, 
    84, 69, 54, 39, 24, 9, 115, 100, 85, 70, 55, 40, 25, 10, 116, 101, 86, 71, 56, 41, 26, 
    11, 117, 102, 87, 72, 57, 42, 27, 12, 118, 103, 88, 73, 58, 43, 28, 13, 119, 104, 89, 74, 
    59, 44, 29, 14, 120, 105, 90, 75, 60, 45, 30, 15, 121, 106, 91, 76, 61, 46, 31, 122, 107, 
    92, 77, 62, 47, 123, 108, 93, 78, 63, 124, 109, 94, 79, 125, 110, 95, 126, 111, 127
};

static const stbv_u16 stbv_av1_scan_16x8[128] = {
    0, 1, 8, 2, 9, 16, 3, 10, 17, 24, 4, 11, 18, 25, 32, 5, 12, 19, 26, 33, 40, 6, 13, 20, 
    27, 34, 41, 48, 7, 14, 21, 28, 35, 42, 49, 56, 15, 22, 29, 36, 43, 50, 57, 64, 23, 30, 
    37, 44, 51, 58, 65, 72, 31, 38, 45, 52, 59, 66, 73, 80, 39, 46, 53, 60, 67, 74, 81, 88, 
    47, 54, 61, 68, 75, 82, 89, 96, 55, 62, 69, 76, 83, 90, 97, 104, 63, 70, 77, 84, 91, 98, 
    105, 112, 71, 78, 85, 92, 99, 106, 113, 120, 79, 86, 93, 100, 107, 114, 121, 87, 94, 101, 
    108, 115, 122, 95, 102, 109, 116, 123, 103, 110, 117, 124, 111, 118, 125, 119, 126, 127
};

static const stbv_u16 stbv_av1_scan_16x32[512] = {
    0, 32, 1, 64, 33, 2, 96, 65, 34, 3, 128, 97, 66, 35, 4, 160, 129, 98, 67, 36, 5, 192, 161, 
    130, 99, 68, 37, 6, 224, 193, 162, 131, 100, 69, 38, 7, 256, 225, 194, 163, 132, 101, 70, 
    39, 8, 288, 257, 226, 195, 164, 133, 102, 71, 40, 9, 320, 289, 258, 227, 196, 165, 134, 
    103, 72, 41, 10, 352, 321, 290, 259, 228, 197, 166, 135, 104, 73, 42, 11, 384, 353, 322, 
    291, 260, 229, 198, 167, 136, 105, 74, 43, 12, 416, 385, 354, 323, 292, 261, 230, 199, 
    168, 137, 106, 75, 44, 13, 448, 417, 386, 355, 324, 293, 262, 231, 200, 169, 138, 107, 
    76, 45, 14, 480, 449, 418, 387, 356, 325, 294, 263, 232, 201, 170, 139, 108, 77, 46, 15, 
    481, 450, 419, 388, 357, 326, 295, 264, 233, 202, 171, 140, 109, 78, 47, 16, 482, 451, 
    420, 389, 358, 327, 296, 265, 234, 203, 172, 141, 110, 79, 48, 17, 483, 452, 421, 390, 
    359, 328, 297, 266, 235, 204, 173, 142, 111, 80, 49, 18, 484, 453, 422, 391, 360, 329, 
    298, 267, 236, 205, 174, 143, 112, 81, 50, 19, 485, 454, 423, 392, 361, 330, 299, 268, 
    237, 206, 175, 144, 113, 82, 51, 20, 486, 455, 424, 393, 362, 331, 300, 269, 238, 207, 
    176, 145, 114, 83, 52, 21, 487, 456, 425, 394, 363, 332, 301, 270, 239, 208, 177, 146, 
    115, 84, 53, 22, 488, 457, 426, 395, 364, 333, 302, 271, 240, 209, 178, 147, 116, 85, 54, 
    23, 489, 458, 427, 396, 365, 334, 303, 272, 241, 210, 179, 148, 117, 86, 55, 24, 490, 459, 
    428, 397, 366, 335, 304, 273, 242, 211, 180, 149, 118, 87, 56, 25, 491, 460, 429, 398, 
    367, 336, 305, 274, 243, 212, 181, 150, 119, 88, 57, 26, 492, 461, 430, 399, 368, 337, 
    306, 275, 244, 213, 182, 151, 120, 89, 58, 27, 493, 462, 431, 400, 369, 338, 307, 276, 
    245, 214, 183, 152, 121, 90, 59, 28, 494, 463, 432, 401, 370, 339, 308, 277, 246, 215, 
    184, 153, 122, 91, 60, 29, 495, 464, 433, 402, 371, 340, 309, 278, 247, 216, 185, 154, 
    123, 92, 61, 30, 496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124, 93, 62, 
    31, 497, 466, 435, 404, 373, 342, 311, 280, 249, 218, 187, 156, 125, 94, 63, 498, 467, 
    436, 405, 374, 343, 312, 281, 250, 219, 188, 157, 126, 95, 499, 468, 437, 406, 375, 344, 
    313, 282, 251, 220, 189, 158, 127, 500, 469, 438, 407, 376, 345, 314, 283, 252, 221, 190, 
    159, 501, 470, 439, 408, 377, 346, 315, 284, 253, 222, 191, 502, 471, 440, 409, 378, 347, 
    316, 285, 254, 223, 503, 472, 441, 410, 379, 348, 317, 286, 255, 504, 473, 442, 411, 380, 
    349, 318, 287, 505, 474, 443, 412, 381, 350, 319, 506, 475, 444, 413, 382, 351, 507, 476, 
    445, 414, 383, 508, 477, 446, 415, 509, 478, 447, 510, 479, 511
};

static const stbv_u16 stbv_av1_scan_32x16[512] = {
    0, 1, 16, 2, 17, 32, 3, 18, 33, 48, 4, 19, 34, 49, 64, 5, 20, 35, 50, 65, 80, 6, 21, 36, 
    51, 66, 81, 96, 7, 22, 37, 52, 67, 82, 97, 112, 8, 23, 38, 53, 68, 83, 98, 113, 128, 9, 
    24, 39, 54, 69, 84, 99, 114, 129, 144, 10, 25, 40, 55, 70, 85, 100, 115, 130, 145, 160, 
    11, 26, 41, 56, 71, 86, 101, 116, 131, 146, 161, 176, 12, 27, 42, 57, 72, 87, 102, 117, 
    132, 147, 162, 177, 192, 13, 28, 43, 58, 73, 88, 103, 118, 133, 148, 163, 178, 193, 208, 
    14, 29, 44, 59, 74, 89, 104, 119, 134, 149, 164, 179, 194, 209, 224, 15, 30, 45, 60, 75, 
    90, 105, 120, 135, 150, 165, 180, 195, 210, 225, 240, 31, 46, 61, 76, 91, 106, 121, 136, 
    151, 166, 181, 196, 211, 226, 241, 256, 47, 62, 77, 92, 107, 122, 137, 152, 167, 182, 197, 
    212, 227, 242, 257, 272, 63, 78, 93, 108, 123, 138, 153, 168, 183, 198, 213, 228, 243, 
    258, 273, 288, 79, 94, 109, 124, 139, 154, 169, 184, 199, 214, 229, 244, 259, 274, 289, 
    304, 95, 110, 125, 140, 155, 170, 185, 200, 215, 230, 245, 260, 275, 290, 305, 320, 111, 
    126, 141, 156, 171, 186, 201, 216, 231, 246, 261, 276, 291, 306, 321, 336, 127, 142, 157, 
    172, 187, 202, 217, 232, 247, 262, 277, 292, 307, 322, 337, 352, 143, 158, 173, 188, 203, 
    218, 233, 248, 263, 278, 293, 308, 323, 338, 353, 368, 159, 174, 189, 204, 219, 234, 249, 
    264, 279, 294, 309, 324, 339, 354, 369, 384, 175, 190, 205, 220, 235, 250, 265, 280, 295, 
    310, 325, 340, 355, 370, 385, 400, 191, 206, 221, 236, 251, 266, 281, 296, 311, 326, 341, 
    356, 371, 386, 401, 416, 207, 222, 237, 252, 267, 282, 297, 312, 327, 342, 357, 372, 387, 
    402, 417, 432, 223, 238, 253, 268, 283, 298, 313, 328, 343, 358, 373, 388, 403, 418, 433, 
    448, 239, 254, 269, 284, 299, 314, 329, 344, 359, 374, 389, 404, 419, 434, 449, 464, 255, 
    270, 285, 300, 315, 330, 345, 360, 375, 390, 405, 420, 435, 450, 465, 480, 271, 286, 301, 
    316, 331, 346, 361, 376, 391, 406, 421, 436, 451, 466, 481, 496, 287, 302, 317, 332, 347, 
    362, 377, 392, 407, 422, 437, 452, 467, 482, 497, 303, 318, 333, 348, 363, 378, 393, 408, 
    423, 438, 453, 468, 483, 498, 319, 334, 349, 364, 379, 394, 409, 424, 439, 454, 469, 484, 
    499, 335, 350, 365, 380, 395, 410, 425, 440, 455, 470, 485, 500, 351, 366, 381, 396, 411, 
    426, 441, 456, 471, 486, 501, 367, 382, 397, 412, 427, 442, 457, 472, 487, 502, 383, 398, 
    413, 428, 443, 458, 473, 488, 503, 399, 414, 429, 444, 459, 474, 489, 504, 415, 430, 445, 
    460, 475, 490, 505, 431, 446, 461, 476, 491, 506, 447, 462, 477, 492, 507, 463, 478, 493, 
    508, 479, 494, 509, 495, 510, 511
};

static const stbv_u16 stbv_av1_scan_4x16[64] = {
    0, 16, 1, 32, 17, 2, 48, 33, 18, 3, 49, 34, 19, 4, 50, 35, 20, 5, 51, 36, 21, 6, 52, 37, 
    22, 7, 53, 38, 23, 8, 54, 39, 24, 9, 55, 40, 25, 10, 56, 41, 26, 11, 57, 42, 27, 12, 58, 
    43, 28, 13, 59, 44, 29, 14, 60, 45, 30, 15, 61, 46, 31, 62, 47, 63
};

static const stbv_u16 stbv_av1_scan_16x4[64] = {
    0, 1, 4, 2, 5, 8, 3, 6, 9, 12, 7, 10, 13, 16, 11, 14, 17, 20, 15, 18, 21, 24, 19, 22, 25, 
    28, 23, 26, 29, 32, 27, 30, 33, 36, 31, 34, 37, 40, 35, 38, 41, 44, 39, 42, 45, 48, 43, 
    46, 49, 52, 47, 50, 53, 56, 51, 54, 57, 60, 55, 58, 61, 59, 62, 63
};

static const stbv_u16 stbv_av1_scan_8x32[256] = {
    0, 32, 1, 64, 33, 2, 96, 65, 34, 3, 128, 97, 66, 35, 4, 160, 129, 98, 67, 36, 5, 192, 161, 
    130, 99, 68, 37, 6, 224, 193, 162, 131, 100, 69, 38, 7, 225, 194, 163, 132, 101, 70, 39, 
    8, 226, 195, 164, 133, 102, 71, 40, 9, 227, 196, 165, 134, 103, 72, 41, 10, 228, 197, 166, 
    135, 104, 73, 42, 11, 229, 198, 167, 136, 105, 74, 43, 12, 230, 199, 168, 137, 106, 75, 
    44, 13, 231, 200, 169, 138, 107, 76, 45, 14, 232, 201, 170, 139, 108, 77, 46, 15, 233, 
    202, 171, 140, 109, 78, 47, 16, 234, 203, 172, 141, 110, 79, 48, 17, 235, 204, 173, 142, 
    111, 80, 49, 18, 236, 205, 174, 143, 112, 81, 50, 19, 237, 206, 175, 144, 113, 82, 51, 
    20, 238, 207, 176, 145, 114, 83, 52, 21, 239, 208, 177, 146, 115, 84, 53, 22, 240, 209, 
    178, 147, 116, 85, 54, 23, 241, 210, 179, 148, 117, 86, 55, 24, 242, 211, 180, 149, 118, 
    87, 56, 25, 243, 212, 181, 150, 119, 88, 57, 26, 244, 213, 182, 151, 120, 89, 58, 27, 245, 
    214, 183, 152, 121, 90, 59, 28, 246, 215, 184, 153, 122, 91, 60, 29, 247, 216, 185, 154, 
    123, 92, 61, 30, 248, 217, 186, 155, 124, 93, 62, 31, 249, 218, 187, 156, 125, 94, 63, 
    250, 219, 188, 157, 126, 95, 251, 220, 189, 158, 127, 252, 221, 190, 159, 253, 222, 191, 
    254, 223, 255
};

static const stbv_u16 stbv_av1_scan_32x8[256] = {
    0, 1, 8, 2, 9, 16, 3, 10, 17, 24, 4, 11, 18, 25, 32, 5, 12, 19, 26, 33, 40, 6, 13, 20, 
    27, 34, 41, 48, 7, 14, 21, 28, 35, 42, 49, 56, 15, 22, 29, 36, 43, 50, 57, 64, 23, 30, 
    37, 44, 51, 58, 65, 72, 31, 38, 45, 52, 59, 66, 73, 80, 39, 46, 53, 60, 67, 74, 81, 88, 
    47, 54, 61, 68, 75, 82, 89, 96, 55, 62, 69, 76, 83, 90, 97, 104, 63, 70, 77, 84, 91, 98, 
    105, 112, 71, 78, 85, 92, 99, 106, 113, 120, 79, 86, 93, 100, 107, 114, 121, 128, 87, 94, 
    101, 108, 115, 122, 129, 136, 95, 102, 109, 116, 123, 130, 137, 144, 103, 110, 117, 124, 
    131, 138, 145, 152, 111, 118, 125, 132, 139, 146, 153, 160, 119, 126, 133, 140, 147, 154, 
    161, 168, 127, 134, 141, 148, 155, 162, 169, 176, 135, 142, 149, 156, 163, 170, 177, 184, 
    143, 150, 157, 164, 171, 178, 185, 192, 151, 158, 165, 172, 179, 186, 193, 200, 159, 166, 
    173, 180, 187, 194, 201, 208, 167, 174, 181, 188, 195, 202, 209, 216, 175, 182, 189, 196, 
    203, 210, 217, 224, 183, 190, 197, 204, 211, 218, 225, 232, 191, 198, 205, 212, 219, 226, 
    233, 240, 199, 206, 213, 220, 227, 234, 241, 248, 207, 214, 221, 228, 235, 242, 249, 215, 
    222, 229, 236, 243, 250, 223, 230, 237, 244, 251, 231, 238, 245, 252, 239, 246, 253, 247, 
    254, 255
};

static const stbv_u16 *const stbv_av1_scan_rect[STBV_AV1_N_TX_SIZES] = {
    stbv_av1_scan_4x4,  stbv_av1_scan_8x8,    stbv_av1_scan_16x16,
    stbv_av1_scan_32x32, stbv_av1_scan_32x32,
    stbv_av1_scan_4x8,  stbv_av1_scan_8x4,    stbv_av1_scan_8x16,
    stbv_av1_scan_16x8, stbv_av1_scan_16x32,  stbv_av1_scan_32x16,
    stbv_av1_scan_32x32, stbv_av1_scan_32x32,
    stbv_av1_scan_4x16, stbv_av1_scan_16x4,   stbv_av1_scan_8x32,
    stbv_av1_scan_32x8, stbv_av1_scan_16x32,  stbv_av1_scan_32x16
};

/* Decode one 4/8/16/32 (square or rectangular) transform.
 *
 * This follows dav1d's decode_coefs() representation closely.  cf[] is used
 * as a temporary linked list: bits 11.. carry the coefficient token and the
 * low 10 bits carry the next non-zero scan position.  After the syntax has
 * been consumed the list is walked again to read signs and dequantize.
 *
 * Quantization matrices are intentionally not handled here yet; the caller
 * supplies the scalar DC/AC dequantizers and dq_shift.
 *
 * tx is a dav1d RectTxfmSize value (stbv_av1_tx_dims index), tx_class is
 * STBV_AV1_TX_CLASS_* and chroma is 0 for luma, 1 for a chroma plane.
 */
static int stbv_av1_decode_coeffs_square(struct stb_av1_msac *msac,
                                          stbv_av1_cdf *cdf,
                                          int tx, int chroma, int tx_class,
                                          int dq_dc, int dq_ac,
                                          int dq_shift, int skip_ctx, int dc_sign_ctx,
                                          int bpc,
                                          stbv_i32 *cf,
                                          stbv_u8 *res_ctx_out)
{
    const stbv_u16 *scan;
    stbv_u8 levels[34 * 34];
    unsigned area, slw, slh, szctx, eob, eob_bin, is1d;
    unsigned x, y, rc, i, ctx, tok, mag;
    unsigned cul_level, dc_sign_level;
    int dc_tok, dc_sign, dc_dq, txctx, nonsquare;
    const unsigned char (*lo_ctx)[5];
    stbv_u16 *eob_bin_cdf;
    stbv_u16 *eob_hi_cdf;
    stbv_u16 *eob_cdf;
    stbv_u16 *lo_cdf;
    stbv_u16 *hi_cdf;
    stbv_u16 *dc_sign_cdf;
unsigned stride, shift, shift2, mask;
unsigned char *level;
/* Large enough for any bit depth; final pixel clipping happens
 * in itxfm_add (iclip_pixel). */
int cf_max = ~(~127U << bpc);

    if (tx < 0 || tx >= STBV_AV1_N_TX_SIZES)
        return -1;

    nonsquare = tx >= STBV_AV1_TX_4X8;
    lo_ctx = stbv_av1_lo_ctx_offsets[nonsquare + (tx & nonsquare)];
    txctx = stbv_av1_tx_dims[tx].ctx;
    slw = (unsigned)stbv_av1_tx_dims[tx].lw;
    slh = (unsigned)stbv_av1_tx_dims[tx].lh;
    if (slw > 3U) slw = 3U;
    if (slh > 3U) slh = 3U;

    switch (tx) {
    case STBV_AV1_TX_4X4:   scan = stbv_av1_scan_4x4;   break;
    case STBV_AV1_TX_8X8:   scan = stbv_av1_scan_8x8;   break;
    case STBV_AV1_TX_16X16: scan = stbv_av1_scan_16x16; break;
    case STBV_AV1_TX_32X32: scan = stbv_av1_scan_32x32; break;
    case STBV_AV1_TX_64X64: scan = stbv_av1_scan_32x32; break;
    default:
        scan = stbv_av1_scan_rect[tx];
        break;
    }

    if (txctx < 0) txctx = 0;
    if (txctx > 4) txctx = 4;
    if (skip_ctx < 0) skip_ctx = 0;
    if (skip_ctx > 12) skip_ctx = 12;
    if (dc_sign_ctx < 0) dc_sign_ctx = 0;
    if (dc_sign_ctx > 2) dc_sign_ctx = 2;

area = (4U << slw) * (4U << slh);
szctx = slw + slh;
is1d = tx_class != 0;
    memset(cf, 0, area * sizeof(*cf));
    memset(levels, 0, sizeof(levels));

    /* Coefficient skip is decoded by the caller.  Do not consume it here. */
    (void)skip_ctx;

    /* eob_bin_{16..1024}.  The first two dimensions are chroma and 1-D. */
    switch (szctx) {
    case 0:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB16_OFF +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 1:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB32_OFF +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 2:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB64_OFF +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 3:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB128_OFF +
                      ((unsigned)chroma * 2U + is1d) * 8U;
        break;
    case 4:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB256_OFF +
                      ((unsigned)chroma * 2U + is1d) * 16U;
        break;
    case 5:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB512_OFF + (unsigned)chroma * 16U;
        break;
    default:
        eob_bin_cdf = cdf->coef + STBV_AV1_COEF_EOB1024_OFF + (unsigned)chroma * 16U;
        break;
    }

    eob = stb_av1_msac_symbol(msac, eob_bin_cdf, 4U + szctx);
    if (eob > 1U) {
        eob_bin = eob - 2U;
        /* eob_hi_bit[N_TX_SIZES][2][9][2] */
        eob_hi_cdf = cdf->coef + STBV_AV1_COEF_EOBHI_OFF + (unsigned)txctx * 36U +
                     (unsigned)chroma * 18U + eob_bin * 2U;
        eob = ((stb_av1_msac_bool_adapt(msac, eob_hi_cdf) | 2U) << eob_bin) |
              stb_av1_msac_bools(msac, eob_bin);
    }
    if (eob > area)
        return -2;

    /* eob_base_tok[N_TX_SIZES][2][4][4] */
    eob_cdf = cdf->coef + STBV_AV1_COEF_EOBBASE_OFF + (unsigned)txctx * 32U +
              (unsigned)chroma * 16U;
    /* base_tok[N_TX_SIZES][2][41][4] */
    lo_cdf = cdf->coef + STBV_AV1_COEF_BASE_OFF + (unsigned)txctx * 328U +
             (unsigned)chroma * 164U;
    /* br_tok[min(txctx,3)][2][21][4] */
    hi_cdf = cdf->coef + STBV_AV1_COEF_BR_OFF + (unsigned)(txctx > 3 ? 3 : txctx) * 168U +
             (unsigned)chroma * 84U;

    /* The level scratch layout is exactly the one used by dav1d: for 2-D
     * transforms stride is the coefficient-grid width (4 << slh), which for
     * TX_64X64 is 32 while the real transform is 64 samples wide; H/V use
     * stride 16. */
    if (tx_class == 0) {
        stride = 4U << slh;
        shift = slh + 2U;
        shift2 = 0;
        mask = (4U << slh) - 1U;
    } else if (tx_class == 1) {
        stride = 16U;
        shift = slh + 2U;
        shift2 = 0;
        mask = (4U << slh) - 1U;
    } else {
        stride = 16U;
        shift = slw + 2U;
        shift2 = slh + 2U;
        mask = (4U << slw) - 1U;
    }
    memset(levels, 0, (size_t)(stride * ((4U << slw) + 2U)));

    /* EOB coefficient and descending AC scan, only when eob > 0. */
    if (eob) {
        ctx = 1U + (eob > (2U << szctx)) + (eob > (4U << szctx));
        tok = 1U + stb_av1_msac_symbol(msac, eob_cdf + ctx * 4U, 2U);
        dc_tok = (int)tok;

        if (tx_class == 0) {
            rc = scan[eob];
            x = rc >> shift;
            y = rc & mask;
        } else if (tx_class == 1) {
            x = eob & mask;
            y = eob >> shift;
            rc = eob;
        } else {
            x = eob & mask;
            y = eob >> shift;
            rc = (x << shift2) | y;
        }

        if (tok == 3U) {
            ctx = (tx_class == 0 ? ((x | y) > 1U) : (y != 0U)) ? 14U : 7U;
            tok = stbv_av1_coef_hi_tok(msac, hi_cdf + ctx * 4U);
            dc_tok = (int)tok;
            cf[rc] = tok << 11;
            level = levels + x * stride + y;
            *level = (stbv_u8)(tok + (3U << 6));
        } else {
            cf[rc] = tok << 11;
            level = levels + x * stride + y;
            *level = (stbv_u8)(tok * 0x41U);
        }

        /* Descending AC scan.  The linked-list encoding below is the important
         * detail: it lets the later sign/dequant pass visit only non-zero
         * values. */
        for (i = eob - 1U; i > 0U; i--) {
            unsigned rc_i;

            if (tx_class == 0) {
                rc_i = scan[i];
                x = rc_i >> shift;
                y = rc_i & mask;
            } else if (tx_class == 1) {
                x = i & mask;
                y = i >> shift;
                rc_i = i;
            } else {
                x = i & mask;
                y = i >> shift;
                rc_i = (x << shift2) | y;
            }

            level = levels + x * stride + y;
            ctx = stbv_av1_coef_lo_ctx(levels + x * stride + y,
                                        &mag, x, y, stride, tx_class, lo_ctx);
            tok = stb_av1_msac_symbol(msac, lo_cdf + ctx * 4U, 3U);

            if (tok == 3U) {
                /* dav1d ORs x into y for 2-D and compares y > 1; H/V compares
                 * y != 0.  mag is masked to 63 before the binarization
                 * context. */
                mag &= 63U;
                ctx = (tx_class == 0 ? ((x | y) > 1U) : (y != 0U)) ? 14U : 7U;
                ctx += mag > 12U ? 6U : (mag + 1U) >> 1;
                tok = stbv_av1_coef_hi_tok(msac, hi_cdf + ctx * 4U);
                *level = (stbv_u8)(tok + (3U << 6));
                cf[rc_i] = (tok << 11) | rc;
                rc = rc_i;
            } else {
                /* dav1d's packed expression is equivalent to storing zero or
                 * the token in bits 11+, with rc in the low ten bits when
                 * non-zero. */
                unsigned packed = tok * 0x17ff41U;
                *level = (stbv_u8)packed;
                tok = (packed >> 9) & (rc + ~0x7ffU);
                if (tok)
                    rc = rc_i;
                cf[rc_i] = tok;
            }
        }
    } else {
        rc = 0;
    }

    /* DC token.  For eob > 0 the DC token was already decoded by the EOB
     * section above (eob_base_tok with the eob-derived context).  For
     * dc-only (eob == 0) blocks dav1d reads it from eob_base_tok[0]. */
    if (eob) {
        /* DC token after the AC scan: dav1d decodes it from base_tok with
         * ctx = 0 for 2-D (or a lo_ctx-derived context for H/V), and a hi
         * token when the symbol is 3, using the magnitude of the
         * neighbours of the DC position. */
        ctx = (tx_class == 0U) ? 0U : stbv_av1_coef_lo_ctx(
            levels, &mag, 0, 0, stride, tx_class, lo_ctx);
        tok = stb_av1_msac_symbol(msac, lo_cdf + ctx * 4U, 3U);
        dc_tok = (int)tok;
        if (tok == 3U) {
            if (tx_class == 0U)
                mag = levels[0 * stride + 1U] + levels[1 * stride + 0U]
                    + levels[1 * stride + 1U];
            mag &= 63U;
            ctx = mag > 12U ? 6U : (mag + 1U) >> 1U;
            dc_tok = (int)stbv_av1_coef_hi_tok(msac, hi_cdf + ctx * 4U);
        }
    } else {
        /* dc-only (eob_bin == 0): dav1d evaluates the eob context with
         * eob == 1 (base_tok row 1), and escapes via the br context of
         * the DC position (x == y == 0 -> row 7).  One coefficient, the
         * DC, is present. */
        tok = stb_av1_msac_symbol(msac, eob_cdf, 2U);
        dc_tok = (int)(1U + tok);
        if (tok == 2U)
            dc_tok = (int)stbv_av1_coef_hi_tok(msac, hi_cdf);
        eob = 1; /* DC coefficient is present */
    }

    /* The final rc is the first non-zero coefficient in scan order.  dav1d's
     * residual pass follows the linked list encoded above. */
    cul_level = 0;
    dc_sign_level = 1U << 6;

    if (!dc_tok) {
        dc_sign_level = 1U << 6;
    } else {
        dc_sign_cdf = cdf->coef + STBV_AV1_COEF_DCSIGN_OFF + (chroma != 0) * 6U +
                      (unsigned)dc_sign_ctx * 2U;
        dc_sign = (int)stb_av1_msac_bool_adapt(msac, dc_sign_cdf);
        dc_sign_level = (dc_sign - 1) & (2 << 6);

        dc_dq = dq_dc;
        {
        int dc_tok_orig = dc_tok;
        if (dc_tok == 15) {
            dc_tok = (int)stbv_av1_coef_golomb(msac) + 15;
            dc_tok &= 0xfffff;
            dc_dq = (int)(((stbv_u32)dc_dq * (stbv_u32)dc_tok) & 0xffffffU);
        } else {
            dc_dq *= dc_tok;
        }
        dc_dq >>= dq_shift;
        if (dc_dq > cf_max + dc_sign)
            dc_dq = cf_max + dc_sign;
        cf[0] = dc_sign ? -dc_dq : dc_dq;
        cul_level = (unsigned)dc_tok;
        }
    }

    if (rc) {
        do {
            int sign = (int)stb_av1_msac_bool_equi(msac);
            unsigned rc_tok = (unsigned)cf[rc];
            unsigned vtok;
            int dq;

            if (rc_tok >= (15U << 11)) {
                vtok = stbv_av1_coef_golomb(msac) + 15U;
                vtok &= 0xfffffU;
                /* Escape residuals wrap the product to 24 bits before the
                 * right-shift, exactly like dav1d's ac_noqm path. */
                dq = (int)((((stbv_u32)dq_ac * (stbv_u32)vtok) & 0xffffffU) >>
                           (unsigned)dq_shift);
            } else {
                vtok = rc_tok >> 11;
                dq = (int)(((stbv_u32)dq_ac * (stbv_u32)vtok) >>
                           (unsigned)dq_shift);
            }
            if (dq > cf_max + sign)
                dq = cf_max + sign;
            cul_level += vtok;
            cf[rc] = sign ? -dq : dq;
            rc = rc_tok & 0x3ffU;
        } while (rc);
    }

    /* res_ctx = min(cul_level,63) | dc_sign_level, exactly like dav1d's
     * decode_coefs() tail. */
    if (res_ctx_out)
        *res_ctx_out = (stbv_u8)((cul_level < 63U ? cul_level : 63U) |
                                 dc_sign_level);
    return (int)eob;
}

#endif /* STB_AV1_COEF_H */
