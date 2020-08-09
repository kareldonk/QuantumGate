/*
  This file is for the inversion-free Berlekamp-Massey algorithm
  see https://ieeexplore.ieee.org/document/87857
*/

#ifndef BM_H
#define BM_H
#define bm crypto_kem_mceliece8192128_vec_bm

#include "..\ref\params.h"
#include "vec.h"

void bm(vec [][GFBITS], vec [][ GFBITS ]);

#endif

