/*
  This file is for Niederreiter encryption
*/

#ifndef ENCRYPT_H
#define ENCRYPT_H
#define encrypt crypto_kem_mceliece8192128_vec_encrypt

void encrypt(unsigned char *, const unsigned char *, unsigned char *);

#endif

