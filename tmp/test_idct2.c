#include <stdio.h>
#include <string.h>

static const int cospi[64] = {
   4096, 4095, 4091, 4085, 4076, 4065, 4052, 4036,
   4017, 3996, 3973, 3948, 3920, 3889, 3857, 3822,
   3784, 3745, 3703, 3659, 3612, 3564, 3513, 3461,
   3406, 3349, 3290, 3229, 3166, 3102, 3035, 2967,
   2896, 2824, 2751, 2675, 2598, 2520, 2440, 2359,
   2276, 2191, 2106, 2019, 1931, 1842, 1751, 1660,
   1567, 1474, 1380, 1285, 1189, 1092,  995,  897,
    799,  700,  601,  501,  401,  301,  201,  101
};
#define HALF_BTF(w0,in0,w1,in1,bit) \
   (int)(((long)(w0)*(in0) + (long)(w1)*(in1) + (1L<<((bit)-1))) >> (bit))
#define ROUND_SHIFT(x,bit) (((x) + (1 << ((bit)-1))) >> (bit))

static void idct16(const int *input, int *output) {
   const int *c = cospi;
   int bf0[16], bf1[16], step[16];
   bf1[0]=input[0];bf1[1]=input[8];bf1[2]=input[4];bf1[3]=input[12];
   bf1[4]=input[2];bf1[5]=input[10];bf1[6]=input[6];bf1[7]=input[14];
   bf1[8]=input[1];bf1[9]=input[9];bf1[10]=input[5];bf1[11]=input[13];
   bf1[12]=input[3];bf1[13]=input[11];bf1[14]=input[7];bf1[15]=input[15];
   step[0]=bf1[0];step[1]=bf1[1];step[2]=bf1[2];step[3]=bf1[3];
   step[4]=bf1[4];step[5]=bf1[5];step[6]=bf1[6];step[7]=bf1[7];
   step[8]=HALF_BTF(c[60],bf1[8],-c[4],bf1[15],12);
   step[9]=HALF_BTF(c[28],bf1[9],-c[36],bf1[14],12);
   step[10]=HALF_BTF(c[44],bf1[10],-c[20],bf1[13],12);
   step[11]=HALF_BTF(c[12],bf1[11],-c[52],bf1[12],12);
   step[12]=HALF_BTF(c[52],bf1[11],c[12],bf1[12],12);
   step[13]=HALF_BTF(c[20],bf1[10],c[44],bf1[13],12);
   step[14]=HALF_BTF(c[36],bf1[9],c[28],bf1[14],12);
   step[15]=HALF_BTF(c[4],bf1[8],c[60],bf1[15],12);
   bf0[0]=step[0];bf0[1]=step[1];bf0[2]=step[2];bf0[3]=step[3];
   bf0[4]=HALF_BTF(c[56],step[4],-c[8],step[7],12);
   bf0[5]=HALF_BTF(c[24],step[5],-c[40],step[6],12);
   bf0[6]=HALF_BTF(c[40],step[5],c[24],step[6],12);
   bf0[7]=HALF_BTF(c[8],step[4],c[56],step[7],12);
   bf0[8]=step[8]+step[9];bf0[9]=step[8]-step[9];
   bf0[10]=-step[10]+step[11];bf0[11]=step[10]+step[11];
   bf0[12]=step[12]+step[13];bf0[13]=step[12]-step[13];
   bf0[14]=-step[14]+step[15];bf0[15]=step[14]+step[15];
   step[0]=HALF_BTF(c[32],bf0[0],c[32],bf0[1],12);
   step[1]=HALF_BTF(c[32],bf0[0],-c[32],bf0[1],12);
   step[2]=HALF_BTF(c[48],bf0[2],-c[16],bf0[3],12);
   step[3]=HALF_BTF(c[16],bf0[2],c[48],bf0[3],12);
   step[4]=bf0[4]+bf0[5];step[5]=bf0[4]-bf0[5];
   step[6]=-bf0[6]+bf0[7];step[7]=bf0[6]+bf0[7];
   step[8]=bf0[8];step[9]=HALF_BTF(-c[16],bf0[9],c[48],bf0[14],12);
   step[10]=HALF_BTF(-c[48],bf0[10],-c[16],bf0[13],12);
   step[11]=bf0[11];step[12]=bf0[12];
   step[13]=HALF_BTF(-c[16],bf0[10],c[48],bf0[13],12);
   step[14]=HALF_BTF(c[48],bf0[9],c[16],bf0[14],12);
   step[15]=bf0[15];
   bf0[0]=step[0]+step[3];bf0[1]=step[1]+step[2];
   bf0[2]=step[1]-step[2];bf0[3]=step[0]-step[3];
   bf0[4]=step[4];bf0[5]=HALF_BTF(-c[32],step[5],c[32],step[6],12);
   bf0[6]=HALF_BTF(c[32],step[5],c[32],step[6],12);bf0[7]=step[7];
   bf0[8]=step[8]+step[11];bf0[9]=step[9]+step[10];
   bf0[10]=step[9]-step[10];bf0[11]=step[8]-step[11];
   bf0[12]=-step[12]+step[15];bf0[13]=-step[13]+step[14];
   bf0[14]=step[13]+step[14];bf0[15]=step[12]+step[15];
   step[0]=bf0[0]+bf0[7];step[1]=bf0[1]+bf0[6];
   step[2]=bf0[2]+bf0[5];step[3]=bf0[3]+bf0[4];
   step[4]=bf0[3]-bf0[4];step[5]=bf0[2]-bf0[5];
   step[6]=bf0[1]-bf0[6];step[7]=bf0[0]-bf0[7];
   step[8]=bf0[8];step[9]=bf0[9];
   step[10]=HALF_BTF(-c[32],bf0[10],c[32],bf0[13],12);
   step[11]=HALF_BTF(-c[32],bf0[11],c[32],bf0[12],12);
   step[12]=HALF_BTF(c[32],bf0[11],c[32],bf0[12],12);
   step[13]=HALF_BTF(c[32],bf0[10],c[32],bf0[13],12);
   step[14]=bf0[14];step[15]=bf0[15];
   output[0]=step[0]+step[15];output[1]=step[1]+step[14];
   output[2]=step[2]+step[13];output[3]=step[3]+step[12];
   output[4]=step[4]+step[11];output[5]=step[5]+step[10];
   output[6]=step[6]+step[9];output[7]=step[7]+step[8];
   output[8]=step[7]-step[8];output[9]=step[6]-step[9];
   output[10]=step[5]-step[10];output[11]=step[4]-step[11];
   output[12]=step[3]-step[12];output[13]=step[2]-step[13];
   output[14]=step[1]-step[14];output[15]=step[0]-step[15];
}

int main(void) {
   int coeffs[256], temp[256], buf[16], out[16];
   int i, j;
   memset(coeffs, 0, sizeof(coeffs));
   coeffs[0] = 981;
   for (i = 0; i < 16; i++) {
      for (j = 0; j < 16; j++) buf[j] = coeffs[j*16 + i];
      idct16(buf, out);
      for (j = 0; j < 16; j++) temp[i*16 + j] = ROUND_SHIFT(out[j], 2);
   }
   for (j = 0; j < 16; j++) {
      for (i = 0; i < 16; i++) buf[i] = temp[i*16 + j];
      idct16(buf, out);
      for (i = 0; i < 16; i++) coeffs[i*16 + j] = ROUND_SHIFT(out[i], 4);
   }
   printf("DCT_DCT DC=981 16x16:\n");
   printf("  coeff[0]=%d [1]=%d [16]=%d [255]=%d\n",
      coeffs[0], coeffs[1], coeffs[16], coeffs[255]);
   printf("  gain = %f\n", (double)coeffs[0]/981.0);
   printf("  expected per pixel residual if white(1016)-pred(512)=504: need DC=%d\n",
      (int)(504.0/((double)coeffs[0]/981.0)));
   printf("  with dc_q=327, need level=%d\n",
      (int)(504.0/((double)coeffs[0]/981.0)/327.0));
   return 0;
}
