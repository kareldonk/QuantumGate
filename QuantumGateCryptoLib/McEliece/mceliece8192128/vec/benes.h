/*
  This file is for Benes network related functions
*/

#ifndef BENES_H
#define BENES_H
#define benes crypto_kem_mceliece8192128_vec_benes
#define support_gen crypto_kem_mceliece8192128_vec_support_gen

#include "gf.h"
#include "vec.h"

void benes(vec *, const unsigned char *, int);

#endif

