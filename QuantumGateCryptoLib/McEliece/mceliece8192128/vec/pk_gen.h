/*
  This file is for public-key generation
*/

#ifndef PK_GEN_H
#define PK_GEN_H
#define pk_gen crypto_kem_mceliece8192128_vec_pk_gen

#include <stdint.h>

int pk_gen(unsigned char *, const unsigned char *, uint32_t *, uint64_t *, uint64_t *);

#endif

