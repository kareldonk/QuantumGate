/*
  This file is for the Gao-Mateer FFT
  sse http://www.math.clemson.edu/~sgao/papers/GM10.pdf
*/

#ifndef FFT_H
#define FFT_H
#define fft crypto_kem_mceliece8192128_vec_fft

#include <stdint.h>

#include "..\ref\params.h"
#include "vec.h"

void fft(vec [][GFBITS], vec [][GFBITS]);

#endif

