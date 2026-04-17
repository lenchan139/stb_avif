/*
 * Standalone AOM entropy decoder comparison for steam tile.
 * Reads symbols using AOM's exact od_ec_decode_cdf_q15 algorithm
 * with the same default CDFs, and prints each symbol with decoder state.
 *
 * This lets us diff against our stb_avif decoder output to find
 * the first divergence point.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned int uint32_t_my;
typedef unsigned short uint16_t_my;

/* ---- AOM RANGE DECODER (iCDF-based) ---- */
typedef struct {
   const unsigned char *buf;
   const unsigned char *end;
   uint32_t_my dif;
   unsigned int rng;
   int cnt;
} od_ec_dec;

static void od_ec_dec_refill(od_ec_dec *d) {
   int s = 9 - d->cnt;
   uint32_t_my dif = d->dif;
   const unsigned char *bptr = d->buf;
   while (s >= 0 && bptr < d->end) {
      dif ^= (uint32_t_my)(*bptr++) << s;
      s -= 8;
   }
   d->buf = bptr;
   d->cnt += (9 - s - 9);  /* cnt += bits_read */
   /* Actually: s went from (9-cnt_old) downward. After loop, cnt_new = 9 - s - 8? */
   /* Let me use AOM's exact formula: */
   /* Simplified: just track correctly */
}

/* Re-implement exactly as AOM does it */
static void od_ec_dec_init(od_ec_dec *d, const unsigned char *buf, unsigned int sz) {
   int s;
   d->buf = buf;
   d->end = buf + sz;
   d->dif = 0;
   d->rng = 0x8000;
   d->cnt = -15;
   /* refill */
   s = 9 - d->cnt; /* 9 - (-15) = 24 */
   {
      uint32_t_my dif = d->dif;
      const unsigned char *bptr = d->buf;
      int consumed = 0;
      while (s >= 0 && bptr < d->end) {
         dif ^= (uint32_t_my)(*bptr++) << s;
         s -= 8;
         consumed += 8;
      }
      d->buf = bptr;
      d->dif = dif;
      d->cnt += consumed;
   }
}

static void od_ec_dec_normalize(od_ec_dec *d, uint32_t_my dif, unsigned int rng) {
   int d_val;
   if (rng < 0x8000u) {
      int shift = 0;
      unsigned int r = rng;
      while (r < 0x8000u) { r <<= 1; shift++; }
      d->cnt -= shift;
      dif <<= shift;
      rng <<= shift;
      if (d->cnt < 0) {
         /* refill */
         int s = 9 - d->cnt;
         uint32_t_my df = dif;
         const unsigned char *bptr = d->buf;
         int consumed = 0;
         while (s >= 0 && bptr < d->end) {
            df ^= (uint32_t_my)(*bptr++) << s;
            s -= 8;
            consumed += 8;
         }
         d->buf = bptr;
         dif = df;
         d->cnt += consumed;
      }
   }
   d->dif = dif;
   d->rng = rng;
}

/* Read symbol from iCDF (AOM format: values are 32768-CDF) */
static unsigned int od_ec_decode_cdf_q15(od_ec_dec *d, const uint16_t_my *icdf, int nsyms) {
   uint32_t_my c, dif;
   unsigned int rng, u, v;
   int sym;

   dif = d->dif;
   rng = d->rng;
   c = (uint32_t_my)(dif >> (32 - 16));  /* top 16 bits */

   v = rng;
   sym = -1;
   do {
      u = v;
      v = ((rng >> 8) * (uint32_t_my)icdf[++sym]) >> 7;
   } while (c >= v);

   /* Update: dif -= v << 16, rng = u - v */
   dif -= (uint32_t_my)v << (32 - 16);
   rng = u - v;

   od_ec_dec_normalize(d, dif, rng);
   return (unsigned int)sym;
}

/* Read a literal bit */
static unsigned int od_ec_dec_bit(od_ec_dec *d) {
   static const uint16_t_my half_icdf[2] = { 16384, 0 };  /* AOM_ICDF(16384) = 16384 */
   return od_ec_decode_cdf_q15(d, half_icdf, 2);
}

static unsigned int od_ec_dec_literal(od_ec_dec *d, int bits) {
   unsigned int val = 0;
   int i;
   for (i = bits - 1; i >= 0; --i) {
      val |= od_ec_dec_bit(d) << i;
   }
   return val;
}

/*
 * Convert regular CDF to iCDF on the fly.
 * Our tables store CDF (increasing to 32768).
 * AOM uses iCDF = 32768 - CDF.
 */
static unsigned int read_symbol_cdf(od_ec_dec *d, const uint16_t_my *cdf, int nsyms) {
   uint16_t_my icdf[16];
   int i;
   for (i = 0; i < nsyms; i++) icdf[i] = 32768u - cdf[i];
   icdf[nsyms - 1] = 0;  /* terminal marker */
   return od_ec_decode_cdf_q15(d, icdf, nsyms);
}

/* ---- DEFAULT CDFs (same as stb_avif.h, stored as regular CDF) ---- */

/* kf_y_mode_cdf[5][5][14] (first two from stb_avif) */
static const uint16_t_my kf_y_mode_cdf_0_0[14] = {
   15588, 17027, 19338, 20218, 20682, 21110, 21825, 23244,
   24189, 28165, 29093, 30466, 32768, 0
};

/* Partition CDF for 64x64 SB (10 symbols) */
/* From AOM default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)]
   Context for 64x64 SB at top-left with no above/left = context 4*4+0 = 16? 
   Actually, we need the exact context. Let me use the values from SYM[0] trace. */

int main(int argc, char **argv) {
   FILE *f;
   unsigned char *filedata;
   long fsize;
   const unsigned char *tile_data;
   unsigned int tile_size;
   od_ec_dec dec;
   int sym_count = 0;

   if (argc < 2) {
      fprintf(stderr, "Usage: %s <avif_file>\n", argv[0]);
      return 1;
   }

   f = fopen(argv[1], "rb");
   if (!f) { perror("fopen"); return 1; }
   fseek(f, 0, SEEK_END);
   fsize = ftell(f);
   fseek(f, 0, SEEK_SET);
   filedata = (unsigned char *)malloc(fsize);
   fread(filedata, 1, fsize, f);
   fclose(f);

   /* Find b0 ea ce cb */
   {
      long i;
      for (i = 0; i < fsize - 4; i++) {
         if (filedata[i] == 0xb0 && filedata[i+1] == 0xea &&
             filedata[i+2] == 0xce && filedata[i+3] == 0xcb) {
            tile_data = filedata + i;
            tile_size = (unsigned int)(fsize - i); /* approximate */
            printf("Found tile data at offset %ld, first 4: %02x %02x %02x %02x\n",
               i, tile_data[0], tile_data[1], tile_data[2], tile_data[3]);
            break;
         }
      }
   }

   /* Init decoder like AOM does */
   od_ec_dec_init(&dec, tile_data, tile_size > 45796 ? 45796 : tile_size);
   printf("INIT: dif=%u rng=%u cnt=%d\n", dec.dif, dec.rng, dec.cnt);

   /* Read first few symbols manually using our CDF tables.
    * The key insight: if our decoder diverges from AOM here, we've found the bug.
    * Print dif, rng after each symbol. */

   /* SYM0: partition (10 symbols, context 0 for 64x64 SB) */
   /* We need the actual partition CDF. From trace: cdf[0]=20137 */
   /* Let me just read raw symbols and print state */
   {
      uint16_t_my part_cdf[11] = {
         20137, 21547, 23078, 29566, 29837, 30261, 30524, 30892, 31724, 32088, 32768
      };
      /* Wait, we need 10 symbols, so 10 CDF values + count = 11. But actually
         the CDF has nsyms entries ending at 32768. Let me check:
         10 symbols need CDF[0..9] where CDF[9]=32768 and count at [10]=0 */
      /* From trace: nsyms=10, cdf[0]=20137. Let me reconstruct the full CDF.
         Actually I don't have the full CDF values. Let me just trace the decoder
         state to check if it matches. */
   }

   printf("\nLet me just initialize and read using the half-CDF to verify state:\n");

   /* Just print initial state and manually read bits */
   {
      od_ec_dec d2;
      od_ec_dec_init(&d2, tile_data, 45796);
      printf("After init: dif=%u rng=%u cnt=%d\n", d2.dif, d2.rng, d2.cnt);

      /* Read partition as raw kf_y_mode to see what we get */
      unsigned int sym = read_symbol_cdf(&d2, kf_y_mode_cdf_0_0, 13);
      printf("If we read kf_y_mode from start: sym=%u dif=%u rng=%u\n", sym, d2.dif, d2.rng);
   }

   free(filedata);
   return 0;
}
