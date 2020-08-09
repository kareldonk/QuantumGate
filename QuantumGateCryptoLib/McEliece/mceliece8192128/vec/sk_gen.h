/*
  This file is for secret-key generation
*/

#ifndef SK_GEN_H
#define SK_GEN_H
#define genpoly_gen crypto_kem_mceliece8192128_vec_genpoly_gen
#define perm_check crypto_kem_mceliece8192128_vec_perm_check

#include "gf.h"

#include <stdint.h>

int genpoly_gen(gf *, gf *);
int perm_check(uint32_t *);

#endif

